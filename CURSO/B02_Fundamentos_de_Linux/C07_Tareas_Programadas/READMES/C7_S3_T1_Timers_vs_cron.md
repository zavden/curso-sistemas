# T01 — Timers vs cron

> **Erratas:** Sin erratas detectadas en el material base.

---

## Qué son los systemd timers

Un timer es una unidad de systemd (`.timer`) que **activa otra unidad**
(típicamente un `.service`) en momentos determinados. Es el mecanismo nativo
de systemd para programar tareas:

```bash
systemctl list-timers --all --no-pager
# NEXT                        LEFT       LAST                        PASSED     UNIT               ACTIVATES
# Tue 2026-03-17 18:09:00 UTC 23min left Tue 2026-03-17 17:09:00 UTC 36min ago  fstrim.timer       fstrim.service
# Wed 2026-03-18 00:00:00 UTC 6h left    Tue 2026-03-17 00:00:00 UTC 17h ago    logrotate.timer    logrotate.service
```

| Columna | Significado |
|---|---|
| NEXT | Próxima ejecución programada |
| LEFT | Tiempo restante |
| LAST | Última ejecución |
| PASSED | Hace cuánto fue |
| UNIT | El timer |
| ACTIVATES | El service que activa |

---

## Arquitectura: timer + service

Un timer **siempre** necesita un service asociado. El timer define **cuándo**;
el service define **qué ejecutar**:

```
fstrim.timer
     │
     │  cuando se cumple la condición de tiempo
     ▼
fstrim.service
     │
     │  ExecStart=
     ▼
/usr/sbin/fstrim --all
```

```bash
# Asociación por nombre (automática):
# myapp.timer  →  activa  →  myapp.service

# Para activar un service con nombre diferente:
# [Timer]
# Unit=otro-nombre.service
```

---

## Ventajas sobre cron

### 1. Integración con el journal

```bash
# cron: la salida va a mail (si hay MTA) o se pierde
# timer: logs completos, persistentes, filtrables
journalctl -u fstrim.service --no-pager -n 10
```

### 2. Dependencias y orden

```bash
# cron: las tareas son independientes, sin orden
# timer: el .service puede declarar dependencias
# [Unit]
# After=network-online.target
# Requires=postgresql.service
# La tarea no se ejecuta hasta que las dependencias estén listas
```

### 3. Control de recursos (cgroups)

```bash
# cron: la tarea consume lo que quiera (solo nice/ionice manuales)
# timer (en el .service):
# [Service]
# CPUQuota=50%
# MemoryMax=512M
# IOWeight=100
```

### 4. Ejecuciones perdidas (Persistent=)

```bash
# cron: si el equipo estaba apagado, la tarea se pierde
#       (necesitas anacron, y solo para daily/weekly/monthly)
# timer:
# [Timer]
# Persistent=true
# Si el equipo estaba apagado, se ejecuta al encender
# Funciona para CUALQUIER intervalo
```

### 5. Precisión y aleatorización

```bash
# cron: precisión mínima de 1 minuto, sin aleatorización
# timer:
# [Timer]
# OnCalendar=*-*-* 03:00:00
# AccuracySec=1us              # precisión de microsegundo
# RandomizedDelaySec=1h        # evita thundering herd
```

### 6. Seguridad y sandboxing

```bash
# cron: solo el UID del usuario limita los permisos
# timer (en el .service):
# [Service]
# DynamicUser=true
# ProtectSystem=strict
# PrivateTmp=true
# NoNewPrivileges=true
```

### 7. Estado y diagnóstico

```bash
# cron: ¿se ejecutó? Revisar logs de cron (si existen) o mail
# timer:
systemctl status myapp.timer
# Active: active (waiting)
# Trigger: Wed 2026-03-18 03:00:00 UTC; 11h left

systemctl status myapp.service
# Active: inactive (dead) since ...
# Process: 12345 ExecStart=... (code=exited, status=0/SUCCESS)
```

---

## Desventajas frente a cron

