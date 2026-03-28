# T02 — Filtros y templates

> **Objetivo:** Dominar los filtros avanzados de rsyslog (property-based y RainerScript) y aprender a crear templates para formatear mensajes de log.

## Más allá de facilidad.prioridad

Las reglas básicas (`facility.priority action`) son limitadas. rsyslog ofrece mecanismos más potentes:

```
┌─────────────────────────────────────────────────────────────────────┐
│  MENSAJE ENTRANTE                                                   │
│       │                                                             │
│       ▼                                                             │
│  ┌─────────────────────────────────────────┐                       │
│  │  FILTRO 1: facility.priority            │  ← filtro básico      │
│  └───────────────┬─────────────────────────┘                       │
│       │         │                                                   │
│       ▼         ▼                                                   │
│  ┌─────────────────────────────────────────┐                       │
│  │  FILTRO 2: property-based               │  ← por campo          │
│  │  :msg, contains, "error"                │                       │
│  └───────────────┬─────────────────────────┘                       │
│       │         │                                                   │
│       ▼         ▼                                                   │
│  ┌─────────────────────────────────────────┐                       │
│  │  FILTRO 3: RainerScript                 │  ← lógica compleja    │
│  │  if $programname == "nginx" then {...}  │                       │
│  └───────────────┬─────────────────────────┘                       │
│       │                                                             │
│       ▼                                                             │
│  ┌─────────────────────────────────────────┐                       │
│  │  TEMPLATE                              │  ← formatear salida   │
│  │  "%timegenerated% %HOSTNAME% %msg%"     │                       │
│  └───────────────┬─────────────────────────┘                       │
│       │                                                             │
│       ▼                                                             │
│  ┌─────────────────────────────────────────┐                       │
│  │  ACTION                                 │                       │
│  │  archivo / remoto / BD / discard       │                       │
│  └─────────────────────────────────────────┘                       │
└─────────────────────────────────────────────────────────────────────┘
```

---

## Property-based filters

Los filtros basados en propiedades seleccionan mensajes por **cualquier campo** del mensaje syslog:

### Sintaxis

```bash
# :propiedad, operador, "valor"    acción

:programname, isequal, "sshd"      /var/log/sshd.log
:msg, contains, "error"            /var/log/errors.log
:hostname, isequal, "webserver01"   /var/log/remote/webserver01.log
```

### Operadores disponibles

| Operador | Descripción | Ejemplo |
|----------|-------------|---------|
| `isequal` | Igual exacto (case-sensitive) | `:programname, isequal, "sshd"` |
| `startswith` | Empieza con | `:programname, startswith, "php"` |
| `contains` | Contiene substring | `:msg, contains, "error"` |
| `contains_i` | Contiene (case-insensitive) | `:msg, contains_i, "ERROR"` |
| `regex` | Coincide con regex | `:msg, regex, "error\|fail\|crit"` |
| `ereregex` | Expresión regular ERE | `:msg, ereregex, "user [0-9]+"` |
| `!contains` | **No** contiene | `:msg, !contains, "debug"` |
| `!isequal` | No es igual | `:programname, !isequal, "systemd"` |

### Propiedades disponibles

| Propiedad | Descripción | Ejemplo |
|-----------|-------------|---------|
| `msg` | Texto del mensaje | `:msg, contains, "failed"` |
| `programname` | Nombre del programa | `:programname, isequal, "nginx"` |
| `hostname` | Hostname del origen | `:hostname, isequal, "web01"` |
| `fromhost` | Host que recibió el log | (para logs remotos) |
| `fromhost-ip` | IP del origen | `:fromhost-ip, startswith, "192.168."` |
| `syslogtag` | Tag completo | `sshd[1234]:` |
| `syslogfacility-text` | Facility como texto | `auth`, `mail`, `local0` |
| `syslogseverity-text` | Priority como texto | `err`, `warning`, `info` |
| `timegenerated` | Timestamp de recepción | |
| `timereported` | Timestamp del mensaje original | |
| `pri` | Priority numérico | |
| `$!` | Campos JSON del mensaje | (mensajes estructurados) |

### Ejemplos prácticos

```bash
# Separar logs de nginx a su archivo propio:
:programname, isequal, "nginx"     /var/log/nginx/access.log
:programname, isequal, "nginx"     stop
# stop = no sigue procesando (no aparece en syslog)

# Capturar intentos de login fallidos:
:msg, contains, "authentication failure"   /var/log/auth-failures.log
:msg, contains, "Failed password"            /var/log/auth-failures.log

# Separar por IP de origen (servidor central):
:fromhost-ip, isequal, "192.168.1.10"   /var/log/remote/web.log
:fromhost-ip, isequal, "192.168.1.20"   /var/log/remote/db.log
:fromhost-ip, startswith, "192.168.1."   /var/log/remote/other.log

# Descartar mensajes ruidosos:
:programname, isequal, "systemd-resolved"   stop
:msg, contains, "UFW BLOCK"                stop
:programname, isequal, "CRON"              stop   # si cron es muy ruidoso
```

