# Capítulo 0: QEMU/KVM y Entorno de Virtualización

**Objetivo**: Dominar QEMU/KVM y libvirt lo suficiente para crear y gestionar
máquinas virtuales que sirvan como laboratorio para los bloques 8–11 del curso.
No es un estudio exhaustivo de virtualización — es el conocimiento necesario y
suficiente para construir entornos de práctica realistas donde Docker no alcanza.

**Prerrequisitos**: B02 (Linux fundamentals — procesos, permisos, systemd, paquetes).

**Por qué este capítulo existe**: Docker comparte el kernel del host y no tiene
init system propio. A partir de B08 necesitamos particionar discos, crear
filesystems, configurar RAID/LVM, gestionar firewalls completos, arrancar con GRUB,
cargar módulos del kernel y ejecutar SELinux en modo enforcing. Todo esto requiere
máquinas virtuales completas con su propio kernel.

---

## Sección 1: Fundamentos de Virtualización

### T01 — Emulación vs virtualización
- Qué es emulación: traducir instrucciones de una arquitectura a otra (lento)
- Qué es virtualización: ejecutar instrucciones nativas con aislamiento (rápido)
- Hardware-assisted virtualization: Intel VT-x / AMD-V — extensiones del procesador
- Verificar soporte: `grep -E '(vmx|svm)' /proc/cpuinfo`, `lscpu | grep Virtualization`
- Virtualización anidada: habilitar nested virtualization cuando se corre dentro de otra VM
  (relevante si tu host ya es una VM — ej: VPS, WSL2 con Hyper-V)

### T02 — Hipervisores tipo 1 vs tipo 2
- Tipo 1 (bare-metal): Xen, VMware ESXi, Hyper-V — corre directamente sobre hardware
- Tipo 2 (hosted): VirtualBox, VMware Workstation — corre sobre un OS host
- KVM como caso especial: módulo del kernel Linux que convierte al host en hipervisor tipo 1
- Por qué KVM es la opción estándar en Linux: integrado en el kernel, open source, rendimiento nativo

### T03 — La pila QEMU/KVM
- QEMU: emulador de hardware completo en user-space (CPU, disco, red, USB, etc.)
- KVM: módulo del kernel que acelera la ejecución de instrucciones (/dev/kvm)
- QEMU + KVM juntos: QEMU provee la emulación de dispositivos, KVM ejecuta las instrucciones de CPU a velocidad nativa
- Diagrama de la pila: aplicación en VM → kernel guest → KVM (kernel host) → hardware real
- Verificar que KVM está activo: `lsmod | grep kvm`, `ls -la /dev/kvm`

---

## Sección 2: Instalación y Verificación

### T01 — Paquetes necesarios
- Fedora/RHEL/Alma: `dnf install @virtualization` (grupo completo) o paquetes individuales:
  `qemu-kvm`, `libvirt`, `virt-install`, `virt-manager`, `virt-viewer`
- Debian/Ubuntu: `apt install qemu-kvm libvirt-daemon-system virtinst virt-manager virt-viewer`
- Paquetes auxiliares: `bridge-utils`, `guestfs-tools` (para inspeccionar imágenes),
  `cloud-init` (para imágenes cloud), `swtpm` (para TPM virtual)
- Verificar instalación: `virsh version`, `virt-host-validate`

### T02 — Configuración del servicio libvirtd
- Habilitar e iniciar: `systemctl enable --now libvirtd`
- Socket activation: `libvirtd.socket` vs `libvirtd.service`
- Grupo libvirt: añadir usuario al grupo para evitar sudo
  (`usermod -aG libvirt $USER`, cerrar y abrir sesión)
- Verificar conectividad: `virsh -c qemu:///system list --all`
- Directorios importantes:
  - `/var/lib/libvirt/images/` — pool de almacenamiento por defecto
  - `/etc/libvirt/` — configuración de libvirt
  - `/var/log/libvirt/` — logs de VMs

### T03 — Verificación completa del entorno
- `virt-host-validate`: verificar que CPU, KVM, IOMMU y cgroups están OK
- Problemas comunes: KVM no disponible (nested virt deshabilitada), permisos de /dev/kvm
- Test rápido: crear una VM mínima de prueba y destruirla
- Rendimiento: comparar velocidad con y sin KVM (`-enable-kvm` vs emulación pura)

