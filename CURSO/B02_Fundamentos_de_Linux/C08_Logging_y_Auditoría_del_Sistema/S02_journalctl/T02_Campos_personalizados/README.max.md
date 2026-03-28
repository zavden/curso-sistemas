# T02 — Campos personalizados

> **Objetivo:** Entender la estructura de campos del journal, aprender a usar campos estándar para filtrado, y configurar campos personalizados en aplicaciones y servicios.

## Estructura de campos del journal

Cada entrada del journal es un conjunto de **campos clave-valor**. Se dividen en tres categorías según su prefijo:

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                    CATEGORÍAS DE CAMPOS                                     │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                              │
│  _PREFIX (underscore) — Establecidos por journald/kernel                     │
│  ════════════════════════════════════════════════════                       │
│  Confiables para auditoría — la aplicación NO puede falsificarlos            │
│  _PID, _UID, _COMM, _EXE, _HOSTNAME, _SYSTEMD_UNIT, _BOOT_ID...           │
│                                                                              │
│  NO PREFIX — Establecidos por la aplicación                                 │
│  ══════════════════════════════════════════════════════                     │
│  MESSAGE, PRIORITY, SYSLOG_IDENTIFIER, CODE_FILE, CODE_LINE...             │
│  La aplicación tiene control total sobre estos valores                      │
│                                                                              │
│  __PREFIX (double underscore) — Uso interno de journald                      │
│  ═════════════════════════════════════════════════════════                   │
│  __CURSOR, __REALTIME_TIMESTAMP, __MONOTONIC_TIMESTAMP                      │
│  Solo para uso interno (excepto __CURSOR para incremental reads)            │
│                                                                              │
└─────────────────────────────────────────────────────────────────────────────┘
```

---

## Campos del sistema (_前缀)

**Confiables para auditoría** — el kernel y journald los establecen, no la aplicación.

| Campo | Descripción | Ejemplo |
|-------|-------------|---------|
| `_PID` | PID del proceso | `1234` |
| `_UID` | UID del usuario | `0` |
| `_GID` | GID del grupo | `0` |
| `_COMM` | Nombre del comando (basename) | `sshd` |
| `_EXE` | Ruta completa del ejecutable | `/usr/sbin/sshd` |
| `_CMDLINE` | Línea de comandos completa | `sshd: user [priv]` |
| `_HOSTNAME` | Hostname | `server01` |
| `_TRANSPORT` | Cómo llegó el log | `syslog`, `stdout`, `journal` |
| `_SYSTEMD_UNIT` | Unidad de systemd | `sshd.service` |
| `_SYSTEMD_CGROUP` | Cgroup en systemd | `/system.slice/sshd.service` |
| `_BOOT_ID` | ID único del boot | `abc123...` |
| `_MACHINE_ID` | ID de la máquina | `def456...` |
| `_SOURCE_REALTIME_TIMESTAMP` | Timestamp original del mensaje | `1710689400123456` |

```bash
# Ver todos los campos de un mensaje:
journalctl -n 1 -o verbose
# Observa cómo journald establece todos los campos _
# mientras MESSAGE y PRIORITY los pone la aplicación
```

## Campos de usuario (sin prefijo)

**Controlados por la aplicación** — pueden ser modificados o falsificados.

| Campo | Descripción | Ejemplo |
|-------|-------------|---------|
| `MESSAGE` | Texto del log | `User login successful` |
| `PRIORITY` | Nivel 0-7 | `6` |
| `SYSLOG_FACILITY` | Facility syslog (numérico) | `4` (auth) |
| `SYSLOG_IDENTIFIER` | Tag/identificador | `sshd` |
| `SYSLOG_PID` | PID reportado por la app | `1234` |
| `CODE_FILE` | Archivo fuente | `main.c` |
| `CODE_LINE` | Línea del código | `142` |
| `CODE_FUNC` | Función | `handle_request` |

## Campos internos (__前缀)

| Campo | Descripción | Uso |
|-------|-------------|-----|
| `__CURSOR` | Posición única en el journal | `--after-cursor` para lecturas incrementales |
| `__REALTIME_TIMESTAMP` | Timestamp en microsegundos | Representación canónica del tiempo |
| `__MONOTONIC_TIMESTAMP` | Timestamp monotónico | Tiempo desde el boot (para correlacionar) |

---

## SYSLOG_IDENTIFIER

El campo más usado para identificar la fuente de un mensaje.

```bash
# Es el "tag" del formato short:
# Mar 17 15:30:00 server sshd[1234]: mensaje
#                        ^^^^^
#                        SYSLOG_IDENTIFIER

