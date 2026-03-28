# T03 — Monotonic vs realtime timers

## Erratas detectadas en el material base

| Archivo | Línea(s) | Error | Corrección |
|---|---|---|---|
| README.md | 72–73 | `Persistent=false` mostrado con `OnBootSec`, con comentario *"no ejecutar si el equipo ya lleva más de 10min"* | `Persistent=` **solo aplica a `OnCalendar`** (realtime). No tiene efecto en directivas monotonic. El propio labs (paso 3.2) lo dice correctamente: *"Solo tiene sentido con OnCalendar"*. |
| README.md | 127 | Cálculo: `OnUnitActiveSec: 00:00, 00:40, 01:10` con tarea de 10 min y 30 min de intervalo | El tercer valor debería ser **01:20**, no 01:10. Cálculo: 2ª ejecución empieza a las 00:40, termina a las 00:50 (active), + 30 min = **01:20**. |
| labs/README.md | 672–677 | Cálculo `OnUnitActiveSec=1h` con tarea de 10 min: muestra 08:05, 09:05, 10:05 | Para `Type=oneshot`, el estado "active" se alcanza al **completar** `ExecStart`, no al iniciarlo. Correcto: 08:05 inicia → 08:15 active → +1h = **09:15**; 09:15 inicia → 09:25 active → +1h = **10:25**. El README.md (líneas 440–443) calcula bien 09:15 y 10:25. |

---

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

---

## Realtime timers (OnCalendar)

Se basan en el **reloj del sistema** (fecha y hora reales). Se activan en
momentos absolutos del calendario:

```ini
[Timer]
OnCalendar=*-*-* 03:00:00          # cada día a las 03:00
OnCalendar=Mon *-*-* 09:00:00      # cada lunes a las 09:00
OnCalendar=*-*-01 00:00:00         # día 1 de cada mes
```

Características:
- Dependen del **wall clock** — si se cambia la hora, el timer se ajusta
- Pueden usar `Persistent=true` para ejecuciones perdidas
- Son el equivalente a las expresiones de cron

**Cuándo usar:** tareas a hora específica — backups nocturnos, reportes a
horarios fijos, cualquier tarea tipo "cron".

---

## Monotonic timers (On*Sec)

Se basan en **tiempo transcurrido** desde un evento. No dependen del reloj
del sistema.

### OnBootSec — Desde el boot del kernel

```ini
[Timer]
OnBootSec=5min
# 5 minutos después del boot
```

Útil para tareas de inicialización post-boot (verificar actualizaciones,
limpiezas iniciales).

### OnStartupSec — Desde que systemd arranca

```ini
[Timer]
OnStartupSec=2min
# 2 minutos después de que systemd (service manager) arranca
```

| Contexto | OnBootSec | OnStartupSec |
|---|---|---|
| System timer | Desde boot del kernel | Desde arranque de systemd (≈ igual, milisegundos de diferencia) |
| **User timer (`--user`)** | Desde boot del sistema | **Desde que el user manager arranca (login)** — puede ser horas después |

La diferencia es significativa solo para **user timers**.

### OnActiveSec — Desde la activación del timer

```ini
[Timer]
OnActiveSec=1h
# 1 hora después de que el timer se activa (enable+start)
```

Se cuenta desde que el `.timer` pasa a estado "active". Si el timer se
reinicia (`systemctl restart`), el contador se resetea. Ejecución única (no
se repite).

### OnUnitActiveSec — Desde que el service se activó

```ini
[Timer]
OnUnitActiveSec=30min
# 30 minutos después de que el service alcanzó el estado "active"
```

Crea un **loop de repetición**. Es la directiva más útil para tareas
repetitivas basadas en tiempo relativo.

**Punto crítico para `Type=oneshot`:** el service transiciona a "active"
**después** de que `ExecStart` completa, no cuando inicia. Esto significa que
`OnUnitActiveSec` cuenta desde la **finalización**, no desde el inicio.

```
Diferencia con OnCalendar (tarea que tarda 10 min, intervalo 30 min):

OnCalendar=*:00/30 (absoluto):
  00:00 ejecuta, termina 00:10
  00:30 ejecuta, termina 00:40
  01:00 ejecuta, termina 01:10
  → intervalo fijo desde el reloj (20 min de pausa real)

OnUnitActiveSec=30min (relativo, Type=oneshot):
  00:00 ejecuta, 00:10 active
  00:40 ejecuta, 00:50 active    (30 min desde 00:10)
  01:20 ejecuta, 01:30 active    (30 min desde 00:50)
  → intervalo fijo desde finalización (30 min de pausa real)
```

### OnUnitInactiveSec — Desde que el service se desactivó

```ini
[Timer]
OnUnitInactiveSec=15min
# 15 minutos después de que el service pasa a "inactive"
```

