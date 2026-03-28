# Lab — Crear un timer

## Objetivo

Crear systemd timers: la estructura minima (service Type=oneshot +
timer con OnCalendar), la relacion timer-service (nombre o Unit=),
activacion con enable --now, Persistent=true para tareas perdidas,
ejemplos completos (backup con hardening, limpieza, health check),
timers transient con systemd-run, user timers, gestion del ciclo
de vida, y migracion de cron a timers.

## Prerequisitos

- Entorno del curso levantado (`docker compose up -d`)

---

## Parte 1 — Estructura y activacion

### Objetivo

Entender la estructura minima de un timer y como activarlo.

### Paso 1.1: El service (que ejecutar)

```bash
docker compose exec debian-dev bash -c '
echo "=== El service ==="
echo ""
echo "--- Estructura minima ---"
cat << '\''EOF'\''
# /etc/systemd/system/myapp-backup.service
[Unit]
Description=Backup de myapp

[Service]
Type=oneshot
ExecStart=/opt/myapp/bin/backup.sh
EOF
echo ""
echo "--- Type=oneshot ---"
echo "  Ejecuta el comando, espera a que termine"
echo "  Reporta exito o fallo"
echo "  No queda un proceso residente"
echo "  El mas comun para tareas programadas"
echo ""
echo "--- NO necesita [Install] ---"
echo "  El timer se encarga de activarlo"
echo "  [Install] va en el TIMER, no en el service"
'
```

### Paso 1.2: El timer (cuando ejecutar)

```bash
docker compose exec debian-dev bash -c '
echo "=== El timer ==="
echo ""
echo "--- Estructura minima ---"
cat << '\''EOF'\''
# /etc/systemd/system/myapp-backup.timer
[Unit]
Description=Timer para backup de myapp

[Timer]
OnCalendar=*-*-* 03:00:00
Persistent=true

[Install]
WantedBy=timers.target
EOF
echo ""
echo "--- WantedBy=timers.target ---"
echo "  Hace que el timer se active en el boot"
echo "  Al hacer enable, crea symlink en timers.target.wants/"
echo ""
echo "--- Persistent=true ---"
echo "  Si el equipo estaba apagado a las 03:00"
echo "  Ejecuta al encender"
echo "  Timestamp en /var/lib/systemd/timers/"
'
```

### Paso 1.3: Activar y verificar

```bash
docker compose exec debian-dev bash -c '
echo "=== Activar un timer ==="
echo ""
echo "--- Paso 1: daemon-reload ---"
echo "sudo systemctl daemon-reload"
echo ""
echo "--- Paso 2: enable + start ---"
echo "sudo systemctl enable --now myapp-backup.timer"
echo ""
echo "--- Paso 3: verificar ---"
echo "systemctl status myapp-backup.timer"
echo "  Active: active (waiting)"
echo "  Trigger: Wed 2026-03-18 03:00:00 UTC; 9h left"
echo ""
echo "systemctl list-timers myapp-backup.timer --no-pager"
echo "  NEXT    LEFT    UNIT                 ACTIVATES"
echo "  Wed...  9h left myapp-backup.timer   myapp-backup.service"
echo ""
echo "--- Ejecutar manualmente ---"
echo "sudo systemctl start myapp-backup.service"
echo "  Ejecuta el service sin esperar al timer"
echo ""
echo "--- Verificar ---"
echo "systemctl status myapp-backup.service"
echo "journalctl -u myapp-backup.service --no-pager -n 20"
'
```

### Paso 1.4: Relacion timer-service

```bash
docker compose exec debian-dev bash -c '
echo "=== Relacion timer ↔ service ==="
echo ""
echo "--- Por nombre (automatico) ---"
echo "myapp.timer → activa → myapp.service"
echo "  No necesitas especificar Unit="
echo ""
echo "--- Nombre diferente (Unit=) ---"
cat << '\''EOF'\''
# cleanup.timer → activa → maintenance-cleanup.service
[Timer]
OnCalendar=weekly
Unit=maintenance-cleanup.service
EOF
echo ""
echo "--- Validar con systemd-analyze verify ---"
echo "systemd-analyze verify /etc/systemd/system/myapp-backup.timer"
echo "systemd-analyze verify /etc/systemd/system/myapp-backup.service"
echo ""
echo "--- Explorar timers existentes ---"
for timer in $(systemctl list-unit-files --type=timer --no-pager --no-legend 2>/dev/null | awk "{print \$1}" | head -5); do
    echo "Timer: $timer"
    systemctl cat "$timer" 2>/dev/null | grep -E "^(OnCalendar|OnBoot|Persistent|Unit)" || true
    echo ""
done
[[ -z "$timer" ]] && echo "(sin timers instalados)"
'
```

---

## Parte 2 — Persistent y ejemplos

### Objetivo

Entender Persistent=true y ver ejemplos completos de timers.

### Paso 2.1: Persistent timers

