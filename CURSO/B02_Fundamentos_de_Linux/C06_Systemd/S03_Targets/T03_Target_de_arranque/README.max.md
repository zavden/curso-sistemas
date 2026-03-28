# T03 — Target de arranque

> **Objetivo:** Entender cómo systemd elige el target de arranque, qué sucede durante cada fase del boot, y cómo diagnosticar y optimizar el tiempo de inicio del sistema.

## Cómo systemd decide qué arrancar

 systemd sigue una **jerarquía de decisión** en cadena. Cada paso tiene un "fallback" — si uno falla, pasa al siguiente:

```
                    ┌──────────────────────────────────┐
                    │   1. Kernel Command Line         │
                    │   systemd.unit=...?              │
                    │   (parámetro del kernel)          │
                    └──────────────┬───────────────────┘
                                   │ si existe
                    ┌──────────────▼───────────────────┐
                    │   2. /etc/systemd/system/        │
                    │   default.target (symlink)        │
                    └──────────────┬───────────────────┘
                                   │ si existe
                    ┌──────────────▼───────────────────┐
                    │   3. /usr/lib/systemd/system/    │
                    │   default.target (symlink)       │
                    └──────────────┬───────────────────┘
                                   │ si NO existe
                    ┌──────────────▼───────────────────┐
                    │   4. EMERGENCY MODE              │
                    │   (shell mínimo, sin servicios)  │
                    └──────────────────────────────────┘
```

```bash
# Ver qué target se usa actualmente:
systemctl get-default
# multi-user.target

# Ver todos los targets disponibles:
systemctl list-units --type=target --all --no-pager

# Ver el kernel command line actual:
cat /proc/cmdline
# BOOT_IMAGE=/vmlinuz-6.x root=/dev/sda1 ro quiet
#
# Si incluyera systemd.unit=rescue.target → habría arrancado en rescue mode
```

### Targets de emergencia predefinidos

| Target | Propósito | Acceso |
|--------|-----------|--------|
| `emergency.target` | Shell mínimo; monta solo `/` read-only | `systemd.unit=emergency.target` en cmdline |
| `rescue.target` | Single-user; activa `basic.target` y `sysinit.target` | `systemd.unit=rescue.target` en cmdline |
| `halt.target` | Detiene el sistema sin apagar | `sudo systemctl halt` |
| `poweroff.target` | Apaga la máquina | `sudo systemctl poweroff` |

---

## El proceso de boot paso a paso

### Fase 1: Hardware → systemd (PID 1)

```
BIOS/UEFI  →  GRUB  →  Kernel  →  initramfs  →  systemd (PID 1)
```

```bash
# El kernel ejecuta /sbin/init, que es un symlink a systemd:
ls -la /sbin/init
# /sbin/init → /usr/lib/systemd/systemd

# systemd (PID 1) es el primer proceso — ancestro de todos los demás
ps -p 1 -o comm=
# systemd
```

**initramfs** es una imagen initrd (initial RAM disk) que:
- Contiene módulos del kernel necesarios para montar el filesystem raíz
- Descomprime el root filesystem antes de transferir el control a systemd
- Es provista por el bootloader (GRUB) junto con el kernel

 systemd lee en secuencia:
1. `/proc/cmdline` — parámetros del kernel (incluyendo `systemd.unit=`)
2. `/etc/systemd/system.conf` — configuración global del manager
3. `default.target` — el target que define qué debe estar "activo" al completar el boot

### Fase 2: Resolución de dependencias (transaction)

 systemd construye un **grafo de dependencias** (transaction) antes de activar cualquier unidad:

```bash
# 1. Lee default.target (ej: multi-user.target)
# 2. Resuelve Wants= y Requires= recursivamente → todas las unidades que deben existir
# 3. Ordena según After= y Before= → define el orden de activación
# 4. Detecta dependencias circulares → error si hay ciclos

# Ver la cadena de dependencias completa:
systemctl list-dependencies default.target --no-pager
```

### Fase 3: Arranque paralelo

