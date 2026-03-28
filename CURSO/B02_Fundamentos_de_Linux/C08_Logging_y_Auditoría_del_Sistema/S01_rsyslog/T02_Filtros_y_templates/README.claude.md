# T02 — Filtros y templates

> **Fuentes:** `README.md`, `README.max.md`, `LABS.md`, `labs/README.md`
> **Regla aplicada:** 2 (`.max.md` sin `.claude.md` → crear `.claude.md`)

---

## Erratas detectadas

| # | Archivo | Línea(s) | Error | Corrección |
|---|---------|----------|-------|------------|
| 1 | README.md | 55 | `:msg, regex, "error\|fail\|crit"` — El operador `regex` usa POSIX BRE, donde `\|` es un carácter literal, no alternancia. El patrón busca la cadena textual `error\|fail\|crit` en lugar de "error OR fail OR crit" | Usar `ereregex` (POSIX ERE, donde `\|` sí es alternancia), o escapar como `"error\\|fail\\|crit"` para la extensión GNU BRE. `README.max.md:68` lo hace correctamente con `\\|` |
| 2 | labs/README.md | 476–477 | "rsyslog.conf se procesa primero, luego los archivos de /etc/rsyslog.d/" — Simplificación engañosa | En Debian, `$IncludeConfig /etc/rsyslog.d/*.conf` aparece **antes** de las reglas default en rsyslog.conf; los drop-ins se procesan antes que las reglas principales del archivo base. `README.md:397` lo reconoce correctamente con "(o al revés según el `$IncludeConfig`)" |

---

## Más allá de facility.priority

Las reglas básicas (`facility.priority action`) solo filtran por dos dimensiones. rsyslog ofrece filtros más potentes y templates para controlar el formato:

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

Cada mensaje pasa por **todas** las reglas en orden (salvo que `stop` detenga el procesamiento). Un mensaje puede coincidir con múltiples reglas y escribirse en múltiples destinos.

---

## Property-based filters

Filtran mensajes por **cualquier campo** del mensaje syslog.

### Sintaxis

```bash
# :propiedad, operador, "valor"    acción

:programname, isequal, "sshd"      /var/log/sshd.log
# Mensajes cuyo programa sea exactamente "sshd"

:msg, contains, "error"            /var/log/errors.log
# Mensajes que contengan "error" en el texto

:hostname, isequal, "webserver01"  /var/log/remote/webserver01.log
# Mensajes del host "webserver01" (útil para log central)
```

### Operadores disponibles

| Operador | Descripción | Ejemplo |
|----------|-------------|---------|
| `isequal` | Igual exacto (case-sensitive) | `:programname, isequal, "sshd"` |
| `startswith` | Empieza con | `:programname, startswith, "php"` |
| `contains` | Contiene substring | `:msg, contains, "error"` |
| `contains_i` | Contiene (case-insensitive) | `:msg, contains_i, "ERROR"` |
| `regex` | POSIX BRE (Basic Regular Expression) | `:msg, regex, "error\\|fail"` (GNU `\|` = alternancia) |
| `ereregex` | POSIX ERE (Extended Regular Expression) | `:msg, ereregex, "user [0-9]+ login"` (`|` funciona directo) |
| `!contains` | **No** contiene | `:msg, !contains, "debug"` |
| `!isequal` | No es igual | `:programname, !isequal, "systemd"` |

> **`regex` vs `ereregex`:** `regex` usa BRE — para alternancia necesitas la extensión GNU `\|`. `ereregex` usa ERE — `|` funciona directamente como alternancia. Usa `ereregex` cuando necesites `|`, `+` o `?` sin escapar.

### Propiedades del mensaje

| Propiedad | Descripción | Ejemplo |
|-----------|-------------|---------|
| `msg` | Texto del mensaje | `:msg, contains, "failed"` |
| `programname` | Nombre del programa | `:programname, isequal, "nginx"` |
| `hostname` | Hostname del origen | `:hostname, isequal, "web01"` |
| `fromhost` | Host del que se recibió (logs remotos) | `:fromhost, isequal, "web01"` |
| `fromhost-ip` | IP del origen | `:fromhost-ip, startswith, "192.168."` |
| `syslogtag` | Tag completo (`sshd[1234]:`) | `:syslogtag, startswith, "sshd"` |
| `syslogfacility-text` | Facility como texto | `auth`, `mail`, `local0` |
| `syslogseverity-text` | Priority como texto | `err`, `warning`, `info` |
| `timegenerated` | Timestamp de recepción por rsyslog | |
| `timereported` | Timestamp original del mensaje | |
| `pri` | Priority numérica | |