| Desventaja | Detalle |
|---|---|
| Verbosidad | Dos archivos (`.timer` + `.service`) en lugar de una línea |
| Curva de aprendizaje | OnCalendar con su propia sintaxis + unit files |
| Sin `crontab -e` | Para user timers: crear archivos en `~/.config/systemd/user/` |
| Portabilidad | Solo sistemas con systemd (no BSD, macOS, Solaris) |
| Edición rápida | Editar archivo → `daemon-reload` → restart timer |

---

## OnCalendar — la sintaxis de tiempo

### Formato general

```
DiaDeLaSemana YYYY-MM-DD HH:MM:SS
```

Cada parte es opcional:

```bash
OnCalendar=03:00              # cada día a las 03:00
OnCalendar=Mon 03:00          # cada lunes a las 03:00
OnCalendar=2026-03-20         # el 20 de marzo de 2026
OnCalendar=*-*-01 06:00       # día 1 de cada mes a las 06:00
```

### Comodines, rangos, listas e intervalos

| Operador | Significado | Ejemplo |
|---|---|---|
| `*` | Cualquier valor | `*-*-* 03:00:00` — cada día |
| `..` | Rango | `Mon..Fri` — lunes a viernes |
| `,` | Lista | `Mon,Wed,Fri` — lun, mié, vie |
| `/` | Intervalo (paso) | `00/15` — cada 15 (0, 15, 30, 45) |

```bash
OnCalendar=*-*-* *:00/15:00         # cada 15 minutos
OnCalendar=*-*-* 00/2:00            # cada 2 horas
OnCalendar=Mon..Fri *-*-* 09:00     # lun-vie a las 09:00
OnCalendar=Mon,Wed,Fri 08:00        # lun, mié, vie a las 08:00
```

### Expresiones predefinidas

| Expresión | Equivalente | Cron equivalente |
|---|---|---|
| `minutely` | `*-*-* *:*:00` | `* * * * *` |
| `hourly` | `*-*-* *:00:00` | `0 * * * *` |
| `daily` | `*-*-* 00:00:00` | `0 0 * * *` |
| `weekly` | `Mon *-*-* 00:00:00` | `0 0 * * 0` |
| `monthly` | `*-*-01 00:00:00` | `0 0 1 * *` |
| `yearly` | `*-01-01 00:00:00` | `0 0 1 1 *` |
| `quarterly` | `*-01,04,07,10-01 00:00:00` | — |
| `semiannually` | `*-01,07-01 00:00:00` | — |

**Nota:** `@reboot` de cron no tiene equivalente en OnCalendar, pero sí en
el timer: `OnBootSec=0` o `OnStartupSec=0` (se cubren en el tópico de
monotonic timers).

### Verificar expresiones con systemd-analyze

```bash
# Una ejecución:
systemd-analyze calendar "Mon..Fri *-*-* 09:00"
#   Original form: Mon..Fri *-*-* 09:00
#  Normalized form: Mon..Fri *-*-* 09:00:00
#     Next elapse: Wed 2026-03-18 09:00:00 UTC
#        From now: 15h left

# Varias ejecuciones futuras:
systemd-analyze calendar --iterations=5 "Mon..Fri *-*-* 09:00"

# Verificar una expresión compleja:
systemd-analyze calendar "*-*-* 00/6:00"
# Cada 6 horas: 00:00, 06:00, 12:00, 18:00
```

### Conversión cron → OnCalendar

| Cron | OnCalendar |
|---|---|
| `* * * * *` | `minutely` o `*-*-* *:*:00` |
| `0 * * * *` | `hourly` o `*-*-* *:00:00` |
| `0 0 * * *` | `daily` o `*-*-* 00:00:00` |
| `0 0 * * 0` | `weekly` o `Sun *-*-* 00:00:00` |
| `0 0 1 * *` | `monthly` o `*-*-01 00:00:00` |
| `*/5 * * * *` | `*-*-* *:00/5:00` |
| `30 2 * * *` | `*-*-* 02:30:00` |
| `0 9-17 * * 1-5` | `Mon..Fri *-*-* 09..17:00:00` |
| `0 6-22/2 * * *` | `*-*-* 06..22/2:00:00` |

