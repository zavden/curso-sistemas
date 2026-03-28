# T01 — Timers vs cron

## Qué son los systemd timers

Un timer es una unidad de systemd (`.timer`) que activa otra unidad
(típicamente un `.service`) en momentos determinados. Es el mecanismo
nativo de systemd para programar tareas:

```bash
# Listar todos los timers activos:
systemctl list-timers --all --no-pager
# NEXT                        LEFT          LAST                        PASSED       UNIT                    ACTIVATES
# Tue 2026-03-17 18:09:00 UTC 23min left    Tue 2026-03-17 17:09:00 UTC 36min ago    fstrim.timer            fstrim.service
# Wed 2026-03-18 00:00:00 UTC 6h left       Tue 2026-03-17 00:00:00 UTC 17h ago      logrotate.timer         logrotate.service
# Wed 2026-03-18 06:30:00 UTC 12h left      Tue 2026-03-17 06:30:00 UTC 11h ago      man-db.timer            man-db.service

# Columnas:
# NEXT      — próxima ejecución
# LEFT      — tiempo restante
# LAST      — última ejecución
# PASSED    — hace cuánto fue
# UNIT      — el timer
# ACTIVATES — el service que activa
```

### Arquitectura: timer + service

```
                 fstrim.timer
                     │
                     │ cuando se cumple la condición
                     │ de tiempo
                     ▼
                fstrim.service
                     │
                     │ ExecStart=
                     ▼
              /usr/sbin/fstrim --all
```

```bash
# Un timer siempre necesita un service asociado:
# fstrim.timer  → activa → fstrim.service

# Por defecto, el timer activa un service con el MISMO nombre:
# myapp.timer → myapp.service

# Se puede especificar un nombre diferente:
# [Timer]
# Unit=otro-nombre.service
```

## Ventajas sobre cron

### 1. Integración con el journal

```bash
# Con cron:
# - La salida va a mail (si hay MTA) o se pierde
# - Necesitas redirigir a un archivo de log manualmente
# - No hay forma estándar de ver si la tarea se ejecutó

# Con timers:
journalctl -u fstrim.service --no-pager -n 10
# Logs completos con timestamps, prioridad, PID
# Persistentes, consultables, filtrables
```

### 2. Dependencias y orden

```bash
# Con cron:
# - Las tareas son independientes
# - No puedes decir "ejecutar A después de B"
# - No puedes depender de que la red esté disponible

# Con timers:
# El .service puede tener dependencias:
# [Unit]
# After=network-online.target
# Requires=postgresql.service
# La tarea no se ejecuta hasta que las dependencias estén listas
```

### 3. Control de recursos

```bash
# Con cron:
# - La tarea consume lo que quiera
# - Solo puedes usar nice/ionice manualmente

# Con timers (en el .service):
# [Service]
# CPUQuota=50%
# MemoryMax=512M
# IOWeight=100
# Sandboxing, namespaces, cgroups — control total
```

### 4. Ejecuciones perdidas (Persistent=)

```bash
# Con cron:
# - Si el equipo estaba apagado, la tarea se pierde
# - Necesitas anacron para compensar (solo daily/weekly/monthly)

# Con timers:
# [Timer]
# Persistent=true
# Si el equipo estaba apagado, se ejecuta al encender
# Funciona para CUALQUIER intervalo, no solo daily/weekly/monthly
```

### 5. Precisión y aleatorización

```bash
# Con cron:
# - Precisión de 1 minuto (mínimo)
# - No hay aleatorización nativa

# Con timers:
# [Timer]
# OnCalendar=*-*-* 03:00:00        # precisión de segundo
# AccuracySec=1us                    # tan preciso como quieras
# RandomizedDelaySec=1h              # aleatorización para evitar thundering herd
```

### 6. Seguridad y sandboxing

```bash
# Con cron:
# - El script tiene acceso completo al sistema
# - Solo el UID del usuario limita los permisos

# Con timers (en el .service):
# [Service]
# DynamicUser=true
# ProtectSystem=strict
# PrivateTmp=true
# NoNewPrivileges=true
# Aislamiento completo del sistema
```

### 7. Estado y diagnóstico