```bash
docker compose exec debian-dev bash -c '
echo "=== Persistent timers ==="
echo ""
echo "--- Sin Persistent= ---"
echo "  Si apagado durante la ejecucion programada"
echo "  La tarea se salta hasta la proxima vez"
echo ""
echo "--- Con Persistent=true ---"
echo "  systemd registra la ultima ejecucion"
echo "  Si se perdio una ejecucion, ejecuta al encender"
echo ""
echo "  Timestamps en: /var/lib/systemd/timers/"
ls /var/lib/systemd/timers/ 2>/dev/null || \
    echo "  (directorio no encontrado)"
echo ""
echo "--- Cuando usar Persistent=true ---"
echo "  SI: backups, rotacion de logs, tareas que DEBEN ejecutarse"
echo "  NO: health checks (importa el estado actual, no el pasado)"
echo ""
echo "--- Ver ultima ejecucion ---"
echo "systemctl show myapp-backup.timer --property=LastTriggerUSec"
'
```

### Paso 2.2: Ejemplo — Backup con hardening

```bash
docker compose exec debian-dev bash -c '
echo "=== Ejemplo: backup con hardening ==="
echo ""
echo "--- Service ---"
cat << '\''EOF'\''
# /etc/systemd/system/myapp-backup.service
[Unit]
Description=Backup diario de myapp
After=network-online.target
Wants=network-online.target

[Service]
Type=oneshot
User=myappuser
Group=myappuser
ExecStart=/opt/myapp/bin/backup.sh

Nice=10
IOSchedulingClass=idle
MemoryMax=512M
CPUQuota=50%
TimeoutStartSec=3600

ProtectSystem=strict
ReadWritePaths=/opt/myapp/backups
PrivateTmp=true
NoNewPrivileges=true
EOF
echo ""
echo "--- Timer ---"
cat << '\''EOF'\''
# /etc/systemd/system/myapp-backup.timer
[Unit]
Description=Timer para backup diario

[Timer]
OnCalendar=*-*-* 03:00:00
RandomizedDelaySec=30min
Persistent=true
AccuracySec=1min

[Install]
WantedBy=timers.target
EOF
'
```

### Paso 2.3: Ejemplo — Limpieza y health check

```bash
docker compose exec debian-dev bash -c '
echo "=== Ejemplo: limpieza semanal ==="
echo ""
cat << '\''EOF'\''
# cleanup-tmp.service
[Service]
Type=oneshot
ExecStart=/usr/bin/find /tmp -type f -mtime +7 -delete
ExecStart=/usr/bin/find /var/tmp -type f -mtime +30 -delete

# cleanup-tmp.timer
[Timer]
OnCalendar=Sun *-*-* 04:00:00
Persistent=true
[Install]
WantedBy=timers.target
EOF
echo ""
echo "Nota: multiples ExecStart en oneshot se ejecutan secuencialmente"
echo ""
echo "=== Ejemplo: health check frecuente ==="
echo ""
cat << '\''EOF'\''
# health-check.service
[Service]
Type=oneshot
ExecStart=/opt/myapp/bin/health-check.sh

# health-check.timer
[Timer]
OnCalendar=*-*-* *:00/5:00
AccuracySec=10s
[Install]
WantedBy=timers.target
EOF
echo ""
echo "Sin Persistent — si se pierde un check, no importa"
'
```

### Paso 2.4: OnFailure para notificaciones

```bash
docker compose exec debian-dev bash -c '
echo "=== Notificacion de fallos ==="
echo ""
echo "--- En el service ---"
echo "[Unit]"
echo "Description=Backup diario"
echo "OnFailure=notify-failure@%n.service"
echo "  %n = nombre de la unidad (myapp-backup.service)"
echo ""
echo "--- Service template para notificacion ---"
cat << '\''EOF'\''
# /etc/systemd/system/notify-failure@.service
[Unit]
Description=Notificar fallo de %i

[Service]
Type=oneshot
ExecStart=/usr/local/bin/notify-failure.sh %i
EOF
echo ""
echo "--- Script de notificacion ---"
echo "#!/bin/bash"
echo "UNIT=\"\$1\""
echo "echo \"FALLO: \$UNIT en \$(hostname)\" | \\"
echo "    mail -s \"Fallo: \$UNIT\" ops@example.com"
'
```

---

## Parte 3 — Transient, user timers y migracion

### Objetivo

Usar timers transient, user timers y migrar de cron a timers.

### Paso 3.1: Timers transient (sin archivos)

```bash
docker compose exec debian-dev bash -c '
echo "=== Timers transient ==="
echo ""
echo "systemd-run crea timers temporales sin crear archivos:"
echo ""
echo "--- Ejecutar en N tiempo ---"
echo "systemd-run --on-active=30min /usr/local/bin/task.sh"
echo "  Timer que ejecuta en 30 minutos y se destruye"
echo ""
echo "--- Ejecutar a una hora ---"
echo "systemd-run --on-calendar=\"*-*-* 03:00\" /usr/local/bin/backup.sh"
echo ""
echo "--- Con nombre ---"
echo "systemd-run --unit=my-task --on-active=1h /usr/local/bin/task.sh"
echo ""
echo "--- Listar transient ---"
echo "systemctl list-timers --no-pager | grep run-"
echo ""
echo "--- Intentar ---"
systemd-run --on-active=1h --unit=lab-test \
    /bin/echo "timer transient test" 2>&1 || \
    echo "(systemd-run puede no funcionar en contenedores)"
echo ""
echo "Los timers transient NO sobreviven un reboot."
echo "Para persistencia, crear archivos .timer + .service."
'
```

