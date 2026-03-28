# T04 — Restart policies

## Cómo funciona el restart automático

```
┌─────────────────────────────────────────────────────────────┐
│                 CICLO DE VIDA DE UN SERVICIO                   │
│                                                              │
│   systemctl start myapp                                      │
│         │                                                   │
│         ▼                                                    │
│   ┌──────────┐    exitosa    ┌──────────────┐               │
│   │ ExecStart │───────────▶│ Service Active│               │
│   │  running  │             └──────────────┘               │
│   └─────┬─────┘                  │                          │
│         │                        │                          │
│         │ processo               │ systemctl stop            │
│         │ sale                  │                          │
│         ▼                        ▼                          │
│   ┌──────────┐             ┌──────────────┐                  │
│   │ ExecStart │             │ Stop sequence  │               │
│   │  finished │             │ (ExecStop)   │               │
│   └─────┬─────┘             └──────────────┘               │
│         │                                                   │
│         │                                                   │
│    ¿Restart=?             ┌──────────────┐                  │
│         │                 │ Service Dead  │                  │
│         ▼                 └──────────────┘                  │
│   ┌──────────┐                                           │
│   │ Restart? │                                           │
│   └────┬─────┘                                           │
│        │                                                   │
│        ├─ Restart=no → Service Dead                        │
│        │                                                   │
│        ├─ Restart=on-success → ¿exit==0? → Service Dead    │
│        │                              └─ SI → Restart now  │
│        │                                                   │
│        ├─ Restart=on-failure → ¿exit!=0 o señal? → Restart │
│        │                              └─ no → Service Dead  │
│        │                                                   │
│        └─ Restart=always → Restart always                  │
└─────────────────────────────────────────────────────────────┘
```

## Restart= — Cuándo reiniciar

```ini
[Service]
Restart=on-failure
```

| Valor | ¿Cuándo reinicia? | Caso de uso |
|---|---|---|
| `no` | Nunca (default) | Scripts oneshot, tareas que no deben repetirse |
| `always` | Siempre (cualquier salida) | Servicios críticos que deben estar siempre |
| `on-success` | Solo exit code 0 | Tareas que deben repetirse al completarse |
| `on-failure` | Exit != 0, señal, timeout, watchdog | **El más común** — solo reiniciar en errores |
| `on-abnormal` | Señal, timeout, watchdog (NO exit code) | Para servicios que salen "limpio" con error |
| `on-abort` | Solo por señal (crash real) | Servicios que no deben reiniciar en errores controlados |
| `on-watchdog` | Solo por watchdog timeout | Solo si watchdog expira |

### Tabla de verdad completa

| Causa de salida | `always` | `on-success` | `on-failure` | `on-abnormal` | `on-abort` | `on-watchdog` |
|---|---|---|---|---|---|---|
| Exit code 0 | ✅ | ✅ | ❌ | ❌ | ❌ | ❌ |
| Exit code != 0 | ✅ | ❌ | ✅ | ❌ | ❌ | ❌ |
| Señal (SIGTERM, etc.) | ✅ | ❌ | ✅ | ✅ | ✅ | ❌ |
| Timeout (start/stop) | ✅ | ❌ | ✅ | ✅ | ❌ | ❌ |
| Watchdog timeout | ✅ | ❌ | ✅ | ✅ | ❌ | ✅ |

### ExitStatus — Control fino de reinicio

```ini
[Service]
# NO reiniciar si sale con estos códigos:
RestartPreventExitStatus=0 SIGTERM
# Exit 0 y SIGTERM = shutdown intencional → no reiniciar

# FORZAR reinicio para ciertos códigos (aunque Restart=no):
RestartForceExitStatus=1 SIGSEGV
# Exit 1 y SIGSEGV siempre reinician
# Sobreescribe RestartPreventExitStatus

# Definir qué códigos son "éxito":
SuccessExitStatus=42 SIGHUP
# Exit 42 y SIGHUP = éxito (como exit 0)
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

## Start Limits — Prevenir restart loops

Cuando un servicio falla repetidamente, systemd deja de intentarlo:

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
Intento 5: falla → espera 10s
Intento 6: FALLO → start-limit-hit
→ La unidad entra en FAILED
→ systemd deja de intentar reiniciar
```