### Ejemplos prácticos

```bash
# Separar logs de nginx a su archivo propio:
:programname, isequal, "nginx"     /var/log/nginx/access.log
:programname, isequal, "nginx"     stop
# stop = no sigue procesando (no aparece en syslog)

# Capturar intentos de login fallidos:
:msg, contains, "authentication failure"   /var/log/auth-failures.log
:msg, contains, "Failed password"          /var/log/auth-failures.log

# Separar por IP de origen (servidor central):
:fromhost-ip, isequal, "192.168.1.10"  /var/log/remote/web.log
:fromhost-ip, isequal, "192.168.1.20"  /var/log/remote/db.log
:fromhost-ip, startswith, "192.168.1." /var/log/remote/other.log

# Descartar mensajes ruidosos:
:programname, isequal, "systemd-resolved" stop
:msg, contains, "UFW BLOCK"               stop
:programname, isequal, "CRON"              stop
```

---

## La acción stop

`stop` termina el procesamiento del mensaje actual. Las reglas posteriores no lo ven.

```bash
# --- Sin stop: sshd aparece en sshd.log Y en syslog ---
:programname, isequal, "sshd"   /var/log/sshd.log
*.*                              /var/log/syslog
# sshd.log ✓   syslog ✓

# --- Con stop: sshd SOLO en sshd.log ---
:programname, isequal, "sshd"   /var/log/sshd.log
:programname, isequal, "sshd"   stop
*.*                              /var/log/syslog
# sshd.log ✓   syslog ✗ (stop lo detuvo)

# --- Descartar completamente (no va a ningún archivo) ---
:programname, isequal, "systemd-resolved" stop
# El mensaje no llega a ninguna regla posterior

# --- Legacy (obsoleto, NO usar) ---
:programname, isequal, "sshd"   ~
# ~ era el "discard" antiguo, reemplazado por stop
```

---

## RainerScript

Lenguaje de configuración moderno de rsyslog. Permite condiciones, variables y lógica compleja.

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

### Operadores

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
  ( )         agrupar condiciones

PRIORIDADES (numérico):
  0=emerg  1=alert  2=crit  3=err
  4=warning  5=notice  6=info  7=debug
```

### Variables de mensaje

En RainerScript, las propiedades se acceden con `$`:

```bash
# $msg               — texto del mensaje
# $programname       — nombre del programa
# $hostname          — hostname
# $syslogfacility    — facility (numérico: 0-23)
# $syslogseverity    — priority (numérico: 0-7)
# $fromhost-ip       — IP de origen
# $syslogfacility-text — facility como texto
# $syslogseverity-text — priority como texto
```

### Ejemplos con lógica compleja

```bash
# nginx con warning o superior:
if $programname == "nginx" and $syslogseverity <= 4 then {
    action(type="omfile" file="/var/log/nginx-warnings.log")
}

# Errores o fallos:
if $msg contains "error" or $msg contains "fail" or $msg contains "crit" then {
    action(type="omfile" file="/var/log/problems.log")
}

# Todo excepto servicios del sistema:
if not ($programname == "systemd" or $programname == "kernel" or $programname == "CRON") then {
    action(type="omfile" file="/var/log/apps.log")
}

# Logs remotos con template dinámico:
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

# Alertas críticas con sync (escritura segura):
if $syslogseverity <= 2 then {
    action(type="omfile"
           file="/var/log/critical-alerts.log"
           sync="on")
}
```

---

## Templates

Los templates definen el **formato** de los mensajes escritos por rsyslog.

### Templates predefinidos

| Template | Descripción | Ejemplo de salida |
|----------|-------------|-------------------|
| `RSYSLOG_TraditionalFileFormat` | Formato clásico syslog (default) | `Mar 17 15:30:00 server sshd[1234]: mensaje` |
| `RSYSLOG_FileFormat` | ISO 8601 con microsegundos | `2026-03-17T15:30:00.123456+00:00 server sshd[1234]: mensaje` |
| `RSYSLOG_SyslogProtocol23Format` | RFC 5424 completo | Para envío remoto moderno |
| `RSYSLOG_DebugFormat` | Todos los campos internos | Para debugging |

### Crear templates personalizados

```bash
# Sintaxis RainerScript (preferida):
template(name="MyFormat" type="string"
    string="%timegenerated:::date-rfc3339% %HOSTNAME% %programname%: %msg%\n"
)

# Usar en una acción:
*.* action(type="omfile" file="/var/log/custom.log" template="MyFormat")