### Paso 3.2: User timers

```bash
docker compose exec debian-dev bash -c '
echo "=== User timers ==="
echo ""
echo "Usuarios pueden crear timers sin root:"
echo ""
echo "--- Ubicacion ---"
echo "~/.config/systemd/user/"
echo ""
echo "--- Ejemplo: recordatorio ---"
cat << '\''EOF'\''
# ~/.config/systemd/user/reminder.service
[Service]
Type=oneshot
ExecStart=/usr/bin/notify-send "Tomar un descanso"

# ~/.config/systemd/user/reminder.timer
[Timer]
OnCalendar=*-*-* 09..17/2:00:00
Persistent=true
[Install]
WantedBy=timers.target
EOF
echo ""
echo "--- Gestion ---"
echo "systemctl --user daemon-reload"
echo "systemctl --user enable --now reminder.timer"
echo "systemctl --user list-timers --no-pager"
echo ""
echo "--- Sesion ---"
echo "User timers solo corren con sesion activa."
echo "Para correr sin sesion:"
echo "  loginctl enable-linger username"
'
```

### Paso 3.3: Migracion cron → timers

```bash
docker compose exec debian-dev bash -c '
echo "=== Migracion cron → timers ==="
echo ""
echo "--- Cron original ---"
echo "0 3 * * * /opt/myapp/bin/backup.sh >> /var/log/backup.log 2>&1"
echo ""
echo "--- Equivalente en timer ---"
echo "1. No necesitas redirigir a log (journal captura todo)"
echo "2. La expresion \"0 3 * * *\" = OnCalendar=*-*-* 03:00:00"
echo "3. La redireccion 2>&1 no es necesaria"
echo ""
echo "--- Tabla de conversion ---"
echo "| Cron             | OnCalendar                   |"
echo "|------------------|------------------------------|"
echo "| * * * * *        | minutely                     |"
echo "| 0 * * * *        | hourly                       |"
echo "| 0 0 * * *        | daily                        |"
echo "| 0 0 * * 0        | weekly                       |"
echo "| 0 0 1 * *        | monthly                      |"
echo "| */5 * * * *      | *-*-* *:00/5:00              |"
echo "| 0 9-17 * * 1-5   | Mon..Fri *-*-* 09..17:00:00  |"
echo "| 30 2 * * *       | *-*-* 02:30:00               |"
'
```

### Paso 3.4: Gestion del ciclo de vida

```bash
docker compose exec debian-dev bash -c '
echo "=== Gestion de timers ==="
echo ""
echo "--- Ver todos ---"
echo "systemctl list-timers --all --no-pager"
echo ""
echo "--- Estado ---"
echo "systemctl status myapp-backup.timer"
echo ""
echo "--- Habilitar/deshabilitar ---"
echo "systemctl enable myapp-backup.timer     arrancar en boot"
echo "systemctl disable myapp-backup.timer    no arrancar en boot"
echo ""
echo "--- Arrancar/parar ---"
echo "systemctl start myapp-backup.timer      activar ahora"
echo "systemctl stop myapp-backup.timer       desactivar ahora"
echo ""
echo "--- Ejecutar el service manualmente ---"
echo "systemctl start myapp-backup.service"
echo ""
echo "--- Ver logs ---"
echo "journalctl -u myapp-backup.service --no-pager -n 20"
echo ""
echo "--- Ver ultima ejecucion ---"
echo "systemctl show myapp-backup.timer --property=LastTriggerUSec"
'
```

---

## Limpieza final

No hay recursos que limpiar.

---

## Conceptos reforzados

1. Un timer necesita dos archivos: .service (Type=oneshot, sin
   [Install]) y .timer (OnCalendar, Persistent, [Install]
   WantedBy=timers.target).

2. Persistent=true registra la ultima ejecucion en
   /var/lib/systemd/timers/ y ejecuta tareas perdidas al encender.

3. El service puede incluir hardening (ProtectSystem, PrivateTmp),
   control de recursos (CPUQuota, MemoryMax), y notificacion de
   fallos (OnFailure).

4. `systemd-run --on-active=` crea timers transient sin archivos.
   No sobreviven reboot. Para persistencia, crear archivos.

5. User timers van en ~/.config/systemd/user/ y se gestionan con
   `systemctl --user`. Requieren `loginctl enable-linger` para
   correr sin sesion.

6. Migracion de cron: eliminar redireccion a log (journal lo
   captura), convertir expresion cron a OnCalendar, agregar
   Persistent=true si la tarea no debe perderse.