Similar a `OnUnitActiveSec`, pero cuenta desde que el service **termina**
(pasa a "inactive"). Para `Type=oneshot` la diferencia con
`OnUnitActiveSec` es mínima (el tránsito active→inactive es casi
instantáneo). La diferencia importa para services de tipo `simple` que
corren un tiempo.

```
Service que tarda 5 min, intervalo 30 min:

OnUnitActiveSec=30min:
  t=0   service inicia (activating)
  t=5   service completa → active → inactive
  t=35  30 min desde que se activó (t=5) → ejecutar de nuevo
  → 30 min de pausa

OnUnitInactiveSec=30min:
  t=0   service inicia
  t=5   service termina → inactive
  t=35  30 min desde que terminó (t=5) → ejecutar de nuevo
  → 30 min de pausa

(Para oneshot, ambas dan el mismo resultado porque active→inactive es instantáneo)
```

---

## Combinar directivas

Múltiples directivas en un timer se combinan con **OR** — el service se
activa cuando **cualquiera** se cumple.

### Boot + repetición (patrón más común)

```ini
[Timer]
OnBootSec=5min
OnUnitActiveSec=6h
# 5 min después del boot → ejecutar
# Luego cada 6 horas después de cada ejecución
```

Este patrón reemplaza a `@reboot` + `*/6 * * * *` en cron.

### Calendario + boot

```ini
[Timer]
OnCalendar=*-*-* 03:00:00
OnBootSec=2min
Persistent=true
# Cada día a las 03:00
# También 2 min después de cada boot
# Si se perdió la de las 03:00 → ejecutar al encender (Persistent)
```

### Múltiples OnCalendar

```ini
[Timer]
OnCalendar=Mon *-*-* 03:00:00
OnCalendar=Fri *-*-* 22:00:00
# Lunes a las 03:00 O viernes a las 22:00
```

---

## Unidades de tiempo

systemd acepta estas unidades en directivas `On*Sec`:

| Unidad | Abreviatura | Ejemplo |
|---|---|---|
| usec | us | `OnBootSec=500us` |
| msec | ms | `OnBootSec=100ms` |
| seconds | s | `OnBootSec=30s` |
| minutes | min | `OnBootSec=5min` |
| hours | h | `OnBootSec=2h` |
| days | d | `OnBootSec=1d` |
| weeks | w | `OnBootSec=1w` |
| months | M | `OnBootSec=1M` |
| years | y | `OnBootSec=1y` |

Se pueden combinar: `OnBootSec=1h 30min` (90 minutos),
`OnUnitActiveSec=2d 12h` (2 días y medio).

```bash
# Verificar con systemd-analyze:
systemd-analyze timespan "1h 30min"
systemd-analyze timespan "2d 12h"
```

---

## Tabla de directivas

| Directiva | Cuenta desde | Tipo | Uso típico |
|---|---|---|---|
| `OnCalendar` | Reloj del sistema | Realtime | Tareas a hora fija |
| `OnBootSec` | Boot del kernel | Monotonic | Inicialización |
| `OnStartupSec` | Arranque de systemd/user manager | Monotonic | Inicialización de usuario |
| `OnActiveSec` | Activación del timer | Monotonic | Ejecución diferida (raro) |
| `OnUnitActiveSec` | Activación del service | Monotonic | Repetición periódica |
| `OnUnitInactiveSec` | Desactivación del service | Monotonic | Repetición con pausa |

Notas:
- **`Persistent=`** solo tiene sentido con `OnCalendar`. Los timers monotonic
  se recalculan desde el boot/activación.
- **`AccuracySec=`** aplica a ambos tipos.
- **`RandomizedDelaySec=`** aplica a ambos tipos.

---

## Reloj monotónico vs reloj de pared

| Aspecto | Reloj de pared (wall clock) | Reloj monotónico |
|---|---|---|
| Usado por | `OnCalendar` | `OnBootSec`, `OnStartupSec`, `OnActiveSec`, `OnUnitActiveSec`, `OnUnitInactiveSec` |
| Puede cambiar | Sí (NTP, ajuste manual, DST) | No — solo avanza, nunca retrocede |
| Efecto de cambio | Timers se ajustan; si se atrasa, podría disparar dos veces | No se ve afectado |
| Incluye suspensión | N/A | Sí (systemd usa `CLOCK_BOOTTIME`) |

```bash
# Ver ambos relojes:
timedatectl                # reloj del sistema
cat /proc/uptime           # tiempo monotónico desde el boot
```

**Implicación práctica:** para tareas donde la **hora** importa (backups a
las 03:00), usar `OnCalendar`. Para tareas donde el **intervalo** importa
(health check cada 5 min), usar `On*Sec`.

---

## Elegir entre realtime y monotonic

