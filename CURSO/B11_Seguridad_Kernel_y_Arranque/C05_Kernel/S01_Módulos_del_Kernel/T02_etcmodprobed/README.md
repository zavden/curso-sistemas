# /etc/modprobe.d/

## Índice
1. [Propósito de /etc/modprobe.d/](#propósito-de-etcmodprobed)
2. [Formato de los archivos](#formato-de-los-archivos)
3. [Blacklisting: impedir carga de módulos](#blacklisting-impedir-carga-de-módulos)
4. [options: parámetros persistentes](#options-parámetros-persistentes)
5. [alias: nombres alternativos](#alias-nombres-alternativos)
6. [install y remove: comandos personalizados](#install-y-remove-comandos-personalizados)
7. [softdep: dependencias opcionales](#softdep-dependencias-opcionales)
8. [Precedencia y orden de archivos](#precedencia-y-orden-de-archivos)
9. [Efecto en el initramfs](#efecto-en-el-initramfs)
10. [Errores comunes](#errores-comunes)
11. [Cheatsheet](#cheatsheet)
12. [Ejercicios](#ejercicios)

---

## Propósito de /etc/modprobe.d/

Cuando `modprobe` carga un módulo, no solo consulta `modules.dep` para las dependencias — también lee todos los archivos en `/etc/modprobe.d/` para aplicar configuraciones adicionales:

```
  ┌──────────────────────────────────────────────────────────────┐
  │                     modprobe <módulo>                         │
  │                                                              │
  │  1. Leer /etc/modprobe.d/*.conf                              │
  │     ├── ¿Está blacklisteado? → No cargar                     │
  │     ├── ¿Tiene opciones? → Pasar parámetros                  │
  │     ├── ¿Tiene alias? → Resolver nombre real                 │
  │     ├── ¿Tiene install/remove? → Ejecutar comando            │
  │     └── ¿Tiene softdep? → Cargar dependencias opcionales     │
  │                                                              │
  │  2. Leer modules.dep                                         │
  │     └── Resolver dependencias obligatorias                   │
  │                                                              │
  │  3. Cargar módulos en orden                                  │
  │     └── insmod con parámetros aplicados                      │
  └──────────────────────────────────────────────────────────────┘
```

Esto permite al administrador **controlar el comportamiento de los módulos de forma persistente** sin recompilar nada.

---

## Formato de los archivos

### Ubicación y convenciones

```bash
ls /etc/modprobe.d/
# blacklist.conf
# blacklist-nouveau.conf
# dist.conf
# iwlwifi.conf
# lockdown.conf
# nvidia.conf
# tuned.conf
# vhost.conf
```

Reglas:

- Los archivos **deben** terminar en `.conf` (sin extensión = ignorados)
- Se procesan en **orden alfabético**
- Líneas que empiezan con `#` son comentarios
- Líneas vacías se ignoran
- Cada directiva ocupa una línea

### Directivas disponibles

| Directiva | Propósito |
|---|---|
| `blacklist <module>` | Impedir carga automática (no manual) |
| `options <module> <params>` | Pasar parámetros al cargar |
| `alias <nombre> <module>` | Crear nombre alternativo |
| `install <module> <command>` | Ejecutar comando en vez de insmod |
| `remove <module> <command>` | Ejecutar comando al descargar |
| `softdep <module> pre: <deps>` | Dependencias opcionales |

---

## Blacklisting: impedir carga de módulos

El caso de uso más común de `/etc/modprobe.d/` es **blacklisting** — impedir que un módulo se cargue automáticamente.

### ¿Por qué blacklistear?

| Escenario | Módulo | Razón |
|---|---|---|
| Usar driver NVIDIA propietario | `nouveau` | Conflicto con nvidia.ko |
| Deshabilitar USB storage (seguridad) | `usb_storage` | Prevenir extracción de datos |
| Evitar driver buggy | `pcspkr` | Beep molesto del PC speaker |
| Preferir un driver sobre otro | `radeon` | Usar `amdgpu` en su lugar |
| Cumplimiento de seguridad | `firewire_core` | Prevenir DMA attacks vía FireWire |

### Cómo blacklistear

```bash
# Create a blacklist file
sudo tee /etc/modprobe.d/blacklist-nouveau.conf << 'EOF'
# Prevent nouveau from loading (using NVIDIA proprietary driver)
blacklist nouveau
EOF
```

### blacklist vs install /bin/true vs install /bin/false

`blacklist` solo impide la **carga automática** (por udev/modalias). El módulo aún se puede cargar manualmente con `modprobe`:

```bash
# blacklist: blocks auto-load, allows manual load
blacklist nouveau
# udev won't load it, but "sudo modprobe nouveau" still works

# To COMPLETELY prevent loading (even manual), use install:
install nouveau /bin/false
# Any attempt to load nouveau runs /bin/false instead → fails
# "sudo modprobe nouveau" → FATAL: Module nouveau not found

# Or the softer version that succeeds silently:
install nouveau /bin/true
# "sudo modprobe nouveau" → appears to succeed, but module is NOT loaded
```

```
  ┌────────────────────────────────────────────────────────────┐
  │  Nivel de bloqueo:                                         │
  │                                                            │
  │  blacklist nouveau                                         │
  │  └── udev no lo carga automáticamente                      │
  │  └── modprobe nouveau → SÍ carga                           │
  │  └── modprobe -r nouveau → SÍ descarga                     │
  │                                                            │
  │  blacklist nouveau                                         │
  │  install nouveau /bin/false                                │
  │  └── udev no lo carga                                      │
  │  └── modprobe nouveau → FALLA (ejecuta /bin/false)         │
  │  └── insmod /path/nouveau.ko → SÍ carga (bypasea modprobe)│
  │                                                            │
  │  Nota: insmod SIEMPRE funciona porque no lee modprobe.d    │
  └────────────────────────────────────────────────────────────┘
```

### Blacklist de distribución

Las distribuciones incluyen blacklists predeterminados:

```bash
cat /etc/modprobe.d/blacklist.conf
# Typical entries:
# blacklist pcspkr           ← PC speaker beep
# blacklist snd_pcsp         ← PC speaker sound driver
# blacklist floppy           ← Floppy disk driver
# blacklist amd76x_edac      ← Conflicts with newer EDAC

# NVIDIA-related (installed by nvidia driver package)
cat /etc/modprobe.d/blacklist-nouveau.conf
# blacklist nouveau
# options nouveau modeset=0
```

### Verificar blacklists activos

```bash
# List all blacklisted modules
grep -r "^blacklist" /etc/modprobe.d/
# /etc/modprobe.d/blacklist.conf:blacklist pcspkr
# /etc/modprobe.d/blacklist-nouveau.conf:blacklist nouveau

# Check if a specific module is blacklisted
modprobe --showconfig | grep "^blacklist" | grep nouveau
# blacklist nouveau

# Full modprobe configuration (all directives)
modprobe --showconfig | head -30
```

---

## options: parámetros persistentes

La directiva `options` pasa parámetros a un módulo **cada vez** que se carga, sin necesidad de especificarlos en la línea de comandos.

### Sintaxis

```bash
# /etc/modprobe.d/module-options.conf
options <module_name> param1=value1 param2=value2
```

### Ejemplos prácticos

```bash
# ── Bonding (agregación de interfaces de red) ──
# /etc/modprobe.d/bonding.conf
options bonding mode=4 miimon=100 lacp_rate=1

# ── iwlwifi (driver Intel WiFi) ──
# /etc/modprobe.d/iwlwifi.conf
options iwlwifi power_save=0 11n_disable=1

# ── snd-hda-intel (audio) ──
# /etc/modprobe.d/audio.conf
options snd-hda-intel model=auto power_save=0

# ── nouveau (antes de blacklisting, para deshabilitar modesetting) ──
# /etc/modprobe.d/nouveau.conf
options nouveau modeset=0

# ── USB autosuspend (desactivar para ciertos dispositivos) ──
# /etc/modprobe.d/usb.conf
options usbcore autosuspend=-1

# ── kvm (virtualización) ──
# /etc/modprobe.d/kvm.conf
options kvm_intel nested=1          # Enable nested virtualization
options kvm ignore_msrs=1           # Ignore unknown MSRs (for GPU passthrough)
```

### Verificar que las opciones aplican

```bash
# Method 1: check /sys/module
cat /sys/module/kvm_intel/parameters/nested
# Y

# Method 2: modprobe dry run
modprobe -n -v kvm_intel
# install /sbin/modprobe --ignore-install kvm_intel && ...
# Shows options being applied

# Method 3: full config
modprobe --showconfig | grep "^options kvm"
# options kvm_intel nested=1
```

---

## alias: nombres alternativos

Los aliases permiten referirse a un módulo por un nombre diferente. El sistema usa esto masivamente para mapear hardware a drivers.

### Aliases del sistema (modules.alias)

```bash
# System-generated aliases (from module metadata):
head -5 /lib/modules/$(uname -r)/modules.alias
# alias pci:v00008086d000015F3... e1000e
# alias usb:v046Dp0825d*... uvcvideo

# These are generated by depmod from the alias fields in each .ko
```

### Aliases personalizados en /etc/modprobe.d/

```bash
# /etc/modprobe.d/aliases.conf

# Use a friendly name for a module
alias mynetwork e1000e

# Now you can do:
# sudo modprobe mynetwork   → loads e1000e

# Override which module handles a device class
alias net-pf-10 off        # Disable IPv6 module auto-load
alias char-major-10-130 softdog   # Watchdog device → softdog module
```

### Alias especial: off

```bash
# "off" is a special alias that prevents loading
alias net-pf-10 off
# When something requests net-pf-10 (IPv6), nothing loads
```

> **Nota**: `alias x off` y `blacklist x` tienen efectos similares pero trabajan en niveles diferentes. `blacklist` impide carga por modalias; `alias x off` redirige una solicitud a "nada".

---

## install y remove: comandos personalizados

`install` reemplaza la acción normal de cargar un módulo por un **comando arbitrario**. `remove` hace lo mismo al descargar.

### install

```bash
# Syntax:
install <module> <command>

# When "modprobe <module>" is called, <command> runs instead of insmod
```

Usos comunes:

```bash
# ── Bloquear completamente un módulo ──
# /etc/modprobe.d/block-usb-storage.conf
install usb_storage /bin/false

# ── Cargar módulo + ejecutar acción adicional ──
# /etc/modprobe.d/custom-load.conf
install batman_adv /sbin/modprobe --ignore-install batman_adv && \
    /usr/sbin/batctl ra enable

# --ignore-install: tell modprobe to actually load the module
# (without this, it would call the install command recursively)

# ── Log cuando se carga un módulo ──
# /etc/modprobe.d/audit-usb.conf
install usb_storage /usr/bin/logger "USB storage loaded by $(whoami)" && \
    /sbin/modprobe --ignore-install usb_storage
```

### remove

```bash
# Syntax:
remove <module> <command>

# When "modprobe -r <module>" is called, <command> runs instead of rmmod

# Example: cleanup after removing a module
remove batman_adv /usr/sbin/batctl ra disable; \
    /sbin/modprobe -r --ignore-remove batman_adv
```

### El patrón --ignore-install / --ignore-remove

Cuando usas `install` o `remove`, necesitas `--ignore-install` / `--ignore-remove` para evitar recursión infinita:

```
  Sin --ignore-install:
  modprobe foo
    → ejecuta "install foo" command
      → command incluye "modprobe foo"
        → ejecuta "install foo" command  ← LOOP INFINITO

  Con --ignore-install:
  modprobe foo
    → ejecuta "install foo" command
      → command incluye "modprobe --ignore-install foo"
        → carga foo normalmente (ignora la directiva install)
```

---

## softdep: dependencias opcionales

`softdep` declara dependencias que **deberían** cargarse pero cuya ausencia no es fatal. A diferencia de `depends` (hardcoded en el módulo), `softdep` es configurable por el administrador.

### Sintaxis

```bash
# Load pre-dependencies BEFORE the module:
softdep <module> pre: <dep1> <dep2> ...

# Load post-dependencies AFTER the module:
softdep <module> post: <dep1> <dep2> ...

# Both:
softdep <module> pre: <dep1> post: <dep2>
```

### Ejemplos

```bash
# /etc/modprobe.d/softdeps.conf

# Load crc32c before ext4 (for metadata checksums)
softdep ext4 pre: crc32c

# Load nvidia-uvm after nvidia (for CUDA support)
softdep nvidia post: nvidia_uvm nvidia_modeset nvidia_drm

# Ensure crypto modules load before dm-crypt
softdep dm_crypt pre: aesni_intel aes_x86_64
```

### softdep vs depends

```
  ┌────────────────────────────────────────────────────────────┐
  │                                                            │
  │  depends (hardcoded en el .ko)                             │
  │  • Definido al compilar el módulo                          │
  │  • No se puede cambiar sin recompilar                      │
  │  • Si falta una dependencia → modprobe FALLA               │
  │  • Ejemplo: ext4 depends on jbd2,mbcache                  │
  │                                                            │
  │  softdep (configurable en /etc/modprobe.d/)                │
  │  • Definido por el admin o la distribución                 │
  │  • Modificable sin recompilar                              │
  │  • Si falta una soft-dependency → modprobe CONTINÚA        │
  │  • Ejemplo: ext4 softdep pre: crc32c                      │
  │                                                            │
  └────────────────────────────────────────────────────────────┘
```

---

## Precedencia y orden de archivos

### Orden de lectura

```bash
# modprobe reads configuration from these locations (in order):
# 1. /etc/modprobe.d/*.conf         ← Admin overrides (highest priority)
# 2. /run/modprobe.d/*.conf         ← Runtime overrides (transient)
# 3. /usr/lib/modprobe.d/*.conf     ← Distribution defaults (lowest priority)

# Within each directory, files are read in alphabetical order
# Later entries override earlier ones for the same module
```

```
  ┌────────────────────────────────────────────────────────────┐
  │  PRECEDENCIA (mayor a menor)                               │
  │                                                            │
  │  /etc/modprobe.d/99-custom.conf          ← Tú             │
  │  /etc/modprobe.d/50-nvidia.conf          ← Tú / paquete   │
  │  /etc/modprobe.d/blacklist.conf          ← Paquete         │
  │  /run/modprobe.d/*.conf                  ← Runtime         │
  │  /usr/lib/modprobe.d/dist-blacklist.conf ← Distribución   │
  │                                                            │
  │  Si dos archivos tienen la misma directiva para el mismo   │
  │  módulo, gana la directiva del archivo procesado ÚLTIMO    │
  │  (mayor orden alfabético dentro del directorio de mayor    │
  │  prioridad).                                               │
  └────────────────────────────────────────────────────────────┘
```

### /usr/lib/modprobe.d/ vs /etc/modprobe.d/

```bash
# Distribution defaults (don't edit — overwritten by updates)
ls /usr/lib/modprobe.d/
# dist-blacklist.conf
# systemd.conf
# ...

# Admin overrides (your customizations go here)
ls /etc/modprobe.d/
# Custom files that override /usr/lib/modprobe.d/

# Example: distro blacklists pcspkr in /usr/lib/modprobe.d/
# You want to UN-blacklist it (weird, but possible):
# Don't edit /usr/lib/modprobe.d/ — create override in /etc/
```

### Ver la configuración efectiva

```bash
# Show ALL configuration that modprobe will use
modprobe --showconfig

# Filter by type
modprobe --showconfig | grep "^blacklist"
modprobe --showconfig | grep "^options"
modprobe --showconfig | grep "^alias"
modprobe --showconfig | grep "^install"
modprobe --showconfig | grep "^softdep"
```

---

## Efecto en el initramfs

Los archivos de `/etc/modprobe.d/` se **copian al initramfs** durante su generación. Esto significa que los blacklists y opciones también aplican durante el arranque temprano (dentro del initramfs).

```
  ┌────────────────────────────────────────────────────────────┐
  │  CAMBIO EN /etc/modprobe.d/                                │
  │                                                            │
  │  ¿El módulo afectado se carga durante el arranque?         │
  │                                                            │
  │  NO → El cambio aplica inmediatamente                      │
  │       (próxima vez que el módulo se cargue)                │
  │                                                            │
  │  SÍ → Necesitas regenerar el initramfs:                    │
  │       sudo dracut --force          # RHEL                  │
  │       sudo update-initramfs -u     # Debian                │
  │                                                            │
  │  ¿Cómo saber si el módulo se carga durante el boot?        │
  │  → Verificar si está en el initramfs:                      │
  │    lsinitrd /boot/initramfs-$(uname -r).img | grep module  │
  │  → O si aparece antes de que / se monte:                   │
  │    dmesg | grep -i module_name                             │
  └────────────────────────────────────────────────────────────┘
```

Ejemplo práctico — blacklistear `nouveau` para usar NVIDIA propietario:

```bash
# 1. Create blacklist
sudo tee /etc/modprobe.d/blacklist-nouveau.conf << 'EOF'
blacklist nouveau
options nouveau modeset=0
EOF

# 2. Regenerate initramfs (nouveau loads during boot for display)
sudo dracut --force          # RHEL/Fedora
sudo update-initramfs -u     # Debian/Ubuntu

# 3. Reboot
sudo reboot

# Without step 2, nouveau would still load from the OLD initramfs
# during boot, before /etc/modprobe.d/ on disk is even accessible
```

---

## Errores comunes

### 1. Crear archivo sin extensión .conf

```bash
# WRONG: file without .conf extension is IGNORED
sudo tee /etc/modprobe.d/blacklist-nouveau << 'EOF'
blacklist nouveau
EOF
# modprobe completely ignores this file

# RIGHT: must have .conf extension
sudo tee /etc/modprobe.d/blacklist-nouveau.conf << 'EOF'
blacklist nouveau
EOF
```

### 2. Pensar que blacklist impide toda carga

```bash
# blacklist only prevents AUTO-loading (by udev/modalias)
# Manual loading still works:

echo "blacklist pcspkr" | sudo tee /etc/modprobe.d/no-beep.conf
sudo modprobe pcspkr
lsmod | grep pcspkr
# pcspkr    16384  0    ← It loaded!

# To truly prevent ALL loading:
echo "install pcspkr /bin/false" | sudo tee -a /etc/modprobe.d/no-beep.conf
sudo modprobe pcspkr
# modprobe: ERROR: could not insert 'pcspkr': Operation not permitted
```

### 3. No regenerar initramfs después de blacklistear un módulo de boot

Si el módulo que blacklistas se carga durante el arranque temprano (dentro del initramfs), el blacklist no surte efecto hasta que regeneres el initramfs:

```bash
# Blacklisting nouveau WITHOUT regenerating initramfs:
# Boot: initramfs loads nouveau (old config) → conflict with nvidia
# After pivot_root: /etc/modprobe.d/ blacklist is active → too late

# Always regenerate after blacklisting boot-time modules
```

### 4. Usar guiones vs guiones bajos inconsistentemente

```bash
# Module names use underscores internally:
lsmod | grep snd_hda_intel
# snd_hda_intel  57344  0

# But both work in modprobe.d:
options snd-hda-intel power_save=0    # Works (converted to _)
options snd_hda_intel power_save=0    # Also works

# HOWEVER: be consistent within a file to avoid confusion
# Recommendation: always use underscores (matches lsmod output)
```

### 5. Olvidar --ignore-install en directivas install

```bash
# WRONG: infinite recursion
install my_module /sbin/modprobe my_module && /usr/bin/do_something
# modprobe my_module → runs install command → calls modprobe my_module → ...

# RIGHT: --ignore-install breaks the recursion
install my_module /sbin/modprobe --ignore-install my_module && \
    /usr/bin/do_something
```

---

## Cheatsheet

```bash
# ═══════════════════════════════════════════════
#  ARCHIVOS
# ═══════════════════════════════════════════════
/etc/modprobe.d/*.conf           # Admin config (highest priority)
/run/modprobe.d/*.conf           # Runtime overrides
/usr/lib/modprobe.d/*.conf       # Distro defaults (don't edit)

# ═══════════════════════════════════════════════
#  BLACKLISTING
# ═══════════════════════════════════════════════
# Prevent auto-load (manual still works):
blacklist <module>

# Prevent ALL loading (even manual):
blacklist <module>
install <module> /bin/false

# ═══════════════════════════════════════════════
#  OPTIONS (parámetros persistentes)
# ═══════════════════════════════════════════════
options <module> param1=val1 param2=val2

# ═══════════════════════════════════════════════
#  ALIAS
# ═══════════════════════════════════════════════
alias <name> <real_module>
alias <name> off                 # Disable auto-load for alias

# ═══════════════════════════════════════════════
#  INSTALL / REMOVE (comandos custom)
# ═══════════════════════════════════════════════
install <module> /sbin/modprobe --ignore-install <module> && <extra_cmd>
remove <module> <cleanup_cmd>; /sbin/modprobe -r --ignore-remove <module>

# ═══════════════════════════════════════════════
#  SOFTDEP (dependencias opcionales)
# ═══════════════════════════════════════════════
softdep <module> pre: <dep1> <dep2>
softdep <module> post: <dep1>
softdep <module> pre: <dep1> post: <dep2>

# ═══════════════════════════════════════════════
#  VERIFICAR
# ═══════════════════════════════════════════════
modprobe --showconfig                    # All effective config
modprobe --showconfig | grep "^blacklist"
grep -r "^blacklist" /etc/modprobe.d/    # Blacklists in files
grep -r "^options" /etc/modprobe.d/      # Options in files

# ═══════════════════════════════════════════════
#  DESPUÉS DE CAMBIOS
# ═══════════════════════════════════════════════
# If module loads at boot → regenerate initramfs:
sudo dracut --force                      # RHEL
sudo update-initramfs -u                 # Debian
```

---

## Ejercicios

### Ejercicio 1: Explorar la configuración de módulos

```bash
# Step 1: list all config files
echo "=== /etc/modprobe.d/ ==="
ls -la /etc/modprobe.d/

echo "=== /usr/lib/modprobe.d/ ==="
ls -la /usr/lib/modprobe.d/

# Step 2: find all blacklisted modules
echo "=== Blacklists ==="
grep -r "^blacklist" /etc/modprobe.d/ /usr/lib/modprobe.d/ 2>/dev/null

# Step 3: find all module options
echo "=== Options ==="
grep -r "^options" /etc/modprobe.d/ /usr/lib/modprobe.d/ 2>/dev/null

# Step 4: find all aliases
echo "=== Aliases ==="
grep -r "^alias" /etc/modprobe.d/ /usr/lib/modprobe.d/ 2>/dev/null

# Step 5: find install/remove overrides
echo "=== Install/Remove ==="
grep -r "^install\|^remove" /etc/modprobe.d/ /usr/lib/modprobe.d/ 2>/dev/null

# Step 6: view the effective merged configuration
modprobe --showconfig | grep -c "^blacklist"
modprobe --showconfig | grep -c "^options"
modprobe --showconfig | grep -c "^alias"

# Step 7: check if any blacklisted module is somehow still loaded
for mod in $(grep -rh "^blacklist" /etc/modprobe.d/ /usr/lib/modprobe.d/ 2>/dev/null | awk '{print $2}'); do
    if lsmod | grep -q "^${mod} "; then
        echo "WARNING: $mod is blacklisted but loaded!"
    fi
done
```

**Preguntas:**
1. ¿Cuántos módulos están blacklisteados en tu sistema? ¿Reconoces por qué cada uno?
2. ¿Hay opciones configuradas para algún módulo? ¿Qué hacen?
3. ¿Algún módulo blacklisteado está cargado de todas formas? ¿Cómo es posible?

> **Pregunta de predicción**: si creas `/etc/modprobe.d/test` (sin `.conf`) con `blacklist pcspkr`, ¿`modprobe --showconfig | grep pcspkr` mostrará el blacklist? ¿Por qué?

### Ejercicio 2: Blacklistear y des-blacklistear un módulo

```bash
# We'll use "pcspkr" (PC speaker) as a safe test module

# Step 1: check if pcspkr is loaded
lsmod | grep pcspkr
# May or may not be loaded

# Step 2: check if it's already blacklisted
grep -r pcspkr /etc/modprobe.d/ /usr/lib/modprobe.d/ 2>/dev/null

# Step 3: load it (if not loaded and not blacklisted)
sudo modprobe pcspkr 2>/dev/null
lsmod | grep pcspkr

# Step 4: create a blacklist
sudo tee /etc/modprobe.d/lab-blacklist-pcspkr.conf << 'EOF'
# Lab exercise: blacklist pcspkr
blacklist pcspkr
EOF

# Step 5: verify the blacklist is active
modprobe --showconfig | grep pcspkr

# Step 6: unload the module
sudo modprobe -r pcspkr 2>/dev/null

# Step 7: try to load it manually (blacklist allows manual load!)
sudo modprobe pcspkr
lsmod | grep pcspkr
# pcspkr is loaded — blacklist only blocks AUTO-load

# Step 8: upgrade to full block
sudo tee /etc/modprobe.d/lab-blacklist-pcspkr.conf << 'EOF'
blacklist pcspkr
install pcspkr /bin/false
EOF

# Step 9: unload and try again
sudo modprobe -r pcspkr 2>/dev/null
sudo modprobe pcspkr
# Should fail now

# Step 10: clean up
sudo rm /etc/modprobe.d/lab-blacklist-pcspkr.conf
```

**Preguntas:**
1. ¿`blacklist` sola impidió la carga manual? ¿Qué hizo falta para bloquearla completamente?
2. ¿Qué mostraba `modprobe --showconfig | grep pcspkr` en cada paso?
3. Si este fuera un módulo que se carga en el initramfs, ¿qué paso adicional necesitarías?

> **Pregunta de predicción**: si blacklistas `pcspkr` con `install pcspkr /bin/false`, ¿qué pasa si otro módulo tiene `pcspkr` como dependencia dura (`depends`)? ¿Ese otro módulo fallará al cargar?

### Ejercicio 3: Configurar opciones de módulo

```bash
# We'll configure options for kvm_intel (or kvm_amd) to enable nested virt

# Step 1: check if KVM is available and which variant
lsmod | grep kvm
# kvm_intel or kvm_amd should appear

# Step 2: check current nested virtualization status
KVM_MOD=$(lsmod | grep -oE "kvm_(intel|amd)" | head -1)
echo "KVM module: $KVM_MOD"

if [ -n "$KVM_MOD" ]; then
    echo "Nested virt: $(cat /sys/module/$KVM_MOD/parameters/nested)"
fi

# Step 3: check available parameters
modinfo -F parm $KVM_MOD 2>/dev/null

# Step 4: create options file
sudo tee /etc/modprobe.d/lab-kvm-options.conf << EOF
# Enable nested virtualization
options $KVM_MOD nested=1
EOF

# Step 5: verify the option is in modprobe config
modprobe --showconfig | grep "^options $KVM_MOD"

# Step 6: to apply, you'd need to reload the module:
echo "To apply: sudo modprobe -r $KVM_MOD && sudo modprobe $KVM_MOD"
echo "(Only if no VMs are running!)"

# Step 7: verify after reload (if possible)
# cat /sys/module/$KVM_MOD/parameters/nested
# Should show: Y

# Step 8: clean up
sudo rm /etc/modprobe.d/lab-kvm-options.conf
```

**Preguntas:**
1. ¿La opción `nested` estaba habilitada antes de crear el archivo de configuración?
2. ¿Por qué hay que descargar y recargar el módulo para que la opción aplique?
3. ¿Qué pasaría si hay VMs corriendo y intentas `modprobe -r kvm_intel`?

> **Pregunta de predicción**: si añades `options ext4 debug=1` al archivo de configuración pero no reinicias ni desmontas filesystems ext4, ¿la opción tendrá efecto? ¿Puedes cambiar el parámetro vía `/sys/module/ext4/parameters/debug` en caliente?

---

> **Siguiente tema**: T03 — depmod: dependencias entre módulos, modules.dep.