---

## AccuracySec — Precisión

Por defecto, systemd **agrupa** ejecuciones cercanas para ahorrar wake-ups de
CPU (eficiencia energética):

```bash
# [Timer]
# AccuracySec=1min    ← default
# Una tarea de las 03:00 puede ejecutarse entre 03:00 y 03:01

# Mayor precisión:
# AccuracySec=1s       precisión de 1 segundo
# AccuracySec=1us      precisión de microsegundo

# Menor precisión (ahorro de energía en laptops):
# AccuracySec=1h       puede desviarse hasta 1 hora
```

---

## RandomizedDelaySec — Evitar thundering herd

Cuando muchas máquinas tienen el mismo timer, todas golpean el mismo recurso
al mismo tiempo. `RandomizedDelaySec` agrega un delay aleatorio por máquina:

```bash
# [Timer]
# OnCalendar=*-*-* 03:00
# RandomizedDelaySec=30min
# Ejecuta entre 03:00 y 03:30 (aleatorio por máquina)
```

Útil para:
- Actualizaciones que consultan un repositorio central
- Backups a un servidor NFS compartido
- Reportes que consultan una API con rate limit

---

## Tabla comparativa completa

| Aspecto | cron | anacron | systemd timer |
|---|---|---|---|
| Precisión mínima | 1 minuto | 1 día | 1 microsegundo |
| Tareas perdidas | Se pierden | Se ejecutan después | `Persistent=true` |
| Logs | Mail / syslog | syslog | journal (integrado) |
| Dependencias | No | No | Sí (`After=`, `Requires=`) |
| Recursos (cgroups) | No | No | Sí (`CPUQuota=`, etc.) |
| Seguridad | Solo UID | Solo root | Sandboxing completo |
| Estado | `crontab -l` | `/var/spool/anacron/` | `systemctl status` |
| Usuarios | `crontab -e` | Solo root | User units |
| Portabilidad | Unix universal | Linux | Solo systemd |
| Complejidad | Baja | Baja | Media |
| Archivos necesarios | 1 línea | 1 línea en anacrontab | 2 archivos (.timer + .service) |
| Aleatorización | No | `RANDOM_DELAY` | `RandomizedDelaySec` |

---

## Timers del sistema — ejemplos reales

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

## Cuándo usar cada herramienta

| Herramienta | Usar cuando... |
|---|---|
| **cron** | Tareas simples y repetitivas; entornos sin systemd; portabilidad importa; edición rápida (`crontab -e`) |
| **anacron** | Laptops/escritorios que se apagan; tareas daily/weekly/monthly sin hora exacta |
| **systemd timers** | Servidores con systemd; dependencias o sandboxing necesarios; `Persistent=` para intervalos < 1 día; logs en journal; precisión sub-minuto |
| **at** | Ejecución única programada (`systemd-run --on-active=` como alternativa) |
| **batch** | Tareas pesadas cuando hay recursos libres (`nice + ionice` como alternativa) |

---

## Laboratorio

### Parte 1 — Arquitectura y ventajas

#### Paso 1.1: Arquitectura timer + service

```bash
docker compose exec debian-dev bash -c '
echo "=== Arquitectura: timer + service ==="
echo ""
echo "Un timer siempre necesita un service asociado:"
echo ""
echo "  fstrim.timer"
echo "       │"
echo "       │ cuando se cumple la condicion de tiempo"
echo "       ▼"
echo "  fstrim.service"
echo "       │"
echo "       │ ExecStart="
echo "       ▼"
echo "  /usr/sbin/fstrim --all"
echo ""
echo "--- Asociacion por nombre ---"
echo "myapp.timer → activa → myapp.service (automatico)"
echo ""
echo "--- Nombre diferente ---"
echo "[Timer]"
echo "Unit=otro-nombre.service"
echo ""
echo "--- Listar timers activos ---"
systemctl list-timers --all --no-pager 2>&1 | head -10 || \
    echo "(systemctl no disponible como esperado en contenedor)"
'
```

