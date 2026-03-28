# T02 — Crear un timer

## Erratas detectadas en el material base

| Archivo | Línea(s) | Error | Corrección |
|---|---|---|---|
| README.md | 200–201 | Comentario `# Notificación de fallo:` seguido de `ExecStartPost=...logger "Backup completado exitosamente"` | `ExecStartPost` solo se ejecuta **tras el éxito** de `ExecStart`, no en caso de fallo. El comentario debería decir "Notificación de éxito" o "Logging post-ejecución". La notificación de fallos se gestiona con `OnFailure=` (correctamente documentado más abajo en el mismo archivo). |

---

## Estructura mínima

Un timer necesita **dos archivos**: el `.service` (qué ejecutar) y el
`.timer` (cuándo ejecutar).

### El service

```ini
# /etc/systemd/system/myapp-backup.service
[Unit]
Description=Backup de myapp

[Service]
Type=oneshot
ExecStart=/opt/myapp/bin/backup.sh
```

- **`Type=oneshot`** es el más común para tareas programadas: ejecuta el
  comando, espera a que termine, reporta éxito o fallo, y no deja proceso
  residente.
- **No necesita `[Install]`** — el timer se encarga de activarlo.

### El timer

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

- **`[Install]` va en el timer**, no en el service.
- `WantedBy=timers.target` hace que el timer se active en el boot.

### Activar el timer

```bash
# 1. Recargar configuración:
sudo systemctl daemon-reload

# 2. Habilitar y arrancar:
sudo systemctl enable --now myapp-backup.timer

# 3. Verificar:
systemctl status myapp-backup.timer
# Active: active (waiting)
# Trigger: Wed 2026-03-18 03:00:00 UTC; 9h left

systemctl list-timers myapp-backup.timer --no-pager
```

### Ejecutar manualmente (sin esperar al timer)

```bash
sudo systemctl start myapp-backup.service

# Verificar resultado:
systemctl status myapp-backup.service
journalctl -u myapp-backup.service --no-pager -n 20
```

---

## Relación timer ↔ service

### Asociación por nombre (automática)

Si ambos archivos comparten el mismo nombre base, la asociación es automática:

```
myapp-backup.timer  →  activa  →  myapp-backup.service
```

No se necesita especificar `Unit=` en el timer.

### Nombre diferente con Unit=

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

`cleanup.timer` activa `maintenance-cleanup.service` en vez de
`cleanup.service`.

### Si el timer dispara mientras el service aún corre

Comportamiento por defecto: el nuevo disparo se **ignora**. El service debe
terminar antes de poder ejecutarse de nuevo. Para tareas programadas esto es
lo deseable — evita ejecuciones superpuestas.

---

## Persistent timers

`Persistent=true` hace que systemd registre la última ejecución y ejecute
inmediatamente si se perdió una:

```ini
[Timer]
OnCalendar=daily
Persistent=true
```

**Escenario:**
1. Timer programado para las 03:00
2. El equipo estuvo apagado de 02:00 a 08:00
3. Al encender a las 08:00, systemd detecta la ejecución perdida
4. Ejecuta inmediatamente

```bash
# Los timestamps se guardan en:
ls /var/lib/systemd/timers/
# stamp-fstrim.timer
# stamp-logrotate.timer
# stamp-myapp-backup.timer

# Consultar última ejecución:
systemctl show myapp-backup.timer --property=LastTriggerUSec
```

### Cuándo usar Persistent=true

| Usar | No usar |
|---|---|
| Backups (no saltarse ninguno) | Health checks (importa el estado actual) |
| Rotación de logs | Tareas donde la hora exacta importa |
| Tareas que DEBEN ejecutarse al menos una vez por período | Verificaciones que dependen de condiciones externas |

---

## Ejemplo completo: Backup con hardening

### Service

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

# Logging tras éxito:
ExecStartPost=/bin/bash -c 'logger -t myapp-backup "Backup completado exitosamente"'
```

**Nota:** `ExecStartPost` solo se ejecuta si `ExecStart` termina con éxito.
Para notificar **fallos**, se usa `OnFailure` (ver abajo).

### Timer

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
# En el service principal:
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
#!/bin/bash
# /usr/local/bin/notify-failure.sh
UNIT="$1"
echo "FALLO: $UNIT en $(hostname) a las $(date)" | \
    mail -s "Fallo: $UNIT" ops@example.com
```

El patrón `notify-failure@.service` es un **service template**: el `@` permite
instanciarlo con cualquier nombre de unidad vía `%i`.

---

## Ejemplo: Limpieza periódica

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

---

## Ejemplo: Health check frecuente

```ini
# /etc/systemd/system/health-check.service
[Unit]
Description=Verificación de salud de la aplicación

[Service]
Type=oneshot
ExecStart=/opt/myapp/bin/health-check.sh
```