# Sintaxis legacy (equivalente):
$template MyFormat,"%timegenerated:::date-rfc3339% %HOSTNAME% %programname%: %msg%\n"
*.* /var/log/custom.log;MyFormat
```

### Propiedades en templates

Las propiedades se referencian con `%propiedad%`:

```
PROPIEDADES BÁSICAS:
  %msg%                    — texto del mensaje
  %HOSTNAME%               — hostname
  %programname%            — nombre del programa
  %syslogtag%              — tag completo (programa[PID]:)

PROPIEDADES DE PRIORITY:
  %syslogseverity-text%    — err, warning, info...
  %syslogfacility-text%    — auth, mail, daemon...

PROPIEDADES DE TIEMPO:
  %timegenerated%          — timestamp de cuando rsyslog recibió
  %timereported%           — timestamp del mensaje original

PROPIEDADES DE ORIGEN (logs remotos):
  %fromhost%               — hostname del origen
  %fromhost-ip%            — IP del origen
```

### Modificadores de propiedades

```bash
# Sintaxis: %propiedad:fromChar:toChar:opciones%
# Las tres posiciones se separan con :::

%msg:::drop-last-lf%              # eliminar último salto de línea
%timegenerated:::date-rfc3339%    # formato ISO 8601
%timegenerated:::date-subseconds% # con subsegundos
%msg:1:50%                        # solo caracteres 1 a 50
%programname:::uppercase%         # convertir a mayúsculas
%programname:::lowercase%         # convertir a minúsculas
```

### Templates dinámicos (dynaFile)

El nombre del archivo se genera dinámicamente a partir de propiedades del mensaje:

```bash
# Archivo por hostname y programa:
template(name="PerHostLog" type="string"
    string="/var/log/remote/%HOSTNAME%/%programname%.log"
)

*.* action(type="omfile" dynaFile="PerHostLog")

# Crea archivos automáticamente:
#   /var/log/remote/web01/nginx.log
#   /var/log/remote/web01/sshd.log
#   /var/log/remote/db01/postgresql.log

# Archivo por fecha:
template(name="DailyLog" type="string"
    string="/var/log/daily/%$year%-%$month%-%$day%.log"
)
```

> **`dynaFile` vs `file`:** `file` usa una ruta fija; `dynaFile` referencia un template que genera la ruta. Con `dynaFile`, rsyslog crea los directorios automáticamente si la acción incluye `createDirs="on"`.

### Template JSON (tipo list)

Para enviar logs en formato JSON (ej: a Elasticsearch):

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
# {"@timestamp":"2026-03-17T15:30:00+00:00","host":"server",
#  "severity":"info","program":"nginx","message":"GET /index.html 200"}
```

> **`type="string"` vs `type="list"`:** `string` usa `%propiedades%` en una cadena; `list` construye el output pieza por pieza con `constant()` y `property()`. `list` es mejor para JSON porque `format="jsonf"` escapa caracteres especiales automáticamente.

---

## Orden de procesamiento

```bash
# rsyslog procesa reglas DE ARRIBA A ABAJO.
# Un mensaje se evalúa contra TODAS las reglas (salvo que haya stop).

# --- Orden real en Debian ---
# 1. rsyslog.conf: módulos y config global
# 2. $IncludeConfig /etc/rsyslog.d/*.conf  ← drop-ins (en orden alfabético)
# 3. rsyslog.conf: reglas por defecto (auth.log, syslog, etc.)
#
# Los drop-ins se procesan ANTES que las reglas default de rsyslog.conf.
# Esto es intencional: un drop-in puede usar stop para interceptar
# mensajes antes de que lleguen a las reglas por defecto.
#
# En RHEL la estructura es similar: el include está antes de las reglas.

# --- Ejemplo: auth va a archivo dedicado pero no a syslog ---
auth.*                    /var/log/auth.log       # regla 1: auth → auth.log
*.*;auth.none             /var/log/syslog         # regla 2: todo excepto auth → syslog

# auth.log recibe auth (regla 1)
# syslog recibe todo EXCEPTO auth (regla 2, por auth.none)
# Sin auth.none, auth aparecería en AMBOS archivos

# --- Convención de numeración para drop-ins ---
# 00-19  → módulos e inputs (ej: 10-remote-input.conf)
# 20-49  → reglas de seguridad/filtrado (ej: 20-security.conf)
# 50-79  → reglas de aplicación (ej: 50-myapp.conf)
# 80-99  → reglas catchall/finales
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

### Separar logs de una aplicación

```bash
# /etc/rsyslog.d/25-myapp.conf