#### Paso 1.2: Ventajas sobre cron

```bash
docker compose exec debian-dev bash -c '
echo "=== Ventajas de timers sobre cron ==="
echo ""
echo "1. JOURNAL INTEGRADO"
echo "   cron: salida va a mail (si hay MTA) o se pierde"
echo "   timer: journalctl -u myapp.service (logs completos)"
echo ""
echo "2. DEPENDENCIAS"
echo "   cron: tareas independientes, sin orden"
echo "   timer: After=network-online.target, Requires=postgresql.service"
echo ""
echo "3. CONTROL DE RECURSOS"
echo "   cron: nice/ionice manualmente"
echo "   timer: CPUQuota=50%, MemoryMax=512M, IOWeight=100"
echo ""
echo "4. EJECUCIONES PERDIDAS (Persistent=)"
echo "   cron: si apagado, tarea perdida (necesita anacron)"
echo "   timer: Persistent=true ejecuta al encender"
echo ""
echo "5. PRECISION"
echo "   cron: minuto (minimo)"
echo "   timer: microsegundo (AccuracySec=1us)"
echo ""
echo "6. ALEATORIZACION"
echo "   cron: no hay"
echo "   timer: RandomizedDelaySec=1h (evita thundering herd)"
echo ""
echo "7. SEGURIDAD"
echo "   cron: solo UID del usuario"
echo "   timer: ProtectSystem, PrivateTmp, NoNewPrivileges, etc."
echo ""
echo "8. ESTADO"
echo "   cron: sin forma nativa de saber si se ejecuto"
echo "   timer: systemctl status myapp.timer/.service"
'
```

#### Paso 1.3: Desventajas frente a cron

```bash
docker compose exec debian-dev bash -c '
echo "=== Desventajas frente a cron ==="
echo ""
echo "1. VERBOSIDAD"
echo "   cron:  \"0 3 * * * /opt/task.sh\" (una linea)"
echo "   timer: myapp.timer + myapp.service (dos archivos)"
echo ""
echo "2. CURVA DE APRENDIZAJE"
echo "   cron: 5 campos + comando (conocido universalmente)"
echo "   timer: OnCalendar + unit files (sintaxis propia)"
echo ""
echo "3. SIN \"crontab -e\" PARA USUARIOS"
echo "   cron: cualquier usuario edita su crontab"
echo "   timer: user units en ~/.config/systemd/user/"
echo ""
echo "4. PORTABILIDAD"
echo "   cron: cualquier Unix (BSD, macOS, Solaris)"
echo "   timer: solo systemd"
echo ""
echo "5. EDICION RAPIDA"
echo "   cron: crontab -e → cambiar linea → guardar"
echo "   timer: editar archivo → daemon-reload → restart timer"
'
```

---

### Parte 2 — OnCalendar y herramientas

#### Paso 2.1: Sintaxis OnCalendar

```bash
docker compose exec debian-dev bash -c '
echo "=== OnCalendar — sintaxis ==="
echo ""
echo "Formato: DiaDeLaSemana YYYY-MM-DD HH:MM:SS"
echo "  Cada parte es opcional"
echo ""
echo "--- Ejemplos ---"
echo "OnCalendar=*-*-* 03:00:00         cada dia a las 03:00"
echo "OnCalendar=Mon *-*-* 09:00:00     cada lunes a las 09:00"
echo "OnCalendar=*-*-01 00:00:00        dia 1 de cada mes"
echo ""
echo "--- Operadores ---"
echo "*            cualquier valor"
echo "Mon..Fri     rango (lun a vie)"
echo "Mon,Wed,Fri  lista"
echo "00/2         cada 2 (intervalo)"
echo ""
echo "--- Expresiones predefinidas ---"
echo "minutely     = *-*-* *:*:00"
echo "hourly       = *-*-* *:00:00"
echo "daily        = *-*-* 00:00:00"
echo "weekly       = Mon *-*-* 00:00:00"
echo "monthly      = *-*-01 00:00:00"
echo "yearly       = *-01-01 00:00:00"
echo "quarterly    = *-01,04,07,10-01 00:00:00"
'
```

