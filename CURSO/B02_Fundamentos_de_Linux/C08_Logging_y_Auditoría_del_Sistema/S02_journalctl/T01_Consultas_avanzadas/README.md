# T01 — Consultas avanzadas

## Repaso rápido

En C06/S04 cubrimos los filtros básicos de journalctl. Aquí profundizamos
en las técnicas de consulta avanzadas para escenarios reales de
diagnóstico y monitoreo.

```bash
# Filtros básicos (repaso):
journalctl -u nginx            # por unidad
journalctl -p err              # por prioridad
journalctl -b -1               # por boot
journalctl --since "1 hour ago"  # por tiempo
journalctl -k                  # kernel
```

## Combinar filtros complejos

### AND implícito entre filtros

```bash
# Múltiples filtros se combinan con AND:
journalctl -u sshd -p err -b --since "2 hours ago"
# sshd AND err+ AND boot actual AND últimas 2 horas

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

# Útil para ver toda la pila de una aplicación web:
journalctl -u nginx -u php-fpm -u mysql -p warning --since today
# Warnings+ de cualquiera de los tres, desde hoy
```

### OR con el operador +

```bash
# El + combina grupos de campos con OR:
journalctl _SYSTEMD_UNIT=sshd.service + _COMM=sudo
# Mensajes de sshd.service OR mensajes del comando sudo

# Dentro de cada grupo, los campos son AND
# Entre grupos separados por +, es OR:
journalctl _SYSTEMD_UNIT=nginx.service _PID=1234 + _COMM=curl
# (nginx.service AND PID 1234) OR (comando curl)
```

### Filtros por campo con patrones

```bash
# Los campos soportan wildcards con globbing:
journalctl _SYSTEMD_UNIT=nginx*
# Todas las unidades que empiezan con "nginx"

# Listar valores disponibles de un campo:
journalctl -F _SYSTEMD_UNIT | grep -i docker
# docker.service
# docker.socket

journalctl -F _COMM | head -20
# Lista de los 20 comandos más recientes que han generado logs
```

## Filtros de tiempo avanzados

### Rangos precisos

```bash
# Rango exacto:
journalctl --since "2026-03-17 08:00:00" --until "2026-03-17 12:00:00"

# Con zona horaria:
journalctl --since "2026-03-17 08:00:00 UTC" --until "2026-03-17 12:00:00 UTC"

# Relativos combinados:
journalctl --since "3 hours ago" --until "1 hour ago"
# Ventana de 2 horas que terminó hace 1 hora

# Solo un segundo específico (útil para correlacionar):
journalctl --since "2026-03-17 10:30:45" --until "2026-03-17 10:30:46"
```

### Por cursor

```bash
# Cada entrada del journal tiene un cursor único (posición):
journalctl -n 1 -o json | jq -r '.__CURSOR'
# s=abc123;i=456;b=def789;m=1234567890;t=60abcdef12345;x=abcdef

# Leer desde un cursor (retomar donde se dejó):
journalctl --after-cursor="s=abc123;i=456;b=def789;..."
# Todas las entradas DESPUÉS de esa posición

journalctl --cursor="s=abc123;i=456;b=def789;..."
# Desde esa posición inclusive

# Caso de uso: un script de monitoreo que procesa logs incrementalmente
CURSOR_FILE="/var/lib/mymonitor/cursor"
if [ -f "$CURSOR_FILE" ]; then
    CURSOR=$(cat "$CURSOR_FILE")
    journalctl --after-cursor="$CURSOR" -o json --no-pager
else
    journalctl --since "5 min ago" -o json --no-pager
fi | while read -r line; do
    # Procesar cada línea
    echo "$line" | jq -r '.__CURSOR' > "$CURSOR_FILE"
done
```

## Formatos de salida avanzados

### JSON para procesamiento automático