| Usar `OnCalendar` (realtime) cuando... | Usar `On*Sec` (monotonic) cuando... |
|---|---|
| La tarea debe ejecutarse a hora específica | Importa el intervalo fijo entre ejecuciones |
| Reemplaza un cron job existente | La tarea debe ejecutarse N tiempo después del boot |
| El horario tiene significado de negocio | No importa la hora, solo la frecuencia |
| Necesitas `Persistent=true` para perdidas | Quieres evitar efectos de cambios de hora |
| Backups, reportes, rotación de logs | Health checks, sync, polling |

---

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
OnBootSec=1min
OnUnitActiveSec=5min
AccuracySec=10s
# Sin Persistent — si se pierde un check, no importa

[Install]
WantedBy=timers.target
```

¿Por qué `OnUnitActiveSec` y no `OnCalendar`? Con `OnCalendar=*:00/5`, si
la tarea tarda 3 min, a las `:05` se ignora porque el service aún corre. Con
`OnUnitActiveSec=5min`, siempre hay 5 min entre que completa y vuelve a
empezar.

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

`OnUnitInactiveSec` garantiza siempre 30 min de **pausa** entre ejecuciones,
independientemente de cuánto tarda el sync.

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

# Si falla, reintentar:
Restart=on-failure
RestartSec=10min

# Máximo de reintentos:
StartLimitIntervalSec=1h
StartLimitBurst=3
```

**Nota:** `Restart=on-failure` en oneshot solo se activa si `ExecStart`
falla. Cuando tiene éxito, el service pasa a inactive normalmente.

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

Aquí `OnStartupSec` cuenta desde el **user manager** (login), no desde el
boot. Si el usuario hace login a las 09:00: primer recordatorio a las 11:00,
segundo a las 13:00, tercero a las 15:00.

---

## Patrones comunes

| Patrón | Directivas | Caso de uso |
|---|---|---|
| Hora fija | `OnCalendar=...` + `Persistent=true` | Backups, reportes |
| Boot + repetición | `OnBootSec=` + `OnUnitActiveSec=` | Health checks, polling |
| Sync con pausa garantizada | `OnBootSec=` + `OnUnitInactiveSec=` | Sincronización de datos |
| Hora fija + boot | `OnCalendar=` + `OnBootSec=` + `Persistent=true` | Tarea crítica que no debe perderse |
| User timer periódico | `OnStartupSec=` + `OnUnitActiveSec=` | Recordatorios, scripts de usuario |
| Ejecución diferida | `OnActiveSec=` | Ejecución única N tiempo después de habilitar |

---

## Laboratorio

### Parte 1 — Directivas y relojes

#### Paso 1.1: Dos tipos de timer

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

#### Paso 1.2: OnCalendar (realtime)

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
OnCalendar=*-*-* 03:00:00
OnCalendar=Mon *-*-* 09:00:00
OnCalendar=*-*-01 00:00:00

# Multiples OnCalendar (OR):
OnCalendar=Mon *-*-* 03:00:00
OnCalendar=Fri *-*-* 22:00:00
EOF
echo ""
echo "Puede usar Persistent=true para tareas perdidas"
echo ""
echo "--- Verificar ---"
systemd-analyze calendar "*-*-* 03:00:00" --iterations=3 2>&1 || \
    echo "(systemd-analyze no disponible)"
