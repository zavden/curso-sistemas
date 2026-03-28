# T02 — Crear un timer

## Estructura mínima

Un timer necesita dos archivos: el `.timer` y el `.service` que activa.

### El service (qué ejecutar)

```ini
# /etc/systemd/system/myapp-backup.service
[Unit]
Description=Backup de myapp

[Service]
Type=oneshot
ExecStart=/opt/myapp/bin/backup.sh
```

```bash
# Type=oneshot es el más común para tareas programadas:
# - Ejecuta el comando
# - Espera a que termine
# - Reporta éxito o fallo
# - No queda un proceso residente

# NO necesita [Install] — el timer se encarga de activarlo
```

### El timer (cuándo ejecutar)

```ini
# /etc/systemd/system/myapp-backup.timer
[Unit]
Description=Timer para backup de myapp

[Timer]
OnCalendar=*-*-* 03:00:00
Persistent=true

[Install]
WantedBy=timers.target
```

```bash
# El [Install] va en el TIMER, no en el service
# WantedBy=timers.target hace que el timer se active en el boot
```

### Activar el timer

```bash
# Recargar la configuración:
sudo systemctl daemon-reload

# Habilitar y arrancar el timer:
sudo systemctl enable --now myapp-backup.timer

# Verificar:
systemctl status myapp-backup.timer
# Active: active (waiting)
# Trigger: Wed 2026-03-18 03:00:00 UTC; 9h left

systemctl list-timers myapp-backup.timer --no-pager
# NEXT                        LEFT    UNIT                  ACTIVATES
# Wed 2026-03-18 03:00:00 UTC 9h left myapp-backup.timer    myapp-backup.service
```

### Ejecutar manualmente

```bash
# Ejecutar el service directamente (sin esperar al timer):
sudo systemctl start myapp-backup.service

# Verificar el resultado:
systemctl status myapp-backup.service
journalctl -u myapp-backup.service --no-pager -n 20
```

## Relación timer ↔ service

### Nombre por defecto

```bash
# Si el timer y el service tienen el MISMO nombre base,
# la asociación es automática:
# myapp.timer → activa → myapp.service

# No necesitas especificar Unit= en el timer
```

### Nombre diferente con Unit=

```bash
# Si el service tiene un nombre diferente:
# [Timer]
# Unit=otro-service.service

# Ejemplo:
# cleanup.timer → activa → maintenance-cleanup.service
```

```ini
# /etc/systemd/system/cleanup.timer
[Unit]
Description=Timer de limpieza

[Timer]
OnCalendar=weekly
Unit=maintenance-cleanup.service

[Install]
WantedBy=timers.target
```

### Un timer, múltiples ejecuciones

```bash
# Si el timer dispara mientras el service anterior aún está corriendo:
# Comportamiento por defecto: se IGNORA el nuevo disparo
# El service tiene que terminar antes de que se pueda ejecutar de nuevo

# Para permitir múltiples instancias simultáneas:
# Usar un service template (no es lo habitual para tareas programadas)
```

## Persistent timers

```bash
# Persistent=true hace que systemd registre la última ejecución
# y ejecute inmediatamente si se perdió una ejecución:

# [Timer]
# OnCalendar=daily
# Persistent=true

# Escenario:
# 1. Timer programado para las 03:00
# 2. El equipo estuvo apagado de 02:00 a 08:00
# 3. Al encender a las 08:00, systemd detecta que se perdió la ejecución
# 4. Ejecuta inmediatamente

# El timestamp de última ejecución se guarda en:
# /var/lib/systemd/timers/
ls /var/lib/systemd/timers/
# stamp-fstrim.timer
# stamp-logrotate.timer
# stamp-myapp-backup.timer

# Cada archivo contiene el timestamp de la última ejecución
```

```bash
# Sin Persistent=true:
# Si el equipo estaba apagado durante la ejecución programada,
# la tarea simplemente se salta hasta la próxima vez

# Cuándo usar Persistent=true:
# - Backups (no quieres saltarte un backup)
# - Rotación de logs
# - Cualquier tarea que DEBE ejecutarse al menos una vez por período

# Cuándo NO usar Persistent=true:
# - Verificaciones de salud (importa el estado actual, no el pasado)
# - Tareas que dependen de condiciones externas (hora exacta importa)
```