#### Paso 2.2: systemd-analyze calendar

```bash
docker compose exec debian-dev bash -c '
echo "=== systemd-analyze calendar ==="
echo ""
echo "Verifica cuando se ejecutaria una expresion OnCalendar."
echo ""
echo "1. Cada dia a las 03:00:"
systemd-analyze calendar "daily" --iterations=3 2>&1 || \
    echo "(systemd-analyze no disponible)"
echo ""
echo "2. Lunes a viernes a las 09:00:"
systemd-analyze calendar "Mon..Fri *-*-* 09:00" --iterations=3 2>&1 || true
echo ""
echo "3. Cada 15 minutos:"
systemd-analyze calendar "*-*-* *:00/15:00" --iterations=5 2>&1 || true
echo ""
echo "4. Dia 1 y 15 de cada mes:"
systemd-analyze calendar "*-*-01,15 00:00:00" --iterations=5 2>&1 || true
'
```

#### Paso 2.3: Conversión cron → OnCalendar

```bash
docker compose exec debian-dev bash -c '
echo "=== Conversion cron → OnCalendar ==="
echo ""
printf "%-18s %-35s\n" "Cron" "OnCalendar"
printf "%-18s %-35s\n" "------------------" "-----------------------------------"
printf "%-18s %-35s\n" "* * * * *"         "*-*-* *:*:00 (minutely)"
printf "%-18s %-35s\n" "0 * * * *"         "*-*-* *:00:00 (hourly)"
printf "%-18s %-35s\n" "0 0 * * *"         "*-*-* 00:00:00 (daily)"
printf "%-18s %-35s\n" "0 0 * * 0"         "Sun *-*-* 00:00:00 (weekly)"
printf "%-18s %-35s\n" "0 0 1 * *"         "*-*-01 00:00:00 (monthly)"
printf "%-18s %-35s\n" "*/5 * * * *"       "*-*-* *:00/5:00"
printf "%-18s %-35s\n" "30 2 * * *"        "*-*-* 02:30:00"
printf "%-18s %-35s\n" "0 9-17 * * 1-5"    "Mon..Fri *-*-* 09..17:00:00"
printf "%-18s %-35s\n" "0 6-22/2 * * *"    "*-*-* 06..22/2:00:00"
echo ""
echo "--- Verificar una conversion ---"
echo "Cron: 0 6-22/2 * * * (cada 2h, 06:00-22:00)"
echo "OnCalendar: *-*-* 06..22/2:00:00"
systemd-analyze calendar "*-*-* 06..22/2:00:00" --iterations=5 2>&1 || true
'
```

#### Paso 2.4: AccuracySec y RandomizedDelaySec

```bash
docker compose exec debian-dev bash -c '
echo "=== AccuracySec ==="
echo ""
echo "systemd agrupa ejecuciones cercanas para ahorrar wake-ups:"
echo ""
echo "AccuracySec=1min (default)"
echo "  Tarea de 03:00 puede ejecutarse entre 03:00 y 03:01"
echo ""
echo "AccuracySec=1s    precision de 1 segundo"
echo "AccuracySec=1us   precision de microsegundo"
echo "AccuracySec=1h    puede desviarse 1 hora (ahorro energia)"
echo ""
echo "=== RandomizedDelaySec ==="
echo ""
echo "Agrega delay aleatorio para evitar thundering herd:"
echo ""
echo "RandomizedDelaySec=30min"
echo "  Tarea de 03:00 ejecuta entre 03:00 y 03:30 (aleatorio)"
echo ""
echo "Util para:"
echo "  Actualizaciones desde repositorio central"
echo "  Backups a servidor NFS compartido"
echo "  Reportes a API con rate limit"
'
```

---

### Parte 3 — Comparación completa