# Filtrar por identifier:
journalctl SYSLOG_IDENTIFIER=sshd
journalctl SYSLOG_IDENTIFIER=sudo
journalctl SYSLOG_IDENTIFIER=kernel

# Listar todos los identifiers:
journalctl -F SYSLOG_IDENTIFIER | head -20
```

### SYSLOG_IDENTIFIER vs _COMM

```
SYSLOG_IDENTIFIER = lo que la app dice ser (configurable)
_COMM             = lo que realmente es el ejecutable (confiable)

Pueden diferir:
  _COMM=apache2    SYSLOG_IDENTIFIER=apache2     → coinciden
  _COMM=python3    SYSLOG_IDENTIFIER=myapp        → difieren (app Python)
  _COMM=bash       SYSLOG_IDENTIFIER=backup.sh    → difieren (script)

Para filtrar por identidad declarada:     SYSLOG_IDENTIFIER
Para filtrar por ejecutable real:          _COMM o _EXE
```

### Establecer SYSLOG_IDENTIFIER

```bash
# Desde scripts con logger:
logger -t myapp "mensaje de mi aplicación"
# SYSLOG_IDENTIFIER=myapp

# Desde systemd-cat:
echo "mensaje" | systemd-cat -t myapp -p info
# SYSLOG_IDENTIFIER=myapp

# En un unit file de systemd:
# [Service]
# SyslogIdentifier=myapp
# Todos los logs de stdout/stderr tendrán este identifier
```

---

## MESSAGE_ID

UUID que identifica un **tipo** de mensaje — permite buscar una clase de evento sin depender del texto:

```bash
# MESSAGE_ID de systemd (unidades):
# 39f53479d3a045ac8e11786248231fbf → Unit started
# be02cf6855d2428ba40df7e9d022f03d → Unit stopped
# 7d4958e842da4a758f6c1cdc7b36dcc5 → Unit failed

# Buscar todos los mensajes de un tipo:
journalctl MESSAGE_ID=39f53479d3a045ac8e11786248231fbf -n 5
# Todos los "unidad arrancada" en el journal

# Ver el MESSAGE_ID de una entrada:
journalctl -u nginx -o verbose -n 1 | grep MESSAGE_ID
```

### MESSAGE_IDs comunes de systemd

```
┌────────────────────────────────────────┬──────────────────────────────────┐
│ MESSAGE_ID                             │ Evento                           │
├────────────────────────────────────────┼──────────────────────────────────┤
│ 39f53479d3a045ac8e11786248231fbf      │ Unit started (arrancada)         │
│ be02cf6855d2428ba40df7e9d022f03d       │ Unit stopped (detenida)          │
│ 7d4958e842da4a758f6c1cdc7b36dcc5      │ Unit failed (fallida)            │
│ 98268866d1d54a499c4e98921d93bc40       │ Unit reloaded                   │
│ a4cc150f2a94f4a84b2b8fd40e2a913c       │ System booted                    │
│ d34d037fff1847e6ae669a370e694725       │ Seat started (sesión gráfica)    │
│ 8d45620c1a4348dbb17410da57c60c66       │ New user session                 │
└────────────────────────────────────────┴──────────────────────────────────┘
```

```bash
# Catálogo completo de systemd:
journalctl --catalog | head -50

# Buscar explicación de un MESSAGE_ID:
journalctl --catalog --message-id=7d4958e842da4a758f6c1cdc7b36dcc5
```

### Monitoreo con MESSAGE_ID

```bash
# Contar servicios que fallaron hoy:
journalctl MESSAGE_ID=7d4958e842da4a758f6c1cdc7b36dcc5 \
    --since today -o json --no-pager | \
    jq -r '.UNIT' | sort -u

# Alertar en tiempo real cuando un servicio falle:
journalctl -f -o json MESSAGE_ID=7d4958e842da4a758f6c1cdc7b36dcc5 | \
    while read -r line; do
        UNIT=$(echo "$line" | jq -r '.UNIT')
        TIMESTAMP=$(echo "$line" | jq -r '.__REALTIME_TIMESTAMP')
        echo "ALERTA: $UNIT falló a las $TIMESTAMP"
    done
