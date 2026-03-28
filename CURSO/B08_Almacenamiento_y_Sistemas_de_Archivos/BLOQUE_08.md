# Bloque 8: Almacenamiento y Sistemas de Archivos

**Objetivo**: Gestionar almacenamiento en Linux a nivel profesional. Cubre todo lo
necesario para LPIC-2 (topics 203-204) y RHCSA (almacenamiento local).

**Nota sobre el entorno de lab**: A partir de este bloque, los laboratorios
requieren máquinas virtuales completas (QEMU/KVM) en lugar de contenedores Docker.
El Capítulo 0 cubre la instalación y uso de QEMU/KVM necesario para los labs de
B08–B11. Ver también [LABS_GUIDE.md](../LABS_GUIDE.md) para la guía completa de
entornos de laboratorio.

## Capítulo 0: QEMU/KVM y Entorno de Virtualización [A]

Ver [syllabus detallado](C00_QEMU_KVM_y_Entorno_de_Virtualización/syllabus.md).

### Sección 1: Fundamentos de Virtualización
- **T01 - Emulación vs virtualización**: hardware-assisted (VT-x/AMD-V), verificar soporte
- **T02 - Hipervisores tipo 1 vs tipo 2**: KVM como tipo 1 integrado en el kernel
- **T03 - La pila QEMU/KVM**: QEMU (emulación de dispositivos) + KVM (ejecución nativa)

### Sección 2: Instalación y Verificación
- **T01 - Paquetes necesarios**: qemu-kvm, libvirt, virt-install, virt-manager (Debian y RHEL)
- **T02 - Configuración de libvirtd**: systemctl, grupo libvirt, directorios
- **T03 - Verificación completa**: virt-host-validate, test de VM mínima

### Sección 3: Imágenes de Disco y Storage Pools
- **T01 - Formatos de imagen**: raw vs qcow2, crear/inspeccionar/convertir/redimensionar
- **T02 - Imágenes backing (overlays)**: base read-only + overlay copy-on-write para labs
- **T03 - Storage pools de libvirt**: pools de directorio, gestión con virsh

### Sección 4: Creación de VMs con virt-install
- **T01 - Instalación desde ISO**: virt-install completo, flags, os-variant
- **T02 - Instalación desatendida**: kickstart (RHEL), preseed (Debian)
- **T03 - Imágenes cloud-init**: qcow2 pre-construidas, user-data, VM en 30 segundos
- **T04 - Plantillas de VMs para el curso**: Debian 12 + AlmaLinux 9 base, clonar para labs

### Sección 5: Gestión de VMs con virsh
- **T01 - Ciclo de vida**: start, shutdown, destroy, suspend, undefine
- **T02 - Modificar VMs**: agregar discos/interfaces en caliente, editar XML
- **T03 - Snapshots**: crear, revertir, eliminar — esencial para labs destructivos
- **T04 - Acceso a consola**: virt-viewer, consola serial, SSH

### Sección 6: Networking Virtual
- **T01 - Red NAT por defecto**: 192.168.122.0/24, DHCP, salida a internet
- **T02 - Red aislada**: comunicación solo entre VMs, labs de firewall/servicios
- **T03 - Red bridge**: VMs en la misma red que el host
- **T04 - Redes con múltiples subnets**: simular routing para B09

### Sección 7: Almacenamiento Virtual
- **T01 - Agregar discos virtuales**: crear y adjuntar discos extra a VMs
- **T02 - Múltiples discos para RAID/LVM**: provisionar N discos para labs de B08
- **T03 - Passthrough de dispositivos**: USB passthrough, VFIO (referencia)

### Sección 8: Automatización de Entornos de Lab
- **T01 - Scripts de provisioning**: crear/destruir entornos completos con bash
- **T02 - Vagrant con libvirt**: Vagrantfile declarativo (opcional)
- **T03 - Recetas por bloque**: entornos específicos para B08, B09, B10, B11

## Capítulo 1: Dispositivos de Bloque y Particiones [A]

### Sección 1: Dispositivos
- **T01 - Nomenclatura**: /dev/sd*, /dev/nvme*, /dev/vd* — convenciones según bus
- **T02 - Información del dispositivo**: lsblk, blkid, fdisk -l, udevadm
- **T03 - udev**: reglas, persistent naming, /dev/disk/by-{id,uuid,label,path}

### Sección 2: Particionamiento
- **T01 - MBR vs GPT**: limitaciones, estructura, cuándo usar cada uno
- **T02 - fdisk y gdisk**: creación de particiones, tipos, alineación
- **T03 - parted**: particionamiento avanzado, redimensionar
- **T04 - Loop devices**: simular discos con archivos, losetup, uso cuando no hay discos extra disponibles

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
