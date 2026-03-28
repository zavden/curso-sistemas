# T03 — Target de arranque

## Cómo systemd decide qué arrancar

Al iniciar, systemd sigue esta cadena de decisiones:

```
1. ¿Hay systemd.unit= en el kernel command line?
   Sí → usar ese target
   No ↓

2. ¿Existe /etc/systemd/system/default.target?
   Sí → usar ese symlink
   No ↓

3. ¿Existe /usr/lib/systemd/system/default.target?
   Sí → usar ese symlink
   No → arrancar en emergency mode
```

```bash
# Ver qué target se usó en este boot:
systemctl get-default
# multi-user.target

# Ver el kernel command line actual:
cat /proc/cmdline
# BOOT_IMAGE=/vmlinuz-6.x root=/dev/sda1 ro quiet
# Si incluyera systemd.unit=rescue.target, se habría arrancado en rescue
```

## El proceso de boot paso a paso

### Fase 1: Hardware → systemd

```
BIOS/UEFI → GRUB → kernel → initramfs → systemd (PID 1)
```

```bash
# El kernel ejecuta /sbin/init, que es un symlink a systemd:
ls -la /sbin/init
# /sbin/init → /usr/lib/systemd/systemd

# systemd lee:
# 1. /proc/cmdline (parámetros del kernel)
# 2. /etc/systemd/system.conf (configuración del manager)
# 3. default.target (qué arrancar)
```

### Fase 2: Resolución de dependencias

systemd construye un **transaction** — el grafo de unidades a arrancar:

```bash
# 1. Lee default.target (ej: multi-user.target)
# 2. Resuelve Wants= y Requires= recursivamente
# 3. Ordena según After= y Before=
# 4. Arranca todo en PARALELO respetando las dependencias de orden

# Ver la cadena de dependencias:
systemctl list-dependencies default.target --no-pager
```

### Fase 3: Arranque paralelo

```
                    default.target
                         │
              ┌──────────┼──────────┐
              │          │          │
         local-fs    network    sockets
          .target     .target   .target
           │  │         │         │
        ┌──┘  └──┐   ┌──┘      ┌──┘
        │        │   │         │
     home.    var.  NM.     sshd.    ← arrancan en paralelo
     mount   mount service  socket     si no hay After/Before entre ellos
```

Las unidades sin dependencias de orden entre sí arrancan **simultáneamente**.
Esto es lo que hace que systemd sea significativamente más rápido que SysVinit.

## Analizar el boot

### systemd-analyze — Tiempo total

```bash
systemd-analyze
# Startup finished in 2.5s (kernel) + 5.1s (userspace) = 7.6s
# graphical.target reached after 5.0s in userspace

# Desglose:
# kernel: tiempo desde que el kernel empieza hasta que ejecuta systemd
# userspace: tiempo desde systemd hasta que el default target está activo
```

### systemd-analyze blame — Qué tardó más

```bash
systemd-analyze blame --no-pager
# 5.100s cloud-init.service
# 3.200s NetworkManager-wait-online.service
# 2.100s docker.service
# 1.500s apt-daily.service
# 0.800s nginx.service
# ...

# Son tiempos wall-clock, no CPU
# NetworkManager-wait-online suele ser de los más lentos
# (espera a que la red esté realmente disponible)
```

### systemd-analyze critical-chain — El camino más largo

```bash
# La cadena de dependencias que determina el tiempo total:
systemd-analyze critical-chain --no-pager
# graphical.target @5.0s
# └─display-manager.service @4.2s +800ms
#   └─multi-user.target @4.1s
#     └─nginx.service @3.5s +600ms
#       └─network-online.target @3.4s
#         └─NetworkManager-wait-online.service @1.2s +2.2s
#           └─NetworkManager.service @1.0s +200ms
#             └─basic.target @0.9s

# Cada línea: unidad @cuándo_arrancó +cuánto_tardó
# La indentación muestra qué dependencia la bloqueó

# Para un target específico:
systemd-analyze critical-chain multi-user.target --no-pager
```

### systemd-analyze plot — Gráfico SVG

```bash
# Genera un gráfico SVG con líneas de tiempo de cada unidad:
systemd-analyze plot > /tmp/boot.svg

# Abrir con un navegador:
# Cada unidad es una barra horizontal
# - Rojo: activando
# - Verde: activo
# - Barras que se solapan = arranque paralelo
# - La barra más a la derecha define el tiempo total
```

### systemd-analyze verify — Verificar unidades

```bash
# Verificar errores en una unidad:
systemd-analyze verify /etc/systemd/system/myapp.service
# Reporta errores de sintaxis, dependencias circulares, etc.
```

