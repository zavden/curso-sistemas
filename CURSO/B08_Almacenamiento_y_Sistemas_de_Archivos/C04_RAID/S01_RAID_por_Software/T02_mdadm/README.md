# mdadm

## Índice

1. [Qué es mdadm](#qué-es-mdadm)
2. [Modos de operación](#modos-de-operación)
3. [Crear un array (--create)](#crear-un-array---create)
4. [Ensamblar un array (--assemble)](#ensamblar-un-array---assemble)
5. [Detener un array (--stop)](#detener-un-array---stop)
6. [Persistencia: mdadm.conf](#persistencia-mdadmconf)
7. [Marcar disco como fallido (--fail)](#marcar-disco-como-fallido---fail)
8. [Retirar un disco (--remove)](#retirar-un-disco---remove)
9. [Añadir un disco (--add)](#añadir-un-disco---add)
10. [Reemplazar un disco fallido — flujo completo](#reemplazar-un-disco-fallido--flujo-completo)
11. [Creación por nivel: RAID 0, 1, 5, 6, 10](#creación-por-nivel-raid-0-1-5-6-10)
12. [Errores comunes](#errores-comunes)
13. [Cheatsheet](#cheatsheet)
14. [Ejercicios](#ejercicios)

---

## Qué es mdadm

`mdadm` (Multiple Disk Admin) es la herramienta estándar de Linux para gestionar RAID por software. Crea y administra arrays usando el driver `md` del kernel.

```
┌───────────────────────────────────────────────┐
│            mdadm + md (kernel)                 │
│                                               │
│  mdadm (userspace)                            │
│  ├── Crear arrays                             │
│  ├── Ensamblar al arranque                    │
│  ├── Monitorizar salud                        │
│  └── Gestionar discos (add, fail, remove)     │
│           │                                   │
│           ▼                                   │
│  md driver (kernel)                           │
│  ├── Cálculos de paridad                      │
│  ├── I/O distribuido                          │
│  ├── Rebuild automático                       │
│  └── Expone /dev/mdN                          │
│           │                                   │
│           ▼                                   │
│  /dev/md0, /dev/md1, ...                      │
│  (dispositivos de bloque normales)            │
└───────────────────────────────────────────────┘
```

Los arrays RAID aparecen como `/dev/md0`, `/dev/md1`, etc. Se usan exactamente como cualquier dispositivo de bloque: puedes crear filesystem, montar, usar con LVM, etc.

---

## Modos de operación

mdadm tiene varios modos, cada uno con su flag principal:

| Modo | Flag | Uso |
|------|------|-----|
| Create | `--create` | Crear un array nuevo |
| Assemble | `--assemble` | Re-ensamblar un array existente |
| Manage | `--add`, `--remove`, `--fail` | Gestionar discos de un array activo |
| Misc | `--detail`, `--examine`, `--stop`, `--zero-superblock` | Inspección y limpieza |
| Monitor | `--monitor` | Vigilar arrays y enviar alertas |
| Grow | `--grow` | Redimensionar un array (añadir discos, cambiar nivel) |

---

## Crear un array (--create)

### Sintaxis

```bash
mdadm --create /dev/mdN --level=NIVEL --raid-devices=N DISPOSITIVOS...
```

### Ejemplo: RAID 1 con 2 discos

```bash
mdadm --create /dev/md0 --level=1 --raid-devices=2 /dev/vdb /dev/vdc
```

El comando:
1. Escribe un **superbloque** md en cada disco (metadatos del array)
2. Crea el dispositivo `/dev/md0`
3. Inicia la sincronización inicial (los discos se sincronizan en background)

```bash
# Salida típica:
# mdadm: Note: this array has metadata at the start and
#     may not be suitable as a boot device.  If you plan to
#     store '/boot' on this device please ensure that
#     your boot-loader understands md/v1.x metadata, or use
#     --metadata=0.90
# Continue creating array? y
# mdadm: Defaulting to version 1.2 metadata
# mdadm: array /dev/md0 started.
```

### Opciones principales de --create

| Opción | Descripción | Ejemplo |
|--------|-------------|---------|
| `--level=` | Nivel RAID (0, 1, 5, 6, 10) | `--level=5` |
| `--raid-devices=` | Número de discos activos | `--raid-devices=3` |
| `--spare-devices=` | Número de hot spares | `--spare-devices=1` |
| `--metadata=` | Versión de metadatos | `--metadata=1.2` (default) |
| `--name=` | Nombre del array | `--name=data` |
| `--bitmap=internal` | Bitmap para rebuild más rápido | `--bitmap=internal` |
| `--chunk=` | Tamaño del chunk (stripe) | `--chunk=512K` |

### Sincronización inicial

Después de crear un array con redundancia, los discos se sincronizan. Esto toma tiempo pero ocurre en background — el array ya es usable.

```bash
# Ver progreso de sincronización
cat /proc/mdstat
# md0 : active raid1 vdc[1] vdb[0]
#       1047552 blocks super 1.2 [2/2] [UU]
#       [========>............]  resync = 42.3% (443392/1047552) finish=0.1min speed=88678K/sec

# [UU] = ambos discos Up
# [U_] = un disco Up, uno ausente
```

---

## Ensamblar un array (--assemble)

Después de un reinicio, los arrays deben **ensamblarse** para que `/dev/mdN` reaparezca. Esto normalmente ocurre automáticamente, pero se puede hacer manualmente.

### Ensamblado automático

Si `mdadm.conf` está configurado (ver siguiente sección), los arrays se ensamblan al arrancar. También se puede forzar:

```bash
# Ensamblar todos los arrays definidos en mdadm.conf
mdadm --assemble --scan

# Ensamblar un array específico
mdadm --assemble /dev/md0

# Ensamblar especificando los dispositivos
mdadm --assemble /dev/md0 /dev/vdb /dev/vdc
```

### ¿Cómo sabe mdadm qué discos forman un array?

Cada disco miembro tiene un **superbloque** con:
- UUID del array
- Nivel RAID
- Número de discos
- Posición del disco en el array
- Estado (activo, spare, fallido)

```bash
# Examinar superbloque de un disco
mdadm --examine /dev/vdb
#           Magic : a92b4efc
#         Version : 1.2
#     Raid Level : raid1
#  Raid Devices : 2
#   Array UUID : 12345678:abcdef01:...
#          Name : lab-vm1:0
#    Array Size : 1047552 (1023.00 MiB)
```

---

## Detener un array (--stop)

Detener un array desactiva `/dev/mdN` y libera los discos. El array se puede re-ensamblar después.

```bash
# Desmontar primero si tiene filesystem
umount /mnt/raid

# Detener el array
mdadm --stop /dev/md0
# mdadm: stopped /dev/md0

# Verificar que desapareció
ls /dev/md0
# ls: cannot access '/dev/md0': No such file or directory
```

> **Importante**: detener un array **no destruye datos** ni borra superbloques. Los datos y metadatos siguen en los discos. Un `--assemble` posterior lo recupera.

### Destruir un array permanentemente

```bash
# 1. Desmontar
umount /mnt/raid

# 2. Detener
mdadm --stop /dev/md0

# 3. Borrar superbloques de cada disco
mdadm --zero-superblock /dev/vdb
mdadm --zero-superblock /dev/vdc

# Ahora los discos son discos normales de nuevo
# mdadm no los reconocerá como miembros de un array
```

---

## Persistencia: mdadm.conf

`mdadm.conf` (ubicado en `/etc/mdadm.conf` o `/etc/mdadm/mdadm.conf`) almacena la configuración de los arrays para que se ensamblen automáticamente al arrancar.

### Generar mdadm.conf

```bash
# Escanear arrays activos y generar configuración
mdadm --detail --scan
# ARRAY /dev/md0 metadata=1.2 name=lab-vm1:0 UUID=12345678:abcdef01:...

# Guardar en mdadm.conf
mdadm --detail --scan >> /etc/mdadm.conf
```

### Formato de mdadm.conf

```bash
# /etc/mdadm.conf
MAILADDR root@localhost
ARRAY /dev/md0 metadata=1.2 UUID=12345678:abcdef01:23456789:abcdef01
ARRAY /dev/md1 metadata=1.2 UUID=aabbccdd:11223344:55667788:99aabbcc
```

| Directiva | Significado |
|-----------|-------------|
| `MAILADDR` | Dirección para alertas de monitorización |
| `ARRAY` | Define un array por UUID |

### Actualizar initramfs

En algunos sistemas, el initramfs necesita conocer la configuración de mdadm para ensamblar arrays del rootfs:

```bash
# RHEL/AlmaLinux
dracut --force

# Debian/Ubuntu
update-initramfs -u
```

### Verificar persistencia

```bash
# Reiniciar la VM
reboot

# Después del reinicio, verificar
cat /proc/mdstat
# md0 : active raid1 ...     ← array se ensambló automáticamente

mdadm --detail /dev/md0
```

---

## Marcar disco como fallido (--fail)

Simula un fallo de disco o marca manualmente un disco que muestra errores.

```bash
# Marcar disco como fallido
mdadm /dev/md0 --fail /dev/vdc
# mdadm: set /dev/vdc faulty in /dev/md0

# Verificar
cat /proc/mdstat
# md0 : active raid1 vdc[1](F) vdb[0]
#       1047552 blocks super 1.2 [2/1] [U_]
#                                       ↑ un disco ausente

mdadm --detail /dev/md0
#     Number   Major   Minor   RaidDevice State
#        0     252       16        0      active sync   /dev/vdb
#        1     252       32        -      faulty        /dev/vdc
```

> **Predicción**: después de `--fail`, el array pasa a estado **degradado** (`[U_]`). Sigue funcionando con los discos restantes, pero sin tolerancia a otro fallo. En RAID 1, uno de los discos todavía tiene todos los datos.

---

## Retirar un disco (--remove)

Un disco marcado como faulty se puede retirar del array. Solo se pueden retirar discos que están en estado faulty o spare.

```bash
# Retirar disco fallido
mdadm /dev/md0 --remove /dev/vdc
# mdadm: hot removed /dev/vdc from /dev/md0

# Verificar
mdadm --detail /dev/md0
# Solo queda /dev/vdb como activo
```

> **Nota**: no puedes retirar un disco activo directamente. Primero `--fail`, luego `--remove`.

---

## Añadir un disco (--add)

Añadir un disco a un array activo. Si el array está degradado, el nuevo disco se usa para reconstruir (rebuild). Si el array está completo, el disco se añade como **hot spare**.

```bash
# Añadir disco de reemplazo
mdadm /dev/md0 --add /dev/vdd
# mdadm: added /dev/vdd

# El rebuild comienza automáticamente
cat /proc/mdstat
# md0 : active raid1 vdd[2] vdb[0]
#       1047552 blocks super 1.2 [2/1] [U_]
#       [=>...................]  recovery = 8.5% ...

# Cuando termina:
# md0 : active raid1 vdd[2] vdb[0]
#       1047552 blocks super 1.2 [2/2] [UU]
```

> **Predicción**: al añadir un disco a un array degradado, mdadm inicia el rebuild inmediatamente. El progreso se ve en `/proc/mdstat`. Durante el rebuild, el array funciona pero con rendimiento reducido.

---

## Reemplazar un disco fallido — flujo completo

El escenario más importante en la gestión diaria de RAID:

```
┌──────────────────────────────────────────────────────────┐
│            Flujo: reemplazar disco fallido                │
│                                                          │
│  1. Detectar fallo                                       │
│     cat /proc/mdstat → [U_] o (F)                        │
│     mdadm --detail /dev/md0 → State: faulty              │
│                                                          │
│  2. Marcar como fallido (si no lo está ya)               │
│     mdadm /dev/md0 --fail /dev/vdc                       │
│                                                          │
│  3. Retirar del array                                    │
│     mdadm /dev/md0 --remove /dev/vdc                     │
│                                                          │
│  4. Reemplazar físicamente                               │
│     (en VM: detach disco viejo, attach disco nuevo)      │
│                                                          │
│  5. Añadir disco nuevo al array                          │
│     mdadm /dev/md0 --add /dev/vdd                        │
│                                                          │
│  6. Monitorizar rebuild                                  │
│     watch cat /proc/mdstat                               │
│     (esperar hasta [UU] o [UUU] etc.)                    │
│                                                          │
│  7. Actualizar mdadm.conf si cambió el device            │
│     mdadm --detail --scan > /etc/mdadm.conf              │
│                                                          │
│  Todo esto SIN desmontar el filesystem                   │
└──────────────────────────────────────────────────────────┘
```

### Ejemplo completo en terminal

```bash
# Estado inicial: RAID 1 sano
cat /proc/mdstat
# md0 : active raid1 vdc[1] vdb[0]
#       1047552 blocks super 1.2 [2/2] [UU]

# Simular fallo de disco
mdadm /dev/md0 --fail /dev/vdc
# mdadm: set /dev/vdc faulty in /dev/md0

cat /proc/mdstat
# md0 : active raid1 vdc[1](F) vdb[0]
#       1047552 blocks super 1.2 [2/1] [U_]

# Retirar disco fallido
mdadm /dev/md0 --remove /dev/vdc

# Limpiar superbloque del disco viejo (buena práctica)
mdadm --zero-superblock /dev/vdc

# Añadir disco de reemplazo (supongamos /dev/vdd nuevo)
mdadm /dev/md0 --add /dev/vdd

# Monitorizar rebuild
watch cat /proc/mdstat
# Esperar hasta [2/2] [UU]

# Actualizar configuración
mdadm --detail --scan > /etc/mdadm.conf

# Verificar resultado
mdadm --detail /dev/md0
# State : clean
# Active Devices : 2
# ...
```

> **En una VM de lab**: en lugar de reemplazar físicamente, puedes usar el mismo disco o añadir uno nuevo con `virsh attach-disk`.

---

## Creación por nivel: RAID 0, 1, 5, 6, 10

### RAID 0 (2+ discos)

```bash
mdadm --create /dev/md0 --level=0 --raid-devices=2 /dev/vdb /dev/vdc

# Verificar
mdadm --detail /dev/md0
# Raid Level : raid0
# Array Size : 2093056 (2044.00 MiB)  ← suma de ambos discos
# Raid Devices : 2

# Usar
mkfs.ext4 /dev/md0
mount /dev/md0 /mnt/raid0
```

Sin redundancia — no tiene sentido `--fail` ni `--add`.

### RAID 1 (2+ discos)

```bash
mdadm --create /dev/md0 --level=1 --raid-devices=2 /dev/vdb /dev/vdc

# Con hot spare
mdadm --create /dev/md0 --level=1 --raid-devices=2 \
    /dev/vdb /dev/vdc --spare-devices=1 /dev/vdd

# Verificar
mdadm --detail /dev/md0
# Raid Level : raid1
# Array Size : 1047552 (1023.00 MiB)  ← tamaño de 1 disco
# Raid Devices : 2
# Spare Devices : 1
```

### RAID 5 (3+ discos)

```bash
mdadm --create /dev/md0 --level=5 --raid-devices=3 \
    /dev/vdb /dev/vdc /dev/vdd

# Con hot spare
mdadm --create /dev/md0 --level=5 --raid-devices=3 \
    /dev/vdb /dev/vdc /dev/vdd --spare-devices=1 /dev/vde

# Verificar
mdadm --detail /dev/md0
# Raid Level : raid5
# Array Size : 2093056 (2044.00 MiB)  ← (N-1) × disco
# Raid Devices : 3

# La sincronización inicial incluye cálculo de paridad
cat /proc/mdstat
# md0 : active raid5 vdd[3] vdc[1] vdb[0]
#       [=====>...............]  resync = 28.0%
```

### RAID 6 (4+ discos)

```bash
mdadm --create /dev/md0 --level=6 --raid-devices=4 \
    /dev/vdb /dev/vdc /dev/vdd /dev/vde

# Verificar
mdadm --detail /dev/md0
# Raid Level : raid6
# Array Size : 2093056  ← (N-2) × disco
# Raid Devices : 4
```

### RAID 10 (4+ discos, número par)

```bash
mdadm --create /dev/md0 --level=10 --raid-devices=4 \
    /dev/vdb /dev/vdc /dev/vdd /dev/vde

# Verificar
mdadm --detail /dev/md0
# Raid Level : raid10
# Array Size : 2093056  ← N/2 × disco
# Raid Devices : 4
```

### Después de crear cualquier array

```bash
# 1. Esperar sincronización inicial (o usarlo mientras sincroniza)
cat /proc/mdstat

# 2. Crear filesystem
mkfs.ext4 /dev/md0    # o mkfs.xfs, etc.

# 3. Montar
mkdir -p /mnt/raid
mount /dev/md0 /mnt/raid

# 4. Guardar configuración
mdadm --detail --scan >> /etc/mdadm.conf

# 5. Añadir a fstab (usar UUID)
blkid /dev/md0
echo 'UUID=xxxx  /mnt/raid  ext4  defaults  0  2' >> /etc/fstab
```

---

## Errores comunes

### 1. No guardar mdadm.conf

```bash
# ✗ Crear array sin guardar configuración → se pierde al reiniciar
mdadm --create /dev/md0 --level=1 --raid-devices=2 /dev/vdb /dev/vdc
reboot
# /dev/md0 no aparece (o aparece con nombre distinto)

# ✓ Guardar configuración inmediatamente
mdadm --create /dev/md0 --level=1 --raid-devices=2 /dev/vdb /dev/vdc
mdadm --detail --scan >> /etc/mdadm.conf
# En RHEL: dracut --force
```

### 2. Intentar --remove un disco activo

```bash
# ✗ Remove directo sin fail
mdadm /dev/md0 --remove /dev/vdc
# mdadm: hot remove failed for /dev/vdc: Device or resource busy

# ✓ Primero fail, luego remove
mdadm /dev/md0 --fail /dev/vdc
mdadm /dev/md0 --remove /dev/vdc
```

### 3. No limpiar superbloques al reutilizar discos

```bash
# ✗ Crear array nuevo con discos que tenían superbloque previo
mdadm --create /dev/md1 --level=1 --raid-devices=2 /dev/vdb /dev/vdc
# mdadm: /dev/vdb appears to be part of a raid array
# → puede causar conflictos

# ✓ Limpiar superbloques antes de reutilizar
mdadm --zero-superblock /dev/vdb
mdadm --zero-superblock /dev/vdc
mdadm --create /dev/md1 --level=1 --raid-devices=2 /dev/vdb /dev/vdc
```

### 4. Crear RAID 5 con pocos discos para lab sin confirmar

```bash
# mdadm pregunta confirmación si detecta condiciones inusuales
# En scripts, usar --run para forzar
mdadm --create /dev/md0 --level=5 --raid-devices=3 \
    /dev/vdb /dev/vdc /dev/vdd --run
```

### 5. Olvidar actualizar fstab con UUID del array

```bash
# ✗ fstab con /dev/md0 — puede cambiar de nombre
/dev/md0   /mnt/raid   ext4   defaults   0   2

# ✓ fstab con UUID del array md
UUID=xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx   /mnt/raid   ext4   defaults   0   2

# Obtener UUID
blkid /dev/md0
```

---

## Cheatsheet

```
┌──────────────────────────────────────────────────────────────────┐
│                    mdadm — Referencia rápida                     │
├──────────────────────────────────────────────────────────────────┤
│                                                                  │
│  CREAR:                                                          │
│    mdadm --create /dev/md0 --level=1 --raid-devices=2 \          │
│        /dev/vdb /dev/vdc                                         │
│    mdadm --create /dev/md0 --level=5 --raid-devices=3 \          │
│        /dev/vdb /dev/vdc /dev/vdd --spare-devices=1 /dev/vde     │
│                                                                  │
│  ENSAMBLAR:                                                      │
│    mdadm --assemble --scan                 todos (desde conf)    │
│    mdadm --assemble /dev/md0               uno específico        │
│                                                                  │
│  DETENER:                                                        │
│    mdadm --stop /dev/md0                   detener array         │
│    mdadm --zero-superblock /dev/vdb        limpiar metadatos     │
│                                                                  │
│  INSPECCIONAR:                                                   │
│    mdadm --detail /dev/md0                 detalle del array     │
│    mdadm --examine /dev/vdb                superbloque del disco │
│    cat /proc/mdstat                        estado de todos       │
│                                                                  │
│  GESTIONAR DISCOS:                                               │
│    mdadm /dev/md0 --fail /dev/vdc          marcar fallido        │
│    mdadm /dev/md0 --remove /dev/vdc        retirar (tras fail)   │
│    mdadm /dev/md0 --add /dev/vdd           añadir (rebuild)      │
│                                                                  │
│  PERSISTENCIA:                                                   │
│    mdadm --detail --scan >> /etc/mdadm.conf  guardar config      │
│    dracut --force                            actualizar initramfs │
│                                                                  │
│  FLUJO REEMPLAZO:                                                │
│    --fail → --remove → (cambiar disco) → --add → watch mdstat    │
│                                                                  │
│  POST-CREACIÓN:                                                  │
│    mkfs.ext4 /dev/md0                                            │
│    mkdir -p /mnt/raid && mount /dev/md0 /mnt/raid                │
│    blkid /dev/md0 → añadir UUID a fstab                          │
│    mdadm --detail --scan >> /etc/mdadm.conf                      │
│                                                                  │
└──────────────────────────────────────────────────────────────────┘
```

---

## Ejercicios

### Ejercicio 1: Crear RAID 1 y verificar redundancia

En tu VM de lab con discos `/dev/vdb` y `/dev/vdc`:

1. Crea un array RAID 1:
   ```bash
   mdadm --create /dev/md0 --level=1 --raid-devices=2 /dev/vdb /dev/vdc
   ```
2. Observa la sincronización: `watch cat /proc/mdstat` (espera a `[UU]`)
3. Examina los detalles: `mdadm --detail /dev/md0`
4. Examina el superbloque de un disco: `mdadm --examine /dev/vdb`
5. Crea filesystem y monta:
   ```bash
   mkfs.ext4 /dev/md0
   mkdir -p /mnt/raid
   mount /dev/md0 /mnt/raid
   echo "test data" > /mnt/raid/important.txt
   ```
6. Simula fallo: `mdadm /dev/md0 --fail /dev/vdc`
7. Verifica: `cat /proc/mdstat` — ¿qué muestra `[U_]`?
8. ¿Puedes seguir leyendo? `cat /mnt/raid/important.txt`
9. Retira y reemplaza:
   ```bash
   mdadm /dev/md0 --remove /dev/vdc
   mdadm --zero-superblock /dev/vdc
   mdadm /dev/md0 --add /dev/vdc
   ```
10. Monitoriza el rebuild: `watch cat /proc/mdstat`
11. Guarda configuración: `mdadm --detail --scan > /etc/mdadm.conf`

> **Pregunta de reflexión**: durante el paso 8, el array está degradado pero los datos siguen accesibles. ¿Qué pasaría si en ese momento también fallara `/dev/vdb`?

### Ejercicio 2: Crear RAID 5 con hot spare

Necesitas 4 discos (`/dev/vdb`, `/dev/vdc`, `/dev/vdd`, `/dev/vde`):

1. Crea RAID 5 con 3 discos activos + 1 spare:
   ```bash
   mdadm --create /dev/md0 --level=5 --raid-devices=3 \
       /dev/vdb /dev/vdc /dev/vdd --spare-devices=1 /dev/vde
   ```
2. Espera la sincronización: `watch cat /proc/mdstat`
3. Verifica: `mdadm --detail /dev/md0` — identifica los discos activos y el spare
4. Crea filesystem, monta, escribe datos
5. Simula fallo: `mdadm /dev/md0 --fail /dev/vdc`
6. Observa `/proc/mdstat` — ¿el spare se activó automáticamente?
7. Espera a que el rebuild termine
8. ¿Cuántos discos activos y cuántos spares hay ahora? (`mdadm --detail /dev/md0`)
9. Limpieza:
   ```bash
   umount /mnt/raid
   mdadm --stop /dev/md0
   mdadm --zero-superblock /dev/vdb /dev/vdc /dev/vdd /dev/vde
   ```

> **Pregunta de reflexión**: después del rebuild con el spare, el array tiene 3 discos activos y 0 spares. ¿Qué pasa si ahora falla otro disco? ¿Cómo añadirías un nuevo spare?

### Ejercicio 3: Persistencia tras reinicio

1. Crea un RAID 1 con `/dev/vdb` y `/dev/vdc`
2. Formatea, monta en `/mnt/raid`, escribe datos
3. Guarda configuración:
   ```bash
   mdadm --detail --scan > /etc/mdadm.conf
   ```
4. Añade a fstab usando UUID:
   ```bash
   blkid /dev/md0
   echo 'UUID=xxx  /mnt/raid  ext4  defaults  0  2' >> /etc/fstab
   ```
5. Reinicia la VM
6. Después del reinicio, verifica:
   - `cat /proc/mdstat` — ¿el array se ensambló?
   - `findmnt /mnt/raid` — ¿el filesystem se montó?
   - `cat /mnt/raid/important.txt` — ¿los datos están?
7. Limpieza completa:
   ```bash
   umount /mnt/raid
   mdadm --stop /dev/md0
   mdadm --zero-superblock /dev/vdb /dev/vdc
   # Editar /etc/fstab y /etc/mdadm.conf para eliminar entradas
   ```

> **Pregunta de reflexión**: ¿qué pasaría si olvidas actualizar `mdadm.conf` pero sí tienes la entrada en fstab con UUID? ¿El sistema arrancará correctamente al reiniciar?
