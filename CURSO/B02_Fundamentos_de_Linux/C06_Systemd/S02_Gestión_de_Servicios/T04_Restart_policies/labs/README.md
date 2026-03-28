# Lab — Restart policies

## Objetivo

Dominar las politicas de reinicio de systemd: los valores de
Restart= y bajo que condiciones actuan, RestartSec para el
intervalo entre reinicios, StartLimitBurst/IntervalSec para
prevenir loops, timeouts de arranque y parada, KillMode y senales,
WatchdogSec para health checks, y patrones profesionales.

## Prerequisitos

- Entorno del curso levantado (`docker compose up -d`)

---

## Parte 1 — Restart y RestartSec

### Objetivo

Entender los valores de Restart= y cuando se aplican.

### Paso 1.1: Valores de Restart=

```bash
docker compose exec debian-dev bash -c '
echo "=== Valores de Restart= ==="
echo ""
echo "| Valor        | Reinicia en                              |"
echo "|------------- |-----------------------------------------|"
echo "| no           | Nunca (default)                          |"
echo "| always       | Siempre (exit, senal, timeout, watchdog) |"
echo "| on-success   | Solo si exit code 0                      |"
echo "| on-failure   | Exit != 0, senal, timeout, watchdog      |"
echo "| on-abnormal  | Senal, timeout, watchdog (NO exit code)  |"
echo "| on-abort     | Solo por senal (crash)                   |"
echo "| on-watchdog  | Solo por watchdog timeout                |"
echo ""
echo "--- Tabla de condiciones ---"
echo ""
echo "| Causa          | always | on-success | on-failure | on-abnormal | on-abort |"
echo "|--------------- |--------|-----------|-----------|------------|---------|"
echo "| Exit limpio (0)| Si     | Si        |           |            |         |"
echo "| Exit error     | Si     |           | Si        |            |         |"
echo "| Senal (crash)  | Si     |           | Si        | Si         | Si      |"
echo "| Timeout        | Si     |           | Si        | Si         |         |"
echo "| Watchdog       | Si     |           | Si        | Si         |         |"
echo ""
echo "on-failure es el MAS COMUN para servicios de produccion"
'
```

### Paso 1.2: Crear unit files con diferentes Restart=

```bash
docker compose exec debian-dev bash -c '
echo "=== Ejemplos de Restart= ==="
echo ""
echo "--- on-failure (el mas comun) ---"
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
echo "  Reinicia si exit != 0, senal, timeout, watchdog"
echo "  Espera 5 segundos entre reinicios"
echo ""
echo "--- always (servicios criticos) ---"
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

### Paso 1.3: RestartSec y exit codes

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
echo "Considerar como exito exit codes no estandar:"
echo "  SuccessExitStatus=42 SIGHUP"
echo "  Exit 42 y SIGHUP se tratan como exito"
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

## Parte 2 — Start limits y timeouts

### Objetivo

Prevenir loops de reinicio infinitos y configurar timeouts.

### Paso 2.1: StartLimitBurst e IntervalSec

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
echo "StartLimitBurst=5          maximo 5 arranques"
echo ""
echo "Secuencia:"
echo "  Intento 1: falla → espera 5s"
echo "  Intento 2: falla → espera 5s"
echo "  Intento 3: falla → espera 5s"
echo "  Intento 4: falla → espera 5s"
echo "  Intento 5: falla → start-limit-hit (no mas reinicios)"
echo ""
echo "Estado: failed (Result: start-limit-hit)"
echo "Para resetear: systemctl reset-failed SERVICE"
echo ""
echo "--- Deshabilitar (peligroso) ---"
echo "StartLimitIntervalSec=0"
echo "0 = sin limite (puede causar loops infinitos)"
'
```

### Paso 2.2: StartLimitAction

```bash
docker compose exec debian-dev bash -c '
echo "=== StartLimitAction ==="
echo ""
echo "Que hacer al alcanzar el limite de reinicios:"
echo ""
echo "  StartLimitAction=none          solo dejar de reiniciar (default)"
echo "  StartLimitAction=reboot        reiniciar la maquina"
echo "  StartLimitAction=reboot-force  reiniciar inmediato (sin shutdown)"
echo "  StartLimitAction=poweroff      apagar la maquina"
echo ""
echo "--- Ejemplo: servicio critico ---"
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
echo "CUIDADO: StartLimitAction=reboot reinicia la MAQUINA"
echo "Solo para servicios realmente criticos"
'
```

