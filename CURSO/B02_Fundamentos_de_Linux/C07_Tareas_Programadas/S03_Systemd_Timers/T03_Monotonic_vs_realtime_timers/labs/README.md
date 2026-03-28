# Lab — Monotonic vs realtime timers

## Objetivo

Distinguir los dos tipos de timer en systemd: realtime (OnCalendar,
basado en el reloj del sistema) y monotonic (OnBootSec, OnStartupSec,
OnActiveSec, OnUnitActiveSec, OnUnitInactiveSec, basados en tiempo
transcurrido), combinar directivas (OR), unidades de tiempo, reloj
de pared vs monotonico, ejemplos practicos (health check, sync,
backup con retry, user timer), y criterios para elegir entre ambos.

## Prerequisitos

- Entorno del curso levantado (`docker compose up -d`)

---

## Parte 1 — Directivas y relojes

### Objetivo

Entender las directivas realtime y monotonic, y los relojes subyacentes.

### Paso 1.1: Dos tipos de timer

```bash
docker compose exec debian-dev bash -c '
echo "=== Dos tipos de timer ==="
echo ""
echo "--- Realtime (calendario) ---"
echo "  Basado en el reloj del sistema (fecha y hora reales)"
echo "  Directiva: OnCalendar="
echo ""
echo "  Ejemplo:"
echo "    OnCalendar=*-*-* 03:00:00    cada dia a las 03:00"
echo "    OnCalendar=Mon *-*-* 09:00   cada lunes a las 09:00"
echo ""
echo "--- Monotonic (tiempo transcurrido) ---"
echo "  Basado en tiempo desde un evento"
echo "  No depende del reloj del sistema"
echo ""
echo "  5 directivas:"
echo "    OnBootSec=            desde el boot del kernel"
echo "    OnStartupSec=         desde que systemd arranca"
echo "    OnActiveSec=          desde que el timer se activa"
echo "    OnUnitActiveSec=      desde que el service se activo"
echo "    OnUnitInactiveSec=    desde que el service termino"
echo ""
echo "--- Diferencia clave ---"
echo "  Realtime:   \"ejecutar a las 03:00\""
echo "  Monotonic:  \"ejecutar 30 min despues del boot\""
'
```

### Paso 1.2: OnCalendar (realtime)

```bash
docker compose exec debian-dev bash -c '
echo "=== OnCalendar — realtime ==="
echo ""
echo "Se basa en el reloj del sistema (wall clock)."
echo "Si se cambia la hora, el timer se ajusta."
echo ""
echo "--- Ejemplos ---"
cat << '\''EOF'\''
[Timer]
# Cada dia a las 03:00:
OnCalendar=*-*-* 03:00:00

# Cada lunes a las 09:00:
OnCalendar=Mon *-*-* 09:00:00

# Dia 1 de cada mes:
OnCalendar=*-*-01 00:00:00

# Multiples OnCalendar (OR):
OnCalendar=Mon *-*-* 03:00:00
OnCalendar=Fri *-*-* 22:00:00
EOF
echo ""
echo "--- Puede usar Persistent=true ---"
echo "  Si el equipo estaba apagado a la hora programada"
echo "  Ejecuta al encender"
echo ""
echo "--- Equivalente a cron ---"
echo "  OnCalendar reemplaza las expresiones de 5 campos"
echo ""
echo "--- Verificar ---"
systemd-analyze calendar "*-*-* 03:00:00" --iterations=3 2>&1 || \
    echo "(systemd-analyze no disponible)"
'
```

### Paso 1.3: OnBootSec y OnStartupSec

