# T03 — /dev

## Qué es /dev

`/dev` contiene **archivos de dispositivo** (device nodes) que representan
hardware y dispositivos virtuales. En Unix, todo es un archivo — los
dispositivos no son una excepción. Leer o escribir en estos archivos equivale
a comunicarse con el hardware o con un subsistema del kernel.

```bash
ls /dev/ | head -20
# console  core  cpu  disk  fd  full  hugepages  kmsg  loop0
# mem  null  pts  random  shm  stderr  stdin  stdout  tty
# urandom  vda  zero  ...
```

En sistemas modernos, `/dev` es un filesystem virtual de tipo **devtmpfs**
gestionado por udev:

```bash
mount | grep /dev
# devtmpfs on /dev type devtmpfs (rw,nosuid,size=...,nr_inodes=...,mode=755)
# o en algunas distros:
# udev on /dev type devtmpfs (rw,nosuid,relatime,size=...,nr_inodes=...)
```

## Dispositivos de bloque vs de carácter

Todo device node en `/dev` es de uno de dos tipos:

### Dispositivos de bloque (block devices)

Almacenan datos en bloques de tamaño fijo. El kernel permite acceso aleatorio
(leer cualquier posición) y usa buffers/cache:

```bash
ls -la /dev/sda
# brw-rw---- 1 root disk 8, 0 ... /dev/sda
# ^
# b = block device

ls -la /dev/vda 2>/dev/null   # En máquinas virtuales con virtio
# brw-rw---- 1 root disk 252, 0 ... /dev/vda
```

Ejemplos: discos duros, SSDs, particiones, loop devices.

### Dispositivos de carácter (character devices)

Transmiten datos como flujo de bytes, sin estructura de bloques. El acceso es
generalmente secuencial:

```bash
ls -la /dev/null
# crw-rw-rw- 1 root root 1, 3 ... /dev/null
# ^
# c = character device

ls -la /dev/tty
# crw-rw-rw- 1 root tty 5, 0 ... /dev/tty
```

Ejemplos: terminales, puertos serie, /dev/null, /dev/random.

### Major y minor numbers

Cada device node tiene dos números que identifican el driver (major) y el
dispositivo específico (minor):

```bash
ls -la /dev/sda /dev/sda1 /dev/sda2
# brw-rw---- 1 root disk 8, 0 ... /dev/sda
# brw-rw---- 1 root disk 8, 1 ... /dev/sda1
# brw-rw---- 1 root disk 8, 2 ... /dev/sda2
#                         ^  ^
#                     major  minor
```

- **Major 8** = driver SCSI/SATA
- **Minor 0** = disco completo, 1 = partición 1, 2 = partición 2

```bash
# Ver el registro oficial de major numbers
cat /proc/devices
# Character devices:
#   1 mem
#   4 /dev/vc/0
#   5 /dev/tty
#  10 misc
# Block devices:
#   8 sd
# 252 virtblk
# 253 device-mapper
```

## Dispositivos especiales esenciales

### /dev/null — Agujero negro

Lee siempre vacío, descarta todo lo que se escribe:

```bash
# Descartar salida de un comando
command_ruidoso > /dev/null 2>&1

# Vaciar un archivo sin borrarlo (mantiene inodo y permisos)
cat /dev/null > /var/log/debug.log
# o equivalente:
> /var/log/debug.log

# Leer de /dev/null da EOF inmediato
cat /dev/null
# (nada)

# Tamaño siempre 0
wc -c < /dev/null
# 0
```

Uso principal: suprimir salida no deseada, especialmente en scripts y crontabs.

### /dev/zero — Fuente de ceros

Produce bytes `\0` infinitamente:

```bash
# Crear un archivo de 1MB lleno de ceros
dd if=/dev/zero of=/tmp/zeros.bin bs=1M count=1
# 1+0 records in
# 1+0 records out
# 1048576 bytes (1.0 MB) copied

# Verificar
xxd /tmp/zeros.bin | head -3
# 00000000: 0000 0000 0000 0000 0000 0000 0000 0000  ................
# 00000010: 0000 0000 0000 0000 0000 0000 0000 0000  ................

# Borrar datos de una partición (sobrescribir con ceros)
# sudo dd if=/dev/zero of=/dev/sdX bs=1M  # DESTRUCTIVO
```

### /dev/urandom y /dev/random — Generadores de aleatoriedad