#### Paso 3.1: Tabla comparativa

```bash
docker compose exec debian-dev bash -c '
echo "=== Tabla comparativa ==="
echo ""
printf "%-20s %-12s %-12s %-16s\n" "Aspecto" "cron" "anacron" "systemd timer"
printf "%-20s %-12s %-12s %-16s\n" "--------------------" "------------" "------------" "----------------"
printf "%-20s %-12s %-12s %-16s\n" "Precision minima"   "1 minuto"  "1 dia"     "1 microsegundo"
printf "%-20s %-12s %-12s %-16s\n" "Tareas perdidas"    "Perdidas"  "Ejecuta"   "Persistent="
printf "%-20s %-12s %-12s %-16s\n" "Logs"               "Mail"      "syslog"    "journal"
printf "%-20s %-12s %-12s %-16s\n" "Dependencias"       "No"        "No"        "Si (After=)"
printf "%-20s %-12s %-12s %-16s\n" "Recursos (cgroups)" "No"        "No"        "Si (CPUQuota=)"
printf "%-20s %-12s %-12s %-16s\n" "Seguridad"          "Solo UID"  "Solo root" "Sandboxing"
printf "%-20s %-12s %-12s %-16s\n" "Estado"             "crontab -l" "anacron -d" "systemctl"
printf "%-20s %-12s %-12s %-16s\n" "Portabilidad"       "Universal" "Linux"     "Solo systemd"
printf "%-20s %-12s %-12s %-16s\n" "Complejidad"        "Baja"      "Baja"      "Media"
printf "%-20s %-12s %-12s %-16s\n" "Archivos"           "1 linea"   "1 linea"   "2 archivos"
printf "%-20s %-12s %-12s %-16s\n" "Aleatorizacion"     "No"        "RANDOM_D"  "Randomized*"
'
```

#### Paso 3.2: Timers del sistema

```bash
docker compose exec debian-dev bash -c '
echo "=== Timers del sistema ==="
echo ""
echo "--- Listar todos ---"
systemctl list-timers --all --no-pager 2>&1 || \
    echo "(list-timers no disponible)"
echo ""
echo "--- Examinar un timer real ---"
for timer in fstrim.timer logrotate.timer apt-daily.timer; do
    if systemctl cat "$timer" &>/dev/null 2>&1; then
        echo "--- $timer ---"
        systemctl cat "$timer" 2>/dev/null | grep -E "^(OnCalendar|Persistent|AccuracySec|Randomized)"
        echo ""
    fi
done
echo ""
echo "--- Timer tipico ---"
echo "[Timer]"
echo "OnCalendar=weekly"
echo "AccuracySec=1h"
echo "Persistent=true"
echo ""
echo "[Install]"
echo "WantedBy=timers.target"
'
```

#### Paso 3.3: Cuándo usar cada herramienta

```bash
docker compose exec debian-dev bash -c '
echo "=== Cuando usar cada herramienta ==="
echo ""
echo "--- cron ---"
echo "  Tareas simples y repetitivas"
echo "  Entornos sin systemd"
echo "  Cuando la portabilidad importa"
echo "  Cuando se necesita edicion rapida (crontab -e)"
echo ""
echo "--- anacron ---"
echo "  Laptops y escritorios que se apagan"
echo "  Tareas diarias/semanales/mensuales sin hora exacta"
echo "  Scripts en /etc/cron.daily/"
echo ""
echo "--- systemd timers ---"
echo "  Servidores con systemd"
echo "  Cuando se necesitan dependencias o sandboxing"
echo "  Cuando se necesita Persistent= para intervalos < 1 dia"
echo "  Cuando se necesitan logs en el journal"
echo "  Cuando se necesita precision sub-minuto"
echo ""
echo "--- at ---"
echo "  Ejecucion unica programada"
echo "  systemd-run --on-active= como alternativa"
echo ""
echo "--- batch ---"
echo "  Tareas pesadas cuando hay recursos libres"
echo "  nice -n 19 ionice -c 3 como alternativa"
'
```

