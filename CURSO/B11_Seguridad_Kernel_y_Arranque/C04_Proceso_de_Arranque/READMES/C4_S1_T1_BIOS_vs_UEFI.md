# BIOS vs UEFI

## Índice
1. [¿Qué es el firmware?](#qué-es-el-firmware)
2. [BIOS: el firmware clásico](#bios-el-firmware-clásico)
3. [UEFI: el sucesor moderno](#uefi-el-sucesor-moderno)
4. [Comparación directa BIOS vs UEFI](#comparación-directa-bios-vs-uefi)
5. [Particionado: MBR vs GPT](#particionado-mbr-vs-gpt)
6. [EFI System Partition (ESP)](#efi-system-partition-esp)
7. [Secure Boot](#secure-boot)
8. [Identificar tu firmware](#identificar-tu-firmware)
9. [Interacción con el bootloader](#interacción-con-el-bootloader)
10. [Errores comunes](#errores-comunes)
11. [Cheatsheet](#cheatsheet)
12. [Ejercicios](#ejercicios)

---

## ¿Qué es el firmware?

El firmware es el primer software que ejecuta tu computadora al encenderse. Está grabado en un chip de la placa base y su trabajo es:

1. Inicializar el hardware (CPU, RAM, discos, periféricos)
2. Ejecutar diagnósticos básicos (POST — Power-On Self-Test)
3. Encontrar y transferir el control al bootloader

```
                    FLUJO DE ARRANQUE SIMPLIFICADO

  ┌──────────┐     ┌──────────┐     ┌──────────┐     ┌──────────┐
  │ Encender │────▶│ Firmware │────▶│Bootloader│────▶│  Kernel  │
  │   (PSU)  │     │BIOS/UEFI│     │ (GRUB2)  │     │  Linux   │
  └──────────┘     └──────────┘     └──────────┘     └──────────┘
                    POST + buscar    Cargar kernel     initramfs
                    dispositivo      + initramfs       → init
                    de arranque
```

Existen dos tipos de firmware: **BIOS** (legado) y **UEFI** (moderno). Entender sus diferencias es fundamental para administrar el arranque de sistemas Linux.

---

## BIOS: el firmware clásico

**BIOS** (Basic Input/Output System) fue creado por IBM en 1981 para el PC original. A pesar de su antigüedad, sigue presente en hardware antiguo y máquinas virtuales.

### Cómo arranca BIOS

```
  ┌─────────────────────────────────────────────────────────┐
  │                    DISCO CON MBR                         │
  │                                                         │
  │  Sector 0 (512 bytes)                                   │
  │  ┌────────────────────────────────────────────────────┐  │
  │  │ Bootstrap code (446 bytes)                         │  │
  │  │  → Código del bootloader (stage 1)                 │  │
  │  ├────────────────────────────────────────────────────┤  │
  │  │ Partition table (64 bytes)                         │  │
  │  │  → 4 entradas × 16 bytes = 4 particiones máx      │  │
  │  ├────────────────────────────────────────────────────┤  │
  │  │ Boot signature: 0x55AA (2 bytes)                   │  │
  │  └────────────────────────────────────────────────────┘  │
  │                                                         │
  │  Partición 1 (activa/boot)     Partición 2   ...        │
  │  ┌──────────────────────┐     ┌───────────┐             │
  │  │ GRUB stage 1.5 + 2   │     │   datos   │             │
  │  │ /boot/               │     │           │             │
  │  └──────────────────────┘     └───────────┘             │
  └─────────────────────────────────────────────────────────┘
```

El proceso paso a paso:

1. **POST**: el BIOS prueba RAM, detecta CPU, enumera dispositivos
2. **Orden de arranque**: el BIOS consulta su configuración (guardada en CMOS/NVRAM) para determinar qué disco intentar primero
3. **Lectura del MBR**: lee los primeros 512 bytes del disco seleccionado
4. **Verificación**: comprueba la firma `0x55AA` en los últimos 2 bytes
5. **Ejecución**: transfiere el control al bootstrap code (446 bytes)
6. **Bootloader**: el bootstrap code (GRUB stage 1) es demasiado pequeño para todo, así que carga stages adicionales desde el disco

### Limitaciones de BIOS

| Limitación | Detalle |
|---|---|
| **Modo real 16-bit** | Solo accede a 1 MB de RAM durante el arranque |
| **MBR = 4 particiones** | Máximo 4 primarias (3 + 1 extendida como workaround) |
| **Disco ≤ 2 TB** | MBR usa direcciones de 32 bits × 512 bytes = 2 TiB |
| **Sin verificación** | No hay forma de verificar que el bootloader es legítimo |
| **Bootstrap = 446 bytes** | Código del bootloader extremadamente limitado |
| **Sin red** | No puede arrancar desde red sin extensiones (PXE) |
| **Interfaz texto** | Configuración solo con teclado, sin ratón |

### CMOS y batería

BIOS guarda su configuración (orden de arranque, fecha/hora, contraseña del setup) en una memoria CMOS alimentada por una batería CR2032. Si la batería se agota, la configuración se pierde y el BIOS vuelve a los valores por defecto.

---

## UEFI: el sucesor moderno

**UEFI** (Unified Extensible Firmware Interface) fue diseñado por Intel (como EFI) en los años 90, estandarizado como UEFI en 2005 por el UEFI Forum. Prácticamente todo hardware fabricado después de 2012 usa UEFI.

### Cómo arranca UEFI

```
  ┌──────────────────────────────────────────────────────────┐
  │                   DISCO CON GPT                          │
  │                                                          │
  │  ┌─────────────┐  ┌──────────────────┐  ┌────────────┐  │
  │  │ Protective  │  │  ESP (EFI System  │  │ Partición  │  │
  │  │ MBR         │  │  Partition)       │  │ Linux /    │  │
  │  │ (compat.)   │  │                  │  │            │  │
  │  │             │  │  FAT32, ~512 MB  │  │ ext4/xfs   │  │
  │  │             │  │                  │  │            │  │
  │  │             │  │  /EFI/           │  │            │  │
  │  │             │  │  ├── fedora/     │  │            │  │
  │  │             │  │  │   └── shimx64 │  │            │  │
  │  │             │  │  │       .efi    │  │            │  │
  │  │             │  │  ├── ubuntu/     │  │            │  │
  │  │             │  │  └── BOOT/       │  │            │  │
  │  │             │  │      └── BOOTX64 │  │            │  │
  │  │             │  │          .efi    │  │            │  │
  │  └─────────────┘  └──────────────────┘  └────────────┘  │
  └──────────────────────────────────────────────────────────┘
```

El proceso paso a paso:

1. **POST + SEC (Security Phase)**: inicialización temprana del CPU, verificación de firmware
2. **PEI (Pre-EFI Initialization)**: detecta RAM, inicializa chipset mínimo
3. **DXE (Driver Execution Environment)**: carga drivers UEFI (disco, red, USB, gráficos)
4. **BDS (Boot Device Selection)**: consulta las variables de arranque en NVRAM
5. **Localización del bootloader**: busca el archivo `.efi` en la ESP según la entrada seleccionada
6. **Secure Boot** (si habilitado): verifica la firma digital del bootloader
7. **Ejecución**: transfiere el control al bootloader EFI (ej: `shimx64.efi` → `grubx64.efi`)

### Ventajas de UEFI

| Característica | Detalle |
|---|---|
| **Modo protegido 32/64-bit** | Accede a toda la RAM durante el arranque |
| **GPT** | 128 particiones por defecto, discos de hasta 9.4 ZB |
| **Secure Boot** | Verifica firmas criptográficas del bootloader |
| **NVRAM variables** | Configuración persistente sin batería CMOS |
| **Drivers modulares** | El firmware puede cargar drivers desde la ESP |
| **Interfaz gráfica** | Menú con ratón, resolución alta |
| **Red nativa** | Arranque por red (HTTP boot, PXE) integrado |
| **Boot Manager integrado** | Puede arrancar archivos `.efi` directamente, sin MBR |
| **Shell UEFI** | Línea de comandos para diagnóstico y reparación |

### Variables NVRAM

UEFI almacena sus variables de configuración (entradas de arranque, orden de boot, claves de Secure Boot) en memoria flash NVRAM en la placa base. A diferencia del CMOS de BIOS, no depende de una batería.

Puedes inspeccionar y modificar las variables desde Linux:

```bash
# List boot entries
efibootmgr -v

# Sample output:
# BootCurrent: 0001
# Timeout: 3 seconds
# BootOrder: 0001,0000,0002
# Boot0000* Windows Boot Manager  HD(1,GPT,...)/File(\EFI\Microsoft\Boot\bootmgfw.efi)
# Boot0001* Fedora                HD(1,GPT,...)/File(\EFI\fedora\shimx64.efi)
# Boot0002* USB Drive             ...

# Change boot order (Fedora first, then Windows)
sudo efibootmgr -o 0001,0000

# Set next boot only (one-time override)
sudo efibootmgr -n 0002

# Create a new boot entry
sudo efibootmgr -c -d /dev/sda -p 1 -L "Custom Linux" \
    -l '\EFI\custom\grubx64.efi'

# Delete a boot entry
sudo efibootmgr -b 0003 -B
```

> **Precaución**: modificar variables UEFI incorrectamente puede hacer que el sistema no arranque. Siempre verifica el estado con `efibootmgr -v` antes y después de cambios.

### Compatibility Support Module (CSM)

Muchos firmwares UEFI incluyen un modo de compatibilidad llamado **CSM** que emula BIOS para arrancar sistemas operativos que no soportan UEFI. Cuando CSM está habilitado, el firmware puede arrancar desde MBR como si fuera BIOS.

```
  ┌────────────────────────────────────────────────┐
  │                FIRMWARE UEFI                    │
  │                                                │
  │  ┌──────────────┐    ┌──────────────────────┐  │
  │  │  Modo UEFI   │    │    Modo CSM (BIOS)   │  │
  │  │  nativo      │    │    emulado           │  │
  │  │              │    │                      │  │
  │  │  ESP + GPT   │    │  MBR + bootstrap     │  │
  │  │  Secure Boot │    │  Sin Secure Boot     │  │
  │  │  64-bit      │    │  16-bit real mode    │  │
  │  └──────────────┘    └──────────────────────┘  │
  └────────────────────────────────────────────────┘
```

**Recomendación**: siempre instala Linux en modo UEFI nativo. CSM solo debe usarse si el hardware o el OS lo requieren. Muchos fabricantes están eliminando CSM de los firmwares nuevos.

---

## Comparación directa BIOS vs UEFI

| Aspecto | BIOS | UEFI |
|---|---|---|
| **Año** | 1981 | 2005 (estándar) |
| **Modo CPU** | 16-bit real | 32/64-bit protegido |
| **Esquema partición** | MBR | GPT |
| **Particiones máx** | 4 primarias | 128 (por defecto) |
| **Tamaño disco máx** | 2 TiB | 9.4 ZiB |
| **Almacén config** | CMOS + batería | NVRAM flash |
| **Bootloader** | MBR bootstrap (446 B) | Archivo `.efi` en ESP |
| **Verificación** | Ninguna | Secure Boot |
| **Interfaz config** | Texto, teclado | GUI, ratón (típico) |
| **Arranque por red** | PXE (extensión) | HTTP Boot + PXE nativo |
| **Tiempo POST** | Lento (escaneo serial) | Rápido (parallel init) |
| **Multi-boot** | Depende del bootloader | Boot manager integrado |
| **Herramienta Linux** | – | `efibootmgr` |
| **Shell integrada** | No | UEFI Shell |

---

## Particionado: MBR vs GPT

El firmware determina qué esquema de particiones usa el disco de arranque. BIOS usa MBR; UEFI usa GPT.

### MBR (Master Boot Record)

```
  Byte 0                                           Byte 511
  ┌──────────────────────────┬──────────────┬────┐
  │   Bootstrap code         │  Partition   │Sig │
  │   (446 bytes)            │  Table       │    │
  │                          │  (64 bytes)  │55AA│
  └──────────────────────────┴──────────────┴────┘
                               │
                   ┌───────────┼───────────┐
                   ▼           ▼           ▼
              Entrada 1   Entrada 2   Entrada 3  Entrada 4
              16 bytes    16 bytes    16 bytes   16 bytes
              ┌────────┐
              │ Active │ ← Flag 0x80 = bootable
              │ CHS ini│
              │ Type   │ ← 0x83=Linux, 0x82=swap, 0x05=extended
              │ CHS fin│
              │ LBA ini│
              │ Sectors│
              └────────┘
```

- Cada entrada tiene un byte de tipo: `0x83` = Linux, `0x82` = swap, `0x05` = extended, `0x07` = NTFS
- Solo una partición puede estar marcada como **activa** (bootable)
- La partición extendida contiene particiones lógicas encadenadas

### GPT (GUID Partition Table)

```
  ┌──────────────────────────────────────────────┐
  │ LBA 0: Protective MBR                        │ ← Compatibilidad
  ├──────────────────────────────────────────────┤
  │ LBA 1: GPT Header                            │
  │   Disk GUID, nº entradas, CRC32              │
  ├──────────────────────────────────────────────┤
  │ LBA 2-33: Partition entries                   │
  │   128 bytes/entrada × 128 = 16384 bytes       │
  │   Cada entrada:                               │
  │     - Partition Type GUID                     │
  │     - Unique Partition GUID                   │
  │     - First/Last LBA (64-bit)                 │
  │     - Attributes + Name (36 UTF-16 chars)     │
  ├──────────────────────────────────────────────┤
  │ LBA 34+: Particiones de datos                 │
  │   ...                                         │
  ├──────────────────────────────────────────────┤
  │ LBA -33 to -2: Backup partition entries       │ ← Redundancia
  ├──────────────────────────────────────────────┤
  │ LBA -1: Backup GPT Header                    │ ← Redundancia
  └──────────────────────────────────────────────┘
```

Ventajas de GPT:
- **Protective MBR** en LBA 0 evita que herramientas antiguas sobrescriban el disco
- **Redundancia**: header y tabla de particiones duplicados al final del disco
- **CRC32**: integridad verificada tanto del header como de las entradas
- **128 particiones** por defecto (no hay concepto de primaria/extendida/lógica)
- **Direcciones LBA de 64-bit**: soporta discos de hasta 9.4 ZB
- **GUIDs**: cada disco y partición tiene un identificador único universal

### Inspeccionar particiones

```bash
# View MBR partition table
sudo fdisk -l /dev/sda

# View GPT partition table
sudo gdisk -l /dev/sda

# Works with both (recommended)
sudo parted /dev/sda print
lsblk -o NAME,SIZE,TYPE,FSTYPE,PARTTYPE,MOUNTPOINT

# Check if disk uses GPT or MBR
sudo blkid /dev/sda
# PTUUID="..." PTTYPE="gpt"   ← GPT
# PTUUID="..." PTTYPE="dos"   ← MBR
```

---

## EFI System Partition (ESP)

La ESP es una partición especial requerida por UEFI. Contiene los bootloaders y archivos necesarios para el arranque.

### Características de la ESP

| Propiedad | Valor |
|---|---|
| **Filesystem** | FAT32 (obligatorio por especificación UEFI) |
| **Tamaño típico** | 256 MB – 1 GB |
| **Partition Type GUID** | `C12A7328-F81F-11D2-BA4B-00A0C93EC93B` |
| **Punto de montaje Linux** | `/boot/efi` (RHEL/Fedora) o `/efi` (moderno) |
| **Flag en parted** | `boot` (que en GPT realmente significa `esp`) |

### Estructura interna

```bash
# Mount point: /boot/efi
/boot/efi/
└── EFI/
    ├── BOOT/
    │   └── BOOTX64.EFI          # Fallback bootloader (removable media)
    ├── fedora/
    │   ├── shimx64.efi           # Shim (Secure Boot first stage)
    │   ├── grubx64.efi           # GRUB2 EFI binary
    │   ├── grub.cfg              # GRUB config (minimal, points to /boot)
    │   └── fonts/
    ├── ubuntu/
    │   ├── shimx64.efi
    │   ├── grubx64.efi
    │   └── grub.cfg
    └── Microsoft/
        └── Boot/
            └── bootmgfw.efi      # Windows Boot Manager
```

### Gestionar la ESP

```bash
# Find the ESP
lsblk -o NAME,FSTYPE,PARTTYPE,MOUNTPOINT | grep -i efi
findmnt /boot/efi

# Verify ESP contents
ls -la /boot/efi/EFI/

# Check ESP free space (FAT32 can fill up with old kernels)
df -h /boot/efi

# Mount ESP manually (if not mounted)
sudo mount /dev/sda1 /boot/efi

# fstab entry for ESP (typical)
# UUID=ABCD-1234  /boot/efi  vfat  umask=0077,shortname=winnt  0 2
```

### BOOTX64.EFI: el bootloader de respaldo

`/EFI/BOOT/BOOTX64.EFI` es el path que UEFI busca como **fallback** cuando no tiene entradas de arranque configuradas (por ejemplo, en un USB de instalación). Si tu sistema no arranca después de manipular las variables UEFI, la existencia de este archivo puede salvarte.

---

## Secure Boot

Secure Boot es una característica de UEFI que verifica criptográficamente cada componente de la cadena de arranque antes de ejecutarlo.

### Cadena de confianza

```
  ┌─────────────────────────────────────────────────────────────┐
  │                    SECURE BOOT CHAIN                         │
  │                                                             │
  │  ┌──────────┐ firma  ┌───────────┐ firma  ┌──────────────┐ │
  │  │ Platform │───────▶│   shim    │───────▶│   GRUB2      │ │
  │  │ Key (PK) │ verif. │ (firmado  │ verif. │ (firmado por │ │
  │  │          │        │  por MS)  │        │  distro)     │ │
  │  └──────────┘        └───────────┘        └──────┬───────┘ │
  │                                                   │         │
  │                                            firma  │ verif.  │
  │                                                   ▼         │
  │                                           ┌──────────────┐  │
  │                                           │   vmlinuz    │  │
  │                                           │ (firmado por │  │
  │                                           │  distro)     │  │
  │                                           └──────────────┘  │
  └─────────────────────────────────────────────────────────────┘
```

### Base de datos de claves

UEFI Secure Boot mantiene varias bases de datos en NVRAM:

| Base de datos | Nombre | Contenido |
|---|---|---|
| **PK** | Platform Key | Clave del fabricante (1 sola). Controla quién modifica KEK |
| **KEK** | Key Exchange Key | Claves autorizadas para modificar db/dbx |
| **db** | Signature Database | Claves/hashes de binarios **permitidos** |
| **dbx** | Forbidden Database | Claves/hashes de binarios **prohibidos** (revocados) |

### El rol de shim

Las distribuciones Linux no pueden firmar sus bootloaders directamente con la clave de Microsoft (que está en `db` de casi todos los PCs). En su lugar usan **shim**:

1. **Microsoft** firma `shimx64.efi` (porque shim es un proyecto simple y auditado)
2. **Shim** contiene la clave pública de la distribución (ej: clave de Red Hat, Canonical)
3. **Shim** verifica que `grubx64.efi` esté firmado por la distro
4. **GRUB** verifica que `vmlinuz` esté firmado por la distro

### Machine Owner Key (MOK)

Shim introduce un mecanismo adicional: **MOK** (Machine Owner Key), que permite al usuario agregar sus propias claves (por ejemplo, para módulos de kernel personalizados como los de VirtualBox o NVIDIA):

```bash
# Check Secure Boot status
mokutil --sb-state
# SecureBoot enabled

# List enrolled MOK keys
mokutil --list-enrolled

# Import a new key (requires reboot + confirmation)
sudo mokutil --import my_signing_key.der
# Enter a one-time password → reboot → MokManager asks to confirm

# Check if a specific key is enrolled
mokutil --test-key my_signing_key.der
```

### Implicaciones para sysadmins

```bash
# Verify Secure Boot status from Linux
cat /sys/firmware/efi/efivars/SecureBoot-*/data | xxd | head -1
# ... 01 = enabled, 00 = disabled

# Simpler check
mokutil --sb-state

# If Secure Boot is on, unsigned kernel modules WILL FAIL to load:
# modprobe: ERROR: could not insert 'vboxdrv': Required key not available

# Solutions:
# 1. Sign the module with a MOK key
# 2. Disable Secure Boot (not recommended for production)
# 3. Use DKMS with sign-tool configured
```

---

## Identificar tu firmware

### ¿BIOS o UEFI?

```bash
# Method 1: check for EFI directory
ls /sys/firmware/efi/
# If exists → UEFI; if "No such file" → BIOS (or UEFI with CSM)

# Method 2: dmesg
dmesg | grep -i "efi:"
# [    0.000000] efi: EFI v2.70 by American Megatrends
# [    0.000000] efi: SMBIOS=0x... ACPI 2.0=0x... MEMATTR=0x...

# Method 3: efibootmgr (only works under UEFI)
efibootmgr
# If works → UEFI; if error → BIOS

# Method 4: check boot mode recorded in kernel
cat /proc/cmdline
# Look for "efi" or "BOOT_IMAGE" path starting with /boot/efi

# Method 5: dmidecode (works in both)
sudo dmidecode -t bios
# BIOS Information
#   Vendor: American Megatrends Inc.
#   Version: ...
#   BIOS Revision: 5.17     ← Actual BIOS
#   UEFI is supported       ← Or this
```

### Información del firmware

```bash
# Detailed UEFI firmware info
efivar -l                              # List all EFI variables
efivar -p -n 8be4df61-...-BootOrder   # Print specific variable

# Firmware version and vendor
sudo dmidecode -t 0   # BIOS/UEFI info
sudo dmidecode -t 1   # System info

# ACPI tables (provided by firmware)
ls /sys/firmware/acpi/tables/
```

---

## Interacción con el bootloader

El firmware y el bootloader tienen una relación estrecha pero diferente según el tipo:

### BIOS + GRUB2

```
  BIOS
    │
    ▼
  MBR (sector 0)
    │ boot.img (Stage 1, 446 bytes)
    ▼
  Post-MBR gap o partición BIOS boot
    │ core.img (Stage 1.5, ~32 KB)
    │ Contiene drivers para leer filesystem
    ▼
  /boot/grub2/
    │ Módulos, grub.cfg, themes
    │ (Stage 2 completo)
    ▼
  kernel + initramfs
```

Para BIOS, GRUB necesita instalarse **en el disco** (no solo en el filesystem):

```bash
# Install GRUB to MBR of /dev/sda
sudo grub2-install /dev/sda    # RHEL
sudo grub-install /dev/sda     # Debian

# This writes:
# 1. boot.img → MBR sector 0 (446 bytes)
# 2. core.img → post-MBR gap (sectors 1-62)
```

> **BIOS boot partition**: en discos GPT con BIOS (combinación rara pero posible), GRUB necesita una partición especial de ~1 MB sin filesystem (tipo `ef02`) para almacenar `core.img`, ya que no existe el "post-MBR gap".

### UEFI + GRUB2

```
  UEFI firmware
    │
    ▼
  NVRAM boot entry: "Fedora → /EFI/fedora/shimx64.efi"
    │
    ▼
  ESP:/EFI/fedora/shimx64.efi
    │ (Secure Boot verification)
    ▼
  ESP:/EFI/fedora/grubx64.efi
    │
    ▼
  ESP:/EFI/fedora/grub.cfg (minimal)
    │ "configfile /boot/grub2/grub.cfg" (redirect)
    ▼
  /boot/grub2/grub.cfg (full config)
    │
    ▼
  /boot/vmlinuz-* + /boot/initramfs-*
```

Para UEFI, GRUB se instala **en la ESP** como un archivo `.efi`:

```bash
# Install GRUB to ESP (UEFI)
sudo grub2-install --target=x86_64-efi --efi-directory=/boot/efi \
    --bootloader-id=fedora

# This:
# 1. Copies grubx64.efi → /boot/efi/EFI/fedora/
# 2. Creates NVRAM boot entry via efibootmgr
```

---

## Errores comunes

### 1. Instalar Linux en modo UEFI pero configurar GRUB para BIOS (o viceversa)

Si el instalador arrancó en modo UEFI pero GRUB se instaló para BIOS, el sistema no arrancará. Siempre verifica que el medio de instalación arrancó en el mismo modo que deseas para el sistema instalado:

```bash
# During installation, check:
ls /sys/firmware/efi/    # Exists = UEFI mode
```

### 2. Confundir "Secure Boot deshabilitado" con "firmware es BIOS"

Secure Boot es una **característica** de UEFI que puede habilitarse o deshabilitarse. Deshabilitar Secure Boot **no** convierte UEFI en BIOS. El sistema sigue siendo UEFI, sigue usando ESP y GPT.

### 3. Llenar la ESP con kernels antiguos

Algunas distribuciones copian kernels a la ESP. Con el tiempo, la ESP (FAT32, típicamente 256-512 MB) se llena. Limpia kernels antiguos regularmente:

```bash
# Check ESP usage
df -h /boot/efi

# List old kernel files in ESP (distro-specific)
ls /boot/efi/EFI/*/

# On Fedora, clean old kernels
sudo dnf remove --oldinstallonly
```

### 4. Borrar variables UEFI con `rm -rf /sys/firmware/efi/efivars/`

Las variables EFI aparecen como archivos en `/sys/firmware/efi/efivars/`. Borrarlas puede **brickear** el firmware de algunas placas base. Linux monta este directorio como **inmutable** por defecto como protección, pero nunca intentes forzar la eliminación.

### 5. No tener partición BIOS boot al usar GPT con BIOS

Si usas un disco GPT en un sistema BIOS (sin UEFI), GRUB necesita una partición BIOS boot (`ef02`, 1 MB, sin filesystem) porque no hay post-MBR gap en GPT. Sin ella, `grub-install` falla.

---

## Cheatsheet

```bash
# ═══════════════════════════════════════════════
#  DETECCIÓN DEL FIRMWARE
# ═══════════════════════════════════════════════
ls /sys/firmware/efi/               # Exists = UEFI
mokutil --sb-state                  # Secure Boot status
sudo dmidecode -t bios              # Firmware vendor/version

# ═══════════════════════════════════════════════
#  GESTIÓN UEFI (efibootmgr)
# ═══════════════════════════════════════════════
efibootmgr -v                       # List all boot entries (verbose)
sudo efibootmgr -o 0001,0000        # Change boot order
sudo efibootmgr -n 0002             # Next boot only (one-time)
sudo efibootmgr -c -d /dev/sda \    # Create new entry
    -p 1 -L "MyLinux" -l '\EFI\custom\grubx64.efi'
sudo efibootmgr -b 0003 -B          # Delete entry 0003

# ═══════════════════════════════════════════════
#  EFI SYSTEM PARTITION (ESP)
# ═══════════════════════════════════════════════
findmnt /boot/efi                    # Find ESP mount
ls /boot/efi/EFI/                    # List installed bootloaders
df -h /boot/efi                      # Check ESP free space

# ═══════════════════════════════════════════════
#  SECURE BOOT / MOK
# ═══════════════════════════════════════════════
mokutil --sb-state                   # Enabled/Disabled
mokutil --list-enrolled              # List MOK keys
sudo mokutil --import key.der        # Import key (reboot needed)

# ═══════════════════════════════════════════════
#  PARTICIONADO
# ═══════════════════════════════════════════════
sudo blkid /dev/sda                  # PTTYPE=gpt or dos
sudo parted /dev/sda print           # Partition table
sudo gdisk -l /dev/sda               # GPT details
sudo fdisk -l /dev/sda               # MBR/GPT basic view

# ═══════════════════════════════════════════════
#  GRUB INSTALL
# ═══════════════════════════════════════════════
# BIOS:
sudo grub2-install /dev/sda
# UEFI:
sudo grub2-install --target=x86_64-efi \
    --efi-directory=/boot/efi --bootloader-id=fedora
```

---

## Ejercicios

### Ejercicio 1: Identificar tu firmware y particionado

Ejecuta los siguientes comandos y responde las preguntas:

```bash
# Step 1: determine firmware type
ls /sys/firmware/efi/ 2>/dev/null && echo "UEFI" || echo "BIOS/CSM"

# Step 2: check Secure Boot (only if UEFI)
mokutil --sb-state 2>/dev/null

# Step 3: check partition table type
sudo blkid /dev/sda    # or /dev/nvme0n1

# Step 4: find the ESP (only if UEFI)
lsblk -o NAME,FSTYPE,SIZE,MOUNTPOINT | grep -i vfat

# Step 5: list UEFI boot entries
efibootmgr -v 2>/dev/null
```

**Preguntas:**
1. ¿Tu sistema es BIOS o UEFI?
2. ¿Secure Boot está habilitado?
3. ¿Tu disco usa MBR o GPT?
4. ¿Cuánto espacio libre tiene tu ESP?
5. ¿Cuántas entradas de arranque tienes y cuál es la activa?

> **Pregunta de predicción**: si tu sistema es UEFI con GPT y ejecutas `sudo fdisk -l`, ¿aparecerá una partición de tipo "EFI System"? ¿Y si ejecutas `sudo gdisk -l`? ¿Qué diferencia esperas en la salida de ambos comandos?

### Ejercicio 2: Explorar la ESP

```bash
# Step 1: verify ESP is mounted
findmnt /boot/efi

# Step 2: explore structure
sudo find /boot/efi -type f -name "*.efi" | sort

# Step 3: identify each bootloader
file /boot/efi/EFI/fedora/shimx64.efi    # or your distro
file /boot/efi/EFI/BOOT/BOOTX64.EFI

# Step 4: check if fallback bootloader exists
ls -la /boot/efi/EFI/BOOT/

# Step 5: compare sizes
ls -lh /boot/efi/EFI/fedora/*.efi

# Step 6: check ESP in fstab
grep -i efi /etc/fstab
```

**Preguntas:**
1. ¿Cuántos directorios hay dentro de `/boot/efi/EFI/`? ¿Qué OS/distro representa cada uno?
2. ¿El archivo `BOOTX64.EFI` es idéntico al `shimx64.efi` de tu distro? Compáralos con `sha256sum`
3. ¿Qué opciones de montaje tiene la ESP en fstab?

> **Pregunta de predicción**: si borras `/boot/efi/EFI/fedora/shimx64.efi` pero la entrada NVRAM sigue apuntando a él, ¿qué pasará al reiniciar? ¿El firmware caerá al `BOOTX64.EFI` de respaldo automáticamente?

### Ejercicio 3: Simular la pérdida y recuperación de una entrada UEFI

> **ADVERTENCIA**: este ejercicio modifica variables UEFI. Hazlo solo en una VM o si tienes acceso físico al firmware.

```bash
# Step 1: backup current boot entries
efibootmgr -v > ~/efi_backup.txt
cat ~/efi_backup.txt

# Step 2: note the current boot entry number for your distro
# Example: Boot0001* Fedora  HD(...)/File(\EFI\fedora\shimx64.efi)
ENTRY=0001   # adjust to your system

# Step 3: create a duplicate entry as safety net
sudo efibootmgr -c -d /dev/sda -p 1 -L "Fedora Backup" \
    -l '\EFI\fedora\shimx64.efi'

# Step 4: verify the new entry exists
efibootmgr -v

# Step 5: delete the ORIGINAL entry (the duplicate remains)
sudo efibootmgr -b $ENTRY -B

# Step 6: verify — your distro's original entry is gone
efibootmgr -v
# The "Fedora Backup" entry should still be there

# Step 7: restore by creating the entry with original name
sudo efibootmgr -c -d /dev/sda -p 1 -L "Fedora" \
    -l '\EFI\fedora\shimx64.efi'

# Step 8: clean up the backup entry
# Find its number and delete it
efibootmgr -v
sudo efibootmgr -b XXXX -B    # replace XXXX

# Step 9: set correct boot order
sudo efibootmgr -o YYYY,ZZZZ  # your desired order
```

**Preguntas:**
1. ¿La entrada "Fedora Backup" apunta al mismo archivo `.efi`? ¿Cómo sabe UEFI que son diferentes?
2. ¿Qué hubiera pasado si borrabas la entrada original **sin** crear el backup?

> **Pregunta de predicción**: si tienes Secure Boot habilitado y creas una entrada que apunta a un archivo `.efi` sin firmar, ¿el firmware lo rechazará al intentar arrancar? ¿O la verificación ocurre después de que el firmware carga el archivo?

---

> **Siguiente tema**: T02 — GRUB2: instalación, grub.cfg, grub-mkconfig, GRUB_DEFAULT, parámetros del kernel.
