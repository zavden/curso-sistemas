# Scripts de Mantenimiento — Limpieza de Logs, Verificación de Espacio y Alertas

## Índice

1. [¿Por qué automatizar el mantenimiento?](#1-por-qué-automatizar-el-mantenimiento)
2. [Anatomía de un script de mantenimiento](#2-anatomía-de-un-script-de-mantenimiento)
3. [Limpieza de logs](#3-limpieza-de-logs)
4. [Verificación de espacio en disco](#4-verificación-de-espacio-en-disco)
5. [Monitorización de servicios](#5-monitorización-de-servicios)
6. [Verificación de salud del sistema](#6-verificación-de-salud-del-sistema)
7. [Alertas y notificaciones](#7-alertas-y-notificaciones)
8. [Programar scripts con cron y systemd timers](#8-programar-scripts-con-cron-y-systemd-timers)
9. [Logging y auditoría de scripts](#9-logging-y-auditoría-de-scripts)
10. [Errores comunes](#10-errores-comunes)
11. [Cheatsheet](#11-cheatsheet)
12. [Ejercicios](#12-ejercicios)

---

## 1. ¿Por qué automatizar el mantenimiento?

Un servidor Linux sin mantenimiento periódico acumula problemas silenciosos: logs que llenan el disco, servicios que fallan sin que nadie lo note, paquetes desactualizados con vulnerabilidades, y recursos que se agotan gradualmente.

```
Sin automatización:                 Con automatización:

Disco se llena → servicio cae       Script detecta 85% de uso
  → alguien se da cuenta horas      → envía alerta
  después → pánico → limpieza       → limpieza automática o manual
  manual → downtime                 → 0 downtime

Servicio muere silenciosamente      Script detecta servicio caído
  → usuarios reportan horas         → reinicia automáticamente
  después → investigar causa        → envía alerta al admin
  → reiniciar → investigar logs     → admin revisa cuando puede
```

### Tareas típicas de mantenimiento

```
Frecuencia    Tarea
──────────────────────────────────────────────────────
Cada hora     Verificar servicios críticos
Diario        Verificar espacio en disco
              Rotar y limpiar logs antiguos
              Verificar backups del día anterior
Semanal       Limpiar caches y temporales
              Verificar actualizaciones de seguridad
              Verificar estado de RAID/discos (SMART)
Mensual       Analizar crecimiento de disco
              Revisar usuarios/cuentas inactivas
              Testing de restauración de backup
```

---

## 2. Anatomía de un script de mantenimiento

Un script de mantenimiento robusto debe ser predecible, seguro y auditable.

### Estructura base

```bash
#!/bin/bash
# maintenance_template.sh — Descripción breve
# Ejecutar como: root (vía cron/systemd timer)
# Frecuencia: diario

set -euo pipefail

# ── Configuración ──────────────────────────────────
LOGFILE="/var/log/maintenance/$(basename "$0" .sh).log"
HOSTNAME=$(hostname -s)
DATE=$(date '+%Y-%m-%d %H:%M:%S')
ALERT_EMAIL="admin@empresa.com"
DRY_RUN="${DRY_RUN:-false}"

# ── Funciones ──────────────────────────────────────
log() {
    echo "[$DATE] $1" | tee -a "$LOGFILE"
}

alert() {
    local subject="[$HOSTNAME] MAINTENANCE ALERT: $1"
    local body="$2"
    log "ALERT: $subject"
    # Enviar por el método configurado (ver sección 7)
    echo "$body" | mail -s "$subject" "$ALERT_EMAIL" 2>/dev/null || true
}

die() {
    log "FATAL: $1"
    alert "Script failed" "$1"
    exit 1
}

# ── Pre-checks ─────────────────────────────────────
mkdir -p "$(dirname "$LOGFILE")"
[[ $EUID -eq 0 ]] || die "Must run as root"

# ── Main ───────────────────────────────────────────
log "Starting maintenance"

# ... tareas aquí ...

log "Maintenance completed"
```

### Principios de diseño

```
1. IDEMPOTENTE
   Ejecutar 2 veces produce el mismo resultado que 1 vez.
   No borrar lo que ya se borró. No crear lo que ya existe.

2. SEGURO POR DEFECTO
   Soportar DRY_RUN para probar sin hacer cambios.
   Validar inputs. Nunca asumir que un path existe.

3. AUDITABLE
   Registrar TODO lo que hace en un log con timestamps.
   Registrar qué se borró, cuánto espacio se liberó.

4. RESISTENTE A FALLOS
   set -euo pipefail para fallar rápido.
   Alertar cuando algo va mal.
   No dejar el sistema en estado inconsistente.

5. AUTOCONTENIDO
   Toda la configuración al inicio (no hardcoded en medio).
   Documentar qué hace y cómo ejecutarlo.
```

---

## 3. Limpieza de logs

### El problema

```
/var/log/ sin mantenimiento:
  messages-20250101.gz    2 MB
  messages-20250201.gz    2 MB
  ...
  messages-20260301       500 MB  ← log actual creciendo
  journal/                8 GB    ← systemd journal sin límite
  nginx/access.log        3 GB    ← logs de aplicación
  nginx/error.log         200 MB
  audit/audit.log         1 GB

Total: 15+ GB de logs que crecen cada día
```

### logrotate: la herramienta estándar

`logrotate` es la herramienta del sistema para rotar, comprimir y eliminar logs. Se ejecuta diariamente vía cron o systemd timer.

```bash
# Configuración principal
cat /etc/logrotate.conf

# Configuraciones por aplicación
ls /etc/logrotate.d/
# nginx  syslog  httpd  mysql  ...
```

### Configuración de logrotate

```
# /etc/logrotate.d/nginx
/var/log/nginx/*.log {
    daily               # Rotar diariamente
    missingok           # No error si el log no existe
    rotate 14           # Mantener 14 archivos rotados
    compress            # Comprimir archivos rotados
    delaycompress       # No comprimir el más reciente (programas que aún escriben)
    notifempty          # No rotar si está vacío
    create 0640 www-data adm   # Permisos del nuevo log
    sharedscripts       # Ejecutar scripts una sola vez (no por cada archivo)
    postrotate
        [ -f /var/run/nginx.pid ] && kill -USR1 $(cat /var/run/nginx.pid)
    endscript
}
```

### Directivas principales de logrotate

| Directiva | Descripción |
|-----------|-------------|
| `daily` / `weekly` / `monthly` | Frecuencia de rotación |
| `rotate N` | Mantener N archivos rotados |
| `compress` | Comprimir con gzip |
| `compresscmd zstd` | Usar otro compresor |
| `delaycompress` | Comprimir en la siguiente rotación |
| `missingok` / `nomissingok` | Tolerar log ausente |
| `notifempty` | No rotar si está vacío |
| `create MODE OWNER GROUP` | Crear nuevo log con estos permisos |
| `copytruncate` | Copiar y truncar (para apps que no reabren el fd) |
| `size 100M` | Rotar cuando exceda tamaño (en vez de por tiempo) |
| `maxsize 500M` | Rotar por tamaño O por tiempo, lo que ocurra primero |
| `minsize 10M` | No rotar si es más pequeño que esto |
| `dateext` | Usar fecha en el nombre (log-20260321.gz) |
| `olddir /var/log/old` | Mover rotados a otro directorio |
| `postrotate` / `endscript` | Comando a ejecutar después de rotar |
| `prerotate` / `endscript` | Comando a ejecutar antes de rotar |

### copytruncate vs create

```
create (default):
  1. Renombrar log.txt → log.txt.1
  2. Crear nuevo log.txt (vacío)
  3. Señalizar al daemon (postrotate) para que reabra el fd
  ✅ Seguro: no se pierden datos
  ❌ Requiere que el daemon soporte reabrir logs (SIGHUP/USR1)

copytruncate:
  1. Copiar log.txt → log.txt.1
  2. Truncar log.txt a 0 bytes
  ✅ No requiere señalizar al daemon (el fd sigue abierto)
  ❌ Riesgo: datos escritos entre copy y truncate se pierden
```

### Limpiar journal de systemd

```bash
# Ver espacio usado por el journal
journalctl --disk-usage

# Limpiar journal por tiempo
sudo journalctl --vacuum-time=7d    # Mantener últimos 7 días

# Limpiar journal por tamaño
sudo journalctl --vacuum-size=500M  # Limitar a 500 MB

# Configurar límite permanente
# /etc/systemd/journald.conf
# [Journal]
# SystemMaxUse=500M        # Máximo en disco
# SystemKeepFree=1G        # Mantener 1G libre en la partición
# MaxRetentionSec=1month   # Máximo 1 mes
# MaxFileSec=1week         # Rotar archivos semanalmente

sudo systemctl restart systemd-journald
```

### Script de limpieza de logs

```bash
#!/bin/bash
# cleanup_logs.sh — Limpieza periódica de logs y temporales

set -euo pipefail

LOGFILE="/var/log/maintenance/cleanup.log"
mkdir -p "$(dirname "$LOGFILE")"

log() { echo "[$(date '+%Y-%m-%d %H:%M:%S')] $1" >> "$LOGFILE"; }

log "=== Cleanup started ==="

# 1. Ejecutar logrotate manualmente si hay logs grandes
FORCED=0
find /var/log -name "*.log" -size +500M 2>/dev/null | while read -r f; do
    log "Large log found: $f ($(du -h "$f" | cut -f1))"
    FORCED=1
done
if [ "$FORCED" -eq 1 ]; then
    logrotate -f /etc/logrotate.conf
    log "Forced logrotate due to large logs"
fi

# 2. Limpiar journal
JOURNAL_SIZE=$(journalctl --disk-usage 2>/dev/null | grep -oP '[\d.]+[MG]' || echo "0M")
log "Journal size before: $JOURNAL_SIZE"
journalctl --vacuum-time=14d --vacuum-size=500M 2>/dev/null
JOURNAL_SIZE=$(journalctl --disk-usage 2>/dev/null | grep -oP '[\d.]+[MG]' || echo "0M")
log "Journal size after: $JOURNAL_SIZE"

# 3. Limpiar archivos temporales antiguos
CLEANED=$(find /tmp -type f -atime +7 -delete -print 2>/dev/null | wc -l)
log "Cleaned $CLEANED temp files from /tmp"

CLEANED=$(find /var/tmp -type f -atime +30 -delete -print 2>/dev/null | wc -l)
log "Cleaned $CLEANED temp files from /var/tmp"

# 4. Limpiar cache de paquetes
if command -v dnf &>/dev/null; then
    dnf clean packages -q 2>/dev/null
    log "Cleaned dnf package cache"
elif command -v apt-get &>/dev/null; then
    apt-get autoclean -qq 2>/dev/null
    log "Cleaned apt package cache"
fi

# 5. Limpiar core dumps antiguos
CLEANED=$(find /var/lib/systemd/coredump -type f -mtime +7 -delete -print 2>/dev/null | wc -l)
log "Cleaned $CLEANED old core dumps"

log "=== Cleanup completed ==="
```

---

## 4. Verificación de espacio en disco

### Entender df y du

```bash
# df: espacio por filesystem (rápido, lee metadata del FS)
df -h
# Filesystem      Size  Used Avail Use% Mounted on
# /dev/sda2        50G   42G  5.4G  89% /
# /dev/sda3       449G  200G  226G  47% /home

# du: espacio por directorio (lento, recorre archivos)
du -sh /var/log/
# 2.3G    /var/log/

# Los 10 directorios más grandes en /var
du -h --max-depth=1 /var/ 2>/dev/null | sort -rh | head -10
```

### Script de monitorización de espacio

```bash
#!/bin/bash
# check_disk.sh — Verificar espacio en disco con umbrales

set -euo pipefail

WARN_THRESHOLD=80   # Porcentaje
CRIT_THRESHOLD=90
LOGFILE="/var/log/maintenance/disk-check.log"

mkdir -p "$(dirname "$LOGFILE")"

log() { echo "[$(date '+%Y-%m-%d %H:%M:%S')] $1" | tee -a "$LOGFILE"; }

STATUS=0

# Verificar cada filesystem (excluyendo tmpfs, devtmpfs, squashfs)
while IFS= read -r line; do
    USAGE=$(echo "$line" | awk '{print $5}' | tr -d '%')
    MOUNT=$(echo "$line" | awk '{print $6}')
    AVAIL=$(echo "$line" | awk '{print $4}')

    if [ "$USAGE" -ge "$CRIT_THRESHOLD" ]; then
        log "CRITICAL: $MOUNT at ${USAGE}% (${AVAIL} available)"
        STATUS=2
    elif [ "$USAGE" -ge "$WARN_THRESHOLD" ]; then
        log "WARNING: $MOUNT at ${USAGE}% (${AVAIL} available)"
        [ "$STATUS" -lt 1 ] && STATUS=1
    else
        log "OK: $MOUNT at ${USAGE}%"
    fi
done < <(df -h --output=source,size,used,avail,pcent,target -x tmpfs -x devtmpfs -x squashfs 2>/dev/null | tail -n +2)

# Verificar inodes también (se pueden agotar antes que el espacio)
while IFS= read -r line; do
    USAGE=$(echo "$line" | awk '{print $5}' | tr -d '%')
    MOUNT=$(echo "$line" | awk '{print $6}')

    if [ -n "$USAGE" ] && [ "$USAGE" -ge "$CRIT_THRESHOLD" ]; then
        log "CRITICAL: inodes on $MOUNT at ${USAGE}%"
        STATUS=2
    elif [ -n "$USAGE" ] && [ "$USAGE" -ge "$WARN_THRESHOLD" ]; then
        log "WARNING: inodes on $MOUNT at ${USAGE}%"
        [ "$STATUS" -lt 1 ] && STATUS=1
    fi
done < <(df -i --output=source,itotal,iused,iavail,ipcent,target -x tmpfs -x devtmpfs -x squashfs 2>/dev/null | tail -n +2)

# Si hay problemas, identificar qué ocupa espacio
if [ "$STATUS" -ge 1 ]; then
    log "--- Top space consumers in critical mounts ---"
    df -h -x tmpfs -x devtmpfs -x squashfs | awk -v t="$WARN_THRESHOLD" \
        '{gsub(/%/,"",$5); if($5+0 >= t) print $6}' | while read -r mount; do
        du -h --max-depth=2 "$mount" 2>/dev/null | sort -rh | head -5 >> "$LOGFILE"
    done
fi

exit $STATUS
```

### Agotamiento de inodes

```
Espacio libre ≠ inodes libres

Un filesystem puede tener GB libres pero 0 inodes.
Esto ocurre con millones de archivos pequeños
(caches, sesiones PHP, mail queues).

Síntomas:
  "No space left on device" pero df -h muestra espacio libre
  → verificar con df -i

Ejemplo:
  $ df -h /
  Filesystem  Size  Used  Avail  Use%  Mounted on
  /dev/sda2    50G   30G    17G   64%  /            ← hay espacio

  $ df -i /
  Filesystem   Inodes   IUsed    IFree  IUse%  Mounted on
  /dev/sda2   3276800  3276800       0   100%  /    ← ¡0 inodes!
```

---

## 5. Monitorización de servicios

### Verificar servicios systemd

```bash
#!/bin/bash
# check_services.sh — Verificar servicios críticos

set -euo pipefail

# Lista de servicios que DEBEN estar running
CRITICAL_SERVICES=(
    sshd
    nginx
    postgresql
    firewalld
)

LOGFILE="/var/log/maintenance/services-check.log"
mkdir -p "$(dirname "$LOGFILE")"

log() { echo "[$(date '+%Y-%m-%d %H:%M:%S')] $1" | tee -a "$LOGFILE"; }

FAILURES=0

for svc in "${CRITICAL_SERVICES[@]}"; do
    if systemctl is-active --quiet "$svc"; then
        log "OK: $svc is running"
    else
        log "FAIL: $svc is NOT running"
        FAILURES=$((FAILURES + 1))

        # Intentar reiniciar (opcional)
        if systemctl restart "$svc" 2>/dev/null; then
            log "RECOVERED: $svc restarted successfully"
        else
            log "CRITICAL: $svc failed to restart"
        fi
    fi
done

# Verificar si hay servicios fallidos en general
FAILED_UNITS=$(systemctl --failed --no-legend --no-pager | wc -l)
if [ "$FAILED_UNITS" -gt 0 ]; then
    log "WARNING: $FAILED_UNITS failed systemd units:"
    systemctl --failed --no-legend --no-pager >> "$LOGFILE"
fi

# Verificar puertos que deberían estar escuchando
for port in 22 80 443 5432; do
    if ss -tlnp | grep -q ":$port "; then
        log "OK: port $port listening"
    else
        log "WARNING: port $port not listening"
    fi
done

exit $FAILURES
```

### Verificar procesos con watchdog pattern

```bash
#!/bin/bash
# watchdog.sh — Reiniciar proceso si no responde

PROCESS_NAME="myapp"
PID_FILE="/var/run/myapp.pid"
HEALTH_URL="http://localhost:8080/health"
MAX_RETRIES=3
RETRY_DELAY=5

check_health() {
    # Verificar que el proceso existe
    if [ -f "$PID_FILE" ]; then
        PID=$(cat "$PID_FILE")
        if ! kill -0 "$PID" 2>/dev/null; then
            return 1
        fi
    else
        pgrep -x "$PROCESS_NAME" > /dev/null || return 1
    fi

    # Verificar que responde a HTTP (si aplica)
    if ! curl -sf --max-time 5 "$HEALTH_URL" > /dev/null 2>&1; then
        return 1
    fi

    return 0
}

for i in $(seq 1 $MAX_RETRIES); do
    if check_health; then
        exit 0
    fi
    echo "Health check failed (attempt $i/$MAX_RETRIES)"
    sleep "$RETRY_DELAY"
done

echo "CRITICAL: $PROCESS_NAME unresponsive after $MAX_RETRIES attempts"
systemctl restart "$PROCESS_NAME"
echo "$PROCESS_NAME restarted at $(date)"
```

---

## 6. Verificación de salud del sistema

### Script de health check completo

```bash
#!/bin/bash
# system_health.sh — Diagnóstico rápido del estado del sistema

set -uo pipefail

RED='\033[0;31m'
YELLOW='\033[1;33m'
GREEN='\033[0;32m'
NC='\033[0m'

ok()   { echo -e "${GREEN}[OK]${NC}    $1"; }
warn() { echo -e "${YELLOW}[WARN]${NC}  $1"; }
fail() { echo -e "${RED}[FAIL]${NC}  $1"; }

echo "=== System Health Check — $(hostname) — $(date) ==="
echo ""

# ── 1. Uptime y load ──
UPTIME=$(uptime -p)
LOAD=$(awk '{print $1}' /proc/loadavg)
CPUS=$(nproc)
LOAD_INT=${LOAD%.*}

echo "--- Load & Uptime ---"
echo "Uptime: $UPTIME"
if [ "$LOAD_INT" -lt "$CPUS" ]; then
    ok "Load average: $LOAD (CPUs: $CPUS)"
elif [ "$LOAD_INT" -lt $((CPUS * 2)) ]; then
    warn "Load average: $LOAD (CPUs: $CPUS) — elevated"
else
    fail "Load average: $LOAD (CPUs: $CPUS) — critical"
fi
echo ""

# ── 2. Memoria ──
echo "--- Memory ---"
MEM_TOTAL=$(awk '/MemTotal/ {print $2}' /proc/meminfo)
MEM_AVAIL=$(awk '/MemAvailable/ {print $2}' /proc/meminfo)
MEM_USED_PCT=$(( (MEM_TOTAL - MEM_AVAIL) * 100 / MEM_TOTAL ))

if [ "$MEM_USED_PCT" -lt 80 ]; then
    ok "Memory: ${MEM_USED_PCT}% used ($(( MEM_AVAIL / 1024 )) MB available)"
elif [ "$MEM_USED_PCT" -lt 95 ]; then
    warn "Memory: ${MEM_USED_PCT}% used ($(( MEM_AVAIL / 1024 )) MB available)"
else
    fail "Memory: ${MEM_USED_PCT}% used ($(( MEM_AVAIL / 1024 )) MB available)"
fi

SWAP_TOTAL=$(awk '/SwapTotal/ {print $2}' /proc/meminfo)
SWAP_FREE=$(awk '/SwapFree/ {print $2}' /proc/meminfo)
if [ "$SWAP_TOTAL" -gt 0 ]; then
    SWAP_USED_PCT=$(( (SWAP_TOTAL - SWAP_FREE) * 100 / SWAP_TOTAL ))
    if [ "$SWAP_USED_PCT" -lt 50 ]; then
        ok "Swap: ${SWAP_USED_PCT}% used"
    else
        warn "Swap: ${SWAP_USED_PCT}% used — system may be swapping"
    fi
fi
echo ""

# ── 3. Disco ──
echo "--- Disk ---"
df -h -x tmpfs -x devtmpfs -x squashfs --output=target,pcent,avail 2>/dev/null | \
    tail -n +2 | while read -r mount pct avail; do
    pct_num=${pct%%%}
    if [ "$pct_num" -lt 80 ]; then
        ok "$mount: ${pct} used (${avail} free)"
    elif [ "$pct_num" -lt 90 ]; then
        warn "$mount: ${pct} used (${avail} free)"
    else
        fail "$mount: ${pct} used (${avail} free)"
    fi
done
echo ""

# ── 4. Servicios fallidos ──
echo "--- Failed Services ---"
FAILED=$(systemctl --failed --no-legend --no-pager 2>/dev/null)
if [ -z "$FAILED" ]; then
    ok "No failed services"
else
    FCOUNT=$(echo "$FAILED" | wc -l)
    fail "$FCOUNT failed service(s):"
    echo "$FAILED" | while read -r line; do
        echo "        $line"
    done
fi
echo ""

# ── 5. Seguridad ──
echo "--- Security ---"
# Últimos logins fallidos
FAIL_LOGINS=$(journalctl -u sshd --since "24 hours ago" --no-pager 2>/dev/null | \
    grep -c "Failed password" || echo "0")
if [ "$FAIL_LOGINS" -lt 10 ]; then
    ok "Failed SSH logins (24h): $FAIL_LOGINS"
elif [ "$FAIL_LOGINS" -lt 100 ]; then
    warn "Failed SSH logins (24h): $FAIL_LOGINS"
else
    fail "Failed SSH logins (24h): $FAIL_LOGINS — possible brute force"
fi

# Updates de seguridad pendientes
if command -v dnf &>/dev/null; then
    SEC_UPDATES=$(dnf updateinfo list --security 2>/dev/null | grep -c "^" || echo "0")
    if [ "$SEC_UPDATES" -eq 0 ]; then
        ok "No pending security updates"
    else
        warn "$SEC_UPDATES security updates available"
    fi
fi
echo ""

# ── 6. Conectividad ──
echo "--- Network ---"
if ping -c 1 -W 3 8.8.8.8 > /dev/null 2>&1; then
    ok "External connectivity (8.8.8.8)"
else
    fail "No external connectivity"
fi

if host google.com > /dev/null 2>&1; then
    ok "DNS resolution working"
else
    fail "DNS resolution failing"
fi
echo ""
```

### Verificar SMART de discos

```bash
# Verificar salud de discos físicos
check_smart() {
    for disk in /dev/sd?; do
        [ -b "$disk" ] || continue
        HEALTH=$(sudo smartctl -H "$disk" 2>/dev/null | grep "SMART overall" | awk '{print $NF}')
        if [ "$HEALTH" = "PASSED" ]; then
            echo "OK: $disk SMART health PASSED"
        else
            echo "CRITICAL: $disk SMART health: $HEALTH"
        fi
    done
}
```

---

## 7. Alertas y notificaciones

### Método 1: Email con mail/mailx

```bash
# Enviar alerta por email (requiere MTA configurado: postfix, sendmail)
echo "Disk /dev/sda1 at 95%" | mail -s "[ALERT] Disk space critical" admin@empresa.com

# Con cuerpo más detallado
mail -s "[$(hostname)] Disk alert" admin@empresa.com << EOF
Server: $(hostname)
Time: $(date)
Issue: Disk space critical

$(df -h)

--
Automated maintenance script
EOF
```

### Método 2: Webhook (Slack, Discord, Teams)

```bash
# Enviar alerta a Slack
send_slack() {
    local message="$1"
    local webhook_url="https://hooks.slack.com/services/T.../B.../xxx"

    curl -s -X POST "$webhook_url" \
        -H 'Content-Type: application/json' \
        -d "{\"text\": \"$message\"}" > /dev/null
}

send_slack ":warning: [$(hostname)] Disk space at 95% on /dev/sda1"
```

### Método 3: systemd con OnFailure

```ini
# /etc/systemd/system/backup.service
[Unit]
Description=Daily backup
OnFailure=alert-failure@%n.service

[Service]
Type=oneshot
ExecStart=/usr/local/bin/backup.sh
```

```ini
# /etc/systemd/system/alert-failure@.service
[Unit]
Description=Send alert for %i failure

[Service]
Type=oneshot
ExecStart=/usr/local/bin/send_alert.sh "Service %i failed on $(hostname)"
```

### Método 4: Escribir al journal con prioridad

```bash
# Escribir al journal (se integra con herramientas de monitorización)
logger -p crit "Disk space critical on /: 95%"
logger -p warning "Backup took longer than expected: 3h"
logger -p info "Maintenance completed successfully"

# systemd-cat para redirigir salida de scripts
/usr/local/bin/maintenance.sh 2>&1 | systemd-cat -t maintenance -p info
```

### Patrón genérico de alertas

```bash
# alert.sh — función reutilizable para múltiples métodos

ALERT_METHOD="${ALERT_METHOD:-log}"  # log, email, slack, journal

send_alert() {
    local severity="$1"  # info, warning, critical
    local message="$2"
    local timestamp=$(date '+%Y-%m-%d %H:%M:%S')
    local formatted="[$timestamp] [$severity] [$(hostname)] $message"

    case "$ALERT_METHOD" in
        log)
            echo "$formatted" >> /var/log/maintenance/alerts.log
            ;;
        email)
            echo "$message" | mail -s "[$(hostname)] $severity: $message" "$ALERT_EMAIL"
            ;;
        slack)
            local emoji=""
            case "$severity" in
                critical) emoji=":red_circle:" ;;
                warning)  emoji=":warning:" ;;
                info)     emoji=":information_source:" ;;
            esac
            curl -s -X POST "$SLACK_WEBHOOK" \
                -d "{\"text\": \"$emoji $formatted\"}" > /dev/null
            ;;
        journal)
            local priority=""
            case "$severity" in
                critical) priority="crit" ;;
                warning)  priority="warning" ;;
                info)     priority="info" ;;
            esac
            logger -p "$priority" -t maintenance "$message"
            ;;
    esac
}
```

---

## 8. Programar scripts con cron y systemd timers

### cron

```bash
# Editar crontab del usuario
crontab -e

# Editar crontab de root
sudo crontab -e

# Formato:
# MIN  HORA  DÍA  MES  DÍASEMANA  COMANDO
# 0-59 0-23  1-31 1-12 0-7(0,7=dom)
```

```
# Ejemplos de crontab
# ─────────────────────────────────────────────────────
# Cada hora
0 * * * *   /usr/local/bin/check_services.sh

# Diario a las 03:00
0 3 * * *   /usr/local/bin/cleanup_logs.sh

# Cada 15 minutos
*/15 * * * * /usr/local/bin/check_disk.sh

# Lunes a viernes a las 08:00
0 8 * * 1-5  /usr/local/bin/system_health.sh | mail -s "Daily report" admin@e.com

# Primer domingo de cada mes a las 02:00
0 2 1-7 * 0  /usr/local/bin/monthly_cleanup.sh

# Redirigir salida a log (evitar emails de cron)
0 3 * * *   /usr/local/bin/cleanup.sh >> /var/log/cron-cleanup.log 2>&1
```

### Archivos de cron del sistema

```
/etc/crontab              Crontab del sistema (con campo USER)
/etc/cron.d/              Archivos de cron adicionales
/etc/cron.daily/          Scripts ejecutados diariamente
/etc/cron.weekly/         Scripts ejecutados semanalmente
/etc/cron.monthly/        Scripts ejecutados mensualmente
/etc/cron.hourly/         Scripts ejecutados cada hora
```

```bash
# Poner un script en cron.daily (sin editar crontab)
sudo cp cleanup.sh /etc/cron.daily/cleanup
sudo chmod 755 /etc/cron.daily/cleanup
# NOTA: no debe tener extensión .sh en cron.daily (run-parts los ignora)
```

### systemd timers (alternativa moderna)

```ini
# /etc/systemd/system/disk-check.service
[Unit]
Description=Check disk space

[Service]
Type=oneshot
ExecStart=/usr/local/bin/check_disk.sh
```

```ini
# /etc/systemd/system/disk-check.timer
[Unit]
Description=Check disk space every 15 minutes

[Timer]
OnBootSec=5min
OnUnitActiveSec=15min
# O con calendario fijo:
# OnCalendar=*:0/15

[Install]
WantedBy=timers.target
```

```bash
sudo systemctl enable --now disk-check.timer
systemctl list-timers --all
```

### cron vs systemd timers

| Característica | cron | systemd timer |
|---------------|------|---------------|
| Logging | Manual (redirigir) | Automático (journal) |
| Dependencias | No | Sí (After=, Requires=) |
| Ejecución perdida | No se ejecuta | `Persistent=true` |
| Aleatorización | No | `RandomizedDelaySec` |
| Límite de recursos | No | CPUQuota=, MemoryMax= |
| Estado | `crontab -l` | `systemctl list-timers` |
| Notificación de fallo | No (salvo email) | `OnFailure=` |

---

## 9. Logging y auditoría de scripts

### Patrón de logging robusto

```bash
# Logging con rotación integrada

LOGFILE="/var/log/maintenance/script.log"
MAX_LOG_SIZE=10485760  # 10 MB

# Rotar si excede el tamaño
if [ -f "$LOGFILE" ] && [ "$(stat -c%s "$LOGFILE" 2>/dev/null || echo 0)" -gt "$MAX_LOG_SIZE" ]; then
    mv "$LOGFILE" "${LOGFILE}.1"
    gzip "${LOGFILE}.1" 2>/dev/null &
fi

# Función de log con niveles
log() {
    local level="${1:-INFO}"
    local message="${2:-}"
    echo "[$(date '+%Y-%m-%d %H:%M:%S')] [$level] $message" >> "$LOGFILE"
}

log INFO "Script started"
log WARN "Disk above threshold"
log ERROR "Service failed to restart"
```

### Registrar qué hace el script (auditoría)

```bash
# Para scripts destructivos (limpiar, borrar), registrar EXACTAMENTE
# qué se hizo para poder investigar si algo sale mal

cleanup_old_files() {
    local dir="$1"
    local days="$2"
    local count=0
    local freed=0

    while IFS= read -r -d '' file; do
        size=$(stat -c%s "$file" 2>/dev/null || echo 0)
        freed=$((freed + size))
        count=$((count + 1))
        log INFO "Deleted: $file ($size bytes)"
        rm -f "$file"
    done < <(find "$dir" -type f -mtime +"$days" -print0 2>/dev/null)

    log INFO "Cleanup summary: deleted $count files, freed $(numfmt --to=iec $freed)"
}
```

---

## 10. Errores comunes

### Error 1: Scripts sin set -euo pipefail

```bash
# MAL — errores pasan desapercibidos
#!/bin/bash
rm -rf /backup/old/     # Si falla, sigue ejecutando
cp -a /data/ /backup/   # Puede crear backup incompleto sin aviso

# BIEN
#!/bin/bash
set -euo pipefail
rm -rf /backup/old/     # Si falla, el script se detiene
cp -a /data/ /backup/   # Solo se ejecuta si lo anterior funcionó
```

### Error 2: Hardcodear rutas sin verificar

```bash
# MAL — asume que el directorio existe
#!/bin/bash
rm -rf /backup/snapshots/old/*

# BIEN — verificar antes de operar
#!/bin/bash
BACKUP_DIR="/backup/snapshots/old"
if [ -d "$BACKUP_DIR" ]; then
    find "$BACKUP_DIR" -mindepth 1 -mtime +30 -delete
else
    echo "WARNING: $BACKUP_DIR does not exist" >&2
fi
```

### Error 3: Cron sin PATH ni redirección de errores

```bash
# MAL — cron tiene un PATH mínimo, comandos pueden no encontrarse
0 3 * * * cleanup.sh

# BIEN — ruta completa y capturar salida
PATH=/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin
0 3 * * * /usr/local/bin/cleanup.sh >> /var/log/cron-cleanup.log 2>&1
```

### Error 4: Scripts de limpieza demasiado agresivos

```bash
# MAL — borrar TODOS los .log sin pensar
find /var/log -name "*.log" -delete

# BIEN — borrar solo los ROTADOS de más de N días
find /var/log -name "*.log.*.gz" -mtime +30 -delete
# Los .log activos NO se tocan
```

### Error 5: No usar lockfile para evitar ejecución concurrente

```bash
# MAL — si el script anterior no terminó, se ejecuta otra instancia
0 * * * * /usr/local/bin/heavy_maintenance.sh

# BIEN — usar flock para evitar ejecución concurrente
0 * * * * /usr/bin/flock -n /var/lock/maintenance.lock /usr/local/bin/heavy_maintenance.sh

# O dentro del script:
LOCKFILE="/var/lock/$(basename "$0").lock"
exec 200>"$LOCKFILE"
flock -n 200 || { echo "Already running"; exit 1; }
```

---

## 11. Cheatsheet

```
LIMPIEZA DE LOGS
  logrotate -f /etc/logrotate.conf        Forzar rotación
  journalctl --vacuum-time=7d             Limpiar journal por tiempo
  journalctl --vacuum-size=500M           Limpiar journal por tamaño
  journalctl --disk-usage                 Ver espacio del journal

ESPACIO EN DISCO
  df -h                                   Uso por filesystem
  df -i                                   Uso de inodes
  du -sh /dir/                            Tamaño de directorio
  du -h --max-depth=1 /var/ | sort -rh    Top consumidores

SERVICIOS
  systemctl is-active sshd                Verificar si activo
  systemctl --failed                      Listar servicios fallidos
  ss -tlnp                                Puertos escuchando

PROGRAMACIÓN
  crontab -e                              Editar cron del usuario
  crontab -l                              Listar cron del usuario
  systemctl list-timers                   Listar timers systemd
  flock -n /var/lock/x.lock cmd           Evitar ejecución concurrente

ALERTAS
  logger -p crit "mensaje"                Escribir al journal
  mail -s "subject" user@host             Enviar email
  systemd-cat -t tag -p prio cmd          Redirigir a journal

BUENAS PRÁCTICAS
  set -euo pipefail                       Inicio de todo script
  DRY_RUN=true ./script.sh                Probar sin cambios
  flock -n lockfile script.sh             Evitar concurrencia
  >> logfile 2>&1                          Capturar toda la salida
```

---

## 12. Ejercicios

### Ejercicio 1: Script de limpieza configurable

**Objetivo**: Crear un script de limpieza con modo dry-run y logging.

```bash
# 1. Crear estructura de prueba con archivos de distintas edades
mkdir -p /tmp/maint-lab/{logs,tmp,cache}

# Crear archivos "viejos" (simular con touch -d)
for i in $(seq 1 5); do
    touch -d "$(( i * 10 )) days ago" "/tmp/maint-lab/logs/app.log.$i.gz"
    touch -d "$(( i * 3 )) days ago" "/tmp/maint-lab/tmp/sess_$RANDOM"
    touch -d "$(( i * 7 )) days ago" "/tmp/maint-lab/cache/obj_$RANDOM"
done

# Crear archivos "recientes"
echo "current log" > /tmp/maint-lab/logs/app.log
echo "current session" > /tmp/maint-lab/tmp/sess_current
echo "current cache" > /tmp/maint-lab/cache/obj_current

# 2. Crear el script de limpieza
cat > /tmp/maint-lab/cleanup.sh << 'SCRIPT'
#!/bin/bash
set -euo pipefail

# Configuración
LOG_DIR="/tmp/maint-lab/logs"
TMP_DIR="/tmp/maint-lab/tmp"
CACHE_DIR="/tmp/maint-lab/cache"
LOG_RETENTION=14    # días
TMP_RETENTION=7
CACHE_RETENTION=21
DRY_RUN="${DRY_RUN:-false}"

log() { echo "[$(date '+%H:%M:%S')] $1"; }

cleanup_dir() {
    local dir="$1"
    local days="$2"
    local label="$3"
    local count=0

    if [ ! -d "$dir" ]; then
        log "SKIP: $dir does not exist"
        return
    fi

    while IFS= read -r -d '' file; do
        count=$((count + 1))
        if [ "$DRY_RUN" = "true" ]; then
            log "DRY-RUN: would delete $file"
        else
            rm -f "$file"
            log "DELETED: $file"
        fi
    done < <(find "$dir" -type f -mtime +"$days" -print0 2>/dev/null)

    log "$label: $count files $([ "$DRY_RUN" = "true" ] && echo "would be " || echo "")deleted"
}

log "=== Cleanup $([ "$DRY_RUN" = "true" ] && echo "(DRY RUN) " || echo "")started ==="
cleanup_dir "$LOG_DIR" "$LOG_RETENTION" "Logs"
cleanup_dir "$TMP_DIR" "$TMP_RETENTION" "Temp"
cleanup_dir "$CACHE_DIR" "$CACHE_RETENTION" "Cache"
log "=== Cleanup completed ==="
SCRIPT
chmod +x /tmp/maint-lab/cleanup.sh

# 3. Ejecutar en modo dry-run primero
echo "=== DRY RUN ==="
DRY_RUN=true /tmp/maint-lab/cleanup.sh

# 4. Verificar que nada se borró
echo ""
echo "Files still present:"
find /tmp/maint-lab -type f | sort

# 5. Ejecutar en modo real
echo ""
echo "=== REAL RUN ==="
/tmp/maint-lab/cleanup.sh

# 6. Verificar qué sobrevivió
echo ""
echo "Files remaining:"
find /tmp/maint-lab -type f -not -name "cleanup.sh" | sort

# 7. Limpiar
rm -rf /tmp/maint-lab
```

**Pregunta de reflexión**: ¿Por qué es importante el modo dry-run? ¿Qué podría salir mal si ejecutas un script de limpieza nuevo directamente en producción sin probarlo primero?

> Un error en la configuración (ruta equivocada, retención en 0, patrón demasiado amplio) podría borrar archivos críticos en producción. El dry-run permite ver **exactamente** qué se borraría sin borrar nada. Es especialmente importante la primera vez que se despliega un script nuevo o después de cambiar la configuración. En producción, un script que borre `/var/log/` activo en vez de los rotados puede dejar al sistema sin capacidad de diagnóstico.

---

### Ejercicio 2: Monitor de disco con umbrales

**Objetivo**: Crear un monitor que detecte problemas de espacio y genere alertas diferenciadas.

```bash
# 1. Crear el script de monitorización
cat > /tmp/check_disk.sh << 'SCRIPT'
#!/bin/bash
set -uo pipefail

WARN=80
CRIT=90

RED='\033[0;31m'
YELLOW='\033[1;33m'
GREEN='\033[0;32m'
NC='\033[0m'

EXIT_CODE=0

echo "=== Disk Space Check — $(date) ==="
echo ""

# Espacio
while IFS= read -r line; do
    mount=$(echo "$line" | awk '{print $1}')
    pct=$(echo "$line" | awk '{print $2}' | tr -d '%')
    avail=$(echo "$line" | awk '{print $3}')

    if [ "$pct" -ge "$CRIT" ]; then
        echo -e "${RED}[CRIT]${NC} $mount: ${pct}% used (${avail} free)"
        EXIT_CODE=2
    elif [ "$pct" -ge "$WARN" ]; then
        echo -e "${YELLOW}[WARN]${NC} $mount: ${pct}% used (${avail} free)"
        [ "$EXIT_CODE" -lt 1 ] && EXIT_CODE=1
    else
        echo -e "${GREEN}[ OK ]${NC} $mount: ${pct}% used (${avail} free)"
    fi
done < <(df -h --output=target,pcent,avail -x tmpfs -x devtmpfs -x squashfs 2>/dev/null | tail -n +2)

echo ""

# Inodes
echo "--- Inodes ---"
while IFS= read -r line; do
    mount=$(echo "$line" | awk '{print $1}')
    pct=$(echo "$line" | awk '{print $2}' | tr -d '%')
    [ -z "$pct" ] && continue

    if [ "$pct" -ge "$CRIT" ]; then
        echo -e "${RED}[CRIT]${NC} $mount inodes: ${pct}%"
    elif [ "$pct" -ge "$WARN" ]; then
        echo -e "${YELLOW}[WARN]${NC} $mount inodes: ${pct}%"
    else
        echo -e "${GREEN}[ OK ]${NC} $mount inodes: ${pct}%"
    fi
done < <(df -i --output=target,ipcent -x tmpfs -x devtmpfs -x squashfs 2>/dev/null | tail -n +2)

echo ""
echo "Exit code: $EXIT_CODE (0=ok, 1=warn, 2=crit)"
exit $EXIT_CODE
SCRIPT
chmod +x /tmp/check_disk.sh

# 2. Ejecutar
/tmp/check_disk.sh

# 3. Probar con umbrales bajos para forzar warnings
echo ""
echo "=== Con umbrales bajos (10/20) ==="
sed 's/WARN=80/WARN=10/; s/CRIT=90/CRIT=20/' /tmp/check_disk.sh | bash

# 4. Limpiar
rm /tmp/check_disk.sh
```

**Pregunta de reflexión**: El script verifica espacio e inodes. ¿Qué otro recurso de filesystem podría agotarse sin que df lo muestre? Pista: piensa en descriptores.

> Los **file descriptors** abiertos del sistema. Un proceso con file descriptor leak puede agotar el límite (`/proc/sys/fs/file-max` o `ulimit -n`). Cuando se agotan, ningún proceso puede abrir archivos nuevos — incluyendo logs, sockets y pipes — y el sistema se comporta erráticamente. `df` no lo muestra porque no es un recurso del filesystem sino del kernel. Se verifica con `cat /proc/sys/fs/file-nr` (usado / máximo) o `lsof | wc -l`.

---

### Ejercicio 3: Health check completo con reporte

**Objetivo**: Combinar todas las verificaciones en un script que genere un reporte en formato fijo.

```bash
# 1. Ejecutar el health check y capturar resultados
cat > /tmp/health_check.sh << 'SCRIPT'
#!/bin/bash
set -uo pipefail

REPORT="/tmp/health-report-$(date +%Y%m%d).txt"

{
echo "============================================"
echo " SYSTEM HEALTH REPORT"
echo " Host: $(hostname)"
echo " Date: $(date)"
echo " Kernel: $(uname -r)"
echo "============================================"
echo ""

# Uptime
echo "--- Uptime ---"
uptime
echo ""

# Load
echo "--- Load Average ---"
LOAD1=$(awk '{print $1}' /proc/loadavg)
CPUS=$(nproc)
echo "Load (1min): $LOAD1 | CPUs: $CPUS | Status: $(
    awk "BEGIN {if ($LOAD1 < $CPUS) print \"OK\"; else print \"HIGH\"}"
)"
echo ""

# Memory
echo "--- Memory ---"
free -h
echo ""

# Disk
echo "--- Disk Space ---"
df -h -x tmpfs -x devtmpfs -x squashfs
echo ""

# Top 5 consumidores de espacio en /var
echo "--- Top Space Consumers (/var) ---"
du -h --max-depth=1 /var/ 2>/dev/null | sort -rh | head -5
echo ""

# Services
echo "--- Failed Services ---"
FAILED=$(systemctl --failed --no-legend --no-pager 2>/dev/null)
if [ -z "$FAILED" ]; then
    echo "None"
else
    echo "$FAILED"
fi
echo ""

# Network
echo "--- Listening Ports ---"
ss -tlnp 2>/dev/null | head -20
echo ""

# Recent errors in journal
echo "--- Recent Errors (last 1h) ---"
journalctl --since "1 hour ago" -p err --no-pager 2>/dev/null | tail -10
echo ""

echo "============================================"
echo " END OF REPORT"
echo "============================================"
} | tee "$REPORT"

echo ""
echo "Report saved to: $REPORT"
SCRIPT
chmod +x /tmp/health_check.sh

# 2. Ejecutar
/tmp/health_check.sh

# 3. Limpiar
rm /tmp/health_check.sh /tmp/health-report-*.txt 2>/dev/null
```

**Pregunta de reflexión**: Este script muestra el estado actual del sistema como una "foto". ¿Cómo lo modificarías para detectar **tendencias** (ej: disco que crece 2% diario)? ¿Qué necesitarías almacenar entre ejecuciones?

> Necesitarías almacenar las métricas históricas entre ejecuciones — un archivo CSV o una base de datos ligera (SQLite) con timestamp y valores (porcentaje de disco, memoria, load, etc.). Cada ejecución registraría los valores actuales y los compararía con los de hace N días. Si el disco pasó de 70% a 80% en 5 días, se puede calcular la tasa de crecimiento (2%/día) y predecir cuándo se llenará (en ~10 días). Esto transforma el script de reactivo ("el disco está al 90%") a predictivo ("el disco se llenará en 10 días al ritmo actual").

---

> **Siguiente tema**: T02 — Compilación desde fuente avanzada: configure options, make check/test, DESTDIR.