'
```

#### Paso 1.3: OnBootSec y OnStartupSec

```bash
docker compose exec debian-dev bash -c '
echo "=== OnBootSec ==="
echo "Cuenta desde que el kernel inicia."
echo ""
echo "[Timer]"
echo "OnBootSec=5min"
echo ""
echo "=== OnStartupSec ==="
echo "Cuenta desde que systemd (service manager) arranca."
echo ""
echo "[Timer]"
echo "OnStartupSec=2min"
echo ""
echo "--- Diferencia ---"
echo ""
echo "Para system timers:"
echo "  OnBootSec ~ OnStartupSec (milisegundos de diferencia)"
echo ""
echo "Para user timers (--user):"
echo "  OnBootSec     = desde el boot del sistema"
echo "  OnStartupSec  = desde el login del usuario"
echo "  La diferencia puede ser de horas"
echo ""
echo "--- Uptime ---"
cat /proc/uptime 2>/dev/null | awk "{printf \"Uptime: %.0f seg (%.1f horas)\n\", \$1, \$1/3600}"
'
```

#### Paso 1.4: OnActiveSec

```bash
docker compose exec debian-dev bash -c '
echo "=== OnActiveSec ==="
echo ""
echo "Cuenta desde que el .timer pasa a estado \"active\"."
echo ""
echo "[Timer]"
echo "OnActiveSec=1h"
echo "# 1 hora despues de que el timer se activa (enable+start)"
echo ""
echo "Comportamiento:"
echo "  enable --now → timer se activa"
echo "  1 hora despues → ejecuta el service"
echo "  NO se repite (ejecucion unica)"
echo ""
echo "Si el timer se reinicia, el contador se resetea."
echo ""
echo "Diferencia con OnBootSec:"
echo "  OnBootSec:    siempre cuenta desde el boot (fijo)"
echo "  OnActiveSec:  se resetea con restart del timer"
'
```

#### Paso 1.5: OnUnitActiveSec y OnUnitInactiveSec

```bash
docker compose exec debian-dev bash -c '
echo "=== OnUnitActiveSec ==="
echo ""
echo "Cuenta desde que el service alcanzo el estado \"active\"."
echo ""
echo "Para Type=oneshot: \"active\" se alcanza al COMPLETAR ExecStart,"
echo "no al iniciarlo."
echo ""
echo "[Timer]"
echo "OnUnitActiveSec=30min"
echo ""
echo "Crea un loop: service completa → 30 min → service se ejecuta → ..."
echo ""
echo "--- Ejemplo con tarea de 10 min ---"
echo ""
echo "OnUnitActiveSec=30min:"
echo "  00:00 inicia (activating)"
echo "  00:10 completa → active"
echo "  00:40 30 min desde active → siguiente ejecucion"
echo "  00:50 completa → active"
echo "  01:20 30 min desde active → siguiente ejecucion"
echo ""
echo "=== OnUnitInactiveSec ==="
echo ""
echo "Cuenta desde que el service pasa a \"inactive\" (termina)."
echo ""
echo "Para Type=oneshot, active→inactive es casi instantaneo,"
echo "asi que OnUnitActiveSec y OnUnitInactiveSec dan resultados"
echo "practicamente identicos."
echo ""
echo "La diferencia importa para Type=simple (services que corren"
echo "un tiempo antes de terminar)."
'
```

#### Paso 1.6: Reloj de pared vs monotónico

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
echo "  Incluye tiempo en suspension (sleep/hibernate)"
echo ""
echo "--- Ver ambos relojes ---"
echo "Reloj del sistema:"
timedatectl 2>/dev/null | grep -E "(Local|Universal)" || \
    date "+  %Y-%m-%d %H:%M:%S %Z"
echo ""
echo "Tiempo monotonico (uptime):"
cat /proc/uptime 2>/dev/null | awk "{printf \"  %.0f seg (%.1f horas)\n\", \$1, \$1/3600}"
'
```

---

### Parte 2 — Combinaciones y ejemplos

#### Paso 2.1: Combinar directivas (OR)

```bash
docker compose exec debian-dev bash -c '
echo "=== Combinar directivas ==="
echo ""
echo "Multiples directivas se combinan con OR."
echo "El service se activa cuando CUALQUIERA se cumple."
echo ""
echo "--- Boot + repeticion ---"
echo "[Timer]"
echo "OnBootSec=5min"
echo "OnUnitActiveSec=6h"
echo "# Equivale a @reboot + repeticion en cron"
echo ""
echo "--- Calendario + boot ---"
echo "[Timer]"
echo "OnCalendar=*-*-* 03:00:00"
echo "OnBootSec=2min"
echo "Persistent=true"
echo ""
echo "--- Multiples OnCalendar ---"
echo "[Timer]"
echo "OnCalendar=Mon *-*-* 03:00:00"
echo "OnCalendar=Fri *-*-* 22:00:00"
echo "# Lunes a las 03:00 O viernes a las 22:00"
'
```

#### Paso 2.2: Unidades de tiempo

```bash
docker compose exec debian-dev bash -c '
echo "=== Unidades de tiempo ==="
echo ""
printf "%-12s %-8s %-20s\n" "Unidad" "Abrev." "Ejemplo"
printf "%-12s %-8s %-20s\n" "usec"    "us"  "OnBootSec=500us"
printf "%-12s %-8s %-20s\n" "msec"    "ms"  "OnBootSec=100ms"
printf "%-12s %-8s %-20s\n" "seconds" "s"   "OnBootSec=30s"
printf "%-12s %-8s %-20s\n" "minutes" "min" "OnBootSec=5min"
printf "%-12s %-8s %-20s\n" "hours"   "h"   "OnBootSec=2h"
printf "%-12s %-8s %-20s\n" "days"    "d"   "OnBootSec=1d"
printf "%-12s %-8s %-20s\n" "weeks"   "w"   "OnBootSec=1w"
printf "%-12s %-8s %-20s\n" "months"  "M"   "OnBootSec=1M"
printf "%-12s %-8s %-20s\n" "years"   "y"   "OnBootSec=1y"
echo ""
echo "--- Combinaciones ---"
echo "  OnBootSec=1h 30min        → 90 minutos"
echo "  OnUnitActiveSec=2d 12h    → 2 dias y 12 horas"
echo ""
echo "--- Verificar con systemd-analyze ---"
systemd-analyze timespan "1h 30min" 2>&1 || \
    echo "(systemd-analyze timespan no disponible)"
systemd-analyze timespan "2d 12h" 2>&1 || true
'
```

#### Paso 2.3: Ejemplo — Health check

