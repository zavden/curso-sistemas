# T04 — Restart policies

## Erratas corregidas respecto a las fuentes originales

| # | Ubicación | Error | Corrección |
|---|-----------|-------|------------|
| 1 | max:17 | "processo sale" — portugués/italiano | "proceso sale" (español) |
| 2 | max:35-36 | Diagrama de flujo de `on-success`: la rama SI y Service Dead están en ramas invertidas | Corregido: `¿exit==0?` → SÍ → Restart / NO → Service Dead |
| 3 | max:80 | `RestartForceExitStatus` descrito como que funciona "aunque Restart=no" | No override a `Restart=no`; solo extiende condiciones dentro de la política Restart= configurada |
| 4 | max:236 | Texto en chino "KillMode详解:" | Reemplazado por "KillMode — Detalle:" |

---

## Ciclo de vida del restart

```
  systemctl start myapp
        │
        ▼
  ┌──────────┐    exitosa    ┌──────────────┐
  │ ExecStart │────────────▶│ Service Active │
  │  running  │             └──────┬───────┘
  └─────┬─────┘                    │
        │                          │ systemctl stop
        │ proceso                  │
        │ sale                     ▼
        │                   ┌──────────────┐
        ▼                   │ Stop sequence │
  ┌──────────┐              │  (ExecStop)   │
  │ ExecStart │              └──────┬───────┘
  │ finished  │                     │
  └─────┬─────┘                     ▼
        │                   ┌──────────────┐
   ¿Restart=?               │ Service Dead  │
        │                   └──────────────┘
        ▼
  ┌──────────┐
  │ Evaluar  │
  │ política │
  └────┬─────┘
       │
       ├─ Restart=no         → Service Dead
       ├─ Restart=on-success → ¿exit==0? SÍ → Restart / NO → Dead
       ├─ Restart=on-failure → ¿exit!=0 o señal? SÍ → Restart / NO → Dead
       └─ Restart=always     → Restart siempre
```

## Restart= — Cuándo reiniciar

```ini
[Service]
Restart=on-failure
```

| Valor | ¿Cuándo reinicia? | Caso de uso |
|---|---|---|
| `no` | Nunca (default) | Tareas oneshot, scripts que no deben repetirse |
| `always` | Siempre (cualquier salida) | Servicios críticos que deben estar siempre corriendo |
| `on-success` | Solo si exit code 0 | Tareas que deben repetirse al completarse |
| `on-failure` | Exit != 0, señal, timeout, watchdog | **El más común** — reiniciar solo en errores |
| `on-abnormal` | Señal, timeout, watchdog (NO exit code) | Servicios que pueden salir limpiamente con exit != 0 |
| `on-abort` | Solo por señal (crash) | Servicios que no deben reiniciar en errores controlados |
| `on-watchdog` | Solo por watchdog timeout | Solo si watchdog expira |

### Tabla de verdad completa

| Causa de salida | `always` | `on-success` | `on-failure` | `on-abnormal` | `on-abort` | `on-watchdog` |
|---|---|---|---|---|---|---|
| Exit code 0 | ✅ | ✅ | ❌ | ❌ | ❌ | ❌ |
| Exit code != 0 | ✅ | ❌ | ✅ | ❌ | ❌ | ❌ |
| Señal (crash) | ✅ | ❌ | ✅ | ✅ | ✅ | ❌ |
| Timeout | ✅ | ❌ | ✅ | ✅ | ❌ | ❌ |
| Watchdog | ✅ | ❌ | ✅ | ✅ | ❌ | ✅ |

### Control fino de reinicio con ExitStatus

```ini
[Service]
# NO reiniciar si sale con estos códigos o señales:
RestartPreventExitStatus=0 3 SIGTERM
# Exit 0, exit 3, y SIGTERM = shutdown intencional → no reiniciar

# FORZAR reinicio para ciertos códigos:
RestartForceExitStatus=1 SIGSEGV
# Exit 1 y SIGSEGV siempre reinician
# Tiene precedencia sobre RestartPreventExitStatus
# Solo funciona dentro de la política Restart= configurada

# Considerar como éxito exit codes no estándar:
SuccessExitStatus=42 SIGHUP
# Exit 42 y SIGHUP se tratan como éxito (como exit 0)
# Útil para servicios que usan códigos de salida custom
```

## RestartSec= — Tiempo entre reinicios

```ini
[Service]
Restart=on-failure
RestartSec=5
# Esperar 5 segundos antes de intentar reiniciar
# Default: 100ms (MUY agresivo — SIEMPRE cambiar)

RestartSec=30s     # 30 segundos
RestartSec=1min    # 1 minuto
RestartSec=2min    # 2 minutos
```

```
⚠️ Sin RestartSec, un servicio que crashea inmediatamente:
   Se reinicia 10 veces por segundo
   → Consume 100% CPU
   → Llena los logs
   → Puede causar cascade failure

SIEMPRE definir RestartSec cuando Restart != no
```

## Start Limits — Prevenir loops de reinicio

Cuando un servicio falla repetidamente, systemd deja de reiniciarlo:

```ini
[Unit]
# Límite de arranques:
StartLimitIntervalSec=300    # ventana de tiempo (5 minutos)
StartLimitBurst=5            # máximo 5 arranques en esa ventana

[Service]
Restart=on-failure
RestartSec=10
```

```
Ventana: 300 segundos (5 minutos)
Burst: 5 arranques máximo

Intento 1: falla → espera 10s
Intento 2: falla → espera 10s
Intento 3: falla → espera 10s
Intento 4: falla → espera 10s
Intento 5: falla → start-limit-hit
→ La unidad entra en FAILED
→ systemd deja de intentar reiniciar
```

```bash
# Cuando se alcanza el start limit:
systemctl status myapp
# ○ myapp.service
#    Active: failed (Result: start-limit-hit) since ...

# Resetear y poder reiniciar:
sudo systemctl reset-failed myapp.service
sudo systemctl start myapp
```

