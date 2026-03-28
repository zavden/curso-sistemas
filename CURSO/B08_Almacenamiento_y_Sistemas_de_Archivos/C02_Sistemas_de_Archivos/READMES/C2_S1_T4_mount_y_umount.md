# mount y umount

## Índice

1. [Qué es montar un filesystem](#1-qué-es-montar-un-filesystem)
2. [mount: uso básico](#2-mount-uso-básico)
3. [Opciones de montaje](#3-opciones-de-montaje)
4. [umount: desmontar](#4-umount-desmontar)
5. [Montar dispositivos especiales](#5-montar-dispositivos-especiales)
6. [findmnt y mount sin argumentos](#6-findmnt-y-mount-sin-argumentos)
7. [Bind mounts](#7-bind-mounts)
8. [Remount: cambiar opciones en caliente](#8-remount-cambiar-opciones-en-caliente)
9. [Errores comunes](#9-errores-comunes)
10. [Cheatsheet](#10-cheatsheet)
11. [Ejercicios](#11-ejercicios)

---

## 1. Qué es montar un filesystem

Montar es asociar un filesystem (que vive en un dispositivo de bloque) a un
directorio del árbol de directorios. Sin montar, el filesystem existe pero
no es accesible:

```
┌──────────────────────────────────────────────────────────────┐
│  Sin montar:                                                 │
│                                                              │
│  /dev/vdb1 tiene un filesystem ext4 con datos               │
│  Pero no está conectado a ningún directorio                  │
│  → no puedes acceder a los archivos                          │
│                                                              │
│  Después de mount /dev/vdb1 /mnt:                            │
│                                                              │
│  /                                                           │
│  ├── home/                                                   │
│  ├── etc/                                                    │
│  ├── mnt/ ◄──── /dev/vdb1 (ext4)                             │
│  │   ├── archivo1.txt     ← ahora accesibles                │
│  │   └── archivo2.txt                                        │
│  └── tmp/                                                    │
│                                                              │
│  El directorio /mnt se convierte en la "ventana" al          │
│  contenido de /dev/vdb1                                      │
└──────────────────────────────────────────────────────────────┘
```

El directorio donde se monta se llama **mount point**. Puede ser cualquier
directorio existente y vacío (si tiene contenido, queda oculto mientras el
filesystem esté montado).

---

## 2. mount: uso básico

### 2.1. Montar un dispositivo

```bash
# Sintaxis básica
sudo mount /dev/vdb1 /mnt

# mount detecta el tipo de filesystem automáticamente
# Es equivalente a:
sudo mount -t ext4 /dev/vdb1 /mnt
```

### 2.2. Especificar el tipo de filesystem

```bash
sudo mount -t ext4 /dev/vdb1 /mnt
sudo mount -t xfs /dev/vdc1 /mnt
sudo mount -t btrfs /dev/vdd /mnt
sudo mount -t vfat /dev/sdb1 /mnt      # USB con FAT32
sudo mount -t iso9660 /dev/sr0 /mnt    # CD/DVD
```

Normalmente no necesitas `-t` — mount lo detecta automáticamente. Pero si
la auto-detección falla, especificarlo resuelve el problema.

### 2.3. Crear un mount point

El directorio de montaje debe existir:

```bash
# Si /data no existe:
sudo mount /dev/vdb1 /data
# mount: /data: mount point does not exist.

# Crearlo primero:
sudo mkdir -p /data
sudo mount /dev/vdb1 /data
```

### 2.4. Montar por UUID

```bash
# Más seguro que usar /dev/vdX (ver T03 de S01)
sudo mount UUID="a1b2c3d4-e5f6-7890-abcd-ef1234567890" /mnt
```

### 2.5. Montar por label

```bash
sudo mount LABEL="datos" /mnt
```

---

## 3. Opciones de montaje

Las opciones de montaje controlan el comportamiento del filesystem montado.
Se pasan con `-o`:

```bash
sudo mount -o opciones /dev/vdb1 /mnt
```

### 3.1. Opciones generales (todas los filesystems)

| Opción | Significado | Default |
|--------|-------------|---------|
| `rw` | Lectura y escritura | Sí |
| `ro` | Solo lectura | No |
| `noexec` | No permitir ejecución de binarios | No |
| `nosuid` | Ignorar bits SUID/SGID | No |
| `nodev` | No interpretar dispositivos especiales | No |
| `sync` | I/O síncrono (escribe inmediatamente) | No |
| `async` | I/O asíncrono (buffered) | Sí |
| `noatime` | No actualizar atime (acceso) | No |
| `relatime` | Actualizar atime solo si > mtime | Sí (default moderno) |
| `nodiratime` | No actualizar atime de directorios | No |
| `defaults` | rw, suid, dev, exec, auto, nouser, async | — |

### 3.2. Opciones de seguridad

```bash
# Montar datos de usuarios sin confiar en ejecutables ni SUID
sudo mount -o nosuid,noexec,nodev /dev/vdb1 /data

# Solo lectura (proteger contra escritura)
sudo mount -o ro /dev/vdb1 /mnt
```

Desglose de las opciones de seguridad:

```
nosuid:  Si un archivo tiene bit SUID (chmod u+s), se ignora.
         Previene escalación de privilegios desde filesystems externos.

noexec:  No se pueden ejecutar binarios desde este filesystem.
         Previene ejecución de malware desde USBs o particiones de datos.

nodev:   No se interpretan archivos de dispositivo (/dev/*).
         Previene que un usuario cree un /dev/sda falso en su partición.
```

### 3.3. Opciones de rendimiento

```bash
# Desactivar actualización de atime (menos escrituras)
sudo mount -o noatime /dev/vdb1 /mnt

# Para SSDs y discos virtuales: noatime es una buena práctica
# Reduce escrituras innecesarias sin afectar funcionalidad
```

```
atime (default antiguo):
  Cada vez que LEES un archivo, se actualiza su timestamp de acceso
  → una lectura genera una ESCRITURA → ineficiente

relatime (default moderno):
  Solo actualiza atime si es más antiguo que mtime
  → buen balance entre compatibilidad y rendimiento

noatime:
  NUNCA actualiza atime
  → máximo rendimiento, puede afectar algunos programas
  → seguro para la mayoría de casos
```

### 3.4. Opciones específicas de ext4

| Opción | Significado |
|--------|-------------|
| `data=ordered` | Journal: datos antes de metadatos (default) |
| `data=journal` | Journal: datos y metadatos |
| `data=writeback` | Journal: solo metadatos, datos en cualquier orden |
| `barrier=1` | Write barriers habilitadas (default, seguro) |
| `discard` | Habilitar TRIM/discard para SSDs |
| `commit=N` | Intervalo de commit del journal en segundos |

### 3.5. Opciones específicas de XFS

| Opción | Significado |
|--------|-------------|
| `logbufs=N` | Número de buffers del log |
| `logbsize=N` | Tamaño de buffer del log |
| `allocsize=N` | Pre-allocación para streaming writes |
| `discard` | TRIM/discard para SSDs |
| `nobarrier` | Deshabilitar write barriers (peligroso) |

### 3.6. Opciones específicas de Btrfs

| Opción | Significado |
|--------|-------------|
| `compress=zstd` | Compresión transparente |
| `subvol=@name` | Montar subvolumen específico |
| `subvolid=N` | Montar subvolumen por ID |
| `space_cache=v2` | Cache de espacio libre v2 |
| `autodefrag` | Defragmentación automática |
| `discard=async` | TRIM asíncrono |

### 3.7. Combinar opciones

```bash
# Múltiples opciones separadas por coma
sudo mount -o noatime,nosuid,noexec /dev/vdb1 /data

# Ejemplo para una partición de datos en producción
sudo mount -o defaults,noatime,nosuid,noexec,nodev /dev/vdb1 /data
```

---

## 4. umount: desmontar

### 4.1. Uso básico

```bash
# Por mount point
sudo umount /mnt

# Por dispositivo
sudo umount /dev/vdb1
```

Ambos son equivalentes. Solo funciona si el filesystem no está en uso.

### 4.2. Target busy

```bash
sudo umount /mnt
# umount: /mnt: target is busy.
```

Esto ocurre cuando algo está usando el filesystem:
- Un proceso tiene un archivo abierto
- Un shell tiene su working directory dentro del mount point
- Un proceso en background escribe/lee

### 4.3. Encontrar qué está usando el mount point

```bash
# Opción 1: lsof
sudo lsof +D /mnt
# COMMAND  PID USER   FD   TYPE DEVICE SIZE/OFF NODE NAME
# bash    1234 root  cwd    DIR  252,17     4096    2 /mnt
# vim     1235 root    4r   REG  252,17    12345   11 /mnt/file.txt

# Opción 2: fuser
sudo fuser -mv /mnt
#                      USER        PID ACCESS COMMAND
# /mnt:                root       1234 ..c.. bash
#                      root       1235 .r... vim
```

### 4.4. Forzar el desmontaje

```bash
# Lazy umount: desconecta el mount point inmediatamente
# pero espera a que los procesos terminen de usar los archivos
sudo umount -l /mnt

# Force umount (solo NFS, no funciona con filesystems locales)
sudo umount -f /mnt
```

**`umount -l` (lazy)** es la opción más segura para forzar. No mata procesos,
simplemente hace que el mount point desaparezca del árbol de directorios. Los
procesos que tenían archivos abiertos siguen operando hasta que cierran sus
file descriptors, y entonces el filesystem se desmonta realmente.

### 4.5. La solución correcta: cerrar lo que usa el filesystem

```bash
# 1. Identificar procesos
sudo fuser -mv /mnt

# 2. Cerrarlos manualmente (salir del directorio, cerrar archivos)
cd /

# 3. Si es necesario, matar procesos
sudo fuser -km /mnt    # ← KILL todos los procesos que usan /mnt

# 4. Ahora desmontar
sudo umount /mnt
```

---

## 5. Montar dispositivos especiales

### 5.1. Imagen ISO

```bash
sudo mount -o loop /path/to/image.iso /mnt
# o
sudo mount /path/to/image.iso /mnt    # mount detecta que es una imagen
```

### 5.2. Loop device (archivo como disco)

```bash
# Si el archivo tiene filesystem directamente (sin particiones)
sudo mount -o loop disk.img /mnt

# Si tiene tabla de particiones, usar losetup primero (ver C01/S02/T04)
sudo losetup -Pf --show disk.img
sudo mount /dev/loop0p1 /mnt
```

### 5.3. tmpfs (RAM disk)

```bash
# Crear un filesystem en RAM
sudo mount -t tmpfs -o size=512M tmpfs /mnt/ramdisk
```

tmpfs vive en RAM (y swap si es necesario). Es extremadamente rápido pero
los datos se pierden al desmontar o al apagar.

Usos comunes: `/tmp`, `/run`, `/dev/shm`.

### 5.4. Filesystem de red (NFS)

```bash
sudo mount -t nfs server:/export/share /mnt/nfs
```

### 5.5. Sistemas de archivos virtuales

Estos ya están montados automáticamente por systemd:

```bash
mount | grep -E "^(proc|sysfs|devtmpfs)"
# proc on /proc type proc
# sysfs on /sys type sysfs
# devtmpfs on /dev type devtmpfs
```

---

## 6. findmnt y mount sin argumentos

### 6.1. mount sin argumentos

```bash
mount
```

Muestra todos los filesystems montados. La salida es larga y difícil de leer.

### 6.2. findmnt: la alternativa moderna

`findmnt` muestra los filesystems montados en un formato más legible:

```bash
findmnt
```

```
TARGET                                SOURCE      FSTYPE   OPTIONS
/                                     /dev/vda1   ext4     rw,relatime
├─/sys                                sysfs       sysfs    rw,nosuid,nodev
├─/proc                               proc        proc     rw,nosuid,nodev
├─/dev                                devtmpfs    devtmpfs rw,nosuid
│ ├─/dev/pts                          devpts      devpts   rw,nosuid,noexec
│ └─/dev/shm                          tmpfs       tmpfs    rw,nosuid,nodev
├─/run                                tmpfs       tmpfs    rw,nosuid,nodev
├─/tmp                                tmpfs       tmpfs    rw,nosuid,nodev
└─/mnt                                /dev/vdb1   ext4     rw,noatime,nosuid
```

### 6.3. Opciones útiles de findmnt

```bash
# Solo filesystems reales (no proc, sysfs, etc.)
findmnt -t ext4,xfs,btrfs

# Buscar un mount point
findmnt /mnt
# TARGET SOURCE    FSTYPE OPTIONS
# /mnt   /dev/vdb1 ext4   rw,noatime,nosuid

# Buscar un dispositivo
findmnt -S /dev/vdb1

# Output en formato fstab
findmnt -t ext4,xfs --fstab

# Solo columnas específicas
findmnt -o TARGET,SOURCE,FSTYPE,OPTIONS
```

### 6.4. Verificar si algo está montado

```bash
# ¿Está /dev/vdb1 montado?
findmnt /dev/vdb1
# Si no devuelve nada → no está montado

# En un script:
if findmnt /dev/vdb1 &>/dev/null; then
  echo "Montado"
else
  echo "No montado"
fi
```

---

## 7. Bind mounts

Un bind mount monta un **directorio** (no un dispositivo) en otra ubicación:

### 7.1. Concepto

```
Bind mount:
  /srv/data/docs/ ──(bind mount)──► /home/user/documents/

  Los archivos son los MISMOS — no es una copia.
  Cambiar uno cambia el otro.
```

### 7.2. Crear un bind mount

```bash
sudo mount --bind /srv/data/docs /home/user/documents
```

### 7.3. Bind mount de solo lectura

```bash
sudo mount --bind /srv/data/docs /home/user/documents
sudo mount -o remount,ro,bind /home/user/documents
```

El bind mount de solo lectura requiere dos pasos: primero el bind, luego
remount con `ro`.

### 7.4. Bind mount en fstab

```
/srv/data/docs  /home/user/documents  none  bind  0 0
```

### 7.5. Cuándo usar bind mounts

```
✓ Exponer un subdirectorio en otra ubicación
✓ Contenedores (Docker usa bind mounts internamente)
✓ chroot: hacer visibles directorios del host dentro del chroot
✓ Dar acceso read-only a un directorio normalmente read-write
```

---

## 8. Remount: cambiar opciones en caliente

### 8.1. Cambiar opciones sin desmontar

```bash
# El filesystem está montado rw, cambiar a ro
sudo mount -o remount,ro /mnt

# Cambiar de ro a rw
sudo mount -o remount,rw /mnt

# Agregar noatime sin desmontar
sudo mount -o remount,noatime /mnt
```

### 8.2. Remount del filesystem root

El filesystem root (`/`) no se puede desmontar. Remount es la única forma
de cambiar sus opciones:

```bash
# Poner / en solo lectura (para reparación)
sudo mount -o remount,ro /

# Volver a rw
sudo mount -o remount,rw /
```

### 8.3. Ver las opciones actuales

```bash
findmnt /mnt
# TARGET SOURCE    FSTYPE OPTIONS
# /mnt   /dev/vdb1 ext4   rw,relatime

sudo mount -o remount,noatime /mnt

findmnt /mnt
# TARGET SOURCE    FSTYPE OPTIONS
# /mnt   /dev/vdb1 ext4   rw,noatime     ← cambió
```

---

## 9. Errores comunes

### Error 1: montar sin filesystem

```bash
# ✗ Montar un disco/partición sin filesystem
sudo mount /dev/vdb /mnt
# mount: /mnt: wrong fs type, bad option, bad superblock

# ✓ Crear filesystem primero
sudo mkfs.ext4 /dev/vdb
sudo mount /dev/vdb /mnt
```

### Error 2: mount point no existe

```bash
# ✗ Directorio inexistente
sudo mount /dev/vdb1 /datos
# mount: /datos: mount point does not exist.

# ✓ Crear el directorio
sudo mkdir -p /datos
sudo mount /dev/vdb1 /datos
```

### Error 3: desmontar un filesystem en uso

```bash
# ✗ Tu shell está dentro del mount point
cd /mnt
sudo umount /mnt
# umount: /mnt: target is busy.

# ✓ Salir primero
cd /
sudo umount /mnt
```

### Error 4: confundir umount con unmount

```bash
# ✗ El comando se llama umount (sin la 'n')
unmount /mnt
# bash: unmount: command not found

# ✓ umount
sudo umount /mnt
```

### Error 5: montar dos veces en el mismo punto

```bash
# Si montas otro dispositivo en un mount point ya montado:
sudo mount /dev/vdb1 /mnt    # monta vdb1
sudo mount /dev/vdc1 /mnt    # monta vdc1 ENCIMA de vdb1

# vdb1 queda oculto pero sigue montado
# findmnt muestra ambos
findmnt | grep /mnt
# /mnt  /dev/vdb1 ext4 rw
# /mnt  /dev/vdc1 xfs  rw

# ✓ Desmontar en orden inverso
sudo umount /mnt    # desmonta vdc1
sudo umount /mnt    # desmonta vdb1
```

---

## 10. Cheatsheet

```bash
# ── Montar ──────────────────────────────────────────────────────
sudo mount /dev/vdb1 /mnt                        # básico
sudo mount -t ext4 /dev/vdb1 /mnt               # con tipo
sudo mount -o noatime,nosuid /dev/vdb1 /mnt      # con opciones
sudo mount UUID="xxxx-..." /mnt                  # por UUID
sudo mount LABEL="datos" /mnt                    # por label
sudo mount -o loop image.iso /mnt                # imagen ISO/archivo
sudo mount --bind /src /dst                      # bind mount
sudo mount -t tmpfs -o size=512M tmpfs /ramdisk  # RAM disk

# ── Desmontar ───────────────────────────────────────────────────
sudo umount /mnt                                 # por mount point
sudo umount /dev/vdb1                            # por dispositivo
sudo umount -l /mnt                              # lazy (si busy)
sudo fuser -km /mnt && sudo umount /mnt          # kill + desmontar

# ── Remount ─────────────────────────────────────────────────────
sudo mount -o remount,ro /mnt                    # cambiar a ro
sudo mount -o remount,rw /mnt                    # cambiar a rw
sudo mount -o remount,noatime /mnt               # agregar opción
sudo mount -o remount,rw /                       # remount root

# ── Ver montajes ────────────────────────────────────────────────
findmnt                                          # árbol completo
findmnt /mnt                                     # un mount point
findmnt -S /dev/vdb1                             # un dispositivo
findmnt -t ext4,xfs                              # por tipo
mount | grep vdb                                 # filtrar (legacy)

# ── Diagnóstico de "target busy" ────────────────────────────────
sudo lsof +D /mnt                               # qué archivos abiertos
sudo fuser -mv /mnt                              # qué procesos

# ── Opciones comunes ────────────────────────────────────────────
# defaults    = rw,suid,dev,exec,auto,nouser,async
# ro          = solo lectura
# noatime     = no actualizar atime (rendimiento)
# nosuid      = ignorar bits SUID (seguridad)
# noexec      = no ejecutar binarios (seguridad)
# nodev       = no interpretar devices (seguridad)
# discard     = TRIM para SSDs
# data=ordered  = journal mode para ext4 (default)
# compress=zstd = compresión para Btrfs
# subvol=@name  = subvolumen Btrfs
```

---

## 11. Ejercicios

### Ejercicio 1: montar y desmontar

1. Crea una partición en `/dev/vdb` y formatea con ext4
2. Crea el directorio `/data` y monta la partición ahí
3. Verifica con `findmnt /data` — anota las opciones
4. Crea un archivo de prueba en `/data/`
5. Intenta desmontar sin salir del directorio — observa el error
6. Sal, desmonta y verifica que `/data/` está vacío (el contenido está en vdb1)
7. Monta de nuevo — el archivo reaparece

**Pregunta de reflexión**: cuando desmontaste, el directorio `/data/` aparecía
vacío. ¿Los datos se borraron? ¿Dónde están realmente los datos cuando el
filesystem no está montado?

### Ejercicio 2: opciones de montaje

1. Monta `/dev/vdb1` en `/data` con opciones `noatime,nosuid,noexec`
2. Verifica las opciones con `findmnt /data`
3. Copia un script ejecutable a `/data/` e intenta ejecutarlo — ¿funciona?
4. Usa remount para quitar `noexec`: `sudo mount -o remount,exec /data`
5. Intenta ejecutar el script de nuevo — ¿funciona ahora?
6. Verifica con `findmnt` que las opciones cambiaron

**Pregunta de reflexión**: las opciones `nosuid,noexec,nodev` son las
recomendadas para particiones de datos. ¿Por qué no se usan en la
partición root (`/`)? ¿Qué dejaría de funcionar?

### Ejercicio 3: bind mount y tmpfs

1. Crea un directorio `/srv/shared` con algunos archivos
2. Crea un bind mount: `sudo mount --bind /srv/shared /home/$USER/shared`
3. Verifica con `findmnt` que aparece el bind mount
4. Modifica un archivo desde `/home/$USER/shared/` y verifica que cambió
   en `/srv/shared/`
5. Desmonta el bind mount
6. Crea un tmpfs de 100M: `sudo mount -t tmpfs -o size=100M tmpfs /mnt`
7. Escribe un archivo de 50M con `dd if=/dev/zero of=/mnt/test bs=1M count=50`
8. Verifica con `df -h /mnt` — ¿de dónde sale el espacio?
9. Desmonta — el archivo desaparece

**Pregunta de reflexión**: en el tmpfs, `df -h` muestra 100M de tamaño.
¿Ese espacio viene del disco o de la RAM? ¿Qué pasa si creas un archivo
más grande que la RAM disponible?
