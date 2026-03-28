# DKMS

## Índice
1. [¿Qué es DKMS?](#qué-es-dkms)
2. [El problema que resuelve](#el-problema-que-resuelve)
3. [Arquitectura de DKMS](#arquitectura-de-dkms)
4. [Ciclo de vida de un módulo DKMS](#ciclo-de-vida-de-un-módulo-dkms)
5. [Comandos de DKMS](#comandos-de-dkms)
6. [dkms.conf: configuración del módulo](#dkmsconf-configuración-del-módulo)
7. [DKMS y Secure Boot](#dkms-y-secure-boot)
8. [Módulos DKMS comunes](#módulos-dkms-comunes)
9. [Crear un módulo DKMS propio](#crear-un-módulo-dkms-propio)
10. [Errores comunes](#errores-comunes)
11. [Cheatsheet](#cheatsheet)
12. [Ejercicios](#ejercicios)

---

## ¿Qué es DKMS?

**DKMS** (Dynamic Kernel Module Support) es un framework que permite compilar automáticamente módulos del kernel **fuera del árbol** (out-of-tree) cada vez que se instala un kernel nuevo. Fue creado por Dell en 2003.

```
  ┌──────────────────────────────────────────────────────────────┐
  │                                                              │
  │  Sin DKMS:                                                   │
  │  ┌────────────────────────────────────────────────────────┐  │
  │  │ 1. Instalas driver NVIDIA para kernel 6.7.4            │  │
  │  │ 2. Actualizas a kernel 6.8.0                           │  │
  │  │ 3. Driver NVIDIA deja de funcionar                     │  │
  │  │    (compilado para 6.7.4, incompatible con 6.8.0)      │  │
  │  │ 4. Tienes que recompilar manualmente el driver         │  │
  │  │ 5. Si olvidas → pantalla negra al arrancar             │  │
  │  └────────────────────────────────────────────────────────┘  │
  │                                                              │
  │  Con DKMS:                                                   │
  │  ┌────────────────────────────────────────────────────────┐  │
  │  │ 1. Instalas driver NVIDIA con DKMS                     │  │
  │  │ 2. Actualizas a kernel 6.8.0                           │  │
  │  │ 3. DKMS detecta el nuevo kernel automáticamente        │  │
  │  │ 4. Recompila el driver para 6.8.0                      │  │
  │  │ 5. Driver funciona sin intervención manual              │  │
  │  └────────────────────────────────────────────────────────┘  │
  │                                                              │
  └──────────────────────────────────────────────────────────────┘
```

---

## El problema que resuelve

Los módulos del kernel están vinculados a una versión exacta. Un módulo `.ko` compilado para `6.7.4-200.fc39` no carga en `6.8.0-100.fc40`:

```
  ┌────────────────────────────────────────────────────────────┐
  │                                                            │
  │  /lib/modules/6.7.4-200.fc39/                              │
  │  └── extra/nvidia.ko    ← Funciona en 6.7.4               │
  │                            vermagic: 6.7.4-200.fc39        │
  │                                                            │
  │  Kernel 6.8.0-100.fc40:                                    │
  │  modprobe nvidia                                           │
  │  → ERROR: Invalid module format                            │
  │  → vermagic mismatch: 6.7.4 ≠ 6.8.0                       │
  │                                                            │
  │  DKMS guarda el CÓDIGO FUENTE del módulo, no el .ko        │
  │  → Puede recompilar para cualquier kernel nuevo            │
  │                                                            │
  └────────────────────────────────────────────────────────────┘
```

DKMS almacena el **código fuente** del módulo en `/usr/src/` y lo recompila automáticamente cuando detecta un nuevo kernel. El trigger viene del script post-install del paquete del kernel.

---

## Arquitectura de DKMS

```
  ┌──────────────────────────────────────────────────────────────┐
  │                     DKMS Architecture                        │
  │                                                              │
  │  /usr/src/<module>-<version>/                                │
  │  ├── dkms.conf              ← Instrucciones de compilación  │
  │  ├── Makefile               ← Build system                  │
  │  ├── *.c / *.h              ← Código fuente                 │
  │  └── ...                                                     │
  │                                                              │
  │  /var/lib/dkms/<module>/<version>/                           │
  │  ├── source → /usr/src/<module>-<version>/  (symlink)       │
  │  ├── <kernel-version>/                                       │
  │  │   └── x86_64/                                             │
  │  │       ├── module/                                         │
  │  │       │   └── <module>.ko.xz    ← Módulo compilado       │
  │  │       └── log/                                            │
  │  │           ├── make.log          ← Log de compilación      │
  │  │           └── ...                                         │
  │  └── ...                                                     │
  │                                                              │
  │  /lib/modules/<kernel-version>/                              │
  │  └── updates/dkms/                                           │
  │      └── <module>.ko.xz    ← Symlink o copia del .ko        │
  │                                                              │
  └──────────────────────────────────────────────────────────────┘
```

### Directorios clave

| Directorio | Contenido |
|---|---|
| `/usr/src/<mod>-<ver>/` | Código fuente + dkms.conf |
| `/var/lib/dkms/<mod>/` | Estado, módulos compilados, logs |
| `/lib/modules/<kver>/updates/dkms/` | Módulos instalados (los que modprobe encuentra) |

---

## Ciclo de vida de un módulo DKMS

Un módulo DKMS pasa por 5 estados:

```
  ┌─────────┐    ┌─────────┐    ┌─────────┐    ┌───────────┐
  │  added  │───▶│  built  │───▶│installed│───▶│  (loaded)  │
  │         │    │         │    │         │    │  in kernel │
  └─────────┘    └─────────┘    └───────┬─┘    └───────────┘
       │              │                 │
       │         (falla)                │
       │              ▼                 │
       │         error en              │
       │         make.log              │
       │                               │
       └───────────────────────────────┘
                   dkms remove
```

| Estado | Significado |
|---|---|
| **added** | Fuente registrada en DKMS, no compilada |
| **built** | Compilada para un kernel específico |
| **installed** | Copiada a `/lib/modules/<kver>/updates/dkms/` |
| **(loaded)** | Cargada en el kernel con modprobe (fuera del control de DKMS) |

### Flujo automático al instalar un kernel nuevo

```
  dnf install kernel-6.8.0
       │
       ▼
  Post-install script del paquete kernel
       │
       ▼
  Para cada módulo DKMS registrado:
  ┌──────────────────────────────────────────┐
  │ dkms build <module>/<version> -k 6.8.0  │
  │ dkms install <module>/<version> -k 6.8.0│
  └──────────────────────────────────────────┘
       │
       ▼
  dracut --force (regenerar initramfs si necesario)
       │
       ▼
  Módulos listos para el nuevo kernel
```

---

## Comandos de DKMS

### Estado y consulta

```bash
# List all DKMS modules and their status
dkms status
# nvidia/535.129.03, 6.7.4-200.fc39.x86_64, x86_64: installed
# nvidia/535.129.03, 6.6.9-200.fc39.x86_64, x86_64: installed
# v4l2loopback/0.12.7, 6.7.4-200.fc39.x86_64, x86_64: installed

# Filter by module
dkms status -m nvidia
# nvidia/535.129.03, 6.7.4-200.fc39.x86_64, x86_64: installed

# Filter by kernel
dkms status -k $(uname -r)
```

### Operaciones manuales

```bash
# ── Add: register source with DKMS ──
sudo dkms add -m <module> -v <version>
# Or if dkms.conf has the info:
sudo dkms add /usr/src/<module>-<version>/

# ── Build: compile for a specific kernel ──
sudo dkms build -m <module> -v <version> -k $(uname -r)

# ── Install: copy .ko to /lib/modules/ ──
sudo dkms install -m <module> -v <version> -k $(uname -r)

# ── All in one: add + build + install ──
sudo dkms install -m <module> -v <version>

# ── Remove: uninstall from one kernel ──
sudo dkms remove -m <module> -v <version> -k $(uname -r)

# ── Remove: uninstall from ALL kernels ──
sudo dkms remove -m <module> -v <version> --all

# ── Uninstall: remove .ko but keep built state ──
sudo dkms uninstall -m <module> -v <version> -k $(uname -r)
```

### Build para un kernel diferente

```bash
# Build for a kernel that's installed but not running
sudo dkms build -m nvidia -v 535.129.03 -k 6.8.0-100.fc40.x86_64
sudo dkms install -m nvidia -v 535.129.03 -k 6.8.0-100.fc40.x86_64

# Requires kernel headers for the target kernel:
# /usr/src/kernels/6.8.0-100.fc40.x86_64/   (RHEL)
# /usr/src/linux-headers-6.8.0-100/          (Debian)
```

### Ver logs de compilación

```bash
# If build fails, check the log:
cat /var/lib/dkms/<module>/<version>/build/make.log

# Example:
cat /var/lib/dkms/nvidia/535.129.03/build/make.log | tail -20
```

---

## dkms.conf: configuración del módulo

Cada módulo DKMS tiene un archivo `dkms.conf` que le dice a DKMS cómo compilar e instalar el módulo.

### Formato básico

```bash
# /usr/src/hello-1.0/dkms.conf

PACKAGE_NAME="hello"
PACKAGE_VERSION="1.0"

# What to compile
MAKE[0]="make -C ${kernel_source_dir} M=${dkms_tree}/${PACKAGE_NAME}/${PACKAGE_VERSION}/build modules"
CLEAN="make -C ${kernel_source_dir} M=${dkms_tree}/${PACKAGE_NAME}/${PACKAGE_VERSION}/build clean"

# What module files to install
BUILT_MODULE_NAME[0]="hello"
DEST_MODULE_LOCATION[0]="/updates/dkms"

# Auto-install for new kernels
AUTOINSTALL="yes"
```

### Variables disponibles

| Variable | Significado |
|---|---|
| `PACKAGE_NAME` | Nombre del módulo |
| `PACKAGE_VERSION` | Versión del módulo |
| `MAKE[n]` | Comando(s) de compilación |
| `CLEAN` | Comando de limpieza |
| `BUILT_MODULE_NAME[n]` | Nombre del .ko producido (sin extensión) |
| `BUILT_MODULE_LOCATION[n]` | Directorio donde se genera el .ko |
| `DEST_MODULE_LOCATION[n]` | Directorio destino en /lib/modules/ |
| `AUTOINSTALL` | Compilar automáticamente para kernels nuevos |
| `BUILD_DEPENDS[n]` | Dependencias de otros módulos DKMS |
| `PRE_BUILD` | Script a ejecutar antes de compilar |
| `POST_BUILD` | Script a ejecutar después de compilar |
| `POST_INSTALL` | Script a ejecutar después de instalar |

### Variables de entorno disponibles en MAKE

```bash
# These variables are set by DKMS during build:
${kernel_source_dir}    # /usr/src/kernels/6.7.4-200.fc39.x86_64
                        # or /lib/modules/6.7.4/build
${kernelver}            # 6.7.4-200.fc39.x86_64
${dkms_tree}            # /var/lib/dkms
${source_tree}          # /usr/src
${PACKAGE_NAME}         # hello
${PACKAGE_VERSION}      # 1.0
```

### Ejemplo real: v4l2loopback

```bash
cat /usr/src/v4l2loopback-0.12.7/dkms.conf
# PACKAGE_NAME="v4l2loopback"
# PACKAGE_VERSION="0.12.7"
# MAKE[0]="make -C ${kernel_source_dir} M=${dkms_tree}/${PACKAGE_NAME}/${PACKAGE_VERSION}/build"
# CLEAN="make -C ${kernel_source_dir} M=${dkms_tree}/${PACKAGE_NAME}/${PACKAGE_VERSION}/build clean"
# BUILT_MODULE_NAME[0]="v4l2loopback"
# DEST_MODULE_LOCATION[0]="/updates/dkms"
# AUTOINSTALL="yes"
```

---

## DKMS y Secure Boot

Con Secure Boot habilitado, el kernel solo carga módulos firmados. Los módulos compilados por DKMS **no están firmados** con la clave de la distribución, así que fallan al cargar:

```bash
modprobe nvidia
# modprobe: ERROR: could not insert 'nvidia': Required key not available
```

### Solución: Machine Owner Key (MOK)

```
  ┌──────────────────────────────────────────────────────────────┐
  │                DKMS + SECURE BOOT                            │
  │                                                              │
  │  1. Generar un par de claves (una sola vez)                  │
  │  2. Enrollar la clave pública en MOK (Machine Owner Key)     │
  │  3. Configurar DKMS para firmar módulos con la clave privada │
  │  4. Cada módulo compilado se firma automáticamente           │
  │  5. Secure Boot acepta el módulo (clave en MOK)              │
  └──────────────────────────────────────────────────────────────┘
```

### Configurar firma automática

```bash
# Step 1: generate signing key pair
sudo mkdir -p /etc/dkms
sudo openssl req -new -x509 -newkey rsa:2048 \
    -keyout /etc/dkms/mok.key \
    -outform DER -out /etc/dkms/mok.der \
    -nodes -days 36500 \
    -subj "/CN=DKMS Module Signing Key/"

sudo chmod 600 /etc/dkms/mok.key

# Step 2: enroll the public key in MOK
sudo mokutil --import /etc/dkms/mok.der
# Enter a one-time password (you'll need it during reboot)

# Step 3: reboot → MokManager appears
# Select "Enroll MOK" → "Continue" → enter the password
sudo reboot

# Step 4: verify key is enrolled
mokutil --list-enrolled | grep "DKMS"

# Step 5: configure DKMS to sign modules
# Some distros (Ubuntu, Fedora) do this automatically if keys exist
# For manual config, create /etc/dkms/framework.conf:
sudo tee /etc/dkms/framework.conf << 'EOF'
mok_signing_key="/etc/dkms/mok.key"
mok_certificate="/etc/dkms/mok.der"
sign_tool="/usr/lib/linux-$(uname -r | cut -d- -f1)/scripts/sign-file"
EOF

# Step 6: rebuild DKMS modules (they'll be signed now)
sudo dkms autoinstall
```

### Verificar firma

```bash
# Check if a module is signed
modinfo nvidia | grep "^sig"
# sig_id:         PKCS#7
# signer:         DKMS Module Signing Key
# sig_key:        AB:CD:EF:...

# Check if Secure Boot is enforcing
mokutil --sb-state
# SecureBoot enabled
```

---

## Módulos DKMS comunes

| Módulo | Paquete | Función |
|---|---|---|
| **NVIDIA** | `akmod-nvidia` (Fedora), `nvidia-dkms` (Debian) | GPU driver propietario |
| **VirtualBox** | `VirtualBox-dkms`, `virtualbox-guest-dkms` | Hipervisor (host y guest) |
| **v4l2loopback** | `v4l2loopback-dkms` | Virtual webcam device |
| **wireguard** | `wireguard-dkms` | VPN (kernels < 5.6) |
| **ZFS** | `zfs-dkms` | Filesystem ZFS |
| **broadcom-wl** | `broadcom-sta-dkms` | WiFi Broadcom propietario |
| **tp_smapi** | `tp-smapi-dkms` | ThinkPad battery management |

### NVIDIA: DKMS vs akmod (Fedora)

Fedora ofrece dos mecanismos para módulos NVIDIA:

```
  ┌────────────────────────────────────────────────────────────┐
  │                                                            │
  │  DKMS (nvidia-dkms)                                        │
  │  • Compila desde fuente en TU máquina                      │
  │  • Necesita kernel-devel instalado                         │
  │  • Compilación puede fallar si faltan dependencias         │
  │  • Usa el framework DKMS estándar                          │
  │                                                            │
  │  akmod (akmod-nvidia)  ← Preferido en Fedora               │
  │  • Compila desde fuente pero usa infraestructura de RPM    │
  │  • akmods service compila al arrancar si falta el .ko      │
  │  • Mejor integrado con el sistema de paquetes              │
  │  • Si falla, espera hasta 5 minutos al boot               │
  │                                                            │
  │  kmod (kmod-nvidia)                                        │
  │  • Módulo precompilado (no requiere compilación)           │
  │  • Funciona solo con el kernel exacto del paquete          │
  │  • Si actualizas kernel, necesitas nuevo kmod              │
  │                                                            │
  └────────────────────────────────────────────────────────────┘
```

---

## Crear un módulo DKMS propio

### Ejemplo: módulo "hello world"

```bash
# Step 1: create source directory
sudo mkdir -p /usr/src/hello-1.0

# Step 2: create the module source
sudo tee /usr/src/hello-1.0/hello.c << 'EOF'
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Lab Exercise");
MODULE_DESCRIPTION("Hello World DKMS module");
MODULE_VERSION("1.0");

static int __init hello_init(void) {
    printk(KERN_INFO "Hello from DKMS module!\n");
    return 0;
}

static void __exit hello_exit(void) {
    printk(KERN_INFO "Goodbye from DKMS module!\n");
}

module_init(hello_init);
module_exit(hello_exit);
EOF

# Step 3: create Makefile
sudo tee /usr/src/hello-1.0/Makefile << 'EOF'
obj-m += hello.o

all:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules

clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean
EOF

# Step 4: create dkms.conf
sudo tee /usr/src/hello-1.0/dkms.conf << 'EOF'
PACKAGE_NAME="hello"
PACKAGE_VERSION="1.0"
MAKE[0]="make -C ${kernel_source_dir} M=${dkms_tree}/${PACKAGE_NAME}/${PACKAGE_VERSION}/build modules"
CLEAN="make -C ${kernel_source_dir} M=${dkms_tree}/${PACKAGE_NAME}/${PACKAGE_VERSION}/build clean"
BUILT_MODULE_NAME[0]="hello"
DEST_MODULE_LOCATION[0]="/updates/dkms"
AUTOINSTALL="yes"
EOF

# Step 5: register with DKMS
sudo dkms add -m hello -v 1.0

# Step 6: build
sudo dkms build -m hello -v 1.0

# Step 7: install
sudo dkms install -m hello -v 1.0

# Step 8: verify
dkms status -m hello
# hello/1.0, 6.7.4-200.fc39.x86_64, x86_64: installed

# Step 9: test
sudo modprobe hello
dmesg | tail -1
# [...] Hello from DKMS module!

sudo modprobe -r hello
dmesg | tail -1
# [...] Goodbye from DKMS module!

# Step 10: clean up
sudo modprobe -r hello
sudo dkms remove -m hello -v 1.0 --all
sudo rm -rf /usr/src/hello-1.0
```

### Requisitos para compilar módulos

```bash
# Kernel headers/devel must be installed for each kernel
# RHEL/Fedora:
sudo dnf install kernel-devel-$(uname -r)
ls /usr/src/kernels/$(uname -r)/   # Must exist

# Debian/Ubuntu:
sudo apt install linux-headers-$(uname -r)
ls /usr/src/linux-headers-$(uname -r)/   # Must exist

# Without headers, dkms build fails:
# Error! Your kernel headers for kernel X are not installed.
```

---

## Errores comunes

### 1. Kernel headers no instalados

```bash
# Build fails with:
# Error! Your kernel headers for kernel 6.8.0-100 are not installed.
# Please install the kernel-devel or linux-headers package.

# Fix:
sudo dnf install kernel-devel-$(uname -r)    # RHEL
sudo apt install linux-headers-$(uname -r)   # Debian

# For ALL installed kernels:
sudo dnf install kernel-devel                 # Installs for current
```

### 2. Build falla pero no se nota hasta el reboot

DKMS compila durante la instalación del kernel. Si falla, puede que no veas el error hasta que reinicies con el nuevo kernel y el módulo no carga:

```bash
# Check for failed builds BEFORE rebooting:
dkms status
# nvidia/535.129.03, 6.8.0-100.fc40.x86_64, x86_64: built  ← Not "installed"!

# Check the build log:
cat /var/lib/dkms/nvidia/535.129.03/build/make.log | tail -30

# Force rebuild:
sudo dkms build -m nvidia -v 535.129.03 -k 6.8.0-100.fc40.x86_64
sudo dkms install -m nvidia -v 535.129.03 -k 6.8.0-100.fc40.x86_64
```

### 3. Módulo DKMS no se carga por Secure Boot

```bash
# modprobe: ERROR: could not insert 'nvidia': Required key not available

# Check Secure Boot status
mokutil --sb-state
# SecureBoot enabled

# Solutions:
# A. Sign the module (see DKMS + Secure Boot section above)
# B. Disable Secure Boot in firmware (not recommended for production)
# C. Some distro packages auto-sign (check if keys exist):
ls /etc/dkms/mok.* 2>/dev/null
```

### 4. Versiones conflictivas del mismo módulo

```bash
# Two versions of the same DKMS module:
dkms status
# nvidia/535.129.03, 6.7.4, x86_64: installed
# nvidia/545.29.06, 6.7.4, x86_64: installed  ← Conflict!

# Remove the old version:
sudo dkms remove -m nvidia -v 535.129.03 --all
```

### 5. No ejecutar depmod después de instalar manualmente

DKMS ejecuta `depmod` automáticamente, pero si copias un `.ko` manualmente sin DKMS, debes ejecutar `depmod -a` tú mismo. Con DKMS este error no ocurre, pero es importante saber que DKMS lo hace internamente.

---

## Cheatsheet

```bash
# ═══════════════════════════════════════════════
#  ESTADO
# ═══════════════════════════════════════════════
dkms status                          # All modules, all kernels
dkms status -m <module>              # Specific module
dkms status -k $(uname -r)          # Current kernel only

# ═══════════════════════════════════════════════
#  OPERACIONES
# ═══════════════════════════════════════════════
sudo dkms add -m <mod> -v <ver>      # Register source
sudo dkms build -m <mod> -v <ver>    # Compile for current kernel
sudo dkms install -m <mod> -v <ver>  # Install .ko
sudo dkms remove -m <mod> -v <ver> --all  # Remove completely
sudo dkms autoinstall                # Build+install all for current kernel

# For a specific kernel:
sudo dkms build -m <mod> -v <ver> -k <kernel-version>
sudo dkms install -m <mod> -v <ver> -k <kernel-version>

# ═══════════════════════════════════════════════
#  DIAGNÓSTICO
# ═══════════════════════════════════════════════
cat /var/lib/dkms/<mod>/<ver>/build/make.log   # Build log
ls /usr/src/<mod>-<ver>/dkms.conf              # Config file
ls /lib/modules/$(uname -r)/updates/dkms/      # Installed modules

# ═══════════════════════════════════════════════
#  REQUISITOS
# ═══════════════════════════════════════════════
# RHEL/Fedora:
sudo dnf install dkms kernel-devel-$(uname -r)
# Debian/Ubuntu:
sudo apt install dkms linux-headers-$(uname -r)

# ═══════════════════════════════════════════════
#  SECURE BOOT
# ═══════════════════════════════════════════════
mokutil --sb-state                   # Check Secure Boot
mokutil --list-enrolled              # List MOK keys
sudo mokutil --import /etc/dkms/mok.der  # Enroll signing key

# ═══════════════════════════════════════════════
#  MÓDULO PROPIO
# ═══════════════════════════════════════════════
# 1. Create /usr/src/<name>-<ver>/ with:
#    - Source code (.c, .h)
#    - Makefile
#    - dkms.conf
# 2. sudo dkms add -m <name> -v <ver>
# 3. sudo dkms build -m <name> -v <ver>
# 4. sudo dkms install -m <name> -v <ver>
```

---

## Ejercicios

### Ejercicio 1: Explorar DKMS en tu sistema

```bash
# Step 1: check if DKMS is installed
which dkms && echo "DKMS installed" || echo "DKMS not installed"

# Step 2: install if needed
# sudo dnf install dkms    # RHEL/Fedora
# sudo apt install dkms    # Debian/Ubuntu

# Step 3: list all DKMS modules
dkms status
# If empty: no DKMS modules registered

# Step 4: check for DKMS source directories
ls /usr/src/ | grep -v "kernels\|linux-headers\|debug"

# Step 5: check for kernel headers (needed for DKMS builds)
ls /usr/src/kernels/$(uname -r)/ 2>/dev/null && \
    echo "Headers installed" || \
ls /usr/src/linux-headers-$(uname -r)/ 2>/dev/null && \
    echo "Headers installed" || \
    echo "Headers NOT installed — install kernel-devel / linux-headers"

# Step 6: check DKMS tree
ls /var/lib/dkms/ 2>/dev/null

# Step 7: check for DKMS-installed modules in current kernel
ls /lib/modules/$(uname -r)/updates/dkms/ 2>/dev/null && \
    echo "DKMS modules found:" && \
    ls /lib/modules/$(uname -r)/updates/dkms/ || \
    echo "No DKMS modules installed for this kernel"
```

**Preguntas:**
1. ¿Tienes módulos DKMS en tu sistema? ¿Cuáles?
2. ¿Los kernel headers están instalados para tu kernel actual?
3. ¿En qué directorio de `/lib/modules/` instala DKMS los módulos compilados?

> **Pregunta de predicción**: si instalas un kernel nuevo con `dnf install kernel` pero **no** tienes `kernel-devel` para esa nueva versión, ¿DKMS podrá compilar los módulos para ese kernel? ¿Qué error verás?

### Ejercicio 2: Crear y gestionar un módulo DKMS

```bash
# Step 1: ensure prerequisites
sudo dnf install dkms kernel-devel-$(uname -r) gcc make 2>/dev/null || \
    sudo apt install dkms linux-headers-$(uname -r) gcc make 2>/dev/null

# Step 2: create the module source
sudo mkdir -p /usr/src/labmod-1.0

sudo tee /usr/src/labmod-1.0/labmod.c << 'EOF'
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("DKMS Lab Module");
MODULE_VERSION("1.0");

static char *whom = "World";
module_param(whom, charp, 0644);
MODULE_PARM_DESC(whom, "Whom to greet");

static int __init labmod_init(void) {
    printk(KERN_INFO "labmod: Hello, %s!\n", whom);
    return 0;
}

static void __exit labmod_exit(void) {
    printk(KERN_INFO "labmod: Goodbye, %s!\n", whom);
}

module_init(labmod_init);
module_exit(labmod_exit);
EOF

sudo tee /usr/src/labmod-1.0/Makefile << 'EOF'
obj-m += labmod.o
EOF

sudo tee /usr/src/labmod-1.0/dkms.conf << 'EOF'
PACKAGE_NAME="labmod"
PACKAGE_VERSION="1.0"
MAKE[0]="make -C ${kernel_source_dir} M=${dkms_tree}/${PACKAGE_NAME}/${PACKAGE_VERSION}/build modules"
CLEAN="make -C ${kernel_source_dir} M=${dkms_tree}/${PACKAGE_NAME}/${PACKAGE_VERSION}/build clean"
BUILT_MODULE_NAME[0]="labmod"
DEST_MODULE_LOCATION[0]="/updates/dkms"
AUTOINSTALL="yes"
EOF

# Step 3: add to DKMS
sudo dkms add -m labmod -v 1.0
dkms status -m labmod
# labmod/1.0: added

# Step 4: build
sudo dkms build -m labmod -v 1.0
dkms status -m labmod
# labmod/1.0, 6.7.4-200.fc39.x86_64, x86_64: built

# Step 5: install
sudo dkms install -m labmod -v 1.0
dkms status -m labmod
# labmod/1.0, 6.7.4-200.fc39.x86_64, x86_64: installed

# Step 6: verify it's in the module tree
ls /lib/modules/$(uname -r)/updates/dkms/labmod*
modinfo labmod

# Step 7: load and test
sudo modprobe labmod
dmesg | tail -1
# labmod: Hello, World!

sudo modprobe -r labmod
sudo modprobe labmod whom="DKMS"
dmesg | tail -1
# labmod: Hello, DKMS!

# Step 8: clean up
sudo modprobe -r labmod
sudo dkms remove -m labmod -v 1.0 --all
sudo rm -rf /usr/src/labmod-1.0
```

**Preguntas:**
1. ¿Cuántos estados pasó el módulo (added → built → installed)?
2. ¿El parámetro `whom` funcionó? ¿Dónde aparece en `/sys/module/`?
3. ¿Dónde está el log de compilación? ¿Qué comando de `make` ejecutó DKMS?

> **Pregunta de predicción**: si ahora instalas un kernel nuevo sin borrar `/usr/src/labmod-1.0/`, ¿DKMS compilará automáticamente `labmod` para el nuevo kernel? ¿Qué campo de `dkms.conf` controla esto?

### Ejercicio 3: Diagnosticar un fallo de DKMS

```bash
# We'll simulate a build failure and learn to diagnose it

# Step 1: create a module with a deliberate error
sudo mkdir -p /usr/src/broken-1.0

sudo tee /usr/src/broken-1.0/broken.c << 'EOF'
#include <linux/init.h>
#include <linux/module.h>

MODULE_LICENSE("GPL");

static int __init broken_init(void) {
    this_function_does_not_exist();  /* Deliberate error */
    return 0;
}

static void __exit broken_exit(void) {}

module_init(broken_init);
module_exit(broken_exit);
EOF

sudo tee /usr/src/broken-1.0/Makefile << 'EOF'
obj-m += broken.o
EOF

sudo tee /usr/src/broken-1.0/dkms.conf << 'EOF'
PACKAGE_NAME="broken"
PACKAGE_VERSION="1.0"
MAKE[0]="make -C ${kernel_source_dir} M=${dkms_tree}/${PACKAGE_NAME}/${PACKAGE_VERSION}/build modules"
CLEAN="make -C ${kernel_source_dir} M=${dkms_tree}/${PACKAGE_NAME}/${PACKAGE_VERSION}/build clean"
BUILT_MODULE_NAME[0]="broken"
DEST_MODULE_LOCATION[0]="/updates/dkms"
AUTOINSTALL="yes"
EOF

# Step 2: add and try to build
sudo dkms add -m broken -v 1.0
sudo dkms build -m broken -v 1.0 2>&1
# Should FAIL

# Step 3: check status
dkms status -m broken
# broken/1.0, ...: added  (NOT "built" — build failed)

# Step 4: find and read the build log
cat /var/lib/dkms/broken/1.0/build/make.log
# Look for the error: "implicit declaration of function 'this_function_does_not_exist'"

# Step 5: fix the source
sudo tee /usr/src/broken-1.0/broken.c << 'EOF'
#include <linux/init.h>
#include <linux/module.h>

MODULE_LICENSE("GPL");

static int __init broken_init(void) {
    printk(KERN_INFO "Fixed module loaded!\n");
    return 0;
}

static void __exit broken_exit(void) {
    printk(KERN_INFO "Fixed module unloaded!\n");
}

module_init(broken_init);
module_exit(broken_exit);
EOF

# Step 6: rebuild
sudo dkms build -m broken -v 1.0
dkms status -m broken
# broken/1.0, ...: built  ← Success!

# Step 7: install and test
sudo dkms install -m broken -v 1.0
sudo modprobe broken
dmesg | tail -1
# Fixed module loaded!

# Step 8: clean up
sudo modprobe -r broken
sudo dkms remove -m broken -v 1.0 --all
sudo rm -rf /usr/src/broken-1.0
```

**Preguntas:**
1. ¿Qué error exacto apareció en `make.log`?
2. ¿`dkms status` mostró el módulo como "added" o "built" después del fallo?
3. ¿Fue necesario ejecutar `dkms remove` y `dkms add` de nuevo antes de reconstruir, o bastó con `dkms build`?

> **Pregunta de predicción**: si un módulo DKMS tiene `AUTOINSTALL="yes"` pero su build falla para un kernel nuevo, ¿el nuevo kernel arrancará normalmente sin ese módulo? ¿O DKMS bloqueará la instalación del kernel?

---

> **Siguiente capítulo**: C06 — Mantenimiento del Sistema, S01 — Backup y Recuperación, T01 — tar.