### Acciones al alcanzar el límite

```ini
[Unit]
StartLimitIntervalSec=300
StartLimitBurst=5

# Qué hacer cuando se agota el burst:
StartLimitAction=none          # solo dejar de reiniciar (default)
StartLimitAction=reboot        # reiniciar la máquina completa
StartLimitAction=reboot-force  # reboot inmediato (sin shutdown limpio)
StartLimitAction=poweroff      # apagar la máquina
```

```ini
# Deshabilitar el start limit (CUIDADO — puede causar loops infinitos):
[Unit]
StartLimitIntervalSec=0
# 0 = sin límite
```

## Timeouts

```ini
[Service]
# Tiempo máximo para que el servicio ARRANQUE:
TimeoutStartSec=30
# Si ExecStart no completa en 30s → SIGTERM → espera → SIGKILL
# Default: 90s (DefaultTimeoutStartSec en /etc/systemd/system.conf)

# Tiempo máximo para que el servicio PARE:
TimeoutStopSec=30
# Si ExecStop no completa en 30s → SIGKILL
# Default: 90s

# Ambos a la vez:
TimeoutSec=30
# Equivale a: TimeoutStartSec=30 + TimeoutStopSec=30

# Sin timeout (esperar indefinidamente):
TimeoutStartSec=infinity

# Señal a enviar al timeout de stop (antes de SIGKILL):
TimeoutStopFailureMode=terminate   # SIGTERM (default)
TimeoutStopFailureMode=abort       # SIGABRT (genera core dump)
TimeoutStopFailureMode=kill        # SIGKILL (inmediato)
```

## Señales de stop y KillMode

```ini
[Service]
# Señal enviada al hacer systemctl stop (default: SIGTERM):
KillSignal=SIGTERM

# Señal final si no responde (default: SIGKILL):
FinalKillSignal=SIGKILL

# Enviar SIGHUP antes de SIGTERM (algunos daemons hacen cleanup con HUP):
SendSIGHUP=yes
```

### KillMode — A quién enviar la señal

```ini
[Service]
KillMode=control-group   # TODOS los procesos del cgroup (default, más seguro)
KillMode=process         # solo el proceso principal
KillMode=mixed           # SIGTERM al principal, SIGKILL a los demás después del timeout
KillMode=none            # no matar nada (solo ExecStop — peligroso)
```

```
KillMode — Detalle:

control-group (default):
  → Envía la señal a TODOS los procesos del cgroup
  → El más seguro — mata workers, children, etc.
  → Evita procesos huérfanos

process:
  → Solo al PID principal
  → Los children pueden quedar huérfanos

mixed:
  → SIGTERM al principal
  → SIGKILL a los demás después del timeout

none:
  → No envía señal automáticamente
  → Solo ExecStop se ejecuta
  → Pocos casos de uso legítimos
```

## WatchdogSec= — Health check activo

```
┌─────────────────────────────────────────────────────────────┐
│                    WATCHDOG EN SYSTEMD                       │
│                                                             │
│   Service running                                           │
│        │                                                    │
│        │ sd_notify("WATCHDOG=1") cada WatchdogSec/2         │
│        │ (ej: cada 15s si WatchdogSec=30)                   │
│        ▼                                                    │
│   Si watchdog no recibe ping en WatchdogSec (30s):          │
│        │                                                    │
│        ├─ Restart=on-failure  → reiniciar el servicio        │
│        └─ Restart=on-watchdog → reiniciar                   │
│                                                             │
│   Solo funciona con Type=notify                             │
└─────────────────────────────────────────────────────────────┘
```

```ini
[Service]
Type=notify              # NECESARIO para watchdog
WatchdogSec=30           # debe recibir ping cada 30 segundos
Restart=on-failure       # on-failure incluye watchdog timeout
# El servicio debería enviar ping cada WatchdogSec/2 (cada 15s)
```

```bash
# Cómo enviar el watchdog desde diferentes lenguajes:

# C:
# sd_notify(0, "WATCHDOG=1");

# Python:
# import systemd.daemon
# systemd.daemon.notify("WATCHDOG=1")

# Bash:
# systemd-notify WATCHDOG=1

# Go:
# sdnotify.Notify("WATCHDOG=1")
```

```bash
# Ver propiedades de watchdog de un servicio:
systemctl show nginx --property=WatchdogUSec,NotifyAccess
```

## Patrones profesionales

### Servicio de producción completo

```ini
[Unit]
Description=Production API Server
Documentation=https://api.example.com/docs
After=network-online.target postgresql.service
Wants=network-online.target
Requires=postgresql.service
StartLimitIntervalSec=600    # 10 minutos
StartLimitBurst=5
StartLimitAction=none        # dejar de reiniciar, no apagar servidor

[Service]
Type=notify
User=apiserver
Group=apiserver

ExecStart=/opt/api/bin/server --config /etc/api/config.yaml
ExecReload=/bin/kill -HUP $MAINPID

Restart=on-failure           # reiniciar en errores
RestartSec=10                # esperar 10 segundos

WatchdogSec=30               # health check cada 30 segundos
TimeoutStartSec=60           # 1 minuto para arrancar
TimeoutStopSec=30            # 30 segundos para parar

StateDirectory=api
RuntimeDirectory=api
EnvironmentFile=/etc/api/env

MemoryMax=2G
CPUQuota=200%
LimitNOFILE=65535

NoNewPrivileges=yes
ProtectSystem=strict
ProtectHome=yes
PrivateTmp=yes
ReadWritePaths=/var/lib/api

[Install]
WantedBy=multi-user.target
```

### Servicio que no reinicia en exit limpio

```ini
[Service]
Restart=on-failure
RestartPreventExitStatus=0
# Exit 0 = shutdown intencional → no reiniciar
# Exit != 0 = error → reiniciar
```

### Servicio con backoff exponencial (simulado)

systemd no tiene backoff exponencial nativo, pero se puede simular con un wrapper:

```bash
#!/bin/bash
# /usr/local/bin/myapp-wrapper.sh
RETRY=0
MAX_RETRIES=5

while true; do
    /opt/myapp/bin/server
    EXIT=$?

    # Exit 0 = éxito, salir
    [[ $EXIT -eq 0 ]] && exit 0

    ((RETRY++))
    if [[ $RETRY -ge $MAX_RETRIES ]]; then
        echo "Max retries ($MAX_RETRIES) reached. Exiting." >&2
        exit $EXIT
    fi

    # Backoff exponencial: 2, 4, 8, 16, 32 segundos
    WAIT=$((2 ** RETRY))
    echo "Retry $RETRY/$MAX_RETRIES in ${WAIT}s (exit=$EXIT)..." >&2
    sleep "$WAIT"
done
```

```ini
[Service]
Type=simple
ExecStart=/usr/local/bin/myapp-wrapper.sh
Restart=on-failure
RestartSec=5
```

## Verificar configuración de restart

```bash
# Propiedades de restart:
systemctl show myapp --property=Restart,RestartUSec
systemctl show myapp --property=StartLimitBurst,StartLimitIntervalUSec
systemctl show myapp --property=WatchdogUSec,TimeoutStartUSec

# Cuántas veces se ha reiniciado:
systemctl show myapp --property=NRestarts

# Simular un crash para probar restart (en entorno de pruebas):
sudo systemctl kill -s SIGKILL myapp.service

# Ver logs de reinicios:
journalctl -u myapp --no-pager -b | grep -E "Started|Failed|Stopped"
```

---

## Lab — Restart policies

### Objetivo

Dominar las políticas de reinicio de systemd: los valores de
Restart= y bajo qué condiciones actúan, RestartSec para el
intervalo entre reinicios, StartLimitBurst/IntervalSec para
prevenir loops, timeouts de arranque y parada, KillMode y señales,
WatchdogSec para health checks, y patrones profesionales.

### Prerequisitos

- Entorno del curso levantado (`docker compose up -d`)

---

### Parte 1 — Restart y RestartSec

#### Paso 1.1: Valores de Restart=

```bash
docker compose exec debian-dev bash -c '
echo "=== Valores de Restart= ==="
echo ""
echo "| Valor        | Reinicia en                              |"
echo "|------------- |-----------------------------------------|"
echo "| no           | Nunca (default)                          |"
echo "| always       | Siempre (exit, señal, timeout, watchdog) |"
echo "| on-success   | Solo si exit code 0                      |"
echo "| on-failure   | Exit != 0, señal, timeout, watchdog      |"
echo "| on-abnormal  | Señal, timeout, watchdog (NO exit code)  |"
echo "| on-abort     | Solo por señal (crash)                   |"
echo "| on-watchdog  | Solo por watchdog timeout                |"
echo ""
echo "--- Tabla de condiciones ---"
echo ""
echo "| Causa          | always | on-success | on-failure | on-abnormal | on-abort |"
echo "|--------------- |--------|-----------|-----------|------------|---------|"
echo "| Exit limpio (0)| Sí     | Sí        |           |            |         |"
echo "| Exit error     | Sí     |           | Sí        |            |         |"
echo "| Señal (crash)  | Sí     |           | Sí        | Sí         | Sí      |"
echo "| Timeout        | Sí     |           | Sí        | Sí         |         |"
echo "| Watchdog       | Sí     |           | Sí        | Sí         |         |"
echo ""
echo "on-failure es el MÁS COMÚN para servicios de producción"
'
```

#### Paso 1.2: Crear unit files con diferentes Restart=

```bash
docker compose exec debian-dev bash -c '
echo "=== Ejemplos de Restart= ==="
echo ""
echo "--- on-failure (el más común) ---"
cat > /tmp/lab-restart-failure.service << '\''EOF'\''
[Unit]
Description=Restart on-failure example

[Service]
Type=simple
ExecStart=/usr/bin/sleep infinity
Restart=on-failure
RestartSec=5

[Install]
WantedBy=multi-user.target
EOF
echo "Restart=on-failure + RestartSec=5:"
echo "  Reinicia si exit != 0, señal, timeout, watchdog"
echo "  Espera 5 segundos entre reinicios"
echo ""
echo "--- always (servicios críticos) ---"
cat > /tmp/lab-restart-always.service << '\''EOF'\''
[Unit]
Description=Restart always example

[Service]
Type=simple
ExecStart=/usr/bin/sleep infinity
Restart=always
RestartSec=3

[Install]
WantedBy=multi-user.target
EOF
echo "Restart=always + RestartSec=3:"
echo "  Reinicia SIEMPRE, incluso si exit 0"
echo "  Usar para servicios que NUNCA deben detenerse"
echo ""
echo "--- no (oneshot/scripts) ---"
cat > /tmp/lab-restart-no.service << '\''EOF'\''
[Unit]
Description=No restart example

[Service]
Type=oneshot
ExecStart=/usr/bin/echo "Task done"
Restart=no
RemainAfterExit=yes
EOF
echo "Restart=no (default):"
echo "  No reiniciar bajo ninguna circunstancia"
echo "  Para tareas oneshot que no deben repetirse"
'
```

#### Paso 1.3: RestartSec y exit codes