```bash
docker compose exec debian-dev bash -c '
echo "=== OnBootSec ==="
echo ""
echo "Cuenta desde que el kernel inicia."
echo ""
cat << '\''EOF'\''
[Timer]
OnBootSec=5min
# 5 minutos despues del boot
EOF
echo ""
echo "Uso tipico:"
echo "  Tareas de inicializacion que no son servicios"
echo "  Verificar actualizaciones al encender"
echo "  Limpiezas post-boot"
echo ""
echo "=== OnStartupSec ==="
echo ""
echo "Cuenta desde que systemd (el service manager) arranca."
echo ""
cat << '\''EOF'\''
[Timer]
OnStartupSec=2min
# 2 minutos despues de que systemd arranca
EOF
echo ""
echo "--- Diferencia con OnBootSec ---"
echo ""
echo "Para system timers:"
echo "  OnBootSec ~ OnStartupSec (milisegundos de diferencia)"
echo "  El kernel inicia, luego systemd arranca casi inmediatamente"
echo ""
echo "Para user timers (--user):"
echo "  OnBootSec     = desde el boot del sistema"
echo "  OnStartupSec  = desde que el user manager arranca (login)"
echo "  La diferencia puede ser de horas"
echo ""
echo "--- Ver tiempo desde el boot ---"
cat /proc/uptime 2>/dev/null | awk "{printf \"Uptime: %.0f segundos (%.1f horas)\n\", \$1, \$1/3600}"
'
```

### Paso 1.4: OnActiveSec

```bash
docker compose exec debian-dev bash -c '
echo "=== OnActiveSec ==="
echo ""
echo "Cuenta desde que el .timer pasa a estado \"active\"."
echo ""
cat << '\''EOF'\''
[Timer]
OnActiveSec=1h
# 1 hora despues de que el timer se activa (enable+start)
EOF
echo ""
echo "--- Comportamiento ---"
echo "  enable --now el timer → el timer se activa"
echo "  1 hora despues → ejecuta el service"
echo "  NO se repite (ejecucion unica)"
echo ""
echo "--- Si el timer se reinicia ---"
echo "  systemctl restart myapp.timer"
echo "  El contador se resetea"
echo "  1 hora desde el restart → ejecuta de nuevo"
echo ""
echo "--- Diferencia con OnBootSec ---"
echo "  OnBootSec:    cuenta desde el boot (fijo)"
echo "  OnActiveSec:  cuenta desde la activacion del timer"
echo "  Si haces restart del timer, OnActiveSec reinicia"
echo "  OnBootSec no cambia (siempre cuenta desde el boot)"
'
```

### Paso 1.5: OnUnitActiveSec y OnUnitInactiveSec

```bash
docker compose exec debian-dev bash -c '
echo "=== OnUnitActiveSec ==="
echo ""
echo "Cuenta desde la ultima vez que el service paso a \"active\"."
echo ""
cat << '\''EOF'\''
[Timer]
OnUnitActiveSec=30min
# 30 min despues de que el service se activo (inicio)
EOF
echo ""
echo "Crea un loop:"
echo "  service se activa → 30 min → service se ejecuta → 30 min → ..."
echo ""
echo "--- Diferencia con OnCalendar ---"
echo ""
echo "Si la tarea tarda 10 min:"
echo ""
echo "OnCalendar=*:00/30 (absoluto):"
echo "  00:00 ejecuta, termina 00:10"
echo "  00:30 ejecuta, termina 00:40"
echo "  01:00 ejecuta, termina 01:10"
echo "  → 20 min entre fin y siguiente inicio"
echo ""
echo "OnUnitActiveSec=30min (relativo):"
echo "  00:00 ejecuta (se activa), termina 00:10"
echo "  00:30 ejecuta (30 min desde que se activo a 00:00)"
echo "  01:00 ejecuta (30 min desde que se activo a 00:30)"
echo "  → 20 min entre fin y siguiente inicio"
echo ""
echo "=== OnUnitInactiveSec ==="
echo ""
echo "Cuenta desde que el service pasa a \"inactive\" (termina)."
echo ""
cat << '\''EOF'\''
[Timer]
OnUnitInactiveSec=15min
# 15 min despues de que el service TERMINA
EOF
echo ""
echo "--- Diferencia clave con OnUnitActiveSec ---"
echo ""
echo "Service que tarda 5 minutos, intervalo de 30 min:"
echo ""
echo "OnUnitActiveSec=30min:"
echo "  t=0   service inicia (se activa)"
echo "  t=5   service termina"
echo "  t=30  30 min desde que INICIO → ejecutar"
echo "  → 25 min de pausa entre ejecuciones"
echo ""
echo "OnUnitInactiveSec=30min:"
echo "  t=0   service inicia"
echo "  t=5   service termina (pasa a inactive)"
echo "  t=35  30 min desde que TERMINO → ejecutar"
echo "  → 30 min de pausa entre ejecuciones"
echo ""
echo "Para oneshot rapidos, la diferencia es minima."
echo "Para services lentos, OnUnitInactiveSec da pausas mas predecibles."
'
```

