# Edición de GRUB en Boot

## Índice
1. [¿Por qué editar GRUB en tiempo de arranque?](#por-qué-editar-grub-en-tiempo-de-arranque)
2. [El menú de GRUB2](#el-menú-de-grub2)
3. [Editar una entrada con 'e'](#editar-una-entrada-con-e)
4. [Recovery mode y rescue targets](#recovery-mode-y-rescue-targets)
5. [init=/bin/bash: acceso de emergencia](#initbinbash-acceso-de-emergencia)
6. [rd.break: romper al initramfs](#rdbreak-romper-al-initramfs)
7. [Resetear la contraseña de root](#resetear-la-contraseña-de-root)
8. [Proteger GRUB con contraseña](#proteger-grub-con-contraseña)
9. [Errores comunes](#errores-comunes)
10. [Cheatsheet](#cheatsheet)
11. [Ejercicios](#ejercicios)

---

## ¿Por qué editar GRUB en tiempo de arranque?

Hay situaciones donde necesitas modificar los parámetros de arranque **antes** de que el kernel cargue, sin acceso al sistema:

| Situación | Solución en GRUB |
|---|---|
| Contraseña de root olvidada | `init=/bin/bash` o `rd.break` |
| Sistema no arranca (driver, LVM, fsck) | Añadir `rd.break`, quitar `quiet` |
| SELinux bloquea todo | Añadir `enforcing=0` |
| GUI rota (driver de vídeo) | Añadir `nomodeset`, `3` |
| Investigar arranque lento | Quitar `quiet rhgb`, añadir `debug` |
| Montar root como read-write | Cambiar `ro` por `rw` |
| Arrancar en modo rescue/emergency | Añadir `systemd.unit=rescue.target` |

Esto convierte a GRUB en una herramienta de **rescate y diagnóstico** fundamental para cualquier sysadmin.

---

## El menú de GRUB2

### Comportamiento por defecto

```
  ┌──────────────────────────────────────────────────────┐
  │              GNU GRUB  version 2.06                   │
  │                                                      │
  │  Fedora Linux (6.7.4-200.fc39.x86_64)          ◄──  │
  │  Fedora Linux (6.6.9-200.fc39.x86_64)               │
  │  Fedora Linux (6.5.12-100.fc39.x86_64)              │
  │  UEFI Firmware Settings                              │
  │                                                      │
  │                                                      │
  │  Use the ↑ and ↓ keys to select which entry          │
  │  is highlighted.                                     │
  │  Press enter to boot the selected OS,                │
  │  'e' to edit the commands before booting,            │
  │  or 'c' for a command-line.                          │
  │                                                      │
  │  The highlighted entry will be executed               │
  │  automatically in 5s.                                │
  └──────────────────────────────────────────────────────┘
```

### Teclas en el menú

| Tecla | Acción |
|---|---|
| `↑` / `↓` | Navegar entre entradas |
| `Enter` | Arrancar la entrada seleccionada |
| `e` | **Editar** la entrada antes de arrancar |
| `c` | Abrir la **línea de comandos** de GRUB |
| `Esc` | Volver al menú (desde editor o CLI) |

### Acceder al menú cuando está oculto

Algunas distribuciones ocultan el menú (`GRUB_TIMEOUT_STYLE=hidden` o `GRUB_TIMEOUT=0`). Para forzar su aparición:

```
  Al encender:
  ┌────────────────────────────────────────────┐
  │                                            │
  │  Mantén presionada la tecla Shift (BIOS)   │
  │  o presiona Esc repetidamente (UEFI)       │
  │  durante el POST                           │
  │                                            │
  │  Fedora/RHEL 8+: no funciona Shift.        │
  │  Alternativa: edita /etc/default/grub      │
  │  y cambia GRUB_TIMEOUT_STYLE=menu          │
  │  o GRUB_TIMEOUT=5                          │
  └────────────────────────────────────────────┘
```

> **Fedora 33+/RHEL 8+** con BLS: el menú aparece automáticamente si el arranque anterior falló (`boot_success=0` en grubenv). Si el último arranque fue exitoso, el menú se oculta.

---

## Editar una entrada con 'e'

Al presionar `e` sobre una entrada, GRUB muestra los comandos que ejecutará. Puedes modificarlos **solo para este arranque** (los cambios no se guardan en disco).

### Pantalla de edición

```
  ┌──────────────────────────────────────────────────────────────┐
  │                                                              │
  │  load_video                                                  │
  │  set gfxpayload=keep                                         │
  │  insmod gzio                                                 │
  │  insmod part_gpt                                             │
  │  insmod ext2                                                 │
  │  set root='hd0,gpt2'                                        │
  │  search --no-floppy --fs-uuid --set=root abc123-...          │
  │                                                              │
  │  linux  /vmlinuz-6.7.4-200.fc39.x86_64                      │
  │         root=UUID=def456-... ro                              │
  │         rd.lvm.lv=fedora/root rd.lvm.lv=fedora/swap          │
  │         rhgb quiet                            ◄── edita aquí │
  │                                                              │
  │  initrd /initramfs-6.7.4-200.fc39.x86_64.img                │
  │                                                              │
  │                                                              │
  │  Ctrl-x or F10: boot    Ctrl-c: command line    Esc: back    │
  └──────────────────────────────────────────────────────────────┘
```

### Teclas en el editor

| Tecla | Acción |
|---|---|
| `Ctrl+X` o `F10` | Arrancar con los cambios |
| `Ctrl+C` | Ir a línea de comandos GRUB |
| `Esc` | Descartar cambios, volver al menú |
| `Ctrl+E` | Ir al final de la línea |
| `Ctrl+A` | Ir al inicio de la línea |
| `Ctrl+K` | Borrar desde cursor hasta fin de línea |

### Qué modificar

La línea que casi siempre editas es la que empieza con `linux` (o `linuxefi` en algunos sistemas):

```
linux /vmlinuz-6.7.4-200.fc39.x86_64 root=UUID=... ro rhgb quiet
                                                     │  │    │
                                           ┌─────────┘  │    │
                                           │    ┌───────┘    │
                                           ▼    ▼            ▼
                                     Puedes cambiar, añadir
                                     o eliminar estos parámetros
```

Modificaciones típicas:

```bash
# Remove quiet/rhgb for verbose boot
# Before: ... ro rhgb quiet
# After:  ... ro

# Boot to multi-user (no GUI)
# After:  ... ro 3

# Boot to rescue mode
# After:  ... ro systemd.unit=rescue.target

# Boot to emergency mode
# After:  ... ro systemd.unit=emergency.target

# Disable SELinux for this boot
# After:  ... ro enforcing=0

# Skip video driver (black screen fix)
# After:  ... ro nomodeset

# Get a root shell directly
# After:  ... ro init=/bin/bash

# Break into initramfs
# After:  ... ro rd.break
```

---

## Recovery mode y rescue targets

### Modos de arranque de menor a mayor restricción

```
  ┌──────────────────────────────────────────────────────────────┐
  │                                                              │
  │  NORMAL (multi-user.target o graphical.target)               │
  │  ├── Todos los servicios activos                             │
  │  ├── Red, cron, logging, login                               │
  │  └── Uso normal del sistema                                  │
  │                                                              │
  │  MULTI-USER SIN GUI (multi-user.target / runlevel 3)         │
  │  ├── Todos los servicios excepto display manager              │
  │  ├── Red activa, login en consola                            │
  │  └── Param: "3" o "systemd.unit=multi-user.target"          │
  │                                                              │
  │  RESCUE (rescue.target / single-user / runlevel 1)          │
  │  ├── Solo filesystems locales montados                       │
  │  ├── Sin red, sin la mayoría de servicios                    │
  │  ├── Pide contraseña de root                                 │
  │  └── Param: "single", "1", "systemd.unit=rescue.target"     │
  │                                                              │
  │  EMERGENCY (emergency.target)                                │
  │  ├── Root filesystem montado read-only                       │
  │  ├── Sin otros filesystems, sin servicios                    │
  │  ├── Pide contraseña de root                                 │
  │  └── Param: "systemd.unit=emergency.target"                 │
  │                                                              │
  │  INITRAMFS SHELL (rd.break)                                  │
  │  ├── Antes de montar el root real                            │
  │  ├── Filesystem real en /sysroot (no montado)                │
  │  ├── NO pide contraseña                                      │
  │  └── Param: "rd.break"                                      │
  │                                                              │
  │  INIT OVERRIDE (init=/bin/bash)                              │
  │  ├── Reemplaza init/systemd por una shell                    │
  │  ├── Root montado read-only, sin servicios                   │
  │  ├── NO pide contraseña                                      │
  │  └── Param: "init=/bin/bash"                                │
  │                                                              │
  └──────────────────────────────────────────────────────────────┘

  Más servicios ──────────────────────────────► Menos servicios
  Normal → Multi-user → Rescue → Emergency → rd.break → init=bash
```

### rescue.target vs emergency.target

| Aspecto | rescue.target | emergency.target |
|---|---|---|
| **Filesystems** | Montados (rw) | Solo / (ro) |
| **Red** | No | No |
| **Servicios** | Mínimos (sysinit) | Ninguno |
| **Pide password root** | Sí | Sí |
| **Cuándo usar** | fsck, reparar fstab, servicios | Fstab roto, / corrupto |
| **Parámetro** | `systemd.unit=rescue.target` | `systemd.unit=emergency.target` |

```bash
# In the GRUB editor, append to the linux line:

# Rescue mode (most common for repairs)
systemd.unit=rescue.target

# Emergency mode (when rescue fails due to broken fstab/mounts)
systemd.unit=emergency.target
```

---

## init=/bin/bash: acceso de emergencia

Este parámetro le dice al kernel que en lugar de ejecutar `/sbin/init` (systemd), ejecute `/bin/bash` como PID 1. El resultado es una shell de root **sin autenticación**.

### Procedimiento

```
  En el menú GRUB:
  1. Presiona 'e' sobre la entrada deseada
  2. Busca la línea "linux ..."
  3. Reemplaza "ro" por "rw" (para poder escribir)
  4. Añade al final: init=/bin/bash
  5. Ctrl+X para arrancar

  ┌──────────────────────────────────────────────────────────┐
  │ linux /vmlinuz-6.7.4 root=UUID=... rw init=/bin/bash    │
  │                                     ──  ──────────────  │
  │                                     │   └─ shell en     │
  │                                     │      vez de init  │
  │                                     └─ read-write       │
  └──────────────────────────────────────────────────────────┘
```

### Qué obtienes

```bash
bash-5.2#
# You are root, PID 1 is bash
# No systemd, no services, no network, no logging
# If you used "rw": root filesystem is writable
# If you kept "ro": root filesystem is read-only

# If read-only, remount as read-write:
mount -o remount,rw /

# Now you can make changes (reset password, fix config, etc.)
passwd root

# IMPORTANT: when done, you CANNOT use "reboot" (no systemd)
# Force sync and reboot:
sync
exec /sbin/init          # Start systemd normally
# Or hard reboot (last resort):
echo b > /proc/sysrq-trigger
```

### Limitaciones

- No hay systemd → no `systemctl`, no `journalctl`, no `reboot`
- No hay red
- No hay logging (cambios no se registran en audit)
- Si SELinux está en enforcing, los cambios pueden causar problemas de contexto (ver "Resetear la contraseña de root" para la solución)
- Si GRUB tiene contraseña, no puedes editar la entrada

---

## rd.break: romper al initramfs

`rd.break` detiene el proceso de arranque **dentro del initramfs**, justo antes de montar el root filesystem real y hacer el pivot. Es el método preferido en RHEL/Fedora para resetear contraseñas.

### Procedimiento

```
  En el menú GRUB:
  1. Presiona 'e' sobre la entrada deseada
  2. Busca la línea "linux ..."
  3. Añade al final: rd.break
  4. (Opcional) quita "quiet rhgb" para ver el proceso
  5. Ctrl+X para arrancar
```

### Qué obtienes

```bash
# You land in the initramfs environment:
switch_root:/#

# The REAL root filesystem is mounted read-only at /sysroot
mount | grep sysroot
# /dev/mapper/fedora-root on /sysroot type xfs (ro,...)

# To make changes to the real system:
# 1. Remount /sysroot as read-write
mount -o remount,rw /sysroot

# 2. Chroot into the real system
chroot /sysroot

# Now you're inside your real root filesystem
# You can change passwords, edit configs, etc.
passwd root

# 3. If SELinux is enabled, create autorelabel marker
touch /.autorelabel    # CRITICAL on RHEL/Fedora with SELinux

# 4. Exit chroot
exit

# 5. Exit initramfs (continues boot or reboots)
exit
# Or:
reboot -f
```

### rd.break vs init=/bin/bash

```
  ┌─────────────────────────────────────────────────────────┐
  │              FLUJO DE ARRANQUE                           │
  │                                                         │
  │  Firmware → GRUB → kernel → initramfs ──┐               │
  │                                          │               │
  │                              rd.break ◄──┘               │
  │                              (para aquí)                 │
  │                              /sysroot existe             │
  │                              pero no pivotado            │
  │                                   │                      │
  │                                   ▼                      │
  │                              pivot_root                  │
  │                              (/ = real root)             │
  │                                   │                      │
  │                                   ▼                      │
  │                              exec /sbin/init ──┐         │
  │                                                 │        │
  │                              init=/bin/bash ◄───┘        │
  │                              (reemplaza init)            │
  │                              / = real root               │
  │                              montado read-only           │
  └─────────────────────────────────────────────────────────┘
```

| Aspecto | rd.break | init=/bin/bash |
|---|---|---|
| **Dónde para** | Dentro del initramfs | Después del pivot, antes de init |
| **Root filesystem** | En `/sysroot` (ro) | En `/` (ro) |
| **Necesita chroot** | Sí (`chroot /sysroot`) | No |
| **Método preferido** | RHEL/Fedora (examen RHCSA) | Universal, más directo |
| **systemd disponible** | No | No |
| **Network** | No | No |

---

## Resetear la contraseña de root

Este es uno de los procedimientos más comunes para un sysadmin. Hay dos métodos según la distribución.

### Método 1: rd.break (RHEL/Fedora — método RHCSA)

```bash
# 1. In GRUB menu, press 'e'
# 2. On the "linux" line, add: rd.break
# 3. Remove "quiet rhgb" (optional, for visibility)
# 4. Ctrl+X to boot

# 5. In the initramfs shell:
mount -o remount,rw /sysroot
chroot /sysroot
passwd root
# Enter new password twice

# 6. Handle SELinux relabeling
touch /.autorelabel

# 7. Exit and reboot
exit       # Exit chroot
exit       # Exit initramfs → system reboots

# 8. Wait for SELinux relabel (can take minutes on large systems)
# System will reboot again after relabeling
```

### Método 2: init=/bin/bash (Debian/Ubuntu y universal)

```bash
# 1. In GRUB menu, press 'e'
# 2. On the "linux" line:
#    - Change "ro" to "rw"
#    - Add: init=/bin/bash
# 3. Ctrl+X to boot

# 4. In the bash shell:
passwd root
# Enter new password twice

# 5. If SELinux is enabled (rare on Debian):
touch /.autorelabel

# 6. Sync and reboot
sync
exec /sbin/init
# Or if that fails:
echo b > /proc/sysrq-trigger
```

### ¿Por qué `touch /.autorelabel`?

```
  ┌────────────────────────────────────────────────────────────┐
  │                 PROBLEMA CON SELINUX                        │
  │                                                            │
  │  Cuando cambias la contraseña desde rd.break o init=bash:  │
  │                                                            │
  │  /etc/shadow se modifica FUERA del contexto de SELinux     │
  │  → El archivo queda con contexto incorrecto                │
  │  → SELinux en enforcing BLOQUEA la lectura de shadow       │
  │  → Nadie puede hacer login (ni siquiera root)              │
  │                                                            │
  │  SOLUCIÓN: touch /.autorelabel                             │
  │  → Al arrancar, SELinux re-etiqueta TODOS los archivos     │
  │  → shadow recupera su contexto shadow_t                    │
  │  → Login funciona correctamente                            │
  │                                                            │
  │  Alternativa más rápida (solo el archivo afectado):        │
  │  restorecon /etc/shadow                                    │
  │  (pero requiere que restorecon esté disponible)            │
  └────────────────────────────────────────────────────────────┘
```

### Procedimiento completo con diagrama

```
  ┌──────────┐     ┌──────────────────────┐
  │ Encender │────▶│ Menú GRUB            │
  └──────────┘     │ Presionar 'e'        │
                   └──────────┬───────────┘
                              │
                              ▼
                   ┌──────────────────────┐
                   │ Editar línea linux:  │
                   │ añadir rd.break      │
                   │ Ctrl+X               │
                   └──────────┬───────────┘
                              │
                              ▼
                   ┌──────────────────────┐
                   │ switch_root:/#       │
                   │ mount -o remount,rw  │
                   │   /sysroot           │
                   │ chroot /sysroot      │
                   └──────────┬───────────┘
                              │
                              ▼
                   ┌──────────────────────┐
                   │ passwd root          │
                   │ touch /.autorelabel  │
                   └──────────┬───────────┘
                              │
                              ▼
                   ┌──────────────────────┐
                   │ exit (chroot)        │
                   │ exit (initramfs)     │
                   │ → reboot             │
                   └──────────┬───────────┘
                              │
                              ▼
                   ┌──────────────────────┐
                   │ SELinux relabel      │
                   │ (automático, lento)  │
                   │ → reboot final       │
                   └──────────┬───────────┘
                              │
                              ▼
                   ┌──────────────────────┐
                   │ Login con nueva      │
                   │ contraseña ✓         │
                   └──────────────────────┘
```

---

## Proteger GRUB con contraseña

Si alguien tiene acceso físico al servidor y puede editar GRUB, puede obtener acceso root sin contraseña (como acabamos de ver). La solución es **proteger GRUB con contraseña**.

### Niveles de protección

```
  ┌────────────────────────────────────────────────────────────┐
  │           NIVELES DE PROTECCIÓN GRUB                       │
  │                                                            │
  │  Nivel 0: Sin protección                                   │
  │  → Cualquiera puede editar entradas y arrancar             │
  │                                                            │
  │  Nivel 1: Proteger solo la edición ('e' y 'c')            │
  │  → Arrancar entradas normales: libre                       │
  │  → Editar entradas / línea de comandos: pide password      │
  │  → Recomendado para la mayoría de escenarios               │
  │                                                            │
  │  Nivel 2: Proteger todo                                    │
  │  → Arrancar cualquier entrada: pide password               │
  │  → Máxima seguridad pero impide arranque desatendido       │
  │                                                            │
  └────────────────────────────────────────────────────────────┘
```

### Configurar contraseña (Nivel 1: proteger edición)

#### Paso 1: Generar el hash de la contraseña

```bash
# Generate a PBKDF2 password hash
grub2-mkpasswd-pbkdf2
# Enter password:
# Reenter password:
# PBKDF2 hash of your password is grub.pbkdf2.sha512.10000.ABC123...DEF456

# Copy the ENTIRE hash starting from "grub.pbkdf2.sha512..."
```

#### Paso 2: Crear el archivo de configuración

```bash
# Edit (or create) /etc/grub.d/01_users
sudo cat > /etc/grub.d/01_users << 'GRUBEOF'
#!/bin/sh
cat << 'EOF'
set superusers="grubadmin"
password_pbkdf2 grubadmin grub.pbkdf2.sha512.10000.ABC123...DEF456
EOF
GRUBEOF

sudo chmod 755 /etc/grub.d/01_users
```

#### Paso 3: Marcar entradas como accesibles sin password

Por defecto, con `superusers` definido, **todas** las entradas requieren password para arrancar. Para permitir arranque libre pero proteger la edición, agrega `--unrestricted` a las entradas:

```bash
# Option A: Modify 10_linux to add --unrestricted
# (Not recommended — survives updates poorly)

# Option B: Use GRUB_CMDLINE configuration (RHEL/Fedora BLS)
# BLS entries already have grub_arg --unrestricted by default

# Option C: For custom entries in 40_custom:
menuentry 'Fedora Linux' --unrestricted {
    # ... normal boot commands
}
```

En **RHEL/Fedora con BLS**, las entradas en `/boot/loader/entries/` contienen `grub_arg --unrestricted` por defecto, así que el arranque normal funciona sin password pero editar con `e` o acceder a `c` sí lo requiere.

#### Paso 4: Regenerar grub.cfg

```bash
sudo grub2-mkconfig -o /boot/grub2/grub.cfg
```

#### Verificar

Al reiniciar:
- Seleccionar una entrada y presionar Enter → arranca normalmente
- Presionar `e` → pide usuario y contraseña
- Presionar `c` → pide usuario y contraseña

### Configurar contraseña (Nivel 2: proteger todo)

Mismo procedimiento que Nivel 1, pero **sin** `--unrestricted` en las entradas. Cada arranque pide credenciales.

### Usuarios con permisos limitados

Puedes crear usuarios que solo pueden arrancar entradas específicas pero no editar:

```bash
# In /etc/grub.d/01_users:
set superusers="grubadmin"
password_pbkdf2 grubadmin grub.pbkdf2.sha512.10000.HASH_ADMIN
password_pbkdf2 operator grub.pbkdf2.sha512.10000.HASH_OPERATOR

# In /etc/grub.d/40_custom:
# Only grubadmin and operator can boot this entry:
menuentry 'Production Server' --users operator {
    linux /vmlinuz-... root=UUID=... ro
    initrd /initramfs-...
}
```

- `superusers` puede hacer todo (editar, arrancar todo)
- `--users operator` permite a "operator" arrancar esa entrada pero no editarla
- Usuarios no listados no pueden arrancar esa entrada

### Contraseña en texto plano (no recomendado)

Para entornos de prueba donde el hash PBKDF2 no es necesario:

```bash
# In /etc/grub.d/01_users (INSECURE — testing only):
set superusers="admin"
password admin my_plain_password    # Visible to anyone who reads grub.cfg!
```

> **Nunca uses contraseñas en texto plano en producción.** El hash PBKDF2 protege la contraseña incluso si alguien lee `grub.cfg`.

---

## Errores comunes

### 1. Olvidar `touch /.autorelabel` después de cambiar la contraseña

En sistemas con SELinux en enforcing, el archivo `/etc/shadow` queda con un contexto SELinux incorrecto. El sistema arranca pero **nadie puede hacer login**. La solución es repetir el procedimiento rd.break y ejecutar `touch /.autorelabel` (o `restorecon /etc/shadow`).

### 2. Usar rd.break con `ro` y olvidar remontar /sysroot

```bash
# WRONG: /sysroot is read-only, passwd fails
chroot /sysroot
passwd root
# passwd: Authentication token manipulation error

# RIGHT: remount first
mount -o remount,rw /sysroot
chroot /sysroot
passwd root
# Success
```

### 3. Esperar que `reboot` funcione con init=/bin/bash

Con `init=/bin/bash`, PID 1 es bash, no systemd. El comando `reboot` necesita systemd:

```bash
# This won't work:
reboot
# bash: reboot: command not found (or just hangs)

# Use instead:
sync                              # Flush writes to disk
exec /sbin/init                   # Replace bash with systemd
# Or force hardware reboot:
echo b > /proc/sysrq-trigger     # Last resort
```

### 4. Configurar password de GRUB sin --unrestricted y bloquear el arranque

Si defines `superusers` pero no marcas las entradas como `--unrestricted`, necesitarás la contraseña para **cualquier** arranque. Si olvidas la contraseña de GRUB, necesitas un live USB para editar los archivos de configuración.

### 5. No saber que GRUB password no protege contra live USB

La contraseña de GRUB protege contra la edición del menú, pero si el atacante puede arrancar desde un USB live o cambiar el orden de arranque en el firmware, GRUB queda completamente bypaseado. La protección completa requiere:

```
  Protección completa de acceso físico:
  ┌─────────────────────────────────────┐
  │ 1. Password en firmware (BIOS/UEFI) │ ← Primer nivel
  │ 2. Secure Boot habilitado           │ ← Verificar bootloader
  │ 3. Password en GRUB                 │ ← Impedir edición
  │ 4. LUKS (cifrado de disco)          │ ← Proteger datos en reposo
  └─────────────────────────────────────┘
  Sin LUKS, incluso con todo lo anterior,
  alguien puede extraer el disco y leerlo
  en otra máquina.
```

---

## Cheatsheet

```bash
# ═══════════════════════════════════════════════
#  ACCEDER AL MENÚ GRUB
# ═══════════════════════════════════════════════
# BIOS: mantener Shift durante POST
# UEFI: presionar Esc repetidamente durante POST
# Fedora/RHEL: falla de arranque previo → menú automático

# ═══════════════════════════════════════════════
#  EDICIÓN TEMPORAL (en menú GRUB)
# ═══════════════════════════════════════════════
# 'e'        → Editar entrada
# 'c'        → Línea de comandos GRUB
# Ctrl+X     → Arrancar con cambios
# Esc        → Descartar, volver al menú

# ═══════════════════════════════════════════════
#  PARÁMETROS DE RESCATE (añadir a línea linux)
# ═══════════════════════════════════════════════
# systemd.unit=rescue.target    → Rescue mode (pide password)
# systemd.unit=emergency.target → Emergency mode (pide password)
# rd.break                      → Shell en initramfs (sin password)
# init=/bin/bash                 → Shell como PID 1 (sin password)
# single / 1                    → Single-user mode
# 3                              → Multi-user sin GUI
# enforcing=0                   → SELinux permissive (este arranque)
# nomodeset                     → Sin driver de vídeo del kernel

# ═══════════════════════════════════════════════
#  RESETEAR PASSWORD ROOT (rd.break)
# ═══════════════════════════════════════════════
# En GRUB: 'e' → añadir rd.break → Ctrl+X
mount -o remount,rw /sysroot
chroot /sysroot
passwd root
touch /.autorelabel              # CRITICAL for SELinux
exit                             # Exit chroot
exit                             # Exit initramfs → reboot

# ═══════════════════════════════════════════════
#  RESETEAR PASSWORD ROOT (init=/bin/bash)
# ═══════════════════════════════════════════════
# En GRUB: 'e' → cambiar ro→rw, añadir init=/bin/bash → Ctrl+X
passwd root
touch /.autorelabel              # If SELinux enabled
sync
exec /sbin/init                  # Or: echo b > /proc/sysrq-trigger

# ═══════════════════════════════════════════════
#  PROTEGER GRUB CON PASSWORD
# ═══════════════════════════════════════════════
grub2-mkpasswd-pbkdf2            # Generate hash
sudo vim /etc/grub.d/01_users    # Configure users + hash
sudo grub2-mkconfig -o /boot/grub2/grub.cfg   # Regenerate
```

---

## Ejercicios

### Ejercicio 1: Explorar los modos de arranque

> **NOTA**: estos ejercicios modifican parámetros de arranque. Realízalos en una VM donde puedas tomar snapshots.

```bash
# PREPARATION: take a VM snapshot before starting

# Part A: Verbose boot
# 1. Reboot the VM
# 2. In GRUB menu, press 'e'
# 3. Find the "linux" line
# 4. Remove "quiet" and "rhgb" (or "splash")
# 5. Ctrl+X to boot
# 6. Observe all kernel messages during boot

# Part B: Multi-user without GUI
# 1. Reboot again
# 2. Press 'e' in GRUB
# 3. Add "3" at the end of the "linux" line
# 4. Ctrl+X to boot
# 5. You should get a text login prompt

# Part C: Rescue mode
# 1. Reboot
# 2. Press 'e' in GRUB
# 3. Add "systemd.unit=rescue.target" to the "linux" line
# 4. Ctrl+X
# 5. Enter root password when prompted
# 6. Check what's available:
systemctl list-units --type=service
ip addr show
mount
# 7. Exit rescue mode:
systemctl default
# Or: reboot

# Part D: Emergency mode
# 1. Reboot
# 2. Press 'e', add "systemd.unit=emergency.target"
# 3. Ctrl+X, enter root password
# 4. Check:
mount                        # Root should be read-only
systemctl list-units         # Very few units
# 5. Reboot
```

**Preguntas:**
1. ¿Cuántos servicios están activos en rescue mode vs emergency mode?
2. En emergency mode, ¿puedes editar archivos? ¿Qué necesitas hacer primero?
3. ¿Qué diferencia notas entre quitar `quiet` y mantenerlo?

> **Pregunta de predicción**: si estás en emergency mode y ejecutas `systemctl default`, ¿el sistema arrancará normalmente al target gráfico? ¿O se quedará en el target actual?

### Ejercicio 2: Resetear la contraseña de root

```bash
# PREPARATION: Set a known root password first
sudo passwd root

# Now pretend you forgot it

# Method: rd.break (RHCSA method)
# 1. Reboot the VM
# 2. In GRUB, press 'e'
# 3. On the "linux" line, add: rd.break
# 4. Optionally remove "quiet rhgb"
# 5. Ctrl+X to boot

# 6. When you get the switch_root:/# prompt:
mount -o remount,rw /sysroot
chroot /sysroot

# 7. Change the password
passwd root
# Type new password twice

# 8. SELinux relabel
touch /.autorelabel

# 9. Exit
exit
exit

# 10. Wait for SELinux relabel (watch the screen)
# System will reboot automatically after relabeling

# 11. Login with the new password

# VERIFICATION:
# 12. Check that SELinux contexts are correct:
ls -Z /etc/shadow
# system_u:object_r:shadow_t:s0 /etc/shadow
```

**Preguntas:**
1. ¿Cuánto tardó el relabel de SELinux? ¿De qué depende?
2. ¿Qué hubiera pasado si no hubieras ejecutado `touch /.autorelabel`?
3. Si la máquina tuviera LUKS (cifrado de disco), ¿podrías usar rd.break sin la passphrase de LUKS?

> **Pregunta de predicción**: si en vez de `rd.break` usas `init=/bin/bash` pero olvidas cambiar `ro` por `rw`, ¿qué error obtendrás al ejecutar `passwd`? ¿Cómo lo solucionarías sin reiniciar?

### Ejercicio 3: Proteger GRUB con contraseña

```bash
# Step 1: Generate password hash
grub2-mkpasswd-pbkdf2
# Enter a test password, copy the hash

# Step 2: Create the users file
sudo tee /etc/grub.d/01_users << 'EOF'
#!/bin/sh
cat << 'GRUB'
set superusers="grubadmin"
password_pbkdf2 grubadmin grub.pbkdf2.sha512.10000.YOUR_HASH_HERE
GRUB
EOF

sudo chmod 755 /etc/grub.d/01_users

# Step 3: Regenerate GRUB config
sudo grub2-mkconfig -o /boot/grub2/grub.cfg

# Step 4: Verify the password section exists in grub.cfg
grep -A2 "superusers" /boot/grub2/grub.cfg

# Step 5: Reboot and test
# - Select an entry → should boot normally (--unrestricted)
# - Press 'e' → should prompt for username/password
# - Press 'c' → should prompt for username/password

# Step 6: Test login
# Username: grubadmin
# Password: what you entered in Step 1

# Step 7: CLEANUP — remove password protection (for lab VMs)
sudo rm /etc/grub.d/01_users
sudo grub2-mkconfig -o /boot/grub2/grub.cfg
```

**Preguntas:**
1. ¿La contraseña de GRUB se almacena en texto plano dentro de `grub.cfg` o como hash?
2. Si no hubieras creado el archivo `01_users` sino que hubieras editado `40_custom`, ¿funcionaría igual?
3. ¿Qué pasa si eliminas `01_users` pero no regeneras `grub.cfg`?

> **Pregunta de predicción**: un atacante con acceso físico al servidor ve que GRUB pide contraseña al presionar `e`. ¿Puede arrancar desde un USB live para acceder al disco? ¿Qué medidas adicionales se necesitan para impedirlo?

---

> **Siguiente tema**: T04 — Proceso de arranque completo: firmware → bootloader → kernel → initramfs → init → target.
