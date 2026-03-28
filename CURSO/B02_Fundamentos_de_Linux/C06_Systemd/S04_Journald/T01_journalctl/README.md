# T01 — journalctl

## Qué es el journal

systemd-journald es el servicio de logging de systemd. A diferencia de syslog
(texto plano en archivos), el journal almacena los logs en un formato binario
estructurado con campos indexados:

```bash
# Ver todos los logs:
journalctl

# El journal captura:
# - stdout/stderr de todos los servicios gestionados por systemd
# - Mensajes del kernel (como dmesg)
# - Mensajes de syslog (de aplicaciones que usan syslog())
# - Logs de systemd mismo (arranque, parada de unidades, etc.)

# Por defecto, journalctl muestra los logs en un pager (less)
# con los más antiguos arriba y los más recientes abajo
```

## Filtros básicos

### Por unidad (servicio)

```bash
# Logs de un servicio específico:
journalctl -u nginx.service
journalctl -u nginx            # .service es implícito
journalctl -u sshd -u nginx    # múltiples unidades

# Logs del kernel:
journalctl -k
# Equivale a: journalctl _TRANSPORT=kernel
# Similar a: dmesg (pero con timestamps completos y persistencia)
```

### Por tiempo

```bash
# Desde una fecha/hora:
journalctl --since "2024-03-17 10:00:00"
journalctl --since "1 hour ago"
journalctl --since "today"
journalctl --since "yesterday"

# Hasta una fecha/hora:
journalctl --until "2024-03-17 12:00:00"
journalctl --until "30 min ago"

# Rango:
journalctl --since "2024-03-17 08:00" --until "2024-03-17 12:00"

# Formatos de tiempo aceptados:
# "YYYY-MM-DD HH:MM:SS"
# "today", "yesterday", "tomorrow"
# "N hours ago", "N min ago", "N days ago"
# "HH:MM" (hoy a esa hora)
```

### Por prioridad

```bash
# syslog define 8 niveles de prioridad:
# 0 = emerg    — sistema inutilizable
# 1 = alert    — acción inmediata necesaria
# 2 = crit     — condición crítica
# 3 = err      — error
# 4 = warning  — advertencia
# 5 = notice   — normal pero significativo
# 6 = info     — informacional
# 7 = debug    — depuración

# Filtrar por prioridad (incluye el nivel indicado y todos los más severos):
journalctl -p err              # err + crit + alert + emerg
journalctl -p warning          # warning + err + crit + alert + emerg
journalctl -p 3                # equivale a -p err

# Rango de prioridades:
journalctl -p err..warning     # solo err y warning (no crit ni emerg)
```

### Por boot

```bash
# Logs del boot actual:
journalctl -b
journalctl -b 0                # equivalente

# Logs del boot anterior:
journalctl -b -1

# Dos boots atrás:
journalctl -b -2

# Listar todos los boots registrados:
journalctl --list-boots
# -3 abc123 Mon 2024-03-14 08:00 — Mon 2024-03-14 23:59
# -2 def456 Tue 2024-03-15 08:00 — Tue 2024-03-15 23:59
# -1 ghi789 Wed 2024-03-16 08:00 — Wed 2024-03-16 23:59
#  0 jkl012 Thu 2024-03-17 08:00 — Thu 2024-03-17 15:30

# Boot específico por ID:
journalctl -b abc123
```

### Por PID, UID, GID

```bash
# Por PID:
journalctl _PID=1234

# Por UID del proceso que generó el log:
journalctl _UID=1000

# Por GID:
journalctl _GID=1000

# Por ejecutable:
journalctl _COMM=sshd
journalctl _EXE=/usr/sbin/sshd
```

## Combinación de filtros

Los filtros se combinan con AND implícito:

```bash
# Logs de nginx, solo errores, del boot actual:
journalctl -u nginx -p err -b

# Logs del kernel, solo warnings y superiores, de hoy:
journalctl -k -p warning --since today

# Logs de sshd del boot anterior:
journalctl -u sshd -b -1

# Errores de cualquier servicio en la última hora:
journalctl -p err --since "1 hour ago"
```

Para combinar con OR, usar el operador `+`:

```bash
# Logs de nginx O de sshd:
journalctl -u nginx -u sshd
# Múltiples -u son OR entre sí

# OR con campos diferentes:
journalctl _COMM=nginx + _COMM=sshd
```

## Opciones de salida

### Seguimiento en vivo (follow)

```bash
# Como tail -f para el journal:
journalctl -f
journalctl -f -u nginx          # follow solo nginx
journalctl -f -p err            # follow solo errores

# Combinar con filtros:
journalctl -f -u nginx -u php-fpm -p warning
```

### Número de líneas

```bash
# Últimas N líneas (como tail -n):
journalctl -n 50                 # últimas 50 líneas
journalctl -n 20 -u nginx       # últimas 20 de nginx

# Sin pager (para scripts o redirección):
journalctl --no-pager -n 100

# Sin hostname en la salida:
journalctl --no-hostname -n 10
```

### Orden inverso

```bash
# Más recientes primero (como tail):
journalctl -r
journalctl -r -n 10 -u sshd     # últimas 10 de sshd, más recientes primero
```

### Formatos de salida

