# T01 — journalctl

> **Objetivo:** Dominar `journalctl` para consultar, filtrar y analizar los logs de systemd-journald. Aprender a usar filtros avanzados, formatos de salida y automatización con scripts.

## Qué es el journal

**systemd-journald** es el servicio de logging de systemd. A diferencia de syslog tradicional (archivos de texto plano), el journal almacena logs en un formato **binario estructurado** con campos indexados, lo que permite consultas extremadamente rápidas y filtrado preciso.

```
┌─────────────────────────────────┐          ┌─────────────────────────────────┐
│       syslog tradicional        │          │         systemd journal         │
├─────────────────────────────────┤          ├─────────────────────────────────┤
│ Archivos de texto plano         │          │ Binario indexado por campos     │
│ /var/log/syslog, auth.log       │          │ /var/log/journal/ (persistente) │
│ grep + awk para filtrar          │          │ journalctl para consultar       │
│ Sin estructura formal           │          │ Campos clave=valor estructurados│
│ Difícil de correlacionar        │          │ Cada entry tiene docenas de     │
│ entre servicios                  │          │ campos (PID, UID, TRANSPORT...) │
└─────────────────────────────────┘          └─────────────────────────────────┘
```

### Qué captura el journal

```bash
# Ver todos los logs (pager interactivo con less):
journalctl

# El journal unifica logs de múltiples fuentes:
# ──────────────────────────────────────────────
# stdout/stderr        → servicios gestionados por systemd
# syslog               → aplicaciones que usan syslog()
# kernel (dmesg)       → mensajes del kernel
# systemd interno      → arranque/parada de unidades
# /dev/kmsg            → logging del kernel en tiempo real
```

---

## Filtros básicos

### Por unidad (servicio)

```bash
# Logs de un servicio específico:
journalctl -u nginx.service
journalctl -u nginx              # .service es implícito

# Múltiples unidades (OR entre ellas):
journalctl -u sshd -u nginx

# Logs del kernel (equivale a dmesg, pero con timestamps completos):
journalctl -k
journalctl _TRANSPORT=kernel     # forma larga equivalente

# ¿Qué unidades han generado logs?
journalctl -F _SYSTEMD_UNIT | head -20
```

### Por tiempo

```bash
# Formatos flexibles — todos estos son válidos:
journalctl --since "2024-03-17 10:00:00"
journalctl --since "1 hour ago"
journalctl --since "today"
journalctl --since "yesterday"
journalctl --since "tomorrow"
journalctl --since "2 days ago"
journalctl --since "09:00"              # hoy a las 09:00
journalctl --since "2024-03-17" --until "2024-03-18"

# Rangos:
journalctl --since "2024-03-17 08:00" --until "2024-03-17 12:00"
```

### Por prioridad (severidad)

syslog define **8 niveles de prioridad** (menor número = mayor severidad):

| Nivel | Nombre  | Descripción                          | Uso típico                    |
|-------|---------|--------------------------------------|-------------------------------|
| 0     | `emerg` | Sistema inutilizable                 | Kernel panic                  |
| 1     | `alert` | Acción inmediata necesaria           | Corruption обнаружен          |
| 2     | `crit`  | Condición crítica                    | Hardware error detectado      |
| 3     | `err`   | Error de operación                   | "Connection refused"          |
| 4     | `warning` | Situación anómala                  | "Port already in use"         |
| 5     | `notice`| Normal pero significativo            | Login exitoso de root         |
| 6     | `info`  | Informacional                        | Servicio iniciado/parado      |
| 7     | `debug` | Depuración                           | Logs de desarrollo            |

```bash
# Filtrar por prioridad (incluye el nivel indicado Y los más severos):
journalctl -p err              # err + crit + alert + emerg (3,2,1,0)
journalctl -p warning          # warning + err + crit + alert + emerg (4,3,2,1,0)
journalctl -p 3                # equivalente a -p err

# Rango de prioridades específicas:
journalctl -p err..warning     # solo err y warning (3 y 4, no 2 ni 0)
journalctl -p info..notice     # info y notice (6 y 5)
```

### Por boot

```bash
# Boot actual:
journalctl -b
journalctl -b 0                # equivalente

# Boot anterior:
journalctl -b -1

# Dos boots atrás:
journalctl -b -2

# Boot específico por ID:
journalctl -b abc123def456

# Listar todos los boots registrados:
journalctl --list-boots
# -3 abc123...  Mon 2024-03-14 08:00 — Mon 2024-03-14 23:59 (2 boots)
# -2 def456...  Tue 2024-03-15 08:00 — Tue 2024-03-15 23:59 (2 boots)
# -1 ghi789...  Wed 2024-03-16 08:00 — Wed 2024-03-16 23:59 (2 boots)
#  0 jkl012...  Thu 2024-03-17 08:00 — *currently running*
```