---

## Ejercicios

### Ejercicio 1 — Arquitectura timer + service

¿Qué dos archivos necesita un systemd timer para funcionar? Si el timer se
llama `backup.timer`, ¿qué service activa por defecto?

<details><summary>Predicción</summary>

Necesita:
1. **`backup.timer`** — define cuándo ejecutar (OnCalendar, Persistent, etc.)
2. **`backup.service`** — define qué ejecutar (ExecStart)

Por convención de nombre, `backup.timer` activa automáticamente
`backup.service`. Para activar un service diferente se usa `Unit=` en la
sección `[Timer]`.

</details>

### Ejercicio 2 — Ventaja clave: Persistent=

¿Qué ocurre con una tarea cron programada para las 03:00 si el equipo estaba
apagado a esa hora? ¿Y con un timer que tiene `Persistent=true`?

<details><summary>Predicción</summary>

- **Cron:** la tarea **se pierde**. Si se usa anacron, solo se compensan
  tareas daily/weekly/monthly, no intervalos arbitrarios.
- **Timer con `Persistent=true`:** systemd detecta al encender que la última
  ejecución fue antes de la programada. **Ejecuta la tarea inmediatamente**
  al arrancar. Funciona para cualquier intervalo (no solo daily/weekly).

</details>

### Ejercicio 3 — Traducir cron a OnCalendar

Convierte estas expresiones cron a OnCalendar:

1. `0 6 * * 1-5` (lun-vie a las 06:00)
2. `*/10 * * * *` (cada 10 minutos)
3. `0 0 1,15 * *` (día 1 y 15 de cada mes a medianoche)

<details><summary>Predicción</summary>

1. `OnCalendar=Mon..Fri *-*-* 06:00:00`
2. `OnCalendar=*-*-* *:00/10:00`
3. `OnCalendar=*-*-01,15 00:00:00`

Se pueden verificar con:
```bash
systemd-analyze calendar "Mon..Fri *-*-* 06:00" --iterations=5
systemd-analyze calendar "*-*-* *:00/10:00" --iterations=5
systemd-analyze calendar "*-*-01,15 00:00:00" --iterations=5
```

</details>

### Ejercicio 4 — systemd-analyze calendar

¿Qué comando usarías para verificar cuándo se ejecutarían las próximas 5
ocurrencias de un timer con `OnCalendar=*-*-* 00/6:00`?

<details><summary>Predicción</summary>

```bash
systemd-analyze calendar --iterations=5 "*-*-* 00/6:00"
```

Mostraría las próximas 5 ejecuciones: 00:00, 06:00, 12:00, 18:00, y al día
siguiente 00:00. `systemd-analyze calendar` normaliza la expresión, muestra
la próxima ejecución y el tiempo restante.

</details>

### Ejercicio 5 — AccuracySec

Un timer tiene `OnCalendar=*-*-* 03:00` con el `AccuracySec` por defecto.
¿En qué rango de tiempo podría ejecutarse? ¿Cómo lo harías más preciso?

<details><summary>Predicción</summary>

Con el default `AccuracySec=1min`, la tarea podría ejecutarse en cualquier
momento entre **03:00:00 y 03:01:00**. systemd agrupa ejecuciones cercanas
para ahorrar wake-ups de CPU.

Para mayor precisión:
```ini
[Timer]
OnCalendar=*-*-* 03:00:00
AccuracySec=1s
```

Esto limita la desviación a 1 segundo. `AccuracySec=1us` daría precisión
de microsegundo, aunque raramente es necesario.

</details>

### Ejercicio 6 — RandomizedDelaySec

50 servidores tienen un timer que consulta un repositorio de paquetes a las
03:00. ¿Qué problema causa y cómo lo resuelves con `RandomizedDelaySec`?

<details><summary>Predicción</summary>

**Problema:** thundering herd — los 50 servidores golpean el repositorio
simultáneamente a las 03:00, causando picos de carga y posibles
rate limits o caídas.