template(name="MyAppFormat" type="string"
    string="%timegenerated:::date-rfc3339% [%syslogseverity-text%] %msg:::drop-last-lf%\n"
)

# Solo mensajes de myapp (usa facility local0):
if $syslogfacility-text == "local0" then {
    action(type="omfile"
           file="/var/log/myapp/app.log"
           template="MyAppFormat"
           fileOwner="myapp"
           fileGroup="myapp"
           fileCreateMode="0640"
           dirCreateMode="0750"
           createDirs="on")
    stop
}
```

### Filtrado de seguridad

```bash
# /etc/rsyslog.d/20-security.conf
# Capturar TODOS los eventos de autenticación:
if $programname == "sshd" or
   $programname == "sudo" or
   $programname == "su" or
   $programname == "login" or
   $msg contains "authentication" then {
    action(type="omfile"
           file="/var/log/security-audit.log"
           sync="on")
    # NO stop — también van a auth.log por la regla normal
}
```

---

## Quick reference

```
PROPERTY FILTERS:
  :propiedad, operador, "valor"  acción
  Operadores: isequal, contains, startswith, contains_i, regex, ereregex
  Negación:   !contains, !isequal, !startswith, !regex

RAINERSCRIPT:
  if $propiedad op "valor" then { action(...); stop }
  if ... and ... then { ... } else { ... }
  Comparadores: == != < > <= >= contains startswith regex
  Lógicos: and or not ( )

TEMPLATES:
  template(name="X" type="string" string="...%prop%...\n")
  template(name="X" type="list") { constant... property... }
  Props: %msg% %HOSTNAME% %programname% %timegenerated%
  Modifiers: :::date-rfc3339 :::drop-last-lf :::uppercase %prop:1:50%

ACTIONS:
  action(type="omfile" file="/path/file.log")
  action(type="omfile" dynaFile="templateName")
  action(type="omfile" ... template="templateName")
  sync="on"       escritura segura (sin buffer)
  createDirs="on" crear directorios automáticamente
  stop            terminar procesamiento del mensaje
```

---

## Labs

### Parte 1 — Filtros de propiedades

**Paso 1.1: Sintaxis de property-based filters**

```bash
# :propiedad, operador, "valor"    acción
:programname, isequal, "sshd"        /var/log/sshd.log
:msg, contains, "error"              /var/log/errors.log
:hostname, isequal, "web01"          /var/log/web01.log
:fromhost-ip, startswith, "192.168." /var/log/lan.log
```

**Paso 1.2: Operadores**

```
COMPARACIÓN DE STRINGS:
  isequal      igual exacto (case-sensitive)
  startswith   empieza con
  contains     contiene substring
  contains_i   contiene (case-insensitive)

EXPRESIONES REGULARES:
  regex        POSIX BRE (usar \| para alternancia en GNU)
  ereregex     POSIX ERE (| funciona directamente)

NEGACIÓN:
  !contains    NO contiene
  !isequal     NO es igual
```

**Paso 1.3: Propiedades del mensaje**

Las propiedades más usadas son `msg`, `programname`, `hostname`, `fromhost-ip`, `syslogfacility-text`, `syslogseverity-text`, `syslogtag`, `timegenerated`, `timereported` y `pri`. Un mensaje de log típico:

```
Mar 25 10:30:00 server sshd[1234]: Failed password for root
│               │      │           │
timegenerated   hostname syslogtag  msg
                       programname="sshd"
```

**Paso 1.4: La acción stop**

```bash
# Sin stop — sshd aparece en sshd.log Y en syslog:
:programname, isequal, "sshd"   /var/log/sshd.log
*.*                              /var/log/syslog

# Con stop — sshd SOLO en sshd.log:
:programname, isequal, "sshd"   /var/log/sshd.log
:programname, isequal, "sshd"   stop

# Descartar completamente (no va a ningún archivo):
:programname, isequal, "systemd-resolved" stop
:msg, contains, "UFW BLOCK"               stop

# Legacy (obsoleto): ~ era discard, reemplazado por stop
```

**Paso 1.5: Inspeccionar filtros en la configuración real**

```bash
# Buscar filtros de propiedades existentes:
grep -rh "^:" /etc/rsyslog.conf /etc/rsyslog.d/ 2>/dev/null | grep -v "^#"

# Buscar reglas con stop:
grep -rh "stop" /etc/rsyslog.conf /etc/rsyslog.d/ 2>/dev/null | grep -v "^#"

