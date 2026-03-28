# T02 — Cambiar target

## Conceptos fundamentales

```
┌─────────────────────────────────────────────────────────────┐
│               DEFAULT TARGET vs ISOLATE                        │
│                                                              │
│  default.target                                              │
│  → El target que systemd arranca cuando el sistema BOOTEA  │
│  → Configurado en /etc/systemd/system/default.target         │
│  → Se cambia con: systemctl set-default                    │
│  → Persiste entre reboots                                  │
│                                                              │
│  isolate                                                   │
│  → Cambiar a otro target INMEDIATAMENTE                    │
│  → Detiene las unidades que no pertenecen al nuevo target  │
│  → Solo funciona con targets que tienen AllowIsolate=yes    │
│  → NO persiste al reboot                                    │
└─────────────────────────────────────────────────────────────┘
```

## get-default / set-default — Target al boot

```bash
# Ver el target por defecto (el que se usa al arrancar):
systemctl get-default
# multi-user.target    (servidor)
# graphical.target    (desktop)

# El default target es un symlink:
ls -la /etc/systemd/system/default.target
# ... default.target → /usr/lib/systemd/system/multi-user.target
```

```bash
# Cambiar el target por defecto (para el PRÓXIMO boot):
sudo systemctl set-default multi-user.target
# Removed /etc/systemd/system/default.target
# Created symlink /etc/systemd/system/default.target
#     → /usr/lib/systemd/system/multi-user.target

sudo systemctl set-default graphical.target
# Created symlink /etc/systemd/system/default.target
#     → /usr/lib/systemd/system/graphical.target
```

```
set-default SOLO cambia el PRÓXIMO boot
No afecta la sesión actual
```

```bash
# Caso de uso: convertir un desktop en servidor
# (ahorra ~200-500MB de RAM)

sudo systemctl set-default multi-user.target
sudo reboot
# Al arrancar: entra directo a modo texto (sin GUI)
```

## isolate — Cambio inmediato

`isolate` cambia al target indicado **inmediatamente**, deteniendo las unidades que no pertenecen al nuevo target:

```bash
# Cambiar a multi-user.target (quitar GUI):
sudo systemctl isolate multi-user.target
# → Detiene: display-manager, X11/Wayland, compositor
# → Mantiene: sshd, cron, networking, todos los servicios de multi-user

# Volver a graphical:
sudo systemctl isolate graphical.target
# → Arranca el display manager
# → Sesión gráfica disponible

# Modo rescate:
sudo systemctl isolate rescue.target
# → Detiene casi todo
# → Solo queda el sistema base + shell de root

# Modo emergencia:
sudo systemctl isolate emergency.target
# → Aún más mínimo que rescue
# → Filesystem de root en read-only
```

### isolate vs set-default

```
┌─────────────────────────────────────────────────────────────┐
│                                                              │
│  isolate                                                   │
│  → Cambio INMEDIATO (sesión actual)                       │
│  → Detiene unidades no relacionadas con el target          │
│  → NO persiste al reboot                                    │
│  → Necesita AllowIsolate=yes en el target                │
│                                                              │
│  set-default                                               │
│  → Cambio para el PRÓXIMO boot                           │
│  → NO afecta la sesión actual                              │
│  → Persiste entre reboots                                  │
│                                                              │
└─────────────────────────────────────────────────────────────┘
```

```bash
# Ejemplo: trabajo pesado sin GUI, luego volver
sudo systemctl isolate multi-user.target
# ... hacer trabajo intensivo (más CPU/RAM disponible) ...
sudo systemctl isolate graphical.target
# La GUI vuelve, el default.target no se tocó
```

### AllowIsolate

No todos los targets permiten `isolate`. Solo los que tienen `AllowIsolate=yes`:

```bash
# Ver cuáles targets permiten isolate:
grep -rl "AllowIsolate=yes" /usr/lib/systemd/system/*.target 2>/dev/null | \
    xargs -I {} basename {}

# Generalmente son:
# poweroff.target
# halt.target
# rescue.target
# emergency.target
# multi-user.target
# graphical.target
# reboot.target
```

```bash
# Intentar isolate un target sin AllowIsolate:
sudo systemctl isolate network.target
# Failed to isolate unit: Unit network.target
#     may be requested only indirectly

# Los targets de sincronización (network, local-fs, etc.)
# NO permiten isolate — son puntos de sincronización, no estados
```

## Acciones del sistema

