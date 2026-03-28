# T02 — Filtros y templates

## Más allá de facilidad.prioridad

Las reglas básicas de rsyslog (`facilidad.prioridad acción`) son
limitadas. rsyslog ofrece dos mecanismos más potentes para seleccionar
y formatear mensajes:

```
Mensaje entrante
      │
      ▼
  ¿Coincide con un filtro?  ──── Filtro de facilidad.prioridad
      │                          Filtro basado en propiedades
      │                          Expresión RainerScript
      ▼
  Formatear con template  ──── ¿Qué formato tiene la línea de log?
      │
      ▼
  Ejecutar acción         ──── Archivo, remoto, BD, descarte...
```

## Property-based filters

Los filtros basados en propiedades permiten seleccionar mensajes por
cualquier campo del mensaje syslog:

### Sintaxis

```bash
# :propiedad, operador, "valor"    acción

:programname, isequal, "sshd"      /var/log/sshd.log
# Todos los mensajes cuyo programname sea exactamente "sshd"

:msg, contains, "error"            /var/log/errors.log
# Todos los mensajes que contengan "error" en el texto

:hostname, isequal, "webserver01"  /var/log/webserver01.log
# Mensajes del host "webserver01" (útil para log central)
```

### Operadores disponibles

```bash
# Comparación de strings:
:programname, isequal, "nginx"         # igual exacto
:programname, startswith, "php"        # empieza con
:msg, contains, "failed"               # contiene

# Negación:
:msg, !contains, "debug"              # NO contiene

# Expresiones regulares (ERE):
:msg, regex, "error|fail|crit"        # coincide con regex
:msg, ereregex, "user [0-9]+ login"   # ERE explícito

# Comparación case-insensitive:
:msg, contains_i, "ERROR"             # contiene (sin importar mayúsculas)
```

### Propiedades del mensaje

```bash
# Propiedades más usadas:
# msg            — el texto del mensaje
# programname    — nombre del programa que generó el log
# hostname       — hostname del origen
# syslogfacility-text — facility como texto (kern, auth, mail...)
# syslogseverity-text — prioridad como texto (err, warning, info...)
# fromhost       — host del que se recibió (para logs remotos)
# fromhost-ip    — IP del host origen
# timegenerated  — timestamp de cuando rsyslog recibió el mensaje
# timereported   — timestamp del mensaje original
# syslogtag      — tag completo (ej: "sshd[1234]:")
# pri            — prioridad numérica
# $!             — campos del mensaje estructurado (JSON)
```

### Ejemplos prácticos

```bash
# Separar logs de nginx a su propio archivo:
:programname, isequal, "nginx"     /var/log/nginx/access.log
# Y no incluirlos en syslog (parar procesamiento):
:programname, isequal, "nginx"     stop

# Capturar intentos de login fallidos:
:msg, contains, "authentication failure"   /var/log/auth-failures.log
:msg, contains, "Failed password"          /var/log/auth-failures.log

# Separar por host (servidor central):
:fromhost-ip, isequal, "192.168.1.10"  /var/log/remote/web.log
:fromhost-ip, isequal, "192.168.1.20"  /var/log/remote/db.log
:fromhost-ip, startswith, "192.168.1." /var/log/remote/other.log

# Descartar mensajes ruidosos:
:programname, isequal, "systemd-resolved" stop
:msg, contains, "UFW BLOCK"               stop
```

## La acción stop

```bash
# "stop" termina el procesamiento del mensaje para la regla actual
# El mensaje no se evalúa contra las reglas siguientes

# Ejemplo — enviar sshd a su archivo y NO a syslog:
:programname, isequal, "sshd"   /var/log/sshd.log
:programname, isequal, "sshd"   stop
# Línea 1: escribe en sshd.log
# Línea 2: para el procesamiento (no llega a syslog)

# Sin el "stop", el mensaje también aparecería en /var/log/syslog
# (si hay una regla *.* que captura todo)

# Equivalente legacy (obsoleto):
:programname, isequal, "sshd"   ~
# El ~ era el "discard" antiguo, reemplazado por "stop"
```

