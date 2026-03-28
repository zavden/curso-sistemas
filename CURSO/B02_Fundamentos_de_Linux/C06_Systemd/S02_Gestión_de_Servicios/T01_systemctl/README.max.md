# T01 — systemctl

## systemctl — El centro de control de systemd

`systemctl` es la herramienta CLI para interactuar con systemd (PID 1). Se usa para gestionar servicios, targets, sockets, timers, y más:

```
systemctl COMANDO UNIDAD.TIPO

systemctl start nginx.service
systemctl stop nginx
systemctl status nginx.service
systemctl enable nginx
```

## Operaciones básicas con servicios

```bash
# start/stop/restart
sudo systemctl start nginx.service
sudo systemctl stop nginx.service
sudo systemctl restart nginx.service        # stop + start

# reload —.reload configuration WITHOUT restarting the process
sudo systemctl reload nginx.service
# Solo funciona si el servicio tiene ExecReload definido
# Si no tiene, systemctl reload-or-restart es la opción segura

# reload si soporta, restart si no:
sudo systemctl reload-or-restart nginx.service

# try-restart — restart ONLY if already running
sudo systemctl try-restart nginx.service

# is-active — check if running (exit code 0 = yes)
systemctl is-active nginx.service

# is-enabled — check if starts at boot
systemctl is-enabled nginx.service

# El sufijo .service es opcional (systemd assumes .service)
systemctl start nginx    # equivalent to nginx.service
# But: systemctl start network  → could be network.service OR network.target
# Best practice: always include .service for clarity
```

## status — Inspección detallada

```bash
systemctl status nginx.service
```

```
● nginx.service - A high performance web server and a reverse proxy server
     Loaded: loaded (/lib/systemd/system/nginx.service; enabled; preset: enabled)
     Active: active (running) since Mon 2024-03-17 10:00:00 UTC; 5h 23min ago
       Docs: man:nginx(8)
  Main PID: 1235 (nginx)
     Tasks: 3 (limit: 4915)
    Memory: 8.5M
       CPU: 123ms
    CGroup: /system.slice/nginx.service
            ├─1235 "nginx: master process /usr/sbin/nginx -g daemon on; master_process on;"
            ├─1236 "nginx: worker process"  
            └─1237 "nginx: worker process"

Mar 17 10:00:00 server systemd[1]: Starting nginx.service - A high performance web server...
Mar 17 10:00:00 server systemd[1]: Started nginx.service - A high performance web server...
```

### Desglose línea por línea

```
● / ○ / × — Indicador visual:
  ● green  = active (running)
  ● bright = active (exited/finished successfully)  
  ○ white  = inactive (dead)
  × red    = failed

Loaded: loaded (RUTA; ENABLE_STATE; PRESET)
  RUTA         → dónde está el archivo de unidad
  ENABLE_STATE → enabled | disabled | masked | static
  PRESET       → enabled | disabled | (empty) from system preset

Active: ACTIVE_STATE (SUB_STATE) since TIMESTAMP; DURACIÓN ago
  SUB_STATE depende del Type= de la unidad:
    running  → proceso principal corriendo
    exited   → Type=oneshot, ExecStart terminó OK
    listening → socket escuchando
    waiting  → esperando (idle, timers, etc.)

Main PID: 1235 (nginx)
  → El PID que systemd monitorea como "el" proceso del servicio
  → Para Type=forking, es el PID del proceso padre que hizo fork()

Tasks: 3 (limit: 4915)
  → Número de tasks en el cgroup (hijos + main process)
  → limit: máximo configurado en la unidad

Memory: 8.5M
  → Memoria total consumida por el cgroup del servicio

CGroup: /system.slice/nginx.service
  → Jerarquía del cgroup v2 en el sistema de control de recursos
  → Todos los procesos del servicio están aquí
```

### Opciones de status

```bash
# Sin pager (para scripts, CI, Docker):
systemctl status nginx --no-pager

# Más o menos líneas de log:
systemctl status nginx -n 100     # últimas 100 líneas
systemctl status nginx -n 0       # sin logs

# Líneas completas (no truncar rutas):
systemctl status nginx -l

# Todo junto:
systemctl status nginx -l --no-pager -n 50

# Status de múltiples servicios:
systemctl status nginx cron sshd
```

## enable / disable — Arranque al boot

