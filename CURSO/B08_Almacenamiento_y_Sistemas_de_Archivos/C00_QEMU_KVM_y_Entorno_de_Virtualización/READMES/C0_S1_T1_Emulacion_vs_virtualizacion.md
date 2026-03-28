# Emulación vs virtualización

## Índice

1. [El problema: ejecutar software en hardware ajeno](#el-problema-ejecutar-software-en-hardware-ajeno)
2. [Emulación: traducir instrucciones](#emulación-traducir-instrucciones)
3. [Virtualización: ejecutar con aislamiento](#virtualización-ejecutar-con-aislamiento)
4. [Hardware-assisted virtualization](#hardware-assisted-virtualization)
5. [Verificar soporte en tu máquina](#verificar-soporte-en-tu-máquina)
6. [Virtualización anidada](#virtualización-anidada)
7. [Comparación de rendimiento](#comparación-de-rendimiento)
8. [Errores comunes](#errores-comunes)
9. [Cheatsheet](#cheatsheet)
10. [Ejercicios](#ejercicios)

---

## El problema: ejecutar software en hardware ajeno

Queremos ejecutar un sistema operativo **dentro** de otro. Las razones son
múltiples: laboratorios de práctica, aislar servicios, probar configuraciones
destructivas sin arriesgar el host. Hay dos enfoques fundamentales para
lograrlo: **emulación** y **virtualización**.

```
┌──────────────────────────────────────────────────────────────┐
│  ¿Por qué necesitamos VMs para este curso?                   │
│                                                              │
│  Docker comparte el kernel del host. No puedes:              │
│  - Particionar discos (B08)                                  │
│  - Crear RAID/LVM reales (B08)                               │
│  - Configurar GRUB ni el proceso de boot (B11)               │
│  - Ejecutar SELinux en modo enforcing (B11)                  │
│  - Cargar módulos del kernel (B11)                           │
│  - Configurar firewalls completos con iptables/nftables (B09)│
│  - Correr un init system (systemd) completo (B10)            │
│                                                              │
│  Para todo esto necesitas un kernel propio → VM completa     │
└──────────────────────────────────────────────────────────────┘
```

---

## Emulación: traducir instrucciones

Un **emulador** traduce instrucciones de una arquitectura de CPU a otra.
Simula completamente el hardware: CPU, memoria, dispositivos, buses. El
software guest cree que corre en hardware real, pero cada instrucción pasa
por un traductor.

### Cómo funciona

```
┌──────────────────────────────────────────────────────────────┐
│  Emulación (ej: QEMU sin KVM, BOCHS, DOSBox)                │
│                                                              │
│  Programa guest                                              │
│       │                                                      │
│       ▼                                                      │
│  Instrucción ARM: ADD R1, R2, R3                             │
│       │                                                      │
│       ▼                                                      │
│  ┌──────────────────────────────────┐                        │
│  │  Emulador (software en el host)  │                        │
│  │                                  │                        │
│  │  1. Leer instrucción ARM         │                        │
│  │  2. Decodificar: "sumar R2+R3"   │                        │
│  │  3. Traducir a x86:              │                        │
│  │     MOV EAX, [r2_virtual]        │                        │
│  │     ADD EAX, [r3_virtual]        │                        │
│  │     MOV [r1_virtual], EAX        │                        │
│  │  4. Ejecutar las instrucciones   │                        │
│  │     x86 en el host               │                        │
│  └──────────────────────────────────┘                        │
│       │                                                      │
│       ▼                                                      │
│  CPU x86 real del host                                       │
│                                                              │
│  Una instrucción ARM → varias instrucciones x86              │
│  Resultado: correcto pero LENTO (10x-100x más lento)        │
└──────────────────────────────────────────────────────────────┘
```

### Ejemplos de emulación

| Emulador | Qué emula | Uso típico |
|----------|-----------|------------|
| QEMU (sin KVM) | Cualquier arquitectura (ARM, RISC-V, MIPS, x86...) | Desarrollo embebido, cross-compilation |
| DOSBox | PC x86 con MS-DOS | Juegos retro, software legacy |
| BOCHS | PC x86 completo | Desarrollo de sistemas operativos |
| RetroArch | Consolas (NES, SNES, PS1...) | Gaming retro |
| Rosetta 2 (Apple) | x86_64 en Apple Silicon | Ejecutar apps Intel en Mac M1+ |

### Cuándo se usa emulación

- **Arquitectura diferente**: quieres correr ARM Linux en un host x86
- **Hardware que no existe**: desarrollar para un chip que aún no se fabrica
- **Reproducibilidad exacta**: emular hardware específico bit a bit
- **Software legacy**: correr programas de DOS, Windows 95, etc.

El costo es la velocidad. Un emulador puro puede ser 10x a 100x más lento
que la ejecución nativa, porque cada instrucción del guest se traduce a
múltiples instrucciones del host.

---

## Virtualización: ejecutar con aislamiento

La **virtualización** toma un enfoque diferente: en vez de traducir
instrucciones, las ejecuta **directamente** en el hardware real. El guest
y el host usan la misma arquitectura de CPU (ej: ambos x86_64), así que las
instrucciones del guest pueden correr sin traducción.

El truco es **aislar** al guest para que no pueda interferir con el host
ni con otros guests.

### Cómo funciona (concepto)

```
┌──────────────────────────────────────────────────────────────┐
│  Virtualización (ej: KVM, VMware, Hyper-V)                   │
│                                                              │
│  Programa guest                                              │
│       │                                                      │
│       ▼                                                      │
│  Instrucción x86: ADD EAX, EBX                               │
│       │                                                      │
│       ▼                                                      │
│  ┌──────────────────────────────────┐                        │
│  │  Hipervisor (software mínimo)    │                        │
│  │                                  │                        │
│  │  "Es una instrucción aritmética  │                        │
│  │   normal, no toca hardware       │                        │
│  │   privilegiado → dejarla pasar   │                        │
│  │   directamente al CPU"           │                        │
│  └──────────────────────────────────┘                        │
│       │                                                      │
│       ▼                                                      │
│  CPU x86 real del host                                       │
│  ADD EAX, EBX  ← se ejecuta nativa, sin traducción          │
│                                                              │
│  Una instrucción x86 → una instrucción x86                   │
│  Resultado: correcto Y RÁPIDO (95-99% velocidad nativa)     │
└──────────────────────────────────────────────────────────────┘
```

### El problema de las instrucciones privilegiadas

No todas las instrucciones pueden pasar directo. Las instrucciones
**privilegiadas** (acceso a disco, red, control de interrupciones, tablas
de páginas) deben ser interceptadas por el hipervisor:

```
┌──────────────────────────────────────────────────────────────┐
│  Instrucciones del guest:                                    │
│                                                              │
│  ADD EAX, EBX     → Aritmética: ejecutar directo  ✓ rápido │
│  MOV ECX, [addr]  → Lectura de memoria: directo   ✓ rápido │
│  CMP EAX, 0       → Comparación: directo          ✓ rápido │
│                                                              │
│  IN AL, 0x60      → Leer del teclado: TRAP        ⚡ lento  │
│  OUT 0x3F8, AL    → Escribir a puerto serial: TRAP ⚡ lento  │
│  MOV CR3, EAX     → Cambiar tabla de páginas: TRAP ⚡ lento  │
│  HLT              → Detener CPU: TRAP              ⚡ lento  │
│                                                              │
│  TRAP = el hipervisor intercepta la instrucción,             │
│  simula su efecto, y devuelve el control al guest.           │
│                                                              │
│  ~95% de las instrucciones son normales → velocidad nativa  │
│  ~5% son privilegiadas → el hipervisor las simula           │
│  Resultado neto: 95-99% de rendimiento nativo               │
└──────────────────────────────────────────────────────────────┘
```

### Requisito fundamental

La virtualización **solo funciona cuando guest y host comparten la misma
arquitectura de CPU**. No puedes virtualizar (solo emular) ARM en un host
x86 o viceversa.

```
  x86 guest en x86 host → virtualización posible  ✓
  ARM guest en ARM host → virtualización posible   ✓
  ARM guest en x86 host → solo emulación           ✗
```

---

## Hardware-assisted virtualization

Los primeros intentos de virtualización en x86 tenían un problema: ciertas
instrucciones privilegiadas no generaban traps (el CPU no las interceptaba).
El hipervisor tenía que reescribir el código del guest para poner traps
manuales ("binary translation") — complicado y con overhead.

Intel y AMD resolvieron esto con extensiones de hardware:

### Intel VT-x y AMD-V

```
┌──────────────────────────────────────────────────────────────┐
│  Sin VT-x/AMD-V (virtualización por software):              │
│                                                              │
│  Guest OS                                                    │
│     │                                                        │
│     ▼                                                        │
│  Hipervisor: "Debo interceptar instrucciones privilegiadas.  │
│  Pero en x86 algunas no causan trap. Necesito reescribir     │
│  el código del guest para insertarlas manualmente."          │
│     │                                                        │
│     ▼                                                        │
│  Binary translation → overhead, complejidad                  │
│                                                              │
│  ─────────────────────────────────────────────────────       │
│                                                              │
│  Con VT-x/AMD-V (virtualización por hardware):               │
│                                                              │
│  Guest OS                                                    │
│     │                                                        │
│     ▼                                                        │
│  CPU: "Tengo un modo especial para guests (VMX non-root).    │
│  TODAS las instrucciones privilegiadas causan VM Exit         │
│  automáticamente. El hipervisor las atrapa sin trucos."      │
│     │                                                        │
│     ▼                                                        │
│  VM Exit → hipervisor procesa → VM Entry → guest continúa   │
│  Sin binary translation → más simple y más rápido            │
└──────────────────────────────────────────────────────────────┘
```

### Dos modos de ejecución del CPU

Con VT-x/AMD-V, el procesador tiene dos modos:

```
┌──────────────────────────────────────────────────────────────┐
│                                                              │
│  ┌──────────────────────┐  ┌──────────────────────────────┐ │
│  │  VMX root mode        │  │  VMX non-root mode           │ │
│  │  (host/hipervisor)    │  │  (guest)                     │ │
│  │                       │  │                              │ │
│  │  - Control total      │  │  - Ejecución normal          │ │
│  │  - Gestiona VMs       │  │  - Instrucciones normales    │ │
│  │  - Maneja VM Exits    │  │    corren a velocidad nativa │ │
│  │                       │  │  - Instrucciones sensibles   │ │
│  │                       │  │    causan VM Exit automático │ │
│  └──────────┬────────────┘  └─────────────┬────────────────┘ │
│             │                             │                  │
│             │◀── VM Exit ─────────────────┘                  │
│             │    (guest hizo algo privilegiado)               │
│             │                                                │
│             │──── VM Entry ──────────────▶│                  │
│             │    (hipervisor devuelve control)                │
│                                                              │
│  Cada VM tiene una estructura VMCS (Intel) / VMCB (AMD)     │
│  que guarda su estado (registros, CR3, etc.)                 │
│  El switch entre VMs es un cambio de VMCS, no de proceso    │
└──────────────────────────────────────────────────────────────┘
```

### VMCS/VMCB: el "PCB" de la virtualización

Así como cada proceso tiene un Process Control Block (PCB) en el kernel,
cada VM tiene una estructura de control:

- **VMCS** (Virtual Machine Control Structure) — Intel
- **VMCB** (Virtual Machine Control Block) — AMD

Contiene: registros del guest, estado de los CR (control registers),
qué eventos causan VM Exit, dirección de entrada/salida, etc.

### Extensiones adicionales

Con el tiempo, Intel y AMD agregaron más aceleración por hardware:

| Extensión | Qué acelera |
|-----------|-------------|
| VT-x / AMD-V | Instrucciones privilegiadas (trap automático) |
| EPT / NPT | Traducción de direcciones de memoria (sin shadow page tables) |
| VT-d / AMD-Vi | Acceso directo a dispositivos PCI (IOMMU, passthrough) |
| APICv / AVIC | Interrupciones virtuales (menos VM Exits) |

Para nuestro curso, lo que importa es que **VT-x o AMD-V estén habilitados**.
Las demás extensiones las gestiona KVM automáticamente.

---

## Verificar soporte en tu máquina

### Paso 1: verificar flags del CPU

```bash
# Buscar vmx (Intel) o svm (AMD) en /proc/cpuinfo
grep -Ec '(vmx|svm)' /proc/cpuinfo
```

- Si el resultado es **> 0**: tu CPU soporta virtualización por hardware
- Si es **0**: o tu CPU no lo soporta, o está deshabilitado en BIOS/UEFI

```bash
# Ver cuál tienes (Intel o AMD)
grep -Eo '(vmx|svm)' /proc/cpuinfo | head -1
# vmx → Intel VT-x
# svm → AMD-V (SVM = Secure Virtual Machine)
```

### Paso 2: verificar con lscpu

```bash
lscpu | grep -i virtualization
# Virtualization:        VT-x          ← Intel
# Virtualization:        AMD-V         ← AMD
```

`lscpu` es más legible que parsear `/proc/cpuinfo` directamente.

### Paso 3: verificar el módulo KVM

```bash
# ¿Está cargado el módulo KVM?
lsmod | grep kvm

# Resultado típico en Intel:
# kvm_intel    380928  0
# kvm         1146880  1 kvm_intel

# Resultado típico en AMD:
# kvm_amd      167936  0
# kvm         1146880  1 kvm_amd
```

Si no aparece:

```bash
# Cargar manualmente
sudo modprobe kvm
sudo modprobe kvm_intel  # o kvm_amd

# Si falla: verificar que virtualización está habilitada en BIOS/UEFI
```

### Paso 4: verificar /dev/kvm

```bash
ls -la /dev/kvm
# crw-rw----+ 1 root kvm 10, 232 mar 20 08:00 /dev/kvm
```

Si `/dev/kvm` no existe, KVM no está activo. Causas comunes:
- Virtualización deshabilitada en BIOS/UEFI
- Módulo kvm no cargado
- Otro hipervisor ocupando el hardware (Hyper-V en Windows con WSL2)

### Paso 5: verificación integral

```bash
# Si tienes virt-host-validate instalado (paquete libvirt)
virt-host-validate qemu
```

Salida típica:

```
QEMU: Checking for hardware virtualization          : PASS
QEMU: Checking if device /dev/kvm exists            : PASS
QEMU: Checking if device /dev/kvm is accessible     : PASS
QEMU: Checking if device /dev/vhost-net exists       : PASS
QEMU: Checking for cgroup 'cpu' controller support  : PASS
QEMU: Checking for cgroup 'cpuacct' controller      : PASS
QEMU: Checking for cgroup 'cpuset' controller       : PASS
QEMU: Checking for cgroup 'memory' controller       : PASS
QEMU: Checking for cgroup 'devices' controller      : WARN
QEMU: Checking for cgroup 'blkio' controller        : PASS
QEMU: Checking for device assignment IOMMU support   : WARN
QEMU: Checking for secure guest support              : WARN
```

Los **PASS** en las primeras 3 líneas son los críticos. Los **WARN** son
funcionalidades opcionales que no necesitamos para el curso.

---

## Virtualización anidada

### Qué es

Virtualización anidada (nested virtualization) es ejecutar una VM **dentro**
de otra VM. El guest de primer nivel actúa como host de un guest de segundo
nivel.

```
┌──────────────────────────────────────────────────────────────┐
│  Nivel 0: Hardware real + Host OS                            │
│  └── Nivel 1: VM (guest OS)                                  │
│      └── Nivel 2: VM dentro de la VM (guest del guest)       │
│                                                              │
│  Ejemplos donde necesitas nested virtualization:             │
│                                                              │
│  1. Tu "host" es un VPS en la nube (ya es una VM)            │
│     └── Quieres crear VMs de laboratorio dentro              │
│                                                              │
│  2. Usas WSL2 en Windows (corre en Hyper-V, que es una VM)   │
│     └── Quieres KVM dentro de WSL2                           │
│                                                              │
│  3. Tu laptop es un Mac con UTM/Parallels (VM de Linux)      │
│     └── Quieres KVM dentro de esa VM de Linux               │
│                                                              │
│  Si tu host es Linux nativo en hardware real:                │
│  → NO necesitas nested virtualization                        │
│  → KVM funciona directo                                      │
└──────────────────────────────────────────────────────────────┘
```

### Verificar si nested está habilitado

```bash
# Intel
cat /sys/module/kvm_intel/parameters/nested
# Y → habilitado
# N → deshabilitado

# AMD
cat /sys/module/kvm_amd/parameters/nested
# 1 → habilitado
# 0 → deshabilitado
```

### Habilitar nested virtualization

```bash
# Intel: temporalmente (se pierde al reiniciar)
sudo modprobe -r kvm_intel
sudo modprobe kvm_intel nested=1

# AMD: temporalmente
sudo modprobe -r kvm_amd
sudo modprobe kvm_amd nested=1
```

Para que persista entre reinicios:

```bash
# Crear archivo de configuración del módulo
# Intel:
echo 'options kvm_intel nested=1' | sudo tee /etc/modprobe.d/kvm.conf

# AMD:
echo 'options kvm_amd nested=1' | sudo tee /etc/modprobe.d/kvm.conf
```

### Rendimiento de nested virtualization

```
┌──────────────────────────────────────────────────────────────┐
│  Rendimiento relativo:                                       │
│                                                              │
│  Nivel 0 (nativo):       100%                                │
│  Nivel 1 (VM con KVM):    95-99%                             │
│  Nivel 2 (nested VM):     70-90%                             │
│                                                              │
│  La caída en nivel 2 viene de:                               │
│  - Doble VM Exit: el trap del guest L2 pasa por el          │
│    hipervisor L1 Y el hipervisor L0                          │
│  - Doble traducción de memoria (EPT/NPT de dos niveles)     │
│                                                              │
│  Para laboratorios del curso: perfectamente usable           │
│  Para producción: evitar si es posible                       │
└──────────────────────────────────────────────────────────────┘
```

### Caso especial: WSL2 + KVM

WSL2 corre dentro de Hyper-V. Para usar KVM dentro de WSL2, Windows debe
exponer la virtualización anidada:

```powershell
# En PowerShell como administrador (Windows host)
# Verificar que nested está habilitado para WSL
Get-VMProcessor -VMName "WSL" | Select-Object ExposeVirtualizationExtensions
```

Dentro de WSL2:

```bash
# Verificar que vmx/svm aparece
grep -Ec '(vmx|svm)' /proc/cpuinfo
# Si aparece > 0, KVM está disponible dentro de WSL2
```

---

## Comparación de rendimiento

### Emulación vs virtualización vs nativo

```
┌──────────────────────────────────────────────────────────────┐
│                                                              │
│  Velocidad relativa (mayor = mejor):                         │
│                                                              │
│  ████████████████████████████████████████  100%  Nativo      │
│  ██████████████████████████████████████    96%   KVM (HW)   │
│  ██████████████████████████████████        80%   Nested KVM │
│  █████████████████████████                 60%   VBox/VMware│
│  ████████                                  20%   SW virt    │
│  ██                                         5%   Emulación  │
│                                                              │
│  * SW virt = virtualización sin VT-x/AMD-V (binary trans.)  │
│  * Los % son aproximados y varían por workload               │
│                                                              │
│  Para CPU-bound (cálculos puros):                            │
│    KVM ≈ nativo (las instrucciones corren directo)           │
│                                                              │
│  Para I/O-bound (disco, red):                                │
│    Overhead de virtualizar dispositivos                      │
│    Mitigado por virtio (drivers paravirtualizados)           │
└──────────────────────────────────────────────────────────────┘
```

### QEMU con y sin KVM

QEMU puede funcionar en ambos modos:

```bash
# QEMU sin KVM (emulación pura) — muy lento
qemu-system-x86_64 -m 1024 -hda disk.qcow2

# QEMU con KVM (virtualización) — velocidad nativa
qemu-system-x86_64 -enable-kvm -m 1024 -hda disk.qcow2
#                   ^^^^^^^^^^^
#                   Esta flag activa KVM

# Con libvirt/virt-install, KVM se usa automáticamente
# si está disponible. No necesitas la flag manualmente.
```

### Docker vs VMs: cuándo usar cada uno

```
┌──────────────────────────────────────────────────────────────┐
│  Docker (contenedores):                                      │
│  + Arranque instantáneo (< 1 segundo)                       │
│  + Mínimo overhead (comparte kernel del host)               │
│  + Ideal para aplicaciones y microservicios                 │
│  - NO tiene kernel propio                                    │
│  - NO puede particionar discos, configurar GRUB, RAID, etc. │
│  - NO tiene init system (systemd) real                      │
│                                                              │
│  VMs (QEMU/KVM):                                            │
│  + Kernel propio completo                                    │
│  + Puede hacer todo lo que una máquina física haría         │
│  + Aislamiento completo (hardware virtual propio)           │
│  - Arranque más lento (10-30 segundos)                      │
│  - Más overhead (RAM reservada, disco asignado)             │
│  - Más espacio en disco                                      │
│                                                              │
│  Para este curso:                                            │
│  B01-B07: Docker fue suficiente                              │
│  B08-B11: necesitamos VMs (operaciones a nivel de kernel)   │
└──────────────────────────────────────────────────────────────┘
```

---

## Errores comunes

### 1. Confundir emulación con virtualización

```
❌ "QEMU es un emulador, así que siempre es lento"
   QEMU puede emular (lento) O usar KVM para virtualizar (rápido).
   Cuando usas QEMU+KVM, las instrucciones del guest corren nativas.
   QEMU solo emula los dispositivos (disco, red, USB), no la CPU.

✅ QEMU sin KVM = emulación (CPU traducida, ~5% velocidad)
   QEMU con KVM = virtualización (CPU nativa, ~96% velocidad)
   En ambos casos, QEMU emula el hardware periférico (I/O).
```

### 2. No verificar que VT-x/AMD-V está habilitado en BIOS

```
❌ "Instalé qemu-kvm pero /dev/kvm no existe"
   Muchos BIOS/UEFI traen la virtualización DESHABILITADA
   por defecto, especialmente en laptops.

✅ Pasos para habilitar:
   1. Reiniciar → entrar al BIOS/UEFI (F2, Del, F10 según marca)
   2. Buscar en Advanced → CPU Configuration
   3. Intel: "Intel Virtualization Technology" → Enabled
      AMD: "SVM Mode" → Enabled
   4. Guardar y reiniciar
   5. Verificar: grep -Ec '(vmx|svm)' /proc/cpuinfo
```

### 3. Intentar virtualizar una arquitectura diferente

```
❌ "Quiero correr una VM de ARM Linux en mi laptop x86 con KVM"
   KVM solo puede virtualizar la misma arquitectura del host.
   x86 host → solo puede virtualizar guests x86.

✅ Para ARM en x86:
   - Usar QEMU sin KVM (emulación): funciona pero es lento
   - Usar un host ARM (Raspberry Pi, Mac M1+, cloud ARM)
   Para el curso: solo necesitamos x86 guests en x86 hosts.
```

### 4. Otro hipervisor bloqueando KVM

```
❌ "En Windows con WSL2, KVM dice que /dev/kvm no está disponible"
   Hyper-V y KVM no pueden compartir el hardware simultáneamente
   en el mismo nivel. WSL2 corre sobre Hyper-V, necesitas nested.

✅ Verificar si hay conflictos:
   dmesg | grep -i kvm
   # Si ves "kvm: disabled by bios" → habilitar en BIOS
   # Si ves "kvm: already loaded" → otro hipervisor activo

   # En Linux nativo, VirtualBox puede conflictuar con KVM:
   # Descargar módulos de VirtualBox antes de usar KVM:
   sudo modprobe -r vboxdrv
```

### 5. Confundir paravirtualización con emulación

```
❌ "virtio es emulación porque el hardware no existe"
   virtio no emula hardware real — es un protocolo de comunicación
   optimizado entre guest y host. El guest sabe que está en una VM
   y usa drivers especiales (virtio) en vez de simular hardware real.

✅ Tipos de I/O virtualizado:
   - Emulado: simular un dispositivo real (ej: e1000 NIC) → lento
   - Paravirtualizado: protocolo guest↔host (ej: virtio-net) → rápido
   - Passthrough: acceso directo al hardware real → más rápido aún

   Para el curso usamos virtio siempre que sea posible.
```

---

## Cheatsheet

```
┌──────────────────────────────────────────────────────────────┐
│        EMULACIÓN VS VIRTUALIZACIÓN CHEATSHEET                │
├──────────────────────────────────────────────────────────────┤
│                                                              │
│  CONCEPTOS                                                   │
│  Emulación        Traducir instrucciones (lento, flexible)  │
│  Virtualización   Ejecutar nativo con aislamiento (rápido)  │
│  VT-x / AMD-V     Extensiones HW para virtualización       │
│  VM Exit / Entry  Trap del guest al hipervisor y vuelta     │
│  VMCS / VMCB      Estructura de control por VM              │
│  Nested virt.     VM dentro de VM                           │
│                                                              │
│  VERIFICAR SOPORTE                                           │
│  grep -Ec '(vmx|svm)' /proc/cpuinfo    Flags de CPU        │
│  lscpu | grep -i virtualization         Formato legible     │
│  lsmod | grep kvm                       Módulo cargado?     │
│  ls -la /dev/kvm                        Device node existe? │
│  virt-host-validate qemu                Verificación total  │
│                                                              │
│  HABILITAR KVM                                               │
│  sudo modprobe kvm                      Módulo base         │
│  sudo modprobe kvm_intel                Intel VT-x          │
│  sudo modprobe kvm_amd                  AMD-V               │
│                                                              │
│  NESTED VIRTUALIZATION                                       │
│  cat /sys/module/kvm_intel/parameters/nested   Ver estado   │
│  modprobe kvm_intel nested=1                   Activar temp │
│  echo 'options kvm_intel nested=1' \                        │
│    > /etc/modprobe.d/kvm.conf                  Persistente  │
│                                                              │
│  QEMU MODOS                                                 │
│  qemu-system-x86_64 ...                 Emulación (lento)  │
│  qemu-system-x86_64 -enable-kvm ...     Con KVM (rápido)   │
│  virt-install ...                        KVM automático     │
│                                                              │
│  RENDIMIENTO RELATIVO                                        │
│  Nativo .......... 100%                                      │
│  KVM .............. 96%                                      │
│  Nested KVM ....... 80%                                      │
│  Sin VT-x ......... 20%                                     │
│  Emulación pura .... 5%                                     │
│                                                              │
│  CUÁNDO USAR QUÉ                                            │
│  Misma arch + VT-x/AMD-V → KVM (virtualización)            │
│  Distinta arch            → QEMU sin KVM (emulación)       │
│  Aplicaciones aisladas    → Docker (contenedor)             │
│  Kernel propio / RAID /   → VM completa (QEMU+KVM)         │
│  GRUB / firewall real                                        │
└──────────────────────────────────────────────────────────────┘
```

---

## Ejercicios

### Ejercicio 1: Verificar tu entorno

Determina si tu máquina puede ejecutar VMs con KVM:

**Tareas:**
1. Verifica los flags de CPU:
   ```bash
   grep -Ec '(vmx|svm)' /proc/cpuinfo
   grep -Eo '(vmx|svm)' /proc/cpuinfo | head -1
   ```
2. Verifica con `lscpu`:
   ```bash
   lscpu | grep -i virtualization
   lscpu | grep -i "Model name"
   ```
3. Verifica el módulo KVM:
   ```bash
   lsmod | grep kvm
   ```
   Si no aparece, intenta cargarlo con `sudo modprobe kvm` y `sudo modprobe kvm_intel` (o `kvm_amd`)
4. Verifica `/dev/kvm`:
   ```bash
   ls -la /dev/kvm
   ```
5. Si tienes `virt-host-validate`, ejecútalo y anota qué checks pasan y cuáles no
6. Si algo falla, investiga `dmesg | grep -i kvm` para pistas

**Pregunta de reflexión:** Si tu CPU muestra `vmx` en `/proc/cpuinfo` pero
`/dev/kvm` no existe, ¿cuáles son las posibles causas y cómo las investigarías
sistemáticamente?

---

### Ejercicio 2: Emulación vs KVM — diferencia medible

Compara el rendimiento de QEMU con y sin KVM:

**Tareas:**
1. Instala QEMU si aún no lo tienes:
   ```bash
   # Fedora
   sudo dnf install qemu-kvm
   # Debian/Ubuntu
   sudo apt install qemu-system-x86
   ```
2. Crea una imagen de disco vacía:
   ```bash
   qemu-img create -f qcow2 /tmp/test-perf.qcow2 1G
   ```
3. Intenta arrancar sin KVM (emulación pura) — nota cuánto tarda solo en
   llegar al mensaje de "no bootable device":
   ```bash
   time qemu-system-x86_64 -m 512 -hda /tmp/test-perf.qcow2 \
       -nographic -serial mon:stdio -no-reboot
   # Ctrl+A luego X para salir
   ```
4. Arranca con KVM — compara la velocidad:
   ```bash
   time qemu-system-x86_64 -enable-kvm -m 512 -hda /tmp/test-perf.qcow2 \
       -nographic -serial mon:stdio -no-reboot
   ```
5. Limpia:
   ```bash
   rm /tmp/test-perf.qcow2
   ```

**Pregunta de reflexión:** ¿Por qué el boot sin KVM es tan lento si solo está
ejecutando el firmware (BIOS/UEFI) sin un OS completo? ¿Qué tipo de
instrucciones ejecuta el firmware que causan el overhead de emulación?

---

### Ejercicio 3: Nested virtualization

Verifica el estado de virtualización anidada en tu sistema:

**Tareas:**
1. Determina si corres en hardware real o dentro de una VM:
   ```bash
   systemd-detect-virt
   # "none" → hardware real
   # "kvm"  → VM KVM
   # "vmware", "oracle" → otra VM
   ```
2. Si estás en hardware real, verifica el estado de nested:
   ```bash
   # Intel
   cat /sys/module/kvm_intel/parameters/nested
   # AMD
   cat /sys/module/kvm_amd/parameters/nested
   ```
3. Si nested no está habilitado y lo necesitas, habilítalo con el archivo
   de configuración de modprobe (ver sección de nested más arriba)
4. Si estás en una VM (VPS, WSL2), verifica si la virtualización anidada
   está expuesta:
   ```bash
   grep -Ec '(vmx|svm)' /proc/cpuinfo
   # Si es 0 dentro de una VM → nested no está habilitado
   # Debes configurarlo en el hipervisor de nivel 0
   ```

**Pregunta de reflexión:** Un VPS en AWS/GCP/Azure normalmente NO expone
virtualización anidada en sus instancias más baratas. ¿Qué alternativas
tendrías para hacer los labs del curso si tu única máquina Linux es un VPS
sin KVM? (Pista: piensa en tipos de instancia, proveedores alternativos, o
soluciones locales.)