```bash
# JSON compacto (una línea por entrada):
journalctl -u nginx -o json --no-pager -n 5
# {"__CURSOR":"s=...","PRIORITY":"6","_HOSTNAME":"server","MESSAGE":"GET /index.html","_PID":"1234",...}

# JSON formateado:
journalctl -u nginx -o json-pretty -n 1
# {
#     "__CURSOR" : "s=...",
#     "PRIORITY" : "6",
#     "_HOSTNAME" : "server",
#     "MESSAGE" : "GET /index.html",
#     "_PID" : "1234",
#     ...
# }

# Extraer solo el mensaje con jq:
journalctl -u nginx -o json --no-pager -n 5 | jq -r '.MESSAGE'

# Filtrar por contenido del mensaje:
journalctl -u nginx -o json --no-pager | \
    jq -r 'select(.MESSAGE | test("500|502|503")) | .MESSAGE'
# Solo mensajes que contengan 500, 502 o 503
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

# verbose muestra TODOS los campos, es ideal para:
# - Entender qué campos tiene un mensaje
# - Descubrir campos para filtrar
# - Depurar problemas con reglas de filtrado
```

### Otros formatos útiles

```bash
# cat — solo el mensaje, sin metadata:
journalctl -u nginx -o cat -n 5
# GET /index.html 200
# GET /style.css 200
# POST /api/data 201
# Ideal para alimentar scripts que solo necesitan el texto

# short-iso — timestamps ISO 8601:
journalctl -o short-iso -n 3
# 2026-03-17T15:30:00+0000 server sshd[1234]: ...
# Parseables por herramientas automatizadas

# short-precise — con microsegundos:
journalctl -o short-precise -n 3
# Mar 17 15:30:00.123456 server sshd[1234]: ...
# Para correlacionar eventos con precisión

# short-unix — timestamp Unix epoch:
journalctl -o short-unix -n 3
# 1710689400.123456 server sshd[1234]: ...
# Para procesamiento numérico de tiempos

# export — formato binario de exportación:
journalctl -o export -n 1
# Para systemd-journal-remote o copias binarias
```

## Consultas de análisis

### Contar errores por servicio

```bash
journalctl -p err --since today -o json --no-pager | \
    jq -r '._SYSTEMD_UNIT // .SYSLOG_IDENTIFIER // "unknown"' | \
    sort | uniq -c | sort -rn | head -10
#  45 nginx.service
#  12 php-fpm.service
#   3 sshd.service
#   1 unknown
```

### Encontrar el primer error después de un evento

```bash
# ¿Qué pasó justo después del deploy?
journalctl -u myapp --since "2026-03-17 14:00" -p err -n 1
# Primer error después de las 14:00

# ¿Qué pasó justo antes de que el servicio fallara?
journalctl -u myapp --until "2026-03-17 14:05" -n 20 --no-pager
# Últimas 20 líneas antes de las 14:05
```

### Timeline de un incidente

```bash
# Ver TODO lo que pasó en un rango de 5 minutos:
journalctl --since "2026-03-17 14:00" --until "2026-03-17 14:05" \
    --no-pager -o short-precise

# Solo los errores y warnings en ese rango:
journalctl --since "2026-03-17 14:00" --until "2026-03-17 14:05" \
    -p warning --no-pager -o short-precise

# Con contexto de múltiples servicios:
journalctl --since "2026-03-17 14:00" --until "2026-03-17 14:05" \
    -u nginx -u php-fpm -u mysql --no-pager -o short-precise
```

### Mensajes del kernel relacionados con hardware

```bash
# OOM (Out of Memory):
journalctl -k --grep="Out of memory\|oom-killer\|Killed process"
# --grep filtra por regex dentro del MESSAGE

# Errores de disco:
journalctl -k --grep="I/O error\|EXT4-fs error\|XFS.*error"

# Errores de red:
journalctl -k --grep="link is not ready\|carrier lost\|NIC Link"
```

### Grep dentro del journal

```bash
# --grep filtra por regex en el campo MESSAGE:
journalctl --grep="failed\|error\|timeout" -p warning --since today
# Mensajes warning+ que contengan failed, error o timeout

# Case insensitive:
journalctl --grep="connection refused" --case-sensitive=no -u nginx

# Grep con contexto (no soportado directamente, pero se puede simular):
journalctl -u myapp --no-pager | grep -B2 -A2 "FATAL"
```

