# La pila QEMU/KVM

## Índice

1. [Dos componentes, un sistema](#dos-componentes-un-sistema)
2. [QEMU: el emulador en user-space](#qemu-el-emulador-en-user-space)
3. [KVM: el acelerador en kernel-space](#kvm-el-acelerador-en-kernel-space)
4. [El flujo de ejecución completo](#el-flujo-de-ejecución-completo)
5. [Emulación de dispositivos y virtio](#emulación-de-dispositivos-y-virtio)
6. [Memoria: del guest al host](#memoria-del-guest-al-host)
7. [Verificar que la pila está activa](#verificar-que-la-pila-está-activa)
8. [Errores comunes](#errores-comunes)
9. [Cheatsheet](#cheatsheet)
10. [Ejercicios](#ejercicios)

---

## Dos componentes, un sistema

En T02 vimos la pila de forma general. Ahora profundizamos en cómo QEMU y KVM
se dividen el trabajo. Ninguno de los dos funciona bien solo para nuestro caso
de uso:

```
┌──────────────────────────────────────────────────────────────┐
│  QEMU solo (sin KVM):                                        │
│  ✓ Emula hardware completo: CPU, disco, red, USB, GPU...   │
│  ✓ Puede emular cualquier arquitectura (ARM, RISC-V, MIPS) │
│  ✗ CPU emulada: cada instrucción traducida → MUY lento     │
│                                                              │
│  KVM solo (sin QEMU):                                        │
│  ✓ Ejecuta instrucciones del guest a velocidad nativa       │
│  ✓ Maneja VM Exit/Entry eficientemente                      │
│  ✗ No sabe emular un disco, una tarjeta de red ni un USB    │
│  ✗ Es un módulo del kernel: solo expone /dev/kvm            │
│  ✗ Necesita un programa en user-space que lo use            │
│                                                              │
│  QEMU + KVM (la combinación):                                │
│  ✓ KVM ejecuta las instrucciones de CPU a velocidad nativa  │
│  ✓ QEMU emula todos los dispositivos (disco, red, USB...)  │
│  ✓ Resultado: VM rápida con hardware virtual completo       │
│                                                              │
│  QEMU provee el "cuerpo" de la VM (dispositivos)            │
│  KVM provee el "cerebro" (ejecución de CPU)                  │
└──────────────────────────────────────────────────────────────┘
```

---

## QEMU: el emulador en user-space

QEMU (Quick Emulator) es un programa que corre en user-space y emula una
**máquina completa**: placa madre, CPU, controladores de disco, tarjetas de
red, puertos USB, controlador de interrupciones, reloj, consola de video...
todo lo que un sistema operativo espera encontrar al arrancar.

### Qué emula QEMU

```
┌──────────────────────────────────────────────────────────────┐
│  Máquina virtual emulada por QEMU                            │
│                                                              │
│  ┌────────────────────────────────────────────────────┐     │
│  │  CPU:  "Intel" o "AMD" virtual (o real vía KVM)    │     │
│  │  RAM:  Bloque de memoria del host mapeado          │     │
│  │  BIOS: SeaBIOS (legacy) o OVMF (UEFI)             │     │
│  ├────────────────────────────────────────────────────┤     │
│  │  Disco:  IDE, SATA, SCSI, o virtio-blk            │     │
│  │  Red:    e1000 (Intel), rtl8139, o virtio-net      │     │
│  │  Video:  VGA, QXL (SPICE), virtio-gpu              │     │
│  │  USB:    UHCI, EHCI, xHCI (USB 1.1/2.0/3.0)       │     │
│  │  Audio:  AC97, Intel HDA                           │     │
│  │  Serial: 16550A UART (ttyS0)                       │     │
│  │  Input:  PS/2 keyboard/mouse, USB tablet           │     │
│  │  RNG:    virtio-rng                                │     │
│  │  TPM:    swtpm (TPM 2.0 virtual)                   │     │
│  ├────────────────────────────────────────────────────┤     │
│  │  Controladores:                                    │     │
│  │  - PCI/PCIe bus                                    │     │
│  │  - Interrupt controller (APIC, I/O APIC)           │     │
│  │  - DMA controller                                  │     │
│  │  - RTC (Real Time Clock)                           │     │
│  │  - PIT (timer)                                     │     │
│  └────────────────────────────────────────────────────┘     │
│                                                              │
│  El guest OS ve todo esto como hardware real.                │
│  No sabe (ni necesita saber) que está virtualizado.          │
│  Excepto con virtio: ahí el guest sabe que está en una VM   │
│  y usa drivers optimizados para ello.                        │
└──────────────────────────────────────────────────────────────┘
```

### El binario qemu-system

QEMU tiene un binario por arquitectura emulada:

```bash
# Emular x86_64 (el que usaremos en el curso)
qemu-system-x86_64

# Otros disponibles (si los instalas)
qemu-system-aarch64    # ARM 64-bit
qemu-system-arm        # ARM 32-bit
qemu-system-riscv64    # RISC-V
qemu-system-ppc64      # PowerPC
qemu-system-s390x      # IBM Z

# qemu-system-x86_64 con KVM activado:
# emula dispositivos con QEMU, ejecuta CPU con KVM
```

### QEMU como proceso Linux

Cuando arrancas una VM, QEMU corre como un proceso normal:

```bash
# Ejemplo simplificado de la línea de comandos que genera libvirt:
/usr/bin/qemu-system-x86_64 \
  -name guest=debian-lab,debug-threads=on \
  -machine pc-q35-8.2,accel=kvm \         # Tipo de placa + KVM
  -cpu host \                              # Exponer CPU real al guest
  -m 2048 \                                # 2 GB de RAM
  -smp 2,sockets=1,cores=2,threads=1 \    # 2 vCPUs
  -drive file=/var/lib/libvirt/images/debian.qcow2,format=qcow2,if=virtio \
  -netdev user,id=net0 \
  -device virtio-net-pci,netdev=net0 \
  -display spice-app \
  -monitor unix:/tmp/qemu-monitor.sock,server,nowait

# Este proceso:
# - Tiene un PID como cualquier otro
# - Consume la RAM asignada a la VM + overhead de QEMU (~50MB)
# - Tiene un hilo por cada vCPU
# - Abre /dev/kvm para la aceleración
```

---

## KVM: el acelerador en kernel-space

KVM es un módulo del kernel Linux que expone la interfaz `/dev/kvm`. Programas
en user-space (como QEMU) usan esta interfaz vía `ioctl()` para crear y
ejecutar VMs.

### /dev/kvm y la API ioctl

```
┌──────────────────────────────────────────────────────────────┐
│  Interfaz de /dev/kvm (simplificada):                        │
│                                                              │
│  QEMU (user-space)                                           │
│     │                                                        │
│     │ fd = open("/dev/kvm")                                  │
│     │                                                        │
│     │ ioctl(fd, KVM_CREATE_VM)        → crear una VM         │
│     │   └── vm_fd                                            │
│     │                                                        │
│     │ ioctl(vm_fd, KVM_CREATE_VCPU)   → crear un vCPU        │
│     │   └── vcpu_fd                                          │
│     │                                                        │
│     │ ioctl(vm_fd, KVM_SET_USER_MEMORY_REGION)               │
│     │   → mapear RAM del host como RAM del guest             │
│     │                                                        │
│     │ ioctl(vcpu_fd, KVM_SET_REGS)    → configurar registros │
│     │                                                        │
│     │ loop {                                                 │
│     │   ioctl(vcpu_fd, KVM_RUN)       → ejecutar guest      │
│     │   // Bloquea hasta que hay un VM Exit                  │
│     │   // Lee la razón del exit                             │
│     │   // Maneja el exit (I/O, MMIO, halt, etc.)            │
│     │   // Vuelve a llamar KVM_RUN                           │
│     │ }                                                      │
│     │                                                        │
│     ▼                                                        │
│  KVM (kernel module)                                         │
│     │ Configura VMCS/VMCB                                    │
│     │ Ejecuta VMLAUNCH/VMRESUME (Intel)                      │
│     │ El CPU entra en VMX non-root mode                      │
│     │ Guest ejecuta instrucciones nativas                    │
│     │ VM Exit: CPU vuelve a VMX root mode                    │
│     │ KVM examina la razón del exit                          │
│     │ Si puede resolverlo en kernel → resuelve y reentry    │
│     │ Si no → retorna a QEMU (user-space) para que lo maneje│
│     ▼                                                        │
│  Hardware (VT-x / AMD-V)                                     │
└──────────────────────────────────────────────────────────────┘
```

### Qué puede resolver KVM en kernel-space

No todos los VM Exits requieren volver a QEMU. KVM resuelve muchos en el
kernel directamente, lo cual es más rápido:

```
┌──────────────────────────────────────────────────────────────┐
│  VM Exit → ¿quién lo maneja?                                │
│                                                              │
│  Resuelto por KVM (kernel, rápido):                         │
│  ├── Acceso a MSRs (Model Specific Registers)               │
│  ├── CPUID instruction                                       │
│  ├── Control registers (CR0, CR3, CR4)                       │
│  ├── Excepciones del guest (#PF, #GP)                        │
│  ├── Interrupciones de timer (APIC)                         │
│  └── HLT (pause vCPU hasta interrupción)                    │
│                                                              │
│  Resuelto por QEMU (user-space, más lento):                 │
│  ├── I/O ports (IN/OUT) → disco, red, serial                │
│  ├── MMIO (Memory-Mapped I/O) → dispositivos PCI            │
│  ├── Señales (shutdown, reset)                               │
│  └── Acceso a dispositivos emulados                         │
│                                                              │
│  La mayoría de exits son resueltos por KVM en kernel.        │
│  Solo el I/O de dispositivos requiere volver a QEMU.        │
│  Esto minimiza los context switches kernel↔userspace.        │
└──────────────────────────────────────────────────────────────┘
```

---

## El flujo de ejecución completo

Veamos qué pasa exactamente cuando un programa dentro de la VM ejecuta
distintos tipos de instrucciones:

### Instrucción aritmética (rápida)

```
Guest ejecuta: ADD EAX, EBX

1. El CPU está en VMX non-root mode (guest mode)
2. ADD es una instrucción no-privilegiada
3. Se ejecuta directamente en el hardware → NO hay VM Exit
4. El guest continúa sin interrupciones

Costo: idéntico al nativo (0 overhead)
```

### Acceso a memoria (rápido con EPT/NPT)

```
Guest ejecuta: MOV EAX, [direccion_guest]

1. El CPU traduce la dirección del guest:
   dirección virtual (guest) → dirección física (guest)
                             → dirección física (host)
2. Con EPT (Intel) o NPT (AMD), las dos traducciones
   son en hardware, sin VM Exit
3. TLB cachea las traducciones para futuras accesos

Costo: ~1% overhead por la doble traducción
       (amortizado por el TLB)
```

### Acceso a disco (lento — pasa por QEMU)

```
Guest ejecuta: OUT 0x1F0, AL  (escribir al puerto del disco IDE)

1. CPU detecta instrucción I/O en VMX non-root mode
2. VM Exit: el CPU sale de guest mode
3. KVM (kernel) ve que es un I/O port exit
4. KVM no puede manejarlo → retorna a QEMU (user-space)
5. QEMU identifica el puerto 0x1F0 (controlador IDE)
6. QEMU traduce la operación IDE a una operación en el
   archivo qcow2 del host
7. QEMU escribe al archivo del host (syscall write)
8. QEMU llama ioctl(KVM_RUN) para reanudar el guest
9. KVM ejecuta VMRESUME → guest continúa

Costo: alto por la cadena de exits y context switches
       Por esto virtio es tan importante (reduce exits)
```

### Diagrama temporal

```
┌──────────────────────────────────────────────────────────────┐
│  Línea de tiempo de una VM                                   │
│                                                              │
│  USER-SPACE (QEMU)     KERNEL (KVM)      HARDWARE (CPU)     │
│  ─────────────────     ────────────      ─────────────       │
│                                                              │
│  ioctl(KVM_RUN) ─────▶ VMLAUNCH ────────▶ Guest mode        │
│                                           │                  │
│                                           │ ADD EAX,EBX     │
│                                           │ MOV ECX,[addr]  │
│                                           │ CMP EAX,0       │
│                                           │ JZ label        │
│                                           │  ... (miles de  │
│                                           │   instrucciones │
│                                           │   sin exits)    │
│                                           │                  │
│                                           │ OUT 0x1F0,AL    │
│                         VM Exit ◀─────────┘                  │
│                         │                                    │
│                         │ I/O exit:                          │
│                         │ no puedo                           │
│                         │ manejarlo                          │
│                         │                                    │
│  Return de ioctl ◀──────┘                                    │
│  │                                                           │
│  │ "Ah, es el                                                │
│  │  disco IDE,                                               │
│  │  sector 42"                                               │
│  │                                                           │
│  │ write(host_fd,                                            │
│  │   data, len)                                              │
│  │                                                           │
│  ioctl(KVM_RUN) ─────▶ VMRESUME ───────▶ Guest mode         │
│                                           │                  │
│                                           │ ... continúa    │
│                                                              │
│  El guest pasa >95% del tiempo en "Guest mode" (nativo)     │
│  Los exits son eventos esporádicos, no la norma             │
└──────────────────────────────────────────────────────────────┘
```

---

## Emulación de dispositivos y virtio

### Dispositivos emulados (legacy)

QEMU puede simular dispositivos reales que cualquier OS ya tiene drivers para:

```bash
# Red: emular una Intel e1000 (cualquier Linux/Windows tiene el driver)
-device e1000,netdev=net0

# Disco: emular un controlador IDE (lento pero universal)
-drive file=disk.qcow2,if=ide

# Video: VGA estándar
-vga std
```

El guest usa los mismos drivers que usaría con hardware real. No necesita
saber que está en una VM. Pero la emulación de dispositivos reales es lenta
porque cada acceso al "hardware" causa un VM Exit.

### Paravirtualización con virtio

**virtio** es un estándar de dispositivos virtuales donde el guest **sabe**
que está en una VM y usa drivers optimizados para ello:

```
┌──────────────────────────────────────────────────────────────┐
│  Dispositivo emulado (ej: e1000):                            │
│                                                              │
│  Guest driver        QEMU                  Host              │
│  ──────────          ────                  ────              │
│  "Escribir al        "Simulo los           Operación         │
│   registro 0x0E      registros de          real en la        │
│   de la e1000"       una Intel e1000:      NIC del host      │
│       │              leer registro,                          │
│       │              actualizar estado,     │                │
│       ▼              verificar bits..."     │                │
│  VM Exit ──────────▶ (mucho trabajo) ──────▶│                │
│                      Múltiples exits                         │
│                      por cada paquete                        │
│                                                              │
│  ─────────────────────────────────────────────────────       │
│                                                              │
│  Dispositivo virtio (ej: virtio-net):                        │
│                                                              │
│  Guest driver        QEMU                  Host              │
│  ──────────          ────                  ────              │
│  "Poner paquete      "Leo el paquete       Enviar por       │
│   en la virtqueue    del shared memory     NIC real          │
│   y notificar"       directamente"                          │
│       │                   │                 │                │
│       ▼                   ▼                 │                │
│  1 notificación ──▶ 1 lectura ─────────────▶│                │
│                     Mínimos exits                            │
│                     por cada paquete                         │
└──────────────────────────────────────────────────────────────┘
```

### Virtqueues: el mecanismo de comunicación

virtio usa **virtqueues**: buffers compartidos entre guest y host en memoria.
El guest pone datos en el buffer y notifica al host con un solo kick (VM Exit).
El host lee directamente de la memoria compartida sin exits adicionales.

```
┌──────────────────────────────────────────────────────────────┐
│  Shared memory (virtqueue):                                  │
│                                                              │
│  ┌────┬────┬────┬────┬────┬────┬────┬────┐                  │
│  │ pkt│ pkt│ pkt│ pkt│    │    │    │    │  ← ring buffer   │
│  │  1 │  2 │  3 │  4 │    │    │    │    │                  │
│  └────┴────┴────┴────┴────┴────┴────┴────┘                  │
│    ▲                   ▲                                     │
│    │                   │                                     │
│  Guest escribe       Host lee                                │
│  (sin VM Exit)       (sin VM Exit)                           │
│                                                              │
│  Solo 1 VM Exit para notificar: "hay paquetes nuevos"       │
│  vs. múltiples exits por paquete con dispositivos emulados   │
│                                                              │
│  Resultado: virtio-net es 2-10x más rápido que e1000        │
│  emulada, dependiendo del workload.                          │
└──────────────────────────────────────────────────────────────┘
```

### Dispositivos virtio comunes

| Dispositivo | Nombre virtio | Equivalente emulado |
|-------------|---------------|---------------------|
| Disco | virtio-blk, virtio-scsi | IDE, SATA, SCSI |
| Red | virtio-net | e1000, rtl8139 |
| Memoria (balloon) | virtio-balloon | — (no existe equivalente) |
| Consola serial | virtio-console | 16550 UART |
| Entropía | virtio-rng | — |
| Filesystem compartido | virtio-fs | 9p, NFS |
| GPU | virtio-gpu | VGA, QXL |

Para el curso, **siempre usaremos virtio** cuando sea posible. libvirt y
virt-install lo configuran automáticamente con `--os-variant`.

### Drivers virtio en el guest

Linux incluye drivers virtio en el kernel desde hace años. No necesitas
instalar nada extra. Windows necesita los drivers de Red Hat
(virtio-win ISO), pero eso no aplica a este curso.

```bash
# Dentro de una VM Linux, verificar que usa virtio:
lspci | grep -i virtio
# 00:03.0 Ethernet controller: Red Hat, Inc. Virtio network device
# 00:04.0 SCSI storage controller: Red Hat, Inc. Virtio block device

lsblk
# vda    252:0    0   20G  0  disk  ← "v" de virtio (vs "sd" de SCSI)
# ├─vda1 252:1    0  512M  0  part  /boot
# └─vda2 252:2    0 19.5G  0  part  /

# Los discos virtio aparecen como /dev/vdX (vda, vdb, vdc...)
# Los discos emulados aparecen como /dev/sdX (sda, sdb, sdc...)
```

---

## Memoria: del guest al host

### Traducción de direcciones con EPT/NPT

Sin extensiones de hardware, la virtualización de memoria requiere **shadow
page tables**: el hipervisor mantiene una copia de las tablas de páginas del
guest y las sincroniza manualmente. Esto causa muchos VM Exits.

Con EPT (Extended Page Tables, Intel) o NPT (Nested Page Tables, AMD), la
traducción es en hardware:

```
┌──────────────────────────────────────────────────────────────┐
│  Sin EPT/NPT (shadow page tables):                          │
│                                                              │
│  Guest virtual addr → Guest physical addr → Host physical    │
│       ▼                    ▼                    ▼            │
│  [Guest page table]   [Shadow page table]  [Hardware MMU]    │
│                       (mantenida por KVM)                    │
│  Cada cambio de CR3 en el guest → VM Exit → KVM actualiza  │
│  las shadow page tables → muchos exits                       │
│                                                              │
│  Con EPT/NPT:                                                │
│                                                              │
│  Guest virtual addr → Guest physical addr → Host physical    │
│       ▼                    ▼                    ▼            │
│  [Guest page table]   [EPT/NPT table]     [Hardware MMU]    │
│  (controlada por      (controlada por      (hace las dos    │
│   el guest OS)         KVM, raramente       traducciones    │
│                        modificada)          en hardware)    │
│                                                              │
│  El hardware hace la doble traducción sin VM Exits.          │
│  KVM solo configura las EPT tables al crear la VM.          │
│  Resultado: ~1% overhead vs ~10% con shadow page tables.    │
└──────────────────────────────────────────────────────────────┘
```

### Memory ballooning

virtio-balloon permite ajustar la RAM de una VM dinámicamente:

```
┌──────────────────────────────────────────────────────────────┐
│  Balloon inflado (menos RAM para el guest):                  │
│                                                              │
│  RAM del guest (2 GB asignados):                             │
│  [usado por guest][usado por guest][BALLOON][BALLOON]        │
│                                    ←── 512MB devuelto       │
│                                         al host ──▶         │
│                                                              │
│  El driver virtio-balloon "infla" dentro del guest,          │
│  consumiendo páginas de RAM. Esas páginas se marcan como     │
│  libres y el host puede reasignarlas a otras VMs.            │
│                                                              │
│  Para el curso: no necesitamos ballooning.                  │
│  Solo saber que existe y que virtio-balloon lo implementa.   │
└──────────────────────────────────────────────────────────────┘
```

---

## Verificar que la pila está activa

### Verificación paso a paso

```bash
# 1. Hardware: ¿El CPU soporta virtualización?
grep -Ec '(vmx|svm)' /proc/cpuinfo
# > 0 → sí

# 2. KVM: ¿El módulo está cargado?
lsmod | grep kvm
# kvm_intel    380928  0    (o kvm_amd)
# kvm         1146880  1 kvm_intel

# 3. /dev/kvm: ¿Existe y tiene permisos?
ls -la /dev/kvm
# crw-rw----+ 1 root kvm 10, 232 ...
# El usuario debe estar en el grupo 'kvm'

# 4. QEMU: ¿Está instalado?
qemu-system-x86_64 --version
# QEMU emulator version 8.2.0 ...

# 5. QEMU puede usar KVM: test rápido
qemu-system-x86_64 -accel help
# Accelerators supported in QEMU binary:
# kvm
# tcg    ← (Tiny Code Generator: emulación software, fallback)

# 6. libvirt: ¿El servicio está activo?
systemctl is-active libvirtd
# active

# 7. Verificación integral
virt-host-validate qemu
# QEMU: Checking for hardware virtualization : PASS
# QEMU: Checking if device /dev/kvm exists   : PASS
# ...
```

### Qué pasa si KVM no está disponible

```bash
# QEMU sin KVM funciona (modo TCG) pero es muy lento:
qemu-system-x86_64 -m 512 -hda disk.qcow2
# warning: TCG doesn't support requested feature: ...
# La VM arranca pero el boot tarda minutos en vez de segundos

# Con KVM:
qemu-system-x86_64 -enable-kvm -m 512 -hda disk.qcow2
# Boot en ~10 segundos

# Si -enable-kvm falla:
# "Could not access KVM kernel module: No such file or directory"
# → modprobe kvm kvm_intel (o kvm_amd)

# "Could not access KVM kernel module: Permission denied"
# → Agregar tu usuario al grupo kvm:
#   sudo usermod -aG kvm $USER
#   (cerrar sesión y volver a entrar)
```

---

## Errores comunes

### 1. No distinguir entre QEMU y KVM

```
❌ "No puedo usar KVM, así que no puedo usar QEMU"
   QEMU funciona sin KVM (en modo emulación TCG). Es lento
   pero funciona. KVM solo acelera la CPU.

❌ "Instalé KVM, ¿cómo creo una VM?"
   KVM solo es un módulo del kernel. Necesitas QEMU (o libvirt
   con QEMU como backend) para crear y gestionar VMs.

✅ Recordar: KVM = acelerador de CPU
             QEMU = emulador de hardware completo
             Juntos = VM rápida con hardware virtual
```

### 2. Usar dispositivos emulados en vez de virtio

```
❌ Disco IDE o SATA emulado cuando virtio está disponible
   -drive file=disk.qcow2,if=ide      # Lento
   -drive file=disk.qcow2,if=scsi     # Mejor pero no óptimo

✅ Usar virtio siempre que el guest lo soporte
   -drive file=disk.qcow2,if=virtio   # Rápido
   # Linux soporta virtio desde hace años
   # virt-install con --os-variant lo configura automáticamente

   # Dentro de la VM, verificar:
   lsblk  # vda = virtio, sda = SCSI/SATA emulado
```

### 3. Permisos de /dev/kvm

```
❌ "qemu-system-x86_64: Could not access KVM: Permission denied"
   Tu usuario no tiene permiso para abrir /dev/kvm.

✅ Agregar usuario al grupo kvm:
   sudo usermod -aG kvm $USER
   # Cerrar sesión y volver a entrar para que tome efecto
   # Verificar:
   groups | grep kvm

   # Alternativamente, verificar el grupo del dispositivo:
   ls -la /dev/kvm
   # Si es root:root en vez de root:kvm, verificar las reglas udev
```

### 4. Confundir la RAM asignada con la RAM consumida (qcow2)

```
❌ "Creé una VM de 2GB y mi host perdió 2GB inmediatamente"
   Depende del formato de disco, no de la RAM.
   - La RAM sí se reserva al arrancar la VM (2 GB en el host)
   - El disco qcow2 es thin-provisioned: crece según se usa

   qemu-img create -f qcow2 disk.qcow2 20G
   # El archivo empieza en ~200KB, NO en 20GB
   ls -lh disk.qcow2
   # 197K disk.qcow2

   # Pero la RAM sí se reserva al arrancar:
   # VM con -m 2048 → el proceso QEMU consume ~2GB en el host

✅ Planificar la RAM del host:
   Si tienes 16GB y quieres 3 VMs de 2GB cada una:
   16GB - 6GB (VMs) - 2GB (host OS) = 8GB libre
   Siempre dejar RAM para el host y para el page cache.
```

### 5. Olvidar -enable-kvm al usar QEMU directamente

```
❌ qemu-system-x86_64 -m 2048 -hda disk.qcow2
   Sin -enable-kvm, QEMU usa emulación TCG → muy lento
   El boot puede tardar 5-10 minutos en vez de 10 segundos

✅ Siempre usar -enable-kvm o -accel kvm
   qemu-system-x86_64 -enable-kvm -m 2048 -hda disk.qcow2

   O mejor: usar virt-install / virsh que activan KVM
   automáticamente. Solo necesitas -enable-kvm si usas
   qemu-system directamente (raro en la práctica).
```

---

## Cheatsheet

```
┌──────────────────────────────────────────────────────────────┐
│            LA PILA QEMU/KVM CHEATSHEET                       │
├──────────────────────────────────────────────────────────────┤
│                                                              │
│  COMPONENTES                                                 │
│  QEMU (user-space)      Emula dispositivos (disco,red,USB)  │
│  KVM  (kernel module)   Ejecuta CPU del guest (nativo)      │
│  /dev/kvm               Interfaz entre QEMU y KVM           │
│  libvirt                Capa de gestión (virsh, API)        │
│                                                              │
│  FLUJO DE EJECUCIÓN                                          │
│  QEMU llama ioctl(KVM_RUN)                                   │
│    → KVM ejecuta VMLAUNCH/VMRESUME                           │
│    → Guest corre instrucciones nativas (~95% del tiempo)    │
│    → VM Exit por instrucción privilegiada o I/O              │
│    → KVM maneja si puede (MSR, CPUID, CR, HLT)             │
│    → Si no: retorna a QEMU (dispositivos, I/O ports)       │
│    → QEMU maneja y llama KVM_RUN de nuevo                    │
│                                                              │
│  DISPOSITIVOS EMULADOS VS VIRTIO                             │
│  Emulado:  e1000, IDE, VGA     Lento (compatible)           │
│  Virtio:   virtio-net, -blk   Rápido (optimizado)          │
│  Virtqueues: shared memory     Menos VM Exits              │
│                                                              │
│  DISCOS VIRTIO                                               │
│  /dev/vdX   Disco virtio (vda, vdb, vdc...)                 │
│  /dev/sdX   Disco SCSI/SATA emulado                         │
│  lsblk      Ver tipo de disco                                │
│  lspci | grep virtio   Verificar dispositivos virtio        │
│                                                              │
│  MEMORIA                                                     │
│  EPT (Intel) / NPT (AMD)   Traducción HW de direcciones    │
│  Sin EPT: shadow page tables (lento, muchos exits)          │
│  Con EPT: doble traducción en HW (~1% overhead)             │
│  virtio-balloon              Ajustar RAM dinámicamente      │
│                                                              │
│  VERIFICAR PILA                                              │
│  grep -Ec '(vmx|svm)' /proc/cpuinfo   CPU soporta?        │
│  lsmod | grep kvm                      Módulo cargado?     │
│  ls -la /dev/kvm                       Device existe?       │
│  qemu-system-x86_64 --version          QEMU instalado?     │
│  qemu-system-x86_64 -accel help        KVM disponible?     │
│  systemctl is-active libvirtd          libvirt activo?      │
│  virt-host-validate qemu               Todo junto           │
│                                                              │
│  PERMISOS                                                    │
│  usermod -aG kvm $USER     Acceso a /dev/kvm                │
│  usermod -aG libvirt $USER Acceso a libvirt                 │
│                                                              │
│  BINARIOS QEMU                                               │
│  qemu-system-x86_64       VM x86 (con o sin KVM)           │
│  qemu-img                 Gestión de imágenes de disco      │
│  qemu-system-aarch64      VM ARM (solo emulación en x86)    │
└──────────────────────────────────────────────────────────────┘
```

---

## Ejercicios

### Ejercicio 1: Anatomía de un proceso QEMU

Examina cómo QEMU interactúa con KVM observando un proceso en ejecución:

**Tareas:**
1. Si tienes una VM corriendo, localiza su proceso:
   ```bash
   ps aux | grep qemu-system
   ```
   Si no tienes una VM aún, solo lee la salida de ejemplo y pasa al paso 5.
2. Copia la línea de comandos completa del proceso y identifica:
   - `-machine ... accel=kvm` → ¿está usando KVM?
   - `-m ...` → ¿cuánta RAM tiene asignada?
   - `-smp ...` → ¿cuántas vCPUs?
   - `-drive ...` → ¿qué disco(s) tiene? ¿qcow2 o raw? ¿virtio o IDE?
   - `-device virtio-net...` → ¿usa virtio para red?
3. Verifica que el proceso tiene `/dev/kvm` abierto:
   ```bash
   ls -la /proc/$(pgrep -f qemu-system)/fd | grep kvm
   ```
4. Cuenta los hilos del proceso (deberían corresponder a las vCPUs + hilos de I/O):
   ```bash
   ps -T -p $(pgrep -f qemu-system) | wc -l
   ```
5. Sin VM corriendo, verifica que tu pila está lista para crear una:
   ```bash
   qemu-system-x86_64 -accel help
   virt-host-validate qemu
   ```

**Pregunta de reflexión:** El proceso QEMU tiene abierto `/dev/kvm` como file
descriptor. Cada llamada `ioctl(fd, KVM_RUN)` ejecuta el guest hasta el
siguiente VM Exit. ¿Qué pasaría si el proceso QEMU muere inesperadamente
(ej: `kill -9`)? ¿Los datos del guest se pierden? ¿El estado de /dev/kvm
se limpia automáticamente?

---

### Ejercicio 2: Identificar virtio dentro de una VM

Verifica qué dispositivos usa una VM y si son virtio o emulados:

**Tareas:**
1. Dentro de una VM Linux (o al crear una más adelante), ejecuta:
   ```bash
   lspci
   # Busca "Virtio" en la salida
   ```
2. Identifica el tipo de disco:
   ```bash
   lsblk
   # vdX = virtio, sdX = SCSI/SATA emulado
   ```
3. Identifica el driver de red:
   ```bash
   ip link show
   # Típicamente: enp0s3 (emulado) vs enp1s0 (virtio)
   # O directamente:
   ethtool -i enp1s0 2>/dev/null | head -3
   # driver: virtio_net
   ```
4. Lista los módulos virtio cargados:
   ```bash
   lsmod | grep virtio
   # virtio_net, virtio_blk, virtio_pci, virtio_ring, virtio
   ```
5. Si algún dispositivo NO es virtio, identifica cuál es y por qué podría
   haberse elegido un dispositivo emulado (ej: consola VGA, controlador USB)

**Pregunta de reflexión:** ¿Por qué la consola de video suele ser VGA o QXL
emulado en vez de virtio-gpu? ¿En qué caso sería beneficioso usar virtio-gpu?

---

### Ejercicio 3: Comparar overhead de dispositivos

Estima la diferencia de rendimiento entre virtio y emulación (lectura):

**Tareas:**
1. Lee la documentación del formato qcow2:
   ```bash
   qemu-img --help | head -20
   man qemu-img  # si está disponible
   ```
2. Crea dos imágenes de disco idénticas:
   ```bash
   qemu-img create -f qcow2 /tmp/test-virtio.qcow2 1G
   qemu-img create -f qcow2 /tmp/test-ide.qcow2 1G
   ```
3. Inspecciona las imágenes:
   ```bash
   qemu-img info /tmp/test-virtio.qcow2
   # Nota el "virtual size" vs el "disk size" (thin provisioning)
   ls -lh /tmp/test-virtio.qcow2
   # Mucho más pequeño que 1G
   ```
4. Sin VM corriendo, piensa en la diferencia conceptual:
   - Con IDE emulado: guest escribe al "registro del controlador IDE" →
     VM Exit → QEMU simula el controlador → escribe al qcow2
   - Con virtio-blk: guest pone datos en la virtqueue → un solo kick →
     QEMU lee de shared memory → escribe al qcow2
5. Limpia las imágenes de prueba:
   ```bash
   rm /tmp/test-virtio.qcow2 /tmp/test-ide.qcow2
   ```

**Pregunta de reflexión:** El formato qcow2 tiene overhead propio
(metadata, COW, compresión opcional). Si el disco virtual fuera raw en vez
de qcow2, ¿el rendimiento de I/O mejoraría? ¿En qué situaciones el formato
raw tiene ventaja sobre qcow2 y cuándo la diferencia es despreciable?