```bash
# Habilitar (arranca automáticamente al boot):
sudo systemctl enable nginx.service
# Created symlink /etc/systemd/system/multi-user.target.wants/nginx.service
#  → /lib/systemd/system/nginx.service

# Deshabilitar (NO arranca al boot):
sudo systemctl disable nginx.service
# Removed /etc/systemd/system/multi-user.target.wants/nginx.service

# enable --now → enable + start inmediatamente
sudo systemctl enable --now nginx.service

# disable --now → disable + stop inmediatamente
sudo systemctl disable --now nginx.service

# is-enabled — verificar
systemctl is-enabled nginx.service
# enabled | disabled | masked | static
```

```bash
# Qué hace enable internamente:
# 1. systemctl lee [Install] → WantedBy=multi-user.target
# 2. Crea symlink:
#    /etc/systemd/system/multi-user.target.wants/nginx.service
#    → /lib/systemd/system/nginx.service
# 3. Al arrancar, multi-user.target activa todo en su .wants/
```

## mask / unmask — Bloqueo total

```bash
# mask — crear symlink a /dev/null (imposible iniciar):
sudo systemctl mask nginx.service
# Created symlink /etc/systemd/system/nginx.service → /dev/null.

# Intentar iniciar:
sudo systemctl start nginx.service
# Failed to start nginx.service: Unit nginx.service is masked.

# unmask — quitar el mask:
sudo systemctl unmask nginx.service
# Removed /etc/systemd/system/nginx.service.
```

```
┌──────────────────────────────────────────────────────────────┐
│  disable vs mask                                              │
├──────────────────────────────────────────────────────────────┤
│  disable:                                                    │
│    • symlink en target.wants/ se elimina                     │
│    • no arranca al boot                                      │
│    • systemctl start → FUNCIONA                             │
│                                                              │
│  mask:                                                       │
│    • symlink a /dev/null                                    │
│    • no arranca al boot                                      │
│    • systemctl start → ERROR "Unit is masked"               │
│    • no puede ser iniciado NI como dependencia              │
└──────────────────────────────────────────────────────────────┘
```

## list-units — Listar unidades

```bash
# Todas las unidades cargadas en memoria:
systemctl list-units

# Filtrar por tipo:
systemctl list-units --type=service
systemctl list-units --type=timer
systemctl list-units --type=socket
systemctl list-units --type=mount

# Filtrar por estado:
systemctl list-units --state=active
systemctl list-units --state=failed
systemctl list-units --state=inactive
systemctl list-units --state=activating

# Combinado:
systemctl list-units --type=service --state=active

# Todas las unidades (incluye inactivas):
systemctl list-units --all

# Formato máquina (sin headers, sin colores):
systemctl list-units --type=service --no-legend --plain --no-pager

# Todas las unit files instaladas (enabled, disabled, static):
systemctl list-unit-files
systemctl list-unit-files --type=service

# Solo las unit files en estado específico:
systemctl list-unit-files --state=enabled
systemctl list-unit-files --state=disabled
systemctl list-unit-files --state=masked
```

```bash
# Unidades fallidas:
systemctl --failed
# Equivalente a: systemctl list-units --state=failed

# Count active services:
systemctl list-units --type=service --state=active --no-legend | wc -l
```

## show — Propiedades de unidad

```bash
# TODAS las propiedades (key=value):
systemctl show nginx.service

# Propiedad específica:
systemctl show nginx.service --property=MainPID
# MainPID=1235

# --value para scripts (solo el valor):
PID=$(systemctl show nginx --property=MainPID --value)

# Múltiples propiedades:
systemctl show nginx --property=ActiveState,SubState,MainPID,Type
# ActiveState=active
# SubState=running
# MainPID=1235
# Type=forking
```

```bash
# Script: verificar si un servicio está activo
is_active() {
    local svc=$1
    local state
    state=$(systemctl show "$svc" --property=ActiveState --value)
    [[ "$state" == "active" ]]
}

if is_active nginx; then
    echo "nginx está corriendo"
fi
```

## cat — Ver archivo de unidad

```bash
# Muestra el archivo ORIGINAL + overrides aplicados:
systemctl cat nginx.service

# Salida ejemplo:
# # /usr/lib/systemd/system/nginx.service
# [Unit]
# Description=A high performance web server...
# ...
#
# # /etc/systemd/system/nginx.service.d/override.conf
# [Service]
# MemoryMax=1G
# Environment=WORKER_PROCESSES=8
```

## edit — Modificar unidades