### Paso 1.6: Reloj de pared vs reloj monotonico

```bash
docker compose exec debian-dev bash -c '
echo "=== Reloj de pared vs monotonico ==="
echo ""
echo "--- Reloj de pared (wall clock / realtime) ---"
echo "  Usado por: OnCalendar"
echo "  Puede cambiar: NTP, ajuste manual, DST"
echo "  Si se adelanta 1h → timers realtime se ajustan"
echo "  Si se atrasa → un timer podria dispararse dos veces"
echo ""
echo "--- Reloj monotonico ---"
echo "  Usado por: OnBootSec, OnStartupSec, OnActiveSec,"
echo "             OnUnitActiveSec, OnUnitInactiveSec"
echo "  Solo avanza, nunca retrocede"
echo "  No se ve afectado por cambios de hora"
echo "  Cuenta desde el boot"
echo "  Incluye tiempo en suspension (sleep/hibernate)"
echo ""
echo "--- Ver ambos relojes ---"
echo ""
echo "Reloj del sistema:"
timedatectl 2>/dev/null | grep -E "(Local|Universal|RTC|NTP)" || \
    date "+  %Y-%m-%d %H:%M:%S %Z"
echo ""
echo "Tiempo monotonico (uptime):"
cat /proc/uptime 2>/dev/null | awk "{printf \"  %.0f segundos (%.1f horas, %.1f dias)\n\", \$1, \$1/3600, \$1/86400}"
echo ""
echo "--- Implicaciones ---"
echo "  OnCalendar: afectado por cambios de hora (NTP, DST)"
echo "  On*Sec:     no afectado por cambios de hora"
echo "  Para precision horaria: OnCalendar"
echo "  Para intervalo fijo: On*Sec"
'
```

---

## Parte 2 — Combinaciones y ejemplos

### Objetivo

Combinar directivas en un timer y ver ejemplos practicos.

### Paso 2.1: Combinar directivas (OR)

```bash
docker compose exec debian-dev bash -c '
echo "=== Combinar directivas ==="
echo ""
echo "Multiples directivas en un timer se combinan con OR."
echo "El service se activa cuando CUALQUIERA se cumple."
echo ""
echo "--- Boot + repeticion ---"
cat << '\''EOF'\''
[Timer]
OnBootSec=5min
OnUnitActiveSec=6h
# 5 min despues del boot → ejecutar
# luego cada 6 horas despues de cada ejecucion
EOF
echo ""
echo "Equivale a @reboot + */6 * * * * en cron"
echo ""
echo "--- Calendario + boot ---"
cat << '\''EOF'\''
[Timer]
OnCalendar=*-*-* 03:00:00
OnBootSec=2min
Persistent=true
# Cada dia a las 03:00
# Tambien 2 min despues del boot
# Si perdio las 03:00, ejecutar al encender
EOF
echo ""
echo "--- Multiples OnCalendar ---"
cat << '\''EOF'\''
[Timer]
OnCalendar=Mon *-*-* 03:00:00
OnCalendar=Fri *-*-* 22:00:00
# Lunes a las 03:00 O viernes a las 22:00
EOF
echo ""
echo "--- Regla ---"
echo "  Cualquier combinacion de OnCalendar + On*Sec es valida"
echo "  Todas las directivas se evaluan con OR"
echo "  La primera que se cumpla activa el service"
'
```

### Paso 2.2: Unidades de tiempo

