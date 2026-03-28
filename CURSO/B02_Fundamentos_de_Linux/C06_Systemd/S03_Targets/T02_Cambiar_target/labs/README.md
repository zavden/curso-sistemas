# Lab — Cambiar target

## Objetivo

Gestionar targets de systemd: consultar y cambiar el default target,
usar isolate para cambio inmediato, entender AllowIsolate, acciones
del sistema, cambio de target desde GRUB, y crear targets custom
para agrupar servicios de aplicaciones.

## Prerequisitos

- Entorno del curso levantado (`docker compose up -d`)

---

## Parte 1 — Default target

### Objetivo

Consultar y entender el target de arranque por defecto.

### Paso 1.1: get-default

```bash
docker compose exec debian-dev bash -c '
echo "=== Default target ==="
echo ""
echo "El default target es el target que systemd arranca al boot"
echo ""
echo "--- Verificar ---"
DEFAULT=$(readlink /etc/systemd/system/default.target 2>/dev/null || \
          readlink /usr/lib/systemd/system/default.target 2>/dev/null || \
          echo "no configurado")
echo "default.target → $DEFAULT"
echo ""
echo "--- Internamente es un symlink ---"
ls -la /etc/systemd/system/default.target 2>/dev/null || \
    echo "/etc/systemd/system/default.target no existe"
echo ""
ls -la /usr/lib/systemd/system/default.target 2>/dev/null || \
    ls -la /lib/systemd/system/default.target 2>/dev/null || \
    echo "default.target no encontrado en /usr/lib/"
'
```

### Paso 1.2: set-default

```bash
docker compose exec debian-dev bash -c '
echo "=== set-default ==="
echo ""
echo "Cambia el target del PROXIMO boot (no la sesion actual)"
echo ""
echo "--- Comandos ---"
echo "systemctl set-default multi-user.target"
echo "  Removed /etc/systemd/system/default.target"
echo "  Created symlink → .../multi-user.target"
echo ""
echo "systemctl set-default graphical.target"
echo "  Created symlink → .../graphical.target"
echo ""
echo "--- Caso tipico: desktop a servidor ---"
echo "systemctl set-default multi-user.target"
echo "systemctl reboot"
echo "  Ahorra ~200-500MB RAM (sin display manager, compositor)"
echo ""
echo "--- set-default vs isolate ---"
echo "set-default → cambia el PROXIMO boot, PERSISTE"
echo "isolate     → cambio INMEDIATO, NO persiste al reiniciar"
'
```

### Paso 1.3: Comparar Debian vs RHEL

```bash
docker compose exec debian-dev bash -c '
echo "=== Default target en Debian ==="
DEFAULT=$(readlink /etc/systemd/system/default.target 2>/dev/null || \
          readlink /usr/lib/systemd/system/default.target 2>/dev/null || \
          readlink /lib/systemd/system/default.target 2>/dev/null)
echo "default.target → $DEFAULT"
'
```

```bash
docker compose exec alma-dev bash -c '
echo "=== Default target en AlmaLinux ==="
DEFAULT=$(readlink /etc/systemd/system/default.target 2>/dev/null || \
          readlink /usr/lib/systemd/system/default.target 2>/dev/null)
echo "default.target → $DEFAULT"
'
```

---

## Parte 2 — Isolate y acciones

### Objetivo

Entender isolate para cambio inmediato de target y AllowIsolate.

### Paso 2.1: isolate

```bash
docker compose exec debian-dev bash -c '
echo "=== systemctl isolate ==="
echo ""
echo "Cambia al target indicado INMEDIATAMENTE"
echo "Detiene las unidades que no pertenecen al nuevo target"
echo ""
echo "--- Ejemplos ---"
echo "systemctl isolate multi-user.target"
echo "  Detiene display-manager, X11/Wayland"
echo "  Mantiene sshd, cron, etc."
echo ""
echo "systemctl isolate graphical.target"
echo "  Arranca el display manager"
echo ""
echo "systemctl isolate rescue.target"
echo "  Detiene casi todo — solo lo minimo"
echo "  Pide contrasena de root"
echo ""
echo "systemctl isolate emergency.target"
echo "  Aun mas minimo que rescue"
echo ""
echo "--- isolate es TEMPORAL ---"
echo "El default target NO cambia"
echo "Al reiniciar, vuelve al default"
echo ""
echo "Ejemplo: liberar recursos temporalmente"
echo "  systemctl isolate multi-user.target"
echo "  ... trabajo pesado ..."
echo "  systemctl isolate graphical.target"
echo "  La GUI vuelve sin reiniciar"
'
```