```bash
# Cuando se alcanza el start limit:
systemctl status myapp
# ○ myapp.service
#    Active: failed (Result: start-limit-hit) since ...

# Resetear y reiniciar:
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
StartLimitAction=reboot       # reiniciar la máquina completa
StartLimitAction=reboot-force # reboot inmediato (sin shutdown clean)
StartLimitAction=poweroff     # apagar la máquina
```

```ini
# Ejemplo: servicio que si falla mucho, reinicia el servidor
[Unit]
StartLimitIntervalSec=60
StartLimitBurst=3
StartLimitAction=reboot

[Service]
Restart=on-failure
RestartSec=5
# Si falla 3 veces en 60 segundos → reboot del servidor
```

## Timeouts

```ini
[Service]
# Timeout para que el servicio ARRANQUE:
TimeoutStartSec=30
# Si ExecStart no completa en 30s:
# 1. Envía SIGTERM
# 2. Espera TimeoutStopSec
# 3. Envía SIGKILL
# Default: 90s (DefaultTimeoutStartSec en /etc/systemd/system.conf)

# Timeout para que el servicio PARE:
TimeoutStopSec=30
# Si ExecStop no completa en 30s:
# 1. Envía SIGTERM
# 2. Espera TimeoutStopSec
# 3. Envía SIGKILL
# Default: 90s (DefaultTimeoutStopSec)

# Ambos a la vez:
TimeoutSec=30
# Equivale a: TimeoutStartSec=30 + TimeoutStopSec=30

# Sin timeout (indefinido):
TimeoutStartSec=infinity

# Señal para timeout de stop:
TimeoutStopFailureMode=terminate  # SIGTERM (default)
TimeoutStopFailureMode=abort     # SIGABRT (core dump)
TimeoutStopFailureMode=kill      # SIGKILL (inmediato)
```

## Señales de stop

```ini
[Service]
# Señal enviada con systemctl stop (default: SIGTERM):
KillSignal=SIGTERM

# Señal final si no responde (default: SIGKILL):
FinalKillSignal=SIGKILL

# A quién enviar la señal:
KillMode=control-group   # TODOS los procesos del cgroup (default, más seguro)
KillMode=process        # solo el proceso principal
KillMode=mixed         # SIGTERM al principal, SIGKILL a los demás
KillMode=none          # no matar nada (solo ExecStop — peligroso)

# Enviar SIGHUP antes de SIGTERM (algunos daemons hacen cleanup con HUP):
SendSIGHUP=yes
```

```
KillMode详解:

control-group (default):
  → Envía la señal a TODOS los procesos del cgroup
  → El más seguro — mata trabajadores, children, etc.

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

El watchdog es un health check que verifica que el servicio sigue respondiendo:

```
┌─────────────────────────────────────────────────────────────┐
│                    WATCHDOG EN SYSTEMD                        │
│                                                              │
│   Service running                                            │
│        │                                                    │
│        │ sd_notify("WATCHDOG=1") cada WatchdogSec/2       │
│        │ (ej: cada 15s si WatchdogSec=30)                  │
│        ▼                                                    │
│   Si watchdog no recibe ping en WatchdogSec (30s):          │
│        │                                                    │
│        ├─ Restart=on-failure → reiniciar el servicio        │
│        └─ Restart=on-watchdog → reiniciar                    │
│                                                              │
│   Si el servicio deja de responder:                          │
│        → systemd lo detecta                                  │
│        → Restart según la policy                             │
└─────────────────────────────────────────────────────────────┘
```

```ini
[Service]
Type=notify              # NECESARIO para watchdog
WatchdogSec=30          # Debe recibir ping cada 30/2=15 segundos
Restart=on-failure       # on-failure incluye watchdog timeout

# El servicio debe usar sd_notify() para ping:
```

```bash
# En C:
sd_notify(0, "WATCHDOG=1");

# En Python:
import systemd.daemon
systemd.daemon.notify("WATCHDOG=1")

# En bash:
systemd-notify WATCHDOG=1

# En Go:
sdnotify.Notify("WATCHDOG=1")
```

```bash
# Ver propiedades de watchdog
systemctl show nginx --property=WatchdogSec,NotifyAccess
# WatchdogSec=0 (no configurado)
# NotifyAccess=main (nginx acepta notificaciones)

# Un servicio con Type=notify y WatchdogSec=0 → sin watchdog
```

## Combinación típica para producción

```ini
[Unit]
Description=Production API Server
Documentation=https://api.example.com/docs
After=network-online.target postgresql.service redis.service
Wants=network-online.target
Requires=postgresql.service