# Enviar mensaje y rastrear dónde aparece:
logger -t filter-test "Este mensaje va a donde las reglas digan"
sleep 1
for f in /var/log/syslog /var/log/messages /var/log/auth.log /var/log/daemon.log; do
    [[ -f "$f" ]] || continue
    grep -q "filter-test" "$f" 2>/dev/null && echo "ENCONTRADO en $f"
done
```

### Parte 2 — RainerScript

**Paso 2.1: Sintaxis if/then/else**

```bash
# if/then simple:
if $programname == "sshd" then {
    action(type="omfile" file="/var/log/sshd.log")
    stop
}

# if/then/else:
if $syslogseverity <= 3 then {
    # err (3), crit (2), alert (1), emerg (0)
    action(type="omfile" file="/var/log/critical.log")
} else {
    action(type="omfile" file="/var/log/normal.log")
}
```

Las prioridades son numéricas: `0=emerg, 1=alert, 2=crit, 3=err, 4=warning, 5=notice, 6=info, 7=debug`. `<= 3` significa err y superiores (menor número = más severo).

**Paso 2.2: Operadores y lógica compleja**

```bash
# Combinar condiciones con and:
if $programname == "nginx" and $syslogseverity <= 4 then {
    action(type="omfile" file="/var/log/nginx-warnings.log")
}

# Alternancia con or:
if $msg contains "error" or $msg contains "fail" then {
    action(type="omfile" file="/var/log/problems.log")
}

# Exclusión con not y agrupación:
if not ($programname == "systemd" or $programname == "kernel") then {
    action(type="omfile" file="/var/log/apps.log")
}
```

**Paso 2.3: Ejemplo completo de producción**

```bash
# /etc/rsyslog.d/30-custom.conf

if $programname == "nginx" then {
    action(type="omfile"
           file="/var/log/nginx/error.log"
           template="RSYSLOG_FileFormat")
    stop
}

if $programname == "postgresql" then {
    action(type="omfile"
           file="/var/log/postgresql/postgresql.log"
           fileOwner="postgres"
           fileGroup="postgres"
           fileCreateMode="0640")
    stop
}

if $syslogseverity <= 2 then {
    action(type="omfile"
           file="/var/log/critical-alerts.log"
           sync="on")
}
```

**Paso 2.4: Filtrado de seguridad**

```bash
# /etc/rsyslog.d/20-security.conf
if $programname == "sshd" or
   $programname == "sudo" or
   $programname == "su" or
   $programname == "login" or
   $msg contains "authentication" then {
    action(type="omfile"
           file="/var/log/security-audit.log"
           sync="on")
    # Sin stop: también van a auth.log por la regla normal
}
```

`sync="on"` fuerza escritura inmediata a disco (sin buffer), importante para logs críticos de seguridad.

### Parte 3 — Templates

**Paso 3.1: Templates predefinidos**

```bash
# Ver qué template usa el sistema:
head -3 /var/log/syslog 2>/dev/null || head -3 /var/log/messages

# Si el timestamp es "Mar 17 15:30:00" → RSYSLOG_TraditionalFileFormat
# Si el timestamp es "2026-03-17T15:30:00.123456" → RSYSLOG_FileFormat
```

**Paso 3.2: Templates personalizados y propiedades**

```bash
# Definir:
template(name="MyFormat" type="string"
    string="%timegenerated:::date-rfc3339% %HOSTNAME% %programname%: %msg%\n"
)

# Usar:
*.* action(type="omfile" file="/var/log/custom.log" template="MyFormat")
```

Modificadores disponibles: `:::drop-last-lf` (quitar `\n` final), `:::date-rfc3339` (ISO 8601), `:1:50` (substring), `:::uppercase`, `:::lowercase`.

**Paso 3.3: Templates dinámicos (dynaFile)**

```bash
# Archivo por hostname:
template(name="PerHostLog" type="string"
    string="/var/log/remote/%HOSTNAME%/%programname%.log"
)
*.* action(type="omfile" dynaFile="PerHostLog")
# Crea: /var/log/remote/web01/nginx.log, /var/log/remote/db01/postgresql.log...

# Archivo por fecha:
template(name="DailyLog" type="string"
    string="/var/log/daily/%$year%-%$month%-%$day%.log"
)
```

**Paso 3.4: Template JSON (type="list")**

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
# format="jsonf" escapa caracteres especiales (comillas, backslashes)
```

**Paso 3.5: Orden de procesamiento**

```bash
# El orden real en Debian:
# 1. rsyslog.conf → módulos + config global
# 2. $IncludeConfig /etc/rsyslog.d/*.conf → drop-ins (orden alfabético)
# 3. rsyslog.conf → reglas por defecto

# Los drop-ins van ANTES de las reglas default.
# Por eso un drop-in con stop puede interceptar mensajes
# antes de que lleguen a las reglas de syslog/auth.log/etc.

# Verificar el orden:
ls /etc/rsyslog.d/*.conf 2>/dev/null
```

