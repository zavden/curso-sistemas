# udev: reglas y nombres persistentes

## Índice

1. [Qué es udev](#1-qué-es-udev)
2. [El problema que resuelve](#2-el-problema-que-resuelve)
3. [/dev/disk/by-*: nombres persistentes](#3-devdiskby--nombres-persistentes)
4. [Reglas udev](#4-reglas-udev)
5. [Escribir reglas personalizadas](#5-escribir-reglas-personalizadas)
6. [Recargar y probar reglas](#6-recargar-y-probar-reglas)
7. [udev en la práctica del curso](#7-udev-en-la-práctica-del-curso)
8. [Errores comunes](#8-errores-comunes)
9. [Cheatsheet](#9-cheatsheet)
10. [Ejercicios](#10-ejercicios)

---

## 1. Qué es udev

udev es el gestor de dispositivos del kernel Linux. Cuando el kernel detecta
un dispositivo (al arrancar o por hot-plug), udev:

1. Crea el archivo en `/dev/` (ej: `/dev/vdb`)
2. Asigna permisos y propietario
3. Crea symlinks (ej: `/dev/disk/by-uuid/...`)
4. Ejecuta acciones configuradas (scripts, notificaciones)

```
┌────────────────────────────────────────────────────────────┐
│                    FLUJO DE udev                           │
│                                                            │
│  Kernel detecta dispositivo                                │
│       │                                                    │
│       ▼                                                    │
│  Envía evento (uevent) a udevd                             │
│       │                                                    │
│       ▼                                                    │
│  udevd evalúa reglas (/etc/udev/rules.d/                   │
│                        /usr/lib/udev/rules.d/)             │
│       │                                                    │
│       ├──► Crea /dev/vdb                                   │
│       ├──► Asigna permisos (0660, grupo disk)              │
│       ├──► Crea symlinks (/dev/disk/by-uuid/...)           │
│       └──► Ejecuta acciones (RUN+=)                        │
└────────────────────────────────────────────────────────────┘
```

udev corre como daemon (`systemd-udevd.service`) y está siempre activo.

---

## 2. El problema que resuelve

### 2.1. Sin udev (Linux antiguo)

Antes de udev, `/dev/` contenía archivos estáticos creados manualmente.
Había miles de archivos para dispositivos que no existían:

```
/dev/ antiguo:
  sda, sda1, sda2, ..., sda15
  sdb, sdb1, sdb2, ..., sdb15
  ...hasta sdz
  hda, hda1, ...hasta hdz
  ttyS0, ttyS1, ...hasta ttyS255

  → Miles de archivos, la mayoría para hardware inexistente
  → Permisos fijos, sin flexibilidad
  → Nombres inestables entre reboots
```

### 2.2. Con udev (Linux moderno)

```
/dev/ moderno:
  Solo existen archivos para hardware realmente presente
  Permisos dinámicos según reglas
  Symlinks persistentes automáticos
  Acciones al conectar/desconectar
```

### 2.3. El problema de los nombres inestables

Como vimos en T01, `/dev/sdX` depende del orden de detección. udev resuelve esto
creando symlinks estables en `/dev/disk/`:

```
Inestable:                        Estable (udev):
/dev/sda  (¿cuál disco?)         /dev/disk/by-uuid/a1b2c3d4-...
                                  /dev/disk/by-id/ata-Samsung_860_S3Y9-part1
                                  /dev/disk/by-label/root
                                  /dev/disk/by-path/pci-0000:00:1f.2-part1
```

---

## 3. /dev/disk/by-*: nombres persistentes

udev crea automáticamente varios esquemas de nombres persistentes. Todos son
symlinks que apuntan al dispositivo real:

### 3.1. by-uuid

```bash
ls -la /dev/disk/by-uuid/
```

```
a1b2c3d4-e5f6-7890-abcd-ef1234567890 -> ../../vda1
11223344-5566-7788-99aa-bbccddeeff00 -> ../../vda5
```

| Aspecto | Detalle |
|---------|---------|
| Fuente | UUID generado por `mkfs` al crear el filesystem |
| Único | Sí — probabilidad de colisión prácticamente nula |
| Cambia si | Se reformatea con `mkfs` (genera nuevo UUID) |
| No cambia si | Se mueve el disco a otro puerto/máquina |
| Uso principal | `/etc/fstab` — la forma más confiable de referenciar particiones |

**Solo aparecen dispositivos con filesystem**. Los discos vacíos no tienen UUID.

### 3.2. by-id

```bash
ls -la /dev/disk/by-id/
```

```
# En una máquina real:
ata-Samsung_SSD_860_EVO_500GB_S3Y9NB0K123456 -> ../../sda
ata-Samsung_SSD_860_EVO_500GB_S3Y9NB0K123456-part1 -> ../../sda1
usb-SanDisk_Cruzer_Blade_4C530001234567-0:0 -> ../../sdb

# En una VM virtio:
virtio-serial12345 -> ../../vda
virtio-serial12345-part1 -> ../../vda1
```

| Aspecto | Detalle |
|---------|---------|
| Fuente | Modelo + serial del hardware |
| Único | Sí (si el hardware tiene serial único) |
| Cambia si | Cambias el hardware físico |
| No cambia si | Mueves el disco a otro puerto |
| Uso principal | Identificar un disco físico específico |

### 3.3. by-label

```bash
ls -la /dev/disk/by-label/
```

```
root -> ../../vda1
swap -> ../../vda5
```

| Aspecto | Detalle |
|---------|---------|
| Fuente | Label asignado con `mkfs -L` o `e2label`/`xfs_admin -L` |
| Único | No garantizado — tú eliges el nombre, puede repetirse |
| Cambia si | Cambias el label (`e2label`, `xfs_admin -L`) |
| Uso principal | Identificación humana, no recomendado para fstab |

Las labels son opcionales. Si no asignaste label con `mkfs -L`, el dispositivo
no aparece aquí.

### 3.4. by-path

```bash
ls -la /dev/disk/by-path/
```

```
pci-0000:00:05.0 -> ../../vda
pci-0000:00:05.0-part1 -> ../../vda1
pci-0000:00:05.0-part2 -> ../../vda2
pci-0000:00:05.0-part5 -> ../../vda5
pci-0000:00:06.0 -> ../../vdb
pci-0000:00:07.0 -> ../../vdc
```

| Aspecto | Detalle |
|---------|---------|
| Fuente | Ruta del bus PCI/USB por donde se conecta el dispositivo |
| Único | Sí, por máquina (la ruta PCI es fija) |
| Cambia si | Mueves el disco a otro slot PCI/puerto USB |
| No cambia si | Reemplazas el disco por otro en el mismo slot |
| Uso principal | Referirse a un slot físico específico |

### 3.5. by-partuuid

```bash
ls -la /dev/disk/by-partuuid/
```

```
12345678-01 -> ../../vda1
12345678-02 -> ../../vda2
12345678-05 -> ../../vda5
```

| Aspecto | Detalle |
|---------|---------|
| Fuente | UUID de la partición en la tabla GPT/MBR |
| Único | Sí en GPT, pseudo-único en MBR |
| Cambia si | Reparticionas el disco |
| No cambia si | Formateas el filesystem |
| Uso principal | Bootloader (GRUB, systemd-boot) |

### 3.6. Cuál usar en cada situación

```
┌────────────────────────────────────────────────────────────┐
│              ¿QUÉ IDENTIFICADOR USAR?                     │
│                                                            │
│  /etc/fstab             →  by-uuid (el más confiable)      │
│  Scripts de backup      →  by-id (identifica hardware)     │
│  Documentación interna  →  by-label (legible por humanos)  │
│  Bootloader             →  by-partuuid                     │
│  Hardware fijo (SAN)    →  by-path                         │
│  Labs del curso         →  /dev/vdX (estable en VMs)       │
└────────────────────────────────────────────────────────────┘
```

---

## 4. Reglas udev

### 4.1. Dónde viven las reglas

```
/usr/lib/udev/rules.d/    ← reglas del sistema (paquetes, distro)
/etc/udev/rules.d/         ← reglas del administrador (tus reglas)
```

Las reglas en `/etc/` tienen prioridad sobre las de `/usr/lib/` con el mismo
nombre. Los archivos se procesan en orden numérico:

```
/usr/lib/udev/rules.d/
  50-udev-default.rules     ← reglas base del sistema
  60-persistent-storage.rules  ← crea /dev/disk/by-*
  70-seat.rules
  ...

/etc/udev/rules.d/
  99-custom.rules            ← tus reglas personalizadas
```

Número menor = se procesa primero. Usa números altos (90-99) para tus reglas.

### 4.2. Anatomía de una regla

Una regla udev tiene dos partes: **condiciones** (match) y **acciones**:

```
CONDICIÓN1, CONDICIÓN2, ..., ACCIÓN1, ACCIÓN2
```

Las condiciones usan `==` (comparar). Las acciones usan `=` o `+=` (asignar):

```
# Ejemplo: cuando se conecte un USB SanDisk, crear symlink /dev/mi-usb
SUBSYSTEM=="block", ATTRS{idVendor}=="0781", SYMLINK+="mi-usb"
 ──────────────────────────────────────────   ──────────────────
              condiciones (match)                  acción
```

### 4.3. Operadores

| Operador | Significado | Uso |
|----------|-------------|-----|
| `==` | Es igual a | Condición |
| `!=` | No es igual a | Condición |
| `=` | Asignar valor | Acción |
| `+=` | Agregar valor | Acción (ej: agregar symlink) |
| `:=` | Asignar valor final (no modificable) | Acción |

### 4.4. Claves de condición comunes

| Clave | Qué compara | Ejemplo |
|-------|-------------|---------|
| `KERNEL` | Nombre del device en el kernel | `KERNEL=="vd[b-z]"` |
| `SUBSYSTEM` | Subsistema | `SUBSYSTEM=="block"` |
| `ATTR{X}` | Atributo del dispositivo | `ATTR{size}=="4194304"` |
| `ATTRS{X}` | Atributo del dispositivo o ancestros | `ATTRS{idVendor}=="0781"` |
| `ENV{X}` | Variable de entorno udev | `ENV{ID_FS_TYPE}=="ext4"` |
| `ACTION` | Tipo de evento | `ACTION=="add"` |
| `DRIVER` | Driver del dispositivo | `DRIVER=="usb-storage"` |

### 4.5. Claves de acción comunes

| Clave | Qué hace | Ejemplo |
|-------|----------|---------|
| `NAME` | Renombrar el device | `NAME="my-disk"` |
| `SYMLINK` | Crear symlink | `SYMLINK+="my-usb"` |
| `OWNER` | Asignar propietario | `OWNER="usuario"` |
| `GROUP` | Asignar grupo | `GROUP="users"` |
| `MODE` | Asignar permisos | `MODE="0666"` |
| `RUN` | Ejecutar comando | `RUN+="/usr/local/bin/notify.sh"` |
| `LABEL` | Saltar a un label | `LABEL="end"` |
| `GOTO` | Ir a un label | `GOTO="end"` |

---

## 5. Escribir reglas personalizadas

### 5.1. Proceso para crear una regla

```
1. Identificar el dispositivo    → udevadm info /dev/xxx
2. Elegir atributos únicos       → SUBSYSTEM, ATTRS{idVendor}, etc.
3. Escribir la regla             → /etc/udev/rules.d/99-nombre.rules
4. Recargar reglas               → udevadm control --reload-rules
5. Probar                        → udevadm test o reconectar el device
```

### 5.2. Ejemplo: symlink para un USB específico

Paso 1 — identificar el dispositivo:

```bash
# Conectar el USB y buscar sus atributos
udevadm info --attribute-walk /dev/sdb | grep -E "(idVendor|idProduct|serial)"
```

```
    ATTRS{idVendor}=="0781"
    ATTRS{idProduct}=="5567"
    ATTRS{serial}=="4C530001234567890"
```

Paso 2 — escribir la regla:

```bash
sudo vim /etc/udev/rules.d/99-usb-lab.rules
```

```
# USB SanDisk Cruzer Blade — create /dev/lab-usb symlink
SUBSYSTEM=="block", ATTRS{idVendor}=="0781", ATTRS{idProduct}=="5567", SYMLINK+="lab-usb"
```

Paso 3 — recargar y verificar:

```bash
sudo udevadm control --reload-rules
sudo udevadm trigger

# Verificar
ls -la /dev/lab-usb
# lrwxrwxrwx 1 root root 3 ... /dev/lab-usb -> sdb
```

Ahora `/dev/lab-usb` siempre apunta a ese USB, sin importar si es `sdb` o `sdc`.

### 5.3. Ejemplo: permisos para un grupo

Dar acceso sin sudo a dispositivos de bloque para un usuario de lab:

```
# /etc/udev/rules.d/99-lab-disks.rules
# Allow group "labusers" to access virtio disks vdb-vdz
SUBSYSTEM=="block", KERNEL=="vd[b-z]", GROUP="labusers", MODE="0660"
```

### 5.4. Ejemplo: ejecutar script al conectar

```
# /etc/udev/rules.d/99-usb-notify.rules
# Log when any USB storage device is connected
ACTION=="add", SUBSYSTEM=="block", DRIVER=="usb-storage", \
  RUN+="/usr/local/bin/usb-connected.sh %k"
```

El `%k` se sustituye por el nombre del kernel (ej: `sdb`). El `\` permite
continuar la regla en la siguiente línea.

### 5.5. Variables de sustitución

| Variable | Se sustituye por | Ejemplo |
|----------|------------------|---------|
| `%k` | Nombre del kernel | `sdb` |
| `%n` | Número del kernel | `2` (de sdb) |
| `%p` | Devpath | `/devices/pci.../block/sdb` |
| `%M` | Major number | `8` |
| `%m` | Minor number | `16` |

---

## 6. Recargar y probar reglas

### 6.1. Recargar reglas

Después de crear o modificar un archivo en `/etc/udev/rules.d/`:

```bash
# Recargar reglas (no afecta dispositivos ya conectados)
sudo udevadm control --reload-rules

# Forzar re-evaluación de reglas en dispositivos existentes
sudo udevadm trigger
```

`--reload-rules` carga las reglas nuevas en memoria. `trigger` fuerza a udev
a re-evaluar las reglas para los dispositivos que ya están presentes.

### 6.2. Probar una regla sin aplicarla

```bash
# Simular qué haría udev con un dispositivo
sudo udevadm test /sys/block/vdb
```

Esto muestra todas las reglas que se evaluarían y sus resultados, sin ejecutar
las acciones `RUN+=`. Útil para debugging.

### 6.3. Monitorear eventos

```bash
# Ver eventos en tiempo real
udevadm monitor --property
```

Conecta/desconecta un dispositivo (o haz `virsh attach-disk`) y verás:

```
KERNEL[12345.678] add      /devices/.../block/vdf (block)
ACTION=add
DEVNAME=/dev/vdf
DEVTYPE=disk
MAJOR=252
MINOR=80
SUBSYSTEM=block

UDEV  [12345.690] add      /devices/.../block/vdf (block)
ACTION=add
DEVNAME=/dev/vdf
...symlinks creados, reglas aplicadas...
```

La diferencia entre `KERNEL` y `UDEV`: el evento KERNEL es el raw del kernel,
el evento UDEV es después de que udevd procesó las reglas.

---

## 7. udev en la práctica del curso

### 7.1. ¿Necesitas reglas udev para los labs?

```
┌────────────────────────────────────────────────────────────┐
│          ¿NECESITO REGLAS UDEV PARA B08?                  │
│                                                            │
│  Generalmente NO.                                          │
│                                                            │
│  En VMs virtio:                                            │
│    • vda siempre es el disco del SO                        │
│    • vdb-vde siempre son los discos extra                  │
│    • Los nombres son estables (determinados por XML)       │
│    • No hay hardware real que cambie de puerto              │
│                                                            │
│  Donde SÍ importa udev:                                   │
│    • /etc/fstab: usar UUID (by-uuid) en vez de /dev/vdX   │
│    • Entender los symlinks que las herramientas generan    │
│    • Producción real con múltiples discos SCSI/SATA        │
│    • USB passthrough                                       │
└────────────────────────────────────────────────────────────┘
```

### 7.2. Lo que sí usarás

Aunque no escribas reglas udev propias, usarás los resultados de udev
constantemente:

```bash
# En /etc/fstab (C02) — usarás UUIDs
UUID=a1b2c3d4-... /data ext4 defaults 0 2

# ¿De dónde sale ese UUID? De /dev/disk/by-uuid/
# ¿Quién crea ese symlink? udev (regla 60-persistent-storage.rules)

# Para encontrar el UUID:
sudo blkid /dev/vdb1
# o
ls -la /dev/disk/by-uuid/
```

### 7.3. Regla 60-persistent-storage.rules

Esta es la regla del sistema que crea los symlinks en `/dev/disk/by-*`. Puedes
examinarla:

```bash
cat /usr/lib/udev/rules.d/60-persistent-storage.rules
```

Es larga y compleja, pero las partes clave son:

```
# by-id: usa el serial del hardware
ENV{ID_SERIAL}!="", SYMLINK+="disk/by-id/$env{ID_BUS}-$env{ID_SERIAL}"

# by-path: usa la ruta PCI
ENV{ID_PATH}!="", SYMLINK+="disk/by-path/$env{ID_PATH}"

# by-uuid: usa el UUID del filesystem
ENV{ID_FS_UUID_ENC}!="", SYMLINK+="disk/by-uuid/$env{ID_FS_UUID_ENC}"

# by-label: usa el label del filesystem
ENV{ID_FS_LABEL_ENC}!="", SYMLINK+="disk/by-label/$env{ID_FS_LABEL_ENC}"
```

Estas reglas se ejecutan automáticamente para cada dispositivo de bloque.
No necesitas modificarlas.

---

## 8. Errores comunes

### Error 1: editar reglas en /usr/lib/ en vez de /etc/

```bash
# ✗ Modificar reglas del sistema
sudo vim /usr/lib/udev/rules.d/60-persistent-storage.rules
# Se sobrescribirá en la siguiente actualización del paquete

# ✓ Crear reglas propias en /etc/
sudo vim /etc/udev/rules.d/99-custom.rules
# Persiste entre actualizaciones
```

Si necesitas anular una regla del sistema, crea un archivo con el mismo nombre
en `/etc/udev/rules.d/` — la versión de `/etc/` tiene prioridad.

### Error 2: olvidar recargar después de cambiar reglas

```bash
# ✗ Crear regla y esperar que funcione inmediatamente
sudo vim /etc/udev/rules.d/99-custom.rules
# ... nada pasa ...

# ✓ Recargar y trigger
sudo udevadm control --reload-rules
sudo udevadm trigger
```

### Error 3: usar ATTR cuando se necesita ATTRS

```bash
# ATTR{x}   → atributo del dispositivo EXACTO (este nivel)
# ATTRS{x}  → atributo de este dispositivo O cualquier ancestro

# ✗ idVendor está en el dispositivo USB padre, no en el block device
SUBSYSTEM=="block", ATTR{idVendor}=="0781", SYMLINK+="mi-usb"
# No matchea — el block device no tiene idVendor

# ✓ ATTRS busca en toda la cadena de ancestros
SUBSYSTEM=="block", ATTRS{idVendor}=="0781", SYMLINK+="mi-usb"
# Funciona — encuentra idVendor en el USB device padre
```

### Error 4: mezclar ATTRS de diferentes niveles de ancestros

```bash
# ✗ ATTRS de diferentes niveles en la misma regla
ATTRS{idVendor}=="0781", ATTRS{driver}=="sd", SYMLINK+="mi-usb"
# Si idVendor está en el abuelo y driver en el padre, no matchea

# Las condiciones ATTRS deben referirse al MISMO ancestro
# ✓ Usar udevadm info --attribute-walk para ver qué atributos
# están en qué nivel
```

### Error 5: confundir UUID con PARTUUID en fstab

```bash
# ✗ Mezclar UUID y PARTUUID
# UUID= espera el UUID del filesystem (de blkid)
# PARTUUID= espera el UUID de la partición (de la tabla GPT/MBR)

# ✓ Verificar cuál tienes
sudo blkid /dev/vda1
# UUID="a1b2c3d4-..."      ← para UUID= en fstab
# PARTUUID="12345678-01"   ← para PARTUUID= en fstab

# Son valores DIFERENTES. Usar el incorrecto → no monta.
```

---

## 9. Cheatsheet

```bash
# ── Nombres persistentes (/dev/disk/) ───────────────────────────
ls -la /dev/disk/by-uuid/        # por UUID del filesystem
ls -la /dev/disk/by-id/          # por modelo+serial del hardware
ls -la /dev/disk/by-label/       # por label del filesystem
ls -la /dev/disk/by-path/        # por ruta del bus PCI/USB
ls -la /dev/disk/by-partuuid/    # por UUID de la partición

# ── Información de dispositivos ──────────────────────────────────
udevadm info /dev/vda                    # propiedades
udevadm info --query=symlink /dev/vda    # symlinks creados
udevadm info --attribute-walk /dev/vda   # jerarquía de atributos

# ── Reglas udev ──────────────────────────────────────────────────
# Ubicación de reglas:
#   /usr/lib/udev/rules.d/   ← sistema (NO editar)
#   /etc/udev/rules.d/       ← tus reglas

# Formato de regla:
#   CONDICION=="valor", CONDICION=="valor", ACCION+="valor"

# Ejemplo:
#   SUBSYSTEM=="block", KERNEL=="vd[b-z]", SYMLINK+="lab-%k"

# ── Gestión de reglas ────────────────────────────────────────────
sudo udevadm control --reload-rules      # recargar reglas
sudo udevadm trigger                     # re-evaluar dispositivos
sudo udevadm test /sys/block/vdb         # simular (dry-run)

# ── Monitoreo ────────────────────────────────────────────────────
udevadm monitor                          # eventos en tiempo real
udevadm monitor --property               # con propiedades detalladas

# ── Variables de sustitución en reglas ───────────────────────────
# %k = nombre kernel (sdb)
# %n = número kernel (2)
# %p = devpath
# %M = major     %m = minor

# ── Cuándo usar cada by-* ───────────────────────────────────────
# /etc/fstab           →  by-uuid
# Identificar hardware →  by-id
# Labels humanos       →  by-label
# Bootloader           →  by-partuuid
# Slot físico fijo     →  by-path
# Labs en VMs          →  /dev/vdX (estable)
```

---

## 10. Ejercicios

### Ejercicio 1: explorar los nombres persistentes

1. Ejecuta `ls -la /dev/disk/by-uuid/` — ¿cuántas entradas hay?
2. Ejecuta `ls -la /dev/disk/by-path/` — ¿aparecen los discos vacíos (vdb-vde)?
3. Compara: ¿por qué los discos vacíos aparecen en `by-path` pero no en `by-uuid`?
4. Crea un filesystem en vdb: `sudo mkfs.ext4 -L test-label /dev/vdb`
5. Vuelve a revisar `by-uuid/` y `by-label/` — ¿apareció vdb?
6. Limpia: `sudo wipefs -a /dev/vdb`
7. Revisa de nuevo `by-uuid/` y `by-label/` — ¿desapareció?

**Pregunta de reflexión**: `by-path` funciona sin filesystem porque se basa en
la ruta PCI del hardware. `by-uuid` necesita filesystem porque el UUID se
almacena dentro del filesystem. ¿Qué implicaciones tiene esto para `/etc/fstab`
si formateas una partición accidentalmente?

### Ejercicio 2: investigar reglas del sistema

1. Ejecuta `ls /usr/lib/udev/rules.d/ | head -20` — observa la numeración
2. Busca la regla que crea los symlinks `by-uuid`:
   `grep "by-uuid" /usr/lib/udev/rules.d/60-persistent-storage.rules`
3. Ejecuta `sudo udevadm test /sys/block/vda 2>/dev/null | grep SYMLINK`
   — ¿qué symlinks crearía udev para vda?
4. Compara con `udevadm info --query=symlink /dev/vda`

**Pregunta de reflexión**: las reglas del sistema usan números bajos (50-70)
y tus reglas deberían usar números altos (90-99). ¿Por qué importa el orden?
¿Qué pasaría si tu regla tiene número menor que la del sistema?

### Ejercicio 3: crear una regla personalizada

1. Crea el archivo `/etc/udev/rules.d/99-lab-disks.rules` con esta regla:
   ```
   SUBSYSTEM=="block", KERNEL=="vdb", SYMLINK+="lab-disk-primary"
   SUBSYSTEM=="block", KERNEL=="vdc", SYMLINK+="lab-disk-secondary"
   ```
2. Ejecuta `sudo udevadm control --reload-rules && sudo udevadm trigger`
3. Verifica: `ls -la /dev/lab-disk-*`
4. Verifica que puedes usar el symlink: `sudo fdisk -l /dev/lab-disk-primary`
5. Limpia: elimina el archivo de reglas y recarga

**Pregunta de reflexión**: el symlink `/dev/lab-disk-primary` apunta a
`/dev/vdb`. ¿Qué ventaja tiene usar el symlink sobre usar `/dev/vdb`
directamente? En una VM con discos virtio, ¿es realmente necesario? ¿Cuándo
sí sería útil este tipo de regla?