```bash
# short (default) — similar a syslog:
journalctl -o short
# Mar 17 10:30:00 server sshd[1234]: Accepted publickey for user...

# short-precise — con microsegundos:
journalctl -o short-precise
# Mar 17 10:30:00.123456 server sshd[1234]: ...

# short-iso — timestamp ISO 8601:
journalctl -o short-iso
# 2024-03-17T10:30:00+0000 server sshd[1234]: ...

# verbose — todos los campos del journal entry:
journalctl -o verbose
# Sun 2024-03-17 10:30:00.123456 UTC [s=abc123;i=456;b=def789;m=123456;t=60a...]
#     _BOOT_ID=def789...
#     _MACHINE_ID=abc123...
#     _HOSTNAME=server
#     PRIORITY=6
#     _UID=0
#     _GID=0
#     _COMM=sshd
#     _EXE=/usr/sbin/sshd
#     _PID=1234
#     MESSAGE=Accepted publickey for user...
#     _TRANSPORT=syslog
#     SYSLOG_IDENTIFIER=sshd
#     ...

# json — cada entry como un objeto JSON:
journalctl -o json -n 1
# {"__CURSOR":"s=abc...","__REALTIME_TIMESTAMP":"1710669000123456",...}

# json-pretty — JSON formateado:
journalctl -o json-pretty -n 1

# cat — solo el mensaje, sin metadata:
journalctl -o cat -u nginx -n 5
# Solo los mensajes, sin timestamp ni hostname
```

## Filtros avanzados con campos

El journal almacena cada log como un conjunto de campos clave-valor. Se
puede filtrar por cualquier campo:

```bash
# Ver todos los campos disponibles:
journalctl -o verbose -n 1

# Campos comunes:
# _PID           — PID del proceso
# _UID           — UID del usuario
# _GID           — GID del grupo
# _COMM          — nombre del comando
# _EXE           — ruta del ejecutable
# _CMDLINE       — línea de comandos completa
# _HOSTNAME      — hostname del sistema
# _TRANSPORT     — cómo llegó el log (journal, syslog, kernel, stdout)
# PRIORITY       — nivel de prioridad (0-7)
# SYSLOG_FACILITY — facility de syslog
# SYSLOG_IDENTIFIER — identificador de syslog
# _SYSTEMD_UNIT  — unidad de systemd que generó el log
# _SYSTEMD_CGROUP — cgroup del proceso
# MESSAGE        — el mensaje de log
# MESSAGE_ID     — ID estándar del mensaje (para mensajes conocidos)

# Filtrar por campo:
journalctl _TRANSPORT=kernel
journalctl SYSLOG_IDENTIFIER=sudo
journalctl _SYSTEMD_UNIT=docker.service
```

```bash
# Ver qué valores tiene un campo:
journalctl -F _COMM
# sshd
# nginx
# systemd
# cron
# ...

journalctl -F PRIORITY
# 3
# 4
# 5
# 6

journalctl -F _SYSTEMD_UNIT | head -10
```

## Uso en scripts

```bash
# Verificar si un servicio tiene errores recientes:
if journalctl -u myapp -p err --since "5 min ago" -q --no-pager | grep -q .; then
    echo "myapp tiene errores recientes" >&2
fi
# -q = quiet (no muestra "-- No entries --" cuando no hay resultados)

# Extraer logs en JSON para procesamiento:
journalctl -u nginx -o json --since "1 hour ago" --no-pager | \
    jq -r 'select(.PRIORITY == "3") | .MESSAGE'

# Contar errores por servicio:
journalctl -p err --since today -o json --no-pager | \
    jq -r '._SYSTEMD_UNIT // "unknown"' | sort | uniq -c | sort -rn

# Monitorear y actuar:
journalctl -f -u myapp -o json | while read -r line; do
    PRIORITY=$(echo "$line" | jq -r '.PRIORITY')
    if [[ "$PRIORITY" -le 3 ]]; then
        MSG=$(echo "$line" | jq -r '.MESSAGE')
        echo "ALERTA: $MSG" | mail -s "myapp error" admin@example.com
    fi
done
```

## Espacio en disco

```bash
# Ver cuánto espacio usa el journal:
journalctl --disk-usage
# Archived and active journals take up 256.0M in the file system.

# Limpiar por tamaño:
sudo journalctl --vacuum-size=100M
# Elimina journals antiguos hasta que el total sea ≤ 100MB

# Limpiar por tiempo:
sudo journalctl --vacuum-time=7d
# Elimina journals de más de 7 días

# Limpiar por número de archivos:
sudo journalctl --vacuum-files=5
# Mantiene solo los 5 archivos de journal más recientes

# Verificar integridad:
journalctl --verify
```

---

## Ejercicios

### Ejercicio 1 — Filtros básicos

```bash
# 1. Ver los logs del boot actual, solo errores:
journalctl -b -p err --no-pager

# 2. Ver las últimas 20 líneas de sshd:
journalctl -u sshd -n 20 --no-pager

# 3. Ver los logs del kernel de la última hora:
journalctl -k --since "1 hour ago" --no-pager
```

### Ejercicio 2 — Formatos de salida

```bash
# Comparar la misma entrada en diferentes formatos:
journalctl -n 1 -o short --no-pager; echo "---"
journalctl -n 1 -o verbose --no-pager; echo "---"
journalctl -n 1 -o json-pretty --no-pager
```

### Ejercicio 3 — Explorar campos

```bash
# ¿Qué comandos han generado logs?
journalctl -F _COMM --no-pager | head -20

# ¿Cuántos boots tiene registrados el journal?
journalctl --list-boots --no-pager | wc -l
```