```
                     default.target
                          │
           ┌──────────────┼──────────────┐
           │              │              │
      local-fs       network         sockets
       .target        .target        .target
          │              │              │
       ┌──┴──┐        ┌──┴──┐        ┌──┴──┐
       │     │        │     │        │     │
    home.  var.      NM.   fire-    sshd.
    mount mount   service  walld   socket
                                   ← todos arrancan en paralelo
                                   si no hay After/Before entre ellos
```

Las unidades que **no tienen relación de orden** entre sí (`After=`/`Before=`) arrancan **simultáneamente**. Esto es lo que hace que systemd sea significativamente más rápido que SysVinit, donde todo era secuencial.

---

## Analizar el boot

### systemd-analyze — Tiempo total

```bash
systemd-analyze
# Startup finished in 2.5s (kernel) + 5.1s (userspace) = 7.6s
# graphical.target reached after 5.0s in userspace
```

| Fase | Descripción |
|------|-------------|
| **kernel** | Tiempo desde que el hardware ejecuta el kernel hasta que este lanza systemd (PID 1) |
| **initramfs** | Descompresión y transferencia a systemd (incluido en kernel en algunos sistemas) |
| **userspace** | Tiempo desde que systemd inicia hasta que `default.target` está completamente activo |

### systemd-analyze blame — Qué tardó más

```bash
systemd-analyze blame --no-pager
# 5.100s cloud-init.service
# 3.200s NetworkManager-wait-online.service
# 2.100s docker.service
# 1.500s apt-daily.service
# 0.800s nginx.service
```

> **Nota:** `blame` mide tiempo *wall-clock*, no CPU. Un servicio "lento" puede estar simplemente esperando (ej: red, disco, otra unidad).

 Los culprits más comunes en servidores:
- `NetworkManager-wait-online.service` — espera que la red esté *realmente* disponible
- `cloud-init.service` — innecesario en bare metal (solo en cloud)
- `apt-daily.service` — busca actualizaciones en segundo plano

### systemd-analyze critical-chain — Camino crítico

```bash
systemd-analyze critical-chain --no-pager
# graphical.target @5.0s
# └─display-manager.service @4.2s +800ms
#   └─multi-user.target @4.1s
#     └─nginx.service @3.5s +600ms
#       └─network-online.target @3.4s
#         └─NetworkManager-wait-online.service @1.2s +2.2s
#           └─NetworkManager.service @1.0s +200ms
#             └─basic.target @0.9s
```

Cada línea: `unidad @tiempo_absoluto +tiempo_gastado_en_esta_unidad`

La **cadena crítica** es el camino más largo de dependencias — determina el tiempo mínimo del boot. Cada `+Xms` es tiempo bloqueado esperando a esa dependencia.

```bash
# Para un target específico:
systemd-analyze critical-chain multi-user.target --no-pager
```

### systemd-analyze plot — Gráfico SVG

```bash
# Genera un SVG con líneas de tiempo por unidad:
systemd-analyze plot > /tmp/boot.svg

# Abrir con navegador o visor de imágenes:
# Cada unidad = barra horizontal
#   Rojo   = activando
#   Verde  = activo
#   Gris   = fallido
# Barras que se solapan = arranque paralelo real
# La barra más a la derecha = define el tiempo total
```

### systemd-analyze verify — Verificar unidades

```bash
# Verificar errores de sintaxis y dependencias:
systemd-analyze verify /etc/systemd/system/myapp.service

# Detecta:
# - Syntax errors en el archivo .service
# - Dependencias circulares
# - Units referenciadas que no existen
# - Conflicts con otras units
```

---

## Optimizar el boot

### Paso 1: Identificar cuellos de botella

```bash
# Ver los 10 servicios más lentos:
systemd-analyze blame --no-pager | head -10

# Ver la cadena crítica:
systemd-analyze critical-chain --no-pager

# Causas comunes de boot lento:
# ┌─────────────────────────────────────┬──────────────────────────────────┐
# │ Servicio                            │ Causa                            │
# ├─────────────────────────────────────┼──────────────────────────────────┤
# │ NetworkManager-wait-online.service  │ Espera a que la red esté "up"   │
# │ cloud-init.service                  │ Solo en cloud; inútil en metal  │
# │ apt-daily.service                   │ Busca actualizaciones al boot   │
# │ docker.service                      │ Inicia todos los contenedores  │
# │ plymouth-quit-wait.service          │ Espera a que GDM/X11 arranque  │
# │ snap.appliance.service              │ Inicialización de Snap          │
# └─────────────────────────────────────┴──────────────────────────────────┘
```