```bash
docker compose exec debian-dev bash -c '
echo "=== RestartSec ==="
echo ""
echo "Default de RestartSec: 100ms"
echo "MUY agresivo — un servicio que crashea inmediatamente"
echo "se reinicia 10 veces por segundo"
echo ""
echo "SIEMPRE definir RestartSec cuando Restart != no"
echo ""
echo "Formatos:"
echo "  RestartSec=5       5 segundos"
echo "  RestartSec=30s     30 segundos"
echo "  RestartSec=1min    1 minuto"
echo ""
echo "=== RestartPreventExitStatus ==="
echo ""
echo "Excluir exit codes del reinicio:"
echo "  RestartPreventExitStatus=0 3 SIGTERM"
echo "  No reiniciar si exit 0, exit 3, o SIGTERM"
echo ""
echo "=== SuccessExitStatus ==="
echo ""
echo "Considerar como éxito exit codes no estándar:"
echo "  SuccessExitStatus=42 SIGHUP"
echo "  Exit 42 y SIGHUP se tratan como éxito"
echo ""
echo "--- Ejemplo completo ---"
cat > /tmp/lab-restart-custom.service << '\''EOF'\''
[Unit]
Description=Custom restart example

[Service]
Type=simple
ExecStart=/usr/bin/sleep infinity
Restart=on-failure
RestartSec=10
RestartPreventExitStatus=0
SuccessExitStatus=42

[Install]
WantedBy=multi-user.target
EOF
cat -n /tmp/lab-restart-custom.service
echo ""
systemd-analyze verify /tmp/lab-restart-custom.service 2>&1 || true
'
```

---

### Parte 2 — Start limits y timeouts

#### Paso 2.1: StartLimitBurst e IntervalSec

```bash
docker compose exec debian-dev bash -c '
echo "=== Start limits ==="
echo ""
echo "Si un servicio falla repetidamente, systemd deja de reiniciarlo"
echo ""
cat > /tmp/lab-startlimit.service << '\''EOF'\''
[Unit]
Description=Start limit example
StartLimitIntervalSec=300
StartLimitBurst=5

[Service]
Type=simple
ExecStart=/usr/bin/false
Restart=on-failure
RestartSec=5

[Install]
WantedBy=multi-user.target
EOF
cat -n /tmp/lab-startlimit.service
echo ""
echo "StartLimitIntervalSec=300  ventana de 5 minutos"
echo "StartLimitBurst=5          máximo 5 arranques"
echo ""
echo "Secuencia:"
echo "  Intento 1: falla → espera 5s"
echo "  Intento 2: falla → espera 5s"
echo "  Intento 3: falla → espera 5s"
echo "  Intento 4: falla → espera 5s"
echo "  Intento 5: falla → start-limit-hit (no más reinicios)"
echo ""
echo "Estado: failed (Result: start-limit-hit)"
echo "Para resetear: systemctl reset-failed SERVICE"
echo ""
echo "--- Deshabilitar (peligroso) ---"
echo "StartLimitIntervalSec=0"
echo "0 = sin límite (puede causar loops infinitos)"
'
```

#### Paso 2.2: StartLimitAction

```bash
docker compose exec debian-dev bash -c '
echo "=== StartLimitAction ==="
echo ""
echo "Qué hacer al alcanzar el límite de reinicios:"
echo ""
echo "  StartLimitAction=none          solo dejar de reiniciar (default)"
echo "  StartLimitAction=reboot        reiniciar la máquina"
echo "  StartLimitAction=reboot-force  reiniciar inmediato (sin shutdown)"
echo "  StartLimitAction=poweroff      apagar la máquina"
echo ""
echo "--- Ejemplo: servicio crítico ---"
cat > /tmp/lab-action.service << '\''EOF'\''
[Unit]
Description=Critical service (reboots on failure)
StartLimitIntervalSec=300
StartLimitBurst=5
StartLimitAction=reboot

[Service]
Type=notify
ExecStart=/opt/critical/bin/server
Restart=on-failure
RestartSec=10

[Install]
WantedBy=multi-user.target
EOF
cat -n /tmp/lab-action.service
echo ""
echo "CUIDADO: StartLimitAction=reboot reinicia la MÁQUINA"
echo "Solo para servicios realmente críticos"
'
```

#### Paso 2.3: Timeouts

```bash
docker compose exec debian-dev bash -c '
echo "=== Timeouts ==="
echo ""
echo "--- TimeoutStartSec ---"
echo "Tiempo máximo para que el servicio arranque"
echo "Default: 90s (DefaultTimeoutStartSec del manager)"
echo "Si ExecStart no completa → SIGTERM → timeout → SIGKILL"
echo ""
echo "--- TimeoutStopSec ---"
echo "Tiempo máximo para que el servicio pare"
echo "Default: 90s"
echo "Si ExecStop no completa → SIGKILL"
echo ""
echo "--- TimeoutSec ---"
echo "Atajo para TimeoutStartSec + TimeoutStopSec"
echo ""
echo "--- Ejemplo ---"
cat > /tmp/lab-timeout.service << '\''EOF'\''
[Unit]
Description=Service with timeouts

[Service]
Type=simple
ExecStart=/usr/bin/sleep infinity
TimeoutStartSec=30
TimeoutStopSec=15
Restart=on-failure
RestartSec=5

[Install]
WantedBy=multi-user.target
EOF
cat -n /tmp/lab-timeout.service
echo ""
echo "--- KillMode ---"
echo "A quién enviar la señal de stop:"
echo "  control-group  todos los procesos del cgroup (default, seguro)"
echo "  process        solo el proceso principal"
echo "  mixed          SIGTERM al principal, SIGKILL a los demás"
echo "  none           no matar (peligroso, solo ExecStop)"
echo ""
echo "--- KillSignal ---"
echo "Señal enviada al hacer stop (default: SIGTERM)"
echo "FinalKillSignal: señal final si no muere (default: SIGKILL)"
'
```

---

### Parte 3 — Watchdog y patrones

#### Paso 3.1: WatchdogSec

