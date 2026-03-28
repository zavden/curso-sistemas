# T01 — Consultas avanzadas

> **Objetivo:** Dominar las técnicas avanzadas de `journalctl` para diagnóstico, monitoreo y correlación de eventos en escenarios reales.

## Repaso rápido

En C06/S04 cubrimos los filtros básicos. Aquí profundizamos en técnicas avanzadas para escenarios de producción:

```bash
# Filtros básicos (repaso):
journalctl -u nginx              # por unidad
journalctl -p err                # por prioridad
journalctl -b -1                 # por boot anterior
journalctl --since "1 hour ago"  # por tiempo
journalctl -k                    # kernel
```

---

## Combinar filtros complejos

### AND implícito entre filtros

```bash
# Múltiples filtros se combinan con AND:
journalctl -u sshd -p err -b --since "2 hours ago"
#       ↑ sshd  ↑ err+  ↑ boot actual  ↑ últimas 2 horas
#       (todos deben cumplirse)

# Con campos explícitos:
journalctl _SYSTEMD_UNIT=nginx.service PRIORITY=3 _UID=0
# nginx.service AND prioridad err AND ejecutado como root

# Cada campo adicional restringe más el resultado
```

### OR entre unidades

```bash
# Múltiples -u se combinan con OR:
journalctl -u nginx -u php-fpm -u mysql
# nginx OR php-fpm OR mysql

# Pilas completas de aplicaciones web:
journalctl -u nginx -u php-fpm -u mysql -p warning --since today
# Warnings+ de cualquier servicio, desde hoy
```

### OR con el operador `+`

```bash
# El + combina grupos de campos con OR:
journalctl _SYSTEMD_UNIT=sshd.service + _COMM=sudo
# Mensajes de sshd.service OR mensajes del comando sudo

# Dentro de cada grupo: AND | Entre grupos separados por +: OR
journalctl _SYSTEMD_UNIT=nginx.service _PID=1234 + _COMM=curl
# (nginx.service AND PID 1234) OR (comando curl)
```

### Filtros por campo con wildcards

```bash
# Los campos soportan globbing:
journalctl _SYSTEMD_UNIT=nginx*
# Todas las unidades que empiezan con "nginx"

# Listar valores disponibles de un campo:
journalctl -F _SYSTEMD_UNIT | grep -i docker
# docker.service
# docker.socket

journalctl -F _COMM | head -20
# Lista de los 20 comandos más recientes que han generado logs
```

---

## Filtros de tiempo avanzados

### Rangos precisos

```bash
# Rango exacto:
journalctl --since "2026-03-17 08:00:00" --until "2026-03-17 12:00:00"

# Con zona horaria explícita:
journalctl --since "2026-03-17 08:00:00 UTC" --until "2026-03-17 12:00:00 UTC"

# Relativos combinados:
journalctl --since "3 hours ago" --until "1 hour ago"
# Ventana de 2 horas que terminó hace 1 hora

# Un segundo específico (correlacionar eventos):
journalctl --since "2026-03-17 10:30:45" --until "2026-03-17 10:30:46"
```

### Por cursor (procesamiento incremental)

```bash
# Cada entrada tiene un cursor único (posición en el journal):
journalctl -n 1 -o json | jq -r '.__CURSOR'
# s=abc123;i=456;b=def789;m=1234567890;t=60abcdef12345;x=abcdef

# Leer desde un cursor en adelante:
journalctl --after-cursor="s=abc123;i=456;b=def789;..."
# Todas las entradas DESPUÉS de esa posición

# Caso de uso: monitoreo incremental
CURSOR_FILE="/var/lib/mymonitor/cursor"
LAST_CURSOR=$(cat "$CURSOR_FILE" 2>/dev/null)

if [ -n "$LAST_CURSOR" ]; then
    journalctl --after-cursor="$LAST_CURSOR" -o json --no-pager
else
    journalctl --since "5 min ago" -o json --no-pager
fi | while read -r line; do
    echo "$line" | jq -r '.__CURSOR' > "$CURSOR_FILE"
    echo "$line" | jq -r '.MESSAGE'
done
```

---

## Formatos de salida avanzados

### JSON para procesamiento automático