```bash
# /dev/urandom — generador pseudo-aleatorio (nunca bloquea, suficiente
# para la mayoría de usos)
dd if=/dev/urandom bs=16 count=1 2>/dev/null | xxd
# 00000000: a3f1 2b8c 9e4d 7102 c5b8 3a6f 1d94 e0f7  ..+..Mq...:o....

# Generar un password aleatorio
head -c 32 /dev/urandom | base64
# kR3xV9mN2bQ5jW8pL1cF4hG7tY0sA6dD=

# /dev/random — en kernels antiguos (<5.6), podía bloquear esperando
# entropía. En kernels modernos (>=5.6), se comporta igual que /dev/urandom
# después de la inicialización
```

En la práctica moderna, usar siempre `/dev/urandom`. La distinción con
`/dev/random` solo importa en arranque temprano de sistemas embebidos o
generación de claves criptográficas de nivel extremo.

### /dev/full — Disco lleno simulado

```bash
# Simula un disco lleno al escribir
echo "test" > /dev/full
# bash: echo: write error: No space left on device

# Útil para probar que programas manejan correctamente el error ENOSPC
```

## Terminales y pseudo-terminales

### /dev/tty

La terminal controladora del proceso actual:

```bash
# Siempre apunta a la terminal del proceso que la abre
echo "test" > /dev/tty
# test  (aparece en tu terminal)

# Útil en scripts para forzar output a la terminal del usuario,
# incluso cuando stdout está redirigido
echo "mensaje al usuario" > /dev/tty
```

### /dev/pts/ — Pseudo-terminales

Las terminales gráficas, SSH y tmux/screen usan pseudo-terminales:

```bash
ls /dev/pts/
# 0  1  2  ptmx

# Ver qué terminal estás usando
tty
# /dev/pts/0

# Enviar mensaje a otra terminal (si tienes permisos)
echo "hola" > /dev/pts/1
```

Cada sesión SSH, pestaña de terminal o panel de tmux obtiene un `/dev/pts/N`.

### /dev/ttyS* — Puertos serie

```bash
ls /dev/ttyS*
# /dev/ttyS0  /dev/ttyS1  /dev/ttyS2  /dev/ttyS3

# En máquinas virtuales, la consola serie suele ser ttyS0
# Usar para acceso de emergencia cuando SSH no funciona
```

## Dispositivos de almacenamiento

### Nomenclatura

| Patrón | Tipo | Ejemplos |
|---|---|---|
| `sd[a-z]` | SCSI/SATA/SAS/USB | sda, sdb, sdc |
| `sd[a-z][1-9]` | Particiones SCSI | sda1, sda2 |
| `nvme[0-9]n[1-9]` | NVMe | nvme0n1 |
| `nvme[0-9]n[1-9]p[1-9]` | Particiones NVMe | nvme0n1p1 |
| `vd[a-z]` | virtio (VMs) | vda, vdb |
| `loop[0-9]` | Loop devices | loop0, loop1 |
| `dm-[0-9]` | Device mapper (LVM) | dm-0, dm-1 |
| `sr[0-9]` | CD/DVD | sr0 |

```bash
# Ver discos y particiones
lsblk
# NAME   MAJ:MIN RM  SIZE RO TYPE MOUNTPOINTS
# sda      8:0    0  500G  0 disk
# ├─sda1   8:1    0  512M  0 part /boot/efi
# ├─sda2   8:2    0  499G  0 part /
# sr0     11:0    1 1024M  0 rom
```

### Symlinks estables en /dev/disk/

Los nombres como `sda` pueden cambiar entre reinicios (si se añade un disco,
el orden puede variar). Por eso existen symlinks estables:

```bash
ls /dev/disk/
# by-id  by-label  by-partuuid  by-path  by-uuid

# Por UUID (el más usado en /etc/fstab)
ls -la /dev/disk/by-uuid/
# abc12345-... -> ../../sda1
# def67890-... -> ../../sda2

# Por label
ls -la /dev/disk/by-label/
# BOOT -> ../../sda1
# ROOT -> ../../sda2

# Por ID del hardware
ls -la /dev/disk/by-id/
# scsi-SATA_WDC_WD5000... -> ../../sda
```

## /dev/shm — Memoria compartida

Filesystem tmpfs montado para POSIX shared memory:

```bash
df -h /dev/shm
# Filesystem      Size  Used Avail Use% Mounted on
# tmpfs           7.8G     0  7.8G   0% /dev/shm

# Los programas crean archivos aquí para compartir memoria entre procesos
# Por ejemplo, PostgreSQL usa shared memory para su buffer pool
ls /dev/shm/
# PostgreSQL.12345  sem.myapp  ...
```

El tamaño por defecto es el 50% de la RAM. Se puede ajustar en `/etc/fstab`:

```
tmpfs /dev/shm tmpfs defaults,size=4G 0 0
```

## udev — Gestión dinámica de dispositivos

**udev** es el subsistema que gestiona `/dev` dinámicamente. Cuando se conecta
o desconecta hardware, udev:

1. Recibe un evento del kernel (uevent)
2. Evalúa sus reglas
3. Crea/elimina el device node en `/dev`
4. Crea symlinks (como `/dev/disk/by-uuid/`)
5. Ejecuta acciones configuradas (scripts, permisos, etc.)

### Reglas de udev

```bash
# Reglas del sistema (paquetes)
ls /usr/lib/udev/rules.d/
# 50-udev-default.rules
# 60-persistent-storage.rules
# 70-power-switch.rules
# ...

# Reglas del administrador (overrides)
ls /etc/udev/rules.d/
# 99-custom.rules
```

Las reglas en `/etc/udev/rules.d/` tienen precedencia sobre las del sistema.

### Formato de reglas

```bash
# Ejemplo: dar permisos al grupo "dialout" para un USB serial
# /etc/udev/rules.d/99-usb-serial.rules
SUBSYSTEM=="tty", ATTRS{idVendor}=="2341", ATTRS{idProduct}=="0043", \
    GROUP="dialout", MODE="0660", SYMLINK+="arduino"
```

- `==` es condición (match)
- `=` es asignación
- `+=` es agregar (para SYMLINK, RUN, etc.)

### udevadm — Herramienta de diagnóstico

```bash
# Información de un dispositivo
udevadm info /dev/sda
# P: /devices/pci0000:00/0000:00:1f.2/ata1/host0/target0:0:0/0:0:0:0/block/sda
# N: sda
# S: disk/by-id/ata-WDC_WD5000...
# S: disk/by-path/pci-0000:00:1f.2-ata-1
# E: DEVTYPE=disk
# E: ID_VENDOR=ATA
# ...

# Monitorear eventos en tiempo real (conectar/desconectar hardware)
udevadm monitor
# KERNEL[1234.567] add    /devices/usb3/3-1/3-1:1.0 (usb)
# UDEV  [1234.789] add    /devices/usb3/3-1/3-1:1.0 (usb)
# (Ctrl+C para detener)

# Recargar reglas sin reiniciar
sudo udevadm control --reload-rules
sudo udevadm trigger
```

## /dev en contenedores

Los contenedores Docker tienen un `/dev` mínimo por defecto:

```bash
docker run --rm debian ls /dev/
# console  core  fd  full  mqueue  null  ptmx  pts  random
# shm  stderr  stdin  stdout  tty  urandom  zero
```

No tienen acceso a los dispositivos de bloque del host. Para dar acceso a
dispositivos específicos:

```bash
# Acceso a un dispositivo específico
docker run --rm --device /dev/sda debian fdisk -l /dev/sda

# Acceso a TODOS los dispositivos (privilegiado — solo desarrollo)
docker run --rm --privileged debian ls /dev/
```

El flag `--privileged` da acceso completo a `/dev` del host y es un riesgo
de seguridad — solo usar en entornos controlados.

---

## Ejercicios

### Ejercicio 1 — Clasificar dispositivos

```bash
# Listar dispositivos de bloque en /dev
ls -la /dev/ | grep '^b' | head -10

# Listar dispositivos de carácter en /dev
ls -la /dev/ | grep '^c' | head -10

# Contar cuántos de cada tipo hay
echo "Block devices: $(ls -la /dev/ | grep -c '^b')"
echo "Char devices: $(ls -la /dev/ | grep -c '^c')"
```

### Ejercicio 2 — Usar dispositivos especiales

```bash
# Generar 16 bytes aleatorios y mostrar en hexadecimal
dd if=/dev/urandom bs=16 count=1 2>/dev/null | xxd

# Crear un archivo de exactamente 1KB de ceros
dd if=/dev/zero of=/tmp/zeros.bin bs=1K count=1

# Probar que /dev/full simula disco lleno
echo "test" > /dev/full

# Verificar que /dev/null descarta todo
echo "esto se pierde" > /dev/null
cat /dev/null | wc -c
```

### Ejercicio 3 — Explorar symlinks estables

```bash
# Ver los symlinks por UUID
ls -la /dev/disk/by-uuid/ 2>/dev/null

# Comparar con la salida de lsblk
lsblk -o NAME,UUID,MOUNTPOINT

# ¿Qué método usa /etc/fstab para referenciar particiones?
cat /etc/fstab | grep -v '^#' | grep -v '^$'
```