```bash
docker compose exec debian-dev bash -c '
echo "=== Unidades de tiempo ==="
echo ""
echo "systemd acepta estas unidades en directivas On*Sec:"
echo ""
echo "| Unidad      | Abreviatura | Ejemplo           |"
echo "|-------------|-------------|-------------------|"
echo "| usec        | us          | OnBootSec=500us   |"
echo "| msec        | ms          | OnBootSec=100ms   |"
echo "| seconds     | s           | OnBootSec=30s     |"
echo "| minutes     | min         | OnBootSec=5min    |"
echo "| hours       | h           | OnBootSec=2h      |"
echo "| days        | d           | OnBootSec=1d      |"
echo "| weeks       | w           | OnBootSec=1w      |"
echo "| months      | M           | OnBootSec=1M      |"
echo "| years       | y           | OnBootSec=1y      |"
echo ""
echo "--- Combinaciones ---"
echo "  OnBootSec=1h 30min        → 90 minutos"
echo "  OnUnitActiveSec=2d 12h    → 2 dias y 12 horas"
echo "  OnActiveSec=1w            → 1 semana"
echo ""
echo "--- Verificar con systemd-analyze ---"
systemd-analyze timespan "1h 30min" 2>&1 || \
    echo "(systemd-analyze timespan no disponible)"
echo ""
systemd-analyze timespan "2d 12h" 2>&1 || true
echo ""
systemd-analyze timespan "1w" 2>&1 || true
'
```

### Paso 2.3: Ejemplo — Health check con intervalo fijo

```bash
docker compose exec debian-dev bash -c '
echo "=== Ejemplo: health check ==="
echo ""
echo "--- Service ---"
cat << '\''EOF'\''
# /etc/systemd/system/health-check.service
[Unit]
Description=Health check

[Service]
Type=oneshot
ExecStart=/opt/myapp/bin/health-check.sh
EOF
echo ""
echo "--- Timer ---"
cat << '\''EOF'\''
# /etc/systemd/system/health-check.timer
[Unit]
Description=Health check cada 5 minutos

[Timer]
OnBootSec=1min
OnUnitActiveSec=5min
AccuracySec=10s

[Install]
WantedBy=timers.target
EOF
echo ""
echo "--- Por que OnUnitActiveSec y no OnCalendar ---"
echo ""
echo "OnCalendar=*:00/5 (cada 5 min absolutos):"
echo "  Si la tarea tarda 3 min y se activa a :00"
echo "  A :05 se activa otra vez (puede solaparse)"
echo "  El intervalo es desde el reloj, no desde la tarea"
echo ""
echo "OnUnitActiveSec=5min (5 min desde la activacion):"
echo "  Siempre 5 min entre activaciones"
echo "  Si tarda 3 min: activa a t=0, t=5, t=10..."
echo "  Sin solapamiento"
echo ""
echo "--- Sin Persistent ---"
echo "  Health checks evaluan el estado actual"
echo "  No tiene sentido ejecutar uno perdido"
'
```

### Paso 2.4: Ejemplo — Sync periodico post-boot

```bash
docker compose exec debian-dev bash -c '
echo "=== Ejemplo: sync periodico ==="
echo ""
echo "--- Timer ---"
cat << '\''EOF'\''
# /etc/systemd/system/data-sync.timer
[Unit]
Description=Sincronizar datos periodicamente

[Timer]
OnBootSec=3min
OnUnitInactiveSec=30min
# 3 min despues del boot, luego cada 30 min desde que termina

[Install]
WantedBy=timers.target
EOF
echo ""
echo "--- Por que OnUnitInactiveSec ---"
echo "  Si el sync tarda 10 min:"
echo "  boot+3min → sync (10min) → 30min pausa → sync → ..."
echo "  Siempre hay 30 min de pausa entre ejecuciones"
echo ""
echo "  Con OnUnitActiveSec=30min:"
echo "  boot+3min → sync (10min) → 20min pausa → sync → ..."
echo "  Solo 20 min de pausa (30 - 10 de ejecucion)"
echo ""
echo "--- Patron: boot + repeticion ---"
echo "  OnBootSec=        primera ejecucion"
echo "  OnUnit*Sec=       repeticiones"
echo "  Es el patron mas comun para monotonic timers"
'
```

### Paso 2.5: Ejemplo — Backup con retry