### Paso 2: Deshabilitar servicios innecesarios

```bash
# Solo si los servicios NO necesitan network-online.target:

# En servidores bare metal (sin cloud):
sudo systemctl disable cloud-init.service
sudo systemctl mask cloud-init.service

# Servidores sin Docker:
sudo systemctl disable docker.service
sudo systemctl disable containerd.service

# Servidores donde no importa cuándo esté la red disponible:
sudo systemctl disable NetworkManager-wait-online.service

# Si no necesitas splash screen:
sudo systemctl mask plymouth-quit-wait.service
```

### Paso 3: Cambiar default target (servidores)

```bash
# En servidores, multi-user.target es suficiente (sin GUI):
sudo systemctl set-default multi-user.target

# El graphical.target incluye display-manager + X11/Wayland
# que añaden ~2-5 segundos innecesarios en un servidor
```

### Paso 4: Socket activation (arranque on-demand)

```bash
# En lugar de que sshd arrance al boot:
sudo systemctl disable sshd.service
sudo systemctl enable sshd.socket

# sshd solo se instancia cuando alguien se conecta al puerto 22
# Esto elimina sshd del boot crítico path
```

### Medir impacto antes/después

```bash
# ANTES de optimizar:
systemd-analyze
# Startup finished in 2.5s (kernel) + 8.1s (userspace) = 10.6s

# DESPUÉS:
systemd-analyze
# Startup finished in 2.5s (kernel) + 4.2s (userspace) = 6.7s
#              ↑ unchanged              ↑ -3.9s
```

---

## Kernel command line y systemd

```bash
cat /proc/cmdline
# BOOT_IMAGE=/vmlinuz-6.x root=/dev/sda1 ro quiet splash
```

| Parámetro | Efecto | Ejemplo |
|-----------|--------|---------|
| `systemd.unit=TARGET` | Override temporal del target de arranque | `systemd.unit=rescue.target` |
| `systemd.mask=UNIT` | Enmascarar una unidad para este boot | `systemd.mask=NetworkManager.service` |
| `systemd.wants=UNIT` | Agregar una unidad extra al boot | `systemd.wants=debug-shell.service` |
| `systemd.log_level=debug` | Logs muy verbosos durante boot | Diagnóstico |
| `systemd.log_target=console` | Enviar logs directamente a la consola | Diagnóstico |
| `rd.break` | Romper en initramfs (antes de montar root) | Reset password, recovery |
| `init=/bin/bash` | Bypass total de systemd — shell directo del kernel | **¡Último recurso!** |

### Uso práctico: rescate de contraseña root

```bash
# En GRUB, editar la línea del kernel y añadir:
rd.break

# Esto rompe en un shell dentro de initramfs (antes de montar / real)

# Desde ahí:
mount -o remount,rw /sysroot
chroot /sysroot
passwd root
touch /.autorelabel
exit
# Reiniciar — el sistema restaurará selinux contexts automáticamente
```

---

## debug-shell.service — Shell de emergencia

```bash
# Proporciona un shell root en tty9 accesible durante el boot
# Útil cuando el sistema no puede arrancar y systemctl no funciona

# Habilitar (temporalmente):
sudo systemctl enable debug-shell.service
# O añadir systemd.wants=debug-shell.service en GRUB cmdline

# Durante boot: Ctrl+Alt+F9 → root shell sin contraseña

# ⚠️ RIESGO DE SEGURIDAD: acceso físico = root inmediato
# Deshabilitar INMEDIATAMENTE después de resolver:
sudo systemctl disable debug-shell.service
```

---

## Generar diagramas del grafo de dependencias

```bash
# Generar salida DOT (formato Graphviz):
systemd-analyze dot default.target 2>/dev/null | head -20
# Requesting for units: default.target
#   "default.target" -> (nginx.service)
#   "nginx.service" -> (network-online.target)

# Convertir a imagen PNG:
systemd-analyze dot default.target 2>/dev/null | dot -Tpng -o /tmp/boot-deps.png

# El plot (SVG) muestra TIEMPOS; el dot muestra DEPENDENCIAS
# Ambos son complementarios para diagnóstico
```

