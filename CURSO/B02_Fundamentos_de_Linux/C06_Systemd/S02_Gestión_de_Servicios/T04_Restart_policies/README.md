# T04 — Restart policies

## Restart= — Cuándo reiniciar

La directiva `Restart=` controla bajo qué condiciones systemd reinicia un
servicio automáticamente:

```ini
[Service]
Restart=on-failure
```

| Valor | Reinicia en | Caso de uso |
|---|---|---|
| `no` | Nunca (default) | Tareas oneshot, scripts que no deben repetirse |
| `always` | Siempre (exit, señal, timeout, watchdog) | Servicios críticos que deben estar siempre corriendo |
| `on-success` | Solo si exit code 0 | Tareas que deben repetirse al completarse |
| `on-failure` | Exit != 0, señal, timeout, watchdog | **El más común** — reiniciar solo en errores |
| `on-abnormal` | Señal, timeout, watchdog (NO exit code) | Servicios que pueden salir limpiamente con exit != 0 |
| `on-abort` | Solo por señal (crash) | Servicios que salen con error controlado pero no deben reiniciar |
| `on-watchdog` | Solo por watchdog timeout | Servicios con watchdog habilitado |

### Tabla de condiciones

| Causa de salida | `always` | `on-success` | `on-failure` | `on-abnormal` | `on-abort` | `on-watchdog` |
|---|---|---|---|---|---|---|
| Exit limpio (0) | Sí | Sí | | | | |
| Exit error (!=0) | Sí | | Sí | | | |
| Señal (crash) | Sí | | Sí | Sí | Sí | |
| Timeout | Sí | | Sí | Sí | | |
| Watchdog | Sí | | Sí | Sí | | Sí |

```ini
# Excluir exit codes específicos del reinicio:
RestartPreventExitStatus=0 3 SIGTERM
# No reiniciar si el proceso sale con exit code 0, 3, o por SIGTERM
# Útil cuando exit 0 significa "shutdown limpio intencional"

# Forzar reinicio para exit codes específicos:
RestartForceExitStatus=1 SIGSEGV
# Siempre reiniciar si sale con exit 1 o SIGSEGV
# Tiene precedencia sobre RestartPreventExitStatus

# SuccessExitStatus — considerar como éxito exit codes no estándar:
SuccessExitStatus=42 SIGHUP
# Exit 42 y SIGHUP se tratan como éxito (no como fallo)
```

## RestartSec= — Tiempo entre reinicios

```ini
[Service]
Restart=on-failure
RestartSec=5
# Esperar 5 segundos antes de reiniciar
# Default: 100ms (muy agresivo — cambiar siempre)

RestartSec=30s     # 30 segundos
RestartSec=1min    # 1 minuto
RestartSec=5       # 5 segundos (las unidades son opcionales para segundos)
```

```ini
# Sin RestartSec, un servicio que crashea inmediatamente
# se reinicia 10 veces por segundo → consume CPU y llena logs
# SIEMPRE definir RestartSec cuando Restart != no
```

## Start limit — Prevenir loops de reinicio

Si un servicio falla repetidamente, systemd deja de reiniciarlo:

```ini
[Unit]
# Configurar el límite:
StartLimitIntervalSec=300
StartLimitBurst=5
# Máximo 5 arranques en 300 segundos (5 minutos)
# Si se excede → la unidad entra en estado failed (Result=start-limit-hit)
# y NO se reinicia más

# Acción al alcanzar el límite:
StartLimitAction=none         # solo dejar de reiniciar (default)
StartLimitAction=reboot       # reiniciar la máquina
StartLimitAction=reboot-force # reiniciar inmediatamente (sin shutdown limpio)
StartLimitAction=poweroff     # apagar la máquina
```

```bash
# Cuando el start limit se alcanza:
systemctl status myapp.service
# Active: failed (Result: start-limit-hit)
# Indica que se excedió el límite de reinicios

# Para resetear y poder reiniciar:
sudo systemctl reset-failed myapp.service
sudo systemctl start myapp.service
```

```ini
# Deshabilitar el start limit (cuidado — puede causar loops infinitos):
[Unit]
StartLimitIntervalSec=0
# 0 = sin límite
```

### Combinación típica

```ini
[Unit]
StartLimitIntervalSec=600     # ventana de 10 minutos
StartLimitBurst=5             # máximo 5 arranques en esa ventana

[Service]
Restart=on-failure
RestartSec=10                 # 10 segundos entre reinicios
# Con estos valores:
# Intento 1: falla → espera 10s
# Intento 2: falla → espera 10s
# Intento 3: falla → espera 10s
# Intento 4: falla → espera 10s
# Intento 5: falla → start-limit-hit (no más reinicios)
# Total: ~50 segundos de intentos en la ventana de 600 segundos
```

## Timeouts

```ini
[Service]
# Tiempo máximo para que el servicio arranque:
TimeoutStartSec=30
# Si ExecStart no completa en 30s → envía SIGTERM → timeout → SIGKILL
# Default: DefaultTimeoutStartSec del systemd manager (90s)

# Tiempo máximo para que el servicio pare:
TimeoutStopSec=30
# Si ExecStop no completa en 30s → SIGKILL
# Default: DefaultTimeoutStopSec del systemd manager (90s)

# Ambos a la vez:
TimeoutSec=30
# Equivale a TimeoutStartSec=30 + TimeoutStopSec=30

# Sin timeout (esperar indefinidamente):
TimeoutStartSec=infinity

# Señal a enviar al timeout de stop (antes de SIGKILL):
TimeoutStopFailureMode=terminate   # SIGTERM (default)
TimeoutStopFailureMode=abort       # SIGABRT (genera core dump)
TimeoutStopFailureMode=kill        # SIGKILL (inmediato)
```