```bash
docker compose exec debian-dev bash -c '
echo "=== Ejemplo: health check ==="
echo ""
cat << '\''EOF'\''
# health-check.timer
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
echo "OnCalendar=*:00/5 (absoluto):"
echo "  Si la tarea tarda 3 min, el disparo de :05 se ignora"
echo ""
echo "OnUnitActiveSec=5min (relativo):"
echo "  5 min desde que completa → siempre hay 5 min de pausa"
echo ""
echo "Sin Persistent — health checks evaluan el estado actual."
'
```

#### Paso 2.4: Ejemplo — Sync periódico y backup con retry

```bash
docker compose exec debian-dev bash -c '
echo "=== Sync periodico ==="
echo ""
echo "[Timer]"
echo "OnBootSec=3min"
echo "OnUnitInactiveSec=30min"
echo "# Siempre 30 min de pausa entre ejecuciones"
echo ""
echo "=== Backup con retry ==="
echo ""
cat << '\''EOF'\''
# backup.timer
[Timer]
OnCalendar=*-*-* 03:00:00
Persistent=true
RandomizedDelaySec=15min

# backup.service
[Service]
Type=oneshot
ExecStart=/opt/backup/run.sh
Restart=on-failure
RestartSec=10min
StartLimitIntervalSec=1h
StartLimitBurst=3
EOF
echo ""
echo "Nota: Restart=on-failure en oneshot solo actua si ExecStart falla."
'
```

#### Paso 2.5: Ejemplo — User timer

```bash
docker compose exec debian-dev bash -c '
echo "=== User timer ==="
echo ""
cat << '\''EOF'\''
# ~/.config/systemd/user/break-reminder.timer
[Timer]
OnStartupSec=2h
OnUnitActiveSec=2h
[Install]
WantedBy=timers.target
EOF
echo ""
echo "OnStartupSec en user timers cuenta desde el LOGIN."
echo ""
echo "Login a las 09:00:"
echo "  11:00 → primer recordatorio"
echo "  13:00 → segundo"
echo "  15:00 → tercero"
echo ""
echo "Gestion:"
echo "  systemctl --user enable --now break-reminder.timer"
echo "  loginctl enable-linger user  ← correr sin sesion"
'
```

---

### Parte 3 — Elegir y analizar

#### Paso 3.1: Criterios de elección

```bash
docker compose exec debian-dev bash -c '
echo "=== Cuando usar cada tipo ==="
echo ""
echo "--- OnCalendar (realtime) ---"
echo "  Tarea a hora especifica"
echo "  Reemplaza cron job"
echo "  Horario con significado de negocio"
echo "  Necesitas Persistent=true"
echo ""
echo "--- On*Sec (monotonic) ---"
echo "  Intervalo fijo entre ejecuciones"
echo "  Tarea N tiempo despues del boot"
echo "  No importa la hora, solo la frecuencia"
echo "  Evitar efectos de cambios de hora"
echo ""
printf "%-28s %-10s %-24s\n" "Escenario" "Tipo" "Directiva"
printf "%-28s %-10s %-24s\n" "Backup a las 03:00"       "Realtime"  "OnCalendar=...03:00"
printf "%-28s %-10s %-24s\n" "Health check cada 2 min"   "Monotonic" "OnUnitActiveSec=2min"
printf "%-28s %-10s %-24s\n" "Verificar al encender"     "Monotonic" "OnBootSec=5min"
printf "%-28s %-10s %-24s\n" "Rotacion logs semanal"     "Realtime"  "OnCalendar=weekly"
printf "%-28s %-10s %-24s\n" "Sync cada 30 min"          "Monotonic" "OnUnitInactiveSec=30m"
printf "%-28s %-10s %-24s\n" "Polling API cada 10s"      "Monotonic" "OnUnitActiveSec=10s"
'
```

#### Paso 3.2: Tabla de directivas y notas

```bash
docker compose exec debian-dev bash -c '
echo "=== Tabla de directivas ==="
echo ""
printf "%-22s %-32s %-10s\n" "Directiva" "Cuenta desde" "Tipo"
printf "%-22s %-32s %-10s\n" "OnCalendar"         "Reloj del sistema"           "Realtime"
printf "%-22s %-32s %-10s\n" "OnBootSec"           "Boot del kernel"             "Monotonic"
printf "%-22s %-32s %-10s\n" "OnStartupSec"        "systemd/user manager"        "Monotonic"
printf "%-22s %-32s %-10s\n" "OnActiveSec"         "Activacion del timer"        "Monotonic"
printf "%-22s %-32s %-10s\n" "OnUnitActiveSec"     "Activacion del service"      "Monotonic"
printf "%-22s %-32s %-10s\n" "OnUnitInactiveSec"   "Desactivacion del service"   "Monotonic"
echo ""
echo "--- Persistent= ---"
echo "  Solo tiene sentido con OnCalendar"
echo "  Los monotonic no necesitan Persistent"
echo "  (se recalculan desde el boot/activacion)"
echo ""
echo "--- AccuracySec= ---"
echo "  Aplica a ambos tipos (default: 1 min)"
echo ""
echo "--- RandomizedDelaySec= ---"
echo "  Aplica a ambos tipos"
'
```

