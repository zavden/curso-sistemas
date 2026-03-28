# depmod

## Índice
1. [¿Qué es depmod?](#qué-es-depmod)
2. [modules.dep: el mapa de dependencias](#modulesdep-el-mapa-de-dependencias)
3. [Otros archivos generados por depmod](#otros-archivos-generados-por-depmod)
4. [Uso de depmod](#uso-de-depmod)
5. [Cómo se construyen las dependencias](#cómo-se-construyen-las-dependencias)
6. [modules.alias: hardware → driver](#modulesalias-hardware--driver)
7. [modules.symbols: símbolos exportados](#modulessymbols-símbolos-exportados)
8. [Cuándo se ejecuta depmod](#cuándo-se-ejecuta-depmod)
9. [Errores comunes](#errores-comunes)
10. [Cheatsheet](#cheatsheet)
11. [Ejercicios](#ejercicios)

---

## ¿Qué es depmod?

`depmod` analiza todos los módulos del kernel en `/lib/modules/<version>/` y genera archivos de metadatos que `modprobe` usa para resolver dependencias, encontrar módulos por alias y cargar el módulo correcto para cada dispositivo.

```
  ┌──────────────────────────────────────────────────────────────┐
  │                                                              │
  │  /lib/modules/6.7.4/kernel/                                  │
  │  ├── drivers/ata/ahci.ko.xz                                  │
  │  ├── drivers/nvme/host/nvme.ko.xz                            │
  │  ├── fs/ext4/ext4.ko.xz                                      │
  │  ├── fs/jbd2/jbd2.ko.xz                                      │
  │  └── ... (miles de .ko)                                       │
  │                                                              │
  │         │  depmod -a                                          │
  │         ▼                                                    │
  │                                                              │
  │  /lib/modules/6.7.4/                                          │
  │  ├── modules.dep          ← Quién depende de quién           │
  │  ├── modules.dep.bin      ← Versión binaria (rápida)         │
  │  ├── modules.alias        ← PCI/USB ID → módulo              │
  │  ├── modules.alias.bin    ← Versión binaria                  │
  │  ├── modules.symbols      ← Símbolo → módulo que lo exporta  │
  │  ├── modules.symbols.bin  ← Versión binaria                  │
  │  ├── modules.builtin      ← Módulos compilados built-in      │
  │  ├── modules.devname      ← Device nodes at boot             │
  │  ├── modules.softdep      ← Soft dependencies                │
  │  └── modules.order        ← Orden preferido de carga         │
  │                                                              │
  │  modprobe lee estos archivos para:                            │
  │  • Resolver dependencias (modules.dep)                       │
  │  • Encontrar el driver correcto (modules.alias)              │
  │  • Resolver símbolos (modules.symbols)                       │
  └──────────────────────────────────────────────────────────────┘
```

Sin estos archivos, `modprobe` no puede funcionar — tendría que escanear cada `.ko` individualmente, lo que sería extremadamente lento.

---

## modules.dep: el mapa de dependencias

`modules.dep` es el archivo central. Cada línea mapea un módulo a la lista de módulos de los que depende.

### Formato

```
ruta/del/módulo.ko: ruta/dep1.ko ruta/dep2.ko ...
```

```bash
# View dependency entries
grep ext4 /lib/modules/$(uname -r)/modules.dep
# kernel/fs/ext4/ext4.ko.xz: kernel/fs/jbd2/jbd2.ko.xz kernel/fs/mbcache/mbcache.ko.xz

# Interpretation:
# ext4.ko depends on jbd2.ko AND mbcache.ko
# modprobe will load jbd2 and mbcache BEFORE ext4

# Module with no dependencies (empty after colon):
grep "nvme-core" /lib/modules/$(uname -r)/modules.dep
# kernel/drivers/nvme/host/nvme-core.ko.xz:

# Module that depends on nvme-core:
grep "nvme.ko" /lib/modules/$(uname -r)/modules.dep
# kernel/drivers/nvme/host/nvme.ko.xz: kernel/drivers/nvme/host/nvme-core.ko.xz
```

### Cadenas de dependencia

Las dependencias pueden ser transitivas — `modprobe` las resuelve recursivamente:

```
  ext4 → jbd2, mbcache
  dm_crypt → dm_mod
  dm_mod → (nada)

  Si cargas ext4:
  modprobe ext4
    1. Lee modules.dep → ext4 depende de jbd2, mbcache
    2. Lee modules.dep → jbd2 depende de (nada)
    3. Lee modules.dep → mbcache depende de (nada)
    4. insmod mbcache.ko
    5. insmod jbd2.ko
    6. insmod ext4.ko
```

### modules.dep.bin

Es una versión binaria (formato de hash) de `modules.dep`, optimizada para búsquedas rápidas. `modprobe` usa `.bin` si existe; si no, lee el `.dep` de texto.

```bash
# Text version (human readable)
file /lib/modules/$(uname -r)/modules.dep
# ASCII text

# Binary version (machine optimized)
file /lib/modules/$(uname -r)/modules.dep.bin
# data (binary format)

# Both contain the same information
# modprobe prefers .bin for speed
```

---

## Otros archivos generados por depmod

### Tabla completa

| Archivo | Contenido | Usado por |
|---|---|---|
| `modules.dep` | Dependencias entre módulos | modprobe |
| `modules.dep.bin` | Versión binaria de .dep | modprobe |
| `modules.alias` | Mapeo modalias → módulo | modprobe, udev |
| `modules.alias.bin` | Versión binaria de .alias | modprobe |
| `modules.symbols` | Mapeo símbolo → módulo | modprobe |
| `modules.symbols.bin` | Versión binaria de .symbols | modprobe |
| `modules.builtin` | Lista de módulos built-in | modprobe, modinfo |
| `modules.builtin.modinfo` | Metadatos de built-in | modinfo |
| `modules.devname` | Módulos que crean /dev nodes | systemd-tmpfiles |
| `modules.softdep` | Dependencias soft (del .ko) | modprobe |
| `modules.order` | Orden preferido de carga | modprobe |

### modules.builtin

Este archivo **no** lo genera depmod — lo genera el proceso de compilación del kernel. Lista los módulos que están compilados directamente en vmlinuz:

```bash
head -10 /lib/modules/$(uname -r)/modules.builtin
# kernel/arch/x86/crypto/crc32c-intel.ko
# kernel/fs/ramfs/ramfs.ko
# kernel/fs/configfs/configfs.ko
# ...

# These modules are ALWAYS available (no modprobe needed)
# They appear in /sys/module/ but NOT in lsmod
```

### modules.devname

Mapea módulos a los device nodes que crean. Permite que systemd cree los `/dev` entries antes de que los módulos estén cargados:

```bash
cat /lib/modules/$(uname -r)/modules.devname
# fuse      devname:fuse    char 10:229
# loop      devname:loop-control    char 10:237
# tun       devname:net/tun    char 10:200
# ...

# systemd-tmpfiles uses this to pre-create /dev/fuse, /dev/net/tun, etc.
# so that processes can open them and trigger module auto-loading
```

---

## Uso de depmod

### Uso básico

```bash
# Regenerate ALL metadata files for the current kernel
sudo depmod -a
# or simply:
sudo depmod

# Regenerate for a specific kernel version
sudo depmod -a 6.7.4-200.fc39.x86_64

# Equivalent (explicit version):
sudo depmod 6.7.4-200.fc39.x86_64
```

### Opciones útiles

```bash
# Verbose: show what depmod is doing
sudo depmod -a -v | head -20
# /lib/modules/6.7.4/kernel/drivers/ata/ahci.ko.xz needs "libahci": ...
# /lib/modules/6.7.4/kernel/fs/ext4/ext4.ko.xz needs "jbd2": ...

# Dry run: show output without writing files
sudo depmod -a -n | head -20
# Prints what modules.dep WOULD contain

# Check for errors only (no output if clean)
sudo depmod -a -e
# Shows unresolved symbols (missing dependencies)

# Warn about modules that depend on missing symbols
sudo depmod -a -w
# Warnings for each unresolved symbol

# Write to a different base directory (for building initramfs, cross-compile)
sudo depmod -a -b /path/to/alternate/root 6.7.4

# Show the full path to a module (like modinfo -F filename)
sudo depmod -a -v 2>/dev/null | grep ext4
```

### ¿Qué hace depmod internamente?

```
  depmod -a
       │
       ▼
  1. Escanear /lib/modules/<version>/kernel/
     Encontrar todos los archivos .ko(.xz|.zst|.gz)
       │
       ▼
  2. Para cada .ko, leer la sección ELF de metadatos:
     ├── __versions: símbolos que NECESITA (imports)
     ├── __ksymtab:  símbolos que EXPORTA (exports)
     ├── .modinfo:   alias, license, depends, author, etc.
     └── vermagic:   versión del kernel
       │
       ▼
  3. Construir el grafo de dependencias:
     Si módulo A necesita símbolo X
     y módulo B exporta símbolo X
     → A depende de B
       │
       ▼
  4. Escribir los archivos de salida:
     modules.dep       (dependencias)
     modules.alias     (hardware aliases)
     modules.symbols   (exportaciones)
     modules.dep.bin   (versiones binarias)
     modules.alias.bin
     modules.symbols.bin
     modules.softdep
     modules.devname
```

---

## Cómo se construyen las dependencias

### Símbolos del kernel

Los módulos del kernel se comunican mediante **símbolos exportados**. Un módulo que exporta una función la hace disponible para otros módulos:

```
  ┌──────────────────────────────────────────────────────────┐
  │                                                          │
  │  jbd2.ko                                                 │
  │  ┌────────────────────────────────────┐                  │
  │  │ EXPORT_SYMBOL(jbd2_journal_init)   │ ← Exporta       │
  │  │ EXPORT_SYMBOL(jbd2_journal_start)  │                  │
  │  │ EXPORT_SYMBOL(jbd2_journal_stop)   │                  │
  │  └────────────────────────────────────┘                  │
  │                                                          │
  │  ext4.ko                                                 │
  │  ┌────────────────────────────────────┐                  │
  │  │ uses: jbd2_journal_init            │ ← Necesita       │
  │  │ uses: jbd2_journal_start           │                  │
  │  │ uses: jbd2_journal_stop            │                  │
  │  └────────────────────────────────────┘                  │
  │                                                          │
  │  depmod detecta:                                         │
  │  ext4 necesita jbd2_journal_* → jbd2 los exporta         │
  │  → ext4 depende de jbd2                                  │
  │                                                          │
  │  modules.dep:                                            │
  │  ext4.ko: jbd2.ko mbcache.ko                             │
  │                                                          │
  └──────────────────────────────────────────────────────────┘
```

### EXPORT_SYMBOL vs EXPORT_SYMBOL_GPL

```
  ┌────────────────────────────────────────────────────────────┐
  │                                                            │
  │  EXPORT_SYMBOL(func)                                       │
  │  → Disponible para TODOS los módulos (GPL y propietarios)  │
  │                                                            │
  │  EXPORT_SYMBOL_GPL(func)                                   │
  │  → Solo disponible para módulos con licencia GPL           │
  │  → Módulos propietarios no pueden usar estos símbolos      │
  │  → depmod marca esto como error si un módulo non-GPL       │
  │    intenta usar un símbolo GPL-only                        │
  │                                                            │
  └────────────────────────────────────────────────────────────┘
```

### Ver símbolos de un módulo

```bash
# Symbols EXPORTED by a module
modprobe --dump-modversions /lib/modules/$(uname -r)/kernel/fs/jbd2/jbd2.ko.xz 2>/dev/null | head -5
# 0x12345678    jbd2_journal_init_inode
# 0x87654321    jbd2_journal_start
# ...

# Symbols REQUIRED by a module (dependencies)
modprobe --dump-modversions /lib/modules/$(uname -r)/kernel/fs/ext4/ext4.ko.xz 2>/dev/null | head -5

# In modules.symbols:
grep jbd2_journal_init /lib/modules/$(uname -r)/modules.symbols
# alias symbol:jbd2_journal_init_inode jbd2
# → "The symbol jbd2_journal_init_inode is provided by the module jbd2"
```

---

## modules.alias: hardware → driver

`modules.alias` es el archivo que permite la **carga automática de drivers**. Cada módulo declara qué hardware soporta mediante cadenas de alias que contienen IDs de bus (PCI, USB, etc.).

### Formato

```bash
head -10 /lib/modules/$(uname -r)/modules.alias
# alias pci:v00008086d00001572sv*sd*bc02sc00i* ixgbe
# alias pci:v00008086d000015F3sv*sd*bc02sc00i* e1000e
# alias usb:v046DpC52Bd*dc*dsc*dp*ic*isc*ip*in* hid_logitech_dj
```

### Estructura de un alias PCI

```
  alias pci:v00008086d000015F3sv*sd*bc02sc00i* e1000e
        │   │         │         │  │  │    │    │
        │   │         │         │  │  │    │    └── Módulo
        │   │         │         │  │  │    └── Interface
        │   │         │         │  │  └── Subclass
        │   │         │         │  └── Base class (02=network)
        │   │         │         └── Subsystem device
        │   │         └── Subsystem vendor
        │   │         └── Device ID (15F3)
        │   └── Vendor ID (8086=Intel)
        └── Bus type (pci)

  * = wildcard (match anything)
```

### Flujo de auto-carga

```
  1. Hardware detectado
     lspci: 03:00.0 Ethernet [0200]: Intel [8086:15F3]
         │
         ▼
  2. Kernel genera uevent con modalias
     modalias = "pci:v00008086d000015F3sv0000103Csd00008783bc02sc00i00"
         │
         ▼
  3. udev recibe uevent, ejecuta:
     modprobe "pci:v00008086d000015F3sv0000103Csd00008783bc02sc00i00"
         │
         ▼
  4. modprobe busca en modules.alias.bin:
     "pci:v00008086d000015F3sv*sd*bc02sc00i*" → e1000e
         │
         ▼
  5. modprobe carga e1000e (y dependencias)
         │
         ▼
  6. Driver inicializa el hardware
     → Interface enp3s0 aparece
```

### Cómo depmod construye modules.alias

Los alias están embebidos en cada `.ko` como metadatos:

```bash
# View aliases embedded in a module
modinfo -F alias e1000e | head -5
# pci:v00008086d000015F3sv*sd*bc*sc*i*
# pci:v00008086d00001570sv*sd*bc*sc*i*
# ...

# depmod extracts these and writes them all to modules.alias
# This is why depmod must run after adding/removing any module
```

---

## modules.symbols: símbolos exportados

`modules.symbols` mapea cada símbolo exportado al módulo que lo proporciona. `modprobe` lo usa como fallback para resolver dependencias cuando `modules.dep` no es suficiente.

```bash
# Format:
grep "jbd2" /lib/modules/$(uname -r)/modules.symbols | head -5
# alias symbol:jbd2_journal_init_inode jbd2
# alias symbol:jbd2_journal_start jbd2
# alias symbol:jbd2_journal_stop jbd2

# Interpretation:
# If a module needs "jbd2_journal_start", modprobe knows
# it must load "jbd2" to provide that symbol
```

### Diferencia con modules.dep

```
  modules.dep:
  → "ext4 necesita jbd2 y mbcache" (resultado final)
  → modprobe lo usa directamente para cargar en orden

  modules.symbols:
  → "jbd2_journal_start es exportado por jbd2"
  → Es la FUENTE de información que depmod usa
    para CONSTRUIR modules.dep
  → modprobe lo usa como fallback
```

---

## Cuándo se ejecuta depmod

### Ejecución automática

```
  ┌────────────────────────────────────────────────────────────┐
  │  EVENTO                          │  ¿depmod se ejecuta?    │
  │                                  │                         │
  │  Instalar nuevo kernel           │  SÍ (post-install hook) │
  │  (dnf install kernel /           │  Los scripts del        │
  │   apt install linux-image-*)     │  paquete ejecutan depmod│
  │                                  │                         │
  │  DKMS compila un módulo          │  SÍ (DKMS lo invoca)    │
  │  (nvidia, virtualbox, etc.)      │                         │
  │                                  │                         │
  │  Copiar manualmente un .ko       │  NO — debes ejecutar    │
  │  a /lib/modules/                 │  depmod -a manualmente  │
  │                                  │                         │
  │  Actualizar kernel existente     │  SÍ (post-install hook) │
  │                                  │                         │
  │  Eliminar un kernel              │  Normalmente sí         │
  │  (dnf remove / apt remove)       │  (o se borra el dir)    │
  └────────────────────────────────────────────────────────────┘
```

### Ejecución manual necesaria

Debes ejecutar `depmod -a` manualmente cuando:

```bash
# 1. Compilaste un módulo manualmente y lo copiaste
make -C /lib/modules/$(uname -r)/build M=$(pwd) modules
sudo cp my_module.ko /lib/modules/$(uname -r)/extra/
sudo depmod -a    # ← Sin esto, modprobe no lo encuentra

# 2. Borraste un módulo manualmente
sudo rm /lib/modules/$(uname -r)/extra/old_module.ko
sudo depmod -a    # ← Actualizar modules.dep

# 3. modules.dep parece corrupto o desactualizado
sudo depmod -a    # ← Regenerar desde cero

# 4. Moved modules between directories
sudo mv /lib/modules/$(uname -r)/extra/mod.ko \
    /lib/modules/$(uname -r)/updates/
sudo depmod -a
```

### Directorios de búsqueda

depmod busca módulos en estos directorios dentro de `/lib/modules/<version>/`:

```
  /lib/modules/<version>/
  ├── kernel/        ← Módulos del kernel oficial
  ├── updates/       ← Actualizaciones (prioridad MAYOR que kernel/)
  ├── extra/         ← Módulos de terceros
  ├── weak-updates/  ← Symlinks a módulos compatibles de otros kernels
  └── misc/          ← Misceláneos
```

La prioridad es: `updates/` > `extra/` > `kernel/`. Si dos módulos con el mismo nombre existen en `updates/` y `kernel/`, `modprobe` usará el de `updates/`.

```bash
# View search order
cat /etc/depmod.d/*.conf 2>/dev/null
# search updates extra built-in

# Or check the compiled-in default
depmod --show-depends ext4 2>/dev/null
```

### /etc/depmod.d/: configuración de depmod

```bash
ls /etc/depmod.d/
# dist.conf

cat /etc/depmod.d/dist.conf
# search updates extra built-in
# → Define el orden de prioridad de directorios

# You can add overrides:
# override module_name kernel_version directory
# Example: always use the version from updates/
# override e1000e * updates
```

---

## Errores comunes

### 1. Copiar un .ko sin ejecutar depmod

```bash
# WRONG: module is invisible to modprobe
sudo cp my_module.ko /lib/modules/$(uname -r)/extra/
sudo modprobe my_module
# FATAL: Module my_module not found

# RIGHT: run depmod after adding modules
sudo cp my_module.ko /lib/modules/$(uname -r)/extra/
sudo depmod -a
sudo modprobe my_module
# Success
```

### 2. Confundir depmod con modprobe

```
  depmod:    genera los ARCHIVOS de metadatos (modules.dep, etc.)
             Se ejecuta UNA VEZ cuando cambian los módulos en disco.
             No carga ni descarga módulos.

  modprobe:  LEE los archivos generados por depmod
             para cargar/descargar módulos en el kernel.
             Se ejecuta cada vez que necesitas un módulo.

  Analogía:
  depmod   = actualizar el índice de una biblioteca
  modprobe = buscar un libro usando el índice
```

### 3. Ejecutar depmod para la versión de kernel equivocada

```bash
# depmod without arguments uses $(uname -r)
sudo depmod -a
# Regenerates for the RUNNING kernel

# If you installed modules for a NEW kernel (not yet running):
sudo depmod -a 6.8.0-100.fc40.x86_64
# Must specify the version explicitly
```

### 4. Ignorar warnings de depmod -e

```bash
sudo depmod -a -e
# WARNING: /lib/modules/6.7.4/extra/broken.ko needs unknown symbol do_something

# This means broken.ko depends on a symbol that NO loaded/available
# module provides. The module WILL FAIL to load with:
# modprobe: ERROR: could not insert 'broken': Unknown symbol in module

# Common causes:
# - Module compiled for a different kernel version
# - Missing dependency not installed
# - Module depends on a proprietary module not present
```

### 5. No entender la prioridad de directorios

```bash
# If you have:
# /lib/modules/6.7.4/kernel/drivers/net/ethernet/intel/e1000e/e1000e.ko
# /lib/modules/6.7.4/updates/e1000e.ko

# depmod will prefer updates/ over kernel/
# modprobe will load the one from updates/

# This is BY DESIGN — updates/ is for patched/newer versions
# Check which one modprobe would use:
modinfo -F filename e1000e
# Shows the path that modprobe would use
```

---

## Cheatsheet

```bash
# ═══════════════════════════════════════════════
#  REGENERAR METADATOS
# ═══════════════════════════════════════════════
sudo depmod -a                       # Current kernel
sudo depmod -a <version>             # Specific kernel
sudo depmod -a -v                    # Verbose
sudo depmod -a -n                    # Dry run (stdout)
sudo depmod -a -e                    # Show errors (unresolved symbols)
sudo depmod -a -w                    # Show warnings

# ═══════════════════════════════════════════════
#  ARCHIVOS GENERADOS
# ═══════════════════════════════════════════════
# All in /lib/modules/$(uname -r)/
cat modules.dep                      # Dependencies (text)
cat modules.alias | head             # Hardware → module (text)
cat modules.symbols | head           # Symbol → module (text)
cat modules.builtin                  # Built-in modules
cat modules.devname                  # Device nodes
cat modules.softdep                  # Soft dependencies
cat modules.order                    # Preferred load order

# ═══════════════════════════════════════════════
#  INSPECCIONAR DEPENDENCIAS
# ═══════════════════════════════════════════════
grep <module> /lib/modules/$(uname -r)/modules.dep
modprobe -n -v <module>              # Show full load order
modinfo -F depends <module>          # Direct dependencies

# ═══════════════════════════════════════════════
#  INSPECCIONAR ALIASES
# ═══════════════════════════════════════════════
grep <module> /lib/modules/$(uname -r)/modules.alias
modinfo -F alias <module>            # Aliases from module metadata
modprobe --resolve-alias "pci:v..."  # Which module for this alias?

# ═══════════════════════════════════════════════
#  INSPECCIONAR SÍMBOLOS
# ═══════════════════════════════════════════════
grep <symbol> /lib/modules/$(uname -r)/modules.symbols

# ═══════════════════════════════════════════════
#  CONFIGURACIÓN DE DEPMOD
# ═══════════════════════════════════════════════
cat /etc/depmod.d/*.conf             # Search order, overrides

# ═══════════════════════════════════════════════
#  AÑADIR MÓDULO MANUALMENTE
# ═══════════════════════════════════════════════
sudo cp module.ko /lib/modules/$(uname -r)/extra/
sudo depmod -a                       # Update metadata
sudo modprobe module                 # Now it's loadable
```

---

## Ejercicios

### Ejercicio 1: Explorar los archivos de metadatos

```bash
# Step 1: list all metadata files
ls -lh /lib/modules/$(uname -r)/modules.*

# Step 2: count entries in key files
echo "Dependencies: $(wc -l < /lib/modules/$(uname -r)/modules.dep)"
echo "Aliases: $(wc -l < /lib/modules/$(uname -r)/modules.alias)"
echo "Symbols: $(wc -l < /lib/modules/$(uname -r)/modules.symbols)"
echo "Built-in: $(wc -l < /lib/modules/$(uname -r)/modules.builtin)"

# Step 3: find a module with many dependencies
sort -t: -k2 /lib/modules/$(uname -r)/modules.dep | \
    awk -F: '{print NF-1, $1}' | sort -rn | head -5
# Shows modules with most dependencies

# Step 4: find a module that many others depend on
grep -o "[^ ]*\.ko" /lib/modules/$(uname -r)/modules.dep | \
    sort | uniq -c | sort -rn | head -10
# Shows most-depended-upon modules

# Step 5: compare text vs binary file sizes
echo "modules.dep:     $(stat -c%s /lib/modules/$(uname -r)/modules.dep) bytes"
echo "modules.dep.bin: $(stat -c%s /lib/modules/$(uname -r)/modules.dep.bin) bytes"
echo "modules.alias:     $(stat -c%s /lib/modules/$(uname -r)/modules.alias) bytes"
echo "modules.alias.bin: $(stat -c%s /lib/modules/$(uname -r)/modules.alias.bin) bytes"

# Step 6: check for modules in non-standard directories
ls /lib/modules/$(uname -r)/updates/ 2>/dev/null && \
    echo "Has updates/" || echo "No updates/"
ls /lib/modules/$(uname -r)/extra/ 2>/dev/null && \
    echo "Has extra/" || echo "No extra/"
ls /lib/modules/$(uname -r)/weak-updates/ 2>/dev/null && \
    echo "Has weak-updates/" || echo "No weak-updates/"
```

**Preguntas:**
1. ¿Cuántas entradas tiene `modules.dep` vs `modules.alias`? ¿Por qué uno tiene más que el otro?
2. ¿Cuál es el módulo con más dependencias? ¿Qué hace?
3. ¿Cuál es el módulo más "dependido" (del que más otros módulos dependen)?

> **Pregunta de predicción**: ¿el archivo `modules.dep.bin` será más grande o más pequeño que `modules.dep` (texto)? ¿Por qué?

### Ejercicio 2: Trazar una cadena de dependencias

```bash
# Step 1: pick a module and view its direct dependencies
MODULE="ext4"  # or xfs, btrfs, your filesystem
echo "=== Direct dependencies of $MODULE ==="
grep "/$MODULE\.ko" /lib/modules/$(uname -r)/modules.dep

# Step 2: use modprobe to see the full load order
echo "=== Full load order ==="
modprobe -n -v $MODULE

# Step 3: count how many modules would be loaded
COUNT=$(modprobe -n -v $MODULE 2>/dev/null | grep -c "^insmod")
echo "Total modules in chain: $COUNT"

# Step 4: do the same for a complex module
echo "=== nf_tables dependency chain ==="
modprobe -n -v nf_tables 2>/dev/null

# Step 5: find which module provides a specific symbol
SYMBOL="jbd2_journal_start"
echo "=== Who provides $SYMBOL? ==="
grep "$SYMBOL" /lib/modules/$(uname -r)/modules.symbols

# Step 6: verify with modinfo
echo "=== modinfo confirms ==="
modinfo -F depends $MODULE

# Step 7: visualize the dependency tree for your storage driver
STORAGE=$(lsmod | grep -oE "nvme|ahci|virtio_blk" | head -1)
echo "=== $STORAGE dependency tree ==="
modprobe -n -v $STORAGE 2>/dev/null
```

**Preguntas:**
1. ¿La cadena de `modprobe -n -v` coincide con lo que muestra `modules.dep` directamente?
2. ¿Tu driver de almacenamiento tiene dependencias? ¿Cuáles?
3. ¿Cuántos módulos se necesitan para cargar `nf_tables` (netfilter)?

> **Pregunta de predicción**: si borras `modules.dep` y `modules.dep.bin` y luego ejecutas `modprobe ext4`, ¿funcionará? ¿Qué error esperas? ¿Cómo lo repararías?

### Ejercicio 3: Regenerar y verificar depmod

```bash
# Step 1: check current modules.dep timestamp
stat /lib/modules/$(uname -r)/modules.dep | grep Modify

# Step 2: do a dry run to see what depmod would generate
sudo depmod -a -n 2>/dev/null | wc -l
echo "Lines that would be generated"

# Step 3: check for errors (unresolved symbols)
echo "=== Checking for errors ==="
sudo depmod -a -e 2>&1
# Should be clean on a well-maintained system

# Step 4: regenerate all metadata
sudo depmod -a -v 2>/dev/null | tail -5

# Step 5: verify timestamp changed
stat /lib/modules/$(uname -r)/modules.dep | grep Modify

# Step 6: verify modprobe still works
modprobe -n -v ext4

# Step 7: check depmod configuration
echo "=== depmod config ==="
cat /etc/depmod.d/*.conf 2>/dev/null || echo "No config files"

# Step 8: simulate adding a fake module
# (This creates a dummy file — NOT a real module)
TMPMOD="/lib/modules/$(uname -r)/extra/test_depmod_lab.ko"
sudo mkdir -p /lib/modules/$(uname -r)/extra/
echo "not a real module" | sudo tee $TMPMOD > /dev/null

# Step 9: run depmod and see if it warns about the fake module
sudo depmod -a -e 2>&1 | grep test_depmod
# Should show an error (invalid ELF format or similar)

# Step 10: clean up
sudo rm -f $TMPMOD
sudo depmod -a
```

**Preguntas:**
1. ¿`depmod -a -e` mostró algún error antes de añadir el archivo falso?
2. ¿Qué error dio depmod con el archivo falso? ¿Lo incluyó en `modules.dep`?
3. ¿Cuánto tiempo tardó `depmod -a` en regenerar todos los archivos?

> **Pregunta de predicción**: si tienes un módulo real compilado para el kernel `6.7.4-200` y lo copias al directorio de módulos del kernel `6.8.0-100`, ¿`depmod -a 6.8.0-100` lo incluirá en `modules.dep`? ¿`modprobe` podrá cargarlo? ¿Qué verificación del kernel lo impedirá?

---

> **Siguiente sección**: S02 — Configuración del Kernel en Runtime, T01 — sysctl.
