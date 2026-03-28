# dd — Clonar Discos, Crear Imágenes y Operaciones de Bajo Nivel

## Índice

1. [¿Qué es dd?](#1-qué-es-dd)
2. [Anatomía del comando dd](#2-anatomía-del-comando-dd)
3. [Block size: el parámetro que más importa](#3-block-size-el-parámetro-que-más-importa)
4. [Clonar discos y particiones](#4-clonar-discos-y-particiones)
5. [Crear y restaurar imágenes de disco](#5-crear-y-restaurar-imágenes-de-disco)
6. [Crear medios de instalación](#6-crear-medios-de-instalación)
7. [Operaciones especiales con dd](#7-operaciones-especiales-con-dd)
8. [Borrado seguro](#8-borrado-seguro)
9. [Precauciones y seguridad](#9-precauciones-y-seguridad)
10. [Alternativas modernas a dd](#10-alternativas-modernas-a-dd)
11. [Errores comunes](#11-errores-comunes)
12. [Cheatsheet](#12-cheatsheet)
13. [Ejercicios](#13-ejercicios)

---

## 1. ¿Qué es dd?

`dd` (**Data Duplicator**, o según la tradición, **"Disk Destroyer"**) es una herramienta de copia a nivel de bloques. A diferencia de `cp`, `tar` o `rsync` que trabajan con archivos y filesystems, dd trabaja directamente con **bytes crudos** — lee bloques de un dispositivo o archivo y los escribe en otro, sin interpretar su contenido.

```
cp / rsync / tar                    dd
┌─────────────────────┐           ┌─────────────────────┐
│  Trabajan a nivel    │           │  Trabaja a nivel     │
│  de FILESYSTEM       │           │  de BLOQUES          │
│                      │           │                      │
│  Ven: archivos,      │           │  Ve: secuencia de    │
│  directorios,        │           │  bytes crudos        │
│  permisos, nombres   │           │                      │
│                      │           │  No interpreta:      │
│  Requieren: FS       │           │  particiones, FS,    │
│  montado             │           │  archivos            │
└─────────────────────┘           └─────────────────────┘

Caso de uso:                       Caso de uso:
  Copiar archivos                    Clonar disco completo
  Sincronizar directorios            Crear imagen bit-a-bit
  Hacer backups de datos             Escribir ISO a USB
                                     Leer/escribir MBR
                                     Borrado seguro
```

### Por qué dd es necesario

Hay operaciones que **solo** dd (o herramientas similares de bajo nivel) puede hacer:

- Clonar un disco **incluyendo** tabla de particiones, boot sector, espacio no asignado
- Crear una imagen exacta bit-a-bit para análisis forense
- Escribir una imagen ISO directamente a un dispositivo USB
- Leer o escribir el MBR (primeros 512 bytes de un disco)
- Crear archivos de tamaño exacto (swap files, archivos de prueba)
- Borrado seguro sobrescribiendo con datos aleatorios o ceros

---

## 2. Anatomía del comando dd

dd usa una sintaxis inusual heredada de IBM JCL (Job Control Language), con operandos `clave=valor` sin guiones:

```bash
dd if=ORIGEN of=DESTINO bs=TAMAÑO count=BLOQUES [opciones...]
```

### Operandos principales

| Operando | Descripción | Default |
|----------|-------------|---------|
| `if=` | Input file (origen) | stdin |
| `of=` | Output file (destino) | stdout |
| `bs=` | Block size (lectura y escritura) | 512 bytes |
| `ibs=` | Input block size | 512 bytes |
| `obs=` | Output block size | 512 bytes |
| `count=` | Número de bloques a copiar | Todo |
| `skip=` | Saltar N bloques al inicio del input | 0 |
| `seek=` | Saltar N bloques al inicio del output | 0 |
| `conv=` | Conversiones (lista separada por comas) | Ninguna |
| `status=` | Nivel de información | default |

### Sufijos de tamaño

```
Sufijo    Valor          Ejemplo
──────────────────────────────────────
c         1 byte         bs=512c  (= 512 bytes)
w         2 bytes
b         512 bytes      bs=1b   (= 512 bytes)
K         1024 bytes     bs=4K   (= 4096 bytes)
M         1048576        bs=1M   (= 1 MiB)
G         1073741824     bs=1G   (= 1 GiB)

Multiplicadores:
  bs=4Kx256   = 4096 × 256 = 1 MiB
```

### Opciones de conversión (conv=)

| Conversión | Descripción |
|------------|-------------|
| `notrunc` | No truncar el archivo de salida |
| `noerror` | Continuar después de errores de lectura |
| `sync` | Rellenar bloques con lectura parcial con NUL |
| `fsync` | Flush datos al dispositivo antes de terminar |
| `fdatasync` | Flush datos (sin metadata) antes de terminar |
| `excl` | Fallar si el output ya existe |
| `nocreat` | No crear el archivo de salida (debe existir) |
| `swab` | Intercambiar cada par de bytes |
| `ucase` | Convertir a mayúsculas |
| `lcase` | Convertir a minúsculas |

### Status

```bash
# Sin información de progreso (default)
dd if=/dev/sda of=disk.img

# Mostrar progreso periódico
dd if=/dev/sda of=disk.img status=progress

# Solo mostrar resultado final (sin warnings)
dd if=/dev/sda of=disk.img status=none

# Enviar señal para obtener progreso en versiones antiguas
kill -USR1 $(pidof dd)
```

---

## 3. Block size: el parámetro que más importa

El block size (`bs=`) es el factor que más afecta el rendimiento de dd. Un bs demasiado pequeño genera miles de syscalls; uno demasiado grande puede desperdiciar memoria sin beneficio.

```
bs=512 (default):
  ┌─────┐ ┌─────┐ ┌─────┐ ┌─────┐ ... × miles
  │ 512 │→│ 512 │→│ 512 │→│ 512 │→
  └─────┘ └─────┘ └─────┘ └─────┘
  Muchos syscalls (read+write) = LENTO

bs=4M:
  ┌──────────────────────────────┐ ┌──────────────────────────────┐ ...
  │          4 MiB               │→│          4 MiB               │→
  └──────────────────────────────┘ └──────────────────────────────┘
  Pocos syscalls = RÁPIDO
```

### Impacto real del block size

```
Block size     Syscalls/GB    Throughput típico (HDD)
──────────────────────────────────────────────────────
512 (default)  ~2,097,152     ~5-10 MB/s
4K             ~262,144       ~50-80 MB/s
64K            ~16,384        ~100-120 MB/s
1M             ~1,024         ~130-150 MB/s
4M             ~256           ~140-160 MB/s
16M            ~64            ~140-160 MB/s (diminishing returns)
```

### Recomendaciones

| Operación | bs recomendado | Por qué |
|-----------|---------------|---------|
| Clonar HDD | `bs=4M` o `bs=64K` | Balance entre velocidad y memoria |
| Clonar SSD/NVMe | `bs=1M` a `bs=4M` | SSDs manejan bien bloques grandes |
| Escribir ISO a USB | `bs=4M` | Estándar de facto |
| Leer MBR | `bs=512 count=1` | MBR son exactamente 512 bytes |
| Crear swap file | `bs=1M` | Rápido para archivos grandes |
| Recovery (disco dañado) | `bs=4K` | Minimizar datos perdidos por sector malo |

> **Pregunta de predicción**: Si clonas un disco de 500 GB con `dd if=/dev/sda of=/dev/sdb` (sin especificar bs), ¿cuántos syscalls de read+write realizará dd? ¿Y con `bs=4M`?

Sin especificar bs (default 512): 500 GB / 512 B = ~1,000,000,000 de reads + ~1,000,000,000 de writes = ~2 mil millones de syscalls. Con `bs=4M`: 500 GB / 4 MiB = ~125,000 de reads + ~125,000 de writes = ~250,000 syscalls. La diferencia en velocidad puede ser de 10-30x.

---

## 4. Clonar discos y particiones

### Clonar un disco completo a otro

```bash
# PRECAUCIÓN: el disco destino se sobrescribe COMPLETAMENTE
# Verificar dispositivos ANTES de ejecutar
lsblk
# NAME   SIZE TYPE MOUNTPOINTS
# sda    500G disk
# ├─sda1   1G part /boot
# ├─sda2  50G part /
# └─sda3 449G part /home
# sdb    500G disk              ← destino (vacío o sacrificable)

# Clonar disco completo (incluye MBR/GPT, particiones, espacio libre)
sudo dd if=/dev/sda of=/dev/sdb bs=4M status=progress conv=fsync

# conv=fsync: asegura que los datos se escriban al disco antes de terminar
```

### Clonar una partición

```bash
# Clonar solo una partición
sudo dd if=/dev/sda2 of=/dev/sdb2 bs=4M status=progress conv=fsync

# El filesystem destino tendrá el mismo UUID que el origen
# Cambiar UUID después del clone:
sudo xfs_admin -U generate /dev/sdb2    # XFS
sudo tune2fs -U random /dev/sdb2        # ext4
```

### Clonar a un disco más grande

```bash
# dd copia bit-a-bit; si el destino es más grande, el espacio extra
# queda sin usar. Después del clone:

# 1. Expandir la tabla de particiones
sudo parted /dev/sdb resizepart 3 100%

# 2. Expandir el filesystem
sudo resize2fs /dev/sdb3       # ext4
sudo xfs_growfs /mountpoint    # XFS (montar primero)
```

### Clonar a un disco más pequeño

```
⚠ PELIGRO: dd NO verifica tamaños. Si el destino es más pequeño,
dd escribe hasta llenar el destino y los datos restantes se pierden.

Solución:
1. Reducir el filesystem y la partición ANTES de clonar
2. Verificar que los datos caben en el disco destino
3. Usar herramientas de partición (gparted) que sí verifican
```

---

## 5. Crear y restaurar imágenes de disco

### Crear imagen de disco

```bash
# Imagen raw (bit-a-bit, tamaño = disco completo)
sudo dd if=/dev/sda of=/backup/sda.img bs=4M status=progress

# Imagen comprimida (mucho más pequeña)
sudo dd if=/dev/sda bs=4M status=progress | gzip -c > /backup/sda.img.gz
sudo dd if=/dev/sda bs=4M status=progress | zstd -T0 -o /backup/sda.img.zst

# Imagen de una partición
sudo dd if=/dev/sda1 of=/backup/boot.img bs=4M status=progress
```

### Restaurar imagen

```bash
# Desde imagen raw
sudo dd if=/backup/sda.img of=/dev/sda bs=4M status=progress conv=fsync

# Desde imagen comprimida
gunzip -c /backup/sda.img.gz | sudo dd of=/dev/sda bs=4M status=progress conv=fsync
zstd -d /backup/sda.img.zst --stdout | sudo dd of=/dev/sda bs=4M status=progress conv=fsync
```

### Montar una imagen de disco sin restaurarla

```bash
# Imagen de partición: montar directamente
sudo mount -o loop,ro /backup/boot.img /mnt/

# Imagen de disco completo: necesitas offset de la partición
fdisk -l /backup/sda.img
# Device          Start      End  Sectors  Size Type
# sda.img1         2048  2099199  2097152    1G EFI System
# sda.img2      2099200 106856447 104757248  50G Linux filesystem

# Calcular offset: Start × 512 (sector size)
# Partición 2: 2099200 × 512 = 1074790400
sudo mount -o loop,ro,offset=1074790400 /backup/sda.img /mnt/

# Alternativamente, usar losetup para mapear todas las particiones
sudo losetup -fP /backup/sda.img
# Crea /dev/loop0, /dev/loop0p1, /dev/loop0p2, etc.
lsblk /dev/loop0
sudo mount -o ro /dev/loop0p2 /mnt/
# Liberar al terminar
sudo umount /mnt/
sudo losetup -d /dev/loop0
```

### Reducir tamaño de imagen: rellenar espacio libre con ceros

```bash
# Antes de crear la imagen, llenar espacio libre con ceros
# (el compresor los comprimirá eficientemente)

# En el sistema origen (montado):
sudo dd if=/dev/zero of=/mountpoint/zero.fill bs=1M status=progress || true
sudo rm /mountpoint/zero.fill
sudo sync

# Ahora crear la imagen comprimida (mucho más pequeña)
sudo dd if=/dev/sda bs=4M | gzip -c > sda.img.gz
```

---

## 6. Crear medios de instalación

### Escribir ISO a USB

```bash
# 1. Identificar el dispositivo USB (NO la partición)
lsblk
# NAME   SIZE TYPE MOUNTPOINTS
# sda    500G disk /
# sdb    16G  disk             ← USB
# └─sdb1  16G part /media/usb

# 2. Desmontar si está montado
sudo umount /dev/sdb1

# 3. Escribir la ISO directamente al DISCO (no a la partición)
sudo dd if=fedora-41.iso of=/dev/sdb bs=4M status=progress conv=fsync

# conv=fsync: fundamental — asegura que el write cache se vacíe
# antes de que dd termine. Sin esto, podrías retirar el USB
# antes de que todos los datos se hayan escrito realmente
```

```
⚠ IMPORTANTE: el destino es /dev/sdb, NO /dev/sdb1
   La ISO contiene su propia tabla de particiones que se
   escribe sobre la existente en el USB.

   /dev/sdb   ← CORRECTO (disco completo)
   /dev/sdb1  ← INCORRECTO (solo una partición)
```

### Verificar la escritura

```bash
# Método 1: comparar checksum de la ISO con lo escrito
ISO_SIZE=$(stat -c %s fedora-41.iso)
ISO_BLOCKS=$((ISO_SIZE / 4194304))  # dividir por bs=4M

sha256sum fedora-41.iso
sudo dd if=/dev/sdb bs=4M count=$ISO_BLOCKS status=none | sha256sum
# Los hashes deben coincidir

# Método 2: usar cmp
sudo cmp fedora-41.iso /dev/sdb -n "$ISO_SIZE"
# Sin salida = idénticos
```

### Alternativa moderna: cp funciona para ISOs

```bash
# En kernels modernos, cp también funciona para escribir ISOs
sudo cp fedora-41.iso /dev/sdb
sudo sync
# Más simple, pero sin progreso ni control de bs
```

---

## 7. Operaciones especiales con dd

### Leer y escribir el MBR

```bash
# Backup del MBR (primeros 512 bytes)
sudo dd if=/dev/sda of=mbr_backup.bin bs=512 count=1

# El MBR contiene:
# Bytes 0-445:   Bootstrap code (bootloader)
# Bytes 446-509: Partition table (4 entradas × 16 bytes)
# Bytes 510-511: Boot signature (0x55AA)

# Restaurar MBR completo
sudo dd if=mbr_backup.bin of=/dev/sda bs=512 count=1 conv=notrunc

# Restaurar SOLO el bootstrap (sin tocar la tabla de particiones)
sudo dd if=mbr_backup.bin of=/dev/sda bs=446 count=1 conv=notrunc

# Restaurar SOLO la tabla de particiones
sudo dd if=mbr_backup.bin of=/dev/sda bs=1 skip=446 seek=446 count=64 conv=notrunc
```

```
MBR: 512 bytes
┌────────────────────────────────────────────────┐
│  Bootstrap code (446 bytes)                    │ ← Bootloader stage 1
├────────────────────────────────────────────────┤
│  Partition entry 1 (16 bytes)                  │
│  Partition entry 2 (16 bytes)                  │ ← Tabla de particiones
│  Partition entry 3 (16 bytes)                  │
│  Partition entry 4 (16 bytes)                  │
├────────────────────────────────────────────────┤
│  Boot signature: 0x55 0xAA (2 bytes)           │
└────────────────────────────────────────────────┘
```

### Crear un archivo swap

```bash
# Crear archivo de swap de 4 GiB
sudo dd if=/dev/zero of=/swapfile bs=1M count=4096 status=progress
sudo chmod 600 /swapfile
sudo mkswap /swapfile
sudo swapon /swapfile

# Verificar
swapon --show
```

### Crear archivos de tamaño específico

```bash
# Archivo lleno de ceros (1 GiB)
dd if=/dev/zero of=testfile.bin bs=1M count=1024

# Archivo sparse (parece grande pero no ocupa espacio real)
dd if=/dev/zero of=sparse.img bs=1 count=0 seek=10G
ls -lh sparse.img   # Muestra 10 GiB
du -h sparse.img     # Muestra 0 (sparse)

# Archivo con datos aleatorios (para testing de IO)
dd if=/dev/urandom of=random.bin bs=1M count=100
```

### Benchmark básico de disco

```bash
# Velocidad de escritura
dd if=/dev/zero of=/tmp/testwrite bs=1M count=1024 conv=fdatasync status=progress
# conv=fdatasync asegura que mides escritura real, no cache
# Ejemplo de salida: 1073741824 bytes copied, 5.2 s, 207 MB/s

# Velocidad de lectura (limpiar cache primero)
sudo sh -c 'echo 3 > /proc/sys/vm/drop_caches'
dd if=/tmp/testwrite of=/dev/null bs=1M status=progress
# Ejemplo de salida: 1073741824 bytes copied, 2.1 s, 511 MB/s

# Limpiar
rm /tmp/testwrite
```

### Recuperar datos con skip y seek

```bash
# Extraer los bytes 1024 a 2048 de un archivo
dd if=archivo.bin of=fragmento.bin bs=1 skip=1024 count=1024

# Más eficiente con bloques alineados
dd if=archivo.bin of=fragmento.bin bs=512 skip=2 count=2
# skip=2 × 512 = byte 1024, count=2 × 512 = 1024 bytes
```

---

## 8. Borrado seguro

### Sobrescribir con ceros

```bash
# Borrar un disco completo con ceros
sudo dd if=/dev/zero of=/dev/sdb bs=4M status=progress

# Borrar solo el espacio libre de una partición montada
dd if=/dev/zero of=/mount/zero.fill bs=1M status=progress || true
rm /mount/zero.fill
```

### Sobrescribir con datos aleatorios

```bash
# Una pasada de datos aleatorios
sudo dd if=/dev/urandom of=/dev/sdb bs=4M status=progress

# /dev/urandom es lento (~80 MB/s); alternativa más rápida:
sudo openssl enc -aes-256-ctr -pass pass:"$(head -c 32 /dev/urandom | xxd -p)" \
    -nosalt < /dev/zero | sudo dd of=/dev/sdb bs=4M status=progress
```

### Nota sobre SSDs y borrado seguro

```
⚠ En SSDs, dd NO garantiza borrado seguro:
  - El SSD tiene más capacidad física que lógica (overprovisioning)
  - El firmware puede reubicar bloques (wear leveling)
  - Datos pueden quedar en bloques "retirados"

  Para SSDs, usar en su lugar:
  - hdparm --security-erase /dev/sdX    (ATA Secure Erase)
  - blkdiscard -s /dev/sdX              (Secure discard, si soportado)
  - nvme format /dev/nvme0 --ses=1      (NVMe Secure Erase)
```

---

## 9. Precauciones y seguridad

### La regla de oro: dd no pide confirmación

```
dd NUNCA pregunta "¿Estás seguro?"

  sudo dd if=/dev/zero of=/dev/sda bs=4M
  ─── ejecuta inmediatamente ───
  ─── destruye el disco sda ────
  ─── sin ningún warning ───────

Por eso se le llama "Disk Destroyer"
```

### Checklist de seguridad antes de ejecutar dd

```
1. ☐ Verificar dispositivos con lsblk (NO confiar en la memoria)
2. ☐ Confirmar que if= y of= son correctos
   (invertir if/of sobrescribe tu origen con basura)
3. ☐ Verificar que of= NO es tu disco de sistema
4. ☐ Si of= es un disco, ¿está desmontado?
5. ☐ Tener backup del destino si contiene datos
6. ☐ Verificar espacio disponible si of= es un archivo
```

### Errores catastróficos y cómo evitarlos

```bash
# CATASTRÓFICO: if y of invertidos — sobrescribes tu disco fuente
sudo dd if=/dev/sdb of=/dev/sda bs=4M    # DESTRUYE sda

# CATASTRÓFICO: destino equivocado
sudo dd if=image.iso of=/dev/sda bs=4M   # Sobrescribe disco de sistema
# Querías /dev/sdb (el USB)

# PROTECCIÓN: usar conv=excl para no sobrescribir archivos existentes
dd if=/dev/sda of=disk.img bs=4M conv=excl
# Falla si disk.img ya existe

# PROTECCIÓN: montar disco de sistema como read-only durante operaciones
# (no siempre posible si es el disco donde estás booteado)
```

### Verificar dispositivos antes de operar

```bash
# SIEMPRE verificar con lsblk antes de dd
lsblk -o NAME,SIZE,TYPE,MOUNTPOINTS,MODEL

# Ejemplo de salida:
# NAME   SIZE TYPE MOUNTPOINTS       MODEL
# sda    500G disk                   Samsung SSD 870
# ├─sda1   1G part /boot/efi
# ├─sda2   1G part /boot
# └─sda3 498G part /                                ← NO tocar
# sdb     16G disk                   USB Flash Drive
# └─sdb1  16G part /media/usb                       ← destino

# Verificar con blkid para confirmar
sudo blkid /dev/sdb
```

---

## 10. Alternativas modernas a dd

### Para clonar discos

| Herramienta | Ventaja sobre dd | Cuándo usarla |
|-------------|-------------------|---------------|
| `partclone` | Solo copia bloques usados (mucho más rápido) | Clonar particiones con filesystem conocido |
| `clonezilla` | GUI/TUI, compresión, multicast | Clonación masiva de estaciones |
| `pv + dd` | Barra de progreso real | Cuando necesitas progreso detallado |

### Para escribir ISOs

```bash
# Fedora Media Writer, Balena Etcher, Ventoy
# Son interfaces gráficas que llaman a dd internamente

# cp funciona igual que dd para ISOs:
sudo cp image.iso /dev/sdb && sudo sync
```

### Para benchmarking

```bash
# fio es mucho más preciso que dd para benchmarks
fio --name=test --rw=write --bs=1M --size=1G --numjobs=1 --runtime=30
```

### pv: añadir progreso a dd

```bash
# pv (pipe viewer) muestra una barra de progreso real
sudo pv /dev/sda | dd of=disk.img bs=4M
# 12.5GiB 0:01:23 [ 156MiB/s] [========>                    ] 25% ETA 0:04:09

# También se puede usar entre pipes
sudo dd if=/dev/sda bs=4M | pv -s 500G | gzip > disk.img.gz
```

### dcfldd: dd para forense

```bash
# dcfldd: versión de dd del Department of Defense Cyber Crime Center
# Añade hashing en tiempo real y progreso

sudo dcfldd if=/dev/sda of=evidence.img \
    hash=sha256 \
    hashwindow=1G \
    hashlog=evidence.hash \
    bs=4M

# Genera hashes SHA-256 cada GiB — ideal para cadena de custodia
```

---

## 11. Errores comunes

### Error 1: No especificar block size

```bash
# MAL — usa bs=512 por defecto, extremadamente lento
sudo dd if=/dev/sda of=disk.img

# BIEN — bs=4M es el estándar para operaciones de disco
sudo dd if=/dev/sda of=disk.img bs=4M status=progress
```

### Error 2: Olvidar conv=fsync al escribir a USB

```bash
# MAL — dd termina pero los datos pueden estar en write cache
sudo dd if=image.iso of=/dev/sdb bs=4M status=progress
# Si retiras el USB ahora, puede estar corrupto

# BIEN — fsync fuerza el flush al dispositivo
sudo dd if=image.iso of=/dev/sdb bs=4M status=progress conv=fsync

# ALTERNATIVA — sync después de dd
sudo dd if=image.iso of=/dev/sdb bs=4M status=progress
sudo sync
```

### Error 3: Escribir a una partición en vez del disco (para ISOs)

```bash
# MAL — escribe la ISO sobre la partición, no arrancará
sudo dd if=image.iso of=/dev/sdb1 bs=4M

# BIEN — escribir al disco completo
sudo dd if=image.iso of=/dev/sdb bs=4M status=progress conv=fsync
# La ISO contiene su propia tabla de particiones
```

### Error 4: Invertir if y of

```bash
# CATASTRÓFICO — sobrescribe tu disco fuente con datos del destino
sudo dd if=/dev/sdb of=/dev/sda bs=4M
# Querías clonar sda → sdb, pero hiciste sdb → sda

# TRUCO: leer el comando en voz alta antes de ejecutar
# "input FROM sda, output TO sdb" ← verificar lógica
```

### Error 5: Usar dd para copiar archivos normales

```bash
# INNECESARIO — dd funciona pero cp es más simple y seguro
dd if=archivo.txt of=copia.txt bs=4M

# MEJOR — cp es la herramienta correcta para archivos
cp archivo.txt copia.txt

# dd es para operaciones de BAJO NIVEL donde cp no puede operar:
# dispositivos de bloque, rangos de bytes, MBR, etc.
```

---

## 12. Cheatsheet

```
CLONAR
  dd if=/dev/sda of=/dev/sdb bs=4M status=progress conv=fsync
      Clonar disco sda a sdb
  dd if=/dev/sda2 of=/dev/sdb2 bs=4M status=progress conv=fsync
      Clonar partición

IMÁGENES
  dd if=/dev/sda of=disk.img bs=4M status=progress
      Crear imagen raw
  dd if=/dev/sda bs=4M | gzip > disk.img.gz
      Crear imagen comprimida
  gunzip -c disk.img.gz | dd of=/dev/sda bs=4M conv=fsync
      Restaurar imagen comprimida
  mount -o loop,ro,offset=N disk.img /mnt/
      Montar partición desde imagen

ISO A USB
  dd if=distro.iso of=/dev/sdX bs=4M status=progress conv=fsync
      Escribir ISO (sdX = disco, NO partición)

MBR
  dd if=/dev/sda of=mbr.bin bs=512 count=1
      Backup MBR
  dd if=mbr.bin of=/dev/sda bs=446 count=1 conv=notrunc
      Restaurar bootstrap (sin tabla de particiones)
  dd if=mbr.bin of=/dev/sda bs=512 count=1 conv=notrunc
      Restaurar MBR completo

ARCHIVOS ESPECIALES
  dd if=/dev/zero of=/swapfile bs=1M count=4096
      Crear archivo para swap (4 GiB)
  dd if=/dev/zero of=testfile bs=1 count=0 seek=10G
      Crear archivo sparse (10 GiB virtual, 0 real)

BORRADO
  dd if=/dev/zero of=/dev/sdX bs=4M status=progress
      Llenar con ceros
  dd if=/dev/urandom of=/dev/sdX bs=4M status=progress
      Llenar con aleatorios

BENCHMARK
  dd if=/dev/zero of=/tmp/test bs=1M count=1024 conv=fdatasync
      Test de escritura
  dd if=/tmp/test of=/dev/null bs=1M
      Test de lectura

VERIFICAR
  lsblk -o NAME,SIZE,TYPE,MOUNTPOINTS,MODEL
      Identificar dispositivos antes de operar
```

---

## 13. Ejercicios

### Ejercicio 1: Operaciones básicas con dd

**Objetivo**: Familiarizarse con dd en un entorno seguro usando archivos regulares (no discos reales).

```bash
# 1. Crear un archivo de prueba con contenido conocido
echo "AAAA BBBB CCCC DDDD EEEE FFFF" > /tmp/dd-test-input.txt
cat /tmp/dd-test-input.txt
xxd /tmp/dd-test-input.txt | head -3

# 2. Copiar con dd (equivalente a cp)
dd if=/tmp/dd-test-input.txt of=/tmp/dd-test-copy.txt
diff /tmp/dd-test-input.txt /tmp/dd-test-copy.txt

# 3. Extraer los primeros 10 bytes
dd if=/tmp/dd-test-input.txt of=/tmp/dd-first10.txt bs=1 count=10
xxd /tmp/dd-first10.txt

# 4. Extraer bytes 5-14 (skip 5, count 10)
dd if=/tmp/dd-test-input.txt of=/tmp/dd-mid.txt bs=1 skip=5 count=10
xxd /tmp/dd-mid.txt

# 5. Comparar rendimiento con diferentes block sizes
dd if=/dev/zero of=/tmp/dd-bench bs=512 count=200000 2>&1 | tail -1
dd if=/dev/zero of=/tmp/dd-bench bs=4K count=25000 2>&1 | tail -1
dd if=/dev/zero of=/tmp/dd-bench bs=1M count=100 2>&1 | tail -1
# Comparar las velocidades (bytes/second)

# 6. Limpiar
rm /tmp/dd-test-* /tmp/dd-first10.txt /tmp/dd-mid.txt /tmp/dd-bench
```

**Pregunta de reflexión**: En el paso 5, los tres comandos escriben la misma cantidad de datos (~100 MB). ¿Por qué `bs=1M` es mucho más rápido que `bs=512`? ¿Qué recurso del sistema se está desperdiciando con el block size pequeño?

> Cada operación de `read()` + `write()` es un **syscall** que requiere cambiar del espacio de usuario al espacio de kernel y viceversa (context switch). Con `bs=512`, dd hace ~200,000 pares de syscalls. Con `bs=1M`, solo hace ~100 pares. Los syscalls son el cuello de botella, no el disco. Se desperdicia **tiempo de CPU** en overhead del kernel, no ancho de banda de I/O.

---

### Ejercicio 2: Backup y restauración de un MBR simulado

**Objetivo**: Practicar la manipulación de estructuras de bajo nivel con dd usando un archivo en vez de un disco real.

```bash
# 1. Crear un "disco virtual" de 10 MiB
dd if=/dev/zero of=/tmp/fake-disk.img bs=1M count=10

# 2. Escribir un "MBR" simulado: bootstrap + signature
# Llenar los primeros 446 bytes con un patrón reconocible
printf '%0.s\xAB' $(seq 1 446) | dd of=/tmp/fake-disk.img bs=1 count=446 conv=notrunc
# Escribir boot signature 0x55AA en bytes 510-511
printf '\x55\xAA' | dd of=/tmp/fake-disk.img bs=1 seek=510 count=2 conv=notrunc

# 3. Verificar la estructura
xxd /tmp/fake-disk.img | head -33    # Ver primeros 512+ bytes
xxd -s 510 -l 2 /tmp/fake-disk.img  # Verificar firma 55AA

# 4. Backup del MBR (512 bytes)
dd if=/tmp/fake-disk.img of=/tmp/mbr-backup.bin bs=512 count=1
xxd /tmp/mbr-backup.bin | tail -3

# 5. "Corromper" el MBR del disco virtual
dd if=/dev/zero of=/tmp/fake-disk.img bs=512 count=1 conv=notrunc
xxd -s 510 -l 2 /tmp/fake-disk.img  # Firma borrada (0000)

# 6. Restaurar solo el bootstrap (446 bytes, sin tabla de particiones)
dd if=/tmp/mbr-backup.bin of=/tmp/fake-disk.img bs=446 count=1 conv=notrunc

# 7. Restaurar la firma de boot (bytes 510-511)
dd if=/tmp/mbr-backup.bin of=/tmp/fake-disk.img bs=1 skip=510 seek=510 count=2 conv=notrunc

# 8. Verificar restauración
xxd -s 510 -l 2 /tmp/fake-disk.img  # Debería ser 55AA de nuevo

# 9. Limpiar
rm /tmp/fake-disk.img /tmp/mbr-backup.bin
```

**Pregunta de reflexión**: En el paso 6 usamos `conv=notrunc`. ¿Qué pasaría si lo omitimos? ¿Cuál sería el tamaño del archivo resultante?

> Sin `conv=notrunc`, dd **trunca** el archivo de salida al tamaño de los datos escritos. Escribir 446 bytes sin `notrunc` reduciría el "disco virtual" de 10 MiB a solo 446 bytes, destruyendo todo el contenido posterior. `conv=notrunc` dice "escribe en el archivo existente sin cambiar su tamaño" — esencial cuando modificas una porción de un archivo o dispositivo sin alterar el resto.

---

### Ejercicio 3: Crear imagen de loopback con filesystem

**Objetivo**: Crear una imagen de disco, particionarla, formatearla y montarla — todo con archivos regulares.

```bash
# 1. Crear una imagen de disco de 100 MiB
dd if=/dev/zero of=/tmp/disk-lab.img bs=1M count=100

# 2. Crear una tabla de particiones MBR con una partición
# (fdisk funciona con archivos de imagen)
echo -e "n\np\n1\n\n\nw" | fdisk /tmp/disk-lab.img
fdisk -l /tmp/disk-lab.img

# 3. Asociar la imagen a un loop device con particiones
sudo losetup -fP /tmp/disk-lab.img
LOOP=$(losetup -l | grep disk-lab.img | awk '{print $1}')
echo "Loop device: $LOOP"
lsblk "$LOOP"

# 4. Formatear la partición
sudo mkfs.ext4 "${LOOP}p1"

# 5. Montar y escribir datos
sudo mkdir -p /mnt/disk-lab
sudo mount "${LOOP}p1" /mnt/disk-lab
sudo sh -c 'echo "Hello from dd lab" > /mnt/disk-lab/test.txt'
sudo sh -c 'date > /mnt/disk-lab/timestamp.txt'
ls -la /mnt/disk-lab/

# 6. Desmontar
sudo umount /mnt/disk-lab

# 7. Crear backup comprimido de la imagen
dd if=/tmp/disk-lab.img bs=4M | gzip > /tmp/disk-lab.img.gz
ls -lh /tmp/disk-lab.img /tmp/disk-lab.img.gz
# La imagen comprimida es mucho más pequeña (la mayoría son ceros)

# 8. Verificar: restaurar la imagen y comprobar datos
gunzip -c /tmp/disk-lab.img.gz | dd of=/tmp/disk-lab-restored.img bs=4M
sudo losetup -fP /tmp/disk-lab-restored.img
LOOP2=$(losetup -l | grep disk-lab-restored | awk '{print $1}')
sudo mount -o ro "${LOOP2}p1" /mnt/disk-lab
cat /mnt/disk-lab/test.txt  # Debería mostrar "Hello from dd lab"

# 9. Limpiar
sudo umount /mnt/disk-lab
sudo losetup -d "$LOOP" "$LOOP2"
sudo rmdir /mnt/disk-lab
rm /tmp/disk-lab.img /tmp/disk-lab.img.gz /tmp/disk-lab-restored.img
```

**Pregunta de reflexión**: La imagen original es de 100 MiB pero la imagen comprimida es mucho más pequeña. ¿Por qué? ¿Qué pasaría con la ratio de compresión si antes de crear la imagen hubieras llenado todo el espacio libre con `dd if=/dev/urandom` en vez de que sean ceros?

> La imagen está mayoritariamente llena de **ceros** (`/dev/zero`): solo la tabla de particiones, el filesystem ext4, y los dos archivos de texto tienen datos reales. Los ceros se comprimen extremadamente bien (gzip los reduce a casi nada). Si hubieras llenado con `/dev/urandom`, los datos aleatorios son **incompresibles** — la imagen comprimida sería prácticamente del mismo tamaño que la original (100 MiB), porque la compresión no puede encontrar patrones repetitivos en datos aleatorios.

---

> **Siguiente tema**: T04 — Estrategias de backup: completo, incremental, diferencial, rotación, 3-2-1.
