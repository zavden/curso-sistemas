# Partición vs archivo swap

## Índice

1. [Qué es swap](#qué-es-swap)
2. [Cuándo se usa swap](#cuándo-se-usa-swap)
3. [Cuánto swap configurar](#cuánto-swap-configurar)
4. [Partición swap](#partición-swap)
5. [Archivo swap (swapfile)](#archivo-swap-swapfile)
6. [mkswap — inicializar swap](#mkswap--inicializar-swap)
7. [swapon y swapoff](#swapon-y-swapoff)
8. [Prioridad de swap](#prioridad-de-swap)
9. [Swap en LVM](#swap-en-lvm)
10. [Persistencia en fstab](#persistencia-en-fstab)
11. [Partición vs archivo — comparación](#partición-vs-archivo--comparación)
12. [Errores comunes](#errores-comunes)
13. [Cheatsheet](#cheatsheet)
14. [Ejercicios](#ejercicios)

---

## Qué es swap

Swap es espacio en disco que el kernel usa como extensión de la RAM. Cuando la memoria física se agota, el kernel mueve páginas de memoria poco usadas al swap para liberar RAM para procesos activos.

```
┌──────────────────────────────────────────────────────┐
│                                                      │
│   RAM (4 GiB)                                        │
│   ┌──────────────────────────────────────┐           │
│   │ Proceso A │ Proceso B │ Kernel │ ... │           │
│   │           │           │        │     │           │
│   └──────────────────────────────────────┘           │
│         │                                            │
│         │  RAM llena → mover páginas poco usadas     │
│         ▼                                            │
│   Swap (disco)                                       │
│   ┌──────────────────────┐                           │
│   │ Páginas inactivas    │                           │
│   │ de Proceso A, B...   │                           │
│   └──────────────────────┘                           │
│                                                      │
│   Cuando esas páginas se necesitan de nuevo:         │
│   swap → RAM (swap in, page fault)                   │
│                                                      │
└──────────────────────────────────────────────────────┘
```

Swap **no es un reemplazo de la RAM** — el disco es órdenes de magnitud más lento. Es un mecanismo de emergencia y optimización:
- **Emergencia**: evita el OOM killer cuando la RAM se agota
- **Optimización**: libera RAM para caché de disco moviendo páginas inactivas

---

## Cuándo se usa swap

El kernel decide qué mover a swap basándose en la actividad de las páginas de memoria:

```
Página activa    → permanece en RAM
Página inactiva  → candidata para swap out
                   (si la RAM está bajo presión)
```

Swap se usa cuando:

| Situación | Comportamiento |
|-----------|---------------|
| RAM al 100%, proceso pide más | Swap out de páginas inactivas → RAM libre |
| Proceso duerme por horas | Sus páginas se mueven a swap → RAM para otros |
| Hibernación (suspend-to-disk) | Todo el contenido de RAM se escribe a swap |
| RAM suficiente pero kernel optimiza | Páginas frías a swap → más RAM para caché |

Swap **no** se usa cuando:
- Hay RAM suficiente y `vm.swappiness` es bajo
- El proceso accede activamente a todas sus páginas

---

## Cuánto swap configurar

No hay una regla universal. Depende de la RAM, la carga de trabajo, y si necesitas hibernación.

### Recomendaciones generales

| RAM | Swap recomendado | Con hibernación |
|-----|-----------------|-----------------|
| ≤ 2 GiB | 2× RAM | 3× RAM |
| 2-8 GiB | = RAM | 2× RAM |
| 8-64 GiB | 4-8 GiB (fijo) | = RAM |
| > 64 GiB | 4-8 GiB | No práctico |

### Para servidores

```
Regla práctica para servidores:
  - Siempre tener algo de swap (mínimo 1-2 GiB)
  - Sin swap, el OOM killer actúa antes y más agresivamente
  - Demasiado swap es inofensivo (no se usa si no se necesita)
  - Swap no ocupa RAM — solo espacio en disco
```

### Para VMs de lab

```bash
# En VMs de lab con 1-2 GiB de RAM:
# 512 MiB - 1 GiB de swap es suficiente
```

---

## Partición swap

Una partición dedicada formateada como swap. Es el método tradicional.

### Crear partición swap

```bash
# 1. Crear partición (con fdisk, gdisk o parted)
fdisk /dev/vdb
# n → nueva partición → tamaño +1G
# t → cambiar tipo → 82 (Linux swap) para MBR
#                   → 19 (Linux swap) para GPT en fdisk
# w → escribir

# 2. Inicializar como swap
mkswap /dev/vdb1

# 3. Activar
swapon /dev/vdb1

# 4. Verificar
swapon --show
# NAME      TYPE      SIZE USED PRIO
# /dev/vdb1 partition   1G   0B   -2
```

### Tipo de partición

| Tabla | Tipo | Código |
|-------|------|--------|
| MBR | Linux swap | `82` |
| GPT | Linux swap | `0657FD6D-A4AB-43C4-84E5-0933C84B4F4F` (o `swap` en fdisk) |

El tipo no es obligatorio — `mkswap` funciona sin importar el tipo de partición — pero documenta el propósito.

---

## Archivo swap (swapfile)

Un archivo regular en un filesystem existente, usado como swap. Más flexible que una partición.

### Crear archivo swap

```bash
# 1. Crear archivo del tamaño deseado
# Método recomendado (rápido, no fragmentado):
fallocate -l 1G /swapfile

# Alternativa si fallocate no funciona (Btrfs, algunos FS):
dd if=/dev/zero of=/swapfile bs=1M count=1024

# 2. Asegurar permisos (OBLIGATORIO — solo root)
chmod 600 /swapfile

# 3. Inicializar como swap
mkswap /swapfile

# 4. Activar
swapon /swapfile

# 5. Verificar
swapon --show
# NAME      TYPE SIZE USED PRIO
# /swapfile file   1G   0B   -3
```

> **Predicción**: si los permisos no son 600, `swapon` mostrará un warning: "insecure permissions 0644, 0600 suggested". El swap funcionará, pero cualquier usuario podría leer el contenido (que incluye datos de memoria de otros procesos).

### Consideraciones con swapfile

- **Btrfs**: en kernels antiguos (<5.0), fallocate no funcionaba para swap en Btrfs. Usar `dd` o crear un subvolumen dedicado con `nodatacow`
- **XFS/ext4**: `fallocate` funciona sin problemas
- **No usar en RAID md**: el swapfile puede fragmentarse en los discos de forma impredecible
- **No usar copy-on-write**: en Btrfs, el swapfile debe estar en un subvolumen con `chattr +C` (nodatacow)

---

## mkswap — inicializar swap

`mkswap` prepara un dispositivo o archivo para ser usado como swap. Escribe una cabecera con el magic number y metadatos.

### Sintaxis

```bash
mkswap [opciones] <dispositivo_o_archivo>
```

### Opciones

| Opción | Efecto |
|--------|--------|
| `-L <label>` | Asignar un label |
| `-U <UUID>` | Asignar un UUID específico |
| `-f` | Forzar (si el dispositivo tiene datos) |

### Ejemplos

```bash
# Partición
mkswap /dev/vdb1
# Setting up swapspace version 1, size = 1024 MiB (1073737728 bytes)
# no label, UUID=aabbccdd-1122-3344-5566-778899aabbcc

# Con label
mkswap -L myswap /dev/vdb1

# Archivo
mkswap /swapfile

# Ver UUID asignado
blkid /dev/vdb1
# /dev/vdb1: UUID="aabbccdd-..." TYPE="swap"
```

> **Importante**: `mkswap` destruye cualquier dato previo en el dispositivo/archivo. Úsalo solo en dispositivos dedicados a swap.

---

## swapon y swapoff

### swapon — activar swap

```bash
# Activar un dispositivo o archivo
swapon /dev/vdb1
swapon /swapfile

# Activar todo lo definido en fstab
swapon -a

# Activar con prioridad específica
swapon -p 10 /dev/vdb1

# Ver swap activo
swapon --show
# NAME      TYPE      SIZE   USED PRIO
# /dev/vdb1 partition 1024M    0B   -2
# /swapfile file      1024M    0B   -3

# Alternativa: free
free -h
#               total   used   free  shared  buff/cache  available
# Mem:          1.9Gi  500Mi   1.2Gi   10Mi    200Mi      1.3Gi
# Swap:         2.0Gi    0B    2.0Gi
```

### swapoff — desactivar swap

```bash
# Desactivar un swap específico
swapoff /dev/vdb1

# Desactivar todo el swap
swapoff -a

# Desactivar archivo swap
swapoff /swapfile
```

> **Predicción**: `swapoff` mueve todas las páginas del swap de vuelta a RAM antes de desactivar. Si no hay suficiente RAM libre para absorber las páginas, `swapoff` fallará o el sistema se quedará sin memoria. Siempre verificar `free -h` antes de desactivar swap.

### Verificar estado

```bash
# Forma 1: swapon --show (más detallado)
swapon --show
# NAME      TYPE      SIZE   USED PRIO
# /dev/dm-1 partition   2G  100M   -2

# Forma 2: free (resumen)
free -h
# Swap:  2.0Gi  100Mi  1.9Gi

# Forma 3: /proc/swaps (raw)
cat /proc/swaps
# Filename              Type       Size      Used  Priority
# /dev/dm-1             partition  2097148   102400  -2

# Forma 4: swapon --summary (deprecated, usar --show)
```

---

## Prioridad de swap

Cuando hay múltiples áreas de swap activas, la **prioridad** determina cuál se usa primero.

### Cómo funciona

```
Prioridad más alta (número mayor) → se usa primero

  Swap A: prioridad 10 (disco rápido SSD)
  Swap B: prioridad  5 (disco lento HDD)
  Swap C: prioridad -2 (asignada automáticamente)

  Orden de uso: A → B → C
  A se llena primero, luego B, luego C
```

### Misma prioridad = round-robin

```
  Swap A: prioridad 10
  Swap B: prioridad 10

  Se usan en paralelo (round-robin)
  → Similar a striping, mejor rendimiento
```

### Asignar prioridad

```bash
# Con swapon
swapon -p 10 /dev/vdb1    # prioridad 10
swapon -p 5 /swapfile      # prioridad 5

# En fstab
/dev/vdb1   none   swap   defaults,pri=10   0   0
/swapfile   none   swap   defaults,pri=5    0   0

# Sin prioridad explícita: se asigna -1, -2, -3... (decreciente)
```

### Caso de uso: SSD + HDD

```bash
# SSD swap — prioridad alta (se usa primero, más rápido)
swapon -p 100 /dev/ssd_swap

# HDD swap — prioridad baja (solo si SSD swap se llena)
swapon -p 10 /dev/hdd_swap

swapon --show
# NAME           TYPE      SIZE USED PRIO
# /dev/ssd_swap  partition  2G   0B  100
# /dev/hdd_swap  partition  4G   0B   10
```

---

## Swap en LVM

En sistemas con LVM, swap suele ser un **Logical Volume** dedicado.

### Crear swap en LV

```bash
# Crear LV para swap
lvcreate -L 2G -n lv_swap vg0

# Inicializar
mkswap /dev/vg0/lv_swap

# Activar
swapon /dev/vg0/lv_swap

# Persistencia en fstab
echo '/dev/mapper/vg0-lv_swap  none  swap  defaults  0  0' >> /etc/fstab
```

### Redimensionar swap en LVM

Una ventaja de swap en LV: se puede cambiar el tamaño.

```bash
# 1. Desactivar swap
swapoff /dev/vg0/lv_swap

# 2. Redimensionar el LV
lvresize -L 4G /dev/vg0/lv_swap

# 3. Reinicializar swap (mkswap reescribe la cabecera)
mkswap /dev/vg0/lv_swap

# 4. Reactivar
swapon /dev/vg0/lv_swap

# 5. Verificar
swapon --show
```

> **Nota**: `mkswap` es necesario después de redimensionar porque la cabecera de swap tiene el tamaño grabado. Sin `mkswap`, el swap usará solo el tamaño original.

### Instalación típica con LVM

La mayoría de instaladores Linux con LVM crean automáticamente:

```bash
lvs
# LV      VG     LSize
# lv_root rhel   17.00g
# lv_swap rhel    2.00g    ← swap como LV

swapon --show
# NAME                TYPE      SIZE USED PRIO
# /dev/mapper/rhel-lv_swap partition 2G  0B  -2
```

---

## Persistencia en fstab

### Partición swap en fstab

```bash
# Obtener UUID
blkid /dev/vdb1
# /dev/vdb1: UUID="aabbccdd-..." TYPE="swap"

# Añadir a fstab
UUID=aabbccdd-1122-3344-5566-778899aabbcc   none   swap   defaults   0   0
```

Campos de fstab para swap:
- **Campo 2** (mountpoint): `none` o `swap` — no tiene punto de montaje
- **Campo 3** (type): `swap`
- **Campo 5** (dump): `0`
- **Campo 6** (pass): `0` — no se verifica con fsck

### Archivo swap en fstab

```bash
/swapfile   none   swap   defaults   0   0
```

> **Nota**: para swapfiles, se usa el **path del archivo**, no UUID. Los archivos no tienen UUID propio (el UUID de `blkid` es de la cabecera swap, pero fstab identifica el archivo por su path).

### Con prioridad

```bash
UUID=aabbccdd-...   none   swap   defaults,pri=10   0   0
/swapfile            none   swap   defaults,pri=5    0   0
```

### Verificar persistencia

```bash
# Activar todo el swap de fstab
swapon -a

# Verificar
swapon --show
```

---

## Partición vs archivo — comparación

```
┌────────────────────────┬──────────────────┬──────────────────┐
│ Aspecto                │ Partición swap   │ Archivo swap     │
├────────────────────────┼──────────────────┼──────────────────┤
│ Rendimiento            │ Ligeramente      │ Mínimo overhead  │
│                        │ mejor (directo)  │ del filesystem   │
│ Flexibilidad tamaño    │ Requiere         │ Crear/borrar     │
│                        │ reparticionar    │ archivo           │
│ Fragmentación          │ No               │ Posible          │
│ Configuración          │ Más pasos        │ Más simple       │
│ Múltiples swaps        │ Múltiples        │ Múltiples        │
│                        │ particiones      │ archivos         │
│ LVM                    │ LV dedicado      │ Archivo en LV    │
│ Hibernación            │ Soportado        │ Soportado*       │
│ Btrfs                  │ N/A              │ Requiere config  │
│                        │                  │ especial         │
│ Examen RHCSA           │ Esperado         │ También          │
│ Recomendación moderna  │ LV en LVM        │ Swapfile si no   │
│                        │                  │ hay LVM          │
└────────────────────────┴──────────────────┴──────────────────┘

* Hibernación con swapfile requiere configuración adicional del
  kernel (resume=, resume_offset=)
```

### Recomendación

```
¿Tienes LVM? → Sí → Swap como LV (lvcreate -n lv_swap)
              → No → ¿Partición libre?
                      → Sí → Partición swap
                      → No → Archivo swap (/swapfile)
```

---

## Errores comunes

### 1. Olvidar chmod 600 en el swapfile

```bash
# ✗ Permisos por defecto — inseguro
fallocate -l 1G /swapfile
mkswap /swapfile
swapon /swapfile
# swapon: /swapfile: insecure permissions 0644, 0600 suggested.

# ✓ Siempre restringir permisos
chmod 600 /swapfile
mkswap /swapfile
swapon /swapfile
```

### 2. Olvidar mkswap después de crear el dispositivo/archivo

```bash
# ✗ Intentar swapon sin mkswap
fallocate -l 1G /swapfile
chmod 600 /swapfile
swapon /swapfile
# swapon: /swapfile: read swap header failed

# ✓ Siempre mkswap antes de swapon
mkswap /swapfile
swapon /swapfile
```

### 3. Desactivar swap sin suficiente RAM libre

```bash
# ✗ swapoff cuando swap tiene datos y RAM está llena
free -h
# Mem:   1.9Gi  1.8Gi  100Mi    ← casi sin RAM libre
# Swap:  2.0Gi  1.5Gi  500Mi    ← 1.5 GiB en uso

swapoff -a
# El kernel intenta mover 1.5 GiB de swap a 100 MiB de RAM libre
# → OOM killer, sistema colgado, o swapoff se bloquea

# ✓ Verificar que hay RAM suficiente
free -h    # ¿RAM libre > swap usado?
# Si no: liberar memoria primero, o añadir más swap antes de quitar
```

### 4. Usar fallocate en Btrfs para swapfile

```bash
# ✗ fallocate en Btrfs puede crear archivos con extents preallocados
# que swap no puede usar
fallocate -l 1G /swapfile
mkswap /swapfile
swapon /swapfile
# swapon: /swapfile: swapon failed: Invalid argument

# ✓ En Btrfs: usar dd y desactivar CoW
truncate -s 0 /swapfile
chattr +C /swapfile
dd if=/dev/zero of=/swapfile bs=1M count=1024
chmod 600 /swapfile
mkswap /swapfile
swapon /swapfile
```

### 5. Olvidar mkswap después de redimensionar LV swap

```bash
# ✗ Redimensionar sin reinicializar
swapoff /dev/vg0/lv_swap
lvresize -L 4G /dev/vg0/lv_swap
swapon /dev/vg0/lv_swap
# Swap sigue con el tamaño viejo (cabecera no actualizada)

# ✓ mkswap después de resize
swapoff /dev/vg0/lv_swap
lvresize -L 4G /dev/vg0/lv_swap
mkswap /dev/vg0/lv_swap     # reescribir cabecera
swapon /dev/vg0/lv_swap
```

---

## Cheatsheet

```
┌──────────────────────────────────────────────────────────────────┐
│             Partición vs archivo swap — Referencia rápida        │
├──────────────────────────────────────────────────────────────────┤
│                                                                  │
│  CREAR PARTICIÓN SWAP:                                           │
│    fdisk /dev/vdb → tipo 82 (MBR) o swap (GPT)                  │
│    mkswap /dev/vdb1                                              │
│    swapon /dev/vdb1                                              │
│                                                                  │
│  CREAR ARCHIVO SWAP:                                             │
│    fallocate -l 1G /swapfile  (o dd if=/dev/zero)                │
│    chmod 600 /swapfile                                           │
│    mkswap /swapfile                                              │
│    swapon /swapfile                                              │
│                                                                  │
│  CREAR SWAP EN LVM:                                              │
│    lvcreate -L 2G -n lv_swap vg0                                │
│    mkswap /dev/vg0/lv_swap                                       │
│    swapon /dev/vg0/lv_swap                                       │
│                                                                  │
│  GESTIONAR:                                                      │
│    swapon --show               ver swap activo                   │
│    swapon -a                   activar todo (fstab)              │
│    swapoff /dev/vdb1           desactivar (verificar RAM libre)  │
│    free -h                     uso de RAM y swap                 │
│    cat /proc/swaps             raw                               │
│                                                                  │
│  PRIORIDAD:                                                      │
│    swapon -p 10 /dev/vdb1      mayor número = se usa primero     │
│    pri=10 en fstab             misma prioridad = round-robin     │
│                                                                  │
│  FSTAB:                                                          │
│    UUID=xxx   none  swap  defaults       0  0    partición       │
│    /swapfile  none  swap  defaults       0  0    archivo         │
│    /dev/mapper/vg0-lv_swap  none  swap  defaults  0  0   LVM    │
│                                                                  │
│  REDIMENSIONAR (LVM):                                            │
│    swapoff → lvresize → mkswap → swapon                          │
│                                                                  │
│  CUÁNTO:                                                         │
│    RAM ≤ 8G → = RAM    |    RAM > 8G → 4-8 GiB fijo             │
│                                                                  │
└──────────────────────────────────────────────────────────────────┘
```

---

## Ejercicios

### Ejercicio 1: Crear partición swap y archivo swap

En tu VM de lab:

1. Verifica swap actual: `swapon --show` y `free -h`
2. Crea una partición swap en `/dev/vdb`:
   ```bash
   fdisk /dev/vdb    # crear partición de 512 MiB, tipo 82
   mkswap /dev/vdb1
   swapon /dev/vdb1
   ```
3. Verifica: `swapon --show` — ¿aparece con tipo `partition`?
4. Crea un archivo swap:
   ```bash
   fallocate -l 256M /swapfile
   chmod 600 /swapfile
   mkswap /swapfile
   swapon /swapfile
   ```
5. Verifica: `swapon --show` — ¿aparecen ambos?
6. Verifica: `free -h` — ¿el total de swap es la suma?
7. Desactiva el archivo swap: `swapoff /swapfile && rm /swapfile`
8. Desactiva la partición swap: `swapoff /dev/vdb1`

> **Pregunta de reflexión**: ¿por qué es obligatorio `chmod 600` en el swapfile? ¿Qué tipo de información sensible podría estar en el swap?

### Ejercicio 2: Prioridad de swap

1. Crea dos áreas de swap:
   ```bash
   # Partición con prioridad alta
   mkswap /dev/vdb1
   swapon -p 10 /dev/vdb1

   # Archivo con prioridad baja
   fallocate -l 256M /swapfile
   chmod 600 /swapfile
   mkswap /swapfile
   swapon -p 5 /swapfile
   ```
2. Verifica prioridades: `swapon --show` o `cat /proc/swaps`
3. Genera presión de memoria para ver cuál se usa primero:
   ```bash
   # Crear proceso que consume memoria
   python3 -c "x = 'A' * (300 * 1024 * 1024); input('Press enter')" &
   ```
4. Mientras corre, verifica: `swapon --show` — ¿cuál tiene USED > 0?
5. Mata el proceso y verifica que el swap se libera
6. Limpia: `swapoff -a` (cuidado con el swap del sistema)

> **Pregunta de reflexión**: si dos áreas de swap tienen la misma prioridad, el kernel usa round-robin entre ellas. ¿Esto es similar a qué nivel de RAID? ¿Qué ventaja de rendimiento ofrece?

### Ejercicio 3: Swap en LVM con redimensionado

1. Crea el stack:
   ```bash
   pvcreate /dev/vdb
   vgcreate vg_lab /dev/vdb
   lvcreate -L 256M -n lv_swap vg_lab
   mkswap /dev/vg_lab/lv_swap
   swapon /dev/vg_lab/lv_swap
   ```
2. Verifica: `swapon --show` y `lvs`
3. Redimensiona a 512 MiB:
   ```bash
   swapoff /dev/vg_lab/lv_swap
   lvresize -L 512M /dev/vg_lab/lv_swap
   mkswap /dev/vg_lab/lv_swap
   swapon /dev/vg_lab/lv_swap
   ```
4. Verifica: `swapon --show` — ¿muestra 512 MiB?
5. ¿Qué pasa si omites `mkswap` después del resize? Pruébalo:
   ```bash
   swapoff /dev/vg_lab/lv_swap
   lvresize -L 768M /dev/vg_lab/lv_swap
   swapon /dev/vg_lab/lv_swap    # sin mkswap
   swapon --show                  # ¿qué tamaño reporta?
   ```
6. Limpia: `swapoff`, `vgremove -f vg_lab`, `pvremove /dev/vdb`

> **Pregunta de reflexión**: ¿por qué `mkswap` es necesario después de redimensionar, pero `resize2fs` no lo es en ext4 (donde el filesystem detecta el nuevo tamaño)? ¿Qué diferencia hay entre la cabecera de swap y los metadatos de un filesystem?