```bash
# JSON compacto (una línea por entry):
journalctl -u nginx -o json --no-pager -n 5
# {"__CURSOR":"s=...","PRIORITY":"6","_HOSTNAME":"server","MESSAGE":"GET /index.html",...}

# JSON formateado (legible):
journalctl -u nginx -o json-pretty -n 1

# Extraer solo el mensaje con jq:
journalctl -u nginx -o json --no-pager -n 5 | jq -r '.MESSAGE'

# Filtrar por contenido del mensaje (jq):
journalctl -u nginx -o json --no-pager | \
    jq -r 'select(.MESSAGE | test("500|502|503")) | .MESSAGE'
# Solo mensajes que contengan 500, 502 o 503

# Filtrar con múltiples condiciones:
journalctl -o json --no-pager | \
    jq -r 'select(.PRIORITY | tonumber | . <= 3) | "\(.MESSAGE)"'
```

### Verbose — Todos los campos

```bash
journalctl -u sshd -n 1 -o verbose
# Tue 2026-03-17 15:30:00.123456 UTC [s=abc;i=456;b=def;m=123;t=60a;x=abc]
#     _BOOT_ID=def789...
#     _MACHINE_ID=abc123...
#     _HOSTNAME=server
#     PRIORITY=6
#     SYSLOG_FACILITY=4
#     SYSLOG_IDENTIFIER=sshd
#     _UID=0
#     _GID=0
#     _COMM=sshd
#     _EXE=/usr/sbin/sshd
#     _CMDLINE=sshd: user [priv]
#     _PID=1234
#     _SYSTEMD_UNIT=sshd.service
#     _SYSTEMD_CGROUP=/system.slice/sshd.service
#     _TRANSPORT=syslog
#     MESSAGE=Accepted publickey for user from 192.168.1.100 port 45678 ssh2
#     _SOURCE_REALTIME_TIMESTAMP=1710689400123456

# verbose muestra TODOS los campos — ideal para:
# - Descubrir qué campos tiene un mensaje
# - Depurar reglas de filtrado
# - Entender la estructura del journal
```

### Guía rápida de formatos

| Formato | Uso | Ejemplo de salida |
|---------|-----|-------------------|
| `short` | Default, estilo syslog clásico | `Mar 17 15:30:00 server sshd[1234]: mensaje` |
| `short-precise` | Con microsegundos | `Mar 17 15:30:00.123456 server sshd[1234]: mensaje` |
| `short-iso` | Timestamps ISO 8601 | `2026-03-17T15:30:00+0000 server sshd[1234]: mensaje` |
| `short-unix` | Timestamp Unix epoch | `1710689400.123456 server sshd[1234]: mensaje` |
| `verbose` | Todos los campos | Multi-línea con todos los metadata |
| `json` | Una línea JSON por entry | `{"MESSAGE":"...","PRIORITY":"6",...}` |
| `json-pretty` | JSON formateado | Multi-línea legible |
| `cat` | Solo el mensaje | `GET /index.html 200` (sin timestamp) |
| `export` | Formato binario export | Para `systemd-journal-remote` |

---

## Consultas de análisis

### Contar errores por servicio

```bash
journalctl -p err --since today -o json --no-pager | \
    jq -r '._SYSTEMD_UNIT // .SYSLOG_IDENTIFIER // "unknown"' | \
    sort | uniq -c | sort -rn | head -10
#   45 nginx.service
#   12 php-fpm.service
#    3 sshd.service
#    1 unknown
```

### Timeline de un incidente

```bash
# Ver TODO lo que pasó en un rango de 5 minutos:
journalctl --since "2026-03-17 14:00" --until "2026-03-17 14:05" \
    --no-pager -o short-precise

# Solo errores y warnings:
journalctl --since "2026-03-17 14:00" --until "2026-03-17 14:05" \
    -p warning --no-pager -o short-precise

# Con contexto de múltiples servicios:
journalctl --since "2026-03-17 14:00" --until "2026-03-17 14:05" \
    -u nginx -u php-fpm -u mysql --no-pager -o short-precise
```

### Correlacionar eventos (antes y después)

```bash
# ¿Qué pasó justo después del deploy?
journalctl -u myapp --since "2026-03-17 14:00" -p err -n 1
# Primer error después de las 14:00

# ¿Qué pasó justo ANTES de que el servicio fallara?
journalctl -u myapp --until "2026-03-17 14:05" -n 20 --no-pager
# Últimas 20 líneas antes de las 14:05

# Contexto amplio:
journalctl -u myapp --since "13:55" --until "14:05" --no-pager
```

### Mensajes del kernel específicos