```bash
# Con cron:
# - ¿Se ejecutó? Revisar logs de cron (si existen)
# - ¿Falló? Revisar mail (si hay MTA)
# - ¿Cuándo fue la última ejecución? No hay forma nativa

# Con timers:
systemctl status myapp.timer
# Active: active (waiting)
# Trigger: Wed 2026-03-18 03:00:00 UTC; 11h left

systemctl status myapp.service
# Active: inactive (dead) since Tue 2026-03-17 03:00:05 UTC; 14h ago
# Process: 12345 ExecStart=/opt/myapp/task.sh (code=exited, status=0/SUCCESS)
# Main PID: 12345 (code=exited, status=0/SUCCESS)
```

## Desventajas frente a cron

```bash
# 1. Verbosidad — dos archivos en lugar de una línea:
#    cron:  "0 3 * * * /opt/task.sh" (una línea)
#    timer: myapp.timer + myapp.service (dos archivos)

# 2. Curva de aprendizaje:
#    cron: 5 campos + comando (conocido universalmente)
#    timer: OnCalendar con su propia sintaxis + unit files

# 3. No hay equivalente a "crontab -e" para usuarios:
#    En cron, cualquier usuario edita su crontab fácilmente
#    Con timers, se necesitan user units en ~/.config/systemd/user/

# 4. Portabilidad:
#    cron funciona en cualquier Unix (BSD, macOS, Solaris)
#    timers solo en sistemas con systemd

# 5. Edición rápida:
#    cron: crontab -e → cambiar una línea → guardar
#    timer: editar archivo → daemon-reload → restart timer
```

## OnCalendar — La sintaxis de tiempo

### Formato general

```bash
# OnCalendar=DiaDeLaSemana YYYY-MM-DD HH:MM:SS

# Cada parte es opcional:
# OnCalendar=03:00            → cada día a las 03:00
# OnCalendar=Mon 03:00        → cada lunes a las 03:00
# OnCalendar=2026-03-20       → el 20 de marzo de 2026
# OnCalendar=*-*-01 06:00     → el día 1 de cada mes a las 06:00
```

### Comodines y rangos

```bash
# * = cualquier valor (como en cron):
OnCalendar=*-*-* 03:00:00       # cada día a las 03:00

# Rango con ..:
OnCalendar=Mon..Fri *-*-* 09:00  # lun-vie a las 09:00

# Lista con ,:
OnCalendar=Mon,Wed,Fri 08:00     # lun, mié, vie a las 08:00

# Intervalo con /:
OnCalendar=*-*-* *:00/15:00      # cada 15 minutos (minuto 0, 15, 30, 45)
OnCalendar=*-*-* 00/2:00          # cada 2 horas (00, 02, 04, ...)
```

### Expresiones útiles

```bash
# Equivalentes a las cadenas de cron:
OnCalendar=minutely           # @reboot no tiene equivalente directo
OnCalendar=hourly             # = *-*-* *:00:00
OnCalendar=daily              # = *-*-* 00:00:00
OnCalendar=weekly             # = Mon *-*-* 00:00:00
OnCalendar=monthly            # = *-*-01 00:00:00
OnCalendar=yearly             # = *-01-01 00:00:00
OnCalendar=quarterly          # = *-01,04,07,10-01 00:00:00
OnCalendar=semiannually       # = *-01,07-01 00:00:00
```

### Verificar expresiones con systemd-analyze

```bash
# Verificar cuándo se ejecutaría:
systemd-analyze calendar "Mon..Fri *-*-* 09:00"
#   Original form: Mon..Fri *-*-* 09:00
#  Normalized form: Mon..Fri *-*-* 09:00:00
#     Next elapse: Wed 2026-03-18 09:00:00 UTC
#        From now: 15h left

# Múltiples ejecuciones futuras:
systemd-analyze calendar --iterations=5 "Mon..Fri *-*-* 09:00"
#     Next elapse: Wed 2026-03-18 09:00:00 UTC
#        From now: 15h left
#     Iter. #2: Thu 2026-03-19 09:00:00 UTC
#     Iter. #3: Fri 2026-03-20 09:00:00 UTC
#     Iter. #4: Mon 2026-03-23 09:00:00 UTC
#     Iter. #5: Tue 2026-03-24 09:00:00 UTC

# Verificar una expresión compleja:
systemd-analyze calendar "*-*-* 00/6:00"
# Cada 6 horas: 00:00, 06:00, 12:00, 18:00
```

