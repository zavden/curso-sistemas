# LVM — Conceptos

## Índice

1. [El problema que resuelve LVM](#el-problema-que-resuelve-lvm)
2. [Arquitectura de tres capas](#arquitectura-de-tres-capas)
3. [Physical Volume (PV)](#physical-volume-pv)
4. [Volume Group (VG)](#volume-group-vg)
5. [Logical Volume (LV)](#logical-volume-lv)
6. [Physical Extent (PE) y Logical Extent (LE)](#physical-extent-pe-y-logical-extent-le)
7. [Mapa completo: del disco al filesystem](#mapa-completo-del-disco-al-filesystem)
8. [Device Mapper](#device-mapper)
9. [Tipos de Logical Volumes](#tipos-de-logical-volumes)
10. [Nomenclatura y paths](#nomenclatura-y-paths)
11. [Metadatos LVM](#metadatos-lvm)
12. [LVM vs particiones tradicionales](#lvm-vs-particiones-tradicionales)
13. [Errores comunes](#errores-comunes)
14. [Cheatsheet](#cheatsheet)
15. [Ejercicios](#ejercicios)

---

## El problema que resuelve LVM

Con particiones tradicionales, el tamaño de cada partición queda fijo al crearla. Si `/home` se llena pero `/var` tiene espacio libre, no hay forma sencilla de transferir ese espacio.

```
Particiones tradicionales:
┌──────────────────────────────────────────────────┐
│                  /dev/sda                         │
├────────────┬───────────────┬─────────────────────┤
│  sda1      │  sda2         │  sda3               │
│  /boot     │  /            │  /home               │
│  1 GiB     │  20 GiB       │  79 GiB              │
│            │               │  ← ¡LLENO!           │
│  (fijo)    │  (30% libre)  │  (no puedo crecer)   │
└────────────┴───────────────┴─────────────────────┘

Para redimensionar: desmontar, borrar partición, recrear más grande,
redimensionar filesystem. Riesgo alto. Downtime obligatorio.
```

LVM introduce una **capa de abstracción** entre los discos físicos y los filesystems:

```
Con LVM:
┌──────────────────────────────────────────────────┐
│  /dev/sda          /dev/sdb (disco nuevo)        │
│  ┌────────────┐    ┌────────────┐                │
│  │  PV        │    │  PV        │                │
│  └─────┬──────┘    └─────┬──────┘                │
│        │                 │                        │
│        └────────┬────────┘                        │
│                 ▼                                  │
│        ┌────────────────┐                         │
│        │  VG (pool)     │  ← espacio combinado    │
│        └──┬──────┬──────┘                         │
│           ▼      ▼                                │
│        ┌─────┐ ┌─────┐                            │
│        │ LV  │ │ LV  │  ← tamaño flexible        │
│        │ /   │ │/home│                            │
│        └─────┘ └─────┘                            │
└──────────────────────────────────────────────────┘

Para crecer /home: lvextend + resize2fs. Online. Sin desmontar.
Para añadir espacio: conectar nuevo disco, añadir al VG.
```

---

## Arquitectura de tres capas

LVM organiza el almacenamiento en tres niveles:

```
┌─────────────────────────────────────────────────────────┐
│                                                         │
│   Capa 3: Logical Volume (LV)                           │
│   ┌──────────┐  ┌──────────┐  ┌──────────┐             │
│   │ lv_root  │  │ lv_home  │  │ lv_swap  │             │
│   │ 20 GiB   │  │ 50 GiB   │  │  4 GiB   │             │
│   │ ext4     │  │ xfs      │  │ swap     │             │
│   └────┬─────┘  └────┬─────┘  └────┬─────┘             │
│        │             │             │                    │
│   ─────┴─────────────┴─────────────┴──── ← asignación  │
│                                            de extents   │
│   Capa 2: Volume Group (VG)                             │
│   ┌─────────────────────────────────────┐               │
│   │          vg0  (74 GiB total)        │               │
│   │   Pool de Physical Extents libres   │               │
│   └───────────┬──────────┬──────────────┘               │
│               │          │                              │
│   Capa 1: Physical Volume (PV)                          │
│   ┌───────────┴──┐  ┌───┴───────────┐                  │
│   │  /dev/sda2   │  │  /dev/sdb1    │                  │
│   │  (50 GiB)    │  │  (50 GiB)    │                  │
│   └──────────────┘  └──────────────┘                   │
│                                                         │
│   Hardware: discos, particiones, o dispositivos de bloque│
└─────────────────────────────────────────────────────────┘
```

La lectura es **de abajo hacia arriba**:
1. Los **discos/particiones** se inicializan como **Physical Volumes** (PV)
2. Los PVs se agrupan en un **Volume Group** (VG) — un pool de almacenamiento
3. Del VG se asignan porciones llamadas **Logical Volumes** (LV) — estos son los dispositivos donde creas filesystems

---

## Physical Volume (PV)

Un PV es un disco o partición que ha sido inicializado para uso por LVM. Al crear un PV, LVM escribe una pequeña cabecera de metadatos al inicio del dispositivo.

### Qué puede ser un PV

```
┌────────────────────────────────────────────┐
│  Dispositivos válidos como PV:             │
│                                            │
│  ● Partición:     /dev/sda2, /dev/vdb1    │
│  ● Disco completo: /dev/sdb, /dev/vdc     │
│  ● Loop device:   /dev/loop0              │
│  ● RAID (md):     /dev/md0                │
│  ● Multipath:     /dev/mpath0             │
│                                            │
│  NO usar como PV:                          │
│  ✗ /dev/sda1 si es /boot (no usar LVM     │
│    para boot en BIOS legacy)              │
│  ✗ Un dispositivo ya montado con datos     │
│    (se sobreescribirá)                     │
└────────────────────────────────────────────┘
```

### Estructura interna de un PV

```
┌──────────────────────────────────────────────┐
│              Physical Volume                  │
│                                              │
│  ┌──────────┬───────────────────────────┐    │
│  │ PV Label │     Physical Extents      │    │
│  │ + Meta   │  ┌────┬────┬────┬────┬──  │    │
│  │ datos    │  │PE 0│PE 1│PE 2│PE 3│... │    │
│  │ LVM      │  └────┴────┴────┴────┴──  │    │
│  └──────────┴───────────────────────────┘    │
│                                              │
│  PV Label: UUID del PV, tamaño, offset       │
│  PEs: bloques de tamaño fijo (default 4 MiB) │
└──────────────────────────────────────────────┘
```

### Tipo de partición para LVM

Cuando usas una **partición** como PV (no un disco completo), es buena práctica marcarla con el tipo correcto:

| Tabla de particiones | Tipo | Código |
|---------------------|------|--------|
| MBR | Linux LVM | `8e` |
| GPT | Linux LVM | `E6D6D379-F507-44C2-A23C-238F2A3DF928` (o alias `lvm` en fdisk/gdisk) |

Esto no es obligatorio — LVM funciona sin el tipo correcto — pero ayuda a otros administradores y herramientas a identificar el propósito de la partición.

### Comandos PV (vista previa)

```bash
pvcreate /dev/vdb1       # Inicializar como PV
pvs                      # Listar PVs (resumen)
pvdisplay /dev/vdb1      # Detalle de un PV
pvremove /dev/vdb1       # Eliminar label de PV
```

---

## Volume Group (VG)

Un VG es un **pool de almacenamiento** formado por uno o más PVs. Todo el espacio de los PVs se combina en un solo pool del que se asignan LVs.

### Concepto

```
                PV1 (50 GiB)         PV2 (100 GiB)
              ┌──────────────┐    ┌──────────────────────────┐
              │ PE PE PE ... │    │ PE PE PE PE PE PE PE ... │
              └──────┬───────┘    └────────────┬─────────────┘
                     │                         │
                     └────────────┬─────────────┘
                                  ▼
                    ┌────────────────────────┐
                    │    VG: vg_data          │
                    │    150 GiB total        │
                    │    12800 PEs (4 MiB c/u)│
                    │                        │
                    │    Asignados:   74 GiB  │
                    │    Libres:      76 GiB  │
                    └────────────────────────┘
```

Propiedades clave del VG:

- **Nombre**: elegido por el administrador (ej: `vg0`, `vg_data`, `rhel`)
- **PE Size**: tamaño de los Physical Extents — se define al crear el VG y aplica a todos los PVs del grupo. Default: 4 MiB
- **Extensible**: se pueden añadir PVs a un VG existente para aumentar el pool
- **Reducible**: se pueden retirar PVs de un VG (si sus PEs no están en uso)

### Comandos VG (vista previa)

```bash
vgcreate vg0 /dev/vdb1 /dev/vdc1   # Crear VG con 2 PVs
vgs                                  # Listar VGs (resumen)
vgdisplay vg0                        # Detalle de un VG
vgextend vg0 /dev/vdd1              # Añadir PV al VG
vgreduce vg0 /dev/vdc1              # Retirar PV del VG
vgremove vg0                         # Eliminar VG
```

---

## Logical Volume (LV)

Un LV es una **porción del VG** asignada para un propósito específico. Es el equivalente a una partición, pero flexible: se puede redimensionar, mover, hacer snapshot, etc.

```
              Volume Group: vg0 (150 GiB)
    ┌──────────────────────────────────────────────┐
    │                                              │
    │  ┌──────────┐  ┌──────────┐  ┌──────────┐   │
    │  │ lv_root  │  │ lv_home  │  │ lv_swap  │   │
    │  │ 20 GiB   │  │ 50 GiB   │  │  4 GiB   │   │
    │  └──────────┘  └──────────┘  └──────────┘   │
    │                                              │
    │  Espacio libre: 76 GiB                       │
    │  (disponible para nuevos LVs o para crecer)  │
    │                                              │
    └──────────────────────────────────────────────┘
```

El LV aparece como un dispositivo de bloque en `/dev/` y se usa exactamente como una partición:

```bash
# Crear filesystem en un LV
mkfs.ext4 /dev/vg0/lv_home

# Montar
mount /dev/vg0/lv_home /home

# En fstab
/dev/mapper/vg0-lv_home   /home   ext4   defaults   0   2
```

### Comandos LV (vista previa)

```bash
lvcreate -L 20G -n lv_root vg0     # Crear LV de 20 GiB
lvs                                  # Listar LVs (resumen)
lvdisplay /dev/vg0/lv_root          # Detalle de un LV
lvextend -L +10G /dev/vg0/lv_root   # Crecer 10 GiB
lvreduce -L -5G /dev/vg0/lv_root    # Reducir 5 GiB
lvremove /dev/vg0/lv_root           # Eliminar LV
```

---

## Physical Extent (PE) y Logical Extent (LE)

El PE es la **unidad mínima de asignación** en LVM. Todo se mide en PEs.

### Concepto

```
PE Size = 4 MiB (default)

Un PV de 1 GiB = 256 PEs
Un LV de 20 GiB = 5120 PEs

┌─────────────────────────────────────────────────┐
│  Physical Volume: /dev/vdb1 (1 GiB = 256 PEs)  │
│                                                 │
│  ┌────┬────┬────┬────┬────┬────┬────┬────┬───   │
│  │ 0  │ 1  │ 2  │ 3  │ 4  │ 5  │ 6  │ 7  │...  │
│  │used│used│used│free│free│used│used│free│     │
│  └────┴────┴────┴────┴────┴────┴────┴────┴───   │
│                                                 │
│  used = asignado a un LV                        │
│  free = disponible para asignar                 │
└─────────────────────────────────────────────────┘
```

### PE vs LE

- **PE** (Physical Extent): bloque en el PV (disco físico)
- **LE** (Logical Extent): bloque en el LV (volumen lógico)

Un LE se **mapea** a un PE. En un LV lineal (el tipo por defecto), el LE 0 del LV apunta al PE X del PV, el LE 1 al PE X+1, etc.

```
  Logical Volume: lv_home
  LE:  [  0  ][  1  ][  2  ][  3  ][  4  ][  5  ]
        │       │       │       │       │       │
        ▼       ▼       ▼       ▼       ▼       ▼
  PV1: [PE 5 ][PE 6 ][PE 7 ]
  PV2:                         [PE 0 ][PE 1 ][PE 2 ]

  Un LV puede extenderse por múltiples PVs — transparente
  para el filesystem
```

### Tamaño del PE

| PE Size | PEs por GiB | Máximo VG teórico | Cuándo usar |
|---------|-------------|-------------------|-------------|
| 4 MiB (default) | 256 | ~16 TiB | Casi siempre |
| 8 MiB | 128 | ~32 TiB | VGs grandes |
| 16 MiB | 64 | ~64 TiB | VGs muy grandes |
| 32 MiB | 32 | ~128 TiB | Almacenamiento masivo |

El PE size se define al crear el VG y **no se puede cambiar** después:

```bash
# VG con PE de 4 MiB (default)
vgcreate vg0 /dev/vdb1

# VG con PE de 16 MiB
vgcreate -s 16M vg_big /dev/vdb1
```

> **Consecuencia práctica**: un LV no puede ser más pequeño que un PE. Con PE de 4 MiB, el LV mínimo es 4 MiB. Para labs esto no importa, pero es bueno entenderlo.

---

## Mapa completo: del disco al filesystem

Vista end-to-end de cómo se relacionan todos los componentes:

```
┌─────────────────────────────────────────────────────────────┐
│                    VISTA COMPLETA                            │
│                                                             │
│  HARDWARE                                                   │
│  ┌──────────┐  ┌──────────┐                                 │
│  │ /dev/vdb │  │ /dev/vdc │    discos físicos (o virtuales) │
│  └────┬─────┘  └────┬─────┘                                 │
│       │              │                                      │
│  PARTICIONES                                                │
│  ┌────┴─────┐  ┌────┴─────┐                                │
│  │ /dev/    │  │ /dev/    │    particiones tipo 8e/lvm      │
│  │ vdb1     │  │ vdc1     │                                 │
│  └────┬─────┘  └────┬─────┘                                │
│       │              │                                      │
│  PV (pvcreate)       │                                      │
│  ┌────┴─────┐  ┌────┴─────┐                                │
│  │ PV       │  │ PV       │    cabecera LVM + PEs           │
│  │ uuid-A   │  │ uuid-B   │                                 │
│  └────┬─────┘  └────┬─────┘                                │
│       │              │                                      │
│       └──────┬───────┘                                      │
│              │                                              │
│  VG (vgcreate)                                              │
│  ┌───────────┴───────────┐                                  │
│  │       vg0             │    pool de PEs                   │
│  │   Total: 200 GiB     │                                  │
│  └───┬───────┬───────┬───┘                                  │
│      │       │       │                                      │
│  LV (lvcreate)                                              │
│  ┌───┴───┐ ┌─┴─────┐ ┌┴──────┐                             │
│  │lv_root│ │lv_home│ │lv_var │  volúmenes lógicos          │
│  │ 30G   │ │ 100G  │ │ 50G   │                             │
│  └───┬───┘ └───┬───┘ └───┬───┘                             │
│      │         │         │                                  │
│  FILESYSTEM (mkfs)                                          │
│  ┌───┴───┐ ┌───┴───┐ ┌───┴───┐                             │
│  │ xfs   │ │ ext4  │ │ xfs   │  formato del filesystem     │
│  └───┬───┘ └───┬───┘ └───┬───┘                             │
│      │         │         │                                  │
│  MOUNT                                                      │
│    /          /home      /var      puntos de montaje        │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

---

## Device Mapper

LVM no implementa el mapeo de dispositivos directamente — usa el subsistema **Device Mapper** (dm) del kernel Linux.

```
┌─────────────────────────────────────────────┐
│              Espacio de usuario              │
│                                             │
│   LVM tools ──► Device Mapper library       │
│   (lvcreate,    (libdevmapper)              │
│    lvextend)                                │
│                     │                       │
│─────────────────────┼───────────────────────│
│                     │   Espacio de kernel    │
│                     ▼                       │
│              Device Mapper (dm)              │
│              Crea /dev/dm-N                  │
│                     │                       │
│                     ▼                       │
│              Discos físicos                  │
└─────────────────────────────────────────────┘
```

Device Mapper crea dispositivos virtuales en `/dev/dm-N`. Los **symlinks** en `/dev/mapper/` y `/dev/vg0/` apuntan a estos dispositivos dm:

```bash
ls -l /dev/mapper/vg0-lv_root
# lrwxrwxrwx 1 root root 7 ... /dev/mapper/vg0-lv_root -> ../dm-0

ls -l /dev/vg0/lv_root
# lrwxrwxrwx 1 root root 7 ... /dev/vg0/lv_root -> ../dm-0

# Ambos apuntan al mismo dispositivo real: /dev/dm-0
```

Device Mapper también se usa para otras tecnologías:
- **dm-crypt** (LUKS): cifrado de disco
- **dm-multipath**: múltiples caminos al mismo disco
- **dm-thin**: thin provisioning

---

## Tipos de Logical Volumes

### Linear (default)

Los PEs se asignan secuencialmente. Si el LV abarca múltiples PVs, los PEs del segundo PV continúan después de los del primero.

```
LV lineal de 8 PEs:
  PV1: [PE0][PE1][PE2][PE3]
  PV2: [PE4][PE5][PE6][PE7]

  Lectura secuencial: PV1 primero, luego PV2
```

### Striped

Los PEs se alternan entre PVs, similar a RAID0. Mejora rendimiento de I/O paralelo.

```
LV striped de 8 PEs (2 PVs, stripe=2):
  PV1: [PE0][PE2][PE4][PE6]   ← PEs pares
  PV2: [PE1][PE3][PE5][PE7]   ← PEs impares

  Lectura paralela: ambos PVs simultáneamente
```

```bash
# Crear LV striped
lvcreate -L 10G -n lv_fast -i 2 vg0
#                           -i = número de stripes (PVs)
```

### Mirror

Cada PE se duplica en otro PV. Similar a RAID1 — redundancia a nivel LVM.

```bash
# Crear LV mirror
lvcreate -L 10G -n lv_safe -m 1 vg0
#                           -m 1 = 1 copia espejo
```

### Thin (thin provisioning)

El LV puede ser más grande que el espacio realmente disponible. El espacio se asigna bajo demanda conforme se escribe.

```
Thin pool: 50 GiB reales

  lv_web:  100 GiB (declarado)  →  usa 20 GiB (real)
  lv_db:   200 GiB (declarado)  →  usa 15 GiB (real)

  Total declarado: 300 GiB
  Total real:       35 GiB de 50 GiB

  ⚠ Si el uso real supera 50 GiB → problema
```

### Snapshot

Copia point-in-time de un LV usando Copy-on-Write. Se cubre en detalle en T04.

---

## Nomenclatura y paths

Un LV se puede referenciar de varias formas. Todas son equivalentes:

```
┌──────────────────────────────────────────────────────────┐
│                  Paths del LV                             │
│                                                          │
│  /dev/vg0/lv_root             ← path lógico (symlink)    │
│  /dev/mapper/vg0-lv_root      ← path de Device Mapper    │
│  /dev/dm-0                    ← dispositivo real          │
│                                                          │
│  Los tres apuntan al mismo dispositivo.                   │
│  En fstab y scripts, usar /dev/mapper/vg0-lv_root        │
│  es lo más claro y estable.                               │
└──────────────────────────────────────────────────────────┘
```

### Convenciones de nombres

| Recurso | Convención | Ejemplos |
|---------|------------|----------|
| VG | `vg0`, `vg_data`, nombre del host | `vg0`, `rhel`, `alma` |
| LV | `lv_propósito` o solo propósito | `lv_root`, `lv_home`, `root`, `swap` |
| PV | No se nombra — usa el device path | `/dev/vdb1` |

Nombre del VG en instaladores automáticos:

```bash
# RHEL/AlmaLinux usa el hostname o "rhel" / "almalinux"
/dev/mapper/rhel-root
/dev/mapper/rhel-swap

# Fedora
/dev/mapper/fedora-root

# Recomendación para labs: vg0 (simple y claro)
/dev/mapper/vg0-lv_root
```

### Guiones en los nombres

Si el nombre del VG o LV contiene un guión (`-`), Device Mapper lo **duplica** en el path:

```bash
# VG: my-vg, LV: my-lv
/dev/mapper/my--vg-my--lv
#           ^^    ^^  guiones duplicados

# Por eso es mejor evitar guiones en nombres de VG/LV
# Usar underscore: vg_data, lv_home
```

---

## Metadatos LVM

LVM almacena sus metadatos (qué PVs existen, qué VGs, qué LVs, mapping de PEs) en dos lugares:

### En los PVs

Cada PV tiene una copia de los metadatos del VG al que pertenece. Esto significa que los metadatos están distribuidos y hay redundancia automática.

```bash
# Ver metadatos raw de un PV
pvck /dev/vdb1

# Dump de metadatos
pvck --dump metadata /dev/vdb1
```

### En /etc/lvm/

```bash
# Backup automático de metadatos
ls /etc/lvm/backup/
# vg0    ← último estado conocido del VG

ls /etc/lvm/archive/
# vg0_00000-*.vg   ← histórico de cambios

# Restaurar metadatos si se corrompen
vgcfgrestore vg0
```

Los backups en `/etc/lvm/backup/` se actualizan automáticamente con cada operación LVM. El directorio `archive/` mantiene un histórico.

---

## LVM vs particiones tradicionales

```
┌─────────────────────┬──────────────────┬──────────────────┐
│ Característica      │ Particiones      │ LVM              │
├─────────────────────┼──────────────────┼──────────────────┤
│ Redimensionar       │ Complejo,        │ Fácil, online    │
│                     │ requiere umount  │ (para crecer)    │
│ Múltiples discos    │ Un FS por disco  │ Pool combinado   │
│ Snapshots           │ No               │ Sí (CoW)         │
│ Mover datos         │ dd/rsync manual  │ pvmove online    │
│ Thin provisioning   │ No               │ Sí               │
│ Complejidad         │ Simple           │ Moderada         │
│ Overhead            │ Ninguno          │ Mínimo (~1%)     │
│ Recovery            │ Directo          │ Requiere LVM     │
│ /boot               │ Recomendado      │ Posible pero     │
│                     │                  │ no recomendado*  │
│ Examen RHCSA        │ Básico           │ Obligatorio      │
└─────────────────────┴──────────────────┴──────────────────┘

* GRUB2 puede leer LVM, pero mantener /boot en partición
  simple es la práctica estándar.
```

### Cuándo usar LVM

- **Siempre** en servidores — la flexibilidad justifica la mínima complejidad
- Cuando necesitas snapshots para backups
- Cuando planeas añadir discos en el futuro
- En cualquier instalación RHEL/AlmaLinux (es el default del instalador)

### Cuándo NO usar LVM

- `/boot` y `/boot/efi` — mejor en particiones simples
- Sistemas embebidos con recursos mínimos
- Discos de un solo propósito sin expectativa de cambio

---

## Errores comunes

### 1. Confundir los niveles PV → VG → LV

```
✗ "Crear un filesystem en el PV"
   mkfs.ext4 /dev/vdb1   ← esto destruye los metadatos LVM

✓ El flujo correcto:
   pvcreate → vgcreate → lvcreate → mkfs en el LV
```

### 2. Pensar que el tamaño del LV incluye el filesystem

```
✗ "El LV es de 20 GiB, así que tengo 20 GiB de espacio"
   → El filesystem tiene overhead (metadatos, journal, reserved blocks)
   → Un LV de 20 GiB ext4 ofrece ~18.6 GiB usables

✓ Verificar espacio real: df -h /mount/point
```

### 3. Usar guiones en nombres de VG/LV

```bash
# ✗ Guiones causan confusión en /dev/mapper/
vgcreate my-vg /dev/vdb1
lvcreate -L 10G -n my-lv my-vg
# /dev/mapper/my--vg-my--lv  ← difícil de leer

# ✓ Usar underscores
vgcreate my_vg /dev/vdb1
lvcreate -L 10G -n my_lv my_vg
# /dev/mapper/my_vg-my_lv  ← claro
```

### 4. Olvidar que pvcreate destruye datos

```bash
# ✗ Inicializar un dispositivo con datos sin backup
pvcreate /dev/vdb1    # sobreescribe la cabecera del disco

# ✓ Verificar que no haya nada importante antes
blkid /dev/vdb1       # ¿tiene filesystem?
mount | grep vdb1     # ¿está montado?
# Solo si está vacío o ya respaldado:
pvcreate /dev/vdb1
```

### 5. No dejar espacio libre en el VG

```bash
# ✗ Asignar el 100% del VG a LVs
lvcreate -l 100%FREE -n lv_data vg0
# No queda espacio para snapshots ni para crecer otros LVs

# ✓ Dejar 10-20% libre para snapshots y crecimiento futuro
lvcreate -L 80G -n lv_data vg0    # en un VG de 100 GiB
```

---

## Cheatsheet

```
┌──────────────────────────────────────────────────────────────────┐
│                   LVM Conceptos — Referencia rápida              │
├──────────────────────────────────────────────────────────────────┤
│                                                                  │
│  ARQUITECTURA:                                                   │
│    Disco/Partición → PV → VG (pool) → LV → Filesystem → Mount   │
│                                                                  │
│  PHYSICAL VOLUME (PV):                                           │
│    pvcreate /dev/vdb1          inicializar como PV               │
│    pvs                         listar PVs                        │
│    pvdisplay /dev/vdb1         detalle                           │
│                                                                  │
│  VOLUME GROUP (VG):                                              │
│    vgcreate vg0 /dev/vdb1      crear VG con un PV                │
│    vgs                         listar VGs                        │
│    vgdisplay vg0               detalle                           │
│    vgextend vg0 /dev/vdc1      añadir PV al pool                │
│                                                                  │
│  LOGICAL VOLUME (LV):                                            │
│    lvcreate -L 20G -n lv vg0   crear LV de 20 GiB               │
│    lvs                         listar LVs                        │
│    lvdisplay /dev/vg0/lv       detalle                           │
│                                                                  │
│  PHYSICAL EXTENT (PE):                                           │
│    Default: 4 MiB              unidad mínima de asignación       │
│    Se define al crear el VG    no se puede cambiar después       │
│                                                                  │
│  PATHS DEL LV (todos equivalentes):                              │
│    /dev/vg0/lv_root            path lógico                       │
│    /dev/mapper/vg0-lv_root     path de Device Mapper             │
│    /dev/dm-N                   dispositivo real                  │
│                                                                  │
│  FLUJO COMPLETO:                                                 │
│    pvcreate /dev/vdb1                                            │
│    vgcreate vg0 /dev/vdb1                                        │
│    lvcreate -L 20G -n lv_data vg0                                │
│    mkfs.ext4 /dev/vg0/lv_data                                    │
│    mount /dev/vg0/lv_data /mnt/data                              │
│                                                                  │
└──────────────────────────────────────────────────────────────────┘
```

---

## Ejercicios

### Ejercicio 1: Mapear la arquitectura LVM de tu sistema

En tu VM de lab (o tu sistema host si usa LVM):

1. Lista todos los PVs: `pvs`
2. Lista todos los VGs: `vgs`
3. Lista todos los LVs: `lvs`
4. Para cada LV, verifica los tres paths:
   ```bash
   ls -l /dev/mapper/vg0-*
   ls -l /dev/vg0/
   ```
5. Identifica el dispositivo dm real: `dmsetup ls`
6. Dibuja un diagrama en papel con la relación PV → VG → LV de tu sistema
7. Compara con la salida de `lsblk` — ¿ves la jerarquía?

> **Pregunta de reflexión**: si tu sistema tiene un VG llamado `rhel` con LVs `root` y `swap`, ¿cuántos PVs componen ese VG? ¿Qué pasaría si añades un segundo disco como PV al mismo VG?

### Ejercicio 2: Calcular Physical Extents

Usando la información de tu VG:

1. `vgdisplay vg0` — anota el PE Size y el Total PE / Free PE
2. Calcula: ¿cuántos PEs necesitas para un LV de 10 GiB con PE de 4 MiB?
3. Verifica tu cálculo: `lvcreate -L 10G -n lv_test vg0` y luego `lvdisplay /dev/vg0/lv_test | grep "Current LE"`
4. ¿Coincide con tu cálculo? (10 GiB ÷ 4 MiB = 2560 LEs)
5. Elimina el LV de prueba: `lvremove /dev/vg0/lv_test`

> **Pregunta de reflexión**: ¿Qué ocurre si intentas crear un LV de 1 MiB en un VG con PE de 4 MiB? ¿LVM lo redondea hacia arriba o lo rechaza?

### Ejercicio 3: Explorar los metadatos

1. Examina los backups automáticos: `ls -la /etc/lvm/backup/`
2. Lee el contenido del backup: `cat /etc/lvm/backup/vg0` (si existe)
3. Identifica en el archivo: nombre del VG, lista de PVs, lista de LVs, PE size
4. Examina el archivo histórico: `ls /etc/lvm/archive/` — ¿cuántos snapshots de configuración hay?
5. Usa `pvck /dev/vdb1` para verificar los metadatos en el PV

> **Pregunta de reflexión**: si pierdes el disco que contiene `/etc/lvm/backup/` pero los PVs están intactos, ¿se pierde la configuración LVM? ¿Dónde más están almacenados los metadatos?