### Por proceso (PID, UID, GID, ejecutable)

```bash
# Por PID:
journalctl _PID=1234

# Por UID del proceso:
journalctl _UID=1000

# Por GID:
journalctl _GID=1000

# Por nombre del comando:
journalctl _COMM=sshd

# Por ruta del ejecutable:
journalctl _EXE=/usr/sbin/sshd

# Por línea de comandos completa:
journalctl _CMDLINE='/usr/sbin/sshd -D'
```

---

## Combinación de filtros

Los filtros se combinan con **AND implícito**:

```bash
# nginx + errores + boot actual:
journalctl -u nginx -p err -b

# kernel + warnings y superiores + última hora:
journalctl -k -p warning --since "1 hour ago"

# sshd + boot anterior:
journalctl -u sshd -b -1

# Múltiples -u son OR entre sí:
journalctl -u nginx -u sshd -u php-fpm
```

Para **OR explícito** con campos diferentes, usar el operador `+`:

```bash
# Logs de nginx O de sshd (forma avanzada):
journalctl _COMM=nginx + _COMM=sshd

# Errores de cualquier servicio de los últimos 30 minutos:
journalctl -p err --since "30 min ago"
```

---

## Opciones de salida

### Seguimiento en vivo (follow)

```bash
# Como tail -f — seguir los logs en tiempo real:
journalctl -f

# Follow + unidad específica:
journalctl -f -u nginx

# Follow + prioridad:
journalctl -f -p err

# Follow + múltiples unidades y prioridad:
journalctl -f -u nginx -u php-fpm -p warning
```

### Control de volumen

```bash
# Últimas N líneas (como tail -n):
journalctl -n 50
journalctl -n 20 -u nginx

# Reversa: más recientes primero:
journalctl -r
journalctl -r -n 10 -u sshd

# Sin pager (para scripts o piping):
journalctl --no-pager -n 100

# Sin hostname:
journalctl --no-hostname -n 10

# quiet: no imprime "-- No entries --" cuando no hay resultados:
journalctl -u nonexistent -q
```

### Formatos de salida

```bash
# short (default) — estilo syslog clásico:
journalctl -o short
# Mar 17 10:30:00 server sshd[1234]: Accepted publickey for user

# short-precise — con microsegundos:
journalctl -o short-precise
# Mar 17 10:30:00.123456 server sshd[1234]: ...

# short-iso — timestamps ISO 8601:
journalctl -o short-iso
# 2024-03-17T10:30:00+0000 server sshd[1234]: ...

# short-full — con todos los campos de fecha:
journalctl -o short-full
# Thu 2024-03-17 10:30:00.123456 server sshd[1234]: ...

# verbose — TODOS los campos del entry (ideal para debugging):
journalctl -o verbose -n 1
# Thu 2024-03-17 10:30:00.123456 UTC [s=abc123;i=456;b=def789;m=123456;...]
#     _BOOT_ID=def789...
#     _MACHINE_ID=abc123...
#     _HOSTNAME=server
#     PRIORITY=6
#     _UID=0
#     _GID=0
#     _COMM=sshd
#     _EXE=/usr/sbin/sshd
#     _PID=1234
#     MESSAGE=Accepted publickey for user
#     _TRANSPORT=syslog
#     SYSLOG_IDENTIFIER=sshd
#     ...

# json — un objeto JSON por línea (ideal para processing):
journalctl -o json -n 1
# {"__CURSOR":"s=abc...","__REALTIME_TIMESTAMP":"1710669000123456",...}

# json-pretty — JSON formateado (legible):
journalctl -o json-pretty -n 1

# cat — SOLO el mensaje, sin metadata:
journalctl -o cat -u nginx -n 5
# Accepted publickey for user from 192.168.1.1 port 52321
# Connection closed from 192.168.1.1 port 52321
```

---

## Filtros avanzados con campos

Cada entrada del journal es un conjunto de **campos clave-valor**. Se puede filtrar por cualquier campo:

```bash
# Ver todos los campos de un entry:
journalctl -o verbose -n 1

# Campos más comunes:
# ──────────────────────────────────────────────
# _PID              PID del proceso
# _UID              UID del usuario
# _GID              GID del grupo
# _COMM             Nombre del comando
# _EXE              Ruta absoluta del ejecutable
# _CMDLINE          Línea de comandos completa
# _HOSTNAME         Hostname del sistema
# _TRANSPORT        Origen: journal, syslog, kernel, stdout, driver
# PRIORITY          Nivel 0-7
# SYSLOG_FACILITY   Facility de syslog (local0-local7, auth, daemon...)
# SYSLOG_IDENTIFIER Nombre del programa (como aparece en syslog)
# _SYSTEMD_UNIT     Unidad de systemd que gobierna el proceso
# _SYSTEMD_CGROUP   Cgroup del proceso en systemd
# MESSAGE           El mensaje de log
# MESSAGE_ID        ID único del mensaje (para mensajes estructurados)
# _SOURCE_REALTIME_TIMESTAMP  Timestamp de cuando se generó el mensaje
```

```bash
# Filtrar por cualquier campo:
journalctl _TRANSPORT=kernel
journalctl SYSLOG_IDENTIFIER=sudo
journalctl _SYSTEMD_UNIT=docker.service
journalctl _HOSTNAME=server1

# Combinar campos:
journalctl _COMM=sshd _HOSTNAME=server1

# Ver todos los valores únicos que tiene un campo:
journalctl -F _COMM
journalctl -F PRIORITY
journalctl -F _SYSTEMD_UNIT
```

### MESSAGE_ID — Mensajes estructurados estándar

Algunos mensajes tienen un `MESSAGE_ID` único que permite identificarlos sin depender del texto:

```bash
# Buscar todos los mensajes de tipo "service start":
journalctl MESSAGE_ID=8d45620c3f5a49f09f2e2a3d4f5e6b7a

# Los MESSAGE_ID estándar de systemd incluyen:
# 8d45620c3f5a49f09f2e2a3d4f5e6b7a → service start
# b07f2cae8c6a40b5a2f4e3c1d0b7e9f6 → service stop
# 8d5e3f1a0b2c4d5e6f7a8b9c0d1e2f3a → job start
```

---

## Uso en scripts

```bash
# Verificar si un servicio tiene errores recientes:
if journalctl -u myapp -p err --since "5 min ago" -q --no-pager | grep -q .; then
    echo "ERROR: myapp tiene errores recientes" >&2
    # actuar: notificar, reiniciar, etc.
fi
# Flags útiles para scripts:
# -q  = quiet (no imprime "-- No entries --")
# --no-pager = salida directa (sin less)
# -n 0 = no mostrar líneas (solo verificar existencia)

# Extraer mensajes de error en JSON y procesarlos con jq:
journalctl -u nginx -o json --since "1 hour ago" --no-pager | \
    jq -r 'select(.PRIORITY == "3") | .MESSAGE'

# Contar errores por servicio (boot actual):
journalctl -p err -b -o json --no-pager | \
    jq -r '._SYSTEMD_UNIT // "unknown"' | \
    sort | uniq -c | sort -rn

# Pipeline de monitoreo en tiempo real:
journalctl -f -u myapp -o json | while read -r line; do
    PRIORITY=$(echo "$line" | jq -r '.PRIORITY')
    case "$PRIORITY" in
        0|1|2|3)
            MSG=$(echo "$line" | jq -r '.MESSAGE')
            echo "[ALERTA] $MSG"
            ;;
    esac
done

# Parsear logs de autenticación (formato json):
journalctl -u sshd -o json-pretty --since today --no-pager | \
    jq -r 'select(.SYSLOG_IDENTIFIER=="sshd") | .MESSAGE'
```

---

## Espacio en disco

```bash
# Ver uso actual:
journalctl --disk-usage
# Archived and active journals take up 256.0M in the file system.

# Limpiar por tamaño (mantener ≤ 100MB):
sudo journalctl --vacuum-size=100M

# Limpiar por tiempo (solo últimos 7 días):
sudo journalctl --vacuum-time=7d

# Limpiar por número de archivos (solo últimos 3 boots):
sudo journalctl --vacuum-files=3

# Verificar integridad (detecta archivos corruptos):
journalctl --verify
# Si todo está bien:    passed
# Si hay corruption:   fail (bytes corruptos)

# Ver el límite configurado (en /etc/systemd/journald.conf):
journalctl -F SystemMaxUse
```

### Configuración de retención (journald.conf)

```bash
# /etc/systemd/journald.conf
# [Journal]
# SystemMaxUse=500M        # máximo espacio en disco
# SystemMaxFileSize=50M    # máximo tamaño por archivo
# MaxRetentionSec=30day    # máxima retención
# Storage=persistent       # persistent|volatile|auto|none
```