## AccuracySec — Precisión

```bash
# Por defecto, systemd agrupa ejecuciones cercanas para ahorrar
# wake-ups de CPU (eficiencia energética):
# AccuracySec=1min (default)

# Esto significa que una tarea programada para las 03:00
# podría ejecutarse en cualquier momento entre 03:00 y 03:01

# Para mayor precisión:
# [Timer]
# AccuracySec=1s       # precisión de 1 segundo
# AccuracySec=1us      # precisión de 1 microsegundo

# Para menor precisión (ahorro de energía en laptops):
# [Timer]
# AccuracySec=1h       # puede desviarse hasta 1 hora
```

## RandomizedDelaySec — Evitar thundering herd

```bash
# Cuando muchas máquinas tienen el mismo timer,
# todas golpean el mismo recurso al mismo tiempo

# RandomizedDelaySec agrega un delay aleatorio:
# [Timer]
# OnCalendar=*-*-* 03:00
# RandomizedDelaySec=30min
# Ejecuta entre 03:00 y 03:30 (aleatorio por máquina)

# Útil para:
# - Actualizaciones que consultan un repositorio central
# - Backups a un servidor NFS compartido
# - Reportes que consultan una API con rate limit
```

## Tabla comparativa completa

| Aspecto | cron | anacron | systemd timer |
|---|---|---|---|
| Precisión mínima | 1 minuto | 1 día | 1 microsegundo |
| Tareas perdidas | Se pierden | Se ejecutan después | Persistent=true |
| Logs | Mail / syslog | syslog | journal (integrado) |
| Dependencias | No | No | Sí (After=, Requires=) |
| Recursos (cgroups) | No | No | Sí (CPUQuota=, etc.) |
| Seguridad | Solo UID | Solo root | Sandboxing completo |
| Estado | atq/crontab -l | /var/spool/anacron | systemctl status |
| Usuarios | crontab -e | Solo root | User units |
| Portabilidad | Unix universal | Linux | Solo systemd |
| Complejidad | Baja | Baja | Media |
| Archivos necesarios | 1 línea | 1 línea en anacrontab | 2 archivos (.timer + .service) |
| Aleatorización | No | RANDOM_DELAY | RandomizedDelaySec |

## Timers del sistema — Ejemplos reales

```bash
# Ver los timers que ya tiene tu sistema:
systemctl list-timers --no-pager

# Examinar un timer real:
systemctl cat fstrim.timer
# [Timer]
# OnCalendar=weekly
# AccuracySec=1h
# Persistent=true

systemctl cat logrotate.timer
# [Timer]
# OnCalendar=daily
# AccuracySec=12h
# Persistent=true

# Ver el service asociado:
systemctl cat fstrim.service
# [Service]
# Type=oneshot
# ExecStart=/usr/sbin/fstrim --listed-in /etc/fstab:/proc/self/mountinfo ...
```

---

## Ejercicios

### Ejercicio 1 — Explorar timers del sistema

```bash
# 1. ¿Cuántos timers hay activos?
systemctl list-timers --no-pager | grep -c "\.timer"

# 2. ¿Cuál es el próximo timer que se ejecutará?
systemctl list-timers --no-pager | head -2

# 3. Examinar un timer y su service:
TIMER=$(systemctl list-timers --no-pager | awk 'NR==2{print $NF}')
echo "Timer: $TIMER"
systemctl cat "$TIMER" 2>/dev/null
```

### Ejercicio 2 — Verificar expresiones OnCalendar

```bash
# Verificar cuándo se ejecutarían:
systemd-analyze calendar "daily" --iterations=3
systemd-analyze calendar "Mon *-*-* 09:00" --iterations=3
systemd-analyze calendar "*-*-01 06:00:00" --iterations=3
```

### Ejercicio 3 — Comparar cron y timer

```bash
# ¿Qué tareas del sistema usan cron vs timers?
echo "=== Cron ==="
cat /etc/crontab 2>/dev/null | grep -v '^#' | grep -v '^$'
ls /etc/cron.d/ 2>/dev/null
echo "=== Timers ==="
systemctl list-timers --no-pager --all
```