#### Paso 3.3: Inspeccionar timers del sistema

```bash
docker compose exec debian-dev bash -c '
echo "=== Clasificar timers del sistema ==="
echo ""
FOUND=0
for timer in $(systemctl list-unit-files --type=timer --no-pager --no-legend 2>/dev/null | awk "{print \$1}" | head -10); do
    FOUND=1
    CONTENT=$(systemctl cat "$timer" 2>/dev/null)
    HAS_CAL=$(echo "$CONTENT" | grep -c "^OnCalendar" || true)
    HAS_MONO=$(echo "$CONTENT" | grep -cE "^On(Boot|Startup|Active|UnitActive|UnitInactive)Sec" || true)

    if [[ "$HAS_CAL" -gt 0 ]] && [[ "$HAS_MONO" -gt 0 ]]; then
        TYPE="MIXTO"
    elif [[ "$HAS_CAL" -gt 0 ]]; then
        TYPE="REALTIME"
    elif [[ "$HAS_MONO" -gt 0 ]]; then
        TYPE="MONOTONIC"
    else
        TYPE="(sin directivas)"
    fi
    echo "Timer: $timer → $TYPE"
    echo "$CONTENT" | grep -E "^On" 2>/dev/null || true
    echo ""
done
[[ "$FOUND" -eq 0 ]] && echo "(sin timers instalados)"
'
```

#### Paso 3.4: Calcular ejecuciones

```bash
docker compose exec debian-dev bash -c '
echo "=== Calcular ejecuciones ==="
echo ""
echo "--- OnUnitActiveSec (cuenta desde activacion/completado) ---"
echo ""
echo "Timer: OnBootSec=5min, OnUnitActiveSec=1h"
echo "Boot a 08:00, cada ejecucion tarda 10 min (Type=oneshot):"
echo ""
echo "  1a: 08:05  inicia (boot + 5min)"
echo "      08:15  completa → active"
echo "  2a: 09:15  inicia (08:15 active + 1h)"
echo "      09:25  completa → active"
echo "  3a: 10:25  inicia (09:25 active + 1h)"
echo ""
echo "--- OnUnitInactiveSec (cuenta desde que termina) ---"
echo ""
echo "Timer: OnBootSec=5min, OnUnitInactiveSec=1h"
echo "Boot a 08:00, cada ejecucion tarda 10 min:"
echo ""
echo "  1a: 08:05  inicia"
echo "      08:15  termina → inactive"
echo "  2a: 09:15  inicia (08:15 inactive + 1h)"
echo "      09:25  termina → inactive"
echo "  3a: 10:25  inicia (09:25 inactive + 1h)"
echo ""
echo "Para oneshot, ambas dan resultados practicamente iguales"
echo "(active→inactive es instantaneo)."
echo ""
echo "--- systemd-analyze calendar (solo para OnCalendar) ---"
systemd-analyze calendar "Mon..Fri *-*-* 09:00" --iterations=3 2>&1 || \
    echo "(no disponible)"
echo ""
echo "Para monotonic, el calculo es manual."
'
```

#### Paso 3.5: Patrones comunes

```bash
docker compose exec debian-dev bash -c '
echo "=== Patrones comunes ==="
echo ""
echo "1. Tarea a hora fija:"
echo "   OnCalendar=*-*-* 03:00:00 + Persistent=true"
echo ""
echo "2. Boot + repeticion:"
echo "   OnBootSec=5min + OnUnitActiveSec=6h"
echo ""
echo "3. Health check frecuente:"
echo "   OnBootSec=1min + OnUnitActiveSec=5min + AccuracySec=10s"
echo ""
echo "4. Sync con pausa garantizada:"
echo "   OnBootSec=3min + OnUnitInactiveSec=30min"
echo ""
echo "5. Tarea critica (hora + boot):"
echo "   OnCalendar=...03:00 + OnBootSec=2min + Persistent=true"
echo ""
echo "6. User timer periodico:"
echo "   OnStartupSec=2h + OnUnitActiveSec=2h"
echo ""
echo "Resumen:"
echo "  OnCalendar:          hora fija"
echo "  OnBootSec:           primera ejecucion post-boot"
echo "  OnUnitActiveSec:     repeticion (desde inicio/completado)"
echo "  OnUnitInactiveSec:   repeticion (desde fin)"
echo "  OnStartupSec:        user timers (desde login)"
echo "  OnActiveSec:         ejecucion diferida (raro)"
'
```

---

## Ejercicios

### Ejercicio 1 — Identificar el tipo

