# T02 — Cambiar target

## get-default / set-default

El **default target** es el target que systemd arranca al boot:

```bash
# Ver el target por defecto:
systemctl get-default
# graphical.target  (desktop)
# multi-user.target (servidor)

# Internamente es un symlink:
ls -la /etc/systemd/system/default.target
# → /usr/lib/systemd/system/multi-user.target
```

```bash
# Cambiar el target por defecto:
sudo systemctl set-default multi-user.target
# Removed /etc/systemd/system/default.target
# Created symlink /etc/systemd/system/default.target → .../multi-user.target

sudo systemctl set-default graphical.target
# Created symlink → .../graphical.target

# set-default cambia el PRÓXIMO boot, no la sesión actual
```

```bash
# Caso típico: convertir un desktop en servidor
sudo systemctl set-default multi-user.target
sudo systemctl reboot
# Ahorra ~200-500MB de RAM (display manager, compositor, etc.)
```

## isolate — Cambiar target en caliente

`isolate` cambia al target indicado **inmediatamente**, deteniendo las unidades
que no pertenecen al nuevo target:

```bash
# Quitar la GUI:
sudo systemctl isolate multi-user.target
# Detiene display-manager, X11/Wayland
# Mantiene sshd, cron, etc. (pertenecen a multi-user.target)

# Arrancar la GUI:
sudo systemctl isolate graphical.target
# Arranca el display manager

# Modo rescate:
sudo systemctl isolate rescue.target
# Detiene casi todo — solo queda lo mínimo
# Pide contraseña de root

# Modo emergencia:
sudo systemctl isolate emergency.target
# Aún más mínimo que rescue
```

### isolate vs set-default

```bash
# isolate   → cambio INMEDIATO, NO persiste al reiniciar
# set-default → cambia el PRÓXIMO boot, PERSISTE

# Ejemplo: liberar recursos temporalmente sin cambiar el default
sudo systemctl isolate multi-user.target
# ... hacer trabajo pesado ...
sudo systemctl isolate graphical.target
# La GUI vuelve sin reiniciar, el default no se tocó
```

### AllowIsolate

No todos los targets permiten isolate. Solo los que tienen `AllowIsolate=yes`:

```bash
# Ver cuáles permiten isolate:
grep -rl "AllowIsolate=yes" /usr/lib/systemd/system/*.target 2>/dev/null | \
    xargs -I {} basename {}
# poweroff.target
# rescue.target
# emergency.target
# multi-user.target
# graphical.target
# reboot.target
# halt.target

# Intentar isolate un target sin AllowIsolate:
sudo systemctl isolate network.target
# Operation refused, unit network.target may be requested only indirectly
```

## Acciones del sistema

```bash
# Apagar:
sudo systemctl poweroff         # equivale a shutdown -h now

# Reiniciar:
sudo systemctl reboot           # equivale a shutdown -r now

# Halt (detener sin apagar hardware):
sudo systemctl halt

# Suspender (a RAM — bajo consumo, despertar rápido):
sudo systemctl suspend

# Hibernar (a disco — se apaga, restaura al encender):
sudo systemctl hibernate

# Suspend-then-hibernate (suspende, después de un tiempo hiberna):
sudo systemctl suspend-then-hibernate
```

### shutdown programado

```bash
# Con delay:
sudo shutdown -h +10              # apagar en 10 minutos
sudo shutdown -r 23:00            # reiniciar a las 23:00
sudo shutdown -h now              # apagar ahora

# Cancelar:
sudo shutdown -c

# Mensaje a usuarios loggeados:
sudo shutdown -h +5 "Mantenimiento programado, guardar trabajo"
```

## Cambiar target desde GRUB

Cuando no se puede hacer login, el target se puede cambiar desde GRUB antes
de que el sistema arranque:

```bash
# 1. En el menú de GRUB, presionar 'e' para editar la entrada

# 2. Encontrar la línea que empieza con 'linux' (o 'linuxefi'):
#    linux /vmlinuz-6.x root=/dev/sda1 ro quiet

# 3. Agregar al final de esa línea:
#    systemd.unit=rescue.target       # modo rescate
#    systemd.unit=emergency.target    # modo emergencia
#    systemd.unit=multi-user.target   # sin GUI

# 4. Presionar Ctrl+X o F10 para arrancar

# Estos cambios son TEMPORALES — solo para este boot
```

```bash
# Alternativas legacy que siguen funcionando:
# linux /vmlinuz... single            # rescue mode
# linux /vmlinuz... 1                 # rescue mode (runlevel 1)
# linux /vmlinuz... 3                 # multi-user (runlevel 3)
# linux /vmlinuz... emergency         # emergency mode
```

### Reset de contraseña de root

```bash
# Método con rd.break:
# 1. En GRUB, editar la entrada y agregar: rd.break

# 2. Se obtiene un shell en el initramfs (antes de montar el root real)

# 3. Montar el filesystem real:
mount -o remount,rw /sysroot

# 4. Entrar al filesystem:
chroot /sysroot

# 5. Cambiar la contraseña:
passwd root

# 6. Si SELinux está activo (RHEL):
touch /.autorelabel
# Fuerza relabel completo en el próximo boot

# 7. Salir y reiniciar:
exit
exit
```

## Targets custom

Se pueden crear targets personalizados para agrupar servicios de una
aplicación:

```ini
# /etc/systemd/system/myapp.target
[Unit]
Description=My Application Stack
Requires=multi-user.target
After=multi-user.target
```

```ini
# Los servicios de la aplicación apuntan al target:
# /etc/systemd/system/myapp-api.service
[Install]
WantedBy=myapp.target

# /etc/systemd/system/myapp-worker.service
[Install]
WantedBy=myapp.target
PartOf=myapp.target
```

```bash
# Gestionar toda la aplicación como un grupo:
sudo systemctl enable myapp-api.service myapp-worker.service
sudo systemctl start myapp.target    # arranca todo el stack
sudo systemctl stop myapp.target     # detiene todo (si tienen PartOf)

systemctl list-dependencies myapp.target
```

## Equivalencias SysVinit → systemd

| SysVinit | systemd |
|---|---|
| `init 0` | `systemctl poweroff` |
| `init 1` | `systemctl isolate rescue.target` |
| `init 3` | `systemctl isolate multi-user.target` |
| `init 5` | `systemctl isolate graphical.target` |
| `init 6` | `systemctl reboot` |
| `telinit 3` | `systemctl isolate multi-user.target` |
| `shutdown -h now` | `systemctl poweroff` |
| `shutdown -r now` | `systemctl reboot` |

---

## Ejercicios

### Ejercicio 1 — Default target

```bash
# 1. ¿Cuál es el target por defecto?
systemctl get-default

# 2. Verificar el symlink:
ls -la /etc/systemd/system/default.target 2>/dev/null

# 3. ¿Cuántas unidades dependen del default?
systemctl list-dependencies "$(systemctl get-default)" --no-pager | wc -l
```

### Ejercicio 2 — Targets que permiten isolate

```bash
for target in /usr/lib/systemd/system/*.target; do
    if grep -q "AllowIsolate=yes" "$target" 2>/dev/null; then
        echo "$(basename "$target")"
    fi
done
```

### Ejercicio 3 — Estado actual

```bash
# ¿En qué "runlevel" estás?
runlevel

# ¿El default target está activo?
DEFAULT=$(systemctl get-default)
systemctl is-active "$DEFAULT" && echo "$DEFAULT activo" || echo "$DEFAULT NO activo"
```