### Señales de stop

```ini
[Service]
# Señal enviada al hacer systemctl stop (default: SIGTERM):
KillSignal=SIGTERM

# Señal final si el proceso no muere después del timeout:
FinalKillSignal=SIGKILL    # default

# A quién enviar la señal:
KillMode=control-group     # a TODOS los procesos del cgroup (default, más seguro)
KillMode=process           # solo al proceso principal
KillMode=mixed             # SIGTERM al principal, SIGKILL a los demás después del timeout
KillMode=none              # no matar nada (peligroso — solo ExecStop)

# Enviar SIGHUP antes de SIGTERM:
SendSIGHUP=yes             # algunos daemons hacen cleanup con HUP
```

## WatchdogSec= — Health check

```ini
[Service]
Type=notify
WatchdogSec=30
# El servicio debe enviar sd_notify("WATCHDOG=1") cada 30 segundos
# Si no lo hace, systemd lo considera muerto y actúa según Restart=
# El servicio debería enviar el ping cada WatchdogSec/2 (cada 15s)
# Solo funciona con Type=notify y servicios que usan la API sd_notify

Restart=on-watchdog
# O más comúnmente:
Restart=on-failure
# on-failure incluye watchdog timeouts
```

```bash
# El servicio usa la API de systemd:
# En C: sd_notify(0, "WATCHDOG=1");
# En Python: systemd.daemon.notify("WATCHDOG=1")
# En bash (con systemd-notify):
# systemd-notify WATCHDOG=1
```

## Patrones profesionales

### Servicio de producción

```ini
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
```

### Servicio que no debe reiniciar en exit limpio

```ini
[Service]
Restart=on-failure
RestartPreventExitStatus=0
# Exit 0 = shutdown intencional → no reiniciar
# Exit != 0 = error → reiniciar
```

### Servicio con backoff exponencial

systemd no tiene backoff exponencial nativo, pero se puede simular:

```ini
[Service]
Type=simple
ExecStart=/opt/myapp/bin/server
Restart=on-failure
RestartSec=5

# Limitar reinicios rápidos:
[Unit]
StartLimitIntervalSec=600
StartLimitBurst=10
```

```bash
# Para backoff real, usar un wrapper script:
#!/bin/bash
RETRY=0
MAX_RETRIES=10

while true; do
    /opt/myapp/bin/server
    EXIT_CODE=$?

    [[ $EXIT_CODE -eq 0 ]] && exit 0

    ((RETRY++))
    if [[ $RETRY -ge $MAX_RETRIES ]]; then
        echo "Max retries reached" >&2
        exit 1
    fi

    WAIT=$((2 ** RETRY))
    echo "Retry $RETRY/$MAX_RETRIES in ${WAIT}s..." >&2
    sleep "$WAIT"
done
```

## Verificar la configuración de restart

```bash
# Ver las propiedades de restart de un servicio:
systemctl show myapp.service --property=Restart,RestartSec,StartLimitBurst,StartLimitIntervalUSec

# Simular un crash para probar el restart:
# (en un entorno de pruebas)
sudo systemctl kill -s SIGKILL myapp.service
# Debería reiniciar según la policy configurada

# Ver cuántas veces se ha reiniciado:
systemctl show myapp.service --property=NRestarts
# NRestarts=3

# Ver logs de los reinicios:
journalctl -u myapp.service | grep -E "Started|Stopped|Failed"
```

---

## Ejercicios

### Ejercicio 1 — Observar restart

```bash
# Crear un servicio que crashea:
sudo tee /usr/local/bin/crasher.sh << 'SCRIPT'
#!/bin/bash
echo "Starting (PID $$)..."
sleep 3
echo "Crashing!"
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

# Observar los reinicios:
journalctl -u crasher -f
# Esperar a que alcance el start limit

# Ver el estado:
systemctl status crasher

# Limpiar:
sudo systemctl stop crasher
sudo rm /etc/systemd/system/crasher.service /usr/local/bin/crasher.sh
sudo systemctl daemon-reload
```

### Ejercicio 2 — Inspeccionar restart de servicios reales

```bash
# ¿Qué política de restart tiene sshd?
systemctl show sshd --property=Restart,RestartUSec

# ¿Cuántas veces se ha reiniciado?
systemctl show sshd --property=NRestarts
```

### Ejercicio 3 — Comparar políticas

```bash
# Pregunta conceptual:
# ¿Qué Restart= usarías para cada caso?
# 1. Un servidor web de producción → ?
# 2. Un script de migración de datos (oneshot) → ?
# 3. Un worker que puede salir con exit 0 cuando no hay trabajo → ?
# 4. Un servicio que solo debe reiniciar si crashea por señal → ?

# Respuestas:
# 1. on-failure (o always si es MUY crítico)
# 2. no (no debe repetirse)
# 3. on-failure (exit 0 es intencional, no reiniciar)
# 4. on-abort
```