¿Realtime o monotonic para cada caso?

1. Backup a las 03:00
2. Health check cada 2 minutos
3. Verificación de actualizaciones al encender
4. Rotación de logs semanal
5. Sync de datos cada 30 min después de terminar el anterior

<details><summary>Predicción</summary>

1. **Realtime** — `OnCalendar=*-*-* 03:00:00` (hora fija)
2. **Monotonic** — `OnUnitActiveSec=2min` (intervalo fijo)
3. **Monotonic** — `OnBootSec=5min` (relativo al boot)
4. **Realtime** — `OnCalendar=weekly` (calendario)
5. **Monotonic** — `OnUnitInactiveSec=30min` (pausa fija entre ejecuciones)

</details>

### Ejercicio 2 — OnBootSec vs OnStartupSec

¿Cuál es la diferencia entre `OnBootSec=30min` y `OnStartupSec=30min` en un
**user timer**? Si el sistema arrancó a las 08:00 y el usuario hizo login a
las 09:00, ¿cuándo se ejecutaría cada uno?

<details><summary>Predicción</summary>

- **`OnBootSec=30min`:** cuenta desde el boot del sistema → 08:00 + 30min =
  **08:30**. Como el user manager no existía a las 08:30 (login fue a las
  09:00), la ejecución se dispara **inmediatamente** al hacer login (ya
  pasaron los 30 min desde boot).
- **`OnStartupSec=30min`:** cuenta desde el arranque del user manager
  (login) → 09:00 + 30min = **09:30**.

Para system timers, la diferencia es despreciable (milisegundos entre kernel
y systemd).

</details>

### Ejercicio 3 — OnUnitActiveSec vs OnCalendar

Un timer tiene `OnCalendar=*:00/10` (cada 10 min). Otro tiene
`OnUnitActiveSec=10min`. Si la tarea tarda 4 minutos, ¿cuáles son las
primeras 3 ejecuciones de cada uno (empezando a las 12:00)?

<details><summary>Predicción</summary>

**OnCalendar=*:00/10 (absoluto):**
```
12:00 inicia, 12:04 termina
12:10 inicia, 12:14 termina
12:20 inicia, 12:24 termina
→ 6 min de pausa entre ejecuciones
```

**OnUnitActiveSec=10min (Type=oneshot, relativo):**
```
12:00 inicia, 12:04 completa (active)
12:14 inicia, 12:18 completa (active)    (12:04 + 10min)
12:28 inicia, 12:32 completa (active)    (12:18 + 10min)
→ 10 min desde completado, 6 min de pausa real
```

OnCalendar dispara a tiempos fijos del reloj. OnUnitActiveSec espera 10 min
desde que el service alcanzó "active" (completó para oneshot).

</details>

### Ejercicio 4 — Combinar directivas (OR)

¿Qué hace este timer?

```ini
[Timer]
OnBootSec=2min
OnCalendar=*-*-* 03:00:00
Persistent=true
```

<details><summary>Predicción</summary>

El service se ejecuta en **dos** situaciones (OR):
1. **2 minutos después de cada boot** (monotonic)
2. **Cada día a las 03:00** (realtime)

Con `Persistent=true`: si el equipo estaba apagado a las 03:00, la tarea
se ejecuta al encender (además de la ejecución por `OnBootSec`).

Si el equipo arranca a las 08:00: se ejecuta a las 08:02 (OnBootSec) y
luego a las 03:00 del día siguiente (OnCalendar). Si estuvo apagado durante
las 03:00, Persistent dispara una ejecución inmediata, que podría coincidir
con OnBootSec.

</details>

### Ejercicio 5 — Calcular ejecuciones OnUnitActiveSec

Timer: `OnBootSec=5min`, `OnUnitActiveSec=1h`. Boot a las 08:00. Cada
ejecución tarda 10 minutos (`Type=oneshot`). ¿A qué hora empiezan la 1ª,
2ª y 3ª ejecución?

<details><summary>Predicción</summary>

```
1ª: 08:05  inicia (boot 08:00 + 5min)
    08:15  completa → active

2ª: 09:15  inicia (08:15 active + 1h)
    09:25  completa → active

3ª: 10:25  inicia (09:25 active + 1h)
    10:35  completa → active
```

`OnUnitActiveSec` cuenta desde que el service alcanzó "active", que para
`Type=oneshot` es **al completar** `ExecStart`, no al iniciarlo. Por eso
los 10 minutos de ejecución se "acumulan" en cada ciclo.

</details>

### Ejercicio 6 — OnUnitActiveSec vs OnUnitInactiveSec

Mismo escenario que el ejercicio 5, pero con `OnUnitInactiveSec=1h` en lugar
de `OnUnitActiveSec`. ¿Cambian los tiempos?

<details><summary>Predicción</summary>

Para `Type=oneshot`, los tiempos son **prácticamente iguales**:

```
1ª: 08:05  inicia
    08:15  completa → active → inactive (instantáneo)

2ª: 09:15  inicia (08:15 inactive + 1h)
    09:25  completa → active → inactive

3ª: 10:25  inicia (09:25 inactive + 1h)
```

Para oneshot, el tránsito active→inactive es instantáneo, así que
`OnUnitActiveSec` y `OnUnitInactiveSec` dan el mismo resultado. La
diferencia importa para `Type=simple`, donde el service permanece en
"active" mientras corre.

</details>

### Ejercicio 7 — Persistent y monotonic

¿Por qué `Persistent=true` solo tiene sentido con `OnCalendar` y no con
directivas monotonic como `OnBootSec`?

<details><summary>Predicción</summary>

`Persistent=true` compensa **ejecuciones perdidas mientras el sistema estuvo
apagado**. Guarda el timestamp de la última ejecución y, al arrancar, verifica
si se perdió alguna.

Los timers monotonic **no necesitan esto** porque:
- `OnBootSec` se recalcula desde cada boot — siempre se ejecutará N tiempo
  después de arrancar
- `OnUnitActiveSec`/`OnUnitInactiveSec` se recalculan desde la última
  ejecución del service — no hay concepto de "perderse"
- `OnActiveSec` se recalcula desde la activación del timer

Solo `OnCalendar` referencia momentos absolutos del reloj que pueden "pasar"
mientras el sistema está apagado.

</details>

### Ejercicio 8 — User timer con OnStartupSec

Escribe un timer de usuario que muestre una notificación 1 hora después del
login y luego cada 90 minutos. ¿Dónde va el archivo?

<details><summary>Predicción</summary>

```ini
# ~/.config/systemd/user/reminder.timer
[Unit]
Description=Recordatorio periódico

[Timer]
OnStartupSec=1h
OnUnitActiveSec=1h 30min

[Install]
WantedBy=timers.target
```

```ini
# ~/.config/systemd/user/reminder.service
[Unit]
Description=Mostrar recordatorio

[Service]
Type=oneshot
ExecStart=/usr/bin/notify-send "Recordatorio periódico"
```

Va en `~/.config/systemd/user/`. Se activa con:
```bash
systemctl --user daemon-reload
systemctl --user enable --now reminder.timer
```

`OnStartupSec` cuenta desde el login (user manager), no desde el boot.

</details>

### Ejercicio 9 — Efecto de cambio de hora

Un timer tiene `OnCalendar=*-*-* 15:00`. NTP adelanta el reloj de 14:30 a
15:30. ¿Qué pasa? ¿Y si el timer fuera `OnUnitActiveSec=30min`?

<details><summary>Predicción</summary>

- **OnCalendar:** al adelantar el reloj, systemd detecta que las 15:00 ya
  pasaron. El timer se ejecuta **inmediatamente** (o espera hasta mañana a las
  15:00 si ya se registró una ejecución de hoy). Los timers realtime son
  sensibles a cambios de hora.
- **OnUnitActiveSec:** el reloj monotónico **no cambia** por NTP. Si faltaban
  30 min, siguen faltando 30 min. El cambio de hora no afecta a los timers
  monotonic.

Esta es la razón principal para usar timers monotonic en health checks y
polling: los cambios de hora de NTP no causan ejecuciones inesperadas.

</details>

### Ejercicio 10 — Diseñar un timer completo

Un servidor necesita:
- Sincronizar datos 3 min después del boot
- Repetir la sincronización cada 20 min (medido desde que termina)
- Si la sincronización falla, que no bloquee la siguiente
- Precisión de 5 segundos

Escribe el `.timer` y justifica cada directiva.

<details><summary>Predicción</summary>

```ini
# /etc/systemd/system/data-sync.timer
[Unit]
Description=Sincronización periódica de datos

[Timer]
OnBootSec=3min
OnUnitInactiveSec=20min
AccuracySec=5s

[Install]
WantedBy=timers.target
```

Justificación:
- **`OnBootSec=3min`:** primera sincronización 3 min después del boot
  (monotonic, no afectado por hora)
- **`OnUnitInactiveSec=20min`:** 20 min desde que el service **termina**
  (inactive). Garantiza 20 min de pausa real independientemente de cuánto
  tarde el sync.
- **`AccuracySec=5s`:** precisión de 5 segundos (el default de 1 min sería
  demasiado impreciso para un sync frecuente)
- No se usa `Persistent=true` porque es monotonic (se recalcula desde
  boot/inactivación)
- No se usa `OnUnitActiveSec` porque queremos pausa fija desde la
  **finalización**, no desde el inicio

Para el service:
```ini
[Service]
Type=oneshot
ExecStart=/opt/sync/run.sh
TimeoutStartSec=600
```

`TimeoutStartSec=600` evita que un sync colgado bloquee la cola (mata el
proceso si tarda más de 10 minutos).

</details>