---

## Sección 3: Imágenes de Disco y Storage Pools

### T01 — Formatos de imagen
- raw: sin metadata, acceso directo, mejor rendimiento, tamaño fijo
- qcow2: copy-on-write, snapshots, compresión, thin provisioning (crece bajo demanda)
- Comparación de rendimiento: raw vs qcow2 (diferencia real es mínima con preallocation)
- Crear imágenes: `qemu-img create -f qcow2 disk.qcow2 20G`
- Inspeccionar: `qemu-img info disk.qcow2`
- Convertir entre formatos: `qemu-img convert -f raw -O qcow2 input.raw output.qcow2`
- Redimensionar: `qemu-img resize disk.qcow2 +10G`

### T02 — Imágenes backing (overlays)
- Concepto: imagen base (read-only) + overlay (copy-on-write para cambios)
- Crear overlay: `qemu-img create -f qcow2 -b base.qcow2 -F qcow2 overlay.qcow2`
- Por qué es útil para labs: una imagen base Debian/Alma, múltiples overlays para
  distintos ejercicios — rápido de crear, rápido de destruir, poco espacio en disco
- Cadenas de backing: overlay → overlay → base (evitar cadenas largas)
- Commit de cambios: `qemu-img commit overlay.qcow2` (fusionar overlay → base)
- Rebase: cambiar la imagen base de un overlay

### T03 — Storage pools de libvirt
- Qué es un pool: directorio u otro backend donde libvirt gestiona volúmenes
- Pool por defecto: `/var/lib/libvirt/images/`
- Crear pool en directorio custom: `virsh pool-define-as`, `pool-build`, `pool-start`, `pool-autostart`
- Listar y gestionar: `virsh pool-list`, `virsh vol-list <pool>`, `virsh vol-create-as`
- Por qué usar pools: libvirt gestiona permisos, limpieza, y los volúmenes aparecen en virt-manager

---

## Sección 4: Creación de VMs con virt-install

### T01 — Instalación desde ISO
- Descargar ISOs: Debian netinst, AlmaLinux minimal
- Comando básico:
  ```
  virt-install \
    --name debian-lab \
    --ram 2048 \
    --vcpus 2 \
    --disk size=20 \
    --cdrom /path/to/debian.iso \
    --os-variant debian12 \
    --network network=default \
    --graphics spice
  ```
- Flags importantes:
  - `--name`: nombre único de la VM
  - `--ram` / `--memory`: MB de RAM
  - `--vcpus`: número de CPUs virtuales
  - `--disk`: ruta o tamaño (crea qcow2 en el pool por defecto)
  - `--cdrom` / `--location`: medio de instalación
  - `--os-variant`: optimiza virtio/drivers (`osinfo-query os` para lista)
  - `--network`: red virtual (default = NAT)
  - `--graphics`: spice (GUI), vnc, o none (solo consola serial)
- Listar variantes de OS: `osinfo-query os | grep -i debian`

### T02 — Instalación desatendida (kickstart / preseed)
- Concepto: archivo de respuestas que automatiza la instalación sin intervención
- Kickstart (RHEL/Alma): archivo .ks con particionado, paquetes, usuarios, red
  - Estructura básica: `install`, `rootpw`, `timezone`, `part`, `%packages`, `%post`
  - Pasar a virt-install: `--extra-args "inst.ks=http://host/ks.cfg"`
- Preseed (Debian): archivo preseed.cfg con respuestas para d-i
  - Pasar a virt-install: `--extra-args "preseed/url=http://host/preseed.cfg"`
- Por qué importa: crear VMs de lab reproducibles en minutos, no en media hora

### T03 — Imágenes cloud-init
- Concepto: imágenes pre-construidas que se configuran al primer boot via cloud-init
- Dónde obtenerlas: cloud.debian.org, cloud.almalinux.org (qcow2 listas para usar)
- Crear disco de configuración (seed):
  - `meta-data`: instance-id, hostname
  - `user-data`: usuarios, contraseñas, paquetes, scripts, SSH keys
  - Generar ISO seed: `cloud-localds seed.iso user-data meta-data`
