# T03 — Estados de unidad

## Las dos dimensiones de estado

Cada unidad systemd tiene **dos estados independientes** que deben entenderse por separado:

```
┌─────────────────────────────────────────────────────────────┐
│                                                              │
│  LOAD STATE — ¿Se pudo leer el archivo de unidad?           │
│                                                              │
│  LOAD STATE = loaded  → systemd parseó el archivo OK        │
│  LOAD STATE = error   → el archivo tiene errores            │
│  LOAD STATE = masked  → está bloqueado con symlink /dev/null│
│                                                              │
├─────────────────────────────────────────────────────────────┤
│                                                              │
│  ACTIVE STATE — ¿La unidad está haciendo su trabajo?      │
│                                                              │
│  ACTIVE STATE = active  → corriendo/montado/escuchando    │
│  ACTIVE STATE = failed  → falló con error                 │
│  ACTIVE STATE = inactive → no está activo                 │
│                                                              │
├─────────────────────────────────────────────────────────────┤
│                                                              │
│  UNIT FILE STATE (separate) — ¿Arrancará al boot?          │
│                                                              │
│  enabled  → symlink existe en target.wants/               │
│  disabled → no hay symlink                                 │
│  masked   → symlink a /dev/null (no puede habilitarse)  │
│  static   → no tiene [Install], no se puede enable/disable │
│                                                              │
└─────────────────────────────────────────────────────────────┘
```

```bash
# Todo junto en systemctl status:
systemctl status nginx.service
#    Loaded: loaded (/usr/lib/systemd/system/nginx.service; enabled; preset: enabled)
#                 ▲                                              ▲        ▲
#                 │                                              │        │
#            LoadState                                     UnitFileState  Preset
#
#    Active: active (running) since Mon 2024-03-17 10:00:00 UTC; 5h ago
#                 ▲               ▲
#                 │               │
#            ActiveState      SubState
```

```bash
# Estados en formato máquina:
systemctl show nginx.service --property=LoadState,ActiveState,SubState,UnitFileState
# LoadState=loaded
# ActiveState=active
# SubState=running
# UnitFileState=enabled
```

## Load State — ¿Se leyó el archivo?

| LoadState | Significado | ¿Se puede iniciar? |
|---|---|---|
| `loaded` | Parseado OK, en memoria | Sí (si no está masked) |
| `not-found` | No existe archivo | ❌ No |
| `bad-setting` | Parseado pero tiene error de configuración | ❌ No |
| `error` | Error al parsear | ❌ No |
| `masked` | Enmascarado con symlink a /dev/null | ❌ No |
| `merged` | Fusionado con otra unidad | Depende de la fusión |

```bash
# Verificar load state:
systemctl show nginx.service --property=LoadState
# loaded

systemctl show noexiste.service --property=LoadState
# not-found
```

## Active State — ¿Está funcionando?

| ActiveState | Significado |
|---|---|
| `active` | La unidad está funcionando |
| `inactive` | No está activa |
| `activating` | En proceso de activar |
| `deactivating` | En proceso de desactivar |
| `failed` | Terminó con error |
| `reloading` | Recargando configuración |
| `maintenance` | En modo mantenimiento (automounts) |

### Sub-states según tipo de unidad

```bash
# Services:
systemctl status nginx.service
# Active: active (running)        ← subprocess main running
# Active: active (exited)         ← Type=oneshot completed successfully
# Active: active (waiting)         ← Type=idle, waiting for activity
# Active: inactive (dead)          ← not running
# Active: failed                   ← exited with error

# Sockets:
# Active: active (listening)      ← accepting connections
# Active: active (running)        ← currently handling a connection

# Mounts:
# Active: active (mounted)        ← mounted
# Active: inactive (dead)         ← not mounted

# Timers:
# Active: active (waiting)        ← waiting for next elapse
# Active: active (elapsed)        ← fired, no next scheduled time

# Paths:
# Active: active (waiting)        ← monitoring for changes
```

## Unit File State — ¿Arrancará al boot?