---

## Ejercicios

### Ejercicio 1 — Interpretar un property-based filter

Dado este fragmento de configuración:

```bash
:programname, isequal, "postfix"   /var/log/mail/postfix.log
:programname, startswith, "postfix" stop
```

**a)** Si `postfix/smtpd` envía un mensaje, ¿se escribe en `postfix.log`?
**b)** ¿El `stop` detiene ese mismo mensaje?
**c)** ¿Qué pasa si `postfix` (sin sufijo) envía un mensaje?

<details><summary>Predicción</summary>

**a)** **No**. `isequal` requiere coincidencia exacta: `"postfix/smtpd" != "postfix"`. El `programname` sería `postfix/smtpd`, que no es igual a `"postfix"`.

**b)** **Sí**. `startswith` coincide: `"postfix/smtpd"` empieza con `"postfix"`. El `stop` detiene el procesamiento y el mensaje no llega a ninguna regla posterior.

**c)** `postfix` coincide con `isequal "postfix"` → se escribe en `postfix.log`. También coincide con `startswith "postfix"` → `stop` detiene el procesamiento. Resultado: solo aparece en `postfix.log`.

**Conclusión:** El filter tiene un bug: los subcomponentes de Postfix (smtpd, cleanup, etc.) no se registran en `postfix.log` pero sí son detenidos por `stop`. Mejor usar `startswith` en ambas líneas.

</details>

### Ejercicio 2 — Negación con operadores

¿Qué mensajes captura cada una de estas reglas?

```bash
# Regla A:
:msg, !contains, "debug"   /var/log/no-debug.log

# Regla B:
:msg, contains_i, "ERROR"  /var/log/errors-ci.log

# Regla C:
:programname, !isequal, "CRON"  /var/log/no-cron.log
```

<details><summary>Predicción</summary>

**Regla A:** Todos los mensajes que **no** contengan la cadena `"debug"` (case-sensitive). Un mensaje con `"Debug"` o `"DEBUG"` **sí** se captura (porque `!contains` es case-sensitive y no encuentra `"debug"` en minúsculas exactas).

**Regla B:** Todos los mensajes que contengan `"error"` en cualquier combinación de mayúsculas/minúsculas: `"ERROR"`, `"error"`, `"Error"`, `"eRrOr"`. `contains_i` ignora caso.

**Regla C:** Todos los mensajes cuyo `programname` **no** sea exactamente `"CRON"`. Esto incluye mensajes de `cron` (minúsculas), `crond`, `sshd`, `nginx`, etc. Solo excluye los que tienen `programname` exactamente `"CRON"`.

</details>

### Ejercicio 3 — Orden de evaluación con stop

Dado este conjunto de reglas (en este orden):

```bash
# Regla 1:
:msg, contains, "error"    /var/log/errors.log

# Regla 2:
:programname, isequal, "nginx"   /var/log/nginx.log
:programname, isequal, "nginx"   stop

# Regla 3:
*.*                        /var/log/syslog
```

¿Dónde aparece cada uno de estos mensajes?

- **Mensaje A:** nginx envía `"connection error on port 80"`
- **Mensaje B:** sshd envía `"session opened"`
- **Mensaje C:** nginx envía `"GET /index.html 200"`

<details><summary>Predicción</summary>

**Mensaje A** (nginx, contiene "error"):
1. Regla 1: `msg contains "error"` → **sí** → escribe en `errors.log`
2. Regla 2: `programname == "nginx"` → **sí** → escribe en `nginx.log`, luego `stop`
3. Regla 3: **no alcanzada** (stop en regla 2)
- Resultado: `errors.log` ✓, `nginx.log` ✓, `syslog` ✗

**Mensaje B** (sshd, "session opened"):
1. Regla 1: `msg contains "error"` → **no**
2. Regla 2: `programname == "nginx"` → **no**
3. Regla 3: `*.*` → **sí** → escribe en `syslog`
- Resultado: `errors.log` ✗, `nginx.log` ✗, `syslog` ✓

**Mensaje C** (nginx, no contiene "error"):
1. Regla 1: `msg contains "error"` → **no**
2. Regla 2: `programname == "nginx"` → **sí** → escribe en `nginx.log`, luego `stop`
3. Regla 3: **no alcanzada**
- Resultado: `errors.log` ✗, `nginx.log` ✓, `syslog` ✗