StartLimitIntervalSec=600    # 10 minutos
StartLimitBurst=5
StartLimitAction=none        # dejar de reiniciar, no apagar servidor

[Service]
Type=notify                 # o simple
User=apiserver
Group=apiserver

ExecStart=/opt/api/bin/server --config /etc/api/config.yaml
ExecReload=/bin/kill -HUP $MAINPID

Restart=on-failure           # reiniciar en errores
RestartSec=10               # esperar 10 segundos

WatchdogSec=30              # health check cada 30 segundos
TimeoutStartSec=60          # 1 minuto para arrancar
TimeoutStopSec=30           # 30 segundos para parar

# Recursos
MemoryMax=2G
CPUQuota=200%
LimitNOFILE=65535

# Seguridad
NoNewPrivileges=true
ProtectSystem=strict
ProtectHome=yes
PrivateTmp=true
ReadWritePaths=/var/lib/api /var/log/api

[Install]
WantedBy=multi-user.target
```

## Servicio que no debe reiniciar en exit limpio

```ini
[Service]
Type=oneshot
ExecStart=/opt/myapp/bin/migrate-db.sh
RemainAfterExit=no
# Exit 0 = migración exitosa → no reiniciar
# Exit != 0 = error → reiniciar
Restart=on-failure
RestartPreventExitStatus=0
```

## Servicio con backoff exponencial (simulado)

Systemd no tiene backoff exponencial nativo, pero se puede simular:

```ini
# Wrapper script en bash:
[Service]
Type=simple
ExecStart=/usr/local/bin/myapp-wrapper.sh
Restart=on-failure
RestartSec=5
```

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
    sleep $WAIT
done
```

## Verificar configuración

```bash
# Propiedades de restart:
systemctl show myapp --property=Restart,RestartSec,RestartUSec
systemctl show myapp --property=StartLimitBurst,StartLimitIntervalUSec
systemctl show myapp --property=WatchdogSec,TimeoutStartUSec

# Ver cuántas veces se ha reiniciado:
systemctl show myapp --property=NRestarts

# Ver logs de los últimos arranques:
journalctl -u myapp --no-pager -b | grep -E "Started|Failed"

# Ver el estado completo:
systemctl status myapp -l
```

## Ejercicios

### Ejercicio 1 — Servicio que crashea y reinicia

```bash
# 1. Crear script que crashea
sudo tee /usr/local/bin/crash-test.sh << 'EOF'
#!/bin/bash
echo "$(date): Process started (PID $$)"
sleep 2
echo "$(date): Crashing with exit code 1"
exit 1
EOF
sudo chmod +x /usr/local/bin/crash-test.sh

# 2. Crear servicio
sudo tee /etc/systemd/system/crash-test.service << 'EOF'
[Unit]
Description=Crash Test Service
StartLimitIntervalSec=120
StartLimitBurst=3

[Service]
Type=simple
ExecStart=/usr/local/bin/crash-test.sh
Restart=on-failure
RestartSec=5
TimeoutStartSec=10
EOF

# 3. Recargar y arrancar
sudo systemctl daemon-reload
sudo systemctl start crash-test

# 4. Observar logs
journalctl -u crash-test -f --no-pager

# 5. Ver cómo alcanza el start limit
# Watch 3 crashes, then:
systemctl status crash-test

# 6. Resetear
sudo systemctl reset-failed crash-test

# 7. Limpiar
sudo systemctl stop crash-test
sudo rm /etc/systemd/system/crash-test.service /usr/local/bin/crash-test.sh
sudo systemctl daemon-reload
```

### Ejercicio 2 — Comparar políticas de restart

```bash
# Pregunta: ¿Qué Restart= usarías para cada caso?

# 1. nginx en producción
# → Restart=on-failure (o always si es crítico)
# → Reason: reiniciar en errores, no en exit limpio

# 2. Script de backup (oneshot)
# → Restart=no
# → Reason: si falla, debe reportar error, no reiniciar automáticamente

# 3. Worker que sale con exit 0 cuando no hay trabajo
# → Restart=on-failure
# → RestartPreventExitStatus=0
# → Reason: exit 0 es "no hay trabajo", no es error

# 4. Servicio que solo debe reiniciar si crashea (no por error)
# → Restart=on-abort
# → Reason: señales tipo SIGSEGV = crash real

# 5. Un servicio con watchdog externo
# → Restart=on-watchdog
# → Reason: solo si el watchdog detecta inactividad
```