```bash
# APAGAR
sudo systemctl poweroff
sudo systemctl poweroff --force     # sin llamar a los servicios de shutdown
# Equivalente: shutdown -h now, init 0

# REINICIAR
sudo systemctl reboot
sudo systemctl reboot --force      # sin shutdown limpio
# Equivalente: shutdown -r now, init 6

# SOLO DETENER (sin apagar hardware)
sudo systemctl halt
# Equivalente: halt

# SUSPEND (a RAM — bajo consumo,醒来 rápido)
sudo systemctl suspend
# El estado se mantiene en RAM
# Despertar: botón de encendido, movimiento de mouse, etc.

# HIBERNATE (a disco — se apaga completamente)
sudo systemctl hibernate
# El estado completo se guarda en swap
# Al encender: restaura exactamente donde estaba

# SUSPEND THEN HIBERNATE
sudo systemctl suspend-then-hibernate
# Si suspend no se despierta en X tiempo → hiberna automáticamente
```

```bash
# SUSPEND vs HIBERNATE:
#
# SUSPEND: estado en RAM, consume ~5-15W,醒来 instantáneo
# HIBERNATE: estado en disco, consume 0W, despertar ~30 segundos
```

## Shutdown programado

```bash
# Apagar en X minutos:
sudo shutdown -h +10              # en 10 minutos
sudo shutdown -h +30             # en 30 minutos
sudo shutdown -h 23:00            # a las 23:00

# Reiniciar:
sudo shutdown -r +15 "Mantenimiento en 15 min"
sudo shutdown -r 02:00 "Actualización del sistema"

# Cancelar shutdown pendiente:
sudo shutdown -c

#shutdown sin delay:
sudo shutdown -h now
sudo shutdown -r now

#shutdown con mensaje (broadcast a usuarios):
sudo shutdown -h +5 "Servidor se apagará en 5 minutos para mantenimiento"
```

## Cambiar target desde GRUB

Cuando el sistema no puede arrancar normalmente (login broken), se puede cambiar el target antes de que systemd inicie:

```bash
# 1. En el menú de GRUB, presionar 'e' para editar

# 2. Encontrar la línea linux (o linuxefi):
linux /vmlinuz-6.1.0-21-amd64 root=/dev/sda1 ro quiet

# 3. Agregar al FINAL de la línea linux:
systemd.unit=rescue.target        # modo rescate
systemd.unit=emergency.target   # modo emergencia
systemd.unit=multi-user.target # sin GUI
systemd.unit=graphical.target  # con GUI

# Ejemplo:
linux /vmlinuz-6.1.0-21-amd64 root=/dev/sda1 ro quiet systemd.unit=rescue.target

# 4. Presionar Ctrl+X o F10 paraboot con ese parámetro

# CAMBIOS SON TEMPORALES — solo para este boot
```

```bash
# Métodos legacy (también funcionan):
linux /vmlinuz... single         # rescue mode
linux /vmlinuz... 1              # rescue mode (runlevel 1)
linux /vmlinuz... 3              # multi-user (runlevel 3)
linux /vmlinuz... emergency       # emergency mode
```

## Resetear contraseña de root

```bash
# Pasos para recovery via GRUB + rd.break:

# 1. En GRUB, editar y agregar rd.break al final de la línea linux:
linux ... ro quiet systemd.unit=emergency.target
# ó:
linux ... rd.break

# 2. Albootear, obtener un shell en el initramfs
# (antes de montar el root real)

# 3. Montar el filesystem real como read-write:
mount -o remount,rw /sysroot

# 4. Entrar al chroot del sistema:
chroot /sysroot

# 5. Cambiar la contraseña:
passwd root

# 6. Si SELinux está activo (RHEL, Fedora):
touch /.autorelabel
# Fuerza relabel de SELinux en el próximo boot (tarda minutos)

# 7. Salir y reiniciar:
exit
# (del chroot)
exit
# (del initramfs shell, reinicia automáticamente)
```

## Targets custom

Se pueden crear targets para agrupar servicios de una aplicación:

```ini
# /etc/systemd/system/myapp.target
[Unit]
Description=My Application Stack
Requires=multi-user.target
After=multi-user.target
AllowIsolate=yes

[Install]
WantedBy=multi-user.target
```

```ini
# /etc/systemd/system/myapp-api.service
[Unit]
Description=My App API Server
After=network.target

[Service]
Type=simple
ExecStart=/opt/myapp/bin/api-server
Restart=on-failure

[Install]
WantedBy=myapp.target
```

```ini
# /etc/systemd/system/myapp-worker.service
[Unit]
Description=My App Background Worker
PartOf=myapp.target
After=myapp.target

[Service]
Type=simple
ExecStart=/opt/myapp/bin/worker
Restart=on-failure

[Install]
WantedBy=myapp.target
```