| UnitFileState | ¿Arranca al boot? | Notes |
|---|---|---|
| `enabled` | ✅ Sí | Symlink en target.wants/ |
| `disabled` | ❌ No | Sin symlink |
| `masked` | ❌❌ No | Symlink a /dev/null |
| `static` | N/A | No tiene [Install], se activa como dependencia |
| `indirect` | Indirectamente | Habilitado vía Also= en otra unidad |
| `generated` | Dinámicamente | Creado por un generator |
| `transient` | Runtime | Creado durante esta sesión |
| `enabled-runtime` | Solo esta sesión | No persiste al reiniciar |

```bash
# Comandos rápidos (exit code 0 = sí, != 0 = no):
systemctl is-active nginx.service     # ¿está corriendo?
systemctl is-enabled nginx.service    # ¿arranca al boot?
systemctl is-failed nginx.service     # ¿está en failed?

# Scripts:
if systemctl is-active --quiet nginx; then
    echo "nginx está corriendo"
fi
```

### Servicios static

Los servicios "static" no tienen sección `[Install]`. No se pueden habilitar/deshabilitar con `systemctl enable/disable`. Siempre se activan como dependencia de otras unidades:

```bash
# Muchos servicios core son static:
systemctl is-enabled systemd-journald.service
# static

systemctl is-enabled dbus.service
# static

systemctl is-enabled systemd-logind.service
# static

# ¿Por qué? Son必需品 del sistema
# No tiene sentido "deshabilitarlos" — siempre los necesita algo
```

```bash
# Verificar que no tiene [Install]:
systemctl cat systemd-journald.service | grep "\[Install\]"
# (sin output = no tiene sección [Install])
```

### Presets

Los presets definen el estado por defecto que un paquete tendrá al instalarse:

```bash
# Preset files:
ls /usr/lib/systemd/system-preset/
# 90-systemd.preset

cat /usr/lib/systemd/system-preset/90-systemd.preset
# enable sshd.service
# disable debug-shell.service
# enable systemd-networkd.service
# disable systemd-pstore.service

# Aplicar todos los presets:
sudo systemctl preset-all

# Preset-mode:
sudo systemctl preset-all --preset-mode=full     # enable/disable según preset
sudo systemctl preset-all --preset-mode=enable-only  # solo enable
sudo systemctl preset-all --preset-mode=disable-only # solo disable
```

## Masked — Bloqueo total

Una unidad **masked** tiene un symlink a `/dev/null`. Ni systemd ni un administrador pueden iniciarla:

```
┌─────────────────────────────────────────────────────────────┐
│  mask vs disable                                             │
│                                                              │
│  disable:                                                    │
│  /etc/systemd/system/nginx.service → (sin symlink)         │
│  ✗ No arranca al boot                                       │
│  ✓ systemctl start nginx → funciona                         │
│                                                              │
│  mask:                                                      │
│  /etc/systemd/system/nginx.service → /dev/null             │
│  ✗ No arranca al boot                                       │
│  ✗ systemctl start nginx → ERROR (Unit is masked)          │
│  ✗ No puede ser iniciada como dependencia                  │
└─────────────────────────────────────────────────────────────┘
```

```bash
# Enmascarar:
sudo systemctl mask nginx.service
# Created symlink /etc/systemd/system/nginx.service → /dev/null.

# Intentar iniciar:
sudo systemctl start nginx.service
# Failed to start nginx.service: Unit nginx.service is masked.

# Ver estado:
systemctl status nginx.service
#    Loaded: masked (Reason: Unit nginx.service is masked.)
#    Active: inactive (dead)

systemctl is-enabled nginx.service
# masked

# Desenmascarar:
sudo systemctl unmask nginx.service
# Removed /etc/systemd/system/nginx.service.
```

```bash
# Cuándo usar mask:
# 1. Reemplazar un servicio del sistema por otro:
sudo systemctl mask iptables.service    # usar firewalld en su lugar
sudo systemctl mask postfix.service     # usar sendmail en su lugar

# 2. Durante migración de servicios:
sudo systemctl mask nginx.service
# Ahora instalar nginx compilado a mano
# o usar docker nginx

# 3. Prevenir que un servicio危险的 arranque:
sudo systemctl mask debug-shell.service   # seguridad

# 4. Cuando dos servicios conflictúan:
# Ej: docker y podman ambos usan /var/run/docker.sock
# mask uno para usar el otro
```

## Failed State — Cuando algo sale mal

Cuando un servicio falla (exit code != 0, señal, timeout), entra en `failed`:

```bash
# Estado failed:
systemctl status myapp.service
# ○ myapp.service
#    Loaded: loaded (...); enabled)
#    Active: failed (Result: exit-code) since Mon 2024-03-17 12:00; 5min ago
#    Process: 12345 ExecStart=/usr/local/bin/myapp (code=exited, status=1/FAILURE)
```

```bash
# Reseteart el failed state (NO arregla el problema, solo limpia el estado):
sudo systemctl reset-failed myapp.service
# Vuelve a: Active: inactive (dead)

# Resetear TODAS las unidades fallidas:
sudo systemctl reset-failed

# Luego intentar arrancar de nuevo:
sudo systemctl start myapp.service
```

### Propiedades de diagnóstico de failure

```bash
# Result: la causa del fallo
systemctl show myapp.service --property=Result
# Result=exit-code      → ExecStart salió con código != 0
# Result=signal        → Recibió una señal (SIGSEGV, etc.)
# Result=timeout        → TimeoutStartSec expiró
# Result=core-dump     → Generó core dump
# Result=watchdog       → No recibió ping de watchdog
# Result=start-limit-hit → StartLimitBurst superado
# Result=exit-code-or-signaled → kombinasi
# Result=resources     → No se pudieron asignar recursos (memoria, fds, etc.)

# Exit code:
systemctl show myapp.service --property=ExecMainExitCode
# ExecMainExitCode=1

# Señal (si fue killed by signal):
systemctl show myapp.service --property=ExecMainExitSignal
# ExecMainExitSignal=SIGSEGV

# Log con más detalle:
systemctl show myapp.service --property=ExecStartMainExitCode,ExecStartMainExitStatus
```

### Diagnóstico completo de un fallo

```bash
# 1. Ver status con logs recientes
systemctl status myapp.service -l --no-pager

# 2. Ver logs del journal
journalctl -u myapp.service --no-pager -n 50
journalctl -u myapp.service -b  # desde el último boot
journalctl -u myapp.service --since "1 hour ago"

# 3. Ver logs del kernel si fue OOM kill:
dmesg | grep -i "killed process"
journalctl -k --since "1 hour ago" | grep -i oom

# 4. Ver límites de recursos:
systemctl show myapp.service --property=MemoryMax,LimitNOFILE

# 5. Ver dependencias fallidas:
systemctl list-dependencies myapp.service --failed
```

## Lista completa de comandos de estado

```bash
# is-* comandos (silent, exit code):
systemctl is-active nginx       # 0 si active
systemctl is-enabled nginx      # 0 si enabled
systemctl is-failed nginx       # 0 si failed

# Listar por estado:
systemctl list-units --state=failed
systemctl list-units --state=active --type=service
systemctl list-unit-files --state=enabled
systemctl list-unit-files --state=masked

# Resumen:
systemctl --failed              # unidades fallidas
```

## Diagrama de transiciones de estado

```
                    ENABLE/DISABLE
                    (solo afecta UnitFileState)

        ┌───────────────────────────────────────────────┐
        │                                               │
        │         UnitFileState: disabled               │
        │         (no hay symlink en target.wants/)   │
        │                                               │
        └───────────────────┬───────────────────────────┘
                            │ enable
                            ▼
        ┌───────────────────────────────────────────────┐
        │         UnitFileState: enabled               │
        │         (symlink en target.wants/)           │
        │                                               │
        │         ┌───────────────────────────────────┐ │
        │         │         UnitFileState: static   │ │
        │         │         (no [Install] section)   │ │
        │         │         Se activa por dep       │ │
        │         └───────────────────────────────────┘ │
        └───────────────────┬───────────────────────────┘
                            │ start
                            ▼
                    ┌───────────────────┐
                    │    ACTIVE         │◀──────────────────┐
                    │   (running)        │                   │
                    └─────────┬─────────┘                   │
                              │                             │
                   stop       │       crash/error           │ reset-failed
                              ▼                             │
                    ┌───────────────────┐    ┌──────────────┴───────┐
                    │    INACTIVE       │    │       FAILED           │
                    │    (dead)        │    │   (exit-code != 0)     │
                    └───────────────────┘    └───────────────────────┘
                              ▲
                              │
                    ┌─────────┴─────────┐
                    │    ACTIVE          │
                    │  (activating)     │
                    └───────────────────┘
```

