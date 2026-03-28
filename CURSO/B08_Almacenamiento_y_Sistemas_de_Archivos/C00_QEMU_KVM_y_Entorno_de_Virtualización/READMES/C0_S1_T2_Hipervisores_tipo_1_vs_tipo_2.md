# Hipervisores tipo 1 vs tipo 2

## Índice

1. [Qué es un hipervisor](#qué-es-un-hipervisor)
2. [Tipo 1: bare-metal](#tipo-1-bare-metal)
3. [Tipo 2: hosted](#tipo-2-hosted)
4. [KVM: el caso especial](#kvm-el-caso-especial)
5. [Comparación práctica](#comparación-práctica)
6. [Por qué KVM para este curso](#por-qué-kvm-para-este-curso)
7. [Errores comunes](#errores-comunes)
8. [Cheatsheet](#cheatsheet)
9. [Ejercicios](#ejercicios)

---

## Qué es un hipervisor

Un **hipervisor** (o Virtual Machine Monitor — VMM) es el software responsable
de crear y gestionar máquinas virtuales. Es la capa que controla el acceso de
las VMs al hardware real, aislándolas entre sí y del host.

```
┌──────────────────────────────────────────────────────────────┐
│  Responsabilidades de un hipervisor:                         │
│                                                              │
│  1. Crear VMs: asignar CPU, RAM, disco virtuales            │
│  2. Aislar: que una VM no pueda leer la memoria de otra     │
│  3. Multiplexar: repartir los recursos reales entre VMs     │
│  4. Interceptar: atrapar instrucciones privilegiadas        │
│  5. Emular I/O: proveer disco, red, USB virtuales           │
│  6. Gestionar: arrancar, pausar, migrar, snapshot VMs       │
└──────────────────────────────────────────────────────────────┘
```

La diferencia clave entre hipervisores es **dónde se ubican** respecto al
hardware y al sistema operativo host.

---

## Tipo 1: bare-metal

Un hipervisor tipo 1 corre **directamente sobre el hardware**, sin un sistema
operativo host intermedio. Es la primera capa de software que se ejecuta
al encender la máquina.

```
┌──────────────────────────────────────────────────────────────┐
│  Hipervisor Tipo 1 (bare-metal)                              │
│                                                              │
│  ┌──────────┐  ┌──────────┐  ┌──────────┐                  │
│  │  VM 1    │  │  VM 2    │  │  VM 3    │                  │
│  │  (Linux) │  │  (Windows)│  │  (Linux) │                  │
│  │  Apps    │  │  Apps    │  │  Apps    │                  │
│  │  Kernel  │  │  Kernel  │  │  Kernel  │                  │
│  └────┬─────┘  └────┬─────┘  └────┬─────┘                  │
│       │              │              │                        │
│  ┌────▼──────────────▼──────────────▼────┐                  │
│  │        HIPERVISOR (Tipo 1)            │                  │
│  │   Corre directamente sobre hardware    │                  │
│  │   No hay OS host debajo                │                  │
│  └────────────────┬──────────────────────┘                  │
│                   │                                          │
│  ┌────────────────▼──────────────────────┐                  │
│  │           HARDWARE REAL               │                  │
│  │     CPU    RAM    Disco    NIC         │                  │
│  └───────────────────────────────────────┘                  │
│                                                              │
│  No hay sistema operativo entre el hipervisor y el hardware │
│  El hipervisor ES el primer software que arranca            │
└──────────────────────────────────────────────────────────────┘
```

### Ejemplos de tipo 1

| Hipervisor | Fabricante | Uso típico |
|------------|-----------|------------|
| VMware ESXi | VMware/Broadcom | Data centers empresariales |
| Microsoft Hyper-V | Microsoft | Servidores Windows, Azure |
| Xen | Open source (Citrix) | AWS EC2 (históricamente), Qubes OS |
| Oracle VM Server | Oracle | Entornos Oracle |

### Características

- **Rendimiento**: acceso directo al hardware, mínimo overhead
- **Seguridad**: superficie de ataque mínima (no hay OS host que comprometer)
- **Complejidad**: requiere instalar el hipervisor en bare-metal, gestionar
  desde consola remota o interfaz de management
- **Uso**: data centers, producción, entornos empresariales

### Xen: arquitectura Dom0/DomU

Xen tiene un diseño particular. El hipervisor es mínimo y delega la gestión
de hardware a un dominio privilegiado (Dom0):

```
┌──────────────────────────────────────────────────────────────┐
│  Arquitectura Xen                                            │
│                                                              │
│  ┌────────────┐  ┌────────────┐  ┌────────────┐            │
│  │  Dom0       │  │  DomU 1    │  │  DomU 2    │            │
│  │  (Linux)    │  │  (guest)   │  │  (guest)   │            │
│  │  Privilegiado│ │  Sin priv. │  │  Sin priv. │            │
│  │  Gestiona   │  │            │  │            │            │
│  │  hardware   │  │            │  │            │            │
│  └──────┬──────┘  └──────┬─────┘  └──────┬─────┘            │
│         │                │               │                   │
│  ┌──────▼────────────────▼───────────────▼──────┐           │
│  │              Xen Hypervisor                   │           │
│  │  (mínimo: solo scheduling y memoria)          │           │
│  └──────────────────┬────────────────────────────┘           │
│                     │                                        │
│  ┌──────────────────▼────────────────────────────┐           │
│  │              HARDWARE                          │           │
│  └────────────────────────────────────────────────┘           │
│                                                              │
│  Dom0 = dominio de gestión con acceso a drivers reales       │
│  DomU = dominios sin privilegio (las VMs de usuario)         │
└──────────────────────────────────────────────────────────────┘
```

---

## Tipo 2: hosted

Un hipervisor tipo 2 corre **como una aplicación** dentro de un sistema
operativo host convencional. El host arranca normalmente (Linux, Windows, macOS)
y luego el hipervisor se ejecuta como un programa más.

```
┌──────────────────────────────────────────────────────────────┐
│  Hipervisor Tipo 2 (hosted)                                  │
│                                                              │
│  ┌──────────┐  ┌──────────┐     ┌──────────┐               │
│  │  VM 1    │  │  VM 2    │     │ App host │               │
│  │  (Linux) │  │ (Windows)│     │ (Firefox,│               │
│  │  Apps    │  │  Apps    │     │  vim...) │               │
│  │  Kernel  │  │  Kernel  │     │          │               │
│  └────┬─────┘  └────┬─────┘     └────┬─────┘               │
│       │              │               │                      │
│  ┌────▼──────────────▼───┐           │                      │
│  │  HIPERVISOR (Tipo 2)  │           │                      │
│  │  (aplicación en el    │           │                      │
│  │   host OS)            │           │                      │
│  └────────┬──────────────┘           │                      │
│           │                          │                      │
│  ┌────────▼──────────────────────────▼──────┐               │
│  │         SISTEMA OPERATIVO HOST            │               │
│  │     (Linux, Windows, macOS)               │               │
│  │     Kernel del host                       │               │
│  └──────────────────┬────────────────────────┘               │
│                     │                                        │
│  ┌──────────────────▼────────────────────────┐               │
│  │           HARDWARE REAL                    │               │
│  └────────────────────────────────────────────┘               │
│                                                              │
│  El hipervisor es un proceso más del host OS                 │
│  Comparte los recursos con las apps normales del host        │
└──────────────────────────────────────────────────────────────┘
```

### Ejemplos de tipo 2

| Hipervisor | Plataformas | Uso típico |
|------------|-------------|------------|
| VirtualBox | Linux, Windows, macOS | Desarrollo, testing, escritorio |
| VMware Workstation | Linux, Windows | Desarrollo, testing profesional |
| VMware Fusion | macOS | VMs en Mac |
| GNOME Boxes | Linux | Uso casual, integrado con GNOME |
| Parallels Desktop | macOS | Mejor rendimiento en Mac |

### Características

- **Facilidad**: se instala como cualquier aplicación, GUI intuitiva
- **Flexibilidad**: convive con el uso normal del escritorio
- **Rendimiento**: overhead adicional por pasar a través del OS host
- **Uso**: desarrollo, pruebas, uso personal y educativo

---

## KVM: el caso especial

KVM (Kernel-based Virtual Machine) no encaja limpiamente en ninguna categoría.
Es un **módulo del kernel Linux** que convierte a Linux mismo en un hipervisor
tipo 1.

### ¿Tipo 1 o tipo 2?

```
┌──────────────────────────────────────────────────────────────┐
│  El debate: ¿KVM es tipo 1 o tipo 2?                         │
│                                                              │
│  Argumento para Tipo 2:                                      │
│  "KVM corre dentro de Linux, y Linux es un OS host.          │
│   Las VMs son procesos del host. Eso es tipo 2."             │
│                                                              │
│  Argumento para Tipo 1:                                      │
│  "Al cargar kvm.ko, Linux SE CONVIERTE en el hipervisor.     │
│   No es una app encima de Linux — es Linux transformado      │
│   en hipervisor. Las instrucciones del guest van directo      │
│   al hardware vía /dev/kvm. Eso es tipo 1."                  │
│                                                              │
│  Consenso:                                                   │
│  KVM es un hipervisor tipo 1 implementado como módulo        │
│  del kernel Linux. Linux actúa simultáneamente como host     │
│  OS y como hipervisor. No hay capa extra entre KVM y el      │
│  hardware — KVM ES parte del kernel.                         │
└──────────────────────────────────────────────────────────────┘
```

### Arquitectura de KVM

```
┌──────────────────────────────────────────────────────────────┐
│  Arquitectura KVM                                            │
│                                                              │
│  ┌──────────────┐  ┌──────────────┐   ┌──────────┐         │
│  │  VM 1        │  │  VM 2        │   │  Apps    │         │
│  │  ┌────────┐  │  │  ┌────────┐  │   │  host    │         │
│  │  │ Apps   │  │  │  │ Apps   │  │   │ (Firefox,│         │
│  │  │ Kernel │  │  │  │ Kernel │  │   │  vim...) │         │
│  │  └────────┘  │  │  └────────┘  │   │          │         │
│  │  QEMU process│  │  QEMU process│   │          │         │
│  │  (user-space)│  │  (user-space)│   │          │         │
│  └──────┬───────┘  └──────┬───────┘   └────┬─────┘         │
│         │                 │                │                │
│  ═══════▼═════════════════▼════════════════▼═════════       │
│  ┌───────────────────────────────────────────────────┐      │
│  │              Kernel Linux                          │      │
│  │  ┌──────────────────────┐                         │      │
│  │  │     KVM module        │  ← /dev/kvm            │      │
│  │  │  (kvm.ko +            │                         │      │
│  │  │   kvm_intel/amd.ko)   │                         │      │
│  │  └──────────────────────┘                         │      │
│  │  Scheduler, memoria, drivers, filesystem...        │      │
│  └──────────────────┬────────────────────────────────┘      │
│                     │                                        │
│  ┌──────────────────▼────────────────────────────────┐      │
│  │    HARDWARE (CPU con VT-x/AMD-V, RAM, disco)       │      │
│  └────────────────────────────────────────────────────┘      │
│                                                              │
│  Puntos clave:                                               │
│  - Cada VM es un proceso Linux (visible con ps)              │
│  - KVM maneja las VM Exits/Entries en kernel-space           │
│  - QEMU (user-space) emula los dispositivos (disco, red)    │
│  - Linux gestiona scheduling de VMs como procesos normales  │
│  - Las instrucciones del guest van directo al CPU vía KVM   │
└──────────────────────────────────────────────────────────────┘
```

### Por qué KVM es la opción estándar en Linux

```
┌──────────────────────────────────────────────────────────────┐
│  Ventajas de KVM:                                            │
│                                                              │
│  1. Integrado en el kernel Linux desde la versión 2.6.20     │
│     (febrero 2007). No es un paquete externo — es parte      │
│     del kernel mainline. Ya está ahí cuando instalas Linux.  │
│                                                              │
│  2. Open source (GPL v2). Sin licencias, sin costos.         │
│                                                              │
│  3. Reutiliza toda la infraestructura de Linux:              │
│     - Scheduler del kernel → scheduling de VMs              │
│     - Memory management → asignación de RAM a VMs           │
│     - Device drivers → acceso a hardware real               │
│     - cgroups → límites de recursos por VM                  │
│     - SELinux/AppArmor → seguridad de VMs                   │
│     - No reinventa nada: aprovecha décadas de desarrollo    │
│                                                              │
│  4. Rendimiento nativo: instrucciones del guest corren       │
│     directamente en el CPU. Solo los VM Exits pasan por KVM.│
│                                                              │
│  5. Ecosistema: libvirt, QEMU, virt-manager, oVirt,          │
│     OpenStack, Proxmox — todas construyen sobre KVM.         │
│                                                              │
│  6. Es lo que usan los grandes:                              │
│     Google Cloud, DigitalOcean, Linode, Hetzner — todos     │
│     usan KVM. AWS migró de Xen a KVM (Nitro).              │
└──────────────────────────────────────────────────────────────┘
```

### Cada VM es un proceso Linux

Esta es una consecuencia profunda de la arquitectura KVM. Como cada VM es un
proceso, puedes usar herramientas estándar de Linux:

```bash
# Ver los procesos QEMU (uno por VM)
ps aux | grep qemu

# Ejemplo de output:
# libvirt+ 12345  15.2  8.0  qemu-system-x86_64 -name guest=debian-lab ...
# libvirt+ 12400   8.1  4.0  qemu-system-x86_64 -name guest=alma-lab ...

# Cada proceso QEMU:
# - Tiene un PID normal
# - Consume CPU y RAM medible con top/htop
# - Puede ser limitado con cgroups
# - Está sujeto al scheduler de Linux
# - Puede ser killed con kill (equivale a desenchufar la VM)

# Ver los hilos de una VM (cada vCPU es un hilo)
ps -T -p $(pgrep -f "name guest=debian-lab")

# Monitorear recursos de VMs con top
top -p $(pgrep -d, -f qemu)
```

---

## Comparación práctica

### Tipo 1 vs Tipo 2 vs KVM

```
┌─────────────────┬──────────────┬──────────────┬──────────────┐
│                 │   Tipo 1     │   Tipo 2     │   KVM        │
│                 │  (ESXi, Xen) │ (VBox, VMW)  │              │
├─────────────────┼──────────────┼──────────────┼──────────────┤
│ Corre sobre     │ Hardware     │ OS host      │ Kernel Linux │
│                 │ directamente │              │ (módulo)     │
├─────────────────┼──────────────┼──────────────┼──────────────┤
│ Rendimiento     │ Excelente    │ Bueno        │ Excelente    │
│                 │ (~97%)       │ (~90%)       │ (~96%)       │
├─────────────────┼──────────────┼──────────────┼──────────────┤
│ Uso del host    │ Solo VMs     │ VMs + apps   │ VMs + apps   │
│                 │ (dedicado)   │ normales     │ normales     │
├─────────────────┼──────────────┼──────────────┼──────────────┤
│ Instalación     │ Instalar el  │ Instalar app │ modprobe kvm │
│                 │ hipervisor   │ en el OS     │ (ya incluido)│
│                 │ como OS      │              │              │
├─────────────────┼──────────────┼──────────────┼──────────────┤
│ GUI             │ Web remota   │ GUI local    │ virt-manager │
│                 │ (vSphere)    │ integrada    │ o CLI (virsh)│
├─────────────────┼──────────────┼──────────────┼──────────────┤
│ Gestión CLI     │ SSH + tools  │ Limitada     │ virsh, qemu  │
│                 │ propios      │              │ (excelente)  │
├─────────────────┼──────────────┼──────────────┼──────────────┤
│ Licencia        │ Comercial    │ Gratis/Pago  │ Open source  │
│                 │ (ESXi pago)  │ (VBox gratis)│ (GPL)        │
├─────────────────┼──────────────┼──────────────┼──────────────┤
│ Uso típico      │ Data center  │ Escritorio   │ Servidores   │
│                 │ Producción   │ Desarrollo   │ y escritorio │
└─────────────────┴──────────────┴──────────────┴──────────────┘
```

### KVM vs VirtualBox: la comparación que importa

Para un usuario de Linux decidiendo entre los dos:

```
┌──────────────────────────────────────────────────────────────┐
│  KVM (QEMU + libvirt):                                       │
│  + Ya incluido en el kernel (no instalar drivers extra)     │
│  + Mejor rendimiento (tipo 1)                                │
│  + Mejor integración con Linux (cgroups, SELinux, virsh)    │
│  + Usado en producción (clouds, servidores)                 │
│  + CLI completo y scriptable (virsh, virt-install)          │
│  - GUI menos pulida que VirtualBox                          │
│  - Curva de aprendizaje más pronunciada                     │
│                                                              │
│  VirtualBox:                                                 │
│  + GUI intuitiva (fácil para principiantes)                 │
│  + Multiplataforma (Linux, Windows, macOS)                  │
│  + Shared folders y clipboard fáciles de configurar         │
│  - Requiere módulo del kernel externo (DKMS)                │
│  - Peor rendimiento que KVM                                  │
│  - Puede conflictuar con KVM (no usar ambos a la vez)       │
│  - No usado en producción seria                              │
│                                                              │
│  Para este curso: usamos KVM                                │
│  - Es lo que encontrarás en servidores reales               │
│  - Mejor rendimiento para labs intensivos (RAID, LVM)       │
│  - virsh y virt-install son herramientas de sysadmin real   │
└──────────────────────────────────────────────────────────────┘
```

---

## Por qué KVM para este curso

La decisión no es arbitraria. KVM es la herramienta que un sysadmin Linux
usa en producción:

```
┌──────────────────────────────────────────────────────────────┐
│  Stack que aprenderemos en este capítulo:                     │
│                                                              │
│  ┌─────────────────────────────────┐                        │
│  │  virt-manager (GUI)             │  ← Interfaz gráfica   │
│  │  virsh (CLI)                    │  ← Gestión por línea  │
│  │  virt-install (CLI)             │  ← Crear VMs          │
│  └───────────────┬─────────────────┘                        │
│                  │ API                                       │
│  ┌───────────────▼─────────────────┐                        │
│  │  libvirt (libvirtd)             │  ← Capa de abstracción│
│  │  Gestión unificada de VMs       │    (funciona con KVM, │
│  │  XML de definición              │     Xen, QEMU, LXC)   │
│  └───────────────┬─────────────────┘                        │
│                  │                                           │
│  ┌───────────────▼─────────────────┐                        │
│  │  QEMU (user-space)              │  ← Emula dispositivos │
│  │  Emulación de disco, red, USB   │    (I/O virtual)      │
│  └───────────────┬─────────────────┘                        │
│                  │ /dev/kvm                                  │
│  ┌───────────────▼─────────────────┐                        │
│  │  KVM (kernel module)            │  ← Ejecuta CPU del    │
│  │  kvm.ko + kvm_intel/amd.ko     │    guest a velocidad  │
│  │                                 │    nativa              │
│  └───────────────┬─────────────────┘                        │
│                  │                                           │
│  ┌───────────────▼─────────────────┐                        │
│  │  HARDWARE (VT-x / AMD-V)       │                        │
│  └─────────────────────────────────┘                        │
│                                                              │
│  Este stack es el mismo que usan:                            │
│  - Proxmox VE (plataforma de virtualización)                │
│  - oVirt / Red Hat Virtualization                            │
│  - OpenStack (cloud computing)                               │
│  - Google Cloud Platform                                     │
│  Aprender a usar este stack es directamente aplicable.       │
└──────────────────────────────────────────────────────────────┘
```

### libvirt como capa de abstracción

Un punto clave: **libvirt** no es un hipervisor. Es una capa de abstracción
que provee una API uniforme para gestionar distintos hipervisores:

```bash
# Mismo virsh, distintos backends:
virsh -c qemu:///system list     # KVM local
virsh -c xen:///system list      # Xen local
virsh -c qemu+ssh://host/system  # KVM remoto por SSH
```

Para el curso, siempre usaremos `qemu:///system` (KVM local).

---

## Errores comunes

### 1. Creer que tipo 1 siempre es mejor que tipo 2

```
❌ "Tipo 1 es superior, siempre debería usarse"
   Tipo 1 está diseñado para dedicar una máquina entera a la
   virtualización. Si quieres también usar tu laptop para navegar,
   programar y correr VMs, tipo 2 (o KVM) es más práctico.

✅ Depende del caso de uso:
   - Servidor dedicado a VMs → Tipo 1 (ESXi, Proxmox con KVM)
   - Laptop de desarrollo → KVM o VirtualBox (convive con el escritorio)
   - Data center → Tipo 1 (rendimiento máximo, sin overhead)
   - Aprender virtualización → KVM (es tipo 1 y convive con el desktop)
```

### 2. Intentar usar VirtualBox y KVM simultáneamente

```
❌ Tener VMs corriendo en VirtualBox y KVM al mismo tiempo
   Ambos necesitan control exclusivo de las extensiones VT-x/AMD-V.
   En algunos kernels, cargar el módulo vboxdrv bloquea kvm y viceversa.

✅ Elegir uno y descargar el otro:
   # Para usar KVM, descargar VirtualBox:
   sudo modprobe -r vboxdrv vboxnetflt vboxnetadp

   # Para usar VirtualBox, verificar que KVM no tiene VMs activas:
   virsh list --all
   # Y si es necesario:
   sudo modprobe -r kvm_intel kvm  # o kvm_amd

   # En kernels modernos (5.x+), pueden coexistir los módulos,
   # pero no ambos usando VMs activamente al mismo tiempo.
```

### 3. Clasificar KVM como tipo 2

```
❌ "KVM es tipo 2 porque corre dentro de Linux"
   KVM no corre "dentro de" Linux — es PARTE de Linux.
   Es un módulo del kernel, no una aplicación en user-space.
   Las instrucciones del guest van directo al hardware vía KVM,
   sin pasar por ninguna capa de user-space.

✅ KVM es tipo 1 implementado como módulo del kernel:
   - Las instrucciones privilegiadas del guest causan VM Exit
     directamente en el kernel (kvm.ko), no en user-space
   - QEMU (user-space) solo maneja la emulación de I/O
   - El rendimiento de CPU es idéntico a un tipo 1 bare-metal
```

### 4. Confundir QEMU solo con QEMU+KVM

```
❌ "QEMU es lento, mejor uso VirtualBox"
   QEMU solo (emulación) es lento. QEMU + KVM es rápido.
   Cuando alguien dice "usa QEMU/KVM" o "usa KVM", se refiere
   a la combinación: QEMU para dispositivos, KVM para CPU.

✅ La nomenclatura:
   "QEMU"     → puede referirse a emulación pura o a QEMU+KVM
   "KVM"      → siempre implica QEMU+KVM (KVM solo no sirve)
   "QEMU/KVM" → explícitamente la combinación (lo más claro)

   En libvirt y virt-install, KVM se usa automáticamente cuando
   está disponible. No necesitas especificarlo manualmente.
```

### 5. Pensar que libvirt es el hipervisor

```
❌ "Instalé libvirt, ya tengo un hipervisor"
   libvirt es una capa de gestión, no un hipervisor. Necesitas
   KVM (o QEMU, o Xen) como backend.

✅ La pila completa:
   libvirt → gestión (API, XML, virsh, virt-manager)
   QEMU   → emulación de dispositivos
   KVM    → ejecución de instrucciones del guest

   Sin KVM, libvirt puede usar QEMU en modo emulación (lento).
   Sin QEMU, KVM no puede proveer discos ni red virtuales.
   Sin libvirt, puedes usar QEMU+KVM directamente (incómodo).
   Los tres juntos forman el stack completo.
```

---

## Cheatsheet

```
┌──────────────────────────────────────────────────────────────┐
│      HIPERVISORES TIPO 1 VS TIPO 2 CHEATSHEET               │
├──────────────────────────────────────────────────────────────┤
│                                                              │
│  TIPO 1 (BARE-METAL)                                         │
│  Corre directo sobre hardware, sin OS host                  │
│  Ejemplos: ESXi, Hyper-V, Xen                               │
│  Uso: data centers, producción                               │
│  Rendimiento: ~97% del nativo                                │
│                                                              │
│  TIPO 2 (HOSTED)                                             │
│  Corre como aplicación dentro de un OS host                 │
│  Ejemplos: VirtualBox, VMware Workstation                    │
│  Uso: desarrollo, escritorio                                 │
│  Rendimiento: ~90% del nativo                                │
│                                                              │
│  KVM (TIPO 1 EN KERNEL LINUX)                                │
│  Módulo del kernel: kvm.ko + kvm_intel.ko / kvm_amd.ko     │
│  Convierte Linux en hipervisor tipo 1                       │
│  Cada VM = proceso QEMU con acceso a /dev/kvm               │
│  Rendimiento: ~96% del nativo                                │
│                                                              │
│  STACK COMPLETO                                              │
│  virt-manager / virsh    Interfaces de usuario              │
│  libvirt (libvirtd)      Capa de gestión (API)              │
│  QEMU                    Emula dispositivos (I/O)           │
│  KVM                     Ejecuta CPU del guest (nativo)     │
│  VT-x / AMD-V            Extensiones de hardware            │
│                                                              │
│  VERIFICAR COMPONENTES                                       │
│  lsmod | grep kvm            Módulo KVM cargado?            │
│  ls -la /dev/kvm             Device node disponible?        │
│  ps aux | grep qemu          Procesos QEMU (VMs activas)   │
│  virsh -c qemu:///system list  Listar VMs vía libvirt       │
│  systemctl status libvirtd   Estado del servicio libvirt    │
│                                                              │
│  VMs COMO PROCESOS LINUX                                     │
│  ps aux | grep qemu          Ver VMs como procesos          │
│  top -p $(pgrep -d, qemu)   Monitorear recursos            │
│  kill <pid>                  Apagado forzado (= destroy)    │
│  taskset -p <pid>            Ver/fijar CPU affinity         │
│                                                              │
│  CUÁNDO USAR QUÉ                                            │
│  Servidor dedicado a VMs    → Tipo 1 (ESXi, Proxmox/KVM)   │
│  Laptop Linux para labs     → KVM (ya está en el kernel)    │
│  Laptop Win/Mac para labs   → VirtualBox o VMware           │
│  Cloud (AWS, GCP)           → KVM (lo usan internamente)   │
└──────────────────────────────────────────────────────────────┘
```

---

## Ejercicios

### Ejercicio 1: Identificar tu hipervisor

Determina qué tipo de hipervisor tienes disponible y su estado:

**Tareas:**
1. Verifica si estás en una VM o en hardware real:
   ```bash
   systemd-detect-virt
   # "none" → hardware real
   # Otro valor → estás dentro de una VM
   ```
2. Verifica si KVM está disponible:
   ```bash
   lsmod | grep kvm
   ls -la /dev/kvm
   ```
3. Verifica si VirtualBox está instalado:
   ```bash
   lsmod | grep vbox
   which VBoxManage 2>/dev/null && VBoxManage --version
   ```
4. Si tienes ambos (KVM y VBox), verifica que no estén en conflicto:
   ```bash
   dmesg | grep -iE '(kvm|vbox)' | tail -20
   ```
5. Documenta tu stack:
   - ¿Qué CPU tienes? (`lscpu | head -15`)
   - ¿Qué kernel? (`uname -r`)
   - ¿KVM disponible? ¿VirtualBox instalado?
   - ¿Nested virtualization habilitada?

**Pregunta de reflexión:** Si tu máquina tiene KVM disponible, ¿hay alguna
razón para instalar VirtualBox? ¿En qué caso un sysadmin de Linux elegiría
VirtualBox sobre KVM?

---

### Ejercicio 2: VMs como procesos

Explora cómo KVM integra las VMs con el modelo de procesos de Linux:

**Tareas:**
1. Si tienes una VM corriendo (o la crearás en S02), observa su proceso:
   ```bash
   ps aux | grep qemu-system
   ```
2. Identifica en la línea de comandos del proceso QEMU:
   - El nombre de la VM (`-name guest=...`)
   - La RAM asignada (`-m ...`)
   - Los discos conectados (`-drive file=...`)
   - La configuración de red (`-netdev ...`)
3. Verifica el usuario bajo el cual corre QEMU:
   ```bash
   ps -eo user,pid,cmd | grep qemu
   # Generalmente corre como usuario 'qemu' o 'libvirt-qemu'
   ```
4. Observa los hilos de la VM (uno por vCPU):
   ```bash
   ps -T -p $(pgrep -f "name guest=")
   ```
5. Verifica cuántos file descriptors tiene abiertos:
   ```bash
   ls /proc/$(pgrep -f "name guest=")/fd | wc -l
   ```

**Pregunta de reflexión:** Si cada VM es un proceso, ¿qué pasa si el
scheduler de Linux le da baja prioridad al proceso QEMU porque hay otros
procesos pesados en el host? ¿Cómo afecta esto al rendimiento de la VM y
cómo lo mitigarías?

---

### Ejercicio 3: Mapa de la pila de virtualización

Dibuja el stack de virtualización de tu máquina verificando cada capa:

**Tareas:**
1. Capa de hardware:
   ```bash
   lscpu | grep -E "(Model name|Virtualization|CPU\(s\))"
   grep -c processor /proc/cpuinfo
   free -h  # RAM total disponible
   ```
2. Capa de kernel:
   ```bash
   uname -r
   lsmod | grep kvm
   cat /sys/module/kvm_intel/parameters/nested 2>/dev/null || \
   cat /sys/module/kvm_amd/parameters/nested 2>/dev/null
   ```
3. Capa de QEMU:
   ```bash
   qemu-system-x86_64 --version 2>/dev/null || echo "QEMU not installed"
   which qemu-img 2>/dev/null
   ```
4. Capa de libvirt:
   ```bash
   virsh version 2>/dev/null || echo "libvirt not installed"
   systemctl is-active libvirtd 2>/dev/null
   ```
5. Capa de herramientas:
   ```bash
   which virt-install virt-manager virt-viewer virsh 2>/dev/null
   ```
6. Con los resultados, dibuja un diagrama ASCII de tu pila (similar al de
   la sección "Por qué KVM para este curso") indicando qué capas tienes
   instaladas y cuáles faltan

**Pregunta de reflexión:** Si te falta alguna capa (ej: libvirt no instalado,
o QEMU no instalado), ¿el stack puede funcionar parcialmente? ¿Podrías usar
KVM sin libvirt? ¿Y sin QEMU?
