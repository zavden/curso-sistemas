# fdisk y gdisk

## Índice

1. [fdisk vs gdisk: cuándo usar cada uno](#1-fdisk-vs-gdisk-cuándo-usar-cada-uno)
2. [fdisk: particionamiento MBR](#2-fdisk-particionamiento-mbr)
3. [fdisk: particionamiento GPT](#3-fdisk-particionamiento-gpt)
4. [gdisk: particionamiento GPT nativo](#4-gdisk-particionamiento-gpt-nativo)
5. [Alineación de particiones](#5-alineación-de-particiones)
6. [Tipos de partición](#6-tipos-de-partición)
7. [Lab completo: particionar un disco](#7-lab-completo-particionar-un-disco)
8. [Errores comunes](#8-errores-comunes)
9. [Cheatsheet](#9-cheatsheet)
10. [Ejercicios](#10-ejercicios)

---

## 1. fdisk vs gdisk: cuándo usar cada uno

```
┌────────────────────────────────────────────────────────────┐
│           fdisk                    gdisk                   │
│  ───────────────────       ───────────────────             │
│  MBR y GPT                 Solo GPT                       │
│  Viene preinstalado        Paquete gdisk                   │
│  Interfaz clásica          Interfaz similar a fdisk        │
│  Comandos: m,n,d,p,w      Comandos: ?,n,d,p,w             │
│                                                            │
│  Usar fdisk:               Usar gdisk:                     │
│  - Discos MBR              - Discos GPT                    │
│  - Cuando no importa       - Conversión MBR↔GPT            │
│    el tipo de tabla        - Funciones GPT avanzadas        │
│  - Labs rápidos            - Producción moderna             │
│                                                            │
│  fdisk moderno (util-linux 2.26+) soporta GPT también,    │
│  así que para la mayoría de tareas fdisk es suficiente.    │
└────────────────────────────────────────────────────────────┘
```

Ambas son herramientas **interactivas**: entras en un prompt, ejecutas comandos
y al final escribes los cambios con `w`. **Nada se escribe a disco hasta que
usas `w`**. Puedes salir con `q` sin guardar en cualquier momento.

---

## 2. fdisk: particionamiento MBR

### 2.1. Entrar al modo interactivo

```bash
sudo fdisk /dev/vdb
```

```
Welcome to fdisk (util-linux 2.38).
Changes will remain in memory only, until you decide to write them.

Command (m for help):
```

### 2.2. Comandos principales

| Comando | Acción |
|---------|--------|
| `m` | Mostrar ayuda (lista de comandos) |
| `p` | Imprimir tabla de particiones actual |
| `n` | Crear nueva partición |
| `d` | Eliminar una partición |
| `t` | Cambiar tipo de partición |
| `a` | Marcar/desmarcar partición como bootable |
| `l` | Listar tipos de partición conocidos |
| `w` | Escribir cambios a disco y salir |
| `q` | Salir sin guardar cambios |
| `o` | Crear nueva tabla MBR (borra todo) |
| `g` | Crear nueva tabla GPT (borra todo) |

### 2.3. Crear tabla MBR y una partición

Ejemplo paso a paso en un disco vacío de 2G:

```bash
sudo fdisk /dev/vdb
```

```
Command: o                          ← crear tabla MBR
Created a new DOS disklabel with disk identifier 0xabcdef12.

Command: n                          ← nueva partición
Partition type
   p   primary (0 primary, 0 extended, 4 free)
   e   extended (container for logical partitions)
Select (default p): p               ← primaria
Partition number (1-4, default 1): 1
First sector (2048-4194303, default 2048):    ← Enter (acepta default)
Last sector ... (default 4194303): +1G        ← 1 GiB de tamaño

Created a new partition 1 of type 'Linux' and of size 1 GiB.

Command: p                          ← verificar
Device     Boot Start     End Sectors  Size Id Type
/dev/vdb1        2048 2099199 2097152    1G 83 Linux

Command: w                          ← escribir a disco
The partition table has been altered.
Calling ioctl() to re-read partition table.
Syncing disks.
```

### 2.4. Especificar tamaño de partición

El prompt `Last sector` acepta varios formatos:

| Formato | Ejemplo | Significado |
|---------|---------|-------------|
| Número de sector | `2099199` | Sector final exacto |
| `+SIZE` | `+1G` | 1 GiB desde el inicio |
| `+SIZE` | `+500M` | 500 MiB desde el inicio |
| `+SIZE` | `+100K` | 100 KiB desde el inicio |
| Default | Enter | Usar todo el espacio restante |

**Recomendación**: usa `+SIZE` para tamaños legibles. Reserva Enter (todo el
resto) para la última partición.

### 2.5. Múltiples particiones MBR

Crear 3 primarias + 1 extendida con 2 lógicas:

```
Command: o                          ← nueva tabla MBR

Command: n → p → 1 → Enter → +500M  ← primaria 1: 500M
Command: n → p → 2 → Enter → +500M  ← primaria 2: 500M
Command: n → p → 3 → Enter → +500M  ← primaria 3: 500M
Command: n → e → Enter → Enter      ← extendida: todo el resto

Command: n → Enter → +256M          ← lógica 5: 256M (auto)
Command: n → Enter → Enter          ← lógica 6: el resto (auto)

Command: p
Device     Boot   Start     End Sectors  Size Id Type
/dev/vdb1          2048 1026047 1024000  500M 83 Linux
/dev/vdb2       1026048 2050047 1024000  500M 83 Linux
/dev/vdb3       2050048 3074047 1024000  500M 83 Linux
/dev/vdb4       3074048 4194303 1120256  547M  5 Extended
/dev/vdb5       3076096 3600383  524288  256M 83 Linux
/dev/vdb6       3602432 4194303  591872  289M 83 Linux

Command: w
```

Observa: después de crear la extendida, `fdisk` ya no pregunta `p/e` — solo
crea particiones lógicas automáticamente (empezando en 5).

### 2.6. Eliminar particiones

```
Command: d
Partition number (1-6, default 6): 3   ← eliminar partición 3

Command: p                             ← verificar que se borró
Command: w                             ← guardar
```

Eliminar una partición no borra los datos — solo elimina la entrada de la tabla.
Los datos siguen en el disco hasta que se sobrescriben.

---

## 3. fdisk: particionamiento GPT

Las versiones modernas de `fdisk` (util-linux 2.26+, incluida en Debian 12 y
AlmaLinux 9) soportan GPT.

### 3.1. Crear tabla GPT con fdisk

```bash
sudo fdisk /dev/vdc
```

```
Command: g                          ← crear tabla GPT
Created a new GPT disklabel (GUID: xxxxxxxx-...).

Command: n                          ← nueva partición
Partition number (1-128, default 1): 1
First sector (2048-4194270, default 2048): Enter
Last sector ... (default 4194270): +1G

Created a new partition 1 of type 'Linux filesystem' and of size 1 GiB.

Command: n → 2 → Enter → Enter      ← partición 2: todo el resto

Command: p
Disk /dev/vdc: 2 GiB
Disklabel type: gpt

Device       Start     End Sectors  Size Type
/dev/vdc1     2048 2099199 2097152    1G Linux filesystem
/dev/vdc2  2099200 4194270 2095071 1023M Linux filesystem

Command: w
```

Diferencias con MBR:
- No pregunta primaria/extendida (no existen en GPT)
- Acepta hasta 128 particiones
- Tipo por defecto es "Linux filesystem" en vez de "Linux"

---

## 4. gdisk: particionamiento GPT nativo

### 4.1. Instalar gdisk

```bash
# Debian/Ubuntu
sudo apt install gdisk

# AlmaLinux/RHEL/Fedora
sudo dnf install gdisk
```

### 4.2. Interfaz de gdisk

`gdisk` es casi idéntico a `fdisk` pero diseñado específicamente para GPT:

```bash
sudo gdisk /dev/vdd
```

```
GPT fdisk (gdisk) version 1.0.9

Partition table scan:
  MBR: not present
  BSD: not present
  APM: not present
  GPT: not present

Creating new GPT entries in memory.

Command (? for help):
```

### 4.3. Comandos de gdisk

| Comando | Acción |
|---------|--------|
| `?` | Ayuda (equivalente a `m` en fdisk) |
| `p` | Imprimir tabla |
| `n` | Nueva partición |
| `d` | Eliminar partición |
| `t` | Cambiar tipo |
| `c` | Cambiar nombre de partición (GPT feature) |
| `i` | Información detallada de una partición |
| `l` | Listar tipos de partición |
| `w` | Escribir y salir |
| `q` | Salir sin guardar |
| `o` | Crear nueva tabla GPT |
| `v` | Verificar integridad de la tabla |
| `r` | Recovery/transformation menu |
| `x` | Expert menu |

### 4.4. Crear particiones con gdisk

```bash
sudo gdisk /dev/vdd
```

```
Command: o                          ← nueva tabla GPT
This option deletes all partitions and creates a new protective MBR.
Proceed? (Y/N): Y

Command: n                          ← nueva partición
Partition number (1-128, default 1): 1
First sector (2048-4194270, default = 2048): Enter
Last sector ... (default = 4194270): +1G
Hex code or GUID (L to show codes, Enter = 8300): Enter

Command: n
Partition number: 2
First sector: Enter
Last sector: Enter                   ← todo el resto
Hex code: 8200                       ← Linux swap

Command: p
Number  Start (sector)    End (sector)  Size       Code  Name
   1            2048         2099199   1024.0 MiB  8300  Linux filesystem
   2         2099200         4194270   1023.0 MiB  8200  Linux swap

Command: w
Do you want to proceed? (Y/N): Y
```

### 4.5. Nombres de partición (GPT exclusivo)

GPT permite asignar un nombre legible a cada partición:

```
Command: c                          ← cambiar nombre
Partition number (1-2): 1
Enter name: root-partition

Command: c
Partition number: 2
Enter name: swap-partition

Command: i                          ← información de partición 1
Partition number: 1
Partition GUID code: 0FC63DAF-...
Partition unique GUID: xxxxxxxx-...
First sector: 2048
Last sector: 2099199
Partition size: 1024.0 MiB
Attribute flags: 0000000000000000
Partition name: 'root-partition'     ← nombre visible
```

Estos nombres se pueden ver con `lsblk -o NAME,PARTLABEL` y son útiles para
documentación.

### 4.6. Verificar integridad

```
Command: v                          ← verificar
No problems found. 0 free sectors (0 bytes) available in 0 segments.
```

`gdisk` verifica los CRC32 del header y la tabla, y comprueba que no haya
particiones solapadas.

### 4.7. Conversión MBR a GPT

`gdisk` puede convertir un disco MBR a GPT preservando las particiones:

```bash
sudo gdisk /dev/vdb    # disco con MBR
```

```
Partition table scan:
  MBR: MBR only              ← detecta MBR existente

# gdisk automáticamente convierte la tabla MBR a GPT en memoria
# Las particiones se preservan

Command: p                   ← ver la conversión
# Muestra las particiones MBR traducidas a GPT

Command: w                   ← escribir la tabla GPT
# CUIDADO: esto reemplaza la MBR con GPT
# Los datos de las particiones NO se tocan
```

**Precaución**: la conversión funciona bien, pero en discos con datos siempre
haz backup primero. En discos de lab vacíos, no hay riesgo.

---

## 5. Alineación de particiones

### 5.1. Por qué importa la alineación

Los discos modernos (SSDs y HDDs con Advanced Format) trabajan internamente
con bloques de 4096 bytes. Si una partición empieza en un sector que no está
alineado a 4K, cada operación de I/O cruza dos bloques físicos:

```
Mal alineada (sector 63):
┌──────┬──────┬──────┬──────┐  bloques físicos (4096 bytes)
│      │XXXXXX│XXXXXX│      │
└──────┴──────┴──────┴──────┘
       ↑ escritura cruza 2 bloques → doble I/O

Bien alineada (sector 2048):
┌──────┬──────┬──────┬──────┐  bloques físicos (4096 bytes)
│      │XXXXXX│XXXXXX│      │
└──────┴──────┴──────┴──────┘
       ↑ escritura cae en 1 bloque → I/O óptimo
```

### 5.2. La regla: empezar en sector 2048

Sector 2048 × 512 bytes = 1 MiB. Esto es múltiplo de 4096, así que siempre está
alineado:

```bash
# fdisk y gdisk usan sector 2048 como default — SIEMPRE acepta el default
First sector (2048-4194303, default 2048): Enter    ← correcto
```

**Nunca cambies el sector de inicio a menos que sepas exactamente lo que haces**.

### 5.3. Verificar alineación

```bash
# Con fdisk: ver la columna Start
sudo fdisk -l /dev/vdb
# /dev/vdb1  2048  ...    ← 2048 es correcto

# Con parted: verificación explícita
sudo parted /dev/vdb align-check optimal 1
# 1 aligned     ← bien
# 1 not aligned ← mal
```

### 5.4. Alineación en VMs

En discos virtuales (qcow2 sobre virtio), la alineación tiene menos impacto
porque no hay hardware físico con bloques de 4K. Pero:

- Los defaults de fdisk/gdisk ya alinean correctamente
- Es buena práctica para cuando trabajes con discos reales
- Algunos tests de certificación (LPIC, RHCSA) verifican alineación

---

## 6. Tipos de partición

### 6.1. Cambiar el tipo en fdisk

```
Command: t
Partition number (1-6, default 6): 2
Hex code or alias (type L to list all): 82

Changed type of partition 'Linux' to 'Linux swap / Solaris'.
```

### 6.2. Cambiar el tipo en gdisk

```
Command: t
Partition number (1-2): 2
Hex code or GUID (L to show codes, Enter = 8300): 8200

Changed type of partition 'Linux filesystem' to 'Linux swap'.
```

### 6.3. Tipos más usados

**fdisk (MBR)**:

| Hex | Tipo | Uso |
|-----|------|-----|
| `83` | Linux | Partición Linux estándar (ext4, xfs, etc.) |
| `82` | Linux swap | Espacio de intercambio |
| `8e` | Linux LVM | Physical Volume para LVM |
| `fd` | Linux RAID autodetect | Partición para mdadm |
| `ef` | EFI System | Partición EFI (FAT32, solo UEFI) |
| `5` | Extended | Contenedor para particiones lógicas |

**gdisk (GPT)**:

| Code | Tipo | Uso |
|------|------|-----|
| `8300` | Linux filesystem | Partición Linux estándar |
| `8200` | Linux swap | Espacio de intercambio |
| `8e00` | Linux LVM | Physical Volume para LVM |
| `fd00` | Linux RAID | Partición para mdadm |
| `ef00` | EFI System | Partición EFI |
| `ef02` | BIOS boot partition | Para GRUB en BIOS+GPT |

### 6.4. ¿Importa el tipo de partición?

El tipo de partición es una **etiqueta informativa**. No impide que hagas algo
diferente en ella:

```bash
# Puedes crear ext4 en una partición tipo "swap" — funciona
# Puedes crear swap en una partición tipo "Linux" — funciona
# El kernel no verifica el tipo al montar

# PERO: algunas herramientas usan el tipo como indicador:
# - mdadm busca particiones tipo "fd" para auto-ensamblaje
# - LVM busca particiones tipo "8e" para auto-detección
# - El instalador del SO lee los tipos para ofrecer opciones

# Buena práctica: usar el tipo correcto siempre
```

---

## 7. Lab completo: particionar un disco

### 7.1. Escenario MBR con fdisk

Particionar `vdb` (2G) con MBR: 1G para datos + 512M para swap + resto para LVM.

```bash
sudo fdisk /dev/vdb
```

```
Command: o                          ← nueva tabla MBR

Command: n                          ← partición 1: datos
  p → 1 → Enter → +1G

Command: n                          ← partición 2: swap
  p → 2 → Enter → +512M

Command: t                          ← cambiar tipo de part. 2
  2 → 82                            ← Linux swap

Command: n                          ← partición 3: LVM
  p → 3 → Enter → Enter             ← todo el resto

Command: t                          ← cambiar tipo de part. 3
  3 → 8e                            ← Linux LVM

Command: p                          ← verificar
Device     Boot   Start     End Sectors  Size Id Type
/dev/vdb1          2048 2099199 2097152    1G 83 Linux
/dev/vdb2       2099200 3147775 1048576  512M 82 Linux swap
/dev/vdb3       3147776 4194303 1046528  511M 8e Linux LVM

Command: w                          ← escribir
```

Verificar:

```bash
lsblk /dev/vdb
```

```
NAME   MAJ:MIN RM  SIZE RO TYPE MOUNTPOINTS
vdb      252:16   0    2G  0 disk
├─vdb1   252:17   0    1G  0 part
├─vdb2   252:18   0  512M  0 part
└─vdb3   252:19   0  511M  0 part
```

### 7.2. Escenario GPT con gdisk

Particionar `vdc` (2G) con GPT: 1G para datos + 512M para swap + resto para datos.

```bash
sudo gdisk /dev/vdc
```

```
Command: o → Y                      ← nueva tabla GPT

Command: n                          ← partición 1
  1 → Enter → +1G → 8300            ← Linux filesystem

Command: n                          ← partición 2
  2 → Enter → +512M → 8200          ← Linux swap

Command: n                          ← partición 3
  3 → Enter → Enter → 8300          ← todo el resto

Command: c → 1 → data-partition     ← nombrar part. 1
Command: c → 2 → swap-partition     ← nombrar part. 2
Command: c → 3 → extra-data         ← nombrar part. 3

Command: p
Number  Start (sector)    End (sector)  Size       Code  Name
   1            2048         2099199   1024.0 MiB  8300  data-partition
   2         2099200         3147775   512.0 MiB   8200  swap-partition
   3         3147776         4194270   511.0 MiB   8300  extra-data

Command: w → Y
```

### 7.3. Limpiar un disco particionado

Para dejar el disco vacío (sin tabla de particiones):

```bash
# Opción 1: wipefs (solo borra firmas/magic bytes)
sudo wipefs -a /dev/vdb

# Opción 2: dd (sobrescribir los primeros sectores)
sudo dd if=/dev/zero of=/dev/vdb bs=1M count=1

# Opción 3: sgdisk (limpiar tabla GPT)
sudo sgdisk --zap-all /dev/vdc
```

Después de limpiar, `fdisk -l` mostrará el disco sin tabla de particiones.

---

## 8. Errores comunes

### Error 1: olvidar `w` para guardar

```bash
# ✗ Crear particiones y salir con 'q'
Command: n → ...
Command: q                    ← NADA se guardó

# ✓ Usar 'w' para escribir los cambios
Command: w                    ← cambios escritos a disco
```

Esto es en realidad una característica de seguridad: puedes experimentar
libremente y solo guardar cuando estés satisfecho.

### Error 2: no informar al kernel de los cambios

```bash
# Después de particionar, el kernel puede no detectar las nuevas particiones
# Especialmente si el disco ya tenía particiones montadas

# Verificar: si lsblk no muestra las particiones nuevas
lsblk /dev/vdb    # solo muestra "vdb" sin hijos

# Solución:
sudo partprobe /dev/vdb    # informar al kernel
# o
sudo fdisk -l /dev/vdb     # a veces basta con releer
```

`partprobe` (del paquete `parted`) fuerza al kernel a releer la tabla de
particiones. En discos nuevos sin particiones previas montadas, generalmente
no es necesario.

### Error 3: particionar el disco equivocado

```bash
# ✗ ¡SIEMPRE verificar qué disco estás particionando!
sudo fdisk /dev/vda          # ← ¡este es el disco del SO!
# Si borras la tabla de particiones de vda, pierdes el sistema

# ✓ Verificar antes de actuar
lsblk                         # ver qué es cada disco
sudo fdisk /dev/vdb           # ← disco extra (vacío, seguro)
```

### Error 4: no dejar espacio para la extendida (MBR)

```bash
# ✗ Crear 4 primarias y después querer más
Command: n → p → 1 → +500M
Command: n → p → 2 → +500M
Command: n → p → 3 → +500M
Command: n → p → 4 → Enter     ← primaria 4 usa todo el resto
# Ya no puedes crear más particiones

# ✓ Si necesitas >4, reservar una primaria como extendida
Command: n → p → 1 → +500M
Command: n → p → 2 → +500M
Command: n → p → 3 → +500M
Command: n → e → Enter → Enter  ← extendida con todo el resto
Command: n → +256M              ← lógica 5
Command: n → Enter              ← lógica 6 (el resto)
```

### Error 5: cambiar de sector de inicio predeterminado

```bash
# ✗ Cambiar el primer sector manualmente
First sector (2048-4194303, default 2048): 63
# Desalinea la partición — I/O subóptimo

# ✓ SIEMPRE aceptar el default
First sector (2048-4194303, default 2048): Enter    ← correcto
```

---

## 9. Cheatsheet

```bash
# ── fdisk (MBR y GPT) ───────────────────────────────────────────
sudo fdisk /dev/vdX              # modo interactivo
# Dentro:
#   o     crear tabla MBR
#   g     crear tabla GPT
#   n     nueva partición
#   d     eliminar partición
#   t     cambiar tipo (83, 82, 8e, fd, ef)
#   a     marcar bootable (solo MBR)
#   p     imprimir tabla
#   w     escribir y salir
#   q     salir sin guardar

# ── gdisk (solo GPT) ────────────────────────────────────────────
sudo gdisk /dev/vdX              # modo interactivo
# Dentro:
#   o     crear tabla GPT
#   n     nueva partición
#   d     eliminar partición
#   t     cambiar tipo (8300, 8200, 8e00, fd00, ef00, ef02)
#   c     nombrar partición
#   i     info detallada de partición
#   v     verificar integridad
#   p     imprimir tabla
#   w     escribir y salir
#   q     salir sin guardar

# ── Tamaños en el prompt Last sector ────────────────────────────
# +1G    +500M    +100K    Enter (todo el resto)

# ── Verificar después de particionar ────────────────────────────
lsblk /dev/vdX                   # ver particiones
sudo fdisk -l /dev/vdX           # tabla detallada
sudo partprobe /dev/vdX          # forzar kernel a releer

# ── Limpiar un disco ────────────────────────────────────────────
sudo wipefs -a /dev/vdX          # borrar firmas
sudo sgdisk --zap-all /dev/vdX   # borrar tabla GPT
sudo dd if=/dev/zero of=/dev/vdX bs=1M count=1   # zeros al inicio

# ── Alineación ──────────────────────────────────────────────────
# SIEMPRE aceptar sector de inicio default (2048)
# Verificar: sudo parted /dev/vdX align-check optimal N

# ── Tipos comunes ────────────────────────────────────────────────
# MBR:  83=Linux  82=swap  8e=LVM  fd=RAID  ef=EFI  5=Extended
# GPT:  8300=Linux  8200=swap  8e00=LVM  fd00=RAID  ef00=EFI
```

---

## 10. Ejercicios

### Ejercicio 1: particionar con fdisk (MBR)

1. Crea una tabla MBR en `/dev/vdb` con `fdisk`
2. Crea 3 particiones:
   - Partición 1: 500M, tipo Linux (83)
   - Partición 2: 500M, tipo Linux swap (82)
   - Partición 3: resto del disco, tipo Linux LVM (8e)
3. Verifica con `p` antes de escribir
4. Escribe con `w`
5. Verifica con `lsblk /dev/vdb` y `sudo fdisk -l /dev/vdb`
6. Limpia con `sudo wipefs -a /dev/vdb`

**Pregunta de reflexión**: después de particionar, las particiones aparecen en
`lsblk` pero no tienen filesystem. ¿Puedes montar `/dev/vdb1` sin crear un
filesystem primero? ¿Qué error obtendrías?

### Ejercicio 2: particionar con gdisk (GPT)

1. Crea una tabla GPT en `/dev/vdc` con `gdisk`
2. Crea 3 particiones:
   - Partición 1: 800M, tipo Linux filesystem (8300), nombre "data"
   - Partición 2: 200M, tipo Linux swap (8200), nombre "swap"
   - Partición 3: resto, tipo Linux filesystem (8300), nombre "extra"
3. Verifica la integridad con `v`
4. Usa `i` para ver los detalles de la partición 1 — anota el GUID
5. Escribe con `w`
6. Verifica con `lsblk -o NAME,SIZE,PARTLABEL /dev/vdc`
7. Limpia con `sudo sgdisk --zap-all /dev/vdc`

**Pregunta de reflexión**: cada partición GPT tiene un GUID único. ¿Es este el
mismo UUID que verías con `blkid`? ¿Cuál es la diferencia entre el GUID de la
partición (PARTUUID) y el UUID del filesystem?

### Ejercicio 3: comparar MBR y GPT

1. Crea tabla MBR en `vdb` y GPT en `vdc`
2. En ambos, crea 2 particiones de tamaños iguales (500M + resto)
3. Compara las salidas de:
   - `sudo fdisk -l /dev/vdb` vs `sudo fdisk -l /dev/vdc`
   - `lsblk -o NAME,SIZE,PTTYPE /dev/vdb /dev/vdc`
4. En el disco GPT, asigna nombres a las particiones con `gdisk` → `c`
5. Verifica los nombres con `lsblk -o NAME,PARTLABEL`
6. Limpia ambos discos

**Pregunta de reflexión**: observa que en MBR la última partición termina en
sector 4194303, pero en GPT termina en 4194270 (33 sectores menos). ¿A qué
se debe esa diferencia? Pista: piensa en dónde almacena GPT su backup.