## RainerScript

RainerScript es el lenguaje de configuración moderno de rsyslog. Permite
condiciones, bucles y lógica compleja:

### Sintaxis básica

```bash
# Bloque if/then:
if $programname == "sshd" then {
    action(type="omfile" file="/var/log/sshd.log")
    stop
}

# if/then/else:
if $syslogseverity <= 3 then {
    # err, crit, alert, emerg
    action(type="omfile" file="/var/log/critical.log")
} else {
    action(type="omfile" file="/var/log/normal.log")
}
```

### Operadores

```bash
# Comparación:
# ==          igual
# !=          diferente
# <, >, <=, >= numérico
# contains    contiene string
# startswith  empieza con
# regex       expresión regular

# Lógicos:
# and         Y lógico
# or          O lógico
# not         negación

# Ejemplos:
if $programname == "nginx" and $syslogseverity <= 4 then {
    action(type="omfile" file="/var/log/nginx-warnings.log")
}

if $msg contains "error" or $msg contains "fail" then {
    action(type="omfile" file="/var/log/problems.log")
}

if not ($programname == "systemd" or $programname == "kernel") then {
    action(type="omfile" file="/var/log/apps.log")
}
```

### Variables de mensaje

```bash
# En RainerScript, las propiedades se acceden con $:
# $msg             — texto del mensaje
# $programname     — nombre del programa
# $hostname        — hostname
# $syslogfacility  — facility (numérico)
# $syslogseverity  — prioridad (numérico)
# $fromhost-ip     — IP de origen

# Variables de prioridad (numérico):
# 0=emerg, 1=alert, 2=crit, 3=err, 4=warning, 5=notice, 6=info, 7=debug

if $syslogseverity <= 3 then {
    # Solo err y superiores
    action(type="omfile" file="/var/log/errors-only.log")
}
```

### Ejemplo completo con RainerScript

```bash
# /etc/rsyslog.d/30-custom.conf

# Separar logs por aplicación:
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

# Alertas críticas a un archivo separado:
if $syslogseverity <= 2 then {
    action(type="omfile"
           file="/var/log/critical-alerts.log"
           sync="on")
}
```

## Templates

Los templates definen el **formato** de los mensajes de log escritos
por rsyslog:

### Templates predefinidos

```bash
# rsyslog incluye templates por defecto:

# RSYSLOG_TraditionalFileFormat (default para archivos):
# "Mar 17 15:30:00 server sshd[1234]: mensaje"
# Formato clásico de syslog

# RSYSLOG_FileFormat:
# "2026-03-17T15:30:00.123456+00:00 server sshd[1234]: mensaje"
# Con timestamp ISO 8601 y precisión de microsegundos

# RSYSLOG_SyslogProtocol23Format:
# Formato RFC 5424 completo (para envío remoto moderno)

# RSYSLOG_DebugFormat:
# Debug con todos los campos internos
```

### Crear templates personalizados

```bash
# Sintaxis de template:
template(name="MyFormat" type="string"
    string="%timegenerated:::date-rfc3339% %HOSTNAME% %programname%: %msg%\n"
)

# Usar el template en una regla:
*.* action(type="omfile" file="/var/log/custom.log" template="MyFormat")

# O en sintaxis legacy:
$template MyFormat,"%timegenerated:::date-rfc3339% %HOSTNAME% %programname%: %msg%\n"
*.* /var/log/custom.log;MyFormat
```

### Propiedades en templates

