# T02 — Cambiar target

## Errata sobre los materiales originales

| # | Archivo | Línea | Error | Corrección |
|---|---------|-------|-------|------------|
| 1 | max.md | 161, 179 | Texto en chino "醒来" (×2) en secciones de suspend/hibernate | Reemplazado por "despertar" |
| 2 | max.md | 225 | "paraboot" — palabras unidas y Spanglish | Corregido: "para arrancar" |
| 3 | max.md | 248 | "Albootear" — palabras unidas y Spanglish | Corregido: "Al arrancar" |
| 4 | max.md | 361 | `runlevel` equiparado con `systemctl get-default + runlevel` — son conceptos distintos | `runlevel` muestra el estado actual; `get-default` muestra el target del próximo boot |

---

## get-default / set-default

El **default target** es el target que systemd arranca al boot. Internamente
es un symlink:

```bash
# Ver el target por defecto:
systemctl get-default
# graphical.target  (desktop)
# multi-user.target (servidor)

# Verificar el symlink:
ls -la /etc/systemd/system/default.target
# → /usr/lib/systemd/system/multi-user.target
```

```bash
# Cambiar el target por defecto (para el PRÓXIMO boot):
sudo systemctl set-default multi-user.target
# Removed /etc/systemd/system/default.target
# Created symlink /etc/systemd/system/default.target → .../multi-user.target

sudo systemctl set-default graphical.target
# Created symlink → .../graphical.target

# set-default cambia el PRÓXIMO boot, NO la sesión actual
```

```bash
# Caso típico: convertir un desktop en servidor
sudo systemctl set-default multi-user.target
sudo systemctl reboot
# Ahorra ~200-500MB de RAM (sin display manager, compositor, etc.)
```

---

