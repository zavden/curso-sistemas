# Instalación de VMs desde ISO

## Índice

1. [virt-install: el creador de VMs](#virt-install-el-creador-de-vms)
2. [Obtener ISOs de instalación](#obtener-isos-de-instalación)
3. [Anatomía del comando virt-install](#anatomía-del-comando-virt-install)
4. [Flags esenciales en detalle](#flags-esenciales-en-detalle)
5. [Modos de visualización: graphics](#modos-de-visualización-graphics)
6. [Consultar variantes de OS](#consultar-variantes-de-os)
7. [Ejemplo completo: Debian 12](#ejemplo-completo-debian-12)
8. [Ejemplo completo: AlmaLinux 9](#ejemplo-completo-almalinux-9)
9. [Qué sucede internamente al ejecutar virt-install](#qué-sucede-internamente-al-ejecutar-virt-install)
10. [Errores comunes](#errores-comunes)
11. [Cheatsheet](#cheatsheet)
12. [Ejercicios](#ejercicios)

---

## virt-install: el creador de VMs

`virt-install` es la herramienta de línea de comandos para crear VMs nuevas
a través de libvirt. No es un hipervisor ni un emulador: es un **frontend**
que traduce tus flags en la definición XML de libvirt y lanza el proceso QEMU
correspondiente.

```
┌─────────────────────────────────────────────────────────────────┐
│                                                                 │
│  tú escribes:                                                   │
│    virt-install --name lab1 --ram 2048 --vcpus 2 ...            │
│         │                                                       │
│         ▼                                                       │
│  virt-install genera:                                           │
│    /etc/libvirt/qemu/lab1.xml   (definición permanente)         │
│         │                                                       │
│         ▼                                                       │
│  libvirtd lanza:                                                │
│    qemu-system-x86_64 -name guest=lab1 -m 2048 -smp 2 ...      │
│         │                                                       │
│         ▼                                                       │
│  KVM ejecuta:                                                   │
│    la VM con aceleración hardware via /dev/kvm                  │
│                                                                 │
└─────────────────────────────────────────────────────────────────┘
```

El paquete que lo proporciona es `virtinst` (Debian/Ubuntu) o parte del
grupo `@virtualization` (Fedora/RHEL).

```bash
# Verificar que está instalado
which virt-install
# /usr/bin/virt-install

virt-install --version
# 4.1.0
```

---

## Obtener ISOs de instalación

Para instalar una VM desde ISO necesitas descargar la imagen de instalación
del sistema operativo. Las variantes mínimas son ideales para laboratorio:

### Debian 12 (netinst)

```bash
# Descarga directa (~600 MB)
wget https://cdimage.debian.org/debian-cd/current/amd64/iso-cd/debian-12.9.0-amd64-netinst.iso

# Verificar integridad
wget https://cdimage.debian.org/debian-cd/current/amd64/iso-cd/SHA256SUMS
sha256sum -c SHA256SUMS 2>/dev/null | grep debian-12
```

### AlmaLinux 9 (minimal)

```bash
# Descarga directa (~2 GB)
wget https://repo.almalinux.org/almalinux/9/isos/x86_64/AlmaLinux-9-latest-x86_64-minimal.iso

# Verificar
wget https://repo.almalinux.org/almalinux/9/isos/x86_64/CHECKSUM
sha256sum -c CHECKSUM 2>/dev/null | grep minimal
```

### Dónde colocar las ISOs

La ubicación natural es el pool de libvirt. Puedes crear un pool dedicado
para ISOs o usar el default:

```bash
# Opción 1: mover al pool default
sudo mv debian-12.9.0-amd64-netinst.iso /var/lib/libvirt/images/

# Opción 2: crear un pool para ISOs (recomendado)
sudo mkdir -p /srv/iso
sudo virsh pool-define-as isos dir --target /srv/iso
sudo virsh pool-build isos
sudo virsh pool-start isos
sudo virsh pool-autostart isos
sudo mv *.iso /srv/iso/
sudo virsh pool-refresh isos
```

> **Nota**: `virt-install` acepta cualquier ruta de archivo para `--cdrom`,
> no es obligatorio que esté en un pool. Pero tener las ISOs en un pool las
> hace visibles en virt-manager y facilita la gestión.

---

## Anatomía del comando virt-install

Un comando `virt-install` típico tiene esta estructura:

```bash
virt-install \
    --name debian-lab \          # identidad
    --ram 2048 \                 # recursos: memoria
    --vcpus 2 \                  # recursos: CPU
    --disk size=20 \             # almacenamiento
    --cdrom /path/to/debian.iso \ # medio de instalación
    --os-variant debian12 \      # optimizaciones
    --network network=default \  # red
    --graphics spice             # visualización
```

Cada flag configura un aspecto de la VM. Vamos a desglosarlos.

---

## Flags esenciales en detalle

### `--name`: identidad de la VM

```bash
--name debian-lab
```

El nombre debe ser **único** dentro de libvirt. Este es el identificador que
usarás con `virsh` para gestionar la VM:

```bash
virsh start debian-lab
virsh shutdown debian-lab
virsh console debian-lab
```

Restricciones:
- Único en el host
- Sin espacios (usar guiones o guiones bajos)
- Sensible a mayúsculas/minúsculas

### `--ram` / `--memory`: memoria RAM

```bash
--ram 2048        # 2048 MB = 2 GB (valor en megabytes)
--memory 2048     # sinónimo de --ram
```

La RAM se reserva al arrancar la VM. Con 2048 MB, el proceso QEMU consumirá
aproximadamente 2 GB de la RAM del host (más un overhead pequeño para QEMU
mismo).

**Mínimos recomendados para lab**:

| Sistema | Mínimo | Recomendado |
|---------|--------|-------------|
| Debian 12 (sin GUI) | 512 MB | 1024 MB |
| AlmaLinux 9 (minimal) | 1024 MB | 2048 MB |
| Instalador Debian | 768 MB | 1024 MB |
| Instalador AlmaLinux | 1536 MB | 2048 MB |

> **Predicción**: si asignas menos RAM de la que necesita el instalador,
> la instalación fallará con errores crípticos o el OOM killer matará
> procesos. Es mejor ser generoso durante la instalación y reducir después.

### `--vcpus`: CPUs virtuales

```bash
--vcpus 2         # 2 CPUs virtuales
```

Cada vCPU se mapea a un hilo del host. Puedes asignar más vCPUs que cores
físicos (overcommit), pero el rendimiento se degrada si la suma total de
vCPUs activas excede los cores disponibles.

Para labs, **1-2 vCPUs** es suficiente. Más vCPUs ayudan en compilaciones
paralelas pero consumen recursos del host.

Sintaxis avanzada:

```bash
# Especificar topología (sockets × cores × threads)
--vcpus 4,sockets=1,cores=2,threads=2

# Máximo dinámico (hotplug)
--vcpus 2,maxvcpus=4
```

### `--disk`: almacenamiento

El flag `--disk` es el más versátil. Tiene múltiples formas:

```bash
# Forma 1: solo tamaño — crea qcow2 en pool default automáticamente
--disk size=20
# Crea /var/lib/libvirt/images/debian-lab.qcow2 de 20 GiB

# Forma 2: ruta explícita a imagen existente
--disk path=/srv/lab-images/mi_disco.qcow2

# Forma 3: ruta + crear si no existe
--disk path=/srv/lab-images/nuevo.qcow2,size=20,format=qcow2

# Forma 4: volumen de un pool
--disk vol=labs/base_debian.qcow2

# Forma 5: múltiples discos
--disk size=20 \
--disk size=10 \
--disk size=5
```

Parámetros adicionales del disco:

```bash
--disk path=/srv/vms/disco.qcow2,size=20,format=qcow2,bus=virtio,cache=writeback
```

| Parámetro | Significado |
|-----------|-------------|
| `path` | Ruta al archivo de imagen |
| `size` | Tamaño en GiB (solo para crear nuevos) |
| `format` | `qcow2`, `raw`, etc. |
| `bus` | Controlador: `virtio` (mejor rendimiento), `sata`, `ide` |
| `cache` | Modo caché: `none` (O_DIRECT), `writeback`, `writethrough` |

> **Predicción**: si usas `--disk size=20` sin especificar ruta, virt-install
> creará el archivo en el pool `default` con el nombre `{vm-name}.qcow2`.
> Si el pool default no existe o no está activo, fallará.

### `--cdrom` / `--location`: medio de instalación

Dos formas de proporcionar el instalador:

```bash
# Forma 1: --cdrom (ISO como unidad de CD-ROM virtual)
--cdrom /srv/iso/debian-12.9.0-amd64-netinst.iso

# Forma 2: --location (extrae kernel/initrd del medio)
--location /srv/iso/debian-12.9.0-amd64-netinst.iso
```

Diferencias clave:

| Aspecto | `--cdrom` | `--location` |
|---------|-----------|--------------|
| Cómo funciona | Monta la ISO como CD-ROM virtual | Extrae kernel+initrd, arranca directamente |
| Consola serial | No compatible directamente | Compatible (con `--extra-args`) |
| Pasar parámetros al kernel | No | Sí, via `--extra-args` |
| Kickstart/preseed | No (necesita CD) | Sí (via `--extra-args`) |
| URLs remotas | No | Sí (`--location http://...`) |

```bash
# --location permite arrancar sin GUI, ideal para servidores headless
virt-install \
    --name debian-lab \
    --ram 2048 --vcpus 2 \
    --disk size=20 \
    --location /srv/iso/debian-12.9.0-amd64-netinst.iso \
    --os-variant debian12 \
    --network network=default \
    --graphics none \
    --extra-args "console=ttyS0,115200n8"
```

> **Nota**: `--location` con ISOs locales requiere que el instalador soporte
> extracción de kernel. Funciona con Debian, Fedora, RHEL, AlmaLinux. No
> funciona con ISOs de Windows u otros sistemas propietarios.

### `--os-variant`: optimizaciones por sistema operativo

```bash
--os-variant debian12
```

Este flag consulta la base de datos `osinfo-db` y aplica automáticamente
las optimizaciones correctas para el SO:

| Lo que configura | Sin `--os-variant` | Con `--os-variant debian12` |
|-----------------|--------------------|-----------------------------|
| Bus de disco | IDE (lento) | virtio (rápido) |
| Modelo de red | e1000 (emulado) | virtio-net (rápido) |
| Timer | Por defecto | Optimizado para el kernel |
| Video | Genérico | QXL/virtio-gpu |
| Input | PS/2 | virtio-input |

Sin `--os-variant`, la VM funcionará pero con **rendimiento inferior**
porque usará dispositivos emulados en vez de paravirtualizados.

### `--network`: conexión de red

```bash
# Red NAT por defecto (la VM sale a internet via el host)
--network network=default

# Sin red
--network none

# Bridge (la VM obtiene IP en la red física del host)
--network bridge=br0

# Múltiples interfaces
--network network=default \
--network network=isolated
```

La red `default` es una red NAT creada automáticamente por libvirt con
DHCP. La estudiaremos en detalle en S06.

```bash
# Verificar que la red default existe y está activa
virsh net-list
```

```
 Name      State    Autostart   Persistent
--------------------------------------------
 default   active   yes         yes
```

### `--graphics`: cómo ver la VM

```bash
# SPICE: protocolo de escritorio remoto con buena experiencia
--graphics spice

# VNC: alternativa más universal
--graphics vnc

# Sin gráficos (solo consola serial — ideal para servidores)
--graphics none

# VNC en puerto específico
--graphics vnc,listen=0.0.0.0,port=5901
```

Cuando usas `--graphics spice` o `vnc`, virt-install abre automáticamente
`virt-viewer` si está disponible en tu escritorio. Si trabajas por SSH sin
X11 forwarding, usa `--graphics none` con consola serial.

---

## Modos de visualización: graphics

La elección de `--graphics` determina cómo interactúas con la VM durante y
después de la instalación:

```
┌──────────────────────────────────────────────────────────────────┐
│                    MODOS DE VISUALIZACIÓN                       │
├────────────────┬─────────────────────────────────────────────────┤
│                │                                                │
│  --graphics    │  Abre virt-viewer en tu escritorio              │
│  spice         │  Mejor rendimiento, copy-paste, resize         │
│                │  Necesita: entorno gráfico en el host           │
│                │                                                │
├────────────────┼─────────────────────────────────────────────────┤
│                │                                                │
│  --graphics    │  Conectar con cualquier cliente VNC             │
│  vnc           │  Compatible con clientes remotos                │
│                │  Necesita: vncviewer o similar                  │
│                │                                                │
├────────────────┼─────────────────────────────────────────────────┤
│                │                                                │
│  --graphics    │  Todo por terminal, sin GUI                     │
│  none          │  Requiere --extra-args "console=ttyS0"         │
│                │  Ideal para: SSH, servidores headless           │
│                │  Salir: Ctrl+]                                  │
│                │                                                │
└────────────────┴─────────────────────────────────────────────────┘
```

### Modo texto por consola serial (sin GUI)

Este es el modo más relevante para administración de servidores y para
trabajar por SSH:

```bash
virt-install \
    --name debian-lab \
    --ram 2048 --vcpus 2 \
    --disk size=20 \
    --location /srv/iso/debian-12.9.0-amd64-netinst.iso \
    --os-variant debian12 \
    --network network=default \
    --graphics none \
    --extra-args "console=ttyS0,115200n8"
```

El `--extra-args "console=ttyS0,115200n8"` le dice al kernel del instalador
que envíe su salida por el puerto serial, que QEMU conecta a tu terminal.

Para salir de la consola sin apagar la VM:

```
Ctrl+]
```

Para reconectarte después:

```bash
virsh console debian-lab
```

---

## Consultar variantes de OS

La base de datos `osinfo-db` contiene perfiles para cientos de sistemas
operativos. Puedes buscar con `osinfo-query`:

```bash
# Listar todas las variantes disponibles
osinfo-query os

# Buscar variantes de Debian
osinfo-query os | grep -i debian
```

```
 debian10     | Debian 10              | 10       | http://debian.org/debian/10
 debian11     | Debian 11              | 11       | http://debian.org/debian/11
 debian12     | Debian 12              | 12       | http://debian.org/debian/12
```

```bash
# Buscar variantes de AlmaLinux
osinfo-query os | grep -i alma
```

```
 almalinux8   | AlmaLinux 8            | 8        | http://almalinux.org/almalinux/8
 almalinux9   | AlmaLinux 9            | 9        | http://almalinux.org/almalinux/9
```

```bash
# Buscar por familia (todas las distros basadas en RHEL)
osinfo-query os | grep -i 'centos\|alma\|rocky\|rhel'
```

Si `osinfo-query` no encuentra tu OS, actualiza la base de datos:

```bash
# Fedora/RHEL
sudo dnf update osinfo-db

# Debian/Ubuntu
sudo apt update && sudo apt install osinfo-db
```

### Qué pasa si usas una variante incorrecta

```bash
# Variante que no existe
--os-variant nosuchos
# ERROR: Unknown OS name 'nosuchos'

# Variante incorrecta (Debian pero pones RHEL)
# La VM arrancará pero con optimizaciones equivocadas
# Puede funcionar, pero perderás rendimiento
```

> **Predicción**: si omites `--os-variant` completamente, virt-install
> mostrará un warning y usará configuración genérica. La VM funcionará
> pero con dispositivos emulados (IDE, e1000) en vez de virtio. Notarás
> la diferencia en rendimiento de disco y red.

---

## Ejemplo completo: Debian 12

### Con GUI (SPICE)

```bash
sudo virt-install \
    --name debian-lab \
    --ram 2048 \
    --vcpus 2 \
    --disk path=/var/lib/libvirt/images/debian-lab.qcow2,size=20,format=qcow2 \
    --cdrom /srv/iso/debian-12.9.0-amd64-netinst.iso \
    --os-variant debian12 \
    --network network=default \
    --graphics spice \
    --boot uefi
```

Esto:
1. Crea una VM llamada `debian-lab`
2. Con 2 GB de RAM y 2 vCPUs
3. Disco de 20 GiB en qcow2 (en pool default)
4. Monta la ISO de Debian como CD-ROM
5. Aplica optimizaciones de Debian 12 (virtio, etc.)
6. Conecta a la red NAT default
7. Abre ventana SPICE para la instalación gráfica
8. Arranca con firmware UEFI

### Sin GUI (consola serial)

```bash
sudo virt-install \
    --name debian-lab \
    --ram 2048 \
    --vcpus 2 \
    --disk path=/var/lib/libvirt/images/debian-lab.qcow2,size=20,format=qcow2 \
    --location /srv/iso/debian-12.9.0-amd64-netinst.iso \
    --os-variant debian12 \
    --network network=default \
    --graphics none \
    --extra-args "console=ttyS0,115200n8"
```

El instalador de Debian detecta que no hay framebuffer gráfico y lanza el
instalador en modo texto (ncurses) directamente en tu terminal.

```
┌──────────────────────────────────────────────────┐
│         Debian GNU/Linux installer               │
│                                                  │
│  ┌────────────────────────────────────────┐      │
│  │ Choose language                        │      │
│  │                                        │      │
│  │   C - No localization                  │      │
│  │   English                              │      │
│  │   Spanish                              │      │
│  │   ...                                  │      │
│  └────────────────────────────────────────┘      │
│                                                  │
│  <Go Back>                          <Continue>   │
└──────────────────────────────────────────────────┘
```

---

## Ejemplo completo: AlmaLinux 9

### Con GUI (SPICE)

```bash
sudo virt-install \
    --name alma-lab \
    --ram 2048 \
    --vcpus 2 \
    --disk path=/var/lib/libvirt/images/alma-lab.qcow2,size=20,format=qcow2 \
    --cdrom /srv/iso/AlmaLinux-9-latest-x86_64-minimal.iso \
    --os-variant almalinux9 \
    --network network=default \
    --graphics spice
```

### Sin GUI (consola serial)

```bash
sudo virt-install \
    --name alma-lab \
    --ram 2048 \
    --vcpus 2 \
    --disk path=/var/lib/libvirt/images/alma-lab.qcow2,size=20,format=qcow2 \
    --location /srv/iso/AlmaLinux-9-latest-x86_64-minimal.iso \
    --os-variant almalinux9 \
    --network network=default \
    --graphics none \
    --extra-args "console=ttyS0,115200n8"
```

AlmaLinux usa el instalador Anaconda, que en modo texto ofrece un menú
numerado:

```
================================================================================
Installation

 1) [x] Language settings                 2) [!] Time settings
        (English (United States))                (Timezone is not set.)
 3) [!] Installation source              4) [!] Software selection
        (Local media)                            (Minimal Install)
 5) [!] Installation Destination          6) [x] Kdump
        (No disks selected)                      (Kdump is enabled)
 7) [!] Network configuration            8) [!] Root password
        (Not connected)                          (Password is not set.)
 9) [!] User creation
        (No user will be created)

  Please make a selection from the above ['b' to begin installation, 'q' to
  quit, 'r' to refresh]:
```

---

## Qué sucede internamente al ejecutar virt-install

Cuando ejecutas `virt-install`, esta es la secuencia completa:

```
┌─────────────────────────────────────────────────────────────────────┐
│  1. VALIDACIÓN                                                     │
│     • ¿Existe ya una VM con ese nombre?                            │
│     • ¿Existe la ISO/imagen?                                       │
│     • ¿La red está activa?                                         │
│     • ¿El pool tiene espacio?                                      │
│     • ¿os-variant es válido?                                       │
├─────────────────────────────────────────────────────────────────────┤
│  2. CREACIÓN DE RECURSOS                                           │
│     • Si --disk size=20: crear imagen qcow2 (qemu-img create)      │
│     • Si --boot uefi: copiar firmware OVMF                          │
├─────────────────────────────────────────────────────────────────────┤
│  3. GENERACIÓN DE XML                                              │
│     • Construir definición XML completa de la VM                   │
│     • Incluye: CPU, memoria, discos, red, gráficos, consola       │
│     • Guardar en /etc/libvirt/qemu/{nombre}.xml                    │
├─────────────────────────────────────────────────────────────────────┤
│  4. DEFINIR + ARRANCAR                                             │
│     • virsh define → registra la VM en libvirt                     │
│     • virsh start  → lanza el proceso qemu-system-x86_64          │
├─────────────────────────────────────────────────────────────────────┤
│  5. CONECTAR A CONSOLA                                             │
│     • --graphics spice/vnc: abrir virt-viewer                      │
│     • --graphics none: conectar a consola serial                   │
│     • virt-install se queda esperando hasta que termine la          │
│       instalación (detecta reboot del guest)                       │
├─────────────────────────────────────────────────────────────────────┤
│  6. POST-INSTALACIÓN                                               │
│     • Desconectar el CD-ROM (quitar la ISO)                        │
│     • Rearrancar la VM desde el disco                              │
│     • La VM queda definida y persistente                           │
└─────────────────────────────────────────────────────────────────────┘
```

### Ver el XML generado

Después de crear la VM puedes inspeccionar todo lo que virt-install configuró:

```bash
virsh dumpxml debian-lab
```

```xml
<domain type='kvm'>
  <name>debian-lab</name>
  <uuid>a1b2c3d4-e5f6-7890-abcd-ef1234567890</uuid>
  <memory unit='KiB'>2097152</memory>
  <vcpu placement='static'>2</vcpu>
  <os>
    <type arch='x86_64' machine='pc-q35-8.2'>hvm</type>
    <boot dev='hd'/>
  </os>
  <devices>
    <disk type='file' device='disk'>
      <driver name='qemu' type='qcow2'/>
      <source file='/var/lib/libvirt/images/debian-lab.qcow2'/>
      <target dev='vda' bus='virtio'/>       <!-- virtio gracias a os-variant -->
    </disk>
    <interface type='network'>
      <source network='default'/>
      <model type='virtio'/>                 <!-- virtio gracias a os-variant -->
    </interface>
    <graphics type='spice' autoport='yes'/>
    <console type='pty'/>
    <channel type='spicevmc'/>
  </devices>
</domain>
```

Observa `bus='virtio'` y `model type='virtio'`: eso es lo que `--os-variant`
aplicó automáticamente.

### Ver el proceso QEMU resultante

```bash
ps aux | grep qemu | grep debian-lab
```

```
qemu  12345  ... qemu-system-x86_64 -name guest=debian-lab,debug-threads=on
  -S -object {"qom-type":"secret","id":"masterKey0",...}
  -machine pc-q35-8.2,usb=off,vmport=off,dump-guest-core=off,memory-backend=...
  -accel kvm -m size=2097152k
  -smp 2,sockets=2,dies=1,cores=1,threads=1
  -drive file=/var/lib/libvirt/images/debian-lab.qcow2,format=qcow2,...
  ...
```

La línea de comandos de QEMU es extremadamente larga. `virt-install` y
libvirt la construyen automáticamente a partir del XML. Esta es exactamente
la complejidad que libvirt abstrae por ti.

---

## Errores comunes

### 1. Red default no activa

```bash
# ❌ Error
virt-install ... --network network=default
# ERROR: Network not found: no network with matching name 'default'

# Diagnóstico
virsh net-list --all
# default   inactive   no

# ✅ Solución
virsh net-start default
virsh net-autostart default
```

### 2. No especificar --os-variant

```bash
# ⚠ Warning (no error, pero rendimiento pobre)
virt-install --name test --ram 1024 --vcpus 1 --disk size=10 \
    --cdrom debian.iso --network network=default --graphics none

# WARNING: No OS detected, VM performance may suffer.
# Install options: --os-variant debian12

# La VM usará ide en vez de virtio para discos
# La VM usará e1000 en vez de virtio-net para red
```

Siempre verifica la variante correcta con `osinfo-query os`.

### 3. ISO no encontrada o permisos insuficientes

```bash
# ❌ Ruta incorrecta
virt-install ... --cdrom /tmp/noexiste.iso
# ERROR: --cdrom: /tmp/noexiste.iso: No such file or directory

# ❌ Permisos (QEMU no puede leer la ISO)
ls -l /root/debian.iso
# -rw------- 1 root root ... /root/debian.iso
virt-install ... --cdrom /root/debian.iso
# ERROR: Couldn't create file monitor: Permission denied

# ✅ Mover a ubicación accesible
sudo mv /root/debian.iso /var/lib/libvirt/images/
# O ajustar permisos
sudo chmod 644 /root/debian.iso
```

### 4. VM con el mismo nombre ya existe

```bash
# ❌ Nombre duplicado
virt-install --name debian-lab ...
# ERROR: Guest name 'debian-lab' is already in use.

# Diagnóstico
virsh list --all | grep debian-lab

# ✅ Opciones:
# a) Elegir otro nombre
virt-install --name debian-lab-2 ...

# b) Eliminar la VM anterior
virsh destroy debian-lab      # forzar apagado si está corriendo
virsh undefine debian-lab --remove-all-storage   # eliminar + borrar discos
```

### 5. Sin espacio en disco

```bash
# ❌ Pool lleno
virt-install ... --disk size=500
# ERROR: Couldn't create storage volume: No space left on device

# Diagnóstico
virsh pool-info default
# Available: 2.00 GiB  ← no caben 500 GiB

# ✅ Reducir tamaño o usar otro pool
--disk size=20
# O
--disk path=/mnt/disco-grande/vm.qcow2,size=500
```

---

## Cheatsheet

```
╔══════════════════════════════════════════════════════════════════════╗
║  VIRT-INSTALL DESDE ISO — REFERENCIA RÁPIDA                        ║
╠══════════════════════════════════════════════════════════════════════╣
║                                                                    ║
║  COMANDO MÍNIMO (con GUI):                                        ║
║    virt-install --name VM --ram 2048 --vcpus 2 --disk size=20 \    ║
║      --cdrom /path/to.iso --os-variant OS --network default \      ║
║      --graphics spice                                              ║
║                                                                    ║
║  COMANDO MÍNIMO (sin GUI, consola serial):                         ║
║    virt-install --name VM --ram 2048 --vcpus 2 --disk size=20 \    ║
║      --location /path/to.iso --os-variant OS --network default \   ║
║      --graphics none --extra-args "console=ttyS0,115200n8"         ║
║                                                                    ║
║  FLAGS ESENCIALES:                                                 ║
║    --name NAME          nombre único de la VM                      ║
║    --ram N / --memory N memoria en MB                              ║
║    --vcpus N            CPUs virtuales                             ║
║    --disk size=N        disco qcow2 de N GiB (pool default)       ║
║    --disk path=P,size=N disco en ruta específica                   ║
║    --cdrom PATH         ISO como CD-ROM                            ║
║    --location PATH      extraer kernel de ISO (permite serial)     ║
║    --os-variant OS      optimizaciones (osinfo-query os)           ║
║    --network network=X  red virtual (default = NAT)                ║
║    --graphics spice|vnc|none   visualización                       ║
║    --extra-args "..."   parámetros al kernel (con --location)      ║
║    --boot uefi          arranque UEFI en vez de BIOS               ║
║                                                                    ║
║  BUSCAR OS-VARIANT:                                                ║
║    osinfo-query os | grep -i debian                                ║
║    osinfo-query os | grep -i alma                                  ║
║                                                                    ║
║  DESPUÉS DE LA INSTALACIÓN:                                        ║
║    virsh list --all              ver todas las VMs                 ║
║    virsh console VM              reconectar consola serial         ║
║    virsh start VM                arrancar VM                       ║
║    virsh shutdown VM             apagar VM                         ║
║    virsh dumpxml VM              ver XML de configuración          ║
║                                                                    ║
║  SALIR DE LA CONSOLA SERIAL:                                       ║
║    Ctrl+]                                                          ║
║                                                                    ║
╚══════════════════════════════════════════════════════════════════════╝
```

---

## Ejercicios

### Ejercicio 1: Analizar flags de virt-install

**Objetivo**: entender cada flag antes de ejecutar el primer virt-install real.

1. Consulta las variantes de OS disponibles para Debian y AlmaLinux:
   ```bash
   osinfo-query os | grep -i debian
   osinfo-query os | grep -i alma
   ```
   Anota los identificadores exactos (`debian12`, `almalinux9`, etc.).

2. Verifica que la red default está activa:
   ```bash
   sudo virsh net-list --all
   ```
   Si está inactiva, arráncala:
   ```bash
   sudo virsh net-start default
   sudo virsh net-autostart default
   ```

3. Verifica que el pool default existe y tiene espacio:
   ```bash
   sudo virsh pool-info default
   ```

4. Construye (sin ejecutar) el comando virt-install para una VM Debian:
   ```
   nombre:      debian-test
   RAM:         1024 MB
   vCPUs:       1
   disco:       15 GiB en pool default
   ISO:         (la ruta donde descargaste la ISO)
   variante:    debian12
   red:         default
   gráficos:    none (consola serial)
   ```
   Escribe el comando completo en un archivo `install_debian.sh`.

5. Revisa tu comando. ¿Usaste `--cdrom` o `--location`? ¿Por qué importa
   si elegiste `--graphics none`?

**Pregunta de reflexión**: si estás conectado al servidor por SSH sin
X11 forwarding y usas `--cdrom` con `--graphics spice`, ¿qué pasará?
¿Podrás ver el instalador? ¿Qué combinación de flags necesitas para
instalar por SSH?

---

### Ejercicio 2: Primera VM real con Debian

**Objetivo**: instalar Debian 12 en una VM por consola serial.

> **Prerequisito**: necesitas la ISO de Debian 12 netinst descargada.

1. Ejecuta la instalación:
   ```bash
   sudo virt-install \
       --name debian-primera \
       --ram 1024 \
       --vcpus 1 \
       --disk path=/var/lib/libvirt/images/debian-primera.qcow2,size=15,format=qcow2 \
       --location /srv/iso/debian-12.9.0-amd64-netinst.iso \
       --os-variant debian12 \
       --network network=default \
       --graphics none \
       --extra-args "console=ttyS0,115200n8"
   ```

2. Realiza una instalación mínima:
   - Idioma: English (o el que prefieras)
   - Particionado: usar disco entero, una sola partición
   - Software: solo "SSH server" y "standard system utilities"
   - Configurar usuario root y un usuario regular

3. Al terminar la instalación, la VM rearrancará. Verifica:
   ```bash
   virsh list
   ```

4. Conéctate a la consola:
   ```bash
   virsh console debian-primera
   ```

5. Dentro de la VM, verifica que virtio está funcionando:
   ```bash
   lsblk       # ¿ves vda (virtio) o sda (emulado)?
   ip link     # ¿ves ens2/enp1s0 (virtio-net) o eth0 (e1000)?
   ```

6. Sal de la consola con `Ctrl+]`.

7. Examina el XML que generó virt-install:
   ```bash
   sudo virsh dumpxml debian-primera | grep -A2 'disk\|interface\|graphics'
   ```

**Pregunta de reflexión**: el disco aparece como `/dev/vda` dentro de la VM.
¿Qué flag de virt-install determinó que fuera `vda` (virtio) en vez de `sda`
(SCSI/SATA emulado)?

---

### Ejercicio 3: Comparar configuraciones con y sin os-variant

**Objetivo**: ver el impacto real de `--os-variant` en la configuración.

1. Crea una VM **con** `--os-variant`:
   ```bash
   sudo virt-install \
       --name test-with-osvariant \
       --ram 512 --vcpus 1 \
       --disk size=5 \
       --os-variant debian12 \
       --network network=default \
       --graphics none \
       --import \
       --noautoconsole
   ```
   (`--import` salta la instalación, `--noautoconsole` no conecta la consola)

2. Crea otra VM **sin** `--os-variant`:
   ```bash
   sudo virt-install \
       --name test-without-osvariant \
       --ram 512 --vcpus 1 \
       --disk size=5 \
       --network network=default \
       --graphics none \
       --import \
       --noautoconsole
   ```

3. Compara los XMLs generados:
   ```bash
   sudo virsh dumpxml test-with-osvariant > /tmp/with.xml
   sudo virsh dumpxml test-without-osvariant > /tmp/without.xml
   diff /tmp/with.xml /tmp/without.xml
   ```

4. Busca diferencias en:
   - `<target dev='...' bus='...'/>` del disco (¿`virtio` vs `sata/ide`?)
   - `<model type='...'/>` de la interfaz de red (¿`virtio` vs `e1000`?)
   - Presencia de dispositivos adicionales (video, input, etc.)

5. Limpia las VMs de prueba:
   ```bash
   sudo virsh destroy test-with-osvariant 2>/dev/null
   sudo virsh destroy test-without-osvariant 2>/dev/null
   sudo virsh undefine test-with-osvariant --remove-all-storage
   sudo virsh undefine test-without-osvariant --remove-all-storage
   ```

**Pregunta de reflexión**: si `--os-variant` tiene tanto impacto en el
rendimiento, ¿por qué crees que virt-install no lo hace obligatorio y
permite crear VMs sin este flag?