## Ejemplo completo: Backup con notificación

### El service

```ini
# /etc/systemd/system/myapp-backup.service
[Unit]
Description=Backup diario de myapp
After=network-online.target postgresql.service
Wants=network-online.target

[Service]
Type=oneshot
User=myappuser
Group=myappuser

ExecStart=/opt/myapp/bin/backup.sh

# Control de recursos:
Nice=10
IOSchedulingClass=idle
MemoryMax=512M
CPUQuota=50%

# Timeout (por si el backup se cuelga):
TimeoutStartSec=3600

# Seguridad:
ProtectSystem=strict
ReadWritePaths=/opt/myapp/backups /var/log/myapp
PrivateTmp=true
NoNewPrivileges=true

# Notificación de fallo:
ExecStartPost=/bin/bash -c 'logger -t myapp-backup "Backup completado exitosamente"'
```

### El timer

```ini
# /etc/systemd/system/myapp-backup.timer
[Unit]
Description=Timer para backup diario de myapp

[Timer]
OnCalendar=*-*-* 03:00:00
RandomizedDelaySec=30min
Persistent=true
AccuracySec=1min

[Install]
WantedBy=timers.target
```

### Notificación de fallos con OnFailure

```ini
# /etc/systemd/system/myapp-backup.service
[Unit]
Description=Backup diario de myapp
OnFailure=notify-failure@%n.service
# %n = nombre de la unidad (myapp-backup.service)
```

```ini
# /etc/systemd/system/notify-failure@.service
[Unit]
Description=Notificar fallo de %i

[Service]
Type=oneshot
ExecStart=/usr/local/bin/notify-failure.sh %i
```

```bash
# /usr/local/bin/notify-failure.sh
#!/bin/bash
UNIT="$1"
echo "FALLO: $UNIT en $(hostname) a las $(date)" | \
    mail -s "Fallo: $UNIT" ops@example.com
```

## Ejemplo completo: Limpieza periódica

```ini
# /etc/systemd/system/cleanup-tmp.service
[Unit]
Description=Limpiar archivos temporales

[Service]
Type=oneshot
ExecStart=/usr/bin/find /tmp -type f -mtime +7 -delete
ExecStart=/usr/bin/find /var/tmp -type f -mtime +30 -delete
# Múltiples ExecStart en oneshot: se ejecutan secuencialmente
```

```ini
# /etc/systemd/system/cleanup-tmp.timer
[Unit]
Description=Timer de limpieza semanal

[Timer]
OnCalendar=Sun *-*-* 04:00:00
Persistent=true

[Install]
WantedBy=timers.target
```

## Ejemplo completo: Health check frecuente

```ini
# /etc/systemd/system/health-check.service
[Unit]
Description=Verificación de salud de la aplicación

[Service]
Type=oneshot
ExecStart=/opt/myapp/bin/health-check.sh
# Sin Persistent — si se pierde una verificación, no importa
```

```ini
# /etc/systemd/system/health-check.timer
[Unit]
Description=Health check cada 5 minutos

[Timer]
OnCalendar=*-*-* *:00/5:00
AccuracySec=10s

[Install]
WantedBy=timers.target
```

## Timers transient (sin archivos)

```bash
# systemd-run puede crear timers temporales sin crear archivos:
sudo systemd-run --on-active=30min /usr/local/bin/task.sh
# Crea un timer que ejecuta en 30 minutos y se destruye después

sudo systemd-run --on-calendar="*-*-* 03:00" /usr/local/bin/backup.sh
# Crea un timer que ejecuta a las 03:00

# Con nombre personalizado:
sudo systemd-run --unit=my-task --on-active=1h /usr/local/bin/task.sh

# Listar timers transient:
systemctl list-timers --no-pager | grep run-

# Los timers transient NO sobreviven un reboot
# Para tareas persistentes, crear archivos .timer + .service
```

## User timers

```bash
# Los usuarios pueden crear timers sin privilegios de root:
mkdir -p ~/.config/systemd/user/
```