---

## La acción stop

```bash
# stop termina el procesamiento del mensaje
# Ya no se evalúa contra las reglas siguientes

# Ejemplo — sshd a archivo Y a syslog:
:programname, isequal, "sshd"   /var/log/sshd.log
# sshd aparece en sshd.log Y en syslog (si existe regla *.*)

# Ejemplo — sshd SOLO a archivo (no a syslog):
:programname, isequal, "sshd"   /var/log/sshd.log
:programname, isequal, "sshd"   stop
# stop en segunda línea = no llega a reglas siguientes

# Equivalente legacy (obsoleto, NO usar):
:programname, isequal, "sshd"   ~
# ~ era "discard" — stop lo reemplazó
```

---

## RainerScript

RainerScript es el **lenguaje moderno** de configuración de rsyslog. Permite condiciones, variables y lógica compleja.

### Sintaxis básica

```bash
# if/then simple:
if $programname == "sshd" then {
    action(type="omfile" file="/var/log/sshd.log")
    stop
}

# if/then/else:
if $syslogseverity <= 3 then {
    # err (3), crit (2), alert (1), emerg (0)
    action(type="omfile" file="/var/log/critical.log" sync="on")
} else {
    action(type="omfile" file="/var/log/normal.log")
}
```

### Operadores en RainerScript

```
COMPARACIÓN:
  ==          igual (texto)
  !=          diferente
  < > <= >=   numérico
  contains    contiene string
  startswith  empieza con
  regex       expresión regular

LÓGICOS:
  and         Y
  or          O
  not         negación
  ( )         agrupar

NUMÉRICOS (prioridades):
  0=emerg  1=alert  2=crit  3=err
  4=warning  5=notice  6=info  7=debug
```

### Variables de mensaje (con $)

```bash
# $msg             — texto del mensaje
# $programname     — nombre del programa
# $hostname        — hostname
# $syslogfacility  — facility (numérico: 0-23)
# $syslogseverity  — priority (numérico: 0-7)
# $fromhost-ip     — IP de origen
# $syslogfacility-text   — facility como texto
# $syslogseverity-text   — priority como texto
```

### Ejemplos avanzados

```bash
# Combinar condiciones:
if $programname == "nginx" and $syslogseverity <= 4 then {
    action(type="omfile" file="/var/log/nginx-warnings.log")
}

# Errores o fallos:
if $msg contains "error" or $msg contains "fail" or $msg contains "crit" then {
    action(type="omfile" file="/var/log/problems.log")
}

# Excluir servicios del sistema:
if not ($programname == "systemd" or $programname == "kernel" or $programname == "CRON") then {
    action(type="omfile" file="/var/log/apps.log")
}

# Log rotate dinámico por host:
if $fromhost-ip != "127.0.0.1" then {
    action(type="omfile"
           dynaFile="RemoteLog"
           template="RemoteFormat")
    stop
}
```

### Ejemplo completo

```bash
# /etc/rsyslog.d/30-custom.conf

# Logs de nginx:
if $programname == "nginx" then {
    action(type="omfile"
           file="/var/log/nginx/error.log"
           template="RSYSLOG_FileFormat")
    stop
}

# Logs de postgresql con permisos específicos:
if $programname == "postgresql" then {
    action(type="omfile"
           file="/var/log/postgresql/postgresql.log"
           fileOwner="postgres"
           fileGroup="postgres"
           fileCreateMode="0640")
    stop
}

# Alertas críticas con sync (más seguro):
if $syslogseverity <= 2 then {
    action(type="omfile"
           file="/var/log/critical-alerts.log"
           sync="on")
}
```

---

## Templates

Los templates definen el **formato** de los mensajes escritos:

### Templates predefinidos

| Template | Descripción | Ejemplo de salida |
|----------|-------------|-------------------|
| `RSYSLOG_TraditionalFileFormat` | Formato clásico syslog | `Mar 17 15:30:00 server sshd[1234]: mensaje` |
| `RSYSLOG_FileFormat` | ISO 8601 con microsegundos | `2026-03-17T15:30:00.123456+00:00 server sshd[1234]: mensaje` |
| `RSYSLOG_SyslogProtocol23Format` | RFC 5424 completo | Para envío remoto moderno |
| `RSYSLOG_DebugFormat` | Todos los campos | Debugging |

### Crear templates personalizados