---

## Ejercicios

### Ejercicio 1 — Inspeccionar todos los estados

```bash
# 1. Elegir un servicio activo y ver sus estados
systemctl status sshd.service

# 2. Extraer las propiedades clave
systemctl show sshd.service --property=LoadState,ActiveState,SubState,UnitFileState

# 3. Comparar con un servicio inactivo
systemctl status cron.service
systemctl show cron.service --property=LoadState,ActiveState,SubState,UnitFileState

# 4. Listar servicios failed
systemctl --failed --no-pager

# 5. Listar servicios masked
systemctl list-unit-files --state=masked --type=service --no-pager
```

### Ejercicio 2 — is-active vs is-enabled vs is-failed

```bash
# Script para verificar servicios críticos:

SERVICES=("sshd" "cron" "systemd-journald" "systemd-logind")

for svc in "${SERVICES[@]}"; do
    echo "=== $svc ==="
    
    # Active state
    if systemctl is-active --quiet "$svc"; then
        echo "  Active: YES"
    else
        echo "  Active: NO"
    fi
    
    # Enabled state
    if systemctl is-enabled --quiet "$svc"; then
        echo "  Enabled: YES"
    else
        echo "  Enabled: NO"
    fi
    
    # Failed state
    if systemctl is-failed --quiet "$svc"; then
        echo "  FAILED: YES ⚠️"
    else
        echo "  Failed: NO"
    fi
    
    echo
done
```

### Ejercicio 3 — Mask y unmask

```bash
# 1. Verificar estado actual de un servicio no crítico
systemctl status cron.service
systemctl is-enabled cron.service

# 2. Disable y verificar
sudo systemctl disable cron.service
systemctl is-enabled cron.service
ls /etc/systemd/system/timers.target.wants/ | grep cron

# 3. Enable de nuevo
sudo systemctl enable cron.service

# 4. Mask (prueba en un servicio que puedas revertir)
sudo systemctl mask cron.service
systemctl is-enabled cron.service
sudo systemctl start cron.service  # fallará

# 5. Ver el symlink
ls -la /etc/systemd/system/cron.service

# 6. Unmask y verificar
sudo systemctl unmask cron.service
systemctl is-enabled cron.service
sudo systemctl start cron.service  # debería funcionar
```

### Ejercicio 4 — Diagnóstico de failure

```bash
# 1. Encontrar un servicio en failed (o provocar uno intencionalmente)
systemctl --failed --type=service

# 2. Ver el diagnóstico
systemctl status <failed-service>

# 3. Ver Result y ExitCode
systemctl show <failed-service> --property=Result,ExecMainExitCode

# 4. Ver logs
journalctl -u <failed-service> --no-pager -n 30

# 5. Resetear y ver que vuelve a inactive
sudo systemctl reset-failed <failed-service>
systemctl status <failed-service>
```

### Ejercicio 5 — Static services

```bash
# 1. Listar todos los servicios static
systemctl list-unit-files --type=service --state=static --no-pager | head -20

# 2. Elegir uno y verificar que no tiene [Install]
systemctl cat systemd-journald.service | grep -A5 "\[Install\]"

# 3. ¿Por qué es static?
# systemd-journald es必需品 — siempre lo necesita algo
systemctl list-dependencies --reverse systemd-journald.service | head -20
```

### Ejercicio 6 — Scripts con exit codes

```bash
# Script que usa los exit codes de systemctl para automatización:

#!/bin/bash
# Verifica y reporta estado de un servicio

check_service() {
    local svc=$1
    
    systemctl is-active "$svc" >/dev/null 2>&1
    case $? in
        0)  echo "$svc: ACTIVE" ;;
        1)  echo "$svc: INACTIVE" ;;
        *)  echo "$svc: UNKNOWN (error)" ;;
    esac
    
    systemctl is-enabled "$svc" >/dev/null 2>&1
    case $? in
        0)  echo "  $svc: ENABLED (boot)" ;;
        1)  echo "  $svc: DISABLED (no boot)" ;;
        *)  echo "  $svc: UNKNOWN" ;;
    esac
}

check_service sshd
check_service nginx
check_service cron
```