- Lanzar VM con cloud-init:
  ```
  virt-install \
    --name alma-lab \
    --ram 2048 \
    --vcpus 2 \
    --import \
    --disk path=alma-cloud.qcow2 \
    --disk path=seed.iso,device=cdrom \
    --os-variant almalinux9 \
    --network network=default
  ```
- Ventaja: VM lista en ~30 segundos vs ~20 minutos de instalación desde ISO

### T04 — Plantillas de VMs para el curso
- Estrategia: crear 2 VMs base (Debian 12, AlmaLinux 9) con configuración mínima
  - Usuario de lab, sudo configurado, SSH habilitado
  - Paquetes base instalados (vim, curl, tmux, man-pages)
  - cloud-init deshabilitado después del primer boot
- Convertir a plantilla: apagar VM, marcar disco como read-only
- Clonar para cada lab: `virt-clone` o crear overlay sobre la plantilla
- Script de provisioning: crear N VMs con discos extra, red interna, etc.

---

## Sección 5: Gestión de VMs con virsh

### T01 — Ciclo de vida
- Iniciar: `virsh start <vm>`
- Apagar (graceful): `virsh shutdown <vm>` (envía ACPI shutdown)
- Forzar apagado: `virsh destroy <vm>` (equivale a desconectar el cable de poder)
- Pausar/reanudar: `virsh suspend <vm>`, `virsh resume <vm>`
- Reiniciar: `virsh reboot <vm>`
- Eliminar VM y disco: `virsh undefine <vm> --remove-all-storage`
- Listar todas: `virsh list --all`

### T02 — Modificar VMs existentes
- Editar XML: `virsh edit <vm>` (validación automática del XML)
- Agregar disco en caliente: `virsh attach-disk <vm> /path/to/disk.qcow2 vdb --persistent`
- Agregar interfaz de red: `virsh attach-interface <vm> network <network-name> --persistent`
- Cambiar RAM/CPU: `virsh setmem`, `virsh setvcpus` (requiere VM apagada o maxmemory configurado)
- Autostart: `virsh autostart <vm>` (arranca con el host)

### T03 — Snapshots
- Crear snapshot: `virsh snapshot-create-as <vm> --name "pre-lab-raid"`
- Listar snapshots: `virsh snapshot-list <vm>`
- Revertir: `virsh snapshot-revert <vm> --snapshotname "pre-lab-raid"`
- Eliminar: `virsh snapshot-delete <vm> --snapshotname "pre-lab-raid"`
- Snapshots internos (qcow2) vs externos: internos son más simples, externos más flexibles
- Uso en el curso: snapshot antes de cada lab destructivo (particionar, RAID, LUKS)
  para poder revertir y repetir sin reinstalar

### T04 — Acceso a la consola
- Consola gráfica: `virt-viewer <vm>` (SPICE), `virt-manager` (GUI completa)
- Consola serial: `virsh console <vm>` (requiere configurar getty en ttyS0 dentro de la VM)
  - Debian: `systemctl enable serial-getty@ttyS0`
  - RHEL: ya habilitado por defecto en imágenes cloud
- SSH: la forma preferida para trabajo real — configurar en el primer boot
- Por qué consola serial importa: cuando rompes GRUB o la red, es tu único acceso

---

## Sección 6: Networking Virtual

### T01 — Red NAT por defecto
- Red `default` de libvirt: 192.168.122.0/24, DHCP, NAT hacia el host
- La VM puede salir a internet, pero no es accesible desde fuera del host
- Verificar: `virsh net-list`, `virsh net-info default`, `virsh net-dhcp-leases default`
- Cuándo es suficiente: VMs individuales que necesitan internet (instalar paquetes, etc.)

### T02 — Red aislada (isolated)
- Red sin NAT ni acceso a internet — solo comunicación entre VMs
- Crear: `virsh net-define`, XML con `<forward mode='none'/>`
- Uso en el curso:
  - B09 (Redes): simular topologías sin afectar la red real del host
  - B10 (Servicios): servidor DNS/DHCP/web que solo sirve a las VMs del lab
  - B11 (Seguridad): lab de firewall sin riesgo de bloquear tu propia conexión

