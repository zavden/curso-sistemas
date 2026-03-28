# Montaje y fstab

## Índice

1. [Qué es el montaje](#1-qué-es-el-montaje)
2. [mount: montar un filesystem](#2-mount-montar-un-filesystem)
3. [umount: desmontar](#3-umount-desmontar)
4. [Opciones de montaje](#4-opciones-de-montaje)
5. [/etc/fstab: montaje persistente](#5-etcfstab-montaje-persistente)
6. [findmnt: visualizar el árbol de montajes](#6-findmnt-visualizar-el-árbol-de-montajes)
7. [Systemd mount units: alternativa a fstab](#7-systemd-mount-units-alternativa-a-fstab)
8. [Flujo completo: disco nuevo en una VM](#8-flujo-completo-disco-nuevo-en-una-vm)
9. [Errores comunes](#9-errores-comunes)
10. [Cheatsheet](#10-cheatsheet)
11. [Ejercicios](#11-ejercicios)

---

## 1. Qué es el montaje

En Linux, todo cuelga de un único árbol de directorios cuya raíz es `/`. Los filesystems no se acceden por letras de unidad (como `C:` o `D:` en Windows) — se **montan** en un directorio del árbol, haciéndolos accesibles como si fueran parte de la misma estructura:

```
 Antes de montar /dev/vdb1:

 /
 ├── etc/
 ├── home/
 └── data/          ← directorio vacío (mount point)

 Después de mount /dev/vdb1 /data:

 /
 ├── etc/
 ├── home/
 └── data/          ← ahora contiene el filesystem de vdb1
     ├── proyecto/
     ├── backups/
     └── config.txt
```

El directorio donde se monta (`/data`) se llama **mount point** (punto de montaje). Cuando el filesystem está montado, cualquier acceso a `/data/...` lee y escribe en `/dev/vdb1`.

### Qué pasa con el contenido previo del mount point

Si `/data/` tenía archivos antes de montar algo ahí, esos archivos quedan **ocultos** (no borrados) mientras el filesystem externo esté montado. Al desmontar, reaparecen:

```
 /data/ con archivo local:
 └── nota.txt           ← existe en el filesystem de /

 mount /dev/vdb1 /data   → nota.txt queda oculto

 /data/ ahora muestra el contenido de vdb1:
 ├── proyecto/
 └── backups/

 umount /data            → nota.txt reaparece
```

Por eso los mount points suelen ser directorios vacíos creados explícitamente para ese propósito.

---

## 2. mount: montar un filesystem

### Sintaxis básica

```bash
sudo mount <dispositivo> <mount_point>
```

```bash
# Montar una partición ext4
sudo mount /dev/vdb1 /data

# mount detecta el tipo de filesystem automáticamente
# Equivale a:
sudo mount -t ext4 /dev/vdb1 /data
```

### mount sin argumentos: ver qué está montado

```bash
mount
```

```
/dev/vda3 on / type xfs (rw,relatime,seclabel,attr2,inode64,logbufs=8,logbsize=32k,noquota)
/dev/vda2 on /boot type ext4 (rw,relatime,seclabel)
tmpfs on /tmp type tmpfs (rw,nosuid,nodev,seclabel,size=4194304k,nr_inodes=1048576,imode=1777)
/dev/vdb1 on /data type ext4 (rw,relatime,seclabel)
```

Cada línea: `dispositivo on mount_point type filesystem (opciones)`.

La salida de `mount` es verbosa. Para algo más legible, usa `findmnt` (sección 6).

### Montar por UUID

Más seguro que usar `/dev/vdb1` porque el UUID no cambia si reordenan los discos:

```bash
sudo mount UUID="a1b2c3d4-e5f6-7890-abcd-111111111111" /data
```

### Montar con opciones específicas

```bash
# Solo lectura
sudo mount -o ro /dev/vdb1 /data

# Múltiples opciones
sudo mount -o ro,noexec,nosuid /dev/vdb1 /data

# Remontar con nuevas opciones (sin desmontar)
sudo mount -o remount,rw /data
```

### Montar una imagen ISO

```bash
sudo mount -o loop imagen.iso /mnt/iso
# mount crea automáticamente un loop device
```

### Montar una imagen de disco raw de QEMU

Si tienes una imagen raw con particiones, puedes montarla en el host:

```bash
# Ver las particiones dentro de la imagen
sudo losetup -fP /var/lib/libvirt/images/disk.raw
# -P = detectar particiones automáticamente

sudo lsblk /dev/loop0
# loop0     7:0  0  20G  0 loop
# ├─loop0p1 7:1  0   1G  0 part
# └─loop0p2 7:2  0  19G  0 part

sudo mount /dev/loop0p2 /mnt/vm-root
# Ahora puedes explorar el filesystem de la VM desde el host

# Al terminar:
sudo umount /mnt/vm-root
sudo losetup -d /dev/loop0
```

Para imágenes qcow2, necesitas `qemu-nbd` (Network Block Device) en vez de losetup — se verá en S03.

---

## 3. umount: desmontar

```bash
sudo umount /data
# o usando el dispositivo:
sudo umount /dev/vdb1
```

Nota: el comando es `umount` (sin la "n" de "un"). Error de escritura frecuente.

### Target is busy

```bash
sudo umount /data
# umount: /data: target is busy.
```

Significa que alguien (un proceso, un shell, un archivo abierto) está usando el filesystem. Opciones:

```bash
# ¿Quién está usándolo?
sudo lsof +f -- /data
# COMMAND  PID  USER  FD  TYPE  DEVICE  SIZE/OFF  NODE  NAME
# bash     1234 user  cwd  DIR   252,17  4096      2     /data
# vim      1235 user   4r  REG   252,17  8192    103     /data/config.txt

# ¿Qué procesos tienen archivos abiertos ahí?
sudo fuser -mv /data
# USER   PID  ACCESS  COMMAND
# user   1234 ..c..   bash        (c = current directory)
# user   1235 .r...   vim         (r = root directory, leyendo)
```

Soluciones (de menos a más agresiva):

```bash
# 1. Salir del directorio (si tu shell está ahí)
cd /

# 2. Cerrar los programas que lo usan

# 3. Lazy unmount: desconecta el mount point inmediatamente,
#    el filesystem se desmonta cuando no haya más accesos
sudo umount -l /data

# 4. Force unmount (solo para NFS y algunos casos especiales)
sudo umount -f /data
```

`umount -l` (lazy) es útil pero peligroso: el filesystem parece desmontado pero los datos pueden seguir escribiéndose hasta que todos los procesos cierren sus file descriptors.

---

## 4. Opciones de montaje

Las opciones controlan cómo el kernel interactúa con el filesystem. Se pasan con `-o` en mount o en la cuarta columna de fstab.

### Opciones comunes

| Opción | Efecto | Caso de uso |
|--------|--------|-------------|
| `defaults` | Equivale a `rw,suid,dev,exec,auto,nouser,async` | La mayoría de casos |
| `rw` | Lectura y escritura | Default |
| `ro` | Solo lectura | Proteger datos, forensics |
| `noexec` | Prohibir ejecución de binarios | `/tmp`, `/data` |
| `nosuid` | Ignorar bits SUID/SGID | Seguridad en particiones de datos |
| `nodev` | Ignorar device nodes | Seguridad en particiones de datos |
| `noatime` | No actualizar access time al leer | Rendimiento (reduce escrituras) |
| `relatime` | Actualizar atime solo si mtime es más reciente | Default moderno, compromiso |
| `auto` | Montar automáticamente con `mount -a` y al boot | Default |
| `noauto` | No montar automáticamente | USB, medios removibles |
| `user` | Permitir a usuarios normales montar | USB drives |
| `nofail` | No fallar el boot si el dispositivo no existe | Discos opcionales en VMs |
| `sync` | Escrituras síncronas (sin buffer) | USB drives (evitar pérdida al desconectar) |
| `async` | Escrituras asíncronas (con buffer) | Default, mejor rendimiento |

### defaults: qué incluye exactamente

`defaults` es un alias que expande a:

```
defaults = rw,suid,dev,exec,auto,nouser,async
```

Puedes combinar `defaults` con opciones extra:

```bash
# defaults + noexec + nosuid
sudo mount -o defaults,noexec,nosuid /dev/vdb1 /data
```

Las opciones extra sobreescriben las de defaults. `defaults,noexec` activa todo lo de defaults pero desactiva exec.

### noatime vs relatime

Cada vez que lees un archivo, el kernel actualiza su **atime** (access time). En discos con muchas lecturas, esto genera escrituras constantes innecesarias:

```
 Leer /data/archivo.txt:
 1. Leer los bloques de datos         (lectura)
 2. Actualizar atime en el inodo      (¡escritura!)  ← innecesaria

 Con noatime:
 1. Leer los bloques de datos         (lectura)
 2. (nada más)                        ← más rápido
```

- **noatime**: nunca actualiza atime. Mejor rendimiento, pero programas que dependen de atime (como `mutt` para correo) no funcionan correctamente.
- **relatime** (default desde Linux 2.6.30): actualiza atime solo si es anterior a mtime o ctime. Compromiso razonable.

Para discos de datos en VMs, `noatime` es seguro y mejora rendimiento.

### Ejemplo de opciones por seguridad

```bash
# Partición /tmp: no ejecutar binarios, no SUID, no devices
sudo mount -o defaults,noexec,nosuid,nodev /dev/vdb2 /tmp

# Partición /data: no ejecutar, proteger contra scripts maliciosos
sudo mount -o defaults,noexec,nosuid /dev/vdb1 /data

# Partición /boot: solo lectura (no se modifica en operación normal)
sudo mount -o defaults,ro /dev/vda2 /boot
```

---

## 5. /etc/fstab: montaje persistente

Todo lo que montas con `mount` es **temporal** — se pierde al reiniciar. `/etc/fstab` define qué filesystems se montan automáticamente al boot.

### Estructura: 6 columnas

```bash
cat /etc/fstab
```

```
# <device>                                <mountpoint>  <type>  <options>        <dump> <pass>
UUID=a1b2c3d4-e5f6-7890-abcd-111111111111 /boot         ext4    defaults         0      2
UUID=e5f6a7b8-c9d0-1234-efab-222222222222 /             xfs     defaults         0      1
UUID=99887766-5544-3322-1100-aabbccddeeff /data          ext4    defaults,noatime 0      2
UUID=11223344-5566-7788-99aa-bbccddeeff00 none           swap    sw               0      0
tmpfs                                     /tmp           tmpfs   defaults,noexec,nosuid,size=2G  0  0
```

Cada columna:

| # | Columna | Descripción | Ejemplo |
|:-:|---------|-------------|---------|
| 1 | **device** | Qué montar (UUID, LABEL, path) | `UUID=a1b2...` |
| 2 | **mountpoint** | Dónde montarlo | `/data` |
| 3 | **type** | Tipo de filesystem | `ext4`, `xfs`, `swap` |
| 4 | **options** | Opciones de montaje | `defaults,noatime` |
| 5 | **dump** | Backup con dump (0 = no, 1 = sí). Obsoleto, siempre 0. | `0` |
| 6 | **pass** | Orden de fsck al boot (0 = no verificar, 1 = primero (/), 2 = después) | `2` |

### Columna 1: cómo identificar el dispositivo

```bash
# Por UUID (recomendado): no cambia si reordenan discos
UUID=a1b2c3d4-e5f6-7890-abcd-111111111111

# Por LABEL: legible, pero debes asegurar unicidad
LABEL=DATOS

# Por path (no recomendado): puede cambiar entre reinicios
/dev/vdb1

# Por PARTUUID (para GPT, usado principalmente en bootloader)
PARTUUID=abcd1234-02
```

Para obtener el UUID:

```bash
sudo blkid /dev/vdb1
# UUID="a1b2c3d4-..."  ← copiar este valor
```

### Columna 6: pass (orden de fsck)

| Valor | Significado |
|:-----:|-------------|
| 0 | No ejecutar fsck (para swap, tmpfs, NFS, etc.) |
| 1 | Verificar primero — solo para `/` |
| 2 | Verificar después de `/` — para el resto de particiones |

Los filesystems con el mismo número de pass se verifican en paralelo (si fsck lo soporta). XFS ignora pass porque usa `xfs_repair` internamente.

### Añadir una entrada a fstab

Flujo seguro:

```bash
# 1. Obtener el UUID
sudo blkid /dev/vdb1
# UUID="nuevo-uuid-aqui" TYPE="ext4"

# 2. Crear el mount point
sudo mkdir -p /data

# 3. Editar fstab
sudo vi /etc/fstab
# Añadir:
# UUID=nuevo-uuid-aqui  /data  ext4  defaults,noatime  0  2

# 4. Probar SIN reiniciar (monta todo lo de fstab que no esté montado)
sudo mount -a

# 5. Verificar
df -h /data
findmnt /data
```

**`mount -a` es el test obligatorio.** Si hay un error en fstab y no lo pruebas, el sistema puede no arrancar al reiniciar.

### Qué pasa si fstab tiene un error

Un fstab con un UUID incorrecto o un mount point inexistente puede hacer que el sistema entre en **modo de emergencia** al boot:

```
 Boot normal:
 systemd lee fstab → monta todos los filesystems → arranca servicios

 Error en fstab (UUID incorrecto, partición no existe):
 systemd lee fstab → intenta montar → falla → MODO DE EMERGENCIA
 "Give root password for maintenance (or press Ctrl-D to continue):"
```

Para evitar esto con discos que pueden no estar presentes (por ejemplo, un disco USB o un segundo disco de VM que no siempre conectas):

```bash
# nofail: no fallar el boot si el dispositivo no existe
UUID=optionaldisk-uuid  /extra  ext4  defaults,nofail  0  2
```

### Editar fstab con seguridad

```bash
# 1. Hacer backup antes de editar
sudo cp /etc/fstab /etc/fstab.bak

# 2. Editar
sudo vi /etc/fstab

# 3. Probar
sudo mount -a

# 4. Si falla, restaurar
sudo cp /etc/fstab.bak /etc/fstab
```

---

## 6. findmnt: visualizar el árbol de montajes

`findmnt` es la herramienta moderna para inspeccionar montajes — más legible que `mount`:

### Árbol completo

```bash
findmnt
```

```
TARGET                       SOURCE     FSTYPE   OPTIONS
/                            /dev/vda3  xfs      rw,relatime,seclabel,attr2
├─/boot                      /dev/vda2  ext4     rw,relatime,seclabel
├─/tmp                        tmpfs     tmpfs    rw,nosuid,nodev,seclabel
├─/data                      /dev/vdb1  ext4     rw,noatime,seclabel
└─/proc                       proc      proc     rw,nosuid,nodev,noexec
  ├─/proc/sys/fs/binfmt_misc  ...
  ...
```

El formato de árbol muestra la jerarquía real de montajes — mucho más útil que la lista plana de `mount`.

### Consultas específicas

```bash
# ¿Qué hay montado en /data?
findmnt /data
# TARGET  SOURCE     FSTYPE  OPTIONS
# /data   /dev/vdb1  ext4    rw,noatime,seclabel

# ¿Dónde está montado /dev/vdb1?
findmnt -S /dev/vdb1

# Buscar por tipo de filesystem
findmnt -t ext4
# TARGET  SOURCE     FSTYPE  OPTIONS
# /boot   /dev/vda2  ext4    rw,relatime
# /data   /dev/vdb1  ext4    rw,noatime

# Buscar por UUID
findmnt -S UUID="a1b2c3d4-..."

# Solo filesystems reales (excluir pseudo-fs como proc, sysfs, tmpfs)
findmnt -t ext4,xfs,btrfs

# Salida en formato lista (sin árbol)
findmnt -l

# Evaluar fstab sin montar (verificar errores)
findmnt --verify
# Si hay problemas, los reporta
```

### findmnt vs mount vs df

| Necesidad | Herramienta |
|-----------|-------------|
| Ver todos los montajes con jerarquía | `findmnt` |
| Ver opciones de montaje de un filesystem | `findmnt /data` |
| Ver espacio libre en filesystems montados | `df -Th` |
| Ver uso de inodos | `df -i` |
| Verificar si una partición está montada | `findmnt /dev/vdb1` o `mount \| grep vdb1` |
| Verificar fstab antes de reiniciar | `findmnt --verify` |

---

## 7. Systemd mount units: alternativa a fstab

Systemd puede gestionar montajes con **mount units** — archivos `.mount` que reemplazan entradas de fstab. Systemd ya convierte fstab en mount units internamente, pero puedes crearlos directamente.

### Cuándo usar mount units en vez de fstab

- Necesitas dependencias (montar B solo después de que A esté montado).
- Quieres montaje on-demand (automount — se monta al primer acceso).
- Necesitas timeouts o reintentos.
- Para mount points complejos con pre/post scripts.

Para montajes simples, fstab es más fácil y legible.

### Nombre del archivo

El nombre **debe** coincidir exactamente con el mount point, reemplazando `/` por `-` y omitiendo el `/` inicial:

```
Mount point       → Nombre del archivo
/data             → data.mount
/mnt/backup       → mnt-backup.mount
/var/lib/docker   → var-lib-docker.mount
```

### Ejemplo: mount unit básico

```ini
# /etc/systemd/system/data.mount
[Unit]
Description=Mount /data partition
After=local-fs-pre.target
Requires=local-fs-pre.target

[Mount]
What=UUID=a1b2c3d4-e5f6-7890-abcd-111111111111
Where=/data
Type=ext4
Options=defaults,noatime

[Install]
WantedBy=local-fs.target
```

```bash
# Activar
sudo systemctl daemon-reload
sudo systemctl enable --now data.mount

# Verificar
sudo systemctl status data.mount
findmnt /data
```

### Automount: montaje on-demand

Un `.automount` unit monta el filesystem solo cuando alguien accede al mount point. Útil para filesystems que no siempre se necesitan:

```ini
# /etc/systemd/system/data.automount
[Unit]
Description=Automount /data on first access

[Automount]
Where=/data
TimeoutIdleSec=300   # Desmontar si no se usa durante 5 minutos

[Install]
WantedBy=local-fs.target
```

```bash
sudo systemctl enable --now data.automount
# /data se montará automáticamente la primera vez que alguien haga ls /data
```

---

## 8. Flujo completo: disco nuevo en una VM

Este es el flujo que usarás repetidamente en los labs del curso. Combina todo lo aprendido en S01 y S02:

### Desde el host: crear y conectar disco

```bash
# 1. Crear imagen de disco
qemu-img create -f qcow2 /var/lib/libvirt/images/lab-data.qcow2 5G

# 2. Conectar a la VM
virsh attach-disk debian-lab /var/lib/libvirt/images/lab-data.qcow2 vdb \
  --driver qemu --subdriver qcow2 --persistent
```

### Dentro de la VM: particionar, formatear, montar

```bash
# 3. Verificar que el disco apareció
lsblk
# vdb  252:16  0  5G  0 disk     ← nuevo disco, sin particiones

# 4. Crear tabla de particiones GPT + una partición
sudo fdisk /dev/vdb
# g     ← tabla GPT
# n     ← nueva partición
# ↵     ← partition 1
# ↵     ← first sector (default)
# ↵     ← last sector (usar todo)
# w     ← escribir

# 5. Verificar partición
lsblk /dev/vdb
# vdb   252:16  0  5G  0 disk
# └─vdb1 252:17 0  5G  0 part

# 6. Crear filesystem
sudo mkfs.ext4 -L "LAB-DATA" /dev/vdb1

# 7. Crear mount point
sudo mkdir -p /data

# 8. Montar temporalmente (para probar)
sudo mount /dev/vdb1 /data
df -h /data
# Filesystem  Size  Used  Avail  Use%  Mounted on
# /dev/vdb1   4.9G   24K  4.6G    1%   /data

# 9. Obtener UUID para fstab
sudo blkid /dev/vdb1
# UUID="nuevo-uuid" TYPE="ext4" LABEL="LAB-DATA"

# 10. Añadir a fstab para persistencia
echo 'UUID=nuevo-uuid  /data  ext4  defaults,noatime  0  2' | sudo tee -a /etc/fstab

# 11. Probar fstab (desmontar y remontar desde fstab)
sudo umount /data
sudo mount -a
df -h /data    # debe aparecer de nuevo

# 12. Listo — sobrevive reinicios
```

### Diagrama del flujo completo

```
 HOST                              VM
┌────────────────────┐      ┌───────────────────────────┐
│                    │      │                           │
│ qemu-img create    │      │                           │
│ (crear .qcow2)     │      │                           │
│       │            │      │                           │
│       ▼            │      │                           │
│ virsh attach-disk  │─────►│ /dev/vdb aparece          │
│                    │      │       │                   │
│                    │      │       ▼                   │
│                    │      │ fdisk /dev/vdb            │
│                    │      │ (crear particiones)       │
│                    │      │       │                   │
│                    │      │       ▼                   │
│                    │      │ mkfs.ext4 /dev/vdb1       │
│                    │      │ (crear filesystem)        │
│                    │      │       │                   │
│                    │      │       ▼                   │
│                    │      │ mount /dev/vdb1 /data     │
│                    │      │ (montar)                  │
│                    │      │       │                   │
│                    │      │       ▼                   │
│                    │      │ editar /etc/fstab         │
│                    │      │ (persistir montaje)       │
│                    │      │       │                   │
│                    │      │       ▼                   │
│                    │      │ mount -a (verificar)      │
│                    │      │                           │
└────────────────────┘      └───────────────────────────┘
```

---

## 9. Errores comunes

### Error 1: no probar fstab con mount -a antes de reiniciar

```bash
sudo vi /etc/fstab
# Añadir entrada con UUID incorrecto (typo)
sudo reboot
# El sistema no arranca → modo de emergencia
```

**Por qué pasa**: systemd intenta montar todos los filesystems de fstab al boot. Si falla un montaje obligatorio, entra en modo de emergencia.

**Solución**: siempre probar después de editar fstab:

```bash
sudo mount -a
# Si no da errores, es seguro reiniciar

# También puedes verificar la sintaxis con findmnt
findmnt --verify
```

Si ya estás en modo de emergencia, monta `/` como lectura-escritura y corrige fstab:

```bash
mount -o remount,rw /
vi /etc/fstab    # corregir o comentar la línea problemática
reboot
```

### Error 2: escribir "unmount" en vez de "umount"

```bash
unmount /data
# command not found: unmount
```

**Solución**: el comando es `umount` (sin la "n"). Error universal — hasta administradores veteranos lo escriben mal.

### Error 3: montar sin crear el mount point

```bash
sudo mount /dev/vdb1 /data
# mount: /data: mount point does not exist.
```

**Por qué pasa**: mount no crea directorios automáticamente.

**Solución**:

```bash
sudo mkdir -p /data
sudo mount /dev/vdb1 /data
```

### Error 4: usar /dev/vdb1 en fstab en vez de UUID

```bash
# En fstab:
/dev/vdb1  /data  ext4  defaults  0  2

# Añades otro disco a la VM... los nombres rotan
# /dev/vdb1 ahora es OTRO disco → datos equivocados en /data
```

**Por qué pasa**: el kernel asigna nombres (`vdb`, `vdc`) según el orden de detección, que puede cambiar.

**Solución**: siempre usar UUID:

```bash
sudo blkid /dev/vdb1   # obtener UUID
# Usar en fstab: UUID=xxxx-xxxx  /data  ext4  defaults  0  2
```

### Error 5: no usar nofail para discos opcionales

```bash
# En fstab de una VM:
UUID=disk-de-laboratorio  /lab  ext4  defaults  0  2

# Desconectas el disco del lab con virsh detach-disk
# La VM no arranca → modo de emergencia
```

**Por qué pasa**: sin `nofail`, systemd considera que el montaje es obligatorio. Si el dispositivo no existe, falla el boot.

**Solución**: para discos que pueden no estar presentes:

```bash
UUID=disk-opcional  /lab  ext4  defaults,nofail  0  2
#                                ^^^^^^^ clave
```

---

## 10. Cheatsheet

```
┌──────────────────────────────────────────────────────────────────┐
│                  MONTAJE Y FSTAB — REFERENCIA                    │
├──────────────────────────────────────────────────────────────────┤
│ MONTAR                                                           │
│   mount /dev/vdb1 /data          Montar partición                │
│   mount UUID="..." /data         Montar por UUID                 │
│   mount -o ro /dev/vdb1 /data    Montar solo lectura             │
│   mount -o remount,rw /data      Remontar con nuevas opciones    │
│   mount -a                       Montar todo lo de fstab         │
│   mount -o loop img.iso /mnt     Montar imagen ISO               │
├──────────────────────────────────────────────────────────────────┤
│ DESMONTAR                                                        │
│   umount /data                   Desmontar por mount point       │
│   umount /dev/vdb1               Desmontar por dispositivo       │
│   umount -l /data                Lazy unmount (cuando se libere) │
│   lsof +f -- /data               Ver quién usa el mount point    │
│   fuser -mv /data                Ver procesos con archivos aquí  │
├──────────────────────────────────────────────────────────────────┤
│ /etc/fstab (6 columnas)                                          │
│   UUID=...  /data  ext4  defaults,noatime  0  2                 │
│   │         │      │     │                 │  │                  │
│   │         │      │     │                 │  └─ pass (fsck)     │
│   │         │      │     │                 └──── dump (0)        │
│   │         │      │     └──────────────── opciones              │
│   │         │      └───────────────────── tipo FS                │
│   │         └──────────────────────────── mount point            │
│   └────────────────────────────────────── dispositivo (UUID)     │
├──────────────────────────────────────────────────────────────────┤
│ OPCIONES IMPORTANTES                                             │
│   defaults        rw,suid,dev,exec,auto,nouser,async            │
│   noatime         No actualizar access time (rendimiento)       │
│   noexec          Prohibir ejecución de binarios                │
│   nosuid          Ignorar SUID/SGID                              │
│   nodev           Ignorar device nodes                           │
│   nofail          No fallar boot si no existe el dispositivo     │
│   ro              Solo lectura                                   │
├──────────────────────────────────────────────────────────────────┤
│ INSPECCIONAR                                                     │
│   findmnt                       Árbol de montajes (legible)      │
│   findmnt /data                 Info de un mount point           │
│   findmnt -t ext4               Filtrar por tipo de FS           │
│   findmnt --verify              Verificar fstab                  │
│   df -Th                        Espacio libre por filesystem     │
│   df -i                         Uso de inodos                    │
├──────────────────────────────────────────────────────────────────┤
│ FLUJO SEGURO PARA FSTAB                                          │
│   1. cp /etc/fstab /etc/fstab.bak   Backup                     │
│   2. blkid /dev/vdb1                 Obtener UUID               │
│   3. mkdir -p /data                  Crear mount point           │
│   4. vi /etc/fstab                   Añadir entrada             │
│   5. mount -a                        Probar (obligatorio!)       │
│   6. findmnt /data                   Verificar                   │
└──────────────────────────────────────────────────────────────────┘
```

---

## 11. Ejercicios

### Ejercicio 1: Montar, explorar opciones, desmontar

Usando las particiones formateadas en T01 (ext4 en `/dev/vdb1`, XFS en `/dev/vdb2`):

1. Crea dos mount points: `/mnt/ext4-lab` y `/mnt/xfs-lab`.
2. Monta ambas particiones.
3. Verifica con `findmnt -t ext4,xfs` que aparecen.
4. Crea un archivo en cada una:
   ```bash
   echo "test" | sudo tee /mnt/ext4-lab/testfile
   echo "test" | sudo tee /mnt/xfs-lab/testfile
   ```
5. Remonta la partición ext4 como solo lectura:
   ```bash
   sudo mount -o remount,ro /mnt/ext4-lab
   ```
6. Intenta crear otro archivo. ¿Qué error ves?
7. Remonta como lectura-escritura de nuevo.
8. Verifica las opciones de montaje con `findmnt /mnt/ext4-lab`. ¿Cambiaron?
9. Desmonta ambas.

> **Pregunta de reflexión**: cuando remontaste como `ro`, ¿el archivo `testfile` que ya existía sigue accesible para lectura? ¿Qué pasa si un proceso tenía el archivo abierto para escritura antes del remount?

### Ejercicio 2: Configurar fstab y sobrevivir reinicios

1. Haz backup de `/etc/fstab`.
2. Obtén el UUID de `/dev/vdb1` con `blkid`.
3. Añade una entrada a fstab para montar `/dev/vdb1` en `/data` con opciones `defaults,noatime`.
4. Ejecuta `mount -a` y verifica que funciona.
5. Ejecuta `findmnt --verify` — ¿reporta algún problema?
6. Desmonta `/data` con `umount`.
7. Ejecuta `mount -a` de nuevo — ¿se monta automáticamente?
8. Ahora modifica la entrada para usar la opción `nofail` y cambia el UUID a uno inventado (simular disco ausente).
9. Ejecuta `mount -a` — ¿falla? ¿Qué muestra `dmesg | tail`?
10. Restaura el UUID correcto y verifica que monta de nuevo.

> **Pregunta de reflexión**: ¿qué diferencia hay entre `pass` 0 y `pass` 2 en fstab para una partición de datos? Si pones `pass` 1 en una partición que no es `/`, ¿qué pasa?

### Ejercicio 3: Flujo completo de disco nuevo

Si tienes una VM con QEMU/KVM, ejecuta el flujo completo de la sección 8:

1. Desde el host: crear imagen qcow2 de 3 GiB y conectarla a la VM.
2. Dentro de la VM: verificar con `lsblk` que apareció un disco nuevo.
3. Particionar con fdisk (GPT, una partición).
4. Formatear con ext4 (label "PRACTICE").
5. Montar en `/practice`.
6. Crear archivos de prueba dentro.
7. Añadir a fstab con UUID y opción `nofail`.
8. Probar con `mount -a`.
9. Reiniciar la VM y verificar que `/practice` está montado automáticamente.
10. Desde el host, verificar con `qemu-img info` cuánto ocupa realmente la imagen después de todo este proceso.

> **Pregunta de reflexión**: el disco es de 3 GiB pero después de particionar, formatear y crear unos pocos archivos, la imagen qcow2 en el host probablemente ocupa menos de 500 MiB. ¿Por qué? ¿Cuándo crecería hasta los 3 GiB reales? ¿Y si hubieras usado formato raw en vez de qcow2?