```ini
# ~/.config/systemd/user/reminder.service
[Unit]
Description=Recordatorio

[Service]
Type=oneshot
ExecStart=/usr/bin/notify-send "Tomar un descanso"
```

```ini
# ~/.config/systemd/user/reminder.timer
[Unit]
Description=Recordatorio cada 2 horas

[Timer]
OnCalendar=*-*-* 09..17/2:00:00
Persistent=true

[Install]
WantedBy=timers.target
```

```bash
# Gestionar user timers:
systemctl --user daemon-reload
systemctl --user enable --now reminder.timer
systemctl --user list-timers --no-pager

# Los user timers solo corren cuando el usuario tiene sesión activa
# Para que corran sin sesión:
loginctl enable-linger username
```

## Gestión de timers

```bash
# Ver todos los timers:
systemctl list-timers --all --no-pager

# Estado de un timer:
systemctl status myapp-backup.timer

# Habilitar/deshabilitar:
sudo systemctl enable myapp-backup.timer     # arrancar en boot
sudo systemctl disable myapp-backup.timer    # no arrancar en boot

# Arrancar/parar:
sudo systemctl start myapp-backup.timer      # activar ahora
sudo systemctl stop myapp-backup.timer       # desactivar ahora

# Ejecutar el service manualmente (sin esperar al timer):
sudo systemctl start myapp-backup.service

# Ver los logs del service:
journalctl -u myapp-backup.service --no-pager -n 20

# Ver cuándo fue la última ejecución:
systemctl show myapp-backup.timer --property=LastTriggerUSec
```

## Migrar de cron a timers

```bash
# Cron original:
# 0 3 * * * /opt/myapp/bin/backup.sh >> /var/log/backup.log 2>&1

# Equivalente en timer:
# 1. No necesitas redirigir a log — journal captura todo
# 2. La expresión cron "0 3 * * *" equivale a OnCalendar=*-*-* 03:00:00
# 3. La redirección "2>&1" no es necesaria — journal captura stdout y stderr
```

| Cron | OnCalendar |
|---|---|
| `* * * * *` | `*-*-* *:*:00` o `minutely` |
| `0 * * * *` | `*-*-* *:00:00` o `hourly` |
| `0 0 * * *` | `*-*-* 00:00:00` o `daily` |
| `0 0 * * 0` | `Sun *-*-* 00:00:00` o `weekly` |
| `0 0 1 * *` | `*-*-01 00:00:00` o `monthly` |
| `*/5 * * * *` | `*-*-* *:00/5:00` |
| `0 9-17 * * 1-5` | `Mon..Fri *-*-* 09..17:00:00` |
| `30 2 * * *` | `*-*-* 02:30:00` |

---

## Ejercicios

### Ejercicio 1 — Explorar timers existentes

```bash
# 1. Listar todos los timers:
systemctl list-timers --all --no-pager

# 2. Examinar un timer y su service asociado:
TIMER=$(systemctl list-timers --no-pager | awk 'NR==2{print $(NF-1)}')
echo "=== Timer ===" && systemctl cat "$TIMER" 2>/dev/null
SERVICE=$(systemctl list-timers --no-pager | awk 'NR==2{print $NF}')
echo "=== Service ===" && systemctl cat "$SERVICE" 2>/dev/null
```

### Ejercicio 2 — Verificar expresiones

```bash
# Convertir estas expresiones cron a OnCalendar y verificar:
# a) 0 6 * * 1-5   (lun-vie a las 06:00)
systemd-analyze calendar "Mon..Fri *-*-* 06:00" --iterations=3

# b) */15 * * * *   (cada 15 minutos)
systemd-analyze calendar "*-*-* *:00/15:00" --iterations=5

# c) 0 0 1,15 * *   (días 1 y 15 a medianoche)
systemd-analyze calendar "*-*-01,15 00:00:00" --iterations=5
```

### Ejercicio 3 — Timer transient

```bash
# Crear un timer que se ejecute en 1 minuto:
sudo systemd-run --on-active=1min /bin/bash -c 'echo "Timer transient: $(date)" >> /tmp/timer-test.txt'

# Verificar que se creó:
systemctl list-timers --no-pager | grep run-

# Esperar 1 minuto y verificar:
# cat /tmp/timer-test.txt
```