```

---

## _TRANSPORT — Cómo llegó el log

```bash
# _TRANSPORT indica el origen del mensaje:
journalctl -F _TRANSPORT
# journal    — API sd_journal_send() directa
# stdout     — stdout/stderr de un servicio systemd
# syslog     — a través de syslog() o /dev/log
# kernel     — mensajes del kernel (dmesg)
# audit      — subsistema de auditoría del kernel
# driver     — mensajes internos de journald
```

```bash
# Filtrar por transport:
journalctl _TRANSPORT=stdout -u nginx
# Solo stdout/stderr de nginx

journalctl _TRANSPORT=kernel -p err
# Solo errores del kernel

journalctl _TRANSPORT=audit
# Solo mensajes de auditoría
```

---

## Campos personalizados desde aplicaciones

### systemd-cat

```bash
# Conectar la salida de un comando al journal:
echo "mensaje personalizado" | systemd-cat -t myapp -p info
# Resultado:
#   SYSLOG_IDENTIFIER=myapp
#   PRIORITY=6
#   MESSAGE=mensaje personalizado

# Ejecutar un script y capturar toda su salida:
systemd-cat -t backup /opt/backup/run.sh
# Todo stdout/stderr va al journal con tag "backup"
```

### sd_journal_send() — C

```c
#include <systemd/sd-journal.h>

sd_journal_send("MESSAGE=User login",
                "PRIORITY=6",
                "USER_NAME=%s", username,
                "LOGIN_IP=%s", ip,
                "LOGIN_METHOD=ssh",
                "SESSION_ID=%s", session_id,
                NULL);

// Filtrar después:
// journalctl USER_NAME=admin
// journalctl LOGIN_METHOD=ssh
```

### Python (systemd.journal)

```python
from systemd import journal

journal.send("User login",
             PRIORITY=journal.LOG_INFO,
             USER_NAME=username,
             LOGIN_IP=ip,
             LOGIN_METHOD="ssh")

# Con filtrado:
# journalctl USER_NAME=admin
```

### En unit files — LogExtraFields

```bash
# [Service]
# SyslogIdentifier=myapp
# LogExtraFields=APP_VERSION=2.1.0
# LogExtraFields=ENVIRONMENT=production
# LogExtraFields=TEAM=backend

# Resultado: TODOS los logs de este servicio incluyen:
#   APP_VERSION=2.1.0
#   ENVIRONMENT=production
#   TEAM=backend

# Filtrar por campo personalizado:
journalctl ENVIRONMENT=production
journalctl TEAM=backend -p err
```

---

## Campos de código fuente

```bash
# Algunas aplicaciones incluyen información de debug:
journalctl -u systemd-logind -o verbose -n 1 | grep CODE_
# CODE_FILE=src/login/logind.c
# CODE_LINE=542
# CODE_FUNC=manager_handle_action

# Filtrar por función:
journalctl CODE_FUNC=handle_request -n 5 --no-pager

# Filtrar por archivo:
journalctl CODE_FILE=src/login/logind.c -n 10 --no-pager
```

---

## Exploración de campos

```bash
# Ver TODOS los campos de un mensaje (formato JSON):
journalctl -u sshd -n 1 -o json-pretty | jq 'keys[]' | sort
# "__CURSOR"
# "__REALTIME_TIMESTAMP"
# "__MONOTONIC_TIMESTAMP"
# "_BOOT_ID"
# "_CMDLINE"
# "_COMM"
# "_EXE"
# "_GID"
# "_HOSTNAME"
# "_MACHINE_ID"
# "_PID"
# "_SYSTEMD_CGROUP"
# "_SYSTEMD_UNIT"
# "_TRANSPORT"
# "_UID"
# "MESSAGE"
# "PRIORITY"
# "SYSLOG_FACILITY"
# "SYSLOG_IDENTIFIER"
# "SYSLOG_PID"

# Contar entradas por nivel de prioridad:
journalctl -F PRIORITY --since today | sort | uniq -c

# Ver qué UIDs generan logs:
journalctl -F _UID --since today | sort -u
# 0       (root)
# 1000    (usuario normal)
# 33      (www-data, apache, etc.)

# Top 10 ejecutables por volumen de logs:
journalctl -b -o json --no-pager 2>/dev/null | \
    jq -r '._COMM // "unknown"' | sort | uniq -c | sort -rn | head -10
```

---

## Quick reference

```
CAMPOS DEL SISTEMA (_):
  _PID, _UID, _GID     — identificación del proceso
  _COMM, _EXE          — ejecutable
  _HOSTNAME            — hostname
  _SYSTEMD_UNIT        — unidad de systemd
  _TRANSPORT            — cómo llegó (journal|stdout|syslog|kernel|audit)