### Paso 2.2: AllowIsolate

Antes de ejecutar, predice: todos los targets permiten isolate?

```bash
docker compose exec debian-dev bash -c '
echo "=== AllowIsolate ==="
echo ""
echo "Solo los targets con AllowIsolate=yes permiten isolate"
echo ""
echo "--- Targets que permiten isolate ---"
for target in /usr/lib/systemd/system/*.target \
              /lib/systemd/system/*.target; do
    [[ -f "$target" ]] || continue
    if grep -q "AllowIsolate=yes" "$target" 2>/dev/null; then
        echo "  $(basename "$target")"
    fi
done | sort -u
echo ""
echo "--- Intentar isolate un target sin AllowIsolate ---"
echo "systemctl isolate network.target"
echo "  Operation refused, unit may be requested only indirectly"
echo ""
echo "Solo los targets de estado del sistema permiten isolate"
echo "Los de sincronizacion (network, basic, etc.) no"
'
```

### Paso 2.3: Acciones del sistema

```bash
docker compose exec debian-dev bash -c '
echo "=== Acciones del sistema ==="
echo ""
echo "--- Equivalencias ---"
echo "| SysVinit              | systemd                          |"
echo "|-----------------------|----------------------------------|"
echo "| init 0                | systemctl poweroff               |"
echo "| init 1                | systemctl isolate rescue.target  |"
echo "| init 3                | systemctl isolate multi-user     |"
echo "| init 5                | systemctl isolate graphical      |"
echo "| init 6                | systemctl reboot                 |"
echo "| telinit 3             | systemctl isolate multi-user     |"
echo "| shutdown -h now       | systemctl poweroff               |"
echo "| shutdown -r now       | systemctl reboot                 |"
echo ""
echo "--- Acciones adicionales ---"
echo "systemctl suspend                suspender a RAM"
echo "systemctl hibernate              hibernar a disco"
echo "systemctl suspend-then-hibernate suspender, luego hibernar"
echo ""
echo "--- shutdown programado ---"
echo "shutdown -h +10                  apagar en 10 minutos"
echo "shutdown -r 23:00                reiniciar a las 23:00"
echo "shutdown -c                      cancelar shutdown"
echo "shutdown -h +5 \"Mantenimiento\"   con mensaje a usuarios"
'
```

---

## Parte 3 — GRUB y targets custom

### Objetivo

Cambiar el target desde GRUB y crear targets personalizados.

### Paso 3.1: Cambio desde GRUB

```bash
docker compose exec debian-dev bash -c '
echo "=== Cambiar target desde GRUB ==="
echo ""
echo "Cuando no se puede hacer login, el target se cambia desde GRUB"
echo ""
echo "Procedimiento:"
echo "  1. En el menu de GRUB, presionar e para editar"
echo ""
echo "  2. Encontrar la linea que empieza con linux:"
echo "     linux /vmlinuz-6.x root=/dev/sda1 ro quiet"
echo ""
echo "  3. Agregar al final:"
echo "     systemd.unit=rescue.target       modo rescate"
echo "     systemd.unit=emergency.target    modo emergencia"
echo "     systemd.unit=multi-user.target   sin GUI"
echo ""
echo "  4. Presionar Ctrl+X o F10 para arrancar"
echo ""
echo "Estos cambios son TEMPORALES — solo para este boot"
echo ""
echo "--- Alternativas legacy ---"
echo "linux /vmlinuz... single            rescue mode"
echo "linux /vmlinuz... 1                 rescue mode (runlevel 1)"
echo "linux /vmlinuz... 3                 multi-user (runlevel 3)"
echo "linux /vmlinuz... emergency         emergency mode"
'
```

### Paso 3.2: Reset de contrasena de root