**Solución:**
```ini
[Timer]
OnCalendar=*-*-* 03:00
RandomizedDelaySec=30min
```

Cada servidor agrega un delay aleatorio entre 0 y 30 minutos. Las peticiones
se distribuyen entre 03:00 y 03:30, reduciendo la carga pico del repositorio.

</details>

### Ejercicio 7 — Desventajas de timers

¿Por qué un administrador podría preferir cron sobre systemd timers para una
tarea simple como rotar logs diariamente?

<details><summary>Predicción</summary>

Razones para preferir cron:
1. **Simplicidad:** una sola línea (`0 0 * * * /opt/rotate.sh`) vs dos
   archivos (`.timer` + `.service`)
2. **Edición rápida:** `crontab -e`, cambiar línea, guardar — sin necesidad
   de `daemon-reload` ni restart
3. **Portabilidad:** funciona en cualquier Unix (BSD, macOS, Solaris)
4. **Conocimiento universal:** todo sysadmin conoce la sintaxis de 5 campos

Para tareas simples sin necesidad de dependencias, sandboxing o logs
integrados, cron sigue siendo más pragmático.

</details>

### Ejercicio 8 — Comparar salida de estado

¿Cómo verificarías si la última ejecución de una tarea fue exitosa usando
cron vs systemd timers?

<details><summary>Predicción</summary>

**Cron:**
- Revisar el mail local (si hay MTA configurado)
- Buscar en `/var/log/syslog` o `/var/log/cron` la línea de ejecución
- No hay forma nativa de ver el exit code del comando

**Systemd timer:**
```bash
systemctl status myapp.service
# Muestra: exit code, PID, timestamp de última ejecución, duración
# Process: 12345 ExecStart=... (code=exited, status=0/SUCCESS)

journalctl -u myapp.service -n 20
# Logs completos con stdout, stderr, timestamps
```

Los timers ganan claramente en diagnóstico — toda la información está en un
solo lugar.

</details>

### Ejercicio 9 — User timers

Un usuario sin privilegios de root quiere ejecutar un script cada hora.
¿Cómo lo haría con cron? ¿Y con un systemd timer?

<details><summary>Predicción</summary>

**Con cron:**
```bash
crontab -e
# Añadir: 0 * * * * /home/user/myscript.sh
```

**Con systemd timer (user unit):**

Crear `~/.config/systemd/user/myscript.timer`:
```ini
[Timer]
OnCalendar=hourly
Persistent=true

[Install]
WantedBy=timers.target
```

Crear `~/.config/systemd/user/myscript.service`:
```ini
[Service]
Type=oneshot
ExecStart=/home/user/myscript.sh
```

Activar:
```bash
systemctl --user daemon-reload
systemctl --user enable --now myscript.timer
```

Cron es significativamente más sencillo para este caso.

</details>

### Ejercicio 10 — Elegir herramienta

Para cada escenario, indica qué herramienta usarías (cron, anacron, timer,
at, batch):

1. Backup diario en un servidor que nunca se apaga
2. Tarea que debe ejecutarse solo después de que PostgreSQL esté listo
3. Script pesado de análisis que no es urgente
4. Limpieza semanal en un laptop que se apaga por las noches
5. Reinicio único del servidor mañana a las 03:00

<details><summary>Predicción</summary>

1. **cron** o **timer** — ambos funcionan. Timer si se necesitan logs en
   journal o control de recursos.
2. **systemd timer** — es el único que soporta `After=postgresql.service` y
   `Requires=postgresql.service`.
3. **batch** — ejecuta cuando el load baja del umbral, o `nice + ionice` si
   se prefiere ejecución inmediata con baja prioridad.
4. **anacron** — diseñado para equipos que se apagan. Ejecuta la tarea
   cuando se encienda si se perdió la ventana. (Un timer con `Persistent=true`
   también funcionaría.)
5. **at** — ejecución única programada: `at 03:00 tomorrow <<< 'sudo reboot'`
   (o `systemd-run --on-active=` como alternativa).

</details>