```bash
docker compose exec debian-dev bash -c '
echo "=== Ejemplo: backup con retry ==="
echo ""
echo "--- Timer (realtime) ---"
cat << '\''EOF'\''
# /etc/systemd/system/backup.timer
[Unit]
Description=Backup nocturno

[Timer]
OnCalendar=*-*-* 03:00:00
Persistent=true
RandomizedDelaySec=15min

[Install]
WantedBy=timers.target
EOF
echo ""
echo "--- Service (con retry) ---"
cat << '\''EOF'\''
# /etc/systemd/system/backup.service
[Unit]
Description=Ejecutar backup

[Service]
Type=oneshot
ExecStart=/opt/backup/run.sh

Restart=on-failure
RestartSec=10min
# Si falla, reintentar en 10 minutos

StartLimitIntervalSec=1h
StartLimitBurst=3
# Maximo 3 intentos en 1 hora
EOF
echo ""
echo "--- Combinacion realtime + Persistent ---"
echo "  OnCalendar: hora fija (03:00)"
echo "  Persistent: si perdio las 03:00, ejecutar al encender"
echo "  RandomizedDelaySec: evitar thundering herd"
echo "  Restart=on-failure: reintentar si falla"
echo ""
echo "Nota: Restart en oneshot solo funciona si ExecStart falla."
echo "Cuando ExecStart tiene exito, el service pasa a inactive."
'
```

### Paso 2.6: Ejemplo — User timer

```bash
docker compose exec debian-dev bash -c '
echo "=== Ejemplo: user timer ==="
echo ""
echo "--- Timer ---"
cat << '\''EOF'\''
# ~/.config/systemd/user/break-reminder.timer
[Unit]
Description=Recordatorio de descanso

[Timer]
OnStartupSec=2h
OnUnitActiveSec=2h
# 2 horas despues del login, luego cada 2 horas

[Install]
WantedBy=timers.target
EOF
echo ""
echo "--- Service ---"
cat << '\''EOF'\''
# ~/.config/systemd/user/break-reminder.service
[Service]
Type=oneshot
ExecStart=/usr/bin/notify-send "Tomar un descanso"
EOF
echo ""
echo "--- OnStartupSec en user timers ---"
echo "  OnStartupSec cuenta desde el user manager (login)"
echo "  NO desde el boot del sistema"
echo ""
echo "  Usuario hace login a las 09:00:"
echo "    11:00 → primer recordatorio"
echo "    13:00 → segundo"
echo "    15:00 → tercero"
echo ""
echo "--- OnBootSec en user timers ---"
echo "  OnBootSec SI cuenta desde el boot del sistema"
echo "  Si boot a 08:00, login a 09:00, OnBootSec=30min:"
echo "    Ya paso (08:30 < 09:00) → ejecuta inmediatamente"
echo ""
echo "--- Gestion ---"
echo "  systemctl --user enable --now break-reminder.timer"
echo "  systemctl --user list-timers"
echo "  loginctl enable-linger user    ← correr sin sesion"
'
```

---

## Parte 3 — Elegir y analizar

### Objetivo

Saber cuando usar cada tipo y analizar timers existentes.

### Paso 3.1: Criterios de eleccion

```bash
docker compose exec debian-dev bash -c '
echo "=== Cuando usar cada tipo ==="
echo ""
echo "--- Usar OnCalendar (realtime) cuando ---"
echo "  La tarea debe ejecutarse a una hora especifica"
echo "  Reemplaza un cron job existente"
echo "  El horario tiene significado (reportes a las 08:00)"
echo "  Necesitas Persistent=true para tareas perdidas"
echo ""
echo "--- Usar On*Sec (monotonic) cuando ---"
echo "  Importa el intervalo fijo entre ejecuciones"
echo "  La tarea debe ejecutarse N tiempo despues del boot"
echo "  No importa la hora, solo la frecuencia"
echo "  Quieres evitar efectos de cambios de hora"
echo "  Health checks, sync, polling"
echo ""
echo "--- Tabla de decision ---"
echo "| Escenario                    | Tipo      | Directiva              |"
echo "|------------------------------|-----------|------------------------|"
echo "| Backup a las 03:00           | Realtime  | OnCalendar=...03:00    |"
echo "| Health check cada 2 min      | Monotonic | OnUnitActiveSec=2min   |"
echo "| Verificar al encender        | Monotonic | OnBootSec=5min         |"
echo "| Rotacion de logs semanal     | Realtime  | OnCalendar=weekly      |"
echo "| Sync cada 30 min             | Monotonic | OnUnitInactiveSec=30m  |"
echo "| Reporte lun-vie 08:00        | Realtime  | OnCalendar=Mon..Fri... |"
echo "| Limpieza post-login          | Monotonic | OnStartupSec=5min      |"
echo "| Polling API cada 10s         | Monotonic | OnUnitActiveSec=10s    |"
'
```

