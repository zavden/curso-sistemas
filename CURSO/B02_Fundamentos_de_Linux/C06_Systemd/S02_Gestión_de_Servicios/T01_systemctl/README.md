# T01 — systemctl

## Operaciones básicas

```bash
# Arrancar un servicio:
sudo systemctl start nginx.service

# Detener un servicio:
sudo systemctl stop nginx.service

# Reiniciar (stop + start):
sudo systemctl restart nginx.service

# Recargar configuración SIN reiniciar el proceso:
sudo systemctl reload nginx.service
# Solo funciona si el servicio soporta reload (ExecReload definido)
# Envía SIGHUP al proceso principal por defecto

# Reiniciar si está corriendo, no hacer nada si no:
sudo systemctl try-restart nginx.service

# Reload si es posible, restart si no:
sudo systemctl reload-or-restart nginx.service
# Preferido en scripts — es seguro sin saber si el servicio soporta reload

# El sufijo .service es opcional:
sudo systemctl start nginx     # equivalente a nginx.service
# PERO es buena práctica incluirlo para evitar ambigüedad con targets
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
    Process: 1234 ExecStartPre=/usr/sbin/nginx -t -q -g daemon on; master_process on; (code=exited, status=0/SUCCESS)
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

```bash
# Desglose de cada línea:
# ● / ○ / × — indicador visual:
#   ● verde = active
#   ○ blanco = inactive
#   × rojo = failed

# Loaded: loaded (RUTA; ENABLE_STATE; PRESET)
#   RUTA: dónde está el archivo de unidad
#   ENABLE_STATE: enabled/disabled/masked/static
#   PRESET: si el preset del sistema lo habilita o no

# Active: ACTIVE_STATE (SUB_STATE) since TIMESTAMP; DURACIÓN ago
#   El SUB_STATE depende del tipo (running, exited, listening, etc.)

# Main PID: el proceso principal que systemd monitorea

# Tasks: procesos/threads del cgroup (y el límite)
# Memory: memoria usada por el cgroup
# CPU: tiempo de CPU acumulado

# CGroup: jerarquía del cgroup con los procesos
# Las últimas líneas son logs recientes del journal
```

### Opciones de status

```bash
# Sin pager (para scripts o terminal sin less):
systemctl status nginx --no-pager

# Más líneas de log:
systemctl status nginx -n 50        # últimas 50 líneas
systemctl status nginx -n 0         # sin logs

# Líneas completas (sin truncar):
systemctl status nginx -l

# Combinado:
systemctl status nginx -l --no-pager -n 30
```

## enable / disable — Arranque al boot

```bash
# Habilitar (arranca al boot):
sudo systemctl enable nginx.service
# Created symlink /etc/systemd/system/multi-user.target.wants/nginx.service
#  → /lib/systemd/system/nginx.service

# Deshabilitar (no arranca al boot):
sudo systemctl disable nginx.service
# Removed /etc/systemd/system/multi-user.target.wants/nginx.service

# Habilitar Y arrancar inmediatamente:
sudo systemctl enable --now nginx.service
# Equivale a: enable + start

# Deshabilitar Y detener inmediatamente:
sudo systemctl disable --now nginx.service
# Equivale a: disable + stop

# Verificar si está habilitado:
systemctl is-enabled nginx.service
# enabled / disabled / static / masked
```

```bash
# Qué hace enable internamente:
# Lee la sección [Install] de la unidad:
#   WantedBy=multi-user.target
# Crea un symlink en:
#   /etc/systemd/system/multi-user.target.wants/nginx.service

# Al arrancar multi-user.target, systemd activa todas las unidades
# en multi-user.target.wants/

# Si no hay sección [Install], la unidad es "static" y no se puede enable:
systemctl enable systemd-journald.service
# The unit files have no installation config (WantedBy, RequiredBy, etc.)
```

## mask / unmask — Bloqueo total

```bash
# Enmascarar (bloquear completamente):
sudo systemctl mask nginx.service
# Created symlink /etc/systemd/system/nginx.service → /dev/null

# Intentar arrancar:
sudo systemctl start nginx.service
# Failed to start nginx.service: Unit nginx.service is masked.

# Desenmascarar:
sudo systemctl unmask nginx.service
# Removed /etc/systemd/system/nginx.service

# mask vs disable:
# disable: no arranca al boot, PERO se puede start manualmente
# mask: no se puede arrancar NUNCA (ni manual, ni como dependencia)
```

## Listar unidades

```bash
# Todas las unidades cargadas y activas:
systemctl list-units

# Filtrar por tipo:
systemctl list-units --type=service
systemctl list-units --type=timer
systemctl list-units --type=socket

# Filtrar por estado:
systemctl list-units --state=active
systemctl list-units --state=failed
systemctl list-units --state=inactive

# Todas las unidades instaladas (incluso las no cargadas):
systemctl list-unit-files
systemctl list-unit-files --type=service

# Solo las fallidas:
systemctl --failed

# Formato completo sin truncar:
systemctl list-units --type=service --no-pager --full

# Con all (incluye inactivas cargadas):
systemctl list-units --all --type=service
```

## show — Propiedades de una unidad

```bash
# Ver TODAS las propiedades:
systemctl show nginx.service
# Devuelve decenas de propiedades en formato key=value

# Propiedades específicas:
systemctl show nginx.service --property=MainPID
# MainPID=1235