```bash
# Crear drop-in (override parcial):
sudo systemctl edit nginx.service
# Abre editor con archivo vacío
# Al guardar: crea /etc/systemd/system/nginx.service.d/override.conf
# Ejecuta daemon-reload automáticamente

# Override completo (copia entera a /etc/):
sudo systemctl edit --full nginx.service
# Copia /usr/lib/systemd/system/nginx.service
# a /etc/systemd/system/nginx.service
# ⚠️ Pierde updates del paquete

# Especificar editor:
sudo EDITOR=vim systemctl edit nginx.service
```

## daemon-reload

```bash
# Recargar configuración de unidades:
sudo systemctl daemon-reload
# Re-escanea /usr/lib/, /etc/, /run/
# Actualiza el estado de systemd en memoria
# NO reinicia servicios activos

# Cuándo es necesario:
# - Después de crear/editar archivos .service en /etc/systemd/system/
# - Después de modificar drop-ins manualmente
# systemctl edit lo hace automáticamente
```

```bash
# Si olvidas daemon-reload, systemctl te avisa:
# Warning: nginx.service changed on disk. Run 'systemctl daemon-reload'.
```

## kill — Enviar señales

```bash
# Enviar señal a TODOS los procesos del cgroup del servicio:
sudo systemctl kill nginx.service
# Envía SIGTERM a todos los procesos del cgroup

# Señal específica:
sudo systemctl kill -s SIGHUP nginx.service    # reload config
sudo systemctl kill -s SIGKILL nginx.service   # forzar kill
sudo systemctl kill -s SIGUSR1 nginx.service  # señal custom

# --kill-who: a quién específicamente
--kill-who=main    # solo el PID principal
--kill-who=control # procesos de control (ExecStartPre, etc.)
--kill-who=all     # todos (default)
```

```
kill vs stop:
  stop     → Ejecuta ExecStop → SIGTERM al proceso → espera TimeoutStopSec
  kill     → Envía señal DIRECTA a los procesos (no pasa por ExecStop)
  kill     → Útil cuando un servicio no responde a stop normal
```

## systemctl isolate — Cambiar target

```bash
# Cambiar a otro target (detener todos los servicios no requeridos):
sudo systemctl isolate multi-user.target
sudo systemctl isolate graphical.target

# isolate significa: detener todos los servicios que no sean
# dependencies del target elegido
```

## systemctl reboot/poweroff

```bash
# Apagar (poweroff.target):
sudo systemctl poweroff
# Equivalente: shutdown -h now

# Reiniciar (reboot.target):
sudo systemctl reboot
# Equivalente: shutdown -r now | reboot

# halt (solo detiene, no apaga hardware):
sudo systemctl halt

# suspend:
sudo systemctl suspend

# hibernate:
sudo systemctl hibernate

# hybrid-sleep:
sudo systemctl hybrid-sleep
```

## show — Propiedades útiles para scripts

```bash
# Estado:
systemctl show nginx --property=ActiveState
systemctl show nginx --property=SubState

# PID:
systemctl show nginx --property=MainPID
systemctl show nginx --property=ControlPID

# Resultado de última ejecución:
systemctl show nginx --property=Result
# exit-code | signal | timeout | start-limit-hit | resources

# ExecStart y configuración:
systemctl show nginx --property=ExecStart
systemctl show nginx --property=ExecReload

# Recursos:
systemctl show nginx --property=MemoryCurrent,CPUUsageNSec

# Tiempo:
systemctl show nginx --property=ActiveEnterTimestamp
systemctl show nginx --property=InactiveExitTimestamp
```

## list-dependencies — Ver dependencias

```bash
# Ver qué unidades necesita nginx:
systemctl list-dependencies nginx.service

# Ver qué unidades dependen de nginx (reverse):
systemctl list-dependencies --reverse nginx.service

# Ver el árbol completo de un target:
systemctl list-dependencies multi-user.target
```

## Comandos útiles para scripts

```bash
# Silent checks (exit code 0 = sí):
systemctl is-active --quiet nginx
systemctl is-enabled --quiet nginx
systemctl is-failed --quiet nginx

# Esperar a que un servicio arranque:
systemctl start myapp.service
systemctl is-active --quiet myapp.service

# Con timeout:
systemctl start myapp.service
sleep 5
if ! systemctl is-active --quiet myapp.service; then
    echo "myapp no arrancó"
fi
```

---

## Ejercicios

### Ejercicio 1 — Operaciones básicas de servicio

