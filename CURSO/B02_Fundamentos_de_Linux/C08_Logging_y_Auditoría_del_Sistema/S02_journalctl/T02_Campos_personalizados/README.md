# T02 — Campos personalizados

## Campos del journal

Cada entrada del journal es un conjunto de campos clave-valor. Los campos
se dividen en categorías según su prefijo:

```bash
# Ver todos los campos de una entrada:
journalctl -n 1 -o verbose
```

### Campos del sistema (prefijo _)

```bash
# Los campos con _ los establece journald automáticamente.
# No pueden ser falsificados por la aplicación:

# _PID=1234              — PID del proceso
# _UID=0                 — UID del usuario
# _GID=0                 — GID del grupo
# _COMM=sshd             — nombre del comando (basename del ejecutable)
# _EXE=/usr/sbin/sshd    — ruta completa del ejecutable
# _CMDLINE=sshd: user    — línea de comandos completa
# _HOSTNAME=server       — hostname
# _TRANSPORT=syslog      — cómo llegó el log (journal, syslog, kernel, stdout)
# _SYSTEMD_UNIT=sshd.service — unidad de systemd
# _SYSTEMD_CGROUP=/system.slice/sshd.service — cgroup
# _BOOT_ID=abc123...     — ID del boot actual
# _MACHINE_ID=def456...  — ID de la máquina
# _SOURCE_REALTIME_TIMESTAMP=... — timestamp original del mensaje

# Estos campos son CONFIABLES para auditoría porque los establece
# el kernel/journald, no la aplicación
```

### Campos de usuario (sin prefijo _)

```bash
# Campos que la aplicación puede establecer:
# MESSAGE=texto del log                  — el mensaje
# PRIORITY=6                             — prioridad (0-7)
# SYSLOG_FACILITY=3                      — facility syslog
# SYSLOG_IDENTIFIER=myapp                — identificador
# SYSLOG_PID=1234                        — PID reportado por la app
# CODE_FILE=main.c                       — archivo fuente
# CODE_LINE=42                           — línea del código
# CODE_FUNC=handle_request               — función
```

### Campos internos (prefijo __)

```bash
# Campos de control interno de journald:
# __CURSOR=s=abc;i=456;...  — posición única en el journal
# __REALTIME_TIMESTAMP=...  — timestamp en microsegundos
# __MONOTONIC_TIMESTAMP=... — timestamp monotónico

# Estos son para uso interno y no deben filtrarse directamente
# (excepto __CURSOR para retomar lecturas)
```

## SYSLOG_IDENTIFIER

El campo más usado para identificar la fuente de un mensaje:

```bash
# SYSLOG_IDENTIFIER es el "tag" del mensaje syslog.
# Es lo que ves entre el hostname y el PID en el formato short:
#
# Mar 17 15:30:00 server sshd[1234]: mensaje
#                        ^^^^
#                        SYSLOG_IDENTIFIER

# Filtrar por identifier:
journalctl SYSLOG_IDENTIFIER=sshd
journalctl SYSLOG_IDENTIFIER=sudo
journalctl SYSLOG_IDENTIFIER=kernel

# Listar todos los identifiers:
journalctl -F SYSLOG_IDENTIFIER | head -20
# CRON
# NetworkManager
# kernel
# sshd
# sudo
# systemd
# ...
```

```bash
# Diferencia entre SYSLOG_IDENTIFIER y _COMM:
# _COMM = nombre del ejecutable (lo pone journald, confiable)
# SYSLOG_IDENTIFIER = tag que la aplicación se pone a sí misma

# Pueden diferir:
# _COMM=apache2  SYSLOG_IDENTIFIER=apache2   (coinciden)
# _COMM=python3  SYSLOG_IDENTIFIER=myapp     (difieren)
# _COMM=bash     SYSLOG_IDENTIFIER=backup.sh (difieren)

# Para filtrar por lo que la app dice ser: SYSLOG_IDENTIFIER
# Para filtrar por lo que realmente es: _COMM o _EXE
```

### Establecer SYSLOG_IDENTIFIER desde scripts

```bash
# Con logger:
logger -t myapp "mensaje de mi aplicación"
# Establece SYSLOG_IDENTIFIER=myapp

# Con systemd-cat (más integrado con journald):
echo "mensaje" | systemd-cat -t myapp
# También establece SYSLOG_IDENTIFIER=myapp

# En un servicio de systemd:
# [Service]
# SyslogIdentifier=myapp
# Todos los logs de stdout/stderr tendrán SYSLOG_IDENTIFIER=myapp
```