## Optimizar el boot

### Identificar cuellos de botella

```bash
# 1. Ver qué tarda más:
systemd-analyze blame | head -10

# 2. Ver la cadena crítica:
systemd-analyze critical-chain

# Causas comunes de boot lento:
# - NetworkManager-wait-online.service (espera la red)
# - cloud-init.service (innecesario en bare metal)
# - apt-daily.service / apt-daily-upgrade.service
# - docker.service (inicialización de contenedores)
# - plymouth-quit-wait.service (splash screen)
```

### Soluciones comunes

```bash
# Deshabilitar servicios innecesarios:
sudo systemctl disable NetworkManager-wait-online.service
# Solo si los servicios no necesitan network-online.target

sudo systemctl disable apt-daily.timer apt-daily-upgrade.timer
# Solo si se gestionan actualizaciones manualmente

sudo systemctl mask plymouth-quit-wait.service
# Deshabilitar splash screen

# Cambiar de graphical a multi-user en servidores:
sudo systemctl set-default multi-user.target
# Elimina X11/Wayland/display-manager del boot

# Socket activation (arrancar servicios on-demand):
sudo systemctl disable sshd.service
sudo systemctl enable sshd.socket
# sshd solo arranca cuando alguien se conecta al puerto 22
```

### Medir el impacto

```bash
# Antes de optimizar:
systemd-analyze
# Startup finished in 2.5s (kernel) + 8.1s (userspace) = 10.6s

# Después de optimizar:
systemd-analyze
# Startup finished in 2.5s (kernel) + 4.2s (userspace) = 6.7s
```

## Kernel command line y systemd

```bash
# Parámetros del kernel que afectan a systemd:
cat /proc/cmdline

# systemd.unit=TARGET
# Arrancar en un target específico (override temporal)
# systemd.unit=rescue.target
# systemd.unit=emergency.target

# systemd.mask=UNIT
# Enmascarar una unidad para este boot
# systemd.mask=NetworkManager.service

# systemd.wants=UNIT
# Agregar una dependencia para este boot
# systemd.wants=debug-shell.service

# systemd.log_level=debug
# Más verbosidad durante el boot

# systemd.log_target=console
# Enviar logs a la consola

# rd.break
# Romper al initramfs (antes de montar root)
# Para resetear contraseña de root

# init=/bin/bash
# Bypass completo de systemd — shell directo del kernel
# CUIDADO: sin systemd, casi nada funciona
```

## debug-shell.service

```bash
# Proporciona un shell root en tty9 durante el boot
# Útil cuando el sistema no arranca correctamente

# Habilitar:
sudo systemctl enable debug-shell.service
# O desde GRUB: systemd.wants=debug-shell.service

# Durante el boot, cambiar a tty9:
# Ctrl+Alt+F9 → shell root SIN autenticación

# RIESGO DE SEGURIDAD: cualquiera con acceso físico tiene root
# Deshabilitar después de resolver el problema:
sudo systemctl disable debug-shell.service
```

## Generar diagrama del boot

```bash
# Diagrama DOT de dependencias:
systemd-analyze dot default.target 2>/dev/null | head -20
# Genera formato DOT (Graphviz)

# Convertir a imagen (si graphviz está instalado):
systemd-analyze dot default.target 2>/dev/null | dot -Tpng -o /tmp/boot-deps.png

# Combinado con el plot:
systemd-analyze plot > /tmp/boot-timeline.svg
# El plot muestra TIEMPOS, el dot muestra DEPENDENCIAS
```

---

## Ejercicios

### Ejercicio 1 — Analizar tu boot

```bash
# 1. ¿Cuánto tardó el boot?
systemd-analyze

# 2. ¿Cuáles son los 5 servicios más lentos?
systemd-analyze blame --no-pager | head -5

# 3. ¿Cuál es la cadena crítica?
systemd-analyze critical-chain --no-pager
```

### Ejercicio 2 — Kernel command line

```bash
# ¿Qué parámetros tiene tu kernel?
cat /proc/cmdline

# ¿Hay algún parámetro de systemd?
grep -oE 'systemd\.[a-z_]+=[^ ]+' /proc/cmdline || echo "Ninguno"
```

### Ejercicio 3 — Trazar dependencias

```bash
# ¿Cuántas unidades dependen del default target?
systemctl list-dependencies "$(systemctl get-default)" --plain --no-pager | wc -l

# ¿Qué es lo primero que arranca en la cadena crítica?
systemd-analyze critical-chain --no-pager | tail -3
```