```bash
docker compose exec debian-dev bash -c '
echo "=== WatchdogSec ==="
echo ""
echo "El servicio debe enviar sd_notify(WATCHDOG=1) periódicamente"
echo "Si no lo hace, systemd lo considera muerto"
echo ""
cat > /tmp/lab-watchdog.service << '\''EOF'\''
[Unit]
Description=Service with watchdog

[Service]
Type=notify
ExecStart=/opt/myapp/bin/server
WatchdogSec=30
Restart=on-failure
RestartSec=10

[Install]
WantedBy=multi-user.target
EOF
cat -n /tmp/lab-watchdog.service
echo ""
echo "WatchdogSec=30:"
echo "  El servicio debe enviar WATCHDOG=1 cada 30 segundos"
echo "  Recomendación: enviar cada WatchdogSec/2 (cada 15s)"
echo "  Solo funciona con Type=notify"
echo ""
echo "--- Cómo enviar el watchdog ---"
echo "C:      sd_notify(0, \"WATCHDOG=1\");"
echo "Python: systemd.daemon.notify(\"WATCHDOG=1\")"
echo "Bash:   systemd-notify WATCHDOG=1"
echo ""
echo "--- NRestarts: ver cuántos reinicios ---"
echo "systemctl show SERVICE --property=NRestarts"
'
```

#### Paso 3.2: Patrón de producción completo

```bash
docker compose exec debian-dev bash -c '
echo "=== Patrón de producción ==="
echo ""
cat > /tmp/lab-production.service << '\''EOF'\''
[Unit]
Description=Production API Server
After=network-online.target postgresql.service
Wants=network-online.target
Requires=postgresql.service
StartLimitIntervalSec=600
StartLimitBurst=5

[Service]
Type=notify
User=apiserver
Group=apiserver

ExecStart=/opt/api/bin/server
ExecReload=/bin/kill -HUP $MAINPID

Restart=on-failure
RestartSec=10
WatchdogSec=30

TimeoutStartSec=60
TimeoutStopSec=30

StateDirectory=api
RuntimeDirectory=api
EnvironmentFile=/etc/api/env

LimitNOFILE=65535
ProtectSystem=strict
ProtectHome=yes
PrivateTmp=yes
NoNewPrivileges=yes

[Install]
WantedBy=multi-user.target
EOF
cat -n /tmp/lab-production.service
echo ""
echo "--- Verificar ---"
systemd-analyze verify /tmp/lab-production.service 2>&1 || true
echo ""
echo "Checklist de restart:"
echo "  [x] Restart=on-failure (reinicia en errores)"
echo "  [x] RestartSec=10 (no demasiado agresivo)"
echo "  [x] StartLimitBurst=5 (previene loops)"
echo "  [x] StartLimitIntervalSec=600 (ventana de 10 min)"
echo "  [x] WatchdogSec=30 (health check)"
echo "  [x] TimeoutStartSec/StopSec (límites razonables)"
'
```

#### Paso 3.3: Elegir la política correcta

```bash
docker compose exec debian-dev bash -c '
echo "=== Elegir la política correcta ==="
echo ""
echo "Pregunta: ¿qué Restart= usarías para cada caso?"
echo ""
echo "1. Servidor web de producción"
echo "   → on-failure (o always si es MUY crítico)"
echo ""
echo "2. Script de migración de datos (oneshot)"
echo "   → no (no debe repetirse)"
echo ""
echo "3. Worker que puede salir con exit 0 cuando no hay trabajo"
echo "   → on-failure (exit 0 es intencional, no reiniciar)"
echo ""
echo "4. Servicio que solo debe reiniciar si crashea por señal"
echo "   → on-abort"
echo ""
echo "5. Servicio crítico de infraestructura"
echo "   → always + StartLimitAction=reboot"
echo ""
echo "--- Verificar restart de servicios existentes ---"
for svc in /usr/lib/systemd/system/systemd-*.service; do
    [[ -f "$svc" ]] || continue
    R=$(grep "^Restart=" "$svc" 2>/dev/null | head -1)
    RS=$(grep "^RestartSec=" "$svc" 2>/dev/null | head -1)
    if [[ -n "$R" ]]; then
        printf "  %-45s %s %s\n" "$(basename "$svc")" "$R" "$RS"
    fi
done | head -10
'
```

---

### Limpieza final

```bash
docker compose exec debian-dev bash -c '
rm -f /tmp/lab-restart-failure.service /tmp/lab-restart-always.service
rm -f /tmp/lab-restart-no.service /tmp/lab-restart-custom.service
rm -f /tmp/lab-startlimit.service /tmp/lab-action.service
rm -f /tmp/lab-timeout.service /tmp/lab-watchdog.service
rm -f /tmp/lab-production.service
echo "Archivos temporales eliminados"
'
```

### Conceptos reforzados

1. `Restart=on-failure` es la política más común: reinicia en
   exit != 0, señal, timeout, watchdog. `Restart=always` reinicia
   siempre, incluso en exit 0.

2. `RestartSec=` define el intervalo entre reinicios. El default
   (100ms) es demasiado agresivo — siempre definir un valor
   razonable (5-30 segundos).

3. `StartLimitBurst=` y `StartLimitIntervalSec=` previenen loops
   de reinicio infinitos. Al alcanzar el límite, el servicio
   entra en `failed (Result: start-limit-hit)`.

4. `TimeoutStartSec=` y `TimeoutStopSec=` limitan cuánto puede
   tardar un servicio en arrancar y parar. Si se excede, systemd
   envía SIGTERM y luego SIGKILL.

5. `WatchdogSec=` habilita health checks: el servicio debe enviar
   `sd_notify("WATCHDOG=1")` periódicamente. Solo funciona con
   `Type=notify`.

6. `KillMode=control-group` (default) envía la señal de stop a
   TODOS los procesos del cgroup, no solo al principal. Es el
   modo más seguro para evitar procesos huérfanos.

---

## Ejercicios

### Ejercicio 1 — Servicio que crashea y reinicia

Crea un script que falla después de 3 segundos (exit 1) y un servicio con `Restart=on-failure` y `RestartSec=5`. Observa cómo systemd lo reinicia automáticamente.

```bash
sudo tee /usr/local/bin/crasher.sh << 'SCRIPT'
#!/bin/bash
echo "$(date): Process started (PID $$)"
sleep 3
echo "$(date): Crashing!"
exit 1
SCRIPT
sudo chmod +x /usr/local/bin/crasher.sh

sudo tee /etc/systemd/system/crasher.service << 'EOF'
[Unit]
Description=Crash Test Service
StartLimitIntervalSec=120
StartLimitBurst=3

[Service]
Type=simple
ExecStart=/usr/local/bin/crasher.sh
Restart=on-failure
RestartSec=5
EOF

sudo systemctl daemon-reload
sudo systemctl start crasher
journalctl -u crasher -f --no-pager
```