CAMPOS DE USUARIO:
  MESSAGE              — texto del log
  PRIORITY (0-7)       — severidad
  SYSLOG_IDENTIFIER    — tag de la aplicación
  CODE_FILE/LINE/FUNC  — código fuente

CAMPOS INTERNOS (__):
  __CURSOR             — posición para lectura incremental
  __REALTIME_TIMESTAMP — timestamp canónico

FILTRAR POR CAMPO:
  journalctl CAMPO=valor
  journalctl _UID=0         — solo root
  journalctl PRIORITY=3     — solo errores
  journalctl SYSLOG_IDENTIFIER=sshd

CAMPOS PERSONALIZADOS:
  systemd-cat -t myapp
  journal.send("msg", CUSTOM_FIELD=value)
  LogExtraFields=KEY=value  (en unit file)
```

---

## Ejercicios

### Ejercicio 1 — Explorar campos existentes

```bash
# 1. Listar identifiers únicos:
journalctl -b -F SYSLOG_IDENTIFIER | head -20

# 2. Ver transports usados:
journalctl -b -F _TRANSPORT | sort | uniq -c

# 3. Top 10 procesos por volumen de logs:
journalctl -b -o json --no-pager 2>/dev/null | \
    jq -r '._COMM // "unknown"' | sort | uniq -c | sort -rn | head -10

# 4. Ver todos los campos de un mensaje de nginx:
journalctl -u nginx -n 1 -o json-pretty | jq 'to_entries[] | select(.key | startswith("_")) | .key' | sort
```

### Ejercicio 2 — Campos personalizados con systemd-cat

```bash
# 1. Enviar mensaje con identifier personalizado:
echo "test personalizado" | systemd-cat -t myapp -p info

# 2. Verificar campos:
journalctl SYSLOG_IDENTIFIER=myapp -o verbose --no-pager -n 1

# 3. Ver el mensaje con solo el texto:
journalctl SYSLOG_IDENTIFIER=myapp -o cat --no-pager -n 1
```

### Ejercicio 3 — MESSAGE_ID

```bash
# 1. ¿Cuántos servicios arrancaron en este boot?
journalctl -b MESSAGE_ID=39f53479d3a045ac8e11786248231fbf -o json --no-pager | \
    jq -r '.UNIT // .USER_UNIT // "unknown"' | sort -u | wc -l

# 2. ¿Algún servicio falló?
journalctl -b MESSAGE_ID=7d4958e842da4a758f6c1cdc7b36dcc5 --no-pager 2>/dev/null || \
    echo "Ningún servicio falló en este boot"

# 3. Listar servicios que se detuvieron:
journalctl -b MESSAGE_ID=be02cf6855d2428ba40df7e9d022f03d -o json --no-pager | \
    jq -r '.UNIT // .USER_UNIT // "unknown"' | sort -u
```

### Ejercicio 4 — Correlacionar eventos

```bash
# 1. Encontrar todos los eventos de un boot específico:
BOOT_ID=$(journalctl -b -o json --no-pager -n 1 | jq -r '._BOOT_ID')
echo "Boot ID: $BOOT_ID"

# 2. Ver todos los logs de un usuario específico (por UID):
journalctl _UID=1000 --since today -n 20 --no-pager

# 3. Ver qué pasó justo antes de que un servicio fallara:
journalctl -b MESSAGE_ID=7d4958e842da4a758f6c1cdc7b36dcc5 --no-pager -n 1 -o short-precise
# Obtener el timestamp y buscar antes de ese momento
```

### Ejercicio 5 — Crear aplicación con campos personalizados

```bash
# Objetivo: crear un script que envíe logs con campos personalizados

cat << 'EOF' > /usr/local/bin/myapp-logger
#!/bin/bash
# Envía logs con campos estructurados

send_log() {
    local level="$1"
    local message="$2"
    local user="$3"
    local action="$4"

    # Usar systemd-cat con campos extra
    echo "[$level] $message USER=$user ACTION=$action" | \
        systemd-cat -t myapp -p "${level}"
}

# Probar:
send_log "info" "Usuario iniciou sesión" "admin" "login"
send_log "err" "Error de conexión a BD" "admin" "db_error"
EOF
chmod +x /usr/local/bin/myapp-logger

# Ahora se puede filtrar por:
# journalctl SYSLOG_IDENTIFIER=myapp ACTION=login
# journalctl SYSLOG_IDENTIFIER=myapp USER=admin
```