</details>

### Ejercicio 4 — Traducir property filter a RainerScript

Convierte estas reglas de property-based filter a RainerScript equivalente:

```bash
:programname, isequal, "sshd"    /var/log/sshd.log
:programname, isequal, "sshd"    stop
:msg, contains, "error"          /var/log/errors.log
```

<details><summary>Predicción</summary>

```bash
if $programname == "sshd" then {
    action(type="omfile" file="/var/log/sshd.log")
    stop
}

if $msg contains "error" then {
    action(type="omfile" file="/var/log/errors.log")
}
```

La ventaja de RainerScript: el `stop` queda dentro del bloque `if`, haciendo explícito que solo aplica a sshd. Con property filters, son dos líneas separadas que hay que leer en secuencia para entender que van juntas.

</details>

### Ejercicio 5 — RainerScript con lógica compleja

Escribe una regla RainerScript que:
1. Capture mensajes de `sshd` O `sudo` con prioridad `warning` o superior
2. Los escriba en `/var/log/auth-warnings.log` con `sync="on"`
3. **No** use `stop` (queremos que también vayan a sus archivos normales)

<details><summary>Predicción</summary>

```bash
if ($programname == "sshd" or $programname == "sudo") and $syslogseverity <= 4 then {
    action(type="omfile"
           file="/var/log/auth-warnings.log"
           sync="on")
}
```

Puntos clave:
- Los paréntesis agrupan la condición OR del programa
- `$syslogseverity <= 4` captura warning (4), err (3), crit (2), alert (1), emerg (0)
- `sync="on"` garantiza escritura inmediata a disco
- Sin `stop`: el mensaje sigue evaluándose contra reglas posteriores (auth.log, syslog, etc.)

</details>

### Ejercicio 6 — Templates predefinidos

Observa estas dos líneas de log:

```
Línea A: Mar 25 10:30:00 server sshd[1234]: Failed password for root
Línea B: 2026-03-25T10:30:00.456789+00:00 server sshd[1234]: Failed password for root
```

**a)** ¿Qué template predefinido generó cada una?
**b)** ¿Cuál es más adecuada para análisis automatizado y por qué?
**c)** ¿Cómo cambiarías el template de un archivo específico sin afectar los demás?

<details><summary>Predicción</summary>

**a)**
- Línea A: `RSYSLOG_TraditionalFileFormat` — formato clásico con timestamp `MMM DD HH:MM:SS`
- Línea B: `RSYSLOG_FileFormat` — timestamp ISO 8601 con microsegundos y timezone

**b)** Línea B (`RSYSLOG_FileFormat`) es mejor para análisis automatizado porque:
- ISO 8601 incluye el año (TraditionalFileFormat no lo tiene)
- Incluye timezone explícita (+00:00)
- Microsegundos permiten ordenar eventos con precisión
- El formato es inequívoco para parsers (no depende del locale)

**c)** Crear un drop-in que aplique el template solo a la regla deseada:

```bash
# /etc/rsyslog.d/25-auth-format.conf
if $syslogfacility-text == "auth" then {
    action(type="omfile"
           file="/var/log/auth.log"
           template="RSYSLOG_FileFormat")
    stop
}
```

Esto solo afecta a auth.log; los demás archivos siguen usando el template por defecto.

</details>

### Ejercicio 7 — Template personalizado

Crea un template llamado `"CompactFormat"` que produzca líneas como:

```
2026-03-25T10:30:00+00:00 [err] nginx: connection refused
```

Formato: `timestamp_rfc3339 [prioridad] programa: mensaje_sin_newline_final`

<details><summary>Predicción</summary>

```bash
template(name="CompactFormat" type="string"
    string="%timegenerated:::date-rfc3339% [%syslogseverity-text%] %programname%: %msg:::drop-last-lf%\n"
)
```

Desglose:
- `%timegenerated:::date-rfc3339%` → `2026-03-25T10:30:00+00:00`
- `[%syslogseverity-text%]` → `[err]`
- `%programname%` → `nginx`
- `%msg:::drop-last-lf%` → el mensaje sin salto de línea final
- `\n` → salto de línea explícito al final de la línea

Para usarlo:

```bash
*.* action(type="omfile" file="/var/log/compact.log" template="CompactFormat")
```

</details>

### Ejercicio 8 — dynaFile

Un servidor recibe logs de 3 hosts: `web01`, `web02` y `db01`. Diseña un template dynaFile que:
1. Organice los logs en `/var/log/remote/HOSTNAME/PROGRAMA.log`
2. Use formato ISO 8601 con la IP de origen incluida

