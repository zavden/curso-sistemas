# Formatos de imagen de disco

## Índice

1. [Imágenes de disco virtual](#imágenes-de-disco-virtual)
2. [Formato raw](#formato-raw)
3. [Formato qcow2](#formato-qcow2)
4. [Comparación raw vs qcow2](#comparación-raw-vs-qcow2)
5. [qemu-img: crear imágenes](#qemu-img-crear-imágenes)
6. [qemu-img: inspeccionar imágenes](#qemu-img-inspeccionar-imágenes)
7. [qemu-img: convertir entre formatos](#qemu-img-convertir-entre-formatos)
8. [qemu-img: redimensionar](#qemu-img-redimensionar)
9. [Errores comunes](#errores-comunes)
10. [Cheatsheet](#cheatsheet)
11. [Ejercicios](#ejercicios)

---

## Imágenes de disco virtual

Una imagen de disco virtual es un **archivo en el host** que la VM ve como un
disco duro real. Desde la perspectiva del guest, es un disco `/dev/vda` de
20 GB. Desde la perspectiva del host, es un archivo `disk.qcow2` en el
filesystem.

```
┌──────────────────────────────────────────────────────────────┐
│  Perspectiva del guest:                                      │
│                                                              │
│  lsblk                                                       │
│  NAME   SIZE  TYPE  MOUNTPOINT                               │
│  vda     20G  disk                                           │
│  ├─vda1 512M  part  /boot                                    │
│  └─vda2  19G  part  /                                        │
│                                                              │
│  El guest ve un disco de 20 GB. No sabe que es un archivo.   │
│                                                              │
│  ─────────────────────────────────────────────────────────── │
│                                                              │
│  Perspectiva del host:                                       │
│                                                              │
│  ls -lh /var/lib/libvirt/images/                             │
│  -rw------- 1 qemu qemu 3.2G  disk.qcow2                   │
│                                                              │
│  El host ve un archivo de 3.2 GB (no 20 GB).                │
│  El archivo crece a medida que el guest escribe datos.       │
│  Esto es "thin provisioning" (aprovisionamiento delgado).    │
└──────────────────────────────────────────────────────────────┘
```

---

## Formato raw

El formato más simple: los bytes del archivo corresponden directamente a los
bytes del disco virtual. Sin metadata, sin compresión, sin features extra.

### Características

```
┌──────────────────────────────────────────────────────────────┐
│  Formato RAW                                                 │
│                                                              │
│  Archivo en el host:                                         │
│  ┌────────────────────────────────────────────────────┐     │
│  │ sector 0 │ sector 1 │ sector 2 │ ... │ sector N   │     │
│  │ (512 B)  │ (512 B)  │ (512 B)  │     │ (512 B)    │     │
│  └────────────────────────────────────────────────────┘     │
│                                                              │
│  Byte 0 del archivo = byte 0 del disco virtual              │
│  Byte 512 del archivo = sector 1 del disco virtual          │
│  Sin headers, sin metadata, sin indirección                 │
│                                                              │
│  Ventajas:                                                   │
│  + Rendimiento máximo: lectura/escritura directa            │
│  + Simple: cualquier herramienta puede leerlo (dd, mount)   │
│  + Sin overhead de formato                                   │
│                                                              │
│  Desventajas:                                                │
│  - Tamaño fijo: 20 GB de disco = 20 GB de archivo*         │
│  - Sin snapshots                                             │
│  - Sin compresión                                            │
│  - Sin copy-on-write                                         │
│  - Sin encriptación integrada                               │
│                                                              │
│  * Los filesystems modernos (ext4, XFS, btrfs) con soporte  │
│    de sparse files pueden no ocupar los 20 GB realmente.    │
│    Pero QEMU no lo garantiza.                                │
└──────────────────────────────────────────────────────────────┘
```

### Sparse files

En Linux, un archivo raw puede ser "sparse": el filesystem solo asigna bloques
para las regiones que tienen datos. El archivo dice ser de 20 GB pero ocupa
menos en disco:

```bash
# Crear un archivo raw de 20 GB
qemu-img create -f raw disk.raw 20G

# Tamaño aparente vs tamaño real
ls -lh disk.raw
# -rw-r--r-- 1 user user 20G  disk.raw    ← aparente: 20 GB

du -h disk.raw
# 0       disk.raw                          ← real: 0 (sparse)

# Comparar:
ls -lsh disk.raw
# 0 -rw-r--r-- 1 user user 20G  disk.raw
# ↑ tamaño real en disco
```

Pero ojo: copiar un sparse file con `cp` sin `--sparse=always` lo expande al
tamaño completo.

### Cuándo usar raw

- Máximo rendimiento de I/O (bases de datos, benchmarks)
- Cuando necesitas montar la imagen directamente con `mount -o loop`
- Cuando no necesitas snapshots ni thin provisioning
- Passthrough de disco: raw es el formato más compatible

---

## Formato qcow2

qcow2 (QEMU Copy-On-Write version 2) es el formato nativo de QEMU. Es más
sofisticado que raw: tiene metadata, soporta features avanzadas y usa
thin provisioning real.

### Estructura interna

```
┌──────────────────────────────────────────────────────────────┐
│  Formato qcow2                                              │
│                                                              │
│  Archivo en el host:                                         │
│  ┌──────────────────────────────────────────────────────┐   │
│  │  Header (72+ bytes)                                   │   │
│  │  - Magic: "QFI\xfb"                                  │   │
│  │  - Version: 3                                         │   │
│  │  - Virtual size, cluster size, backing file...        │   │
│  ├──────────────────────────────────────────────────────┤   │
│  │  L1 Table (tabla de nivel 1)                          │   │
│  │  - Apunta a tablas L2                                 │   │
│  ├──────────────────────────────────────────────────────┤   │
│  │  L2 Tables (tablas de nivel 2)                        │   │
│  │  - Apuntan a clusters de datos                        │   │
│  ├──────────────────────────────────────────────────────┤   │
│  │  Data clusters (los datos reales)                     │   │
│  │  - Solo los clusters escritos existen                 │   │
│  │  - Los no escritos no ocupan espacio                  │   │
│  ├──────────────────────────────────────────────────────┤   │
│  │  Reference count table                                │   │
│  │  - Contadores para COW y snapshots                    │   │
│  └──────────────────────────────────────────────────────┘   │
│                                                              │
│  Lectura del sector X:                                       │
│  1. Buscar en L1 → obtener dirección de L2                  │
│  2. Buscar en L2 → obtener dirección del cluster            │
│  3. Si el cluster no existe → el sector es zeros            │
│  4. Si el cluster existe → leer los datos                   │
│                                                              │
│  Escritura del sector X:                                     │
│  1. Si el cluster no existe → asignar nuevo cluster         │
│  2. Escribir los datos en el cluster asignado               │
│  3. Actualizar L2 y refcount                                │
│                                                              │
│  Esta indirección tiene un costo (~2-5% overhead)           │
│  pero permite thin provisioning y snapshots                  │
└──────────────────────────────────────────────────────────────┘
```

### Features de qcow2

```
┌──────────────────────────────────────────────────────────────┐
│  Feature              Qué hace                               │
│  ───────              ────────                               │
│                                                              │
│  Thin provisioning    El archivo crece según se usa.         │
│                       20 GB virtual → empieza en ~200 KB.    │
│                       Solo los clusters escritos existen.    │
│                                                              │
│  Snapshots internos   Guardar el estado del disco en un      │
│                       instante. Revertir a ese estado.       │
│                       Usa COW: no duplica datos.             │
│                                                              │
│  Backing files        Imagen base (read-only) + overlay      │
│                       (copy-on-write). Tema de T02.          │
│                                                              │
│  Compresión           Clusters pueden estar comprimidos      │
│                       con zlib/zstd. Reduce tamaño en disco. │
│                       Las escrituras se descomprimen.         │
│                                                              │
│  Encriptación         LUKS integrado en el formato.          │
│                       qemu-img create --object secret ...    │
│                                                              │
│  Preallocation        full: reservar espacio completo.       │
│                       metadata: reservar tablas L1/L2.       │
│                       falloc: fallocate (rápido).            │
│                       Reduce fragmentación, mejora I/O.      │
└──────────────────────────────────────────────────────────────┘
```

### Thin provisioning en acción

```bash
# Crear imagen de 20 GB
qemu-img create -f qcow2 disk.qcow2 20G

# ¿Cuánto ocupa realmente?
ls -lh disk.qcow2
# -rw-r--r-- 1 user user 193K disk.qcow2
# ← 193 KB, no 20 GB

qemu-img info disk.qcow2
# virtual size: 20 GiB (21474836480 bytes)
# disk size: 196 KiB                        ← tamaño real en disco
# cluster_size: 65536

# Después de instalar un OS y escribir datos:
qemu-img info disk.qcow2
# virtual size: 20 GiB
# disk size: 3.2 GiB                        ← solo lo usado
```

---

## Comparación raw vs qcow2

```
┌────────────────────┬──────────────────┬──────────────────────┐
│                    │  raw             │  qcow2               │
├────────────────────┼──────────────────┼──────────────────────┤
│ Tamaño en disco    │ Fijo (o sparse)  │ Crece bajo demanda   │
│ disco 20G virtual  │ 20 GB (o sparse) │ ~200 KB al inicio    │
├────────────────────┼──────────────────┼──────────────────────┤
│ Rendimiento I/O    │ Mejor (~2-5%     │ Overhead por L1/L2   │
│                    │ más rápido)      │ lookup + refcount    │
├────────────────────┼──────────────────┼──────────────────────┤
│ Con preallocation  │ = raw            │ ≈ raw (diferencia    │
│ full en qcow2      │                  │ < 1% en la mayoría)  │
├────────────────────┼──────────────────┼──────────────────────┤
│ Snapshots          │ No               │ Sí (internos)        │
├────────────────────┼──────────────────┼──────────────────────┤
│ Backing files      │ No               │ Sí (overlays)        │
├────────────────────┼──────────────────┼──────────────────────┤
│ Compresión         │ No               │ Sí (zlib/zstd)       │
├────────────────────┼──────────────────┼──────────────────────┤
│ Encriptación       │ No (usar LUKS    │ Sí (LUKS integrado)  │
│                    │ del host)        │                      │
├────────────────────┼──────────────────┼──────────────────────┤
│ Montar con loop    │ Sí (directo)     │ No (usar guestmount) │
├────────────────────┼──────────────────┼──────────────────────┤
│ Uso recomendado    │ I/O intensivo    │ Uso general, labs,   │
│                    │ Producción DB    │ snapshots, templates │
└────────────────────┴──────────────────┴──────────────────────┘
```

### La diferencia de rendimiento real

En la práctica, la diferencia raw vs qcow2 es menor de lo que parece:

```
┌──────────────────────────────────────────────────────────────┐
│  Benchmark típico (fio, random 4K writes):                   │
│                                                              │
│  raw:                 50,000 IOPS                            │
│  qcow2 (thin):        47,000 IOPS  (~6% menos)             │
│  qcow2 (prealloc):    49,500 IOPS  (~1% menos)             │
│                                                              │
│  La diferencia importa para bases de datos de alto           │
│  rendimiento. Para labs del curso, es despreciable.          │
│                                                              │
│  Preallocation elimina casi todo el overhead porque          │
│  los clusters ya están asignados: no hay que asignar        │
│  nuevos clusters ni actualizar L2 tables al escribir.        │
│                                                              │
│  Para el curso: usaremos qcow2 sin preallocation (thin).    │
│  Las features (snapshots, overlays, thin) valen más que     │
│  el ~5% de rendimiento perdido.                              │
└──────────────────────────────────────────────────────────────┘
```

---

## qemu-img: crear imágenes

`qemu-img` es la herramienta CLI para gestionar imágenes de disco.

### Crear qcow2 (lo más común)

```bash
# Sintaxis básica
qemu-img create -f qcow2 <nombre> <tamaño>

# Crear imagen de 20 GB
qemu-img create -f qcow2 disk.qcow2 20G

# Tamaños aceptados:
# K = kilobytes, M = megabytes, G = gigabytes, T = terabytes
qemu-img create -f qcow2 small.qcow2 512M
qemu-img create -f qcow2 big.qcow2 2T
```

### Crear raw

```bash
qemu-img create -f raw disk.raw 20G
```

### Crear con preallocation

```bash
# Preasignar metadata (tablas L1/L2) — rápido, mejora rendimiento
qemu-img create -f qcow2 -o preallocation=metadata disk.qcow2 20G

# Preasignar todo el espacio — lento, rendimiento máximo
qemu-img create -f qcow2 -o preallocation=full disk.qcow2 20G

# Preasignar con fallocate — rápido en filesystems que lo soportan
qemu-img create -f qcow2 -o preallocation=falloc disk.qcow2 20G
```

### Crear con cluster size personalizado

```bash
# Cluster size por defecto: 64 KB
# Más grande = menos overhead L2, pero más desperdicio por cluster
qemu-img create -f qcow2 -o cluster_size=128K disk.qcow2 20G

# Para el curso: el default (64 KB) está bien
```

---

## qemu-img: inspeccionar imágenes

### qemu-img info

```bash
qemu-img info disk.qcow2
```

Salida típica:

```
image: disk.qcow2
file format: qcow2
virtual size: 20 GiB (21474836480 bytes)
disk size: 3.2 GiB
cluster_size: 65536
Format specific information:
    compat: 1.1
    compression type: zlib
    lazy refcounts: false
    refcount bits: 16
    corrupt: false
    extended l2: false
```

### Campos importantes

```
┌──────────────────────────────────────────────────────────────┐
│  Campo              Significado                              │
│  ─────              ───────────                              │
│  file format        qcow2, raw, vmdk, vdi...                │
│  virtual size       Tamaño que ve el guest (20 GB)          │
│  disk size          Espacio real en disco del host (3.2 GB) │
│  cluster_size       Tamaño de bloque de asignación (64 KB)  │
│  compat             Versión de compatibilidad (1.1 = v3)    │
│  corrupt            ¿La imagen está corrompida?             │
│  backing file       Imagen base (si es overlay, T02)        │
│  Snapshots          Lista de snapshots internos             │
└──────────────────────────────────────────────────────────────┘
```

### qemu-img check

Verificar la integridad de una imagen qcow2:

```bash
qemu-img check disk.qcow2
```

```
No errors were found on the image.
327680/327680 = 100.00% allocated of 20.00 GiB
Image end offset: 21475885056
```

Si hay errores:

```bash
# Reparar (solo si hay errores)
qemu-img check -r all disk.qcow2
```

### qemu-img map

Ver qué regiones del disco virtual tienen datos asignados:

```bash
qemu-img map --output=json disk.qcow2 | head -20
```

---

## qemu-img: convertir entre formatos

### raw → qcow2

```bash
qemu-img convert -f raw -O qcow2 disk.raw disk.qcow2

# -f raw     formato de entrada
# -O qcow2   formato de salida (O mayúscula)
```

### qcow2 → raw

```bash
qemu-img convert -f qcow2 -O raw disk.qcow2 disk.raw
```

### Otros formatos

```bash
# vmdk (VMware) → qcow2
qemu-img convert -f vmdk -O qcow2 disk.vmdk disk.qcow2

# vdi (VirtualBox) → qcow2
qemu-img convert -f vdi -O qcow2 disk.vdi disk.qcow2

# qcow2 → vmdk (para exportar a VMware)
qemu-img convert -f qcow2 -O vmdk disk.qcow2 disk.vmdk
```

### Conversión con compresión

```bash
# Convertir y comprimir (solo qcow2 destino)
qemu-img convert -f qcow2 -O qcow2 -c disk.qcow2 disk-compressed.qcow2

# -c habilita compresión por cluster
# Reduce tamaño de disco pero las escrituras futuras descomprimen
# Útil para distribuir imágenes base
```

### Conversión in-situ (amend)

Para cambiar opciones de un qcow2 sin reconvertir:

```bash
# Cambiar el tipo de compatibilidad
qemu-img amend -f qcow2 -o compat=1.1 disk.qcow2

# Habilitar lazy refcounts (mejora rendimiento de escritura)
qemu-img amend -f qcow2 -o lazy_refcounts=on disk.qcow2
```

---

## qemu-img: redimensionar

### Agrandar una imagen

```bash
# Agregar 10 GB a una imagen existente
qemu-img resize disk.qcow2 +10G

# O especificar el tamaño final
qemu-img resize disk.qcow2 30G
```

**Importante:** esto solo agranda la imagen (el "disco virtual"). Las
particiones y filesystems dentro del guest no se expanden automáticamente.
Dentro de la VM necesitas:

```bash
# Dentro del guest, después de redimensionar la imagen:
# 1. Ver el nuevo tamaño del disco
lsblk
# vda    30G  ← el disco creció

# 2. Expandir la partición (con growpart o fdisk)
sudo growpart /dev/vda 2

# 3. Expandir el filesystem
sudo resize2fs /dev/vda2       # ext4
# o
sudo xfs_growfs /                # XFS
```

### Encoger una imagen

```bash
# Encoger requiere --shrink (peligroso: puede destruir datos)
qemu-img resize --shrink disk.qcow2 10G
```

**Peligro:** encoger una imagen sin antes encoger las particiones y
filesystems dentro del guest destruye datos. El orden es:

```
┌──────────────────────────────────────────────────────────────┐
│  Agrandar (seguro):                                          │
│  1. qemu-img resize disk.qcow2 +10G    ← primero la imagen │
│  2. Dentro del guest: growpart + resize2fs  ← luego el FS  │
│                                                              │
│  Encoger (peligroso):                                        │
│  1. Dentro del guest: resize2fs + fdisk ← PRIMERO el FS    │
│  2. qemu-img resize --shrink disk.qcow2 10G  ← luego la img│
│                                                              │
│  Si lo haces al revés al encoger, pierdes datos.            │
│  Recomendación: no encoger. Crear una imagen nueva del      │
│  tamaño deseado y copiar los datos.                          │
└──────────────────────────────────────────────────────────────┘
```

### Redimensionar una imagen raw

```bash
# Raw se puede agrandar con truncate o dd también
qemu-img resize disk.raw +10G

# O incluso:
truncate -s +10G disk.raw
```

---

## Errores comunes

### 1. Crear una imagen qcow2 y pensar que ocupa 20 GB

```
❌ qemu-img create -f qcow2 disk.qcow2 20G
   df -h
   # "¡Mi disco se llenó! La imagen de 20 GB no cabe"

✅ qcow2 usa thin provisioning: la imagen empieza en ~200 KB
   ls -lh disk.qcow2    # Tamaño aparente: 20 GB
   du -h disk.qcow2     # Tamaño real: 200 KB
   qemu-img info disk.qcow2
   # virtual size: 20 GiB
   # disk size: 196 KiB   ← esto es lo que ocupa en disco
```

### 2. Copiar un sparse file sin preservar los holes

```bash
# ❌ cp expande los holes por defecto en algunos sistemas
cp disk.raw disk-copy.raw
du -h disk-copy.raw
# 20G  ← ¡expandió los zeros!

# ✅ Preservar sparse holes
cp --sparse=always disk.raw disk-copy.raw
du -h disk-copy.raw
# 196K ← mantiene los holes
```

### 3. Redimensionar la imagen pero no el filesystem

```
❌ qemu-img resize disk.qcow2 +10G
   # Dentro del guest: el disco creció pero la partición no
   df -h
   # /dev/vda2  19G  ← sigue igual

✅ Después de resize, dentro del guest:
   sudo growpart /dev/vda 2     # Expandir partición
   sudo resize2fs /dev/vda2     # Expandir filesystem (ext4)
   df -h
   # /dev/vda2  29G  ← ahora sí
```

### 4. Confundir -f (formato entrada) con -O (formato salida)

```bash
# ❌ Confundir las flags de convert
qemu-img convert -f qcow2 -f raw input.raw output.qcow2
# Error: doble -f

# ✅ -f = formato de entrada, -O = formato de salida
qemu-img convert -f raw -O qcow2 input.raw output.qcow2
#                 ↑ in   ↑ out (O mayúscula)
```

### 5. Redimensionar una imagen con snapshots

```bash
# ❌ No se puede redimensionar una imagen que tiene snapshots internos
qemu-img resize disk-with-snap.qcow2 +10G
# qemu-img: Can't resize an image which has snapshots

# ✅ Primero eliminar los snapshots, luego redimensionar
qemu-img snapshot -l disk-with-snap.qcow2    # Listar
qemu-img snapshot -d snap1 disk-with-snap.qcow2  # Eliminar
qemu-img resize disk-with-snap.qcow2 +10G    # Ahora sí
```

---

## Cheatsheet

```
┌──────────────────────────────────────────────────────────────┐
│        FORMATOS DE IMAGEN DE DISCO CHEATSHEET                │
├──────────────────────────────────────────────────────────────┤
│                                                              │
│  FORMATOS                                                    │
│  raw      Sin metadata, acceso directo, tamaño fijo/sparse  │
│  qcow2    COW, thin provisioning, snapshots, compresión     │
│  vmdk     VMware (convertible a qcow2)                      │
│  vdi      VirtualBox (convertible a qcow2)                  │
│                                                              │
│  CREAR                                                       │
│  qemu-img create -f qcow2 disk.qcow2 20G    Crear qcow2   │
│  qemu-img create -f raw disk.raw 20G         Crear raw     │
│  qemu-img create -f qcow2 -o \                             │
│    preallocation=metadata disk.qcow2 20G     Con prealloc  │
│                                                              │
│  INSPECCIONAR                                                │
│  qemu-img info disk.qcow2         Formato, tamaños, features│
│  qemu-img check disk.qcow2        Verificar integridad     │
│  qemu-img check -r all disk.qcow2 Reparar errores          │
│  qemu-img map disk.qcow2          Regiones asignadas       │
│                                                              │
│  CONVERTIR                                                   │
│  qemu-img convert -f raw -O qcow2 in.raw out.qcow2         │
│  qemu-img convert -f qcow2 -O raw in.qcow2 out.raw         │
│  qemu-img convert -f vmdk -O qcow2 in.vmdk out.qcow2       │
│  qemu-img convert -c -O qcow2 in out.qcow2   Comprimir    │
│                                                              │
│  REDIMENSIONAR                                               │
│  qemu-img resize disk.qcow2 +10G    Agrandar (+relativo)   │
│  qemu-img resize disk.qcow2 30G     Tamaño final           │
│  qemu-img resize --shrink d.qcow2 10G  Encoger (peligroso)│
│  → Después en guest: growpart + resize2fs/xfs_growfs        │
│                                                              │
│  MODIFICAR OPCIONES                                          │
│  qemu-img amend -f qcow2 -o compat=1.1 disk.qcow2         │
│  qemu-img amend -f qcow2 -o lazy_refcounts=on disk.qcow2   │
│                                                              │
│  SNAPSHOTS (interno en qcow2)                                │
│  qemu-img snapshot -c name disk.qcow2    Crear snapshot     │
│  qemu-img snapshot -l disk.qcow2         Listar snapshots   │
│  qemu-img snapshot -a name disk.qcow2    Aplicar/revertir  │
│  qemu-img snapshot -d name disk.qcow2    Eliminar           │
│                                                              │
│  TAMAÑOS                                                     │
│  ls -lh disk.qcow2      Tamaño aparente (virtual)          │
│  du -h disk.qcow2       Tamaño real (disk size)            │
│  qemu-img info disk      Ambos + metadata                   │
│                                                              │
│  ORDEN DE RESIZE                                             │
│  Agrandar: imagen primero → FS después (seguro)             │
│  Encoger:  FS primero → imagen después (peligroso)          │
└──────────────────────────────────────────────────────────────┘
```

---

## Ejercicios

### Ejercicio 1: Crear y examinar imágenes

Experimenta con qemu-img para entender los formatos:

**Tareas:**
1. Crea una imagen qcow2 de 20 GB:
   ```bash
   qemu-img create -f qcow2 /tmp/test-qcow2.qcow2 20G
   ```
2. Crea una imagen raw de 20 GB:
   ```bash
   qemu-img create -f raw /tmp/test-raw.raw 20G
   ```
3. Compara los tamaños reales de ambas:
   ```bash
   ls -lh /tmp/test-qcow2.qcow2 /tmp/test-raw.raw
   du -h /tmp/test-qcow2.qcow2 /tmp/test-raw.raw
   ```
4. Inspecciona ambas con `qemu-img info`:
   ```bash
   qemu-img info /tmp/test-qcow2.qcow2
   qemu-img info /tmp/test-raw.raw
   ```
5. Verifica la integridad de la imagen qcow2:
   ```bash
   qemu-img check /tmp/test-qcow2.qcow2
   ```
6. Crea una imagen qcow2 con preallocation metadata y compara el tamaño:
   ```bash
   qemu-img create -f qcow2 -o preallocation=metadata /tmp/test-prealloc.qcow2 20G
   du -h /tmp/test-prealloc.qcow2
   # Comparar con la imagen sin preallocation
   ```
7. Limpia:
   ```bash
   rm /tmp/test-qcow2.qcow2 /tmp/test-raw.raw /tmp/test-prealloc.qcow2
   ```

**Pregunta de reflexión:** La imagen qcow2 de 20 GB ocupa ~200 KB al crearse.
¿Cuándo empezará a crecer? ¿Hay algún momento en que la imagen qcow2 podría
ocupar **más** de 20 GB en disco? (Pista: piensa en snapshots y metadata.)

---

### Ejercicio 2: Convertir y redimensionar

Practica conversión entre formatos y redimensionado:

**Tareas:**
1. Crea una imagen raw de 5 GB:
   ```bash
   qemu-img create -f raw /tmp/conv-test.raw 5G
   ```
2. Conviértela a qcow2:
   ```bash
   qemu-img convert -f raw -O qcow2 /tmp/conv-test.raw /tmp/conv-test.qcow2
   ```
3. Verifica que la conversión fue correcta:
   ```bash
   qemu-img info /tmp/conv-test.qcow2
   qemu-img check /tmp/conv-test.qcow2
   ```
4. Agranda la imagen a 15 GB:
   ```bash
   qemu-img resize /tmp/conv-test.qcow2 +10G
   qemu-img info /tmp/conv-test.qcow2
   # virtual size debería ser 15 GiB
   ```
5. Conviértela de vuelta a raw:
   ```bash
   qemu-img convert -f qcow2 -O raw /tmp/conv-test.qcow2 /tmp/conv-back.raw
   ls -lh /tmp/conv-back.raw
   du -h /tmp/conv-back.raw
   ```
6. Crea una versión comprimida de la imagen qcow2:
   ```bash
   qemu-img convert -f qcow2 -O qcow2 -c /tmp/conv-test.qcow2 /tmp/conv-compressed.qcow2
   du -h /tmp/conv-test.qcow2 /tmp/conv-compressed.qcow2
   ```
7. Limpia:
   ```bash
   rm /tmp/conv-test.* /tmp/conv-back.raw /tmp/conv-compressed.qcow2
   ```

**Pregunta de reflexión:** Al convertir de qcow2 a raw y de vuelta a qcow2,
¿la imagen final es idéntica a la original bit a bit? ¿Por qué sí o por qué
no? ¿Importa para el funcionamiento de la VM?

---

### Ejercicio 3: Snapshots con qemu-img

Practica snapshots internos de qcow2 (sin VM corriendo):

**Tareas:**
1. Crea una imagen y un snapshot:
   ```bash
   qemu-img create -f qcow2 /tmp/snap-test.qcow2 5G
   qemu-img snapshot -c "initial" /tmp/snap-test.qcow2
   ```
2. Lista los snapshots:
   ```bash
   qemu-img snapshot -l /tmp/snap-test.qcow2
   ```
3. Crea un segundo snapshot:
   ```bash
   qemu-img snapshot -c "second" /tmp/snap-test.qcow2
   qemu-img snapshot -l /tmp/snap-test.qcow2
   ```
4. Verifica el tamaño (los snapshots añaden metadata):
   ```bash
   qemu-img info /tmp/snap-test.qcow2
   ```
5. Intenta redimensionar (debería fallar con snapshots):
   ```bash
   qemu-img resize /tmp/snap-test.qcow2 +5G
   # Debería mostrar error sobre snapshots
   ```
6. Elimina un snapshot y reintenta:
   ```bash
   qemu-img snapshot -d "second" /tmp/snap-test.qcow2
   qemu-img snapshot -d "initial" /tmp/snap-test.qcow2
   qemu-img resize /tmp/snap-test.qcow2 +5G
   qemu-img info /tmp/snap-test.qcow2
   ```
7. Limpia:
   ```bash
   rm /tmp/snap-test.qcow2
   ```

**Pregunta de reflexión:** Los snapshots de qemu-img se crean sobre la imagen
offline (VM apagada). virsh también puede crear snapshots con la VM corriendo.
¿Cuál es la diferencia entre un snapshot offline y uno online? ¿Qué pasa con
los datos en RAM y los buffers del filesystem del guest en cada caso?