<details><summary>Predicción</summary>

La secuencia observada será:
1. "Process started" → espera 3s → "Crashing!" → exit 1
2. systemd detecta exit != 0 → `on-failure` aplica → espera 5s (RestartSec)
3. "Process started" (nuevo PID) → espera 3s → "Crashing!" → exit 1
4. Repite hasta 3 intentos (StartLimitBurst=3 en 120s)
5. Al tercer fallo → `start-limit-hit`

`systemctl status crasher` mostrará:
```
○ crasher.service - Crash Test Service
   Active: failed (Result: start-limit-hit)
```

Para poder reiniciar de nuevo: `systemctl reset-failed crasher`

Cada intento toma ~8s (3s de sleep + 5s de RestartSec). Los 3 intentos ocurren en ~24s, bien dentro de la ventana de 120s.

Limpiar:
```bash
sudo systemctl stop crasher 2>/dev/null
sudo rm /etc/systemd/system/crasher.service /usr/local/bin/crasher.sh
sudo systemctl daemon-reload
```

</details>

### Ejercicio 2 — Restart=always vs on-failure

Crea dos versiones del mismo servicio: una con `Restart=always` y otra con `Restart=on-failure`. El script sale con exit 0. ¿Cuál reinicia y cuál no?

```bash
sudo tee /usr/local/bin/clean-exit.sh << 'EOF'
#!/bin/bash
echo "$(date): Running..."
sleep 2
echo "$(date): Clean exit"
exit 0
EOF
sudo chmod +x /usr/local/bin/clean-exit.sh

# Versión always:
sudo tee /etc/systemd/system/test-always.service << 'EOF'
[Unit]
Description=Test always
StartLimitIntervalSec=60
StartLimitBurst=3

[Service]
Type=simple
ExecStart=/usr/local/bin/clean-exit.sh
Restart=always
RestartSec=3
EOF

# Versión on-failure:
sudo tee /etc/systemd/system/test-onfailure.service << 'EOF'
[Unit]
Description=Test on-failure

[Service]
Type=simple
ExecStart=/usr/local/bin/clean-exit.sh
Restart=on-failure
RestartSec=3
EOF

sudo systemctl daemon-reload
sudo systemctl start test-always test-onfailure
sleep 5
systemctl status test-always --no-pager -n 3
systemctl status test-onfailure --no-pager -n 3
```

<details><summary>Predicción</summary>

- `test-always` **reinicia** repetidamente. `Restart=always` reinicia incluso con exit 0. Después de los 3 intentos en 60s → `start-limit-hit`.
- `test-onfailure` **no reinicia**. `Restart=on-failure` no reinicia en exit 0. El servicio queda `inactive (dead)` inmediatamente.

Esta es la diferencia clave: `always` trata exit 0 como algo que requiere reinicio (el servicio "no debería haber terminado"), mientras `on-failure` trata exit 0 como shutdown intencional.

Para un servicio que debe estar siempre corriendo (daemon): `always`. Para uno que puede terminar normalmente: `on-failure`.

Limpiar:
```bash
sudo systemctl stop test-always test-onfailure 2>/dev/null
sudo rm /etc/systemd/system/test-always.service /etc/systemd/system/test-onfailure.service
sudo rm /usr/local/bin/clean-exit.sh
sudo systemctl daemon-reload
```

</details>

### Ejercicio 3 — RestartPreventExitStatus

Crea un servicio con `Restart=on-failure` y `RestartPreventExitStatus=0 3`. El script sale con exit 1 (debería reiniciar), luego modifícalo para exit 3 (no debería reiniciar). Verifica el comportamiento.

```bash
sudo tee /usr/local/bin/exit-test.sh << 'EOF'
#!/bin/bash
echo "$(date): Exiting with code 1"
exit 1
EOF
sudo chmod +x /usr/local/bin/exit-test.sh

sudo tee /etc/systemd/system/exit-test.service << 'EOF'
[Unit]
Description=Exit Code Test
StartLimitIntervalSec=60
StartLimitBurst=5

[Service]
Type=simple
ExecStart=/usr/local/bin/exit-test.sh
Restart=on-failure
RestartSec=3
RestartPreventExitStatus=0 3
EOF

sudo systemctl daemon-reload
sudo systemctl start exit-test
sleep 5
systemctl show exit-test --property=NRestarts
```

<details><summary>Predicción</summary>

Con `exit 1`:
- `on-failure` aplica (exit != 0)
- `RestartPreventExitStatus=0 3` no incluye exit 1
- El servicio **sí reinicia** → NRestarts > 0

Si cambiamos el script a `exit 3`:
- `on-failure` aplicaría (exit != 0)
- Pero `RestartPreventExitStatus=0 3` incluye exit 3
- El servicio **no reinicia** → queda `inactive (dead)` o `failed`

`RestartPreventExitStatus` permite excluir exit codes específicos del reinicio, incluso cuando la política Restart= los incluiría. Es útil para distinguir entre "error real" (exit 1) y "condición esperada" (exit 3 = sin trabajo).

</details>

### Ejercicio 4 — Observar start-limit-hit

Crea un servicio que falla inmediatamente (`/usr/bin/false`) con `StartLimitBurst=3` e `IntervalSec=30`. Observa cómo alcanza el límite. Luego usa `reset-failed` para poder reiniciarlo.

```bash
sudo tee /etc/systemd/system/limit-test.service << 'EOF'
[Unit]
Description=Start Limit Test
StartLimitIntervalSec=30
StartLimitBurst=3

[Service]
Type=simple
ExecStart=/usr/bin/false
Restart=on-failure
RestartSec=2
EOF

sudo systemctl daemon-reload
sudo systemctl start limit-test

# Esperar a que alcance el límite:
sleep 15
systemctl status limit-test --no-pager
systemctl show limit-test --property=Result,NRestarts
```