systemctl show nginx.service --property=ActiveState,SubState,MainPID,Type
# ActiveState=active
# SubState=running
# MainPID=1235
# Type=forking

# Útil en scripts:
PID=$(systemctl show nginx --property=MainPID --value)
# --value muestra solo el valor, sin el "MainPID="

STATE=$(systemctl show nginx --property=ActiveState --value)
if [[ "$STATE" != "active" ]]; then
    echo "nginx no está activo"
fi
```

## cat — Ver el archivo de unidad

```bash
# Muestra el archivo de unidad con overrides aplicados:
systemctl cat nginx.service
# # /lib/systemd/system/nginx.service
# [Unit]
# Description=A high performance web server...
# ...
#
# # /etc/systemd/system/nginx.service.d/override.conf
# [Service]
# LimitNOFILE=65535

# Muestra el origen de cada parte (archivo original + drop-ins)
```

## edit — Modificar unidades

```bash
# Crear un drop-in override:
sudo systemctl edit nginx.service
# Abre el editor con un archivo vacío
# Se guarda en /etc/systemd/system/nginx.service.d/override.conf
# Ejecuta daemon-reload automáticamente al guardar

# Override completo (copia el archivo entero):
sudo systemctl edit --full nginx.service
# Copia la unidad a /etc/systemd/system/ y abre el editor

# Especificar el editor:
sudo EDITOR=vim systemctl edit nginx.service
sudo SYSTEMD_EDITOR=vim systemctl edit nginx.service
```

## daemon-reload

```bash
# Recargar archivos de unidad después de modificarlos manualmente:
sudo systemctl daemon-reload
# NO reinicia servicios, solo re-lee los archivos

# ¿Cuándo hacer daemon-reload?
# - Después de crear/editar archivos en /etc/systemd/system/
# - Después de modificar drop-ins manualmente
# - systemctl edit lo hace automáticamente
# - Si olvidas, systemctl te advierte:
#   Warning: nginx.service changed on disk. Run 'systemctl daemon-reload'
```

## Operaciones en scripts

```bash
# Verificaciones silenciosas (solo exit code):
systemctl is-active --quiet nginx    # 0 si activo
systemctl is-enabled --quiet nginx   # 0 si habilitado
systemctl is-failed --quiet nginx    # 0 si fallido

# Patrón en scripts:
if ! systemctl is-active --quiet nginx; then
    echo "nginx no está activo, arrancando..."
    sudo systemctl start nginx
fi

# Esperar a que un servicio arranque:
sudo systemctl start myapp.service
systemctl is-active --quiet myapp.service
# Si el servicio es Type=notify, start espera a que notifique
# Si es Type=simple, start retorna inmediatamente

# Listar en formato máquina:
systemctl list-units --type=service --state=active --no-legend --plain
# --no-legend: sin headers
# --plain: sin formato de árbol
```

## kill — Enviar señales

```bash
# Enviar una señal a todos los procesos del servicio (no solo al PID principal):
sudo systemctl kill nginx.service
# Envía SIGTERM a TODOS los procesos del cgroup

# Señal específica:
sudo systemctl kill -s SIGHUP nginx.service    # reload
sudo systemctl kill -s SIGKILL nginx.service   # kill forzado

# kill vs stop:
# stop: ejecuta ExecStop, respeta TimeoutStopSec, procedimiento grácil
# kill: envía la señal directamente a los procesos

# --kill-who: a quién enviar la señal
sudo systemctl kill --kill-who=main nginx.service    # solo PID principal
sudo systemctl kill --kill-who=control nginx.service # solo procesos de control
sudo systemctl kill --kill-who=all nginx.service     # todos (default)
```

## Operaciones del sistema

```bash
# Apagar:
sudo systemctl poweroff

# Reiniciar:
sudo systemctl reboot

# Suspender:
sudo systemctl suspend

# Hibernar:
sudo systemctl hibernate

# Suspend-then-hibernate:
sudo systemctl suspend-then-hibernate

# Equivalencias con comandos clásicos:
# systemctl poweroff  ≈  shutdown -h now
# systemctl reboot    ≈  shutdown -r now  ≈  reboot
# systemctl halt      ≈  halt
```

---

## Ejercicios

### Ejercicio 1 — Operaciones básicas

```bash
# 1. Ver el estado de sshd:
systemctl status sshd -l --no-pager

# 2. ¿Está habilitado al boot?
systemctl is-enabled sshd

# 3. ¿Cuál es su PID principal?
systemctl show sshd --property=MainPID --value
```

### Ejercicio 2 — Listar y filtrar

```bash
# 1. ¿Cuántos servicios están activos?
systemctl list-units --type=service --state=active --no-pager --no-legend | wc -l

# 2. ¿Hay servicios en estado failed?
systemctl --failed --no-pager

# 3. ¿Cuántos servicios están habilitados al boot?
systemctl list-unit-files --type=service --state=enabled --no-pager --no-legend | wc -l
```

### Ejercicio 3 — Script de verificación

```bash
#!/usr/bin/env bash
set -euo pipefail

SERVICES=(sshd cron)

for svc in "${SERVICES[@]}"; do
    printf "%-20s " "$svc"
    if systemctl is-active --quiet "$svc" 2>/dev/null; then
        printf "ACTIVO  "
    else
        printf "PARADO  "
    fi
    if systemctl is-enabled --quiet "$svc" 2>/dev/null; then
        echo "enabled"
    else
        echo "disabled"
    fi
done
```