```bash
# Las propiedades se referencian con %propiedad%:
# %msg%               — el mensaje
# %HOSTNAME%          — hostname
# %programname%       — nombre del programa
# %syslogseverity-text% — prioridad como texto
# %syslogfacility-text% — facility como texto
# %timegenerated%     — timestamp
# %fromhost-ip%       — IP de origen
# %syslogtag%         — tag (programa[PID]:)

# Modificadores de propiedades:
# %msg:::drop-last-lf%     — eliminar el último salto de línea
# %timegenerated:::date-rfc3339%  — formato de fecha RFC 3339
# %msg:1:50%               — solo los primeros 50 caracteres
# %programname:::uppercase%  — en mayúsculas
```

### Template con archivo dinámico

```bash
# Un template puede definir el NOMBRE del archivo dinámicamente:
template(name="PerHostLog" type="string"
    string="/var/log/remote/%HOSTNAME%/%programname%.log"
)

# Usar con dynaFile:
*.* action(type="omfile" dynaFile="PerHostLog")
# Crea archivos como:
# /var/log/remote/web01/nginx.log
# /var/log/remote/web01/sshd.log
# /var/log/remote/db01/postgresql.log

# Template por fecha:
template(name="DailyLog" type="string"
    string="/var/log/daily/%$year%-%$month%-%$day%.log"
)
```

### Template JSON

```bash
# Para enviar logs en formato JSON (ej: a Elasticsearch):
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

if $fromhost-ip != "127.0.0.1" then {
    action(type="omfile" dynaFile="RemoteLog")
    stop
}
```

### Separar logs de una aplicación

```bash
# /etc/rsyslog.d/25-myapp.conf

# Template con formato personalizado:
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

# Capturar todos los eventos de autenticación:
if $programname == "sshd" or
   $programname == "sudo" or
   $programname == "su" or
   $programname == "login" or
   $msg contains "authentication" then {
    action(type="omfile"
           file="/var/log/security-audit.log"
           sync="on")
    # No hacer stop — también van a auth.log por la regla normal
}
```

## Orden de procesamiento

```bash
# rsyslog procesa las reglas EN ORDEN de arriba a abajo:
# 1. Primero los archivos con número más bajo en /etc/rsyslog.d/
# 2. Luego rsyslog.conf (o al revés según el $IncludeConfig)
# 3. Un mensaje se evalúa contra TODAS las reglas (salvo que haya stop)

# Esto significa que un mensaje puede escribirse en múltiples archivos:
auth.*                    /var/log/auth.log       # regla 1
*.*;auth.none             /var/log/syslog         # regla 2
# auth.log recibe mensajes de auth (regla 1)
# syslog recibe todo EXCEPTO auth (regla 2, por auth.none)

# Sin auth.none en la regla 2, auth aparecería en AMBOS archivos
```

---

## Ejercicios

### Ejercicio 1 — Leer filtros existentes

```bash
# Ver todas las reglas activas:
grep -v '^#' /etc/rsyslog.conf /etc/rsyslog.d/*.conf 2>/dev/null | \
    grep -v '^$' | grep -v '^\$'
```

### Ejercicio 2 — Templates en uso

```bash
# ¿Qué templates están definidos?
grep -r "template" /etc/rsyslog.conf /etc/rsyslog.d/ 2>/dev/null | grep -v '^#'

# ¿Qué formato tienen los logs actuales?
head -1 /var/log/syslog 2>/dev/null || head -1 /var/log/messages 2>/dev/null
# ¿Es TraditionalFileFormat o FileFormat?
```

### Ejercicio 3 — Probar un filtro

```bash
# Enviar mensajes con diferentes facilities y verificar dónde van:
logger -p local0.info -t test-local0 "Mensaje facility local0"
logger -p auth.info -t test-auth "Mensaje facility auth"
logger -p kern.warning -t test-kern "Mensaje facility kern"

# ¿En qué archivos aparecieron?
for f in /var/log/syslog /var/log/messages /var/log/auth.log /var/log/secure /var/log/kern.log; do
    grep "test-" "$f" 2>/dev/null && echo "  ↑ encontrado en $f"
done
```
