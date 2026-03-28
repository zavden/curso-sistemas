# Passthrough de dispositivos (para referencia)

## Índice

1. [Qué es passthrough](#1-qué-es-passthrough)
2. [USB passthrough](#2-usb-passthrough)
3. [PCI passthrough (VFIO)](#3-pci-passthrough-vfio)
4. [Necesidad en el curso](#4-necesidad-en-el-curso)
5. [Errores comunes](#5-errores-comunes)
6. [Cheatsheet](#6-cheatsheet)
7. [Ejercicios](#7-ejercicios)

---

> **Nota**: este tema es de **referencia**. Para los labs de B08 no necesitas
> passthrough — los discos virtuales (T01 y T02) son suficientes. Este README
> existe para que entiendas la opción y sepas usarla si algún lab futuro lo
> requiere.

---

## 1. Qué es passthrough

Passthrough es dar acceso **directo** a un dispositivo físico del host a una VM,
sin intermediarios de virtualización. El dispositivo deja de estar disponible para
el host y pasa a ser controlado exclusivamente por la VM.

```
Sin passthrough (emulación):              Con passthrough:
┌──────────┐                              ┌──────────┐
│    VM    │                              │    VM    │
│  driver  │                              │  driver  │
└────┬─────┘                              └────┬─────┘
     │                                         │
┌────▼─────┐                                   │  acceso directo
│  QEMU    │  emulación                        │  (sin QEMU en medio)
│ (virtio) │                                   │
└────┬─────┘                                   │
     │                                         │
┌────▼─────────────────┐              ┌────────▼───────────┐
│ Hardware real (host) │              │ Hardware real (host)│
└──────────────────────┘              └────────────────────┘

Rendimiento: bueno (virtio)           Rendimiento: nativo
Flexibilidad: alta                    Flexibilidad: baja
  (snapshots, migración, etc.)          (el device queda "atrapado")
```

Hay dos tipos principales de passthrough:

| Tipo | Qué pasa | Complejidad | Uso típico |
|------|----------|-------------|------------|
| USB passthrough | Un dispositivo USB específico | Baja | USB drives, tokens |
| PCI passthrough (VFIO) | Un dispositivo PCIe completo | Alta | GPUs, NICs, NVMe |

---

## 2. USB passthrough

### 2.1. Concepto

USB passthrough conecta un dispositivo USB del host directamente a la VM. Es la
forma más sencilla de passthrough:

```
Host                           VM
┌────────────────────┐         ┌──────────────────┐
│                    │         │                  │
│  USB Hub           │         │  /dev/sda        │
│  ├── Teclado       │         │  (aparece como   │
│  ├── Mouse         │         │   disco normal)  │
│  └── USB Drive ════╪════════►│                  │
│      (16GB)        │         │                  │
│      NO disponible │         │                  │
│      para el host  │         │                  │
└────────────────────┘         └──────────────────┘
```

### 2.2. Identificar el dispositivo USB

Primero, identifica el `vendor:product` del dispositivo:

```bash
# Listar dispositivos USB conectados
lsusb
```

Salida ejemplo:

```
Bus 002 Device 003: ID 0781:5567 SanDisk Corp. Cruzer Blade
Bus 002 Device 001: ID 1d6b:0003 Linux Foundation 3.0 root hub
Bus 001 Device 003: ID 046d:c52b Logitech, Inc. Unifying Receiver
Bus 001 Device 001: ID 1d6b:0002 Linux Foundation 2.0 root hub
```

El USB drive SanDisk tiene ID `0781:5567` — esto es `vendor:product`.

### 2.3. Pasar USB a una VM con virt-install

Al crear una VM:

```bash
virt-install \
  --name lab-usb \
  --ram 2048 --vcpus 2 \
  --disk path=/var/lib/libvirt/images/lab-usb.qcow2,size=10 \
  --import \
  --os-variant debian12 \
  --host-device 0781:5567
```

El flag `--host-device vendor:product` pasa el dispositivo USB a la VM.

### 2.4. Pasar USB a una VM existente (hot-plug)

Con `virsh`:

```bash
# Adjuntar un USB a una VM encendida
virsh attach-device lab-debian01 /tmp/usb-device.xml --persistent
```

Donde `/tmp/usb-device.xml` contiene:

```xml
<hostdev mode='subsystem' type='usb' managed='yes'>
  <source>
    <vendor id='0x0781'/>
    <product id='0x5567'/>
  </source>
</hostdev>
```

**Nota**: los IDs en XML llevan prefijo `0x` (hexadecimal).

### 2.5. Verificar dentro de la VM

```bash
# El USB drive aparece como un disco SCSI
lsblk
```

```
NAME   MAJ:MIN RM  SIZE RO TYPE MOUNTPOINTS
sda      8:0    1 14.3G  0 disk            ← USB drive (nota: sd, no vd)
vda    252:0    0   10G  0 disk
└─vda1 252:1    0   10G  0 part /
```

El USB drive aparece como `sda` (SCSI/USB) en vez de `vdX` (virtio) porque usa
su propio controlador USB, no el bus virtio.

### 2.6. Retirar el USB

```bash
# Dentro de la VM: desmontar si está montado
sudo umount /dev/sda1

# Desde el host: detach
virsh detach-device lab-debian01 /tmp/usb-device.xml
```

### 2.7. Caso de uso en el curso

Un escenario donde USB passthrough podría ser útil:

```
Lab de mount/umount:
  - Pasar un USB drive real a la VM
  - Practicar mount, umount, blkid, lsblk con un dispositivo real
  - Ver la diferencia entre /dev/sdX (USB) y /dev/vdX (virtio)

Sin embargo: esto mismo se puede practicar con discos virtuales,
que son más cómodos y no requieren un USB físico.
```

---

## 3. PCI passthrough (VFIO)

### 3.1. Concepto

PCI passthrough (usando el framework VFIO — Virtual Function I/O) asigna un
dispositivo PCIe completo a una VM. Es mucho más complejo que USB passthrough.

```
Casos de uso típicos (fuera del alcance de este curso):

┌──────────────────────────────────────────────────────────┐
│  GPU passthrough:     GPU dedicada para la VM            │
│                       (gaming, ML, rendering)            │
│                                                          │
│  NIC passthrough:     Tarjeta de red dedicada            │
│                       (rendimiento de red nativo)        │
│                                                          │
│  NVMe passthrough:    SSD NVMe directo a la VM           │
│                       (I/O sin overhead de emulación)    │
│                                                          │
│  HBA passthrough:     Controlador de storage             │
│                       (acceso directo a discos SAS/SATA) │
└──────────────────────────────────────────────────────────┘
```

### 3.2. Requisitos

PCI passthrough tiene requisitos estrictos:

| Requisito | Descripción |
|-----------|-------------|
| IOMMU | La CPU y chipset deben soportar IOMMU (Intel VT-d o AMD-Vi) |
| BIOS/UEFI | IOMMU debe estar habilitado en el BIOS |
| Kernel | Parámetro `intel_iommu=on` o `amd_iommu=on` en GRUB |
| Grupo IOMMU | El dispositivo debe estar en un grupo IOMMU aislable |
| Driver VFIO | El dispositivo debe usar `vfio-pci` en vez de su driver normal |

### 3.3. Flujo general (solo referencia)

```
1. Habilitar IOMMU
   ┌──────────────────────────────────────────────┐
   │ /etc/default/grub:                           │
   │ GRUB_CMDLINE_LINUX="... intel_iommu=on"      │
   │ sudo grub2-mkconfig -o /boot/grub2/grub.cfg  │
   │ reboot                                       │
   └──────────────────────────────────────────────┘
                         │
                         ▼
2. Verificar IOMMU
   ┌──────────────────────────────────────────────┐
   │ dmesg | grep -i iommu                        │
   │ "DMAR: IOMMU enabled"                        │
   └──────────────────────────────────────────────┘
                         │
                         ▼
3. Identificar el dispositivo
   ┌──────────────────────────────────────────────┐
   │ lspci -nn                                    │
   │ "01:00.0 VGA ... [10de:1234]"                │
   └──────────────────────────────────────────────┘
                         │
                         ▼
4. Asignar driver vfio-pci
   ┌──────────────────────────────────────────────┐
   │ Blacklist del driver original                │
   │ Configurar vfio-pci para el dispositivo      │
   │ Regenerar initramfs, reboot                  │
   └──────────────────────────────────────────────┘
                         │
                         ▼
5. Adjuntar a la VM
   ┌──────────────────────────────────────────────┐
   │ virsh edit o virt-install --host-device       │
   │ con la dirección PCI del dispositivo          │
   └──────────────────────────────────────────────┘
```

### 3.4. Ejemplo XML (solo referencia)

```xml
<hostdev mode='subsystem' type='pci' managed='yes'>
  <source>
    <address domain='0x0000' bus='0x01' slot='0x00' function='0x0'/>
  </source>
</hostdev>
```

### 3.5. SR-IOV: alternativa al passthrough completo

SR-IOV (Single Root I/O Virtualization) permite que un dispositivo PCIe se
"divida" en múltiples funciones virtuales (VFs), cada una asignable a una VM
diferente:

```
Sin SR-IOV:                       Con SR-IOV:
┌──────────┐                      ┌──────┐ ┌──────┐ ┌──────┐
│   VM     │                      │ VM-1 │ │ VM-2 │ │ VM-3 │
└────┬─────┘                      └──┬───┘ └──┬───┘ └──┬───┘
     │ passthrough exclusivo         │        │        │
┌────▼─────┐                      ┌──▼────────▼────────▼──┐
│  NIC     │                      │         NIC           │
│ (1 VM)   │                      │  PF   VF0   VF1  VF2  │
└──────────┘                      │(host) (VM1) (VM2)(VM3)│
                                  └───────────────────────┘
```

SR-IOV es común en NICs de servidor (Intel X710, Mellanox) y permite rendimiento
cercano a nativo para múltiples VMs simultáneamente. No lo usaremos en el curso.

---

## 4. Necesidad en el curso

```
┌───────────────────────────────────────────────────────────────┐
│              ¿NECESITO PASSTHROUGH PARA B08?                 │
│                                                               │
│                        NO                                     │
│                                                               │
│  Los discos virtuales (qcow2 + virsh attach-disk) cubren      │
│  todos los labs de:                                           │
│    ✓ Particionado (fdisk, gdisk, parted)                      │
│    ✓ Filesystems (mkfs, mount, fstab)                         │
│    ✓ LVM (pvcreate, vgcreate, lvcreate)                       │
│    ✓ RAID (mdadm)                                             │
│    ✓ Swap                                                     │
│    ✓ Quotas                                                   │
│                                                               │
│  Passthrough es útil si:                                      │
│    - Quieres practicar con un USB drive real (opcional)        │
│    - Necesitas rendimiento nativo de I/O (no es el caso)       │
│    - Usas hardware especializado (no aplica a labs)            │
│                                                               │
│  Para el curso: quédate con discos virtuales.                 │
└───────────────────────────────────────────────────────────────┘
```

### Comparación para labs

| Aspecto | Disco virtual (qcow2) | USB passthrough | PCI passthrough |
|---------|----------------------|-----------------|-----------------|
| Facilidad | Muy fácil | Fácil | Difícil |
| Hardware requerido | Ninguno | USB drive | IOMMU + dispositivo |
| Snapshots | Sí | No (dispositivo real) | No |
| Recrear limpio | Instantáneo | Requiere formatear | Requiere formatear |
| Rendimiento | Suficiente para labs | Nativo | Nativo |
| Recomendado para B08 | **Sí** | Opcional | No |

---

## 5. Errores comunes

### Error 1: intentar passthrough de un dispositivo en uso

```bash
# ✗ El host está usando el USB drive (montado)
virsh attach-device lab-debian01 /tmp/usb.xml
# El dispositivo puede no liberarse correctamente

# ✓ Desmontar en el host primero
sudo umount /media/user/USBDRIVE
# Luego hacer passthrough
virsh attach-device lab-debian01 /tmp/usb.xml --persistent
```

### Error 2: vendor:product incorrecto

```bash
# ✗ IDs inventados o con formato incorrecto
# En XML:
<vendor id='781'/>      # falta 0x
<product id='0x5567'/>

# ✓ Formato correcto: 0x + 4 dígitos hex
<vendor id='0x0781'/>
<product id='0x5567'/>

# Para obtener los IDs correctos:
lsusb
# Bus 002 Device 003: ID 0781:5567 SanDisk Corp.
#                         ^^^^:^^^^ ← estos valores con prefijo 0x
```

### Error 3: confundir passthrough con USB redirect

```bash
# USB passthrough: el dispositivo DEJA de estar en el host
#   → solo la VM lo ve
#   → usa hostdev en XML

# USB redirect (SPICE): el cliente SPICE redirige USB de tu máquina
#   al host y luego a la VM — diferente mecanismo
#   → usa redirdev en XML
#   → requiere virt-viewer con SPICE

# Para labs: passthrough directo es lo correcto
```

### Error 4: PCI passthrough sin IOMMU

```bash
# ✗ Intentar PCI passthrough sin habilitar IOMMU
virsh start vm-with-pci-passthrough
# Error: "IOMMU not found" o "failed to set up VFIO"

# ✓ Verificar IOMMU primero
dmesg | grep -i iommu
# Si no aparece: habilitar en BIOS + kernel parameter
```

### Error 5: asumir que el USB drive será vdX

```bash
# Dentro de la VM, un USB pasado por passthrough aparece como:
#   /dev/sda    (SCSI/USB — su propio bus)
# NO como:
#   /dev/vdb    (virtio — solo para discos virtuales)

# ✓ Usar lsblk para identificar el nombre correcto
lsblk
# sda = USB passthrough
# vdX = disco virtual
```

---

## 6. Cheatsheet

```bash
# ── USB: identificar dispositivo ─────────────────────────────────
lsusb
# ID vendor:product (ej: 0781:5567)

# ── USB: passthrough con virt-install ────────────────────────────
virt-install ... --host-device VENDOR:PRODUCT
# Ejemplo: --host-device 0781:5567

# ── USB: passthrough a VM existente ──────────────────────────────
cat > /tmp/usb.xml << 'EOF'
<hostdev mode='subsystem' type='usb' managed='yes'>
  <source>
    <vendor id='0xVENDOR'/>
    <product id='0xPRODUCT'/>
  </source>
</hostdev>
EOF
virsh attach-device VM /tmp/usb.xml --persistent

# ── USB: retirar ────────────────────────────────────────────────
virsh detach-device VM /tmp/usb.xml

# ── PCI: verificar IOMMU ────────────────────────────────────────
dmesg | grep -i iommu

# ── PCI: identificar dispositivo ────────────────────────────────
lspci -nn
# Buscar dirección PCI: domain:bus:slot.function

# ── PCI: passthrough (XML) ──────────────────────────────────────
# <hostdev mode='subsystem' type='pci' managed='yes'>
#   <source>
#     <address domain='0x0000' bus='0xBUS' slot='0xSLOT'
#              function='0xFUNC'/>
#   </source>
# </hostdev>

# ── Decisión para labs de B08 ───────────────────────────────────
# Discos virtuales (qcow2): RECOMENDADO — fácil, con snapshots
# USB passthrough: OPCIONAL — si quieres probar con USB real
# PCI passthrough: NO NECESARIO — complejidad injustificada
```

---

## 7. Ejercicios

### Ejercicio 1: identificar dispositivos USB

1. Conecta un USB drive a tu máquina (o lista los que ya están conectados)
2. Ejecuta `lsusb` e identifica el `vendor:product` del dispositivo
3. Ejecuta `lsusb -v -d VENDOR:PRODUCT` para ver los detalles completos
4. Crea el archivo XML de `<hostdev>` correspondiente (sin adjuntarlo)

**Pregunta de reflexión**: si tienes dos USB drives idénticos (misma marca y
modelo), ambos tendrán el mismo `vendor:product`. ¿Cómo distinguiría libvirt
cuál pasar a la VM? Investiga el atributo `<address bus='X' device='Y'/>` dentro
de `<source>` como alternativa.

### Ejercicio 2: comparar disco virtual vs concepto de passthrough

1. Crea un disco virtual de 2G y adjúntalo como `vdb` a tu VM
2. Dentro de la VM, ejecuta `lsblk` y observa que aparece como `vdb` (virtio)
3. Ejecuta `ls -la /sys/block/vdb/device/driver` para ver qué driver lo maneja
4. Piensa: si hicieras USB passthrough, el dispositivo aparecería como `sda` con
   un driver diferente

**Pregunta de reflexión**: ¿por qué los discos virtuales usan el bus virtio
(`vdX`) mientras que los USB usan el bus SCSI (`sdX`)? ¿Qué ventaja de
rendimiento tiene virtio sobre la emulación de un controlador USB/SCSI real?

### Ejercicio 3: evaluar necesidad de passthrough

Sin ejecutar nada, responde a estos escenarios. Para cada uno, indica si usarías
disco virtual, USB passthrough, o PCI passthrough:

1. Lab de particionado con `fdisk` — necesitas un disco de 5G para practicar
2. Quieres que la VM acceda a una GPU NVIDIA para machine learning
3. Lab de RAID5 — necesitas 4 discos de 2G
4. Quieres que la VM lea un USB drive con datos reales de tu empresa
5. Lab de `mount` y `umount` — quieres practicar montando particiones

**Pregunta de reflexión**: de los 5 escenarios, ¿en cuáles el disco virtual es
claramente superior? ¿En cuáles el passthrough es la única opción? ¿Cambia la
respuesta si consideras que el lab debe ser reproducible y fácil de resetear?