<details><summary>Predicción</summary>

Secuencia:
1. Intento 1: `/usr/bin/false` sale con exit 1 inmediatamente → espera 2s
2. Intento 2: falla → espera 2s
3. Intento 3: falla → `start-limit-hit`

Después de ~6 segundos, el servicio está en:
```
Active: failed (Result: start-limit-hit)
NRestarts=2    (2 reinicios después del arranque inicial)
```

`systemctl start limit-test` falla con: "Start request repeated too quickly"

Para resetear:
```bash
sudo systemctl reset-failed limit-test
sudo systemctl start limit-test   # Ahora funciona (empieza nueva ventana)
```

`reset-failed` limpia el estado y el contador de arranques, permitiendo que el servicio intente arrancar de nuevo.

Limpiar:
```bash
sudo systemctl reset-failed limit-test 2>/dev/null
sudo rm /etc/systemd/system/limit-test.service
sudo systemctl daemon-reload
```

</details>

### Ejercicio 5 — Timeout en acción

Crea un servicio con `TimeoutStartSec=5` cuyo script tarda 15 segundos. ¿Qué pasa? Luego aumenta el timeout y verifica.

```bash
sudo tee /usr/local/bin/slow-start.sh << 'EOF'
#!/bin/bash
echo "$(date): Starting slow service..."
sleep 15
echo "$(date): Ready!"
EOF
sudo chmod +x /usr/local/bin/slow-start.sh

sudo tee /etc/systemd/system/slow-test.service << 'EOF'
[Unit]
Description=Slow Starting Service

[Service]
Type=simple
ExecStart=/usr/local/bin/slow-start.sh
TimeoutStartSec=5
Restart=no
EOF

sudo systemctl daemon-reload
sudo systemctl start slow-test
sleep 7
systemctl status slow-test --no-pager
```

<details><summary>Predicción</summary>

Con `TimeoutStartSec=5` y un script de 15s:
- A los 5s, systemd envía SIGTERM al proceso
- El servicio queda `failed` con `Result=timeout`
- El mensaje "Ready!" nunca aparece en los logs

Si cambiamos a `TimeoutStartSec=20`:
- El servicio tiene suficiente tiempo para completar
- Pero con `Type=simple`, el servicio queda "active" inmediatamente (simple no espera a que ExecStart termine)
- El timeout de start aplica cuando systemd espera al servicio: con Type=simple el timeout solo importa si el proceso no logra ejecutarse (exec() falla)

Nota: para Type=simple, `TimeoutStartSec` realmente controla cuánto espera systemd a que el proceso se ejecute (exec). Para Type=notify/forking, controla cuánto espera a la notificación/fork.

Limpiar:
```bash
sudo systemctl stop slow-test 2>/dev/null
sudo rm /etc/systemd/system/slow-test.service /usr/local/bin/slow-start.sh
sudo systemctl daemon-reload
```

</details>

### Ejercicio 6 — Inspeccionar restart de servicios reales

Consulta las propiedades de restart de varios servicios del sistema. ¿Cuál tiene la política más agresiva? ¿Cuál no reinicia nunca?

```bash
for svc in sshd cron systemd-journald systemd-logind dbus; do
    R=$(systemctl show "$svc" --property=Restart --value 2>/dev/null)
    RS=$(systemctl show "$svc" --property=RestartUSec --value 2>/dev/null)
    NR=$(systemctl show "$svc" --property=NRestarts --value 2>/dev/null)
    if [[ -n "$R" ]]; then
        printf "%-25s Restart=%-15s RestartUSec=%-10s NRestarts=%s\n" \
            "$svc" "$R" "$RS" "$NR"
    fi
done
```

<details><summary>Predicción</summary>

Resultados típicos:
- `systemd-journald` → `Restart=always` — es crítico, debe estar siempre corriendo
- `systemd-logind` → `Restart=always` — gestiona sesiones, crítico
- `dbus` → `Restart=on-failure` — bus de comunicación, reiniciar solo en errores
- `sshd` → `Restart=on-failure` — servidor SSH, reiniciar en errores
- `cron` → `Restart=on-failure` o `Restart=no` (depende de la distro)

Los servicios internos de systemd (`systemd-*`) tienden a usar `Restart=always` porque son esenciales para el sistema. Los servicios de usuario tienden a usar `Restart=on-failure`.

`NRestarts=0` en la mayoría: un sistema sano no debería tener reinicios frecuentes. Un valor alto indicaría un problema.

</details>

### Ejercicio 7 — KillMode en acción

¿Qué diferencia hace `KillMode=process` vs `KillMode=control-group`? Piensa en un servicio que hace fork de workers.

```bash
# Ver KillMode de servicios existentes:
for svc in sshd nginx systemd-journald; do
    KM=$(systemctl show "$svc" --property=KillMode --value 2>/dev/null)
    KS=$(systemctl show "$svc" --property=KillSignal --value 2>/dev/null)
    if [[ -n "$KM" ]]; then
        printf "%-25s KillMode=%-15s KillSignal=%s\n" "$svc" "$KM" "$KS"
    fi
done
```

<details><summary>Predicción</summary>

Resultados típicos:
- Todos usan `KillMode=control-group` (o `mixed` en algunos casos)
- `KillSignal=15` (SIGTERM = señal 15)

La diferencia en la práctica:

**`control-group` (default):** Al hacer `systemctl stop nginx`, SIGTERM se envía a **todos** los procesos del cgroup de nginx: master, workers, y cualquier child process. Todos mueren limpiamente. Es el modo más seguro.

**`process`:** Solo se envía SIGTERM al master process. Si el master no mata a los workers antes de morir, los workers quedan como procesos huérfanos. Esto puede causar puertos ocupados, archivos bloqueados, etc.