```ini
# /etc/systemd/system/health-check.timer
[Unit]
Description=Health check cada 5 minutos

[Timer]
OnCalendar=*-*-* *:00/5:00
AccuracySec=10s
# Sin Persistent — si se pierde una verificación, no importa

[Install]
WantedBy=timers.target
```

---

## Timers transient (sin archivos)

`systemd-run` crea timers temporales que no requieren archivos en disco:

```bash
# Ejecutar en N tiempo:
sudo systemd-run --on-active=30min /usr/local/bin/task.sh

# Ejecutar a una hora:
sudo systemd-run --on-calendar="*-*-* 03:00" /usr/local/bin/backup.sh

# Con nombre personalizado:
sudo systemd-run --unit=my-task --on-active=1h /usr/local/bin/task.sh

# Listar timers transient:
systemctl list-timers --no-pager | grep run-
```

**Los timers transient no sobreviven un reboot.** Para tareas persistentes,
crear archivos `.timer` + `.service`.

---

## User timers

Los usuarios pueden crear timers sin privilegios de root:

```bash
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
```

**Importante:** los user timers solo corren mientras el usuario tiene sesión
activa. Para que funcionen sin sesión:

```bash
loginctl enable-linger username
```

---

## Gestión de timers

```bash
# Ver todos los timers:
systemctl list-timers --all --no-pager

# Estado de un timer:
systemctl status myapp-backup.timer

# Habilitar/deshabilitar (arrancar o no en boot):
sudo systemctl enable myapp-backup.timer
sudo systemctl disable myapp-backup.timer

# Arrancar/parar (inmediato):
sudo systemctl start myapp-backup.timer
sudo systemctl stop myapp-backup.timer

# Ejecutar el service manualmente:
sudo systemctl start myapp-backup.service

# Ver logs del service:
journalctl -u myapp-backup.service --no-pager -n 20

# Ver cuándo fue la última ejecución:
systemctl show myapp-backup.timer --property=LastTriggerUSec
```

---

## Migrar de cron a timers

```bash
# Cron original:
# 0 3 * * * /opt/myapp/bin/backup.sh >> /var/log/backup.log 2>&1

# Equivalente en timer:
# 1. No necesitas redirigir a log — journal captura stdout y stderr
# 2. "0 3 * * *" → OnCalendar=*-*-* 03:00:00
# 3. Añadir Persistent=true si la tarea no debe perderse
```

| Cron | OnCalendar |
|---|---|
| `* * * * *` | `minutely` |
| `0 * * * *` | `hourly` |
| `0 0 * * *` | `daily` |
| `0 0 * * 0` | `weekly` |
| `0 0 1 * *` | `monthly` |
| `*/5 * * * *` | `*-*-* *:00/5:00` |
| `0 9-17 * * 1-5` | `Mon..Fri *-*-* 09..17:00:00` |
| `30 2 * * *` | `*-*-* 02:30:00` |

---

## Laboratorio

### Parte 1 — Estructura y activación

#### Paso 1.1: El service (qué ejecutar)

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

#### Paso 1.2: El timer (cuándo ejecutar)

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

#### Paso 1.3: Activar y verificar

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
echo "--- Ejecutar manualmente ---"
echo "sudo systemctl start myapp-backup.service"
echo "  Ejecuta el service sin esperar al timer"
echo ""
echo "--- Ver resultado ---"
echo "systemctl status myapp-backup.service"
echo "journalctl -u myapp-backup.service --no-pager -n 20"
'
```

#### Paso 1.4: Relación timer ↔ service

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

### Parte 2 — Persistent y ejemplos

#### Paso 2.1: Persistent timers

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
echo "--- Cuando usar ---"
echo "  SI: backups, rotacion de logs, tareas que DEBEN ejecutarse"
echo "  NO: health checks (importa el estado actual, no el pasado)"
echo ""
echo "--- Ver ultima ejecucion ---"
echo "systemctl show myapp-backup.timer --property=LastTriggerUSec"
'
```

#### Paso 2.2: Ejemplo — Backup con hardening

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

ExecStartPost=/bin/bash -c '"'"'logger -t myapp-backup "Backup completado"'"'"'
EOF
echo ""
echo "Nota: ExecStartPost solo corre tras EXITO de ExecStart."
echo "Para notificar FALLOS, usar OnFailure= (ver paso 2.4)."
echo ""
echo "--- Timer ---"
cat << '\''EOF'\''
# /etc/systemd/system/myapp-backup.timer
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

#### Paso 2.3: Ejemplo — Limpieza y health check

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

#### Paso 2.4: OnFailure para notificaciones

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

### Parte 3 — Transient, user timers y migración

#### Paso 3.1: Timers transient (sin archivos)