```bash
# 1. Ver estado de un servicio activo
systemctl status sshd -l --no-pager

# 2. Identificar: PID, CGroup, Tasks, Memory

# 3. Ver propiedades
systemctl show sshd --property=MainPID,ActiveState,SubState,Type

# 4. is-active y is-enabled
systemctl is-active sshd
systemctl is-enabled sshd
echo $?   # verificar exit code

# 5. start y stop de un servicio no crítico (ej: cron)
sudo systemctl stop cron
systemctl is-active cron
sudo systemctl start cron
systemctl is-active cron
```

### Ejercicio 2 — Enable, disable, mask

```bash
# 1. Ver estado de enable de un servicio
systemctl is-enabled nginx

# 2. Disable y verificar
sudo systemctl disable nginx
systemctl is-enabled nginx
ls /etc/systemd/system/multi-user.target.wants/ | grep nginx

# 3. Enable
sudo systemctl enable nginx
systemctl is-enabled nginx

# 4. Mask (prueba en un servicio que puedas revertir)
# No hacer en producción — usa un VM o contenedor
# sudo systemctl mask cron
# systemctl is-enabled cron  # masked
# sudo systemctl start cron   # fallará
# sudo systemctl unmask cron
```

### Ejercicio 3 — Listar unidades

```bash
# 1. Contar servicios por estado
echo "Active services:"
systemctl list-units --type=service --state=active --no-legend | wc -l

echo "Failed services:"
systemctl list-units --type=service --state=failed --no-legend | wc -l

echo "Enabled services:"
systemctl list-unit-files --type=service --state=enabled --no-legend | wc -l

# 2. Ver los primeros 10 servicios activos
systemctl list-units --type=service --state=active --no-legend --plain | head -10

# 3. Ver timers activos
systemctl list-units --type=timer --state=active --no-legend --plain
```

### Ejercicio 4 — Script de verificación de servicios

```bash
#!/usr/bin/env bash
# Script que verifica el estado de una lista de servicios críticos

SERVICES=(
    "sshd"
    "cron"
    "systemd-logind"
    "systemd-journald"
)

all_ok=true

for svc in "${SERVICES[@]}"; do
    printf "%-25s " "$svc"
    
    if systemctl is-active --quiet "$svc" 2>/dev/null; then
        printf "✅ ACTIVE   "
    else
        printf "❌ INACTIVE "
        all_ok=false
    fi
    
    if systemctl is-enabled --quiet "$svc" 2>/dev/null; then
        printf "enabled\n"
    else
        printf "disabled\n"
    fi
done

if $all_ok; then
    echo "All critical services are active"
    exit 0
else
    echo "Some services are not active!"
    exit 1
fi
```

### Ejercicio 5 — Properties y scripting

```bash
# 1. Extraer propiedades
PID=$(systemctl show nginx --property=MainPID --value)
STATE=$(systemctl show nginx --property=ActiveState --value)
TYPE=$(systemctl show nginx --property=Type --value)

echo "nginx: PID=$PID, State=$STATE, Type=$TYPE"

# 2. Esperar a que un servicio arranque
sudo systemctl start myapp
for i in {1..10}; do
    if systemctl is-active --quiet myapp; then
        echo "myapp started successfully"
        break
    fi
    sleep 1
done

# 3. Ver todas las propiedades de un servicio
systemctl show sshd | grep -E "PID|State|Main|Exec|Result"
```

### Ejercicio 6 — kill vs stop

```bash
# 1. Ver ExecStop de un servicio
systemctl show nginx --property=ExecStop

# 2. El proceso de stop:
# stop → Ejecuta ExecStop → Envía señal al proceso
# kill → Envía señal DIRECTA (no pasa por ExecStop)

# 3. Si un servicio no responde a stop, usar kill
# sudo systemctl stop nginx   # si cuelga
# sudo systemctl kill -s SIGKILL nginx   # forzar

# 4. Ver los procesos de un servicio antes de matar
systemctl status nginx
# Ver los PIDs en CGroup

# 5. Enviar SIGHUP para reload de config
sudo systemctl kill -s SIGHUP nginx.service
journalctl -u nginx --since "1 min ago" | tail -5
```

### Ejercicio 7 — list-dependencies

```bash
# 1. Ver dependencias de un target
systemctl list-dependencies multi-user.target | head -30

# 2. Ver qué necesita sshd
systemctl list-dependencies sshd.service | head -20

# 3. Dependencias inversas (qué depende de sshd)
systemctl list-dependencies --reverse sshd.service

# 4. Visualizar como árbol
systemctl list-dependencies --plain multi-user.target | head -30
```
