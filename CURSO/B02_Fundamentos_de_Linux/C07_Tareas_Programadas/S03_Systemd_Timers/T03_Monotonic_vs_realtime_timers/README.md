# T03 — Monotonic vs realtime timers

## Dos tipos de timer

systemd distingue dos formas de definir **cuándo** se activa un timer:

```
Realtime (calendario)              Monotonic (relativo)
─────────────────────              ────────────────────
"El 18 de marzo a las 03:00"       "30 minutos después del boot"
"Cada lunes a las 09:00"           "15 segundos después de que
"Cada 6 horas"                      el timer se active"

OnCalendar=                        OnBootSec=
                                   OnStartupSec=
                                   OnActiveSec=
                                   OnUnitActiveSec=
                                   OnUnitInactiveSec=
```

## Realtime timers (OnCalendar)

Se basan en el **reloj del sistema** (fecha y hora reales). Se activan
en momentos absolutos del calendario:

```ini
[Timer]
# Cada día a las 03:00:
OnCalendar=*-*-* 03:00:00

# Cada lunes a las 09:00:
OnCalendar=Mon *-*-* 09:00:00

# El día 1 de cada mes:
OnCalendar=*-*-01 00:00:00
```

```bash
# Características:
# - Dependen del reloj del sistema (wall clock)
# - Si se cambia la hora del sistema, el timer se ajusta
# - Pueden usar Persistent=true para ejecuciones perdidas
# - Son el equivalente a las expresiones de cron

# Cuándo usar:
# - Tareas que deben ejecutarse a una hora específica
# - Backups nocturnos
# - Reportes a horarios fijos
# - Cualquier tarea tipo "cron"
```

## Monotonic timers (On*Sec)

Se basan en **tiempo transcurrido** desde un evento. No dependen del
reloj del sistema:

### OnBootSec — Desde el boot

```ini
[Timer]
# 5 minutos después del boot:
OnBootSec=5min
```

```bash
# Se cuenta desde que el kernel inicia
# Útil para tareas de inicialización que no son servicios

# Ejemplo: verificar actualizaciones 10 minutos después del boot
# [Timer]
# OnBootSec=10min
# Persistent=false    # no ejecutar si el equipo ya lleva más de 10min
```

### OnStartupSec — Desde que systemd arranca

```ini
[Timer]
# 2 minutos después de que systemd (el service manager) arranca:
OnStartupSec=2min
```

```bash
# Para system timers, OnBootSec ≈ OnStartupSec
# La diferencia es mínima (milisegundos entre kernel y systemd)

# Para user timers (--user), la diferencia es significativa:
# OnBootSec  = desde el boot del sistema
# OnStartupSec = desde que el user manager arranca (login del usuario)
```

### OnActiveSec — Desde la activación del timer

```ini
[Timer]
# 1 hora después de que el timer se activa (enable+start):
OnActiveSec=1h
```

```bash
# Se cuenta desde que el .timer pasa a estado "active"
# Útil para "ejecutar N tiempo después de habilitar el timer"

# Si el timer se para y reinicia, el contador se resetea
```

### OnUnitActiveSec — Desde que el service se activó

```ini
[Timer]
# 30 minutos después de que el service terminó de ejecutarse:
OnUnitActiveSec=30min
```

```bash
# Se cuenta desde la última vez que el service asociado pasó a "active"
# Crea un loop: service termina → esperar 30min → service se ejecuta → esperar 30min

# Es el más útil para tareas repetitivas basadas en tiempo relativo
# Equivale a "ejecutar cada 30 minutos" pero medido desde la última ejecución

# Diferencia con OnCalendar=*:00/30:
# OnCalendar=*:00/30        → a los minutos 00 y 30 de cada hora (absoluto)
# OnUnitActiveSec=30min     → 30 min después de la última ejecución (relativo)
# Si la tarea tarda 10 min:
#   OnCalendar: 00:00, 00:30, 01:00 (se ejecuta aunque la anterior aún corre)
#   OnUnitActiveSec: 00:00, 00:40, 01:10 (30 min después de que termina)
```

### OnUnitInactiveSec — Desde que el service se desactivó

```ini
[Timer]
# 15 minutos después de que el service pasa a "inactive":
OnUnitInactiveSec=15min
```