```bash
# Sintaxis RainerScript (preferida):
template(name="MyFormat" type="string"
    string="%timegenerated:::date-rfc3339% %HOSTNAME% %programname%: %msg%\n"
)

# Usar en una acción:
*.* action(type="omfile" file="/var/log/custom.log" template="MyFormat")

# Sintaxis legacy:
$template MyFormat,"%timegenerated:::date-rfc3339% %HOSTNAME% %programname%: %msg%\n"
*.* /var/log/custom.log;MyFormat
```

### Propiedades en templates

```
PROPIEDADES BÁSICAS:
  %msg%              — texto del mensaje
  %HOSTNAME%         — hostname
  %programname%      — nombre del programa
  %syslogtag%        — tag completo (programa[PID]:)

PROPIEDADES DE PRIORITY:
  %syslogseverity-text%  — err, warning, info...
  %syslogfacility-text%  — auth, mail, daemon...

PROPIEDADES DE TIEMPO:
  %timegenerated%   — timestamp de cuando se recibió
  %timereported%    — timestamp del mensaje original

MODIFICADORES:
  %msg:::drop-last-lf%       — eliminar último \n
  %msg:1:50%                 — solo primeros 50 chars
  %programname:::uppercase%  — convertir a mayúsculas
  %timegenerated:::date-rfc3339%  — formato ISO 8601
  %timegenerated:::date-mysql%    — formato MySQL
  %timegenerated:::date-subseconds% — con subsegundos
```

### Templates dinámicos (dynaFile)

```bash
# El nombre del archivo se genera dinámicamente:
template(name="PerHostLog" type="string"
    string="/var/log/remote/%HOSTNAME%/%programname%.log"
)

*.* action(type="omfile" dynaFile="PerHostLog")

# Resultado:
# /var/log/remote/web01/nginx.log
# /var/log/remote/web01/sshd.log
# /var/log/remote/db01/postgresql.log

# Template por fecha:
template(name="DailyLog" type="string"
    string="/var/log/daily/%$year%-%$month%-%$day%.log"
)
```

### Template JSON (para Elasticsearch)

```bash
template(name="JSONFormat" type="list") {
    constant(value="{")
    constant(value="\"@timestamp\":\"")
    property(name="timegenerated" dateFormat="rfc3339")
    constant(value="\",\"host\":\"")
    property(name="hostname")
    constant(value="\",\"severity\":\"")
    property(name="syslogseverity-text")
    constant(value="\",\"program\":\"")
    property(name="programname")
    constant(value="\",\"message\":\"")
    property(name="msg" format="jsonf")
    constant(value="\"}\n")
}

# Resultado:
# {"@timestamp":"2026-03-17T15:30:00+00:00","host":"server","severity":"info","program":"nginx","message":"GET /index.html 200"}
```

---

## Orden de procesamiento

```bash
# rsyslog procesa reglas DE ARRIBA ABAJO:
# 1. Archivos en /etc/rsyslog.d/ en orden alfabético (primero 10-, luego 50-)
# 2. Las reglas dentro de cada archivo en orden de aparición
# 3. Un mensaje puede hacer match con VARIAS reglas (si no hay stop)

# Ejemplo — auth va a dos archivos:
auth.*                    /var/log/auth.log        # regla 1
*.*;auth.none             /var/log/syslog          # regla 2

# auth.log recibe auth (regla 1)
# syslog recibe TODO EXCEPTO auth (regla 2, auth.none excluye auth)

# Si NO estuviera auth.none, auth aparecería en AMBOS archivos
```

---

## Ejemplos completos

### Servidor de logs centralizado

```bash
# /etc/rsyslog.d/10-remote-input.conf
module(load="imtcp")
input(type="imtcp" port="514")

# /etc/rsyslog.d/30-remote-rules.conf
template(name="RemoteLog" type="string"
    string="/var/log/remote/%HOSTNAME%/%programname%.log"
)

template(name="RemoteFormat" type="string"
    string="%timegenerated:::date-rfc3339% %fromhost-ip% %programname%: %msg%\n"
)

if $fromhost-ip != "127.0.0.1" then {
    action(type="omfile" dynaFile="RemoteLog" template="RemoteFormat")
    stop
}
```

### Filtrado de seguridad

```bash
# /etc/rsyslog.d/20-security.conf
# Capturar TODOS los eventos de autenticación en un solo archivo:
if $programname == "sshd" or
   $programname == "sudo" or
   $programname == "su" or
   $programname == "login" or
   $msg contains "authentication" or
   $msg contains "sudo" then {
    action(type="omfile"
           file="/var/log/security-audit.log"
           sync="on")
    # NO stop — también van a sus archivos normales (auth.log, etc.)
}
```