---

## Troubleshooting quick-reference

| Problema | Diagnóstico | Solución |
|----------|-------------|----------|
| Boot extremadamente lento | `systemd-analyze blame` | Identificar servicio más lento, deshabilitar |
| Sistema pegado en "A start job is running for..." | `journalctl -b` | Buscar el servicio que no responde |
| "Failed to start" pero no sé cuál | `systemctl --failed --no-pager` | Ver units fallidas |
| systemd no arranca (PID 1) | `rd.break` en cmdline | Recovery shell en initramfs |
| Servicios que compiten por el mismo puerto | `ss -tlnp` después del boot | Reconfigurar puertos o deshabilitar servicio |
| Boot correcto pero aplicación no funciona | `journalctl -u NOMBRE.service` | Logs específicos del servicio |

```bash
# Ver logs del boot actual:
journalctl -b --no-pager

# Ver logs de un servicio específico:
journalctl -u nginx.service -b --no-pager

# Ver logs del boot anterior (si persiste):
journalctl -b -1 --no-pager
```

---

## Ejercicios

### Ejercicio 1 — Analizar tu boot

```bash
# 1. ¿Cuánto tardó el boot completo?
systemd-analyze

# 2. ¿Cuáles son los 5 servicios más lentos?
systemd-analyze blame --no-pager | head -5

# 3. ¿Cuál es la cadena crítica?
systemd-analyze critical-chain --no-pager

# 4. ¿Cuántas unidades dependen de tu default target?
systemctl list-dependencies "$(systemctl get-default)" --plain --no-pager | wc -l

# 5. ¿Hay algún parámetro systemd en tu kernel cmdline?
grep -oE 'systemd\.[a-z_]+=[^ ]+' /proc/cmdline || echo "Ninguno"
```

### Ejercicio 2 — Simular un boot rescue

> ⚠️ **Solo hacer en un entorno de prueba (máquina virtual)**

```bash
# Arrancar en rescue target (temporal, no persistente):
# En GRUB, añadir: systemd.unit=rescue.target

# Verificar que el sistema responde:
systemctl status
# Debería mostrar rescue.target como el objetivo actual

# Salir de rescue:
# Escribir Ctrl+D o exit → continúa el boot normal
```

### Ejercicio 3 — Optimizar boot en una VM de pruebas

```bash
# Hipótesis: cloud-init y apt-daily son innecesarios en una VM local

# 1. Medir baseline:
systemd-analyze > /tmp/before.txt

# 2. Deshabilitar servicios innecesarios:
sudo systemctl disable cloud-init.service 2>/dev/null
sudo systemctl mask cloud-init.service 2>/dev/null
sudo systemctl disable apt-daily.timer apt-daily-upgrade.timer 2>/dev/null

# 3. Medir después:
systemd-analyze > /tmp/after.txt
diff /tmp/before.txt /tmp/after.txt

# 4. Comparar tiempos
```

### Ejercicio 4 — Generar y analizar el plot SVG

```bash
# Generar el gráfico de boot:
systemd-analyze plot > /tmp/boot.svg

# Copiar a tu máquina local y abrir en navegador:
# scp usuario@servidor:/tmp/boot.svg .

# Analizar:
# - ¿Qué barras rojas (activando) son más largas?
# - ¿Qué servicios arrancan en paralelo (barras solapadas)?
# - ¿Cuál es la barra más a la derecha?
```

### Ejercicio 5 — Investigar la cadena crítica

```bash
# Identificar qué servicio está en la cadena crítica:
systemd-analyze critical-chain --no-pager | grep -E '^\w+\.\w+ @'

# Intentar reducir su tiempo de activación:
# Ej: si NetworkManager-wait-online está en la cadena:
systemctl status NetworkManager-wait-online.service

# Documentar qué lo hace lento (¿configuración? ¿hardware?)
journalctl -u NetworkManager-wait-online.service -b --no-pager | tail -20
```