```bash
# Similar a OnUnitActiveSec, pero cuenta desde que el service TERMINA
# Para oneshot services, la diferencia es mínima
# Para services que corren un tiempo (Type=simple), la diferencia importa:

# Ejemplo con un service que tarda 5 minutos:
# OnUnitActiveSec=30min:
#   t=0  service inicia
#   t=5  service termina
#   t=30 30 min desde que INICIÓ → ejecutar de nuevo

# OnUnitInactiveSec=30min:
#   t=0  service inicia
#   t=5  service termina (pasa a inactive)
#   t=35 30 min desde que TERMINÓ → ejecutar de nuevo
```

## Combinar directivas

Se pueden usar múltiples directivas en el mismo timer. Se combinan
con **OR** (se activa cuando cualquiera se cumple):

### Boot + repetición

```ini
[Timer]
# Ejecutar al boot y luego cada 6 horas:
OnBootSec=5min
OnUnitActiveSec=6h
```

```bash
# Comportamiento:
# 1. 5 min después del boot → ejecutar
# 2. 6 horas después de esa ejecución → ejecutar
# 3. 6 horas después → ejecutar
# ... y así sucesivamente

# Este patrón reemplaza a @reboot + */6 * * * * en cron
```

### Calendario + boot

```ini
[Timer]
# A las 03:00 diarias Y al boot:
OnCalendar=*-*-* 03:00:00
OnBootSec=2min
Persistent=true
```

```bash
# Comportamiento:
# - Cada día a las 03:00
# - También 2 minutos después de cada boot
# - Si se perdió la ejecución de las 03:00 → ejecutar al boot (Persistent)
```

### Múltiples calendarios

```ini
[Timer]
# Lunes a las 03:00 Y viernes a las 22:00:
OnCalendar=Mon *-*-* 03:00:00
OnCalendar=Fri *-*-* 22:00:00
# Múltiples OnCalendar se combinan con OR
```

## Unidades de tiempo

```bash
# systemd acepta estas unidades:
# usec, us     — microsegundos
# msec, ms     — milisegundos
# seconds, s   — segundos
# minutes, min — minutos
# hours, h     — horas
# days, d      — días
# weeks, w     — semanas
# months, M    — meses
# years, y     — años

# Combinaciones:
# OnBootSec=1h 30min        → 90 minutos
# OnUnitActiveSec=2d 12h    → 2 días y 12 horas
# OnActiveSec=1w            → 1 semana
```

## Ejemplos prácticos

### Health check con intervalo fijo

```ini
# /etc/systemd/system/health-check.service
[Unit]
Description=Health check

[Service]
Type=oneshot
ExecStart=/opt/myapp/bin/health-check.sh
```

```ini
# /etc/systemd/system/health-check.timer
[Unit]
Description=Health check cada 5 minutos

[Timer]
# Al boot + cada 5 minutos después de cada ejecución:
OnBootSec=1min
OnUnitActiveSec=5min
AccuracySec=10s

[Install]
WantedBy=timers.target
```

```bash
# ¿Por qué OnUnitActiveSec en vez de OnCalendar?
# - OnCalendar=*:00/5 ejecuta a :00, :05, :10, :15...
#   Si la tarea tarda 3 min, a las :05 ya está corriendo
#   y el disparo de :05 se ignora
# - OnUnitActiveSec=5min espera 5 min desde que TERMINA
#   Siempre hay 5 minutos entre ejecuciones
```

### Sync periódico post-boot

```ini
# /etc/systemd/system/data-sync.timer
[Unit]
Description=Sincronizar datos periódicamente

[Timer]
OnBootSec=3min
OnUnitInactiveSec=30min
# 3 min después del boot, luego cada 30 min desde que termina

[Install]
WantedBy=timers.target
```

### Backup nocturno con retry

```ini
# /etc/systemd/system/backup.timer
[Unit]
Description=Backup nocturno

[Timer]
OnCalendar=*-*-* 03:00:00
Persistent=true
RandomizedDelaySec=15min

[Install]
WantedBy=timers.target
```

```ini
# /etc/systemd/system/backup.service
[Unit]
Description=Ejecutar backup

[Service]
Type=oneshot
ExecStart=/opt/backup/run.sh

# Si falla, systemd puede reintentarlo:
Restart=on-failure
RestartSec=10min
# Reintentar después de 10 min en caso de fallo

# Máximo de reintentos:
StartLimitIntervalSec=1h
StartLimitBurst=3
# Máximo 3 intentos en 1 hora
```

### Timer de usuario para recordatorios

```ini
# ~/.config/systemd/user/break-reminder.timer
[Unit]
Description=Recordatorio de descanso

[Timer]
OnStartupSec=2h
OnUnitActiveSec=2h
# 2 horas después del login, luego cada 2 horas

[Install]
WantedBy=timers.target
```