```bash
# OOM Killer (Out of Memory):
journalctl -k --grep="Out of memory|oom-killer|Killed process"

# Errores de disco:
journalctl -k --grep="I/O error|EXT4-fs error|XFS.*error"

# Errores de red:
journalctl -k --grep="link is not ready|carrier lost|NIC Link"

# Problemas de hardware:
journalctl -k --grep="hardware error|PCIe|ECC"
```

### --grep (filtro en el mensaje)

```bash
# Filtra por regex en el campo MESSAGE:
journalctl --grep="failed|error|timeout" -p warning --since today

# Case insensitive:
journalctl --grep="connection refused" --case-sensitive=no -u nginx

# Contexto (journalctl no lo soporta, usar grep):
journalctl -u myapp --no-pager | grep -B2 -A2 "FATAL"
# -B2 = 2 líneas antes, -A2 = 2 líneas después
```

---

## Uso en scripts de monitoreo

### Verificar errores recientes de un servicio

```bash
#!/bin/bash
# Uso: check-errors.sh <servicio> [minutos]
SERVICE="${1:-nginx}"
MINUTES="${2:-5}"

ERRORS=$(journalctl -u "$SERVICE" -p err --since "$MINUTES min ago" \
    -q --no-pager 2>/dev/null | wc -l)

if [ "$ERRORS" -gt 0 ]; then
    echo "ALERTA: $SERVICE tiene $ERRORS errores en los últimos $MINUTES min"
    journalctl -u "$SERVICE" -p err --since "$MINUTES min ago" \
        --no-pager -o short-precise | tail -5
    exit 1
fi
echo "OK: $SERVICE sin errores recientes"
```

### Extraer métricas de logs

```bash
# Contar códigos HTTP de nginx:
journalctl -u nginx -o cat --since "1 hour ago" --no-pager | \
    grep -oP '" \K[0-9]{3}' | sort | uniq -c | sort -rn
#  5432 200
#   123 304
#    45 404
#     2 500

# Contar logins SSH exitosos por usuario:
journalctl -u sshd --grep="Accepted" --since today -o cat --no-pager | \
    grep -oP 'for \K\S+' | sort | uniq -c | sort -rn
#    15 admin
#     3 deploy
```

### Procesamiento en stream (follow + jq)

```bash
# Monitorear errores en tiempo real:
journalctl -f -u myapp -o json | while read -r line; do
    PRIO=$(echo "$line" | jq -r '.PRIORITY')
    if [ "$PRIO" -le 3 ]; then
        MSG=$(echo "$line" | jq -r '.MESSAGE')
        UNIT=$(echo "$line" | jq -r '._SYSTEMD_UNIT')
        echo "$(date): ERROR en $UNIT: $MSG" >> /var/log/alerts.log
    fi
done
```

---

## Acceder a journals de otros sistemas

```bash
# Journal de otro sistema montado (diagnóstico de sistema que no arranca):
journalctl --directory=/mnt/broken-system/var/log/journal/

# Archivo de journal individual:
journalctl --file=/path/to/system.journal

# Combinar múltiples directorios (merge):
journalctl --directory=/var/log/journal/ \
           --directory=/mnt/old/var/log/journal/

# Con filtros aplicados:
journalctl --directory=/mnt/broken/var/log/journal/ -p err -b -1
```

---

## Rendimiento de consultas

```
┌─────────────────────────────────────────────────────────────────────┐
│                 ÍNDICES DEL JOURNAL                                 │
├─────────────────────────────────────────────────────────────────────┤
│                                                                     │
│  O(1) —索引 directo:                                               │
│    _SYSTEMD_UNIT, _UID, _PID, PRIORITY, SYSLOG_IDENTIFIER          │
│    → Filtrar por estos es MUY RÁPIDO                               │
│                                                                     │
│  O(log n) — búsqueda binaria:                                      │
│    --since, --until (rangos de tiempo)                             │
│    → Rápido gracias al índice temporal                              │
│                                                                     │
│  O(n) — recorrido completo:                                        │
│    --grep                                                          │
│    → LENTO en journals grandes — siempre acotar antes              │
│                                                                     │
└─────────────────────────────────────────────────────────────────────┘
```

```bash
# MAL (recorre todo el journal):
journalctl --grep="error" --no-pager

# BIEN (acota primero, luego grep):
journalctl -u nginx -p warning --since "1 hour ago" --grep="error" --no-pager
```

---

## Quick reference

