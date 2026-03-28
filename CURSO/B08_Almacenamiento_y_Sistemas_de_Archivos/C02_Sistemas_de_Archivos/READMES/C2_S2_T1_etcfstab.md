# /etc/fstab

## Índice

1. [Qué es /etc/fstab](#qué-es-etcfstab)
2. [Anatomía de una línea](#anatomía-de-una-línea)
3. [Campo 1 — Dispositivo](#campo-1--dispositivo)
4. [Campo 2 — Punto de montaje](#campo-2--punto-de-montaje)
5. [Campo 3 — Tipo de filesystem](#campo-3--tipo-de-filesystem)
6. [Campo 4 — Opciones de montaje](#campo-4--opciones-de-montaje)
7. [Campo 5 — dump](#campo-5--dump)
8. [Campo 6 — pass (fsck)](#campo-6--pass-fsck)
9. [UUID vs device path vs LABEL](#uuid-vs-device-path-vs-label)
10. [Flujo: añadir un filesystem a fstab](#flujo-añadir-un-filesystem-a-fstab)
11. [Validación antes de reiniciar](#validación-antes-de-reiniciar)
12. [Ejemplos completos](#ejemplos-completos)
13. [Errores comunes](#errores-comunes)
14. [Cheatsheet](#cheatsheet)
15. [Ejercicios](#ejercicios)

---

## Qué es /etc/fstab

`/etc/fstab` (filesystem table) es el archivo que define qué filesystems se montan automáticamente durante el arranque del sistema. Cada línea describe un filesystem, su punto de montaje y las opciones con las que se monta.

```
┌─────────────────────────────────────────────────────┐
│                    Arranque del sistema              │
│                                                     │
│  1. Kernel monta rootfs (parámetro root=)           │
│  2. systemd lee /etc/fstab                          │
│  3. Genera .mount units dinámicamente               │
│  4. Monta cada entrada en el orden correcto         │
│  5. Sistema listo con todos los filesystems          │
└─────────────────────────────────────────────────────┘
```

En sistemas con systemd, el archivo se traduce automáticamente a unidades `.mount` mediante `systemd-fstab-generator`. Aunque systemd ofrece sus propias unidades de montaje (tema del siguiente README), fstab sigue siendo el estándar universal y la forma más común de configurar montajes persistentes.

> **Ubicación**: `/etc/fstab` — siempre. No hay alternativa ni override por directorio.

---

## Anatomía de una línea

Cada línea de fstab tiene exactamente **6 campos** separados por espacios o tabuladores:

```
<device>   <mountpoint>   <type>   <options>   <dump>   <pass>
```

Ejemplo real:

```
UUID=a1b2c3d4-...   /home   ext4   defaults,noatime   0   2
```

Diagrama campo a campo:

```
UUID=a1b2c3d4-e5f6-...   /home        ext4     defaults,noatime   0   2
│                         │            │        │                  │   │
│                         │            │        │                  │   └─ Campo 6: pass
│                         │            │        │                  └───── Campo 5: dump
│                         │            │        └──────────────────────── Campo 4: options
│                         │            └───────────────────────────────── Campo 3: type
│                         └────────────────────────────────────────────── Campo 2: mountpoint
└──────────────────────────────────────────────────────────────────────── Campo 1: device
```

Las líneas que comienzan con `#` son comentarios. Las líneas vacías se ignoran.

---

## Campo 1 — Dispositivo

Identifica **qué** se va a montar. Hay varias formas de especificarlo:

| Forma | Ejemplo | Estabilidad |
|-------|---------|-------------|
| Device path | `/dev/sda1` | Inestable — puede cambiar entre reinicios |
| UUID | `UUID=a1b2c3d4-e5f6-7890-abcd-ef1234567890` | Estable — ligado al filesystem |
| LABEL | `LABEL=home` | Estable — pero depende de que no haya duplicados |
| PARTUUID | `PARTUUID=12345678-01` | Estable — ligado a la partición (GPT) |
| Path /dev/disk/by-* | `/dev/disk/by-uuid/a1b2...` | Estable — symlinks gestionados por udev |
| Red (NFS) | `server:/export/data` | N/A — depende de red |

**¿Cómo obtener el UUID?**

```bash
# Método recomendado
blkid /dev/vdb1

# Salida:
# /dev/vdb1: UUID="a1b2c3d4-e5f6-7890-abcd-ef1234567890" TYPE="ext4"

# Alternativa: listar todos
lsblk -o NAME,UUID,FSTYPE,MOUNTPOINT
```

---

## Campo 2 — Punto de montaje

El directorio donde el filesystem será accesible. Debe existir antes del montaje.

Reglas:

- Debe ser una **ruta absoluta**: `/mnt/data`, `/home`, `/var/log`
- El directorio debe **existir** — fstab no lo crea
- Para swap se usa la palabra reservada `none` o `swap`
- No debe haber conflictos — dos entradas no pueden montar en el mismo punto

```bash
# Crear el punto de montaje antes de añadir a fstab
mkdir -p /mnt/data
```

Puntos de montaje estándar del sistema:

```
/           ← rootfs (siempre presente)
/boot       ← kernel e initramfs
/boot/efi   ← partición EFI (sistemas UEFI)
/home       ← datos de usuarios
/var        ← datos variables (logs, spool, lib)
/tmp        ← temporal (a veces tmpfs)
```

---

## Campo 3 — Tipo de filesystem

El tipo de filesystem. Debe coincidir con el formato real del dispositivo.

| Tipo | Descripción |
|------|-------------|
| `ext4` | Filesystem estándar Linux |
| `xfs` | Default en RHEL/AlmaLinux |
| `btrfs` | Copy-on-write, subvolúmenes |
| `vfat` | FAT32 — particiones EFI, USB |
| `swap` | Espacio de intercambio |
| `tmpfs` | Filesystem en RAM |
| `nfs` | Network File System |
| `iso9660` | CD/DVD/ISO |
| `auto` | Detectar automáticamente |

`auto` funciona para la mayoría de filesystems locales, pero es mejor ser explícito:
- Evita ambigüedades en el arranque
- Documenta qué hay en cada partición
- Algunos filesystems (NFS, tmpfs) **requieren** tipo explícito

---

## Campo 4 — Opciones de montaje

Lista de opciones separadas por comas, sin espacios. `defaults` equivale a `rw,suid,dev,exec,auto,nouser,async`.

### Opciones generales

| Opción | Efecto | Opuesto |
|--------|--------|---------|
| `rw` | Lectura y escritura | `ro` |
| `auto` | Montar en boot / con `mount -a` | `noauto` |
| `exec` | Permitir ejecución de binarios | `noexec` |
| `suid` | Permitir bits SUID/SGID | `nosuid` |
| `dev` | Interpretar dispositivos especiales | `nodev` |
| `user` | Cualquier usuario puede montar (implica `noexec,nosuid,nodev`) | `nouser` |
| `users` | Cualquier usuario puede montar **y desmontar** | — |

### Opciones de rendimiento

| Opción | Efecto |
|--------|--------|
| `noatime` | No actualizar tiempo de acceso — mejora rendimiento |
| `relatime` | Actualizar atime solo si es anterior a mtime (default en kernels modernos) |
| `discard` | Habilitar TRIM para SSD |
| `commit=N` | Intervalo de escritura al disco en segundos (ext4) |

### Opciones de seguridad (para particiones de datos)

```
# /tmp seguro — sin ejecución ni SUID
tmpfs   /tmp   tmpfs   defaults,noexec,nosuid,nodev,size=2G   0   0

# USB montado por usuarios — restringido
LABEL=usb   /mnt/usb   vfat   noauto,user,noexec,nosuid   0   0
```

### Opciones específicas por filesystem

| Filesystem | Opciones comunes |
|------------|-----------------|
| ext4 | `data=ordered`, `journal_checksum`, `errors=remount-ro` |
| xfs | `logbufs=8`, `logbsize=256k`, `allocsize=64k` |
| btrfs | `compress=zstd`, `subvol=@home`, `space_cache=v2` |
| nfs | `vers=4.2`, `soft`, `timeo=150`, `retrans=3`, `_netdev` |

> **`_netdev`**: indica que el dispositivo requiere red. systemd esperará a que la red esté activa antes de intentar el montaje. Imprescindible para NFS, iSCSI, etc.

---

## Campo 5 — dump

Valor numérico que indica si el filesystem debe ser respaldado por la utilidad `dump`.

| Valor | Significado |
|-------|-------------|
| `0` | No respaldar (lo más común hoy) |
| `1` | Incluir en backups con dump |

En la práctica, **casi siempre es `0`**. La utilidad `dump` está obsoleta — los backups modernos usan otras herramientas (rsync, borgbackup, etc.). Pero el campo es obligatorio en la sintaxis.

---

## Campo 6 — pass (fsck)

Controla el **orden de verificación de filesystem** durante el arranque. `fsck` revisa la integridad antes de montar.

| Valor | Significado |
|-------|-------------|
| `0` | No verificar (swap, tmpfs, NFS, Btrfs) |
| `1` | Verificar primero — **solo para rootfs** (`/`) |
| `2` | Verificar después — para el resto de filesystems locales |

```
Flujo de fsck en arranque:

  pass=1 ──► Verificar rootfs ──► pass=2 ──► Verificar /home, /var, etc.
                                     │
                                     ├── Los de pass=2 pueden verificarse
                                     │   en paralelo entre sí
                                     │
  pass=0 ──► No verificar (swap, tmpfs, NFS, btrfs, xfs*)
```

> **XFS y Btrfs**: usan `0` porque tienen sus propios mecanismos de verificación (`xfs_repair`, `btrfs check`). El `fsck.xfs` de Linux es un no-op (no hace nada). El `fsck.btrfs` tampoco ejecuta verificación real.

---

## UUID vs device path vs LABEL

Esta decisión afecta directamente la **estabilidad del arranque**.

### El problema con device paths

```
Primer arranque:               Segundo arranque (con USB):
  /dev/sda = disco SATA          /dev/sda = USB (apareció antes)
  /dev/sdb = disco USB           /dev/sdb = disco SATA ← ¡cambió!
```

Si fstab dice `/dev/sda1` → al añadir un USB el sistema puede intentar montar el dispositivo equivocado. En el peor caso: **el sistema no arranca**.

### Comparación

```
┌────────────────┬──────────────┬───────────────┬──────────────────────┐
│ Método         │ Estabilidad  │ Legibilidad   │ Cuándo usar          │
├────────────────┼──────────────┼───────────────┼──────────────────────┤
│ /dev/sdX       │ ✗ Inestable  │ ✓ Fácil       │ Nunca en producción  │
│ UUID=          │ ✓ Estable    │ ✗ Ilegible    │ Recomendado (defau-  │
│                │              │               │ lt de instaladores)  │
│ LABEL=         │ ✓ Estable*   │ ✓ Legible     │ Si controlas labels  │
│ PARTUUID=      │ ✓ Estable    │ ✗ Ilegible    │ rootfs sin userspace │
│ /dev/disk/by-* │ ✓ Estable    │ ~ Regular     │ Alternativa a UUID=  │
└────────────────┴──────────────┴───────────────┴──────────────────────┘

* LABEL es estable siempre que no haya dos filesystems con el mismo label
```

### Recomendación práctica

```
UUID=    ← para todo lo que no sea swap o tmpfs
LABEL=   ← cuando quieres legibilidad y controlas los labels
/dev/X   ← solo en labs temporales donde recreas todo cada vez
```

> **En el lab QEMU/KVM**: los discos virtio (`/dev/vdX`) son relativamente estables porque se adjuntan en orden determinístico. Pero incluso ahí, usar UUID es la buena práctica.

---

## Flujo: añadir un filesystem a fstab

Procedimiento seguro, paso a paso:

```
┌─────────────────────────────────────┐
│ 1. Crear el filesystem (si no existe)│
│    mkfs.ext4 -L data /dev/vdb1      │
├─────────────────────────────────────┤
│ 2. Crear el punto de montaje         │
│    mkdir -p /mnt/data                │
├─────────────────────────────────────┤
│ 3. Obtener UUID                      │
│    blkid /dev/vdb1                   │
├─────────────────────────────────────┤
│ 4. Añadir línea a fstab             │
│    vim /etc/fstab                    │
├─────────────────────────────────────┤
│ 5. Validar sintaxis                  │
│    findmnt --verify                  │
├─────────────────────────────────────┤
│ 6. Probar sin reiniciar              │
│    mount -a                          │
├─────────────────────────────────────┤
│ 7. Verificar resultado               │
│    findmnt /mnt/data                 │
└─────────────────────────────────────┘
```

### Ejemplo completo en terminal

```bash
# 1. Formatear
mkfs.ext4 -L data /dev/vdb1

# 2. Punto de montaje
mkdir -p /mnt/data

# 3. Obtener UUID
blkid /dev/vdb1
# /dev/vdb1: LABEL="data" UUID="f47ac10b-58cc-4372-a567-0e02b2c3d479" TYPE="ext4"

# 4. Añadir a fstab (con el UUID obtenido)
echo 'UUID=f47ac10b-58cc-4372-a567-0e02b2c3d479  /mnt/data  ext4  defaults,noatime  0  2' >> /etc/fstab

# 5. Validar
findmnt --verify
# Si no hay errores → todo bien

# 6. Montar todo lo de fstab que no esté montado
mount -a

# 7. Confirmar
findmnt /mnt/data
# TARGET    SOURCE    FSTYPE  OPTIONS
# /mnt/data /dev/vdb1 ext4    rw,noatime,...
```

> **Predicción**: `mount -a` lee `/etc/fstab` y monta todas las entradas que tengan `auto` (o `defaults`, que incluye `auto`) y no estén ya montadas. Si la entrada es correcta, el filesystem aparecerá en `findmnt`. Si hay un error de sintaxis o el UUID no existe, recibirás un error sin afectar el resto del sistema.

---

## Validación antes de reiniciar

Nunca reinicies después de editar fstab sin validar. Un fstab roto puede impedir el arranque.

### findmnt --verify

```bash
findmnt --verify
```

Verifica la sintaxis de fstab y reporta:
- Campos faltantes o malformados
- Puntos de montaje que no existen
- UUIDs que no corresponden a ningún dispositivo presente
- Opciones inválidas

```bash
# Ejemplo con error
findmnt --verify
# /mnt/noexiste: mountpoint does not exist

# Ejemplo limpio (sin salida = todo bien)
findmnt --verify && echo "fstab OK"
```

### mount -a (prueba en vivo)

```bash
# Montar todo lo pendiente de fstab
mount -a

# Si falla, muestra el error sin afectar lo ya montado
# mount: /mnt/data: special device UUID=malo... does not exist
```

### systemd-analyze verify (alternativa)

```bash
# Verificar que systemd puede generar units desde fstab
systemd-analyze verify --man=no default.target 2>&1 | grep -i mount
```

### Recuperación si fstab rompe el arranque

Si el sistema no arranca por un fstab incorrecto:

```
1. Kernel panic → en GRUB, editar línea del kernel:
   Añadir: systemd.unit=emergency.target

2. En emergency shell:
   mount -o remount,rw /      ← rootfs en lectura-escritura
   vim /etc/fstab             ← corregir el error
   reboot

3. Alternativa: arrancar desde ISO/USB live
   mount /dev/vda2 /mnt       ← montar rootfs del sistema
   vim /mnt/etc/fstab         ← editar desde fuera
   umount /mnt
   reboot
```

---

## Ejemplos completos

### fstab típico de un servidor

```bash
# /etc/fstab — Servidor con UEFI, LVM y datos separados
#
# <device>                                 <mount>      <type>  <options>              <dump> <pass>
UUID=ABCD-1234                             /boot/efi    vfat    umask=0077             0      0
UUID=11111111-2222-3333-4444-555555555555   /boot        ext4    defaults               0      2
/dev/mapper/vg0-root                       /            xfs     defaults               0      1
/dev/mapper/vg0-home                       /home        xfs     defaults,nodev         0      2
/dev/mapper/vg0-var                        /var         xfs     defaults,nosuid        0      2
/dev/mapper/vg0-swap                       none         swap    defaults               0      0
tmpfs                                      /tmp         tmpfs   defaults,noexec,nosuid,nodev,size=2G  0  0
```

### fstab de un lab QEMU/KVM

```bash
# /etc/fstab — VM de lab con discos virtio
#
UUID=aaaaaaaa-bbbb-cccc-dddd-eeeeeeeeeeee   /            ext4    defaults     0   1
UUID=ffffffff-1111-2222-3333-444444444444    /mnt/disk1   ext4    defaults     0   2
UUID=55555555-6666-7777-8888-999999999999    /mnt/disk2   xfs     defaults     0   2
UUID=00000000-aaaa-bbbb-cccc-dddddddddddd   none         swap    defaults     0   0
```

### Entradas especiales

```bash
# tmpfs — filesystem en RAM (no necesita dispositivo)
tmpfs   /dev/shm   tmpfs   defaults          0   0
tmpfs   /run       tmpfs   defaults,mode=755 0   0

# NFS — requiere _netdev para esperar red
server:/export/share   /mnt/nfs   nfs   defaults,_netdev,vers=4.2   0   0

# ISO montado como loop
/opt/images/rhel9.iso   /mnt/iso   iso9660   loop,ro,auto   0   0

# Btrfs con subvolumen
UUID=12345678-...   /          btrfs   subvol=@,compress=zstd   0   0
UUID=12345678-...   /home      btrfs   subvol=@home             0   0

# Swap file (no partición)
/swapfile   none   swap   defaults   0   0
```

---

## Errores comunes

### 1. UUID incorrecto o con typo

```bash
# ✗ UUID mal copiado — sistema no arranca o filesystem no se monta
UUID=a1b2c3d4-e5f6-xxxx-0000-ef1234567890   /mnt/data   ext4   defaults   0   2

# ✓ Copiar directamente de blkid
blkid /dev/vdb1
# Copiar el UUID exacto, o usar:
blkid -s UUID -o value /dev/vdb1 | xargs -I{} echo "UUID={}"
```

### 2. Punto de montaje inexistente

```bash
# ✗ El directorio no existe — mount -a falla
UUID=...   /mnt/datos   ext4   defaults   0   2
# mount: /mnt/datos: mount point does not exist

# ✓ Crear el directorio antes
mkdir -p /mnt/datos
```

### 3. No validar antes de reiniciar

```bash
# ✗ Editar fstab y reiniciar directamente
vim /etc/fstab
reboot  # PELIGROSO

# ✓ Siempre validar
vim /etc/fstab
findmnt --verify
mount -a
# Solo si todo funciona → reboot
```

### 4. Usar device path en lugar de UUID

```bash
# ✗ /dev/sdb1 puede cambiar entre arranques
/dev/sdb1   /mnt/data   ext4   defaults   0   2

# ✓ UUID es estable
UUID=f47ac10b-...   /mnt/data   ext4   defaults   0   2
```

### 5. Olvidar _netdev en montajes de red

```bash
# ✗ Sin _netdev — systemd intenta montar antes de que haya red
server:/share   /mnt/nfs   nfs   defaults   0   0
# Resultado: timeout en arranque, posible boot failure

# ✓ Con _netdev — espera a que la red esté activa
server:/share   /mnt/nfs   nfs   defaults,_netdev   0   0
```

---

## Cheatsheet

```
┌──────────────────────────────────────────────────────────────────┐
│                     /etc/fstab — Referencia rápida               │
├──────────────────────────────────────────────────────────────────┤
│                                                                  │
│  FORMATO:                                                        │
│  <device>  <mountpoint>  <type>  <options>  <dump>  <pass>       │
│                                                                  │
│  DEVICE (campo 1):                                               │
│    UUID=xxxx        ← recomendado                                │
│    LABEL=name       ← legible, si es único                       │
│    /dev/sdXN        ← solo labs temporales                       │
│                                                                  │
│  OPCIONES COMUNES (campo 4):                                     │
│    defaults          = rw,suid,dev,exec,auto,nouser,async        │
│    noatime           = no actualizar atime (rendimiento)         │
│    noexec,nosuid     = seguridad para /tmp, USB                  │
│    _netdev           = requiere red (NFS, iSCSI)                 │
│    noauto            = no montar en boot / mount -a              │
│    user / users      = permitir mount/umount a usuarios          │
│                                                                  │
│  PASS (campo 6):                                                 │
│    1 = rootfs        2 = otros locales        0 = no verificar   │
│                                                                  │
│  COMANDOS:                                                       │
│    blkid /dev/X              obtener UUID                        │
│    findmnt --verify          validar sintaxis de fstab           │
│    mount -a                  montar todo lo pendiente            │
│    findmnt /mnt/X            verificar montaje                   │
│    systemctl daemon-reload   recargar units si se edita fstab    │
│                                                                  │
│  FLUJO SEGURO:                                                   │
│    mkfs → mkdir → blkid → editar fstab → findmnt --verify →     │
│    mount -a → findmnt → (recién entonces considerar reboot)      │
│                                                                  │
└──────────────────────────────────────────────────────────────────┘
```

---

## Ejercicios

### Ejercicio 1: Añadir un disco a fstab

En tu VM de lab con un disco extra `/dev/vdb` ya particionado y formateado:

1. Obtén el UUID de `/dev/vdb1` con `blkid`
2. Crea el punto de montaje `/mnt/persistent`
3. Añade la entrada a `/etc/fstab` usando UUID, tipo ext4, opciones `defaults,noatime`, dump `0`, pass `2`
4. Valida con `findmnt --verify`
5. Monta con `mount -a`
6. Verifica con `findmnt /mnt/persistent`
7. Reinicia la VM y confirma que el montaje persiste

> **Pregunta de reflexión**: ¿Qué pasaría si cambias el UUID en fstab por uno que no existe y reinicias? ¿El sistema arrancará? ¿Qué verías en la consola?

### Ejercicio 2: Configurar /tmp como tmpfs

1. Verifica cómo está montado `/tmp` actualmente (`findmnt /tmp`)
2. Añade esta línea a `/etc/fstab`:
   ```
   tmpfs   /tmp   tmpfs   defaults,noexec,nosuid,nodev,size=1G   0   0
   ```
3. Ejecuta `mount -a` (no funcionará si /tmp ya está montado)
4. Para aplicar: `umount /tmp && mount -a` o simplemente reinicia
5. Tras el montaje, verifica con `findmnt /tmp` que las opciones `noexec,nosuid,nodev` están activas
6. Prueba: crea un script en /tmp e intenta ejecutarlo — debería fallar por `noexec`

> **Pregunta de reflexión**: ¿Por qué es una buena práctica montar `/tmp` con `noexec`? ¿Qué tipo de ataques mitiga?

### Ejercicio 3: Diagnosticar fstab roto

1. Haz una copia de seguridad: `cp /etc/fstab /etc/fstab.bak`
2. Añade una entrada intencionalmente rota:
   ```
   UUID=00000000-0000-0000-0000-000000000000   /mnt/fake   ext4   defaults   0   2
   ```
3. Ejecuta `findmnt --verify` — ¿qué reporta?
4. Ejecuta `mount -a` — ¿qué error ves?
5. Ahora cambia el pass de `2` a `0` y repite `findmnt --verify`
6. Restaura: `cp /etc/fstab.bak /etc/fstab`

> **Pregunta de reflexión**: ¿Cuál es la diferencia de riesgo entre una entrada fstab con pass=0 y pass=2 cuando el UUID no existe? ¿En cuál caso es más probable que el sistema se cuelgue al arrancar?