```bash
docker compose exec debian-dev bash -c '
echo "=== Reset de contrasena de root ==="
echo ""
echo "Metodo con rd.break:"
echo ""
echo "1. En GRUB, editar y agregar: rd.break"
echo "   (rompe al initramfs, antes de montar root)"
echo ""
echo "2. Montar el filesystem real:"
echo "   mount -o remount,rw /sysroot"
echo ""
echo "3. Entrar al filesystem:"
echo "   chroot /sysroot"
echo ""
echo "4. Cambiar la contrasena:"
echo "   passwd root"
echo ""
echo "5. Si SELinux esta activo (RHEL):"
echo "   touch /.autorelabel"
echo "   (fuerza relabel completo en el proximo boot)"
echo ""
echo "6. Salir y reiniciar:"
echo "   exit"
echo "   exit"
echo ""
echo "NOTA: requiere acceso fisico a la maquina"
'
```

### Paso 3.3: Targets custom

```bash
docker compose exec debian-dev bash -c '
echo "=== Targets custom ==="
echo ""
echo "Se pueden crear targets para agrupar servicios de una app"
echo ""
echo "--- myapp.target ---"
cat > /tmp/lab-myapp.target << '\''EOF'\''
[Unit]
Description=My Application Stack
Requires=multi-user.target
After=multi-user.target
AllowIsolate=no
EOF
cat -n /tmp/lab-myapp.target
echo ""
echo "--- Servicios apuntan al target ---"
cat > /tmp/lab-myapp-api.service << '\''EOF'\''
[Unit]
Description=API Server
PartOf=lab-myapp.target

[Service]
Type=simple
ExecStart=/usr/bin/sleep infinity

[Install]
WantedBy=lab-myapp.target
EOF
echo "myapp-api.service: WantedBy=myapp.target"
echo ""
echo "--- Gestionar como grupo ---"
echo "systemctl start myapp.target    arranca todo el stack"
echo "systemctl stop myapp.target     detiene todo (con PartOf)"
echo ""
echo "--- Verificar ---"
systemd-analyze verify /tmp/lab-myapp.target 2>&1 || true
systemd-analyze verify /tmp/lab-myapp-api.service 2>&1 || true
rm /tmp/lab-myapp.target /tmp/lab-myapp-api.service
'
```

### Paso 3.4: Kernel command line

```bash
docker compose exec debian-dev bash -c '
echo "=== Kernel command line y systemd ==="
echo ""
echo "Parametros del kernel que afectan a systemd:"
echo ""
echo "systemd.unit=TARGET"
echo "  Arrancar en un target especifico (override temporal)"
echo ""
echo "systemd.mask=UNIT"
echo "  Enmascarar una unidad para este boot"
echo ""
echo "systemd.wants=UNIT"
echo "  Agregar una dependencia para este boot"
echo ""
echo "systemd.log_level=debug"
echo "  Mas verbosidad durante el boot"
echo ""
echo "rd.break"
echo "  Romper al initramfs (para reset de contrasena)"
echo ""
echo "init=/bin/bash"
echo "  Bypass completo de systemd — shell directo"
echo "  CUIDADO: sin systemd, casi nada funciona"
echo ""
echo "--- Verificar cmdline actual ---"
cat /proc/cmdline 2>/dev/null || echo "(no disponible en contenedor)"
'
```

---

## Limpieza final

No hay recursos que limpiar.

---

## Conceptos reforzados

1. `get-default` muestra el target de arranque (un symlink en
   `/etc/systemd/system/default.target`). `set-default` cambia
   el proximo boot, no la sesion actual.

2. `isolate` cambia al target inmediatamente, deteniendo unidades
   que no pertenecen al nuevo target. Es temporal — no persiste
   al reiniciar.

3. Solo los targets con `AllowIsolate=yes` permiten isolate:
   poweroff, rescue, emergency, multi-user, graphical, reboot.
   Los targets de sincronizacion no.

4. Desde GRUB, agregar `systemd.unit=rescue.target` al kernel
   cmdline para arrancar en modo rescate. `rd.break` rompe al
   initramfs para reset de contrasena.

5. Los targets custom agrupan servicios de una aplicacion.
   Con `PartOf=` en los servicios, `systemctl stop target`
   detiene todo el stack.

6. Equivalencias clave: `init 3` = `systemctl isolate
   multi-user.target`, `shutdown -h now` = `systemctl poweroff`.