## isolate — Cambio inmediato de target

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
# Aún más mínimo que rescue (filesystem read-only, sin sysinit)
```

### isolate vs set-default

| | `isolate` | `set-default` |
|---|---|---|
| Cuándo actúa | **Inmediatamente** | En el **próximo boot** |
| Persiste | **No** — al reiniciar vuelve al default | **Sí** — persiste entre reboots |
| Requisito | Target con `AllowIsolate=yes` | Cualquier target |
| Qué hace | Detiene unidades no relacionadas | Cambia symlink de `default.target` |

```bash
# Ejemplo: liberar recursos temporalmente sin cambiar el default
sudo systemctl isolate multi-user.target
# ... hacer trabajo pesado (más CPU/RAM disponible) ...
sudo systemctl isolate graphical.target
# La GUI vuelve sin reiniciar, el default no se tocó
```

### AllowIsolate

No todos los targets permiten `isolate`. Solo los que tienen
`AllowIsolate=yes` en su archivo de unidad:

```bash
# Ver cuáles permiten isolate:
grep -rl "AllowIsolate=yes" /usr/lib/systemd/system/*.target 2>/dev/null | \
    xargs -I {} basename {}
# poweroff.target
# halt.target
# rescue.target
# emergency.target
# multi-user.target
# graphical.target
# reboot.target
# (también los aliases runlevel{0-6}.target)

# Intentar isolate un target sin AllowIsolate:
sudo systemctl isolate network.target
# Operation refused, unit network.target may be requested only indirectly

# Los targets de sincronización (network, basic, sysinit, etc.)
# NO permiten isolate — son puntos de referencia, no estados del sistema
```

---

## Acciones del sistema

```bash
# APAGAR (desactiva todo y corta energía):
sudo systemctl poweroff
# Equivalente: shutdown -h now, init 0

# REINICIAR:
sudo systemctl reboot
# Equivalente: shutdown -r now, init 6

# HALT (detener sin apagar hardware — pantalla queda encendida):
sudo systemctl halt
# Equivalente: halt

# SUSPENDER (a RAM — bajo consumo, despertar rápido ~instantáneo):
sudo systemctl suspend
# El estado se mantiene en RAM (~5-15W)
# Despertar: botón de encendido, teclado, mouse

# HIBERNAR (a disco — se apaga completamente, consume 0W):
sudo systemctl hibernate
# El estado completo se guarda en swap
# Al encender: restaura exactamente donde estaba (~30 segundos)

# SUSPEND-THEN-HIBERNATE:
sudo systemctl suspend-then-hibernate
# Suspende primero; si no se despierta en un tiempo, hiberna automáticamente
```

```bash
# Forzar apagado/reinicio sin shutdown limpio:
sudo systemctl poweroff --force
sudo systemctl reboot --force
# Omite la detención ordenada de servicios — usar solo en emergencia
```

### shutdown programado

```bash
# Con delay:
sudo shutdown -h +10              # apagar en 10 minutos
sudo shutdown -r 23:00            # reiniciar a las 23:00
sudo shutdown -h now              # apagar ahora

# Cancelar un shutdown pendiente:
sudo shutdown -c

# Con mensaje a usuarios logueados (broadcast):
sudo shutdown -h +5 "Mantenimiento programado, guardar trabajo"
```

---

## Cambiar target desde GRUB

Cuando no se puede hacer login, el target se puede cambiar desde GRUB antes
de que el sistema arranque:

```bash
# 1. En el menú de GRUB, presionar 'e' para editar la entrada

# 2. Encontrar la línea que empieza con 'linux' (o 'linuxefi'):
#    linux /vmlinuz-6.x root=/dev/sda1 ro quiet

# 3. Agregar al FINAL de esa línea uno de:
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

### Otros parámetros del kernel para systemd

```bash
# Parámetros útiles en la línea del kernel:
systemd.unit=TARGET          # arrancar en un target específico
systemd.mask=UNIT            # enmascarar una unidad para este boot
systemd.wants=UNIT           # agregar una dependencia temporal
systemd.log_level=debug      # más verbosidad durante el boot
rd.break                     # romper al initramfs (para recovery)
init=/bin/bash                # bypass completo de systemd — shell directo
                              # (sin systemd, casi nada funciona)

# Ver cmdline del boot actual:
cat /proc/cmdline
```

### Reset de contraseña de root

```bash
# Método con rd.break (requiere acceso físico a la máquina):

# 1. En GRUB, editar la entrada y agregar: rd.break
#    al final de la línea linux

# 2. Se obtiene un shell en el initramfs (antes de montar el root real)

# 3. Montar el filesystem real como read-write:
mount -o remount,rw /sysroot

# 4. Entrar al filesystem:
chroot /sysroot

# 5. Cambiar la contraseña:
passwd root

# 6. Si SELinux está activo (RHEL, Fedora, AlmaLinux):
touch /.autorelabel
# Fuerza relabel completo de SELinux en el próximo boot (tarda varios minutos)

# 7. Salir y reiniciar:
exit          # sale del chroot
exit          # sale del initramfs → reinicia automáticamente
```

---

## Targets custom

Se pueden crear targets personalizados para agrupar servicios de una
aplicación:

```ini
# /etc/systemd/system/myapp.target
[Unit]
Description=My Application Stack
Requires=multi-user.target
After=multi-user.target

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
After=network.target

[Service]
Type=simple
ExecStart=/opt/myapp/bin/worker
Restart=on-failure

[Install]
WantedBy=myapp.target
```

```bash
# Gestionar toda la aplicación como un grupo:
sudo systemctl daemon-reload
sudo systemctl enable myapp-api.service myapp-worker.service

# Arrancar todo el stack:
sudo systemctl start myapp.target

# Detener todo (PartOf propaga el stop del target a los servicios):
sudo systemctl stop myapp.target

# Ver qué tiene el target:
systemctl list-dependencies myapp.target
```

> **Recordatorio** (de T03 Dependencias): `PartOf=` solo propaga
> stop/restart del target al servicio. El **start** lo causa `Wants=`
> (creado por `WantedBy=` en `[Install]` al hacer `enable`).

---

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
| `runlevel` | `runlevel` (sigue funcionando) |

> **Nota**: `runlevel` muestra el estado **actual** del sistema (en qué
> "runlevel" equivalente está). No confundir con `systemctl get-default`,
> que muestra el target configurado para el **próximo boot**.

---

## Ejercicios

### Ejercicio 1 — Consultar el default target

```bash
# 1. ¿Cuál es el target por defecto?
systemctl get-default

# 2. Verificar el symlink:
ls -la /etc/systemd/system/default.target 2>/dev/null

# 3. ¿Cuántas unidades dependen del default?
systemctl list-dependencies "$(systemctl get-default)" --plain --no-pager | wc -l
```

<details><summary>Predicción</summary>

- El default será `graphical.target` (desktop) o `multi-user.target`
  (servidor/contenedor)
- El symlink apuntará a `/usr/lib/systemd/system/<target>.target`
- La cantidad de dependencias será grande (50-150+) porque el default
  target arrastra toda la cadena: basic → sysinit → local-fs, más todos
  los servicios habilitados via `WantedBy=`

</details>

### Ejercicio 2 — Encontrar targets con AllowIsolate

```bash
for target in /usr/lib/systemd/system/*.target; do
    if grep -q "AllowIsolate=yes" "$target" 2>/dev/null; then
        echo "$(basename "$target")"
    fi
done
```

<details><summary>Predicción</summary>

- Aparecerán los targets de "estado del sistema": poweroff, halt, rescue,
  emergency, multi-user, graphical, reboot
- También aparecerán los aliases de runlevel: runlevel0-6.target
- **No** aparecerán targets de sincronización como network.target,
  basic.target, sysinit.target — estos son puntos de referencia, no
  estados completos a los que se pueda transicionar

</details>

### Ejercicio 3 — isolate vs set-default (teórico)

```bash
# Sin ejecutar — responder mentalmente:

# Escenario A: Estás en graphical.target y ejecutas:
#   sudo systemctl set-default multi-user.target
# Pregunta: ¿La GUI se cierra inmediatamente?

# Escenario B: Ejecutas:
#   sudo systemctl isolate multi-user.target
# Pregunta: ¿Al reiniciar, arrancará en multi-user o graphical?

# Verificar tu razonamiento:
systemctl get-default
runlevel
```

<details><summary>Predicción</summary>

- **Escenario A**: La GUI **NO** se cierra. `set-default` solo cambia el
  symlink para el próximo boot. La sesión actual sigue igual.
- **Escenario B**: Al reiniciar, arrancará en el target que diga
  `get-default` (que no fue cambiado por `isolate`). Si el default sigue
  siendo graphical, arrancará con GUI.
- Regla: `set-default` = persistente pero diferido. `isolate` = inmediato
  pero temporal.

</details>

### Ejercicio 4 — Inspeccionar archivos de targets terminales

```bash
# Ver el contenido real de los targets de apagado:
systemctl cat poweroff.target
systemctl cat reboot.target
systemctl cat shutdown.target
```

<details><summary>Predicción</summary>

- `poweroff.target` tendrá `DefaultDependencies=no` (porque es un target
  terminal — no queremos que él mismo tenga Conflicts con shutdown)
- Tendrá `Requires=systemd-poweroff.service` (el servicio que realmente
  ejecuta la llamada al kernel para apagar)
- `shutdown.target` es el punto de coordinación — los servicios normales
  (via DefaultDependencies=yes) declaran `Conflicts=shutdown.target` y
  `Before=shutdown.target`, lo que provoca su detención cuando shutdown
  se activa

</details>

### Ejercicio 5 — Comparar rescue y multi-user con isolate en mente

```bash
# Si hicieras isolate rescue.target, ¿qué servicios se detendrían?
# Comparar las dependencias:
diff <(systemctl list-dependencies rescue.target --plain --no-pager | sort) \
     <(systemctl list-dependencies multi-user.target --plain --no-pager | sort) \
     | grep "^>" | head -20
```

<details><summary>Predicción</summary>

- Las líneas con `>` (solo en multi-user) son los servicios que se
  detendrían al hacer `isolate rescue.target`
- Serán muchos: sshd, cron, servicios de red, bases de datos, etc.
- rescue.target solo necesita sysinit.target y rescue.service
- Esto confirma por qué isolate a rescue es disruptivo — detiene
  prácticamente todo excepto lo básico del sistema

</details>

### Ejercicio 6 — shutdown programado

```bash
# 1. Programar un shutdown (sin ejecutar realmente en producción):
echo "Comando: sudo shutdown -h +10 'Mantenimiento en 10 minutos'"

# 2. ¿Qué archivo crea un shutdown pendiente?
ls /run/systemd/shutdown/ 2>/dev/null || echo "(no hay shutdown pendiente)"

# 3. ¿Cómo cancelar?
echo "Comando: sudo shutdown -c"
```

<details><summary>Predicción</summary>

- `shutdown -h +10` crea un archivo en `/run/systemd/shutdown/scheduled`
  con la hora programada y envía un broadcast a todos los usuarios logueados
- `shutdown -c` cancela eliminando ese archivo y enviando otro broadcast
  diciendo que se canceló
- Si no hay shutdown pendiente, `/run/systemd/shutdown/` estará vacío o
  no existirá

</details>

### Ejercicio 7 — Parámetros del kernel

```bash
# Ver la línea de comandos del kernel actual:
cat /proc/cmdline

# Buscar si hay algún override de systemd:
grep -o "systemd\.[a-z_]*=[^ ]*" /proc/cmdline 2>/dev/null || \
    echo "No hay overrides de systemd en cmdline"
```

<details><summary>Predicción</summary>

- `/proc/cmdline` mostrará algo como: `BOOT_IMAGE=/vmlinuz-6.x
  root=UUID=... ro quiet splash`
- En un boot normal, no habrá parámetros `systemd.unit=` ni `systemd.mask=`
- Si se arrancó con un override desde GRUB, aparecería aquí
- En un contenedor Docker, puede no existir o mostrar la cmdline del host

</details>

### Ejercicio 8 — Simular target custom (sin crear archivos)

```bash
# Diseñar mentalmente un target para una app web con 3 componentes:
# - frontend (nginx)
# - backend (API Node.js)
# - worker (procesador de colas)

# ¿Qué directivas usarías en cada archivo?
cat << 'EOF'
# webapp.target
[Unit]
Description=Web Application Stack
Requires=multi-user.target
After=multi-user.target

[Install]
WantedBy=multi-user.target

# webapp-frontend.service → [Install] WantedBy=webapp.target
# webapp-backend.service  → [Install] WantedBy=webapp.target
#                            [Unit]   PartOf=webapp.target
# webapp-worker.service   → [Install] WantedBy=webapp.target
#                            [Unit]   PartOf=webapp.target
EOF

# ¿Por qué frontend NO tiene PartOf pero backend y worker sí?
```

<details><summary>Predicción</summary>

- `PartOf=webapp.target` en backend y worker significa que al hacer
  `systemctl stop webapp.target`, estos se detienen automáticamente
- Si frontend (nginx) también sirve otros sitios, podría no querer
  detenerse con el target — por eso no tiene `PartOf`
- El `WantedBy=webapp.target` en todos garantiza que al hacer
  `systemctl start webapp.target`, los tres arrancan
- Todos necesitarían `After=network.target` en su `[Unit]` para
  esperar a que la red esté lista

</details>

### Ejercicio 9 — Equivalencias SysVinit

```bash
# Traducir estos comandos SysVinit a systemd:
echo "1. init 0              → ?"
echo "2. init 6              → ?"
echo "3. telinit 3           → ?"
echo "4. shutdown -r +15     → ?"
echo "5. shutdown -h now     → ?"
```

<details><summary>Predicción</summary>

```
1. init 0              → systemctl poweroff
2. init 6              → systemctl reboot
3. telinit 3           → systemctl isolate multi-user.target
4. shutdown -r +15     → shutdown -r +15  (sigue funcionando igual)
5. shutdown -h now     → systemctl poweroff
```

- Los comandos `init` y `telinit` siguen funcionando por compatibilidad
  (systemd los intercepta)
- `shutdown` también sigue funcionando — no hay un reemplazo obligatorio
- La diferencia es que los comandos `systemctl` son la forma "nativa" de
  systemd y ofrecen más control

</details>

### Ejercicio 10 — Diagnóstico: sistema arranca sin GUI

Un servidor que debería arrancar con GUI arranca siempre en modo texto.
¿Cómo diagnosticarlo?

```bash
# 1. ¿Cuál es el default target?
systemctl get-default

# 2. ¿Está graphical.target disponible?
systemctl list-unit-files graphical.target --no-pager

# 3. ¿El display manager está habilitado?
systemctl list-unit-files --type=service --no-pager | grep -i -E "gdm|sddm|lightdm|display"

# 4. ¿Hay algún error en el display manager?
systemctl status display-manager.service 2>/dev/null
```

<details><summary>Predicción</summary>

Las causas más probables, en orden:

1. **`get-default` devuelve `multi-user.target`** — alguien hizo
   `set-default multi-user.target`. Solución: `sudo systemctl set-default
   graphical.target`
2. **graphical.target es el default pero el display manager no está
   habilitado** — `systemctl enable gdm.service` (o sddm/lightdm)
3. **El display manager falla al arrancar** — `systemctl status
   display-manager.service` y `journalctl -u display-manager` para ver
   el error (driver de GPU, config de X11, etc.)
4. **graphical.target está masked** — `systemctl unmask graphical.target`

</details>