```bash
docker compose exec debian-dev bash -c '
echo "=== Timers transient ==="
echo ""
echo "systemd-run crea timers temporales sin crear archivos:"
echo ""
echo "--- Ejecutar en N tiempo ---"
echo "systemd-run --on-active=30min /usr/local/bin/task.sh"
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
echo "Los timers transient NO sobreviven un reboot."
echo "Para persistencia, crear archivos .timer + .service."
'
```

#### Paso 3.2: User timers

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

#### Paso 3.3: Migración cron → timers

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
printf "%-18s %-30s\n" "Cron" "OnCalendar"
printf "%-18s %-30s\n" "------------------" "------------------------------"
printf "%-18s %-30s\n" "* * * * *"         "minutely"
printf "%-18s %-30s\n" "0 * * * *"         "hourly"
printf "%-18s %-30s\n" "0 0 * * *"         "daily"
printf "%-18s %-30s\n" "0 0 * * 0"         "weekly"
printf "%-18s %-30s\n" "0 0 1 * *"         "monthly"
printf "%-18s %-30s\n" "*/5 * * * *"       "*-*-* *:00/5:00"
printf "%-18s %-30s\n" "0 9-17 * * 1-5"    "Mon..Fri *-*-* 09..17:00:00"
printf "%-18s %-30s\n" "30 2 * * *"        "*-*-* 02:30:00"
'
```

#### Paso 3.4: Gestión del ciclo de vida

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

## Ejercicios

### Ejercicio 1 — Estructura mínima

Escribe la estructura mínima de un timer que ejecute
`/opt/reports/daily.sh` cada día a las 06:00 con persistencia. ¿Cuántos
archivos necesitas y dónde van?

<details><summary>Predicción</summary>

Dos archivos en `/etc/systemd/system/`:

**`daily-report.service`:**
```ini
[Unit]
Description=Reporte diario

[Service]
Type=oneshot
ExecStart=/opt/reports/daily.sh
```

**`daily-report.timer`:**
```ini
[Unit]
Description=Timer para reporte diario

[Timer]
OnCalendar=*-*-* 06:00:00
Persistent=true

[Install]
WantedBy=timers.target
```

El `[Install]` va en el timer (no en el service). `Type=oneshot` es el
adecuado para tareas programadas.

</details>

### Ejercicio 2 — [Install] en timer vs service

¿Por qué la sección `[Install]` con `WantedBy=timers.target` va en el
`.timer` y no en el `.service`?

<details><summary>Predicción</summary>

Porque lo que se habilita en el boot es el **timer**, no el service. El timer
es el que debe arrancar automáticamente para poder activar el service cuando
se cumpla la condición de tiempo. Si el `[Install]` estuviera en el service,
el service arrancaría directamente en el boot en lugar de esperar al timer.

`WantedBy=timers.target` crea un symlink en `timers.target.wants/` al hacer
`systemctl enable`, asegurando que el timer se active cuando systemd alcance
`timers.target` durante el arranque.

</details>

### Ejercicio 3 — Persistent=true

Un timer sin `Persistent=true` está programado para las 03:00. El servidor
estuvo apagado de 02:00 a 08:00. ¿Qué pasa? ¿Y si tiene `Persistent=true`?

<details><summary>Predicción</summary>

- **Sin `Persistent=true`:** la ejecución de las 03:00 se **pierde**. El
  timer espera hasta la siguiente ocurrencia (mañana a las 03:00).
- **Con `Persistent=true`:** systemd compara el timestamp guardado en
  `/var/lib/systemd/timers/stamp-*.timer` con la hora actual. Detecta que
  se perdió la ejecución y la **ejecuta inmediatamente** al arrancar (08:00).

</details>

### Ejercicio 4 — ExecStartPost vs OnFailure

¿Cuál es la diferencia entre `ExecStartPost` y `OnFailure` para
notificaciones? ¿Cuándo se ejecuta cada uno?

<details><summary>Predicción</summary>

- **`ExecStartPost`:** se ejecuta solo tras el **éxito** de `ExecStart`.
  Útil para logging de éxito o acciones post-ejecución.
- **`OnFailure`:** se ejecuta solo cuando el service **falla** (exit code
  ≠ 0). Activa otra unidad (típicamente un service template de
  notificación).

Para una estrategia completa de notificación:
```ini
[Unit]
OnFailure=notify-failure@%n.service

