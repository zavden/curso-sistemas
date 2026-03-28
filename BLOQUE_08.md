# Bloque 8: Almacenamiento y Sistemas de Archivos

**Objetivo**: Gestionar almacenamiento en Linux a nivel profesional. Cubre todo lo
necesario para LPIC-2 (topics 203-204) y RHCSA (almacenamiento local).

## Capítulo 1: Dispositivos de Bloque y Particiones [A]

### Sección 1: Dispositivos
- **T01 - Nomenclatura**: /dev/sd*, /dev/nvme*, /dev/vd* — convenciones según bus
- **T02 - Información del dispositivo**: lsblk, blkid, fdisk -l, udevadm
- **T03 - udev**: reglas, persistent naming, /dev/disk/by-{id,uuid,label,path}

### Sección 2: Particionamiento
- **T01 - MBR vs GPT**: limitaciones, estructura, cuándo usar cada uno
- **T02 - fdisk y gdisk**: creación de particiones, tipos, alineación
- **T03 - parted**: particionamiento avanzado, redimensionar
- **T04 - Particiones en contenedores Docker**: uso de loop devices para simular discos

## Capítulo 2: Sistemas de Archivos [A]

### Sección 1: Creación y Montaje
- **T01 - ext4**: mkfs.ext4, tune2fs, características, journal
- **T02 - XFS**: mkfs.xfs, xfs_admin, xfs_repair — diferencias clave con ext4
- **T03 - Btrfs**: subvolúmenes, snapshots, RAID integrado, compresión
- **T04 - mount y umount**: opciones de montaje, mount flags, lazy unmount

### Sección 2: Configuración Persistente
- **T01 - /etc/fstab**: campos, opciones, dump y pass, UUID vs device path
- **T02 - systemd mount units**: .mount y .automount, cuándo preferir sobre fstab
- **T03 - Filesystem check**: fsck, e2fsck, xfs_repair, cuándo ejecutar

## Capítulo 3: LVM [A]

### Sección 1: Volúmenes Lógicos
- **T01 - Conceptos**: PV, VG, LV, PE — la cadena completa
- **T02 - Crear y gestionar**: pvcreate, vgcreate, lvcreate, lvextend, lvreduce
- **T03 - Redimensionar en caliente**: extender un LV + filesystem sin desmontar
- **T04 - Snapshots LVM**: crear, montar, revertir, limitaciones

## Capítulo 4: RAID [A]

### Sección 1: RAID por Software
- **T01 - Niveles RAID**: 0, 1, 5, 6, 10 — cálculos de capacidad y tolerancia a fallos
- **T02 - mdadm**: crear, ensamblar, monitorear, reemplazar discos fallidos
- **T03 - /proc/mdstat y mdadm --detail**: monitoreo y diagnóstico
- **T04 - RAID + LVM**: combinación de tecnologías, orden de las capas

## Capítulo 5: Swap y Cuotas [A]

### Sección 1: Swap
- **T01 - Partición vs archivo swap**: mkswap, swapon, swapoff, prioridad
- **T02 - Swappiness**: vm.swappiness, cuándo ajustar, impacto en rendimiento

### Sección 2: Cuotas de Disco
- **T01 - Cuotas de usuario y grupo**: quota, repquota, edquota
- **T02 - Configuración**: montar con usrquota/grpquota, quotacheck, quotaon
- **T03 - Soft vs hard limits**: grace period, comportamiento al exceder

### Sección 3: Almacenamiento Moderno en RHEL
- **T01 - Stratis**: concepto (pool-based), instalación, stratis pool create, filesystem create, snapshots — alternativa simplificada a LVM+XFS
- **T02 - VDO (Virtual Data Optimizer)**: deduplicación y compresión en línea, vdo create, estadísticas, cuándo es útil (virtualización, backups)
- **T03 - Comparación**: LVM+filesystem manual vs Stratis vs Btrfs, disponibilidad por distribución