### Paso 3.2: Tabla de directivas

```bash
docker compose exec debian-dev bash -c '
echo "=== Tabla de directivas ==="
echo ""
echo "| Directiva           | Cuenta desde                    | Tipo      |"
echo "|---------------------|---------------------------------|-----------|"
echo "| OnCalendar          | Reloj del sistema               | Realtime  |"
echo "| OnBootSec           | Boot del kernel                 | Monotonic |"
echo "| OnStartupSec        | Arranque de systemd/user mgr    | Monotonic |"
echo "| OnActiveSec         | Activacion del timer            | Monotonic |"
echo "| OnUnitActiveSec     | Activacion del service          | Monotonic |"
echo "| OnUnitInactiveSec   | Desactivacion del service       | Monotonic |"
echo ""
echo "--- Persistent= ---"
echo "  Solo tiene sentido con OnCalendar"
echo "  Los monotonic no necesitan Persistent"
echo "  (se recalculan desde el boot/activacion)"
echo ""
echo "--- AccuracySec= ---"
echo "  Aplica a ambos tipos"
echo "  Default: 1 minuto"
echo "  Para health checks: AccuracySec=10s o menos"
echo ""
echo "--- RandomizedDelaySec= ---"
echo "  Aplica a ambos tipos"
echo "  Agrega delay aleatorio"
echo "  Util para evitar thundering herd"
'
```

### Paso 3.3: Inspeccionar timers del sistema

```bash
docker compose exec debian-dev bash -c '
echo "=== Clasificar timers del sistema ==="
echo ""
echo "--- Listar todos los timers ---"
systemctl list-timers --all --no-pager 2>&1 | head -15 || \
    echo "(list-timers no disponible)"
echo ""
echo "--- Clasificar por tipo ---"
FOUND=0
for timer in $(systemctl list-unit-files --type=timer --no-pager --no-legend 2>/dev/null | awk "{print \$1}" | head -10); do
    FOUND=1
    CONTENT=$(systemctl cat "$timer" 2>/dev/null)
    HAS_CALENDAR=$(echo "$CONTENT" | grep -c "^OnCalendar" || true)
    HAS_MONO=$(echo "$CONTENT" | grep -cE "^On(Boot|Startup|Active|UnitActive|UnitInactive)Sec" || true)

    if [[ "$HAS_CALENDAR" -gt 0 ]] && [[ "$HAS_MONO" -gt 0 ]]; then
        TYPE="MIXTO (realtime + monotonic)"
    elif [[ "$HAS_CALENDAR" -gt 0 ]]; then
        TYPE="REALTIME (OnCalendar)"
    elif [[ "$HAS_MONO" -gt 0 ]]; then
        TYPE="MONOTONIC (On*Sec)"
    else
        TYPE="(sin directivas de tiempo)"
    fi

    echo "Timer: $timer → $TYPE"
    echo "$CONTENT" | grep -E "^On" 2>/dev/null || true
    echo ""
done
[[ "$FOUND" -eq 0 ]] && echo "(sin timers instalados)"
'
```

### Paso 3.4: Calcular ejecuciones