```bash
# OnStartupSec aquí cuenta desde que el user manager arranca (login)
# No desde el boot del sistema
# Si el usuario hace login a las 09:00:
# 11:00 → primer recordatorio
# 13:00 → segundo
# 15:00 → tercero
```

## Tabla de directivas

| Directiva | Cuenta desde | Tipo | Uso típico |
|---|---|---|---|
| OnCalendar | Reloj del sistema | Realtime | Tareas a hora fija |
| OnBootSec | Boot del kernel | Monotonic | Inicialización |
| OnStartupSec | Arranque de systemd/user manager | Monotonic | Inicialización de usuario |
| OnActiveSec | Activación del timer | Monotonic | Ejecución diferida |
| OnUnitActiveSec | Activación del service | Monotonic | Repetición periódica |
| OnUnitInactiveSec | Desactivación del service | Monotonic | Repetición con pausa |

## Reloj monotónico vs reloj de pared

```bash
# El reloj de pared (wall clock / realtime clock):
# - Puede cambiar (NTP, ajuste manual, DST)
# - Si se adelanta 1 hora, los timers realtime se ajustan
# - Si se atrasa, un timer podría dispararse dos veces

# El reloj monotónico:
# - Solo avanza, nunca retrocede
# - No se ve afectado por cambios de hora
# - Cuenta el tiempo transcurrido desde el boot
# - Incluye tiempo en suspensión (sleep/hibernate)

# Implicaciones:
# - OnCalendar puede verse afectado por cambios de hora (NTP)
# - OnBootSec/OnUnitActiveSec no se ven afectados
# - Para tareas donde la precisión horaria importa: OnCalendar
# - Para tareas donde el intervalo importa: On*Sec
```

```bash
# Ver ambos relojes:
timedatectl
# Local time: Tue 2026-03-17 15:30:00 UTC
# Universal time: Tue 2026-03-17 15:30:00 UTC
# RTC time: Tue 2026-03-17 15:30:00

# Tiempo desde el boot (monotónico):
cat /proc/uptime
# 432000.50 1700000.00
# 432000.50 segundos = 5 días desde el boot
```

## Elegir entre realtime y monotonic

```bash
# Usar OnCalendar (realtime) cuando:
# ✓ La tarea debe ejecutarse a una hora del día específica
# ✓ La tarea reemplaza un cron job existente
# ✓ El horario tiene significado de negocio (informes diarios a las 08:00)
# ✓ Necesitas Persistent=true para tareas perdidas

# Usar On*Sec (monotonic) cuando:
# ✓ La tarea debe ejecutarse con un intervalo fijo entre ejecuciones
# ✓ La tarea debe ejecutarse N tiempo después del boot
# ✓ No importa la hora del día, solo la frecuencia
# ✓ Quieres evitar que cambios de hora afecten la ejecución
# ✓ Health checks, sincronizaciones, polling
```

---

## Ejercicios

### Ejercicio 1 — Identificar el tipo

```bash
# ¿Qué tipo de timer (realtime o monotonic) usarías para?
# a) Backup a las 03:00
# b) Health check cada 2 minutos
# c) Verificación de actualizaciones al encender
# d) Rotación de logs semanal
# e) Sync de datos cada 30 minutos después de terminar el anterior
```

### Ejercicio 2 — Inspeccionar timers del sistema

```bash
# Clasificar los timers de tu sistema en realtime vs monotonic:
for timer in $(systemctl list-timers --no-pager --all -o json 2>/dev/null | \
    grep -oP '"unit":"[^"]*\.timer"' | cut -d'"' -f4 2>/dev/null || \
    systemctl list-timers --no-pager --all | awk '/\.timer/{print $(NF-1)}'); do
    echo "=== $timer ==="
    systemctl cat "$timer" 2>/dev/null | grep -E "^On"
done
```

### Ejercicio 3 — Calcular ejecuciones

```bash
# Dado este timer:
# [Timer]
# OnBootSec=5min
# OnUnitActiveSec=1h
#
# Si el boot fue a las 08:00 y cada ejecución tarda 10 minutos:
# ¿A qué hora se ejecuta la 1ª, 2ª y 3ª vez?
#
# 1ª: 08:05 (boot + 5min)
# 2ª: 09:15 (08:05 inicio + 10min ejecución + 1h = 09:15)
# 3ª: 10:25 (09:15 inicio + 10min ejecución + 1h = 10:25)
#
# Verificar con systemd-analyze:
systemd-analyze calendar --iterations=3 "hourly" 2>/dev/null
```