```bash
# Ver valores efectivos (los que journald está usando):
sudo journalctl -F SystemMaxUse
sudo journalctl -F SystemMaxFileSize
```

---

## Quick reference — Patrones comunes

| Objetivo | Comando |
|----------|---------|
| Ver todo | `journalctl --no-pager` |
| Errors del boot actual | `journalctl -b -p err --no-pager` |
| Seguir en vivo | `journalctl -f` |
| Últimas 50 líneas | `journalctl -n 50 --no-pager` |
| Logs de nginx (errores) | `journalctl -u nginx -p err` |
| Logs de nginx (últimas 20) | `journalctl -u nginx -n 20 --no-pager` |
| Boot anterior | `journalctl -b -1 --no-pager` |
| Rango de fechas | `journalctl --since "2024-03-01" --until "2024-03-02"` |
| Solo mensajes | `journalctl -u nginx -o cat` |
| JSON para procesar | `journalctl -u nginx -o json` |
| Bytes usados | `journalctl --disk-usage` |
| Limpiar >7 días | `sudo journalctl --vacuum-time=7d` |

---

## Ejercicios

### Ejercicio 1 — Filtros básicos

```bash
# 1. Ver errores (err+) del boot actual:
journalctl -b -p err --no-pager

# 2. Ver las últimas 20 líneas de sshd:
journalctl -u sshd -n 20 --no-pager

# 3. Ver logs del kernel de la última hora:
journalctl -k --since "1 hour ago" --no-pager

# 4. ¿Qué servicios tienen errores hoy?
journalctl -p err --since today -o json --no-pager | \
    jq -r '._SYSTEMD_UNIT' | sort | uniq -c | sort -rn
```

### Ejercicio 2 — Formatos de salida

```bash
# Comparar la misma entrada en 4 formatos:
echo "=== short ==="; journalctl -n 1 -o short --no-pager
echo "=== short-precise ==="; journalctl -n 1 -o short-precise --no-pager
echo "=== verbose ==="; journalctl -n 1 -o verbose --no-pager
echo "=== json-pretty ==="; journalctl -n 1 -o json-pretty --no-pager

# ¿Cuántos campos tiene un entry verbose?
journalctl -n 1 -o verbose --no-pager | grep "^#" | wc -l
```

### Ejercicio 3 — Exploración de campos

```bash
# ¿Qué comandos han escrito en el journal?
journalctl -F _COMM --no-pager | head -20

# ¿Cuántos boots tiene registrados?
journalctl --list-boots --no-pager | wc -l

# ¿Qué valores tiene PRIORITY?
journalctl -F PRIORITY --no-pager

# ¿Qué MESSAGE_ID únicos hay en los últimos 100 entries?
journalctl -n 100 -o json --no-pager | jq -r '.MESSAGE_ID' | sort -u
```

### Ejercicio 4 — Diagnóstico de un servicio

```bash
# Hypothetical: nginx no responde. Hacer diagnosis:
# 1. ¿Hay errores en nginx?
journalctl -u nginx -p err --since "1 hour ago" --no-pager

# 2. ¿nginx arrancó correctamente?
journalctl -u nginx --since today --no-pager | grep -E "(Started|Stopped|Failed)"

# 3. ¿Qué PIDs tiene nginx?
journalctl -u nginx -o short-precise --no-pager | grep -oE "nginx\[.*?\]"

# 4. ¿Hay mensajes de kernel (OOM killer) que puedan afectar?
journalctl -k --since today --no-pager | grep -iE "(oom|kill|out of memory)"
```

### Ejercicio 5 — Script de monitoreo

```bash
# Crear un script que verifique errores de un servicio:
cat <<'EOF' > /usr/local/bin/check-service-errors.sh
#!/bin/bash
SERVICE="${1:-nginx}"
THRESHOLD="${2:-5}"

ERRORS=$(journalctl -u "$SERVICE" -p err --since "10 min ago" -q --no-pager | wc -l)

if [[ "$ERRORS" -gt "$THRESHOLD" ]]; then
    echo "ALERTA: $SERVICE tiene $ERRORS errores en los últimos 10 minutos"
    journalctl -u "$SERVICE" -p err --since "10 min ago" --no-pager | tail -5
    exit 1
else
    echo "OK: $SERVICE — $ERRORS errores (umbral: $THRESHOLD)"
fi
EOF
chmod +x /usr/local/bin/check-service-errors.sh

# Probar:
./check-service-errors.sh sshd
./check-service-errors.sh nginx 3
```