[Service]
ExecStart=/opt/myapp/backup.sh
ExecStartPost=/bin/bash -c 'logger "Backup exitoso"'
```

</details>

### Ejercicio 5 — Múltiples ExecStart

¿Qué ocurre si un service `Type=oneshot` tiene múltiples líneas `ExecStart`?
¿Y si una de ellas falla?

<details><summary>Predicción</summary>

En `Type=oneshot`, múltiples `ExecStart` se ejecutan **secuencialmente** (uno
tras otro). Si una falla (exit code ≠ 0), las siguientes **no se ejecutan**
y el service se marca como fallido.

```ini
[Service]
Type=oneshot
ExecStart=/usr/bin/find /tmp -type f -mtime +7 -delete
ExecStart=/usr/bin/find /var/tmp -type f -mtime +30 -delete
```

Si el primer `find` falla, el segundo no se ejecuta. Este comportamiento
secuencial es exclusivo de `Type=oneshot` — otros types solo permiten un
`ExecStart`.

</details>

### Ejercicio 6 — Timer transient

¿Cómo crearías un timer transient que ejecute `/opt/cleanup.sh` en 2 horas
sin crear archivos en disco? ¿Sobrevive un reboot?

<details><summary>Predicción</summary>

```bash
sudo systemd-run --on-active=2h /opt/cleanup.sh
```

El timer se crea en memoria y se destruye tras la ejecución. **No sobrevive
un reboot** porque no tiene archivos en `/etc/systemd/system/`.

Para verificar que se creó:
```bash
systemctl list-timers --no-pager | grep run-
```

Para tareas que deben persistir, crear archivos `.timer` + `.service`.

</details>

### Ejercicio 7 — User timer

Un usuario sin root quiere ejecutar `~/bin/sync-notes.sh` cada hora.
¿Dónde crea los archivos? ¿Qué comandos usa para activarlo? ¿Qué pasa si
cierra sesión?

<details><summary>Predicción</summary>

Archivos en `~/.config/systemd/user/`:

**`sync-notes.service`:**
```ini
[Service]
Type=oneshot
ExecStart=%h/bin/sync-notes.sh
```

**`sync-notes.timer`:**
```ini
[Timer]
OnCalendar=hourly
Persistent=true
[Install]
WantedBy=timers.target
```

Activar:
```bash
systemctl --user daemon-reload
systemctl --user enable --now sync-notes.timer
```

Si cierra sesión, el timer **se detiene**. Para que siga corriendo sin
sesión:
```bash
loginctl enable-linger username
```

</details>

### Ejercicio 8 — Migrar de cron

Convierte esta línea de cron a un timer + service:

```
30 2 * * 1-5 /opt/reports/weekday-report.sh >> /var/log/reports.log 2>&1
```

<details><summary>Predicción</summary>

**`weekday-report.service`:**
```ini
[Unit]
Description=Reporte de dias laborables

[Service]
Type=oneshot
ExecStart=/opt/reports/weekday-report.sh
```

**`weekday-report.timer`:**
```ini
[Unit]
Description=Timer para reporte lun-vie 02:30

[Timer]
OnCalendar=Mon..Fri *-*-* 02:30:00
Persistent=true

[Install]
WantedBy=timers.target
```

No se necesita la redirección `>> /var/log/reports.log 2>&1` porque el
journal captura automáticamente stdout y stderr. Se consultan con:
```bash
journalctl -u weekday-report.service
```

</details>

### Ejercicio 9 — Unit= para nombre diferente

Tienes un service existente llamado `database-maintenance.service` y quieres
dispararlo cada domingo a las 04:00. ¿Cómo escribes el timer si no quieres
renombrar el service?

<details><summary>Predicción</summary>

```ini
# /etc/systemd/system/weekly-db-maint.timer
[Unit]
Description=Timer semanal para mantenimiento de BD

[Timer]
OnCalendar=Sun *-*-* 04:00:00
Persistent=true
Unit=database-maintenance.service

[Install]
WantedBy=timers.target
```

La clave es `Unit=database-maintenance.service` en la sección `[Timer]`.
Sin esta directiva, el timer buscaría `weekly-db-maint.service` (por
convención de nombre), que no existe.

</details>

### Ejercicio 10 — Disparo durante ejecución

Un timer dispara cada 5 minutos, pero el service tarda 7 minutos en
ejecutarse. ¿Qué ocurre con los disparos que llegan mientras el service
está corriendo?

<details><summary>Predicción</summary>

Los disparos que llegan mientras el service está corriendo se **ignoran**.
systemd no permite ejecutar la misma instancia del service en paralelo
consigo misma (por defecto).

En la práctica:
```
t=0:   timer dispara → service empieza (tarda 7 min)
t=5:   timer dispara → service aún corriendo → se ignora
t=7:   service termina
t=10:  timer dispara → service empieza
t=15:  timer dispara → service aún corriendo → se ignora
t=17:  service termina
```

Esto evita ejecuciones superpuestas que podrían causar condiciones de carrera
(ej: dos backups simultáneos escribiendo en el mismo destino). Si la tarea
tarda más que el intervalo, es señal de que hay que aumentar el intervalo o
optimizar el script.

</details>