```
FILTROS COMBINADOS:
  -u x -u y -u z           → OR entre unidades
  _SYSTEMD_UNIT=x _UID=y   → AND entre campos
  +                        → OR entre grupos

TIEMPO:
  --since "1 hour ago"     → relativo
  --since "2026-03-17"    → absoluto
  --since X --until Y      → rango
  --after-cursor "s=..."   → incremental

FORMATOS:
  -o short, short-precise, short-iso, short-unix
  -o verbose (todos los campos)
  -o json, json-pretty
  -o cat (solo mensaje)

RENDIMIENTO:
  Acotar por unidad/tiempo antes de --grep
  journalctl -u x -p y --since "1 hour ago" --grep "pattern"
```

---

## Ejercicios

### Ejercicio 1 — Consultas combinadas

```bash
# 1. Errores del kernel en el boot actual:
journalctl -k -p err -b --no-pager

# 2. Warnings de servicios en la última hora:
journalctl -p warning --since "1 hour ago" --no-pager | head -20

# 3. Todo de un PID específico:
PID=$(pgrep -x nginx | head -1)
journalctl _PID=$PID -n 10 --no-pager

# 4. Múltiples unidades con prioridad:
journalctl -u nginx -u php-fpm -u mysql -p err --since today --no-pager
```

### Ejercicio 2 — Formatos de salida

```bash
# Comparar la misma entrada en todos los formatos:
for fmt in short short-precise short-iso short-unix cat json json-pretty; do
    echo "=== $fmt ==="
    journalctl -n 1 -o "$fmt" --no-pager 2>/dev/null | head -5
    echo
done
```

### Ejercicio 3 — Análisis con jq

```bash
# Top 5 servicios con más errores hoy:
journalctl -p err --since today -o json --no-pager 2>/dev/null | \
    jq -r '._SYSTEMD_UNIT // .SYSLOG_IDENTIFIER // "unknown"' | \
    sort | uniq -c | sort -rn | head -5

# Top 5 IPs con más logins SSH exitosos:
journalctl -u sshd --grep="Accepted" --since today -o json --no-pager | \
    jq -r '.MESSAGE' | grep -oP '[0-9]+\.[0-9]+\.[0-9]+\.[0-9]+' | \
    sort | uniq -c | sort -rn | head -5

# Extraer mensajes de error y su timestamp:
journalctl -p err --since today -o json --no-pager | \
    jq -r '"\(._SOURCE_REALTIME_TIMESTAMP | .[0:13] | tonumber | strftime("%H:%M:%S")) \(.MESSAGE)"' | \
    head -10
```

### Ejercicio 4 — Diagnóstico de incidente

```bash
# Simular un incidente: nginx empieza a reportar errores
# 1. Timeline completo de nginx en un rango:
journalctl -u nginx --since "10 min ago" --until "5 min ago" \
    -o short-precise --no-pager

# 2. Solo errores:
journalctl -u nginx -p err --since "10 min ago" --no-pager

# 3. Correlacionar con logs del sistema:
journalctl --since "10 min ago" --until "5 min ago" \
    -p warning --no-pager | grep -iE "nginx|cpu|memory|disk"

# 4. Ver qué pasó en el kernel:
journalctl -k --since "10 min ago" --no-pager | tail -20
```

### Ejercicio 5 — Monitoreo incremental con cursor

```bash
# Script que procesa logs增量mente (solo lo nuevo):
#!/bin/bash
CURSOR_FILE="/tmp/journal_cursor"
OUTPUT_DIR="/var/log/monitor"

mkdir -p "$OUTPUT_DIR"

LAST_CURSOR=$(cat "$CURSOR_FILE" 2>/dev/null)

CMD="journalctl --since '5 min ago' -o json --no-pager"
[ -n "$LAST_CURSOR" ] && CMD="journalctl --after-cursor '$LAST_CURSOR' -o json --no-pager"

$CMD | while read -r line; do
    CURSOR=$(echo "$line" | jq -r '.__CURSOR')
    MSG=$(echo "$line" | jq -r '.MESSAGE')
    PRIO=$(echo "$line" | jq -r '.PRIORITY')
    UNIT=$(echo "$line" | jq -r '._SYSTEMD_UNIT // .SYSLOG_IDENTIFIER')

    echo "$CURSOR" > "$CURSOR_FILE"

    if [ "$PRIO" -le 3 ]; then
        echo "$(date): [$UNIT] $MSG" >> "$OUTPUT_DIR/errors.log"
    fi
done
```
