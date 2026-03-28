# Paquetes necesarios

## Índice

1. [Qué necesitamos instalar](#qué-necesitamos-instalar)
2. [Instalación en Fedora/RHEL/AlmaLinux](#instalación-en-fedorarhelAlmaLinux)
3. [Instalación en Debian/Ubuntu](#instalación-en-debianubuntu)
4. [Paquetes auxiliares](#paquetes-auxiliares)
5. [Verificar la instalación](#verificar-la-instalación)
6. [Qué hace cada paquete](#qué-hace-cada-paquete)
7. [Errores comunes](#errores-comunes)
8. [Cheatsheet](#cheatsheet)
9. [Ejercicios](#ejercicios)

---

## Qué necesitamos instalar

En S01 vimos que la pila tiene varias capas. Cada capa corresponde a uno o
más paquetes:

```
┌──────────────────────────────────────────────────────────────┐
│  Capa                   Paquete(s)            ¿Obligatorio? │
│  ────                   ──────────            ───────────── │
│  Hardware (VT-x/AMD-V)  (no es paquete)       Sí            │
│  KVM (kernel module)     qemu-kvm / kvm       Ya incluido*  │
│  QEMU (emulador)         qemu-kvm / qemu-system  Sí        │
│  libvirt (gestión)       libvirt-daemon       Sí            │
│  CLI crear VMs           virt-install         Sí            │
│  CLI gestionar VMs       virsh (viene con     Sí            │
│                          libvirt-client)                     │
│  GUI gestión             virt-manager         Opcional**    │
│  Visor de consola        virt-viewer          Opcional**    │
│                                                              │
│  * KVM es un módulo del kernel Linux mainline.               │
│    Los paquetes qemu-kvm aseguran que está disponible.       │
│  ** Si trabajas solo por SSH/CLI, no necesitas GUI.          │
│     Para el curso, virt-manager es útil para inspección.     │
└──────────────────────────────────────────────────────────────┘
```

---

## Instalación en Fedora/RHEL/AlmaLinux

### Opción 1: grupo de virtualización (recomendado)

Fedora y las distribuciones RHEL-family ofrecen un grupo de paquetes que
instala todo lo necesario de una vez:

```bash
# Instalar el grupo completo de virtualización
sudo dnf install @virtualization
```

Este grupo incluye: `qemu-kvm`, `libvirt`, `virt-install`, `virt-manager`,
`virt-viewer`, y dependencias.

Para ver qué contiene el grupo:

```bash
dnf group info virtualization
```

### Opción 2: paquetes individuales

Si prefieres control fino sobre qué se instala:

```bash
sudo dnf install \
    qemu-kvm \          # QEMU con soporte KVM
    libvirt \           # Daemon de gestión (libvirtd)
    virt-install \      # CLI para crear VMs
    virt-manager \      # GUI para gestionar VMs
    virt-viewer         # Visor de consola SPICE/VNC
```

### Sin entorno gráfico (servidor headless)

Si tu máquina no tiene desktop environment (servidor por SSH):

```bash
sudo dnf install \
    qemu-kvm \
    libvirt \
    virt-install
# No instalar virt-manager ni virt-viewer (requieren X11/Wayland)
# Gestionar todo con virsh y virt-install por CLI
```

---

## Instalación en Debian/Ubuntu

### Paquetes necesarios

Los nombres de paquetes difieren ligeramente de Fedora:

```bash
sudo apt install \
    qemu-system-x86 \              # QEMU para x86_64
    libvirt-daemon-system \        # libvirtd + configs del sistema
    libvirt-clients \              # virsh y herramientas CLI
    virtinst \                     # virt-install, virt-clone
    virt-manager \                 # GUI (opcional)
    virt-viewer \                  # Visor de consola (opcional)
    ovmf                           # Firmware UEFI para VMs
```

### Paquete qemu-kvm en Debian

En Debian, `qemu-kvm` es un metapaquete que depende de `qemu-system-x86`.
Puedes instalar cualquiera de los dos:

```bash
# Equivalentes en Debian:
sudo apt install qemu-kvm
# o
sudo apt install qemu-system-x86
```

### Sin entorno gráfico

```bash
sudo apt install \
    qemu-system-x86 \
    libvirt-daemon-system \
    libvirt-clients \
    virtinst
```

---

## Paquetes auxiliares

Además de los paquetes base, hay herramientas complementarias útiles para
el curso:

### bridge-utils

Herramientas para gestionar bridges de red (necesario para redes bridged
en S06):

```bash
# Fedora
sudo dnf install bridge-utils

# Debian/Ubuntu
sudo apt install bridge-utils
```

### guestfs-tools

Herramientas para inspeccionar y modificar imágenes de disco **sin arrancar
la VM**:

```bash
# Fedora
sudo dnf install guestfs-tools

# Debian/Ubuntu
sudo apt install libguestfs-tools
```

```bash
# Ejemplos de uso:
# Listar archivos dentro de una imagen
virt-ls -a disk.qcow2 /etc/

# Leer un archivo de una imagen sin arrancar la VM
virt-cat -a disk.qcow2 /etc/hostname

# Copiar archivos al interior de una imagen
virt-copy-in -a disk.qcow2 myfile.conf /etc/

# Inspeccionar una imagen (OS, particiones, filesystem)
virt-inspector -a disk.qcow2
```

### cloud-image-utils / cloud-init

Para trabajar con imágenes cloud preconfiguradas (S04 T03):

```bash
# Fedora
sudo dnf install cloud-utils

# Debian/Ubuntu
sudo apt install cloud-image-utils
```

Proporciona `cloud-localds` para crear ISOs de configuración cloud-init.

### swtpm

TPM (Trusted Platform Module) virtual. No es esencial para el curso pero
algunas distribuciones modernas lo requieren en la instalación:

```bash
# Fedora
sudo dnf install swtpm swtpm-tools

# Debian/Ubuntu
sudo apt install swtpm swtpm-tools
```

### osinfo-db y osinfo-query

Base de datos de variantes de OS para que `virt-install` configure
automáticamente los parámetros óptimos:

```bash
# Fedora (generalmente ya incluido con virt-install)
sudo dnf install osinfo-db osinfo-db-tools

# Debian/Ubuntu
sudo apt install osinfo-db osinfo-db-tools

# Listar variantes disponibles
osinfo-query os | head -20
osinfo-query os | grep -i debian
osinfo-query os | grep -i alma
```

### Resumen de paquetes auxiliares

```
┌──────────────────────────────────────────────────────────────┐
│  Paquete             Para qué                    Cuándo     │
│  ───────             ────────                    ──────     │
│  bridge-utils        Redes bridged               S06        │
│  guestfs-tools       Inspeccionar imágenes       S03, S04   │
│  cloud-image-utils   Imágenes cloud-init         S04        │
│  swtpm               TPM virtual                 Opcional   │
│  osinfo-db           Variantes de OS             S04        │
└──────────────────────────────────────────────────────────────┘
```

---

## Verificar la instalación

### virsh version

```bash
virsh version
```

Salida esperada:

```
Compiled against library: libvirt 10.0.0
Using library: libvirt 10.0.0
Using API: QEMU 10.0.0
Running hypervisor: QEMU 8.2.0
Running against daemon: 10.0.0
```

Si ves errores de conexión, el servicio libvirtd probablemente no está
activo (lo configuraremos en T02).

### qemu-img y qemu-system

```bash
# Versión de QEMU
qemu-system-x86_64 --version
# QEMU emulator version 8.2.0 (qemu-8.2.0-1.fc41)

# Herramienta de imágenes
qemu-img --version
# qemu-img version 8.2.0

# Verificar que KVM está como acelerador disponible
qemu-system-x86_64 -accel help
# Accelerators supported in QEMU binary:
# kvm
# tcg
```

### virt-install

```bash
virt-install --version
# 4.1.0
```

### virt-host-validate

La verificación integral que comprueba toda la pila:

```bash
virt-host-validate qemu
```

```
QEMU: Checking for hardware virtualization                 : PASS
QEMU: Checking if device /dev/kvm exists                   : PASS
QEMU: Checking if device /dev/kvm is accessible            : PASS
QEMU: Checking if device /dev/vhost-net exists              : PASS
QEMU: Checking for cgroup 'cpu' controller support         : PASS
QEMU: Checking for cgroup 'cpuacct' controller support     : PASS
QEMU: Checking for cgroup 'cpuset' controller support      : PASS
QEMU: Checking for cgroup 'memory' controller support      : PASS
QEMU: Checking for cgroup 'devices' controller support     : WARN (...)
QEMU: Checking for cgroup 'blkio' controller support       : PASS
QEMU: Checking for device assignment IOMMU support         : WARN (...)
QEMU: Checking for secure guest support                     : WARN (...)
```

Los PASS en las primeras 4 líneas son los críticos. Los WARN en cgroups v2
y IOMMU son normales y no afectan al curso.

### Tabla de paquetes por distribución

```
┌────────────────────┬─────────────────────┬─────────────────────┐
│  Función           │  Fedora/RHEL/Alma   │  Debian/Ubuntu      │
├────────────────────┼─────────────────────┼─────────────────────┤
│  Todo junto        │  @virtualization    │  (no hay grupo)     │
├────────────────────┼─────────────────────┼─────────────────────┤
│  QEMU + KVM        │  qemu-kvm           │  qemu-system-x86    │
│  libvirt daemon    │  libvirt             │  libvirt-daemon-     │
│                    │                     │    system            │
│  virsh CLI         │  (incluido en       │  libvirt-clients     │
│                    │   libvirt)           │                     │
│  virt-install      │  virt-install        │  virtinst            │
│  GUI               │  virt-manager        │  virt-manager        │
│  Visor consola     │  virt-viewer         │  virt-viewer         │
│  UEFI firmware     │  edk2-ovmf           │  ovmf                │
│  Bridge utils      │  bridge-utils        │  bridge-utils        │
│  Inspección imgs   │  guestfs-tools       │  libguestfs-tools    │
│  Cloud-init utils  │  cloud-utils         │  cloud-image-utils   │
│  TPM virtual       │  swtpm swtpm-tools   │  swtpm swtpm-tools   │
│  OS variants DB    │  osinfo-db           │  osinfo-db           │
├────────────────────┼─────────────────────┼─────────────────────┤
│  Grupo usuario     │  libvirt             │  libvirt             │
│  Grupo /dev/kvm    │  kvm                 │  kvm                 │
└────────────────────┴─────────────────────┴─────────────────────┘
```

---

## Qué hace cada paquete

### Diagrama de relaciones

```
┌──────────────────────────────────────────────────────────────┐
│                                                              │
│  ┌────────────────────┐    ┌──────────────────┐             │
│  │  virt-manager (GUI) │    │  virt-viewer      │             │
│  │  Crear, editar,     │    │  Consola SPICE/   │             │
│  │  monitorear VMs     │    │  VNC de una VM     │             │
│  └─────────┬──────────┘    └────────┬─────────┘             │
│            │                        │                        │
│            ▼                        ▼                        │
│  ┌─────────────────────────────────────────────┐            │
│  │  libvirt (libvirtd)                          │            │
│  │  ─ Daemon que gestiona VMs                   │            │
│  │  ─ API unificada (XML, virsh, bindings)      │            │
│  │  ─ Gestiona storage pools, redes, snapshots  │            │
│  │  ─ Aplica permisos y seguridad               │            │
│  └──────┬──────────────────┬────────────────────┘            │
│         │                  │                                 │
│         ▼                  ▼                                 │
│  ┌────────────┐    ┌────────────────┐                       │
│  │ virt-install│    │ virsh           │                       │
│  │ Crear VMs  │    │ Gestionar VMs  │                       │
│  │ desde CLI  │    │ desde CLI      │                       │
│  │ (una vez)  │    │ (día a día)    │                       │
│  └────────────┘    └────────────────┘                       │
│                                                              │
│  libvirt lanza procesos QEMU:                                │
│                                                              │
│  ┌─────────────────────────────────────────────┐            │
│  │  qemu-system-x86_64 (proceso por VM)        │            │
│  │  ─ Emula dispositivos (disco, red, USB)      │            │
│  │  ─ Usa /dev/kvm para ejecutar CPU del guest  │            │
│  └──────┬──────────────────┬────────────────────┘            │
│         │                  │                                 │
│         ▼                  ▼                                 │
│  ┌────────────┐    ┌────────────────┐                       │
│  │ qemu-img   │    │ /dev/kvm (KVM) │                       │
│  │ Crear,     │    │ Módulo kernel  │                       │
│  │ convertir, │    │ Ejecución de   │                       │
│  │ inspeccionar│   │ CPU nativa     │                       │
│  │ imágenes   │    │                │                       │
│  └────────────┘    └────────────────┘                       │
│                                                              │
└──────────────────────────────────────────────────────────────┘
```

### Resumen de binarios

| Binario | Paquete | Función |
|---------|---------|---------|
| `virsh` | libvirt-client | CLI principal para gestionar VMs |
| `virt-install` | virt-install / virtinst | Crear VMs nuevas |
| `virt-clone` | virt-install / virtinst | Clonar VMs existentes |
| `virt-manager` | virt-manager | GUI completa |
| `virt-viewer` | virt-viewer | Visor de consola SPICE/VNC |
| `virt-host-validate` | libvirt-client | Verificar el entorno |
| `qemu-system-x86_64` | qemu-kvm | Ejecutar VMs |
| `qemu-img` | qemu-img | Gestionar imágenes de disco |
| `virt-ls`, `virt-cat` | guestfs-tools | Inspeccionar imágenes |
| `osinfo-query` | osinfo-db-tools | Consultar variantes de OS |
| `cloud-localds` | cloud-image-utils | Crear ISOs cloud-init |

---

## Errores comunes

### 1. Instalar solo QEMU y olvidar libvirt

```
❌ sudo dnf install qemu-kvm
   # "Listo, ya puedo crear VMs"
   # No: sin libvirt no tienes virsh, ni redes virtuales,
   # ni storage pools, ni gestión de snapshots

✅ Instalar la pila completa:
   sudo dnf install @virtualization
   # O al menos: qemu-kvm libvirt virt-install
```

### 2. No agregar el usuario a los grupos necesarios

```
❌ virsh list --all
   # error: failed to connect to the hypervisor
   # error: authentication unavailable: no polkit agent

✅ Agregar usuario a los grupos:
   sudo usermod -aG libvirt $USER
   sudo usermod -aG kvm $USER
   # IMPORTANTE: cerrar sesión y volver a entrar
   # (o reiniciar, o usar newgrp libvirt)

   # Verificar:
   groups
   # ... libvirt kvm ...
```

### 3. Confundir nombres de paquetes entre distribuciones

```
❌ En Debian:
   sudo apt install virt-install
   # E: Unable to locate package virt-install

✅ En Debian el paquete se llama 'virtinst':
   sudo apt install virtinst

❌ En Fedora:
   sudo dnf install libvirt-daemon-system
   # No available package

✅ En Fedora el paquete se llama simplemente 'libvirt':
   sudo dnf install libvirt
```

### 4. Instalar QEMU para la arquitectura equivocada

```
❌ sudo apt install qemu-system-arm
   # Instala QEMU para emular ARM, no para virtualizar x86

✅ Para VMs x86_64 en un host x86_64:
   sudo apt install qemu-system-x86
   # o qemu-kvm (metapaquete que apunta a qemu-system-x86)
```

### 5. No instalar firmware UEFI (OVMF)

```
❌ Crear una VM con --boot uefi sin tener OVMF instalado:
   virt-install ... --boot uefi
   # ERROR: UEFI firmware not found

✅ Instalar el firmware UEFI:
   # Fedora
   sudo dnf install edk2-ovmf

   # Debian/Ubuntu
   sudo apt install ovmf

   # Verificar:
   ls /usr/share/OVMF/
   # o
   ls /usr/share/edk2/ovmf/
```

---

## Cheatsheet

```
┌──────────────────────────────────────────────────────────────┐
│          PAQUETES NECESARIOS CHEATSHEET                       │
├──────────────────────────────────────────────────────────────┤
│                                                              │
│  INSTALACIÓN RÁPIDA                                          │
│  Fedora:  sudo dnf install @virtualization                  │
│  Debian:  sudo apt install qemu-system-x86 \               │
│             libvirt-daemon-system libvirt-clients \          │
│             virtinst virt-manager virt-viewer ovmf           │
│                                                              │
│  PAQUETES ESENCIALES                                         │
│  qemu-kvm / qemu-system-x86    QEMU + KVM                  │
│  libvirt / libvirt-daemon-system  Daemon de gestión         │
│  virt-install / virtinst        Crear VMs (CLI)             │
│  virsh (en libvirt-clients)     Gestionar VMs (CLI)         │
│                                                              │
│  PAQUETES OPCIONALES                                         │
│  virt-manager                   GUI completa                │
│  virt-viewer                    Visor consola SPICE/VNC     │
│  edk2-ovmf / ovmf              Firmware UEFI               │
│                                                              │
│  PAQUETES AUXILIARES                                         │
│  bridge-utils                   Redes bridged               │
│  guestfs-tools                  Inspeccionar imágenes       │
│  cloud-utils / cloud-image-utils  Imágenes cloud-init       │
│  swtpm swtpm-tools              TPM virtual                 │
│  osinfo-db                      Variantes de OS             │
│                                                              │
│  GRUPOS DE USUARIO                                           │
│  sudo usermod -aG libvirt $USER   Acceso a libvirt          │
│  sudo usermod -aG kvm $USER      Acceso a /dev/kvm         │
│  (cerrar sesión y volver a entrar)                          │
│                                                              │
│  VERIFICAR INSTALACIÓN                                       │
│  virsh version                  Versiones de la pila        │
│  qemu-system-x86_64 --version  Versión de QEMU             │
│  qemu-system-x86_64 -accel help  KVM disponible?           │
│  virt-install --version         Versión de virt-install     │
│  virt-host-validate qemu        Verificación integral       │
│  osinfo-query os | grep debian  Variantes de OS             │
│                                                              │
│  GUESTFS-TOOLS                                               │
│  virt-ls -a img.qcow2 /etc/    Listar archivos             │
│  virt-cat -a img.qcow2 /path   Leer archivo                │
│  virt-copy-in -a img file /dst  Copiar archivo dentro      │
│  virt-inspector -a img.qcow2    Inspeccionar imagen        │
└──────────────────────────────────────────────────────────────┘
```

---

## Ejercicios

### Ejercicio 1: Instalar la pila de virtualización

Instala todos los paquetes necesarios en tu distribución:

**Tareas:**
1. Identifica tu distribución:
   ```bash
   cat /etc/os-release | head -5
   ```
2. Instala los paquetes base según tu distribución (ver secciones anteriores)
3. Instala los paquetes auxiliares que usaremos en el curso:
   `bridge-utils`, `guestfs-tools`, `cloud-image-utils` (o `cloud-utils`)
4. Agrega tu usuario a los grupos necesarios:
   ```bash
   sudo usermod -aG libvirt $USER
   sudo usermod -aG kvm $USER
   ```
5. Cierra sesión y vuelve a entrar (o reinicia)
6. Verifica los grupos:
   ```bash
   groups | tr ' ' '\n' | grep -E '(libvirt|kvm)'
   ```
7. Verifica que todos los binarios están disponibles:
   ```bash
   for cmd in virsh virt-install qemu-system-x86_64 qemu-img virt-host-validate; do
       printf "%-30s" "$cmd:"
       which $cmd 2>/dev/null || echo "NOT FOUND"
   done
   ```

**Pregunta de reflexión:** El grupo `@virtualization` de Fedora instala
también `virt-manager` (GUI). Si estás en un servidor headless sin entorno
gráfico, ¿hay alguna forma de usar virt-manager remotamente? (Pista: SSH
X forwarding, o conexión remota desde otro host con virt-manager.)

---

### Ejercicio 2: Verificar la pila completa

Ejecuta todas las verificaciones y documenta el estado:

**Tareas:**
1. Ejecuta la verificación integral:
   ```bash
   virt-host-validate qemu
   ```
2. Para cada línea, anota si es PASS, WARN o FAIL
3. Verifica las versiones de todos los componentes:
   ```bash
   virsh version
   qemu-system-x86_64 --version
   virt-install --version
   ```
4. Verifica que KVM está como acelerador:
   ```bash
   qemu-system-x86_64 -accel help
   ```
5. Lista las variantes de OS disponibles para el curso:
   ```bash
   osinfo-query os | grep -iE '(debian|alma|fedora|rhel)' | head -20
   ```
6. Si algo da FAIL, investiga con `dmesg | grep -i kvm` y los pasos
   de verificación de S01 T01

**Pregunta de reflexión:** `virt-host-validate` reporta WARN para "cgroup
devices controller" en sistemas con cgroups v2. ¿Qué son los cgroups y
por qué son relevantes para la virtualización? ¿Es este WARN un problema
real para nuestro uso?

---

### Ejercicio 3: Explorar los binarios instalados

Familiarízate con las herramientas antes de usarlas:

**Tareas:**
1. Explora la ayuda de cada herramienta principal:
   ```bash
   virsh help | head -30          # Categorías de comandos
   virt-install --help | head -30 # Opciones de creación
   qemu-img --help | head -20    # Operaciones con imágenes
   ```
2. Lista los comandos de virsh por categoría:
   ```bash
   virsh help domain    # Gestión de VMs
   virsh help network   # Gestión de redes
   virsh help volume    # Gestión de volúmenes
   virsh help snapshot  # Gestión de snapshots
   ```
3. Verifica si hay algún recurso ya configurado:
   ```bash
   virsh list --all        # VMs existentes
   virsh net-list --all    # Redes virtuales
   virsh pool-list --all   # Storage pools
   ```
4. La red `default` debería existir. Inspecciona:
   ```bash
   virsh net-info default
   virsh net-dumpxml default
   ```
5. Si la red default no está activa, actívala:
   ```bash
   virsh net-start default
   virsh net-autostart default
   ```

**Pregunta de reflexión:** `virsh net-dumpxml default` muestra un XML que
define la red. ¿Qué rango de IPs usa? ¿Tiene DHCP configurado? ¿Y NAT?
Lee el XML y describe qué entiendas de cada sección.