---

## Quick reference

```
PROPERTY FILTERS:
  :propiedad, operador, "valor"  acción
  Operadores: isequal, contains, startswith, regex, !contains

RAINERSCRIPT:
  if $propiedad op "valor" then { acción }
  if $propiedad op "valor" and $propiedad2 op "valor2" then { ... }
  Comparadores: == != < > <= >= contains regex
  Lógicos: and or not

TEMPLATES:
  template(name="X" type="string" string="...%propiedad%...\n")
  template(name="X" type="list") { constant... property... }
  Props: %msg% %HOSTNAME% %programname% %timegenerated%
  Modifiers: :::date-rfc3339 :::drop-last-lf %prop:1:50%

ACTIONS:
  action(type="omfile" file="...")
  action(type="omfile" dynaFile="templateName")
  action(type="omfile" template="templateName")
  sync="on" para escritura segura (sin buffer)
  stop para terminar procesamiento
```

---

## Ejercicios

### Ejercicio 1 — Identificar filtros activos

```bash
# Ver property-based filters:
grep -E "^:.*,.*," /etc/rsyslog.conf /etc/rsyslog.d/*.conf 2>/dev/null | grep -v '^#'

# Ver RainerScript if statements:
grep -rE "^if |^[[:space:]]+if " /etc/rsyslog.d/*.conf 2>/dev/null | grep -v '^#'

# Ver templates:
grep -rE "template\(" /etc/rsyslog.d/*.conf 2>/dev/null | grep -v '^#'
```

### Ejercicio 2 — Comparar formatos

```bash
# Ver el formato actual de syslog:
head -1 /var/log/syslog 2>/dev/null || head -1 /var/log/messages

# Enviar un mensaje de prueba y observar formato:
logger -t TEST -p user.info "mensaje de prueba"
grep TEST /var/log/syslog 2>/dev/null | tail -1
grep TEST /var/log/messages 2>/dev/null | tail -1
```

### Ejercicio 3 — Crear un filtro de errores

```bash
# Objetivo: separar todos los errores en /var/log/errors.log

# 1. Crear configuración:
cat << 'EOF' | sudo tee /etc/rsyslog.d/40-errors.conf
# RainerScript: separar errores y superiores
if $syslogseverity <= 3 then {
    action(type="omfile"
           file="/var/log/errors.log"
           template="RSYSLOG_FileFormat")
}
EOF

# 2. Validar y reiniciar:
sudo rsyslogd -N1
sudo systemctl restart rsyslog

# 3. Probar:
logger -t test -p user.err "Este es un error"
logger -t test -p user.info "Esta es info"

# 4. Verificar:
grep "test" /var/log/errors.log
```

### Ejercicio 4 — Crear un filtro por programa

```bash
# Objetivo: que los logs de cron vayan SOLO a /var/log/cron.log
# (y NO aparezcan en syslog general)

# 1. Crear configuración:
cat << 'EOF' | sudo tee /etc/rsyslog.d/40-cron-only.conf
# Opción A: property-based
:programname, isequal, "CRON"   /var/log/cron.log
:programname, isequal, "CRON"   stop

# Opción B: RainerScript
# if $programname == "CRON" then {
#     action(type="omfile" file="/var/log/cron.log")
#     stop
# }
EOF

# 2. Validar y aplicar:
sudo rsyslogd -N1
sudo systemctl restart rsyslog

# 3. Generar actividad cron:
(crontab -l 2>/dev/null | head -1) || echo "* * * * * logger cron-test"
logger -t CRON "cron test message"

# 4. Verificar:
grep CRON /var/log/cron.log
grep CRON /var/log/syslog 2>/dev/null && echo "PROBLEMA: CRON en syslog" || echo "OK"
```

### Ejercicio 5 — Template dinámico por fecha

```bash
# Objetivo: logs en archivos separados por día
# /var/log/daily/2026-03-17.log, /var/log/daily/2026-03-18.log, etc.

# 1. Crear template:
cat << 'EOF' | sudo tee /etc/rsyslog.d/50-daily.conf
template(name="DailyLog" type="string"
    string="/var/log/daily/%$year%-%$month%-%$day%.log"
)

*.* action(type="omfile"
          dynaFile="DailyLog"
          template="RSYSLOG_FileFormat")
EOF

# 2. Crear directorio:
sudo mkdir -p /var/log/daily
sudo chown syslog:adm /var/log/daily

# 3. Validar y aplicar:
sudo rsyslogd -N1
sudo systemctl restart rsyslog

# 4. Probar:
logger "test daily log"
ls -la /var/log/daily/
cat /var/log/daily/$(date +%Y-%m-%d).log | tail -5
```