**`mixed`:** SIGTERM al principal, y si no muere en TimeoutStopSec, SIGKILL a todos los demás. Permite al master hacer cleanup antes de que systemd mate a los workers.

**`none`:** Peligroso. Solo ejecuta ExecStop, no envía señales. Si ExecStop no mata todos los procesos, quedan corriendo indefinidamente.

</details>

### Ejercicio 8 — Elegir la política correcta

Para cada escenario, ¿qué combinación de Restart=, RestartSec=, y StartLimit usarías?

1. Servidor web de producción
2. Script de migración de datos (oneshot)
3. Worker que sale con exit 0 cuando no hay trabajo
4. Servicio crítico de infraestructura que si falla mucho, debe reiniciar la máquina
5. Servicio con watchdog que debe reiniciar solo si se cuelga

<details><summary>Predicción</summary>

1. **Servidor web de producción:**
   ```ini
   Restart=on-failure
   RestartSec=10
   StartLimitIntervalSec=600
   StartLimitBurst=5
   ```
   Reinicia en errores, no en shutdown limpio. 5 intentos en 10 min.

2. **Script de migración (oneshot):**
   ```ini
   Restart=no
   ```
   No debe repetirse. Si falla, se investiga manualmente.

3. **Worker que sale con exit 0:**
   ```ini
   Restart=on-failure
   RestartPreventExitStatus=0
   RestartSec=5
   ```
   Exit 0 = "no hay trabajo", no reiniciar. Exit != 0 = error, reiniciar.

4. **Servicio crítico con reboot:**
   ```ini
   Restart=always
   RestartSec=5
   StartLimitIntervalSec=60
   StartLimitBurst=3
   StartLimitAction=reboot
   ```
   Siempre reiniciar. Si falla 3 veces en 1 min → reboot de la máquina.

5. **Servicio con watchdog:**
   ```ini
   Type=notify
   WatchdogSec=30
   Restart=on-watchdog
   RestartSec=10
   ```
   Solo reinicia si el watchdog expira (servicio colgado).

</details>

### Ejercicio 9 — NRestarts y monitoreo

Escribe un comando que muestre el número de reinicios de todos los servicios activos. ¿Hay alguno con NRestarts > 0?

```bash
systemctl list-units --type=service --state=active --no-legend --plain | \
    awk '{print $1}' | while read svc; do
        NR=$(systemctl show "$svc" --property=NRestarts --value 2>/dev/null)
        if [[ "$NR" -gt 0 ]] 2>/dev/null; then
            printf "%-40s NRestarts=%s\n" "$svc" "$NR"
        fi
    done
echo "---"
echo "Si no hay salida, ningún servicio se ha reiniciado (sistema sano)"
```

<details><summary>Predicción</summary>

En un sistema sano, la mayoría de servicios tendrán `NRestarts=0`. Si alguno tiene NRestarts > 0, indica que:
- El servicio falló y fue reiniciado automáticamente
- Vale la pena investigar: `journalctl -u SERVICIO | grep -E "Failed|Started|Stopped"`

`NRestarts` es un contador acumulativo desde el último arranque del sistema. Se resetea al reiniciar el sistema o al ejecutar `systemctl reset-failed`.

Este tipo de comando es útil para monitoreo: un script de health check puede alertar si `NRestarts` supera un umbral, indicando un servicio inestable que merece atención.

</details>

### Ejercicio 10 — Checklist de restart para producción

Revisa un servicio existente del sistema y verifica si cumple el checklist de restart para producción. Si no, ¿qué le falta?

```bash
SVC="sshd"
echo "=== Checklist de restart para $SVC ==="

RESTART=$(systemctl show "$SVC" --property=Restart --value)
RESTARTSEC=$(systemctl show "$SVC" --property=RestartUSec --value)
BURST=$(systemctl show "$SVC" --property=StartLimitBurst --value)
INTERVAL=$(systemctl show "$SVC" --property=StartLimitIntervalUSec --value)
WATCHDOG=$(systemctl show "$SVC" --property=WatchdogUSec --value)
TIMEOUT_START=$(systemctl show "$SVC" --property=TimeoutStartUSec --value)
TIMEOUT_STOP=$(systemctl show "$SVC" --property=TimeoutStopUSec --value)
KILLMODE=$(systemctl show "$SVC" --property=KillMode --value)

printf "  Restart=          %s\n" "$RESTART"
printf "  RestartUSec=      %s\n" "$RESTARTSEC"
printf "  StartLimitBurst=  %s\n" "$BURST"
printf "  StartLimitInterval= %s\n" "$INTERVAL"
printf "  WatchdogUSec=     %s\n" "$WATCHDOG"
printf "  TimeoutStartUSec= %s\n" "$TIMEOUT_START"
printf "  TimeoutStopUSec=  %s\n" "$TIMEOUT_STOP"
printf "  KillMode=         %s\n" "$KILLMODE"
```

<details><summary>Predicción</summary>

Checklist de producción:
- [x] `Restart=on-failure` o `always` — sshd típicamente tiene `on-failure`
- [ ] `RestartSec=` razonable (5-30s) — sshd puede tener el default de 100ms
- [x] `StartLimitBurst` definido — típicamente 5 por defecto del sistema
- [x] `StartLimitIntervalSec` definido — típicamente 10s por defecto
- [ ] `WatchdogSec=` — sshd probablemente no tiene watchdog (0)
- [x] `TimeoutStartSec/StopSec` — tiene defaults del manager (90s)
- [x] `KillMode=control-group` — el default, correcto

Lo que puede faltar:
- `RestartSec` explícito (100ms es demasiado agresivo para un servicio de producción)
- `WatchdogSec` no está configurado (sshd no usa sd_notify para watchdog, aunque sí para readiness)
- Los `StartLimit` defaults del sistema pueden ser insuficientes para producción

Un servicio propio debería definir explícitamente todos estos valores en lugar de depender de los defaults.

</details>