## MESSAGE_ID

MESSAGE_ID es un UUID que identifica un **tipo** de mensaje. Permite
buscar una clase específica de evento sin depender del texto:

```bash
# Los mensajes de systemd usan MESSAGE_IDs estandarizados:
journalctl MESSAGE_ID=39f53479d3a045ac8e11786248231fbf -n 5
# Todos los mensajes de "unidad arrancada" (Unit started)

journalctl MESSAGE_ID=be02cf6855d2428ba40df7e9d022f03d -n 5
# Todos los mensajes de "unidad parada" (Unit stopped)

# Ver el MESSAGE_ID de una entrada:
journalctl -u nginx -o verbose -n 1 | grep MESSAGE_ID
# MESSAGE_ID=39f53479d3a045ac8e11786248231fbf
```

### MESSAGE_IDs comunes de systemd

```bash
# Catálogo de mensajes de systemd:
# 39f53479d3a045ac8e11786248231fbf — Unit started (unidad arrancada)
# be02cf6855d2428ba40df7e9d022f03d — Unit stopped (unidad detenida)
# 7d4958e842da4a758f6c1cdc7b36dcc5 — Unit failed (unidad fallida)
# a]4cc150f2a94f4a84b2b8fd40e2a913c — System boot completed
# 98268866d1d54a499c4e98921d93bc40 — Unit reloaded
# d34d037fff1847e6ae669a370e694725 — Seat started (sesión gráfica)
# 8d45620c1a4348dbb17410da57c60c66 — New user session (nueva sesión)

# Consultar el catálogo completo:
journalctl --catalog | head -50

# Buscar la explicación de un MESSAGE_ID:
journalctl --catalog --message-id=7d4958e842da4a758f6c1cdc7b36dcc5
```

### Usar MESSAGE_ID para monitoreo

```bash
# Contar cuántos servicios fallaron hoy:
journalctl MESSAGE_ID=7d4958e842da4a758f6c1cdc7b36dcc5 \
    --since today -o json --no-pager | \
    jq -r '.UNIT' | sort -u
# Lista de servicios que fallaron

# Alertar cuando un servicio falle:
journalctl -f -o json MESSAGE_ID=7d4958e842da4a758f6c1cdc7b36dcc5 | \
    while read -r line; do
        UNIT=$(echo "$line" | jq -r '.UNIT')
        echo "ALERTA: $UNIT ha fallado a las $(date)"
    done
```

## Campos personalizados desde aplicaciones

### Desde systemd-cat

```bash
# systemd-cat conecta la salida de un comando al journal:
echo "mensaje personalizado" | systemd-cat -t myapp -p info
# Crea una entrada con:
# SYSLOG_IDENTIFIER=myapp
# PRIORITY=6
# MESSAGE=mensaje personalizado

# Ejecutar un comando con su salida al journal:
systemd-cat -t backup /opt/backup/run.sh
# Todo stdout y stderr van al journal con tag "backup"
```

### Desde la API de systemd (sd_journal)

```bash
# Las aplicaciones en C/Python/etc. pueden enviar campos personalizados
# usando la API sd_journal_send():

# C:
# sd_journal_send("MESSAGE=User login",
#                 "PRIORITY=6",
#                 "USER_NAME=%s", username,
#                 "LOGIN_IP=%s", ip,
#                 "LOGIN_METHOD=ssh",
#                 NULL);

# Python (systemd.journal):
# from systemd import journal
# journal.send("User login",
#              PRIORITY=6,
#              USER_NAME=username,
#              LOGIN_IP=ip,
#              LOGIN_METHOD="ssh")

# Estos campos personalizados se pueden filtrar:
journalctl USER_NAME=admin
journalctl LOGIN_METHOD=ssh
```

### Desde logger con campos estructurados

```bash
# logger soporta el formato RFC 5424 con datos estructurados:
logger --sd-id=myapp@12345 --sd-param=user=admin --sd-param=action=login \
    "Usuario admin ha iniciado sesión"

# Sin embargo, journald no siempre parsea estos campos estructurados
# de la misma forma que sd_journal_send()

# Para campos personalizados fiables, usar systemd-cat o la API directa
```

## Campos de código fuente