Escribe la configuración completa (template de ruta + template de formato + acción).

<details><summary>Predicción</summary>

```bash
# Template para la ruta dinámica:
template(name="RemoteLogPath" type="string"
    string="/var/log/remote/%HOSTNAME%/%programname%.log"
)

# Template para el formato de cada línea:
template(name="RemoteLineFormat" type="string"
    string="%timegenerated:::date-rfc3339% %fromhost-ip% %programname%: %msg:::drop-last-lf%\n"
)

# Acción: solo para logs remotos
if $fromhost-ip != "127.0.0.1" then {
    action(type="omfile"
           dynaFile="RemoteLogPath"
           template="RemoteLineFormat"
           createDirs="on"
           dirCreateMode="0750"
           fileCreateMode="0640")
    stop
}
```

Resultado: archivos como `/var/log/remote/web01/nginx.log` con líneas como:
```
2026-03-25T10:30:00+00:00 192.168.1.10 nginx: GET /index.html 200
```

Notas:
- `dynaFile` usa un template que genera la **ruta** del archivo
- `template` en la acción controla el **formato** de cada línea
- Son dos templates separados con propósitos diferentes
- `createDirs="on"` permite que rsyslog cree `/var/log/remote/web01/` automáticamente

</details>

### Ejercicio 9 — Template JSON

Observa este template JSON:

```bash
template(name="MyJSON" type="list") {
    constant(value="{\"time\":\"")
    property(name="timegenerated" dateFormat="rfc3339")
    constant(value="\",\"msg\":\"")
    property(name="msg")
    constant(value="\"}\n")
}
```

**a)** Si el mensaje contiene una comilla doble (`He said "hello"`), ¿qué problema ocurre?
**b)** ¿Cómo lo solucionas?

<details><summary>Predicción</summary>

**a)** El JSON generado sería inválido:

```json
{"time":"2026-03-25T10:30:00+00:00","msg":"He said "hello""}
```

Las comillas dentro del mensaje rompen la estructura JSON. Un parser JSON rechazaría esto.

**b)** Usar `format="jsonf"` en la propiedad `msg`:

```bash
property(name="msg" format="jsonf")
```

`format="jsonf"` escapa automáticamente los caracteres especiales de JSON (comillas → `\"`, backslashes → `\\`, etc.). El resultado correcto sería:

```json
{"time":"2026-03-25T10:30:00+00:00","msg":"He said \"hello\""}
```

</details>

### Ejercicio 10 — Diseño completo de filtrado

Una empresa tiene estas necesidades:
1. Logs de `nginx` → `/var/log/apps/nginx.log` (no van a syslog)
2. Logs de `postgresql` → `/var/log/apps/pgsql.log` (no van a syslog)
3. Todo lo que contenga `"error"` o `"fail"` (de cualquier programa) → `/var/log/alerts.log` (con `sync="on"`)
4. Todo lo demás sigue las reglas normales de rsyslog

¿En qué archivo drop-in pondrías esto y por qué? Escribe la configuración completa.

<details><summary>Predicción</summary>

Archivo: `/etc/rsyslog.d/25-apps.conf`
- Numeración 25: antes de las reglas default de rsyslog.conf (que están después del `$IncludeConfig`)
- Después de posibles inputs remotos (10-) y reglas de seguridad (20-)

```bash
# /etc/rsyslog.d/25-apps.conf

# 1. nginx a su archivo, sin syslog:
if $programname == "nginx" then {
    action(type="omfile"
           file="/var/log/apps/nginx.log"
           template="RSYSLOG_FileFormat"
           createDirs="on")
    stop
}

# 2. postgresql a su archivo, sin syslog:
if $programname == "postgresql" then {
    action(type="omfile"
           file="/var/log/apps/pgsql.log"
           template="RSYSLOG_FileFormat"
           createDirs="on")
    stop
}

# 3. Errores de cualquier programa (SIN stop — siguen a syslog):
if $msg contains "error" or $msg contains "fail" then {
    action(type="omfile"
           file="/var/log/alerts.log"
           sync="on")
}
```

Orden crítico:
- Las reglas de nginx y postgresql van **antes** que la regla de errores, con `stop`
- Si un error de nginx dice "error", solo aparece en `nginx.log` (el `stop` impide que llegue a la regla 3)
- Si un error de sshd dice "error", aparece en `alerts.log` Y en syslog (sin `stop`)
- Si se quiere que los errores de nginx también vayan a `alerts.log`, mover la regla 3 **antes** de las reglas 1 y 2

</details>