```bash
# Gestionar toda la aplicación como una unidad:
sudo systemctl daemon-reload
sudo systemctl enable myapp-api.service myapp-worker.service

# Arrancar todo el stack:
sudo systemctl start myapp.target

# Detener todo el stack (PartOf hace que se detenga automáticamente):
sudo systemctl stop myapp.target

# Ver qué tiene el target:
systemctl list-dependencies myapp.target
```

## systemctl —systemd-sysv-install (para crear servicios)

```bash
# systemd-sysv-install es para crear symlinks de servicios SysV:
# (ya no se usa prácticamente, pero existe)

# Para habilitados de servicios:
systemctl enable nginx.service
# Crea symlink en target.wants/

systemctl disable nginx.service
# Elimina symlink de target.wants/
```

## Equivalencias SysVinit → systemd

| SysVinit | systemd |
|---|---|
| `init 0` | `systemctl poweroff` |
| `init 1` | `systemctl isolate rescue.target` |
| `init 2` | `systemctl isolate multi-user.target` |
| `init 3` | `systemctl isolate multi-user.target` |
| `init 4` | `systemctl isolate multi-user.target` |
| `init 5` | `systemctl isolate graphical.target` |
| `init 6` | `systemctl reboot` |
| `telinit 3` | `systemctl isolate multi-user.target` |
| `shutdown -h now` | `systemctl poweroff` |
| `shutdown -r now` | `systemctl reboot` |
| `runlevel` | `systemctl get-default` + `runlevel` |

---

## Ejercicios

### Ejercicio 1 — Default target

```bash
# 1. Ver el target por defecto
systemctl get-default

# 2. Ver el symlink real
ls -la /etc/systemd/system/default.target
readlink /etc/systemd/system/default.target

# 3. Cuántas unidades pertenecen al default target?
systemctl list-dependencies "$(systemctl get-default)" --no-pager | wc -l

# 4. NO cambiar nada permanente en este ejercicio
```

### Ejercicio 2 — isolate vs set-default

```bash
# 1. Ver el estado actual
runlevel
systemctl get-default

# 2. En un desktop con GUI (si aplica):
# Cambiar temporalmente a multi-user (sin GUI):
# sudo systemctl isolate multi-user.target
# Ver que la GUI se cerró

# Volver a graphical:
# sudo systemctl isolate graphical.target

# 3. Verificar que set-default NO cambia nada ahora
# sudo systemctl set-default multi-user.target
# runlevel  (el actual sigue igual)

# 4. El cambio de set-default se ve SOLO después de reboot
```

### Ejercicio 3 — Targets que permiten isolate

```bash
# 1. Encontrar todos los targets con AllowIsolate=yes
grep -l "AllowIsolate=yes" /usr/lib/systemd/system/*.target 2>/dev/null | \
    while read f; do basename "$f"; done

# 2. Intentar isolate uno que NO permite isolate
# (en un VM o entorno de prueba)
# sudo systemctl isolate basic.target
# Debería fallar

# 3. Intentar isolate uno que SÍ permite
# sudo systemctl isolate multi-user.target
# Debería funcionar
```

### Ejercicio 4 — Targets custom

```bash
# 1. Crear un target simple de prueba
sudo tee /etc/systemd/system/testapp.target << 'EOF'
[Unit]
Description=Test Application Target
Requires=multi-user.target
After=multi-user.target
AllowIsolate=yes

[Install]
WantedBy=multi-user.target
EOF

# 2. Recargar
sudo systemctl daemon-reload

# 3. Ver el target
systemctl status testapp.target 2>/dev/null || echo "not started yet"

# 4. Arrancar
sudo systemctl start testapp.target
systemctl status testapp.target

# 5. Detener
sudo systemctl stop testapp.target

# 6. Limpiar
sudo systemctl disable testapp.target 2>/dev/null || true
sudo rm /etc/systemd/system/testapp.target
sudo systemctl daemon-reload
```

### Ejercicio 5 — systemctl poweroff/reboot

```bash
# En un VM para probar:

# 1. Programar shutdown en 1 minuto (para probar sin afectar producción)
# sudo shutdown -h +1 "Test shutdown"

# 2. Cancelar antes de que pase
# sudo shutdown -c

# 3. Ver que mensaje a usuarios logueados:
who
# broadcast message from root@server:
# El sistema se apagará en X minutos
```

### Ejercicio 6 — Cambiar target via kernel parameter

```bash
# Este ejercicio es TEÓRICO — no ejecutar en producción

# 1. En GRUB (albootear), editar la línea linux
# Agregar: systemd.unit=rescue.target

# 2. boot
# El sistema entra en rescue mode directamente

# 3. Verificar
systemctl get-default
# Nota: el default.target real no cambió, solo este boot usó rescue

# 4. Normal: hacer lo mismo con graphical.target
# ó reiniciar sin parámetro para boot normal
```