## Uso en scripts de monitoreo

### Verificar si un servicio tiene errores recientes

```bash
#!/bin/bash
SERVICE="$1"
MINUTES="${2:-5}"

ERRORS=$(journalctl -u "$SERVICE" -p err --since "$MINUTES min ago" \
    -q --no-pager 2>/dev/null | wc -l)

if [ "$ERRORS" -gt 0 ]; then
    echo "ALERTA: $SERVICE tiene $ERRORS errores en los últimos $MINUTES minutos"
    journalctl -u "$SERVICE" -p err --since "$MINUTES min ago" \
        --no-pager -o short-precise
    exit 1
fi
echo "OK: $SERVICE sin errores recientes"
```

### Extraer métricas de logs

```bash
# Contar requests por código de respuesta (nginx en journal):
journalctl -u nginx -o cat --since "1 hour ago" --no-pager | \
    grep -oP '" \K[0-9]{3}' | sort | uniq -c | sort -rn
#  5432 200
#   123 304
#    45 404
#     2 500

# Contar logins SSH exitosos por usuario:
journalctl -u sshd --grep="Accepted" --since today -o cat --no-pager | \
    grep -oP 'for \K\S+' | sort | uniq -c | sort -rn
#   15 admin
#    3 deploy
```

### Procesamiento en stream (follow + jq)

```bash
# Monitorear errores en tiempo real y actuar:
journalctl -f -u myapp -o json | while read -r line; do
    PRIO=$(echo "$line" | jq -r '.PRIORITY')
    if [ "$PRIO" -le 3 ]; then
        MSG=$(echo "$line" | jq -r '.MESSAGE')
        UNIT=$(echo "$line" | jq -r '._SYSTEMD_UNIT')
        echo "$(date): ERROR en $UNIT: $MSG" >> /var/log/alerts.log
    fi
done
```

## Acceder a journals de otros sistemas

```bash
# Leer un journal de otro sistema montado:
journalctl --directory=/mnt/broken-system/var/log/journal/
# Útil para diagnóstico de sistemas que no arrancan

# Leer un archivo de journal individual:
journalctl --file=/path/to/system.journal

# Merge de múltiples directorios:
journalctl --directory=/var/log/journal/ --directory=/mnt/old/var/log/journal/
```

## Rendimiento de consultas

```bash
# El journal está indexado — las consultas por campos son rápidas:
# - Filtrar por _SYSTEMD_UNIT es O(1) (índice directo)
# - Filtrar por PRIORITY es O(1)
# - Filtrar por tiempo es O(log n) (búsqueda binaria)

# --grep es LENTO — recorre todos los mensajes:
# Evitar --grep en journals grandes sin acotar con otros filtros

# Buena práctica: acotar primero, grep después:
# LENTO:
journalctl --grep="error" --no-pager
# RÁPIDO:
journalctl -u nginx -p warning --since "1 hour ago" --grep="error" --no-pager
```

---

## Ejercicios

### Ejercicio 1 — Consultas combinadas

```bash
# 1. Errores del kernel en el boot actual:
journalctl -k -p err -b --no-pager

# 2. Warnings de cualquier servicio en la última hora:
journalctl -p warning --since "1 hour ago" --no-pager | head -20

# 3. Todo lo relacionado con un PID específico:
PID=$(pgrep -x systemd | head -1)
journalctl _PID=$PID -n 10 --no-pager
```

### Ejercicio 2 — Formatos de salida

```bash
# Comparar la misma entrada en 4 formatos:
for fmt in short short-precise verbose json-pretty; do
    echo "=== $fmt ==="
    journalctl -n 1 -o "$fmt" --no-pager 2>/dev/null
    echo
done
```

### Ejercicio 3 — Análisis con jq

```bash
# Top 5 servicios con más errores hoy:
journalctl -p err --since today -o json --no-pager 2>/dev/null | \
    jq -r '._SYSTEMD_UNIT // .SYSLOG_IDENTIFIER // "unknown"' | \
    sort | uniq -c | sort -rn | head -5
```