```bash
# Algunas aplicaciones incluyen información del código fuente:
# CODE_FILE=src/handler.c       — archivo fuente
# CODE_LINE=142                 — número de línea
# CODE_FUNC=handle_request      — nombre de la función

# systemd mismo incluye estos campos en sus mensajes:
journalctl -u systemd-logind -o verbose -n 1 | grep CODE_
# CODE_FILE=src/login/logind.c
# CODE_LINE=542
# CODE_FUNC=manager_handle_action

# Filtrar por archivo fuente (útil para debug):
journalctl CODE_FILE=src/login/logind.c -n 10 --no-pager
```

## _TRANSPORT — Cómo llegó el log

```bash
# _TRANSPORT indica cómo llegó el mensaje a journald:
journalctl -F _TRANSPORT
# journal   — API directa de systemd (sd_journal_send)
# stdout    — stdout/stderr de un servicio de systemd
# syslog    — a través de la API syslog() o /dev/log
# kernel    — mensajes del kernel (dmesg)
# audit     — subsistema de auditoría del kernel
# driver    — mensajes internos de journald

# Filtrar por transport:
journalctl _TRANSPORT=stdout -u nginx
# Solo los mensajes que nginx escribió en stdout/stderr

journalctl _TRANSPORT=kernel -p err
# Solo errores del kernel

journalctl _TRANSPORT=audit
# Solo mensajes de auditoría
```

## Exploración de campos

```bash
# Ver qué campos tiene un servicio específico:
journalctl -u sshd -n 1 -o json-pretty | jq 'keys[]' | sort
# "__CURSOR"
# "__MONOTONIC_TIMESTAMP"
# "__REALTIME_TIMESTAMP"
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

# Contar valores únicos de un campo:
journalctl -F PRIORITY | sort | uniq -c
#  mensajes con cada nivel de prioridad

# Ver qué UIDs generan logs:
journalctl -F _UID --since today
# 0
# 1000
# 33
```

## Campos y SyslogIdentifier en unit files

```bash
# En un archivo .service, puedes controlar los campos:

# [Service]
# SyslogIdentifier=myapp
# → SYSLOG_IDENTIFIER=myapp (en lugar del nombre del ejecutable)

# SyslogFacility=local0
# → SYSLOG_FACILITY=16 (local0)

# SyslogLevel=info
# → Nivel mínimo para capturar (no recomendado, mejor filtrar en journalctl)

# SyslogLevelPrefix=true (default)
# → Si la app imprime "<3>error message", journald interpreta <3> como prioridad err
# → Protocolo del kernel para prioridad en el mensaje

# LogExtraFields=APP_VERSION=2.1.0
# LogExtraFields=ENVIRONMENT=production
# → Campos personalizados adicionales en TODAS las entradas del servicio
```

```bash
# Ejemplo completo:
# [Service]
# SyslogIdentifier=myapp
# LogExtraFields=APP_VERSION=2.1.0
# LogExtraFields=ENVIRONMENT=production
# LogExtraFields=TEAM=backend

# Ahora:
journalctl ENVIRONMENT=production
# Todos los logs de servicios marcados como producción

journalctl TEAM=backend -p err
# Errores de todos los servicios del equipo backend
```

---

## Ejercicios

### Ejercicio 1 — Explorar campos

```bash
# 1. ¿Qué SYSLOG_IDENTIFIERs hay en el boot actual?
journalctl -b -F SYSLOG_IDENTIFIER | head -20

# 2. ¿Qué transports se usan?
journalctl -b -F _TRANSPORT

# 3. ¿Qué ejecutables generan más logs?
journalctl -b -o json --no-pager 2>/dev/null | \
    jq -r '._COMM // "unknown"' | sort | uniq -c | sort -rn | head -10
```

### Ejercicio 2 — Enviar con campos personalizados

```bash
# 1. Enviar un log con identifier personalizado:
echo "test con systemd-cat" | systemd-cat -t mi-test -p info

# 2. Verificar que se recibió:
journalctl SYSLOG_IDENTIFIER=mi-test --no-pager

# 3. Ver todos los campos de esa entrada:
journalctl SYSLOG_IDENTIFIER=mi-test -o verbose --no-pager -n 1
```

### Ejercicio 3 — MESSAGE_IDs

```bash
# ¿Cuántos servicios se arrancaron en el boot actual?
journalctl -b MESSAGE_ID=39f53479d3a045ac8e11786248231fbf -o json --no-pager 2>/dev/null | \
    jq -r '.UNIT // .USER_UNIT // "unknown"' | sort -u | wc -l

# ¿Algún servicio falló?
journalctl -b MESSAGE_ID=7d4958e842da4a758f6c1cdc7b36dcc5 --no-pager 2>/dev/null || \
    echo "Ningún servicio falló en este boot"
```
