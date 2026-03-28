# T03 — Estados de unidad

## Dos dimensiones de estado

Cada unidad tiene dos estados independientes:

1. **Load state** — si el archivo de unidad fue leído y parseado
2. **Active state** — si la unidad está corriendo/activa

```bash
systemctl status nginx.service
#    Loaded: loaded (/usr/lib/systemd/system/nginx.service; enabled; preset: enabled)
#    Active: active (running) since Mon 2024-03-17 10:00:00 UTC; 5h ago
#            ▲                        ▲
#            │                        │
#            active state             sub-state (específico del tipo)
```

```bash
# Ver estados en formato máquina:
systemctl show nginx.service --property=LoadState,ActiveState,SubState
# LoadState=loaded
# ActiveState=active
# SubState=running
```

## Load states

| Estado | Significado |
|---|---|
| `loaded` | Archivo parseado correctamente, unidad en memoria |
| `not-found` | No se encontró el archivo de unidad |
| `bad-setting` | Archivo parseado pero tiene errores de configuración |
| `error` | Error al parsear el archivo |
| `masked` | Unidad enmascarada (deshabilitada permanentemente) |
| `merged` | Unidad fusionada con otra (raro) |

```bash
# Verificar load state:
systemctl show nginx.service --property=LoadState
# loaded

systemctl show servicio-inexistente.service --property=LoadState
# not-found
```

## Active states

| Estado | Significado |
|---|---|
| `active` | La unidad está corriendo/montada/escuchando |
| `inactive` | La unidad no está activa |
| `activating` | La unidad está en proceso de arrancar |
| `deactivating` | La unidad está en proceso de detenerse |
| `failed` | La unidad falló (el proceso terminó con error) |
| `reloading` | La unidad está recargando su configuración |
| `maintenance` | La unidad está en mantenimiento (solo automounts) |

### Sub-states

El sub-state da más detalle según el tipo de unidad:

```bash
# Services:
active (running)       # proceso principal corriendo
active (exited)        # ExecStart terminó exitosamente (Type=oneshot)
active (waiting)       # esperando (ej: Type=idle)
inactive (dead)        # no corriendo

# Sockets:
active (listening)     # escuchando conexiones
active (running)       # conexión activa

# Mounts:
active (mounted)       # montado
inactive (dead)        # no montado

# Timers:
active (waiting)       # esperando el próximo disparo
active (elapsed)       # ya se disparó, sin próximo programado
```

## Enabled vs disabled (UnitFileState)

Independiente del active state, una unidad puede estar **habilitada** o
**deshabilitada** para arrancar en el boot:

```bash
# enabled — arranca automáticamente al boot (symlink en target.wants/)
systemctl enable nginx.service

# disabled — NO arranca al boot
systemctl disable nginx.service

# Ver el estado:
systemctl is-enabled nginx.service
# enabled, disabled, masked, static, etc.
```

| UnitFileState | Significado |
|---|---|
| `enabled` | Habilitada para arrancar al boot |
| `disabled` | No arranca al boot |
| `static` | No tiene sección [Install] — no se puede enable/disable directamente, solo se activa como dependencia de otra unidad |
| `masked` | Enmascarada — no puede arrancar bajo ninguna circunstancia |
| `indirect` | Habilitada indirectamente (a través de un Also= en otra unidad) |
| `generated` | Generada dinámicamente por un generator |
| `transient` | Creada en runtime (transient unit) |
| `enabled-runtime` | Habilitada solo para esta sesión (no persiste al reiniciar) |

```bash
# Muchos servicios son "static":
systemctl is-enabled systemd-journald.service
# static
# No tiene [Install] — siempre se activa como dependencia de otros

systemctl is-enabled dbus.service
# static
# dbus siempre es necesario, no tiene sentido disable

# Ver todas las unidades y su UnitFileState:
systemctl list-unit-files --type=service
# UNIT FILE                    STATE     PRESET
# nginx.service                enabled   enabled
# sshd.service                 enabled   enabled
# bluetooth.service            disabled  enabled
# debug-shell.service          static    -
# emergency.service            static    -
```

### Presets

Los presets definen el estado por defecto de enable/disable al instalar un
paquete:

```bash
# Ver presets:
systemctl preset-all --preset-mode=full    # aplicar todos los presets

# Los presets están definidos en:
ls /usr/lib/systemd/system-preset/
cat /usr/lib/systemd/system-preset/90-systemd.preset
# enable sshd.service
# disable debug-shell.service
# ...

# Diferencias entre distribuciones:
# Debian: pocos presets, enable/disable manual
# RHEL/Fedora: usa presets activamente
```

## Masked — Bloqueo total

Una unidad enmascarada no puede arrancar bajo ninguna circunstancia — ni
manualmente, ni como dependencia, ni al boot:

```bash
# Enmascarar:
sudo systemctl mask nginx.service
# Crea un symlink a /dev/null:
# /etc/systemd/system/nginx.service → /dev/null

# Intentar arrancar una unidad enmascarada:
sudo systemctl start nginx.service
# Failed to start nginx.service: Unit nginx.service is masked.

# Ver el estado:
systemctl status nginx.service
# Loaded: masked (Reason: Unit nginx.service is masked.)
# Active: inactive (dead)

systemctl is-enabled nginx.service
# masked

# Desenmascarar:
sudo systemctl unmask nginx.service
# Elimina el symlink a /dev/null
```

```bash
# Cuándo enmascarar:
# 1. Prevenir que un servicio arranque accidentalmente
sudo systemctl mask iptables.service    # si usas firewalld
sudo systemctl mask sendmail.service    # si usas postfix

# 2. Durante migración de servicios
sudo systemctl mask nginx.service       # migrar a un nginx compilado

# 3. Seguridad — desactivar servicios peligrosos
sudo systemctl mask debug-shell.service

# Diferencia con disable:
# disable: no arranca al boot, pero SÍ se puede iniciar manualmente
# mask: NO se puede iniciar bajo ninguna circunstancia
```

## Consultar estados

```bash
# Comandos rápidos de verificación (exit code 0 = sí, != 0 = no):
systemctl is-active nginx.service       # ¿está corriendo?
systemctl is-enabled nginx.service      # ¿arranca al boot?
systemctl is-failed nginx.service       # ¿está en estado failed?

# Útil en scripts:
if systemctl is-active --quiet nginx.service; then
    echo "nginx está corriendo"
fi

# Listar por estado:
systemctl list-units --state=failed
systemctl list-units --state=active --type=service
systemctl list-unit-files --state=enabled
systemctl list-unit-files --state=masked

# Listar unidades fallidas:
systemctl --failed
# Es equivalente a: systemctl list-units --state=failed
```

## Failed state y reseteo

Cuando un servicio falla (exit code != 0, señal, timeout), entra en estado
`failed`:

```bash
systemctl status myapp.service
# Active: failed (Result: exit-code) since ...
#   ...
# Process: 12345 ExecStart=... (code=exited, status=1/FAILURE)

# El estado failed persiste hasta que se resetea:
sudo systemctl reset-failed myapp.service
# Vuelve a inactive (dead)

# Resetear todas las unidades fallidas:
sudo systemctl reset-failed

# Intentar arrancar después del reseteo:
sudo systemctl start myapp.service
```

### Diagnóstico de fallo

```bash
# 1. Ver status detallado:
systemctl status myapp.service -l --no-pager

# 2. Ver los logs del servicio:
journalctl -u myapp.service --no-pager -n 50

# 3. Ver los logs desde el último intento de arranque:
journalctl -u myapp.service --since "5 min ago"

# 4. Ver la propiedad Result para saber cómo falló:
systemctl show myapp.service --property=Result
# Result=exit-code       → proceso salió con error
# Result=signal          → proceso recibió señal (crash)
# Result=timeout         → TimeoutStartSec/TimeoutStopSec expiró
# Result=core-dump       → proceso generó core dump
# Result=watchdog        → WatchdogSec expiró
# Result=start-limit-hit → demasiados reinicios (StartLimitBurst)
# Result=resources       → no se pudieron asignar recursos
```

## Diagrama de transiciones

```
                    ┌─────────────┐
          enable    │             │  disable
        ┌──────────▶│  Habilitada  │◀──────────┐
        │           │  (boot)     │            │
        │           └─────────────┘            │
        │                                      │
  ┌─────┴──────┐    start    ┌──────────┐    ┌─┴───────────┐
  │  inactive   │───────────▶│  active   │    │  disabled    │
  │  (dead)     │◀───────────│ (running) │    │  (no boot)  │
  └──────▲──────┘    stop    └─────┬─────┘    └─────────────┘
         │                         │
         │         crash/error     │
         │                    ┌────▼─────┐
         │    reset-failed    │  failed   │
         └────────────────────│ (error)   │
                              └──────────┘
```

---

## Ejercicios

### Ejercicio 1 — Inspeccionar estados

```bash
# 1. ¿Cuántos servicios están en estado failed?
systemctl --failed --no-pager

# 2. ¿Cuántos servicios están enabled?
systemctl list-unit-files --type=service --state=enabled --no-pager | tail -1

# 3. ¿Hay unidades masked?
systemctl list-unit-files --state=masked --no-pager
```

### Ejercicio 2 — Verificar en un script

```bash
# Script que verifica el estado de servicios críticos:
SERVICES=(sshd cron)

for svc in "${SERVICES[@]}"; do
    if systemctl is-active --quiet "$svc"; then
        echo "OK: $svc activo"
    else
        echo "WARN: $svc NO activo"
    fi

    if systemctl is-enabled --quiet "$svc"; then
        echo "  → habilitado al boot"
    else
        echo "  → NO habilitado al boot"
    fi
done
```

### Ejercicio 3 — Entender static

```bash
# Encontrar servicios "static" y entender por qué:
systemctl list-unit-files --type=service --state=static --no-pager | head -10

# Elegir uno y verificar que no tiene [Install]:
systemctl cat systemd-journald.service | grep -A5 "\[Install\]"
# No debería mostrar nada (no tiene sección [Install])
```