### T03 — Red bridge (puente)
- Bridge: las VMs aparecen en la misma red que el host (como máquinas físicas)
- Configurar bridge en el host: `nmcli` o `/etc/network/interfaces`
- Usar bridge en libvirt: `<interface type='bridge'><source bridge='br0'/>`
- Cuándo usar: cuando necesitas acceder a las VMs desde otros dispositivos de la red

### T04 — Redes internas con múltiples subnets
- Crear varias redes aisladas para simular routing:
  - Red A: 10.0.1.0/24 (oficina)
  - Red B: 10.0.2.0/24 (servidores)
  - VM router con dos interfaces (una en cada red)
- Uso en B09: practicar routing, NAT, firewall entre subnets
- Uso en B10: aislar servicios (DNS en una red, web en otra)

---

## Sección 7: Almacenamiento Virtual — Discos Extra

### T01 — Agregar discos virtuales a una VM
- Crear disco adicional: `qemu-img create -f qcow2 extra-disk.qcow2 5G`
- Adjuntar a VM existente (persistente):
  `virsh attach-disk <vm> extra-disk.qcow2 vdb --driver qemu --subdriver qcow2 --persistent`
- Verificar dentro de la VM: `lsblk`, `fdisk -l`
- Uso en B08: cada lab de particionado/LVM/RAID usa discos extra desechables

### T02 — Múltiples discos para labs de RAID/LVM
- Crear N discos de prueba con un script:
  ```bash
  for i in 1 2 3 4; do
    qemu-img create -f qcow2 disk-raid-${i}.qcow2 2G
  done
  ```
- Adjuntar todos a la VM: vdb, vdc, vdd, vde
- Lab de RAID5: 3 discos + 1 spare
- Lab de LVM: 2 discos → PVs → VG → múltiples LVs
- Después del lab: detach y eliminar discos — la VM base queda intacta

### T03 — Passthrough de dispositivos (para referencia)
- Qué es: dar acceso directo de un dispositivo físico a la VM
- USB passthrough: `--host-device <vendor:product>` (ej: USB drive para labs de mount)
- PCI passthrough (VFIO): concepto, requiere IOMMU — solo mencionado como referencia
- Para el curso: generalmente no es necesario, los discos virtuales son suficientes

---

## Sección 8: Automatización de Entornos de Lab

### T01 — Scripts de provisioning
- Script bash que crea el entorno completo para un bloque:
  - Crea VMs desde plantillas (overlay sobre imagen base)
  - Adjunta discos extra según el lab
  - Configura redes internas
  - Arranca las VMs
- Script de teardown: apaga y elimina todo
- Ejemplo para B08: 1 VM Debian + 1 VM Alma, cada una con 4 discos extra de 2G

### T02 — Vagrant con libvirt (opcional)
- Vagrant: herramienta para definir VMs en un Vagrantfile declarativo
- Plugin vagrant-libvirt: usa KVM en vez de VirtualBox
- Ventaja: `vagrant up` crea todo, `vagrant destroy` limpia todo
- Vagrantfile de ejemplo para un lab multi-VM
- Desventaja: otra herramienta que aprender, los scripts bash son más transparentes

### T03 — Recetas por bloque
- Entorno para B08 (Almacenamiento):
  - 1 VM Debian + 1 VM Alma
  - 4 discos extra cada una (para particiones, LVM, RAID)
  - Red default (NAT para instalar paquetes)
- Entorno para B09 (Redes):
  - 3 VMs: router + 2 hosts en subnets distintas
  - 2 redes aisladas + 1 red NAT (para el router)
  - Sin discos extra
- Entorno para B10 (Servicios):
  - 2-3 VMs: servidor Debian + servidor Alma + cliente
  - 1 red aislada (el servidor DNS/DHCP controla la red)
  - 1 red NAT (para instalar paquetes)
- Entorno para B11 (Seguridad/Kernel/Boot):
  - 1 VM Alma (SELinux) + 1 VM Debian (AppArmor)
  - 1 disco extra para LUKS
  - Snapshots antes de cada lab de GRUB/boot (recovery)
