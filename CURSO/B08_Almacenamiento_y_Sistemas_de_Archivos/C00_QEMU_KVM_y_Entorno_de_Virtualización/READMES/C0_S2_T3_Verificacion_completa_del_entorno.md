# Verificación completa del entorno

## Índice

1. [Lista de verificación](#lista-de-verificación)
2. [virt-host-validate en detalle](#virt-host-validate-en-detalle)
3. [Diagnóstico de problemas comunes](#diagnóstico-de-problemas-comunes)
4. [Test rápido: VM de prueba](#test-rápido-vm-de-prueba)
5. [Comparar rendimiento KVM vs emulación](#comparar-rendimiento-kvm-vs-emulación)
6. [Errores comunes](#errores-comunes)
7. [Cheatsheet](#cheatsheet)
8. [Ejercicios](#ejercicios)

---

## Lista de verificación

Antes de continuar con el curso, todo esto debe estar en orden:

```
┌──────────────────────────────────────────────────────────────┐
│  CHECK   COMPONENTE           CÓMO VERIFICAR                │
│  ─────   ──────────           ──────────────                │
│  [ ]     CPU VT-x/AMD-V      grep -Ec '(vmx|svm)' \        │
│                               /proc/cpuinfo  → >0           │
│  [ ]     Módulo KVM           lsmod | grep kvm              │
│  [ ]     /dev/kvm             ls -la /dev/kvm               │
│  [ ]     QEMU instalado       qemu-system-x86_64 --version  │
│  [ ]     KVM como acelerador  qemu-system-x86_64 -accel help│
│  [ ]     libvirtd activo      systemctl is-active libvirtd   │
│  [ ]     Grupo libvirt        groups | grep libvirt          │
│  [ ]     Grupo kvm            groups | grep kvm              │
│  [ ]     virsh conecta        virsh list --all (sin sudo)   │
│  [ ]     Red default activa   virsh net-list --all           │
│  [ ]     Pool default activo  virsh pool-list --all          │
│  [ ]     virt-host-validate   virt-host-validate qemu → PASS│
└──────────────────────────────────────────────────────────────┘
```

### Script de verificación rápida

```bash
#!/bin/bash
# verify-virt-env.sh — Verificar entorno de virtualización

echo "=== CPU ==="
grep -Eo '(vmx|svm)' /proc/cpuinfo | head -1 || echo "NO SUPPORT"
lscpu | grep -i virtualization

echo ""
echo "=== KVM ==="
lsmod | grep kvm || echo "KVM MODULE NOT LOADED"
ls -la /dev/kvm 2>/dev/null || echo "/dev/kvm NOT FOUND"

echo ""
echo "=== QEMU ==="
qemu-system-x86_64 --version 2>/dev/null || echo "QEMU NOT INSTALLED"

echo ""
echo "=== libvirt ==="
systemctl is-active libvirtd 2>/dev/null || echo "libvirtd NOT ACTIVE"
virsh version 2>/dev/null | head -3 || echo "CANNOT CONNECT TO libvirt"

echo ""
echo "=== Groups ==="
groups | tr ' ' '\n' | grep -E '(libvirt|kvm)' || echo "NOT IN libvirt/kvm GROUPS"

echo ""
echo "=== Resources ==="
virsh net-list --all 2>/dev/null || echo "CANNOT LIST NETWORKS"
virsh pool-list --all 2>/dev/null || echo "CANNOT LIST POOLS"

echo ""
echo "=== virt-host-validate ==="
virt-host-validate qemu 2>/dev/null || echo "virt-host-validate NOT AVAILABLE"
```

---

## virt-host-validate en detalle

`virt-host-validate` es la herramienta oficial para verificar que el host está
listo para ejecutar VMs. Analicemos cada check:

```bash
virt-host-validate qemu
```

### Checks críticos (deben ser PASS)

```
┌──────────────────────────────────────────────────────────────┐
│  Check                                    Qué verifica       │
│  ─────                                    ────────────       │
│                                                              │
│  Checking for hardware virtualization     ¿CPU tiene         │
│                                           vmx o svm?         │
│  Si FAIL:                                                    │
│  → Verificar BIOS: habilitar VT-x / SVM                     │
│  → Si es VM: habilitar nested virtualization                │
│                                                              │
│  Checking if device /dev/kvm exists       ¿Módulo KVM       │
│                                           cargado?           │
│  Si FAIL:                                                    │
│  → sudo modprobe kvm kvm_intel (o kvm_amd)                  │
│  → Si falla: kvm deshabilitado en BIOS o conflicto          │
│                                                              │
│  Checking if device /dev/kvm is accessible ¿Permisos?       │
│                                                              │
│  Si FAIL:                                                    │
│  → sudo usermod -aG kvm $USER                               │
│  → Cerrar sesión y volver a entrar                          │
│                                                              │
│  Checking if device /dev/vhost-net exists  ¿vhost-net        │
│                                           disponible?        │
│  Si FAIL:                                                    │
│  → sudo modprobe vhost_net                                   │
│  → vhost-net acelera virtio-net (no es crítico sin él)       │
└──────────────────────────────────────────────────────────────┘
```

### Checks informativos (WARN es aceptable)

```
┌──────────────────────────────────────────────────────────────┐
│  Check                                    Resultado          │
│  ─────                                    ────────           │
│                                                              │
│  Checking for cgroup 'cpu' controller     PASS o WARN       │
│  Checking for cgroup 'memory' controller  PASS o WARN       │
│  Checking for cgroup 'devices' controller WARN (cgroups v2) │
│                                                              │
│  En cgroups v2 (Fedora 31+, Debian 11+, Ubuntu 22.04+),     │
│  algunos controladores no existen como entidades separadas.  │
│  El WARN es esperado y NO afecta la funcionalidad.           │
│                                                              │
│  Checking for device assignment IOMMU     WARN              │
│                                                              │
│  IOMMU es necesario solo para PCI passthrough                │
│  (dar un dispositivo PCI físico directamente a una VM).      │
│  No lo necesitamos para el curso. WARN es normal.            │
│                                                              │
│  Checking for secure guest support        WARN              │
│                                                              │
│  AMD SEV o Intel TDX para VMs encriptadas.                   │
│  No lo necesitamos. WARN es normal.                          │
└──────────────────────────────────────────────────────────────┘
```

### Interpretar la salida completa

```
QEMU: Checking for hardware virtualization                 : PASS  ← CRÍTICO
QEMU: Checking if device /dev/kvm exists                   : PASS  ← CRÍTICO
QEMU: Checking if device /dev/kvm is accessible            : PASS  ← CRÍTICO
QEMU: Checking if device /dev/vhost-net exists              : PASS  ← Bueno
QEMU: Checking for cgroup 'cpu' controller support         : PASS  ← OK
QEMU: Checking for cgroup 'cpuacct' controller support     : PASS  ← OK
QEMU: Checking for cgroup 'cpuset' controller support      : PASS  ← OK
QEMU: Checking for cgroup 'memory' controller support      : PASS  ← OK
QEMU: Checking for cgroup 'devices' controller support     : WARN  ← OK (v2)
QEMU: Checking for cgroup 'blkio' controller support       : PASS  ← OK
QEMU: Checking for device assignment IOMMU support         : WARN  ← OK
QEMU: Checking for secure guest support                     : WARN  ← OK

Resumen: las 3 primeras líneas PASS = listo para virtualizar
```

---

## Diagnóstico de problemas comunes

### Problema: "hardware virtualization: FAIL"

```bash
# Verificar manualmente
grep -Ec '(vmx|svm)' /proc/cpuinfo
# Si es 0:

# Posibilidad 1: deshabilitado en BIOS
# → Reiniciar → BIOS/UEFI → Advanced → CPU → Intel VT / SVM → Enabled

# Posibilidad 2: estás dentro de una VM sin nested virtualization
systemd-detect-virt
# Si muestra "kvm", "vmware", etc.: estás en una VM
# → Habilitar nested en el hipervisor de nivel 0

# Posibilidad 3: otro hipervisor bloquea (Hyper-V, VirtualBox)
dmesg | grep -iE '(kvm|vmx|svm)' | tail -10
```

### Problema: "/dev/kvm does not exist"

```bash
# Verificar que el módulo está cargado
lsmod | grep kvm
# Si vacío:

# Intentar cargar
sudo modprobe kvm
sudo modprobe kvm_intel   # Intel
# o
sudo modprobe kvm_amd     # AMD

# Si falla:
dmesg | tail -20
# Buscar mensajes como:
# "kvm: disabled by bios"
# "kvm: no hardware support"

# Verificar que no hay blacklist
grep -r kvm /etc/modprobe.d/
# Si ves "blacklist kvm": alguien lo bloqueó intencionalmente
```

### Problema: "/dev/kvm is not accessible"

```bash
# Verificar permisos
ls -la /dev/kvm
# crw-rw----+ 1 root kvm 10, 232 ...
#                    ^^^
# El grupo debe ser 'kvm'

# ¿Tu usuario está en el grupo?
groups | grep kvm
# Si no:
sudo usermod -aG kvm $USER
# Cerrar sesión y volver

# Verificar reglas udev
cat /lib/udev/rules.d/*kvm* 2>/dev/null
# Debería haber una regla que asigna el grupo 'kvm' a /dev/kvm
```

### Problema: virsh no conecta

```bash
# Error típico:
# "failed to connect to the hypervisor"
# "authentication unavailable"

# 1. ¿libvirtd está corriendo?
systemctl status libvirtd

# 2. ¿Socket existe?
ls -la /run/libvirt/libvirt-sock

# 3. ¿Estás en el grupo libvirt?
groups | grep libvirt

# 4. ¿Polkit está instalado y corriendo?
systemctl status polkit

# 5. ¿Puedes conectar con sudo?
sudo virsh list --all
# Si esto funciona, es un problema de permisos de usuario
```

### Árbol de decisión

```
┌──────────────────────────────────────────────────────────────┐
│  ¿virt-host-validate pasa las 3 primeras?                    │
│     │                                                        │
│     ├── Sí → ¿virsh list funciona sin sudo?                 │
│     │         │                                              │
│     │         ├── Sí → ¿virsh net-list muestra 'default'?   │
│     │         │         │                                    │
│     │         │         ├── Sí → LISTO ✓                    │
│     │         │         │                                    │
│     │         │         └── No → virsh net-start default     │
│     │         │                  virsh net-autostart default  │
│     │         │                                              │
│     │         └── No → usermod -aG libvirt $USER             │
│     │                  + cerrar sesión                        │
│     │                                                        │
│     └── No → ¿/dev/kvm existe?                              │
│               │                                              │
│               ├── Sí → permisos: usermod -aG kvm $USER      │
│               │                                              │
│               └── No → ¿CPU tiene vmx/svm?                  │
│                         │                                    │
│                         ├── Sí → modprobe kvm kvm_intel      │
│                         │                                    │
│                         └── No → habilitar en BIOS           │
│                                  o nested virtualization     │
└──────────────────────────────────────────────────────────────┘
```

---

## Test rápido: VM de prueba

La verificación definitiva: crear una VM, arrancarla y destruirla.

### Opción A: QEMU directo (sin libvirt)

Test mínimo para verificar que QEMU+KVM funcionan:

```bash
# Crear una imagen vacía
qemu-img create -f qcow2 /tmp/test-vm.qcow2 1G

# Arrancar QEMU con KVM (sin OS instalado)
# Debería mostrar el BIOS/UEFI y luego "No bootable device"
qemu-system-x86_64 \
    -enable-kvm \
    -m 512 \
    -hda /tmp/test-vm.qcow2 \
    -nographic \
    -serial mon:stdio \
    -no-reboot

# Si ves el prompt de SeaBIOS o UEFI shell → KVM funciona ✓
# Salir: Ctrl+A luego X

# Limpiar
rm /tmp/test-vm.qcow2
```

Si aparece rápidamente (< 2 segundos) el mensaje de BIOS, KVM está
funcionando. Si tarda más de 10 segundos, probablemente está usando
emulación TCG.

### Opción B: virt-install con una ISO mínima

Si tienes una ISO descargada:

```bash
# Crear una VM de prueba (no completar la instalación)
virt-install \
    --name test-vm \
    --ram 1024 \
    --vcpus 1 \
    --disk size=5 \
    --cdrom /path/to/any-linux.iso \
    --os-variant generic \
    --graphics none \
    --extra-args "console=ttyS0"

# Verificar que la VM se creó
virsh list --all

# Destruir la VM de prueba
virsh destroy test-vm 2>/dev/null   # Apagar si está corriendo
virsh undefine test-vm --remove-all-storage
```

### Opción C: boot sin ISO (solo verificar KVM)

```bash
# VM que solo arranca el BIOS y se apaga (sin ISO ni disco real)
virt-install \
    --name test-kvm \
    --ram 512 \
    --vcpus 1 \
    --disk none \
    --import \
    --os-variant generic \
    --graphics none \
    --noautoconsole \
    --transient

# Verificar que se creó y KVM está activo
virsh dumpxml test-kvm 2>/dev/null | grep "type='kvm'"
# <domain type='kvm'>   ← Confirmado: usa KVM

# La VM se apagará sola (no tiene OS)
# Si fue --transient, se elimina al apagarse
```

---

## Comparar rendimiento KVM vs emulación

### Test de velocidad del BIOS boot

```bash
# Crear imagen de prueba
qemu-img create -f qcow2 /tmp/perf-test.qcow2 512M

# Con KVM (debería tardar < 2 segundos en llegar al BIOS)
echo "=== Con KVM ==="
time timeout 10 qemu-system-x86_64 \
    -enable-kvm \
    -m 256 \
    -hda /tmp/perf-test.qcow2 \
    -nographic \
    -serial mon:stdio \
    -no-reboot 2>/dev/null

# Sin KVM (emulación TCG, tardará 10-60 segundos)
echo "=== Sin KVM (emulación) ==="
time timeout 60 qemu-system-x86_64 \
    -m 256 \
    -hda /tmp/perf-test.qcow2 \
    -nographic \
    -serial mon:stdio \
    -no-reboot 2>/dev/null

rm /tmp/perf-test.qcow2
```

### Qué esperar

```
┌──────────────────────────────────────────────────────────────┐
│  Resultado típico:                                           │
│                                                              │
│  Con KVM:                                                    │
│  - BIOS aparece en < 1 segundo                              │
│  - "No bootable device" inmediatamente después              │
│  - Total: ~1-2 segundos                                     │
│                                                              │
│  Sin KVM (emulación TCG):                                    │
│  - Nada visible durante 5-15 segundos                       │
│  - BIOS aparece lentamente                                  │
│  - "No bootable device" después de otros 5-10 segundos     │
│  - Total: ~15-30 segundos                                   │
│  - Puede mostrar warnings de "TCG doesn't support ..."     │
│                                                              │
│  La diferencia es obvia e inmediata.                        │
│  Si no notas diferencia → probablemente ambos usan KVM      │
│  o ambos usan TCG (verificar con -accel help).              │
└──────────────────────────────────────────────────────────────┘
```

### Verificar qué acelerador usa una VM

```bash
# Dentro de una VM corriendo, verificar
virsh dumpxml <vm-name> | grep "<domain"
# <domain type='kvm'>     ← Usa KVM ✓
# <domain type='qemu'>    ← Usa emulación TCG ✗

# O directamente en la línea de comandos del proceso QEMU
ps aux | grep qemu | grep -o "accel=[a-z]*"
# accel=kvm   ← Usa KVM ✓
```

---

## Errores comunes

### 1. Asumir que WARN en virt-host-validate es un problema

```
❌ "virt-host-validate muestra 3 WARNs, algo está mal"
   Los WARN en cgroups devices, IOMMU y secure guest son
   normales en la mayoría de sistemas. No afectan al curso.

✅ Solo importan las primeras 3-4 líneas:
   - hardware virtualization: PASS
   - /dev/kvm exists: PASS
   - /dev/kvm is accessible: PASS
   Si estas son PASS, el entorno está listo.
```

### 2. No distinguir entre "KVM no disponible" y "KVM no cargado"

```
❌ "grep vmx /proc/cpuinfo no muestra nada → mi CPU no soporta KVM"
   Puede que la CPU sí soporte pero esté deshabilitado en BIOS.
   O puede que estés en una VM sin nested.

✅ Investigar por capas:
   1. lscpu | grep Virtualization
      → Si muestra algo: la CPU soporta, puede estar deshabilitado
   2. dmesg | grep -i kvm
      → "disabled by bios" = habilitarlo en BIOS
      → "no hardware support" = CPU no soporta (raro en CPUs modernos)
   3. systemd-detect-virt
      → Si no es "none": estás en VM, necesitas nested
```

### 3. Crear VM de prueba y olvidar limpiarla

```
❌ # Crear VM de prueba, verificar, olvidarla
   virt-install --name test-vm ...
   # Meses después: 5GB de disco usado por una VM que no se usa

✅ Siempre limpiar VMs de prueba:
   virsh destroy test-vm        # Apagar
   virsh undefine test-vm --remove-all-storage  # Eliminar todo
   # --remove-all-storage elimina los discos asociados
```

### 4. Comparar rendimiento con un disco vacío y concluir que KVM es lento

```
❌ "Mi VM con KVM tarda 20 segundos en arrancar, pensé que sería instantáneo"
   El arranque de un OS completo incluye:
   - BIOS/UEFI (~1s)
   - Bootloader/GRUB (~2s)
   - Kernel Linux (~3-5s)
   - systemd y servicios (~10-15s)
   Esto es normal incluso con KVM.

✅ Para comparar KVM vs TCG, medir el tiempo de BIOS boot
   (sin OS), no el boot completo. La diferencia es:
   - BIOS con KVM: < 1 segundo
   - BIOS con TCG: 10-30 segundos
   El boot de un OS completo siempre tarda 15-30s.
```

### 5. Intentar la verificación sin los paquetes instalados

```
❌ virt-host-validate
   # command not found
   # "No puedo verificar, el entorno no funciona"

✅ virt-host-validate está en el paquete libvirt-client (o libvirt):
   # Fedora
   sudo dnf install libvirt
   # Debian
   sudo apt install libvirt-clients

   # Si no puedes instalar, verificar manualmente:
   grep -Ec '(vmx|svm)' /proc/cpuinfo && echo "CPU OK"
   ls /dev/kvm && echo "KVM OK"
   systemctl is-active libvirtd && echo "libvirtd OK"
```

---

## Cheatsheet

```
┌──────────────────────────────────────────────────────────────┐
│     VERIFICACIÓN COMPLETA DEL ENTORNO CHEATSHEET             │
├──────────────────────────────────────────────────────────────┤
│                                                              │
│  VERIFICACIÓN RÁPIDA (3 comandos)                            │
│  virt-host-validate qemu          Todo-en-uno               │
│  virsh list --all                  Conectividad libvirt     │
│  virsh net-list --all              Red default activa       │
│                                                              │
│  VERIFICACIÓN POR CAPAS                                      │
│  grep -Ec '(vmx|svm)' /proc/cpuinfo   CPU flags            │
│  lsmod | grep kvm                      Módulo kernel        │
│  ls -la /dev/kvm                       Device node          │
│  qemu-system-x86_64 -accel help        QEMU + KVM          │
│  systemctl is-active libvirtd          Daemon libvirt       │
│  groups | grep -E '(libvirt|kvm)'      Permisos usuario    │
│  virsh -c qemu:///system list          Conexión libvirt    │
│  virsh net-list --all                  Red virtual          │
│  virsh pool-list --all                 Storage pool         │
│                                                              │
│  DIAGNÓSTICO                                                 │
│  dmesg | grep -i kvm              Mensajes del kernel       │
│  journalctl -u libvirtd           Log de libvirtd           │
│  systemd-detect-virt               ¿Estoy en una VM?       │
│  cat /sys/module/kvm_intel/parameters/nested  ¿Nested?     │
│                                                              │
│  TEST RÁPIDO                                                 │
│  qemu-img create -f qcow2 /tmp/t.qcow2 512M                │
│  qemu-system-x86_64 -enable-kvm -m 256 \                   │
│    -hda /tmp/t.qcow2 -nographic \                           │
│    -serial mon:stdio -no-reboot                              │
│  # Ctrl+A, X para salir                                     │
│  rm /tmp/t.qcow2                                            │
│                                                              │
│  VERIFICAR ACELERADOR DE UNA VM                              │
│  virsh dumpxml <vm> | grep "<domain"                        │
│    type='kvm'  → KVM activo ✓                               │
│    type='qemu' → emulación TCG ✗                            │
│                                                              │
│  RESOLVER PROBLEMAS                                          │
│  CPU sin vmx/svm     → BIOS → habilitar VT-x / SVM        │
│  /dev/kvm no existe   → modprobe kvm kvm_intel              │
│  /dev/kvm sin acceso  → usermod -aG kvm $USER + relogin    │
│  virsh falla          → usermod -aG libvirt $USER + relogin │
│  Red default inactiva → virsh net-start default              │
│  Pool default inactivo→ virsh pool-start default             │
│                                                              │
│  LIMPIAR VM DE PRUEBA                                        │
│  virsh destroy <vm>                    Apagar               │
│  virsh undefine <vm> --remove-all-storage  Eliminar todo   │
└──────────────────────────────────────────────────────────────┘
```

---

## Ejercicios

### Ejercicio 1: Verificación completa documentada

Ejecuta todas las verificaciones y documenta el resultado:

**Tareas:**
1. Ejecuta el script de verificación de la primera sección (o hazlo
   comando por comando)
2. Ejecuta `virt-host-validate qemu` y anota cada línea
3. Para cada WARN o FAIL, investiga la causa y documenta si es un problema
   real o esperado
4. Crea un archivo `~/virt-env-status.txt` con el resultado:
   ```
   Fecha: 2026-03-20
   Host: (tu hostname)
   CPU: (modelo)
   Distro: (distribución y versión)
   KVM: OK/FAIL (detalles)
   libvirtd: OK/FAIL
   Red default: OK/FAIL
   Pool default: OK/FAIL
   virt-host-validate: X PASS, Y WARN, Z FAIL
   ```
5. Si tienes FAILs, resuélvelos siguiendo el árbol de decisión de este tema

**Pregunta de reflexión:** ¿Qué harías si tu CPU no soporta VT-x/AMD-V
(una laptop muy antigua o un procesador Atom)? ¿Es posible completar los labs
del curso solo con emulación TCG, aunque sea lento?

---

### Ejercicio 2: Test de VM de prueba

Crea una VM mínima, verifica que usa KVM, y destrúyela:

**Tareas:**
1. Crea una imagen de disco de prueba:
   ```bash
   qemu-img create -f qcow2 /tmp/test-boot.qcow2 512M
   ```
2. Arranca con KVM:
   ```bash
   qemu-system-x86_64 \
       -enable-kvm \
       -m 256 \
       -hda /tmp/test-boot.qcow2 \
       -nographic \
       -serial mon:stdio \
       -no-reboot
   ```
3. Verifica que el BIOS aparece en menos de 2 segundos
4. Sal con `Ctrl+A` luego `X`
5. Repite sin `-enable-kvm` y compara el tiempo:
   ```bash
   qemu-system-x86_64 \
       -m 256 \
       -hda /tmp/test-boot.qcow2 \
       -nographic \
       -serial mon:stdio \
       -no-reboot
   ```
6. Documenta la diferencia de tiempo
7. Limpia:
   ```bash
   rm /tmp/test-boot.qcow2
   ```

**Pregunta de reflexión:** Al arrancar QEMU con `-nographic -serial mon:stdio`,
la consola de QEMU se mezcla con tu terminal. ¿Qué otros modos de consola
existen (`-display`, `-vnc`, `-spice`)? ¿Cuál es el más práctico para usar
VMs sin entorno gráfico en el host?

---

### Ejercicio 3: Diagnosticar un entorno problemático

Practica el diagnóstico simulando o investigando problemas:

**Tareas:**
1. Verifica qué pasaría si el módulo KVM no estuviera cargado:
   ```bash
   # Solo investigar, NO descargar el módulo si tienes VMs corriendo
   # Si NO tienes VMs corriendo, puedes probar:
   # sudo modprobe -r kvm_intel kvm
   # virt-host-validate qemu
   # sudo modprobe kvm kvm_intel   # Restaurar
   ```
2. Investiga los logs de libvirtd:
   ```bash
   journalctl -u libvirtd --since "today" --no-pager | tail -30
   ```
3. Verifica qué proceso está usando /dev/kvm:
   ```bash
   sudo fuser /dev/kvm 2>/dev/null
   # Muestra los PIDs de procesos que tienen /dev/kvm abierto
   ```
4. Verifica la versión de cgroups de tu sistema:
   ```bash
   stat -fc %T /sys/fs/cgroup/
   # "cgroup2fs" → cgroups v2 (los WARN son esperados)
   # "tmpfs"     → cgroups v1
   ```
5. Documenta cómo resolverías cada uno de estos escenarios:
   - CPU soporta VT-x pero `/dev/kvm` no existe
   - `/dev/kvm` existe pero el usuario no tiene acceso
   - libvirtd está corriendo pero `virsh list` falla
   - La red `default` no está activa

**Pregunta de reflexión:** En un entorno de producción con decenas de
servidores de virtualización, ¿cómo automatizarías la verificación del
entorno? ¿Ejecutarías `virt-host-validate` periódicamente, o usarías
otra estrategia de monitoreo?