### Paso 2.3: Timeouts

```bash
docker compose exec debian-dev bash -c '
echo "=== Timeouts ==="
echo ""
echo "--- TimeoutStartSec ---"
echo "Tiempo maximo para que el servicio arranque"
echo "Default: 90s (DefaultTimeoutStartSec del manager)"
echo "Si ExecStart no completa → SIGTERM → timeout → SIGKILL"
echo ""
echo "--- TimeoutStopSec ---"
echo "Tiempo maximo para que el servicio pare"
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
echo "A quien enviar la senal de stop:"
echo "  control-group  todos los procesos del cgroup (default, seguro)"
echo "  process        solo el proceso principal"
echo "  mixed          SIGTERM al principal, SIGKILL a los demas"
echo "  none           no matar (peligroso, solo ExecStop)"
echo ""
echo "--- KillSignal ---"
echo "Senal enviada al hacer stop (default: SIGTERM)"
echo "FinalKillSignal: senal final si no muere (default: SIGKILL)"
'
```

---

## Parte 3 — Watchdog y patrones

### Objetivo

Configurar health checks con WatchdogSec y crear patrones
de reinicio profesionales.

### Paso 3.1: WatchdogSec

```bash
docker compose exec debian-dev bash -c '
echo "=== WatchdogSec ==="
echo ""
echo "El servicio debe enviar sd_notify(WATCHDOG=1) periodicamente"
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
echo "  Recomendacion: enviar cada WatchdogSec/2 (cada 15s)"
echo "  Solo funciona con Type=notify"
echo ""
echo "--- Como enviar el watchdog ---"
echo "C:      sd_notify(0, \"WATCHDOG=1\");"
echo "Python: systemd.daemon.notify(\"WATCHDOG=1\")"
echo "Bash:   systemd-notify WATCHDOG=1"
echo ""
echo "--- NRestarts: ver cuantos reinicios ---"
echo "systemctl show SERVICE --property=NRestarts"
'
```

### Paso 3.2: Patron de produccion completo

```bash
docker compose exec debian-dev bash -c '
echo "=== Patron de produccion ==="
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
echo "  [x] TimeoutStartSec/StopSec (limites razonables)"
'
```

### Paso 3.3: Elegir la politica correcta

```bash
docker compose exec debian-dev bash -c '
echo "=== Elegir la politica correcta ==="
echo ""
echo "Pregunta: que Restart= usarias para cada caso?"
echo ""
echo "1. Servidor web de produccion"
echo "   → on-failure (o always si es MUY critico)"
echo ""
echo "2. Script de migracion de datos (oneshot)"
echo "   → no (no debe repetirse)"
echo ""
echo "3. Worker que puede salir con exit 0 cuando no hay trabajo"
echo "   → on-failure (exit 0 es intencional, no reiniciar)"
echo ""
echo "4. Servicio que solo debe reiniciar si crashea por senal"
echo "   → on-abort"
echo ""
echo "5. Servicio critico de infraestructura"
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

## Limpieza final

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

---

## Conceptos reforzados

1. `Restart=on-failure` es la politica mas comun: reinicia en
   exit != 0, senal, timeout, watchdog. `Restart=always` reinicia
   siempre, incluso en exit 0.

2. `RestartSec=` define el intervalo entre reinicios. El default
   (100ms) es demasiado agresivo — siempre definir un valor
   razonable (5-30 segundos).

3. `StartLimitBurst=` y `StartLimitIntervalSec=` previenen loops
   de reinicio infinitos. Al alcanzar el limite, el servicio
   entra en `failed (Result: start-limit-hit)`.

4. `TimeoutStartSec=` y `TimeoutStopSec=` limitan cuanto puede
   tardar un servicio en arrancar y parar. Si se excede, systemd
   envia SIGTERM y luego SIGKILL.

5. `WatchdogSec=` habilita health checks: el servicio debe enviar
   `sd_notify("WATCHDOG=1")` periodicamente. Solo funciona con
   `Type=notify`.

6. `KillMode=control-group` (default) envia la senal de stop a
   TODOS los procesos del cgroup, no solo al principal. Es el
   modo mas seguro para evitar procesos huerfanos.