```bash
docker compose exec debian-dev bash -c '
echo "=== Calcular ejecuciones ==="
echo ""
echo "--- Ejercicio 1: boot + repeticion ---"
echo ""
echo "Timer:"
echo "  OnBootSec=5min"
echo "  OnUnitActiveSec=1h"
echo ""
echo "Boot a las 08:00, cada ejecucion tarda 10 min:"
echo ""
echo "  1a: 08:05  (boot + 5min)"
echo "  2a: 09:05  (08:05 activacion + 1h)"
echo "  3a: 10:05  (09:05 activacion + 1h)"
echo ""
echo "Nota: OnUnitActiveSec cuenta desde la ACTIVACION (inicio),"
echo "no desde que termina."
echo ""
echo "--- Ejercicio 2: usando OnUnitInactiveSec ---"
echo ""
echo "Timer:"
echo "  OnBootSec=5min"
echo "  OnUnitInactiveSec=1h"
echo ""
echo "Boot a las 08:00, cada ejecucion tarda 10 min:"
echo ""
echo "  1a: 08:05  (boot + 5min)"
echo "      08:15  service termina (inactive)"
echo "  2a: 09:15  (08:15 inactive + 1h)"
echo "      09:25  service termina"
echo "  3a: 10:25  (09:25 inactive + 1h)"
echo ""
echo "OnUnitInactiveSec siempre da 1h de pausa entre ejecuciones."
echo "OnUnitActiveSec da 50min de pausa (1h - 10min de ejecucion)."
echo ""
echo "--- Verificar con systemd-analyze calendar ---"
echo "Solo funciona para OnCalendar (realtime):"
systemd-analyze calendar "Mon..Fri *-*-* 09:00" --iterations=5 2>&1 || \
    echo "(systemd-analyze calendar no disponible)"
echo ""
echo "Para monotonic, no hay herramienta de verificacion."
echo "El calculo es manual: evento + tiempo = proxima ejecucion."
'
```

### Paso 3.5: Patrones comunes

```bash
docker compose exec debian-dev bash -c '
echo "=== Patrones comunes ==="
echo ""
echo "--- 1. Tarea a hora fija ---"
echo "[Timer]"
echo "OnCalendar=*-*-* 03:00:00"
echo "Persistent=true"
echo ""
echo "--- 2. Tarea al boot + repeticion ---"
echo "[Timer]"
echo "OnBootSec=5min"
echo "OnUnitActiveSec=6h"
echo ""
echo "--- 3. Health check frecuente ---"
echo "[Timer]"
echo "OnBootSec=1min"
echo "OnUnitActiveSec=5min"
echo "AccuracySec=10s"
echo ""
echo "--- 4. Sync con pausa garantizada ---"
echo "[Timer]"
echo "OnBootSec=3min"
echo "OnUnitInactiveSec=30min"
echo ""
echo "--- 5. Tarea a hora fija + al boot ---"
echo "[Timer]"
echo "OnCalendar=*-*-* 03:00:00"
echo "OnBootSec=2min"
echo "Persistent=true"
echo ""
echo "--- 6. User timer cada N horas ---"
echo "[Timer]"
echo "OnStartupSec=2h"
echo "OnUnitActiveSec=2h"
echo ""
echo "--- Resumen ---"
echo "  OnCalendar:          tareas con hora fija"
echo "  OnBootSec:           primera ejecucion post-boot"
echo "  OnUnitActiveSec:     repeticion (intervalo desde inicio)"
echo "  OnUnitInactiveSec:   repeticion (intervalo desde fin)"
echo "  OnStartupSec:        user timers (desde login)"
echo "  OnActiveSec:         ejecucion diferida (raro)"
'
```

---

## Limpieza final

No hay recursos que limpiar.

---

## Conceptos reforzados

1. Realtime (OnCalendar) usa el reloj del sistema y es sensible
   a cambios de hora. Monotonic (On*Sec) usa tiempo transcurrido
   desde un evento y no se ve afectado por cambios de hora.

2. Las 5 directivas monotonic: OnBootSec (boot), OnStartupSec
   (systemd/user manager), OnActiveSec (timer se activa),
   OnUnitActiveSec (service se activa), OnUnitInactiveSec
   (service termina).

3. OnUnitActiveSec cuenta desde el INICIO del service.
   OnUnitInactiveSec cuenta desde el FIN. Para tareas lentas,
   OnUnitInactiveSec da pausas mas predecibles.

4. Multiples directivas en un timer se combinan con OR.
   El patron mas comun: OnBootSec + OnUnitActiveSec para
   "ejecutar al boot y luego repetir cada N tiempo".

5. OnCalendar para tareas con hora fija (backups, reportes).
   On*Sec para intervalos fijos (health checks, sync, polling).

6. systemd-analyze calendar verifica expresiones OnCalendar.
   Para monotonic, el calculo es manual: evento + tiempo.