### Ejercicio 3 — Inspeccionar restart de servicios reales

```bash
# 1. sshd
systemctl show sshd --property=Restart,RestartSec,StartLimitBurst
systemctl show sshd --property=NRestarts

# 2. nginx
systemctl show nginx --property=Restart,RestartSec
systemctl show nginx --property=WatchdogSec,NotifyAccess

# 3. cron
systemctl show cron --property=Restart,RestartSec
# cron generalmente Restart=no (espera a que expire)

# 4. Comparar con docker
systemctl show docker --property=Restart,RestartSec,TimeoutStartSec
```

### Ejercicio 4 — Servicio que no reinicia en exit 0

```bash
# 1. Crear script
sudo tee /usr/local/bin/graceful-exit.sh << 'EOF'
#!/bin/bash
echo "$(date): Starting"
sleep 1
# Simula shutdown limpio intencional
exit 0
EOF
sudo chmod +x /usr/local/bin/graceful-exit.sh

# 2. Crear servicio
sudo tee /etc/systemd/system/graceful-exit.service << 'EOF'
[Unit]
Description=Graceful Exit Test
StartLimitIntervalSec=60
StartLimitBurst=2

[Service]
Type=simple
ExecStart=/usr/local/bin/graceful-exit.sh
Restart=on-failure
RestartPreventExitStatus=0
RestartSec=3
EOF

# 3. Probar
sudo systemctl daemon-reload
sudo systemctl start graceful-exit

# 4. Ver estado
systemctl status graceful-exit
# Debería: inactive (dead) — NO reiniciar (exit 0)

# 5. Modificar script para exit != 0
sudo tee /usr/local/bin/graceful-exit.sh << 'EOF'
#!/bin/bash
echo "$(date): Starting with error"
exit 1
EOF

# 6. Reiniciar
sudo systemctl start graceful-exit

# 7. Ver que ahora SÍ reinicia
systemctl status graceful-exit
# Debería: active (running) → crashed → restart

# 8. Limpiar
sudo systemctl stop graceful-exit
sudo rm /etc/systemd/system/graceful-exit.service /usr/local/bin/graceful-exit.sh
sudo systemctl daemon-reload
```

### Ejercicio 5 — Timeout y signals

```bash
# 1. Crear script que tarda mucho
sudo tee /usr/local/bin/slow-start.sh << 'EOF'
#!/bin/bash
echo "$(date): Starting slow service..."
sleep 15
echo "$(date): Done"
exit 0
EOF
sudo chmod +x /usr/local/bin/slow-start.sh

# 2. Crear servicio con timeout corto
sudo tee /etc/systemd/system/slow-start.service << 'EOF'
[Unit]
Description=Slow Starting Service

[Service]
Type=simple
ExecStart=/usr/local/bin/slow-start.sh
TimeoutStartSec=5   # Solo 5 segundos!
Restart=no
EOF

# 3. Probar
sudo systemctl daemon-reload
sudo systemctl start slow-start

# 4. Ver que falla por timeout
systemctl status slow-start
journalctl -u slow-start -n 10

# 5. Modificar TimeoutStartSec a 30 y probar de nuevo
# 6. Limpiar
sudo systemctl stop slow-start 2>/dev/null || true
sudo rm /etc/systemd/system/slow-start.service /usr/local/bin/slow-start.sh
sudo systemctl daemon-reload
```

### Ejercicio 6 — Simular start limit y reboot action

```bash
# 1. Crear servicio que siempre falla
sudo tee /usr/local/bin/always-fail.sh << 'EOF'
#!/bin/bash
echo "$(date): Failing..."
exit 1
EOF
sudo chmod +x /usr/local/bin/always-fail.sh

# 2. Crear servicio con StartLimitAction
sudo tee /etc/systemd/system/always-fail.service << 'EOF'
[Unit]
Description=Always Fail Service
StartLimitIntervalSec=30
StartLimitBurst=3
StartLimitAction=poweroff

[Service]
Type=simple
ExecStart=/usr/local/bin/always-fail.sh
Restart=on-failure
RestartSec=2
EOF

# 3. NO ejecutar en producción — esto apagará la máquina
# En un VM o entorno de prueba:
# sudo systemctl start always-fail
# → 3 intentos → poweroff

# 4. Limpiar
sudo rm /etc/systemd/system/always-fail.service /usr/local/bin/always-fail.sh
sudo systemctl daemon-reload
```
