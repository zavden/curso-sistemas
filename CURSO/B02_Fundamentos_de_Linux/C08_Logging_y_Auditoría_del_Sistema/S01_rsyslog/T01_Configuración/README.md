# T01 — Configuración de rsyslog

## Qué es rsyslog

rsyslog es el daemon de logging más extendido en Linux. Recibe mensajes
de log de aplicaciones, el kernel y otros daemons, y los enruta a
archivos, consolas, servidores remotos u otros destinos:

```
Aplicaciones ──── syslog() ────┐
Kernel ──── /dev/kmsg ─────────┤
journald ── ForwardToSyslog ───┼──→ rsyslogd ──→ /var/log/syslog
Remoto ──── TCP/UDP ───────────┤                  /var/log/auth.log
                               │                  /var/log/kern.log
                               │                  servidor remoto
                               │                  base de datos
                               └                  ...
```

```bash
# Verificar que rsyslog está activo:
systemctl status rsyslog     # Debian/Ubuntu
systemctl status rsyslog     # RHEL/Fedora (mismo nombre)

# Versión:
rsyslogd -v
# rsyslogd 8.2xxx.x
```

## El modelo syslog

rsyslog implementa y extiende el protocolo syslog (RFC 5424). Cada
mensaje tiene tres componentes:

### Facility (facilidad) — Quién genera el log

```bash
# Las facilities identifican el ORIGEN del mensaje:
# kern     (0)  — kernel
# user     (1)  — programas de usuario
# mail     (2)  — sistema de correo
# daemon   (3)  — daemons del sistema
# auth     (4)  — autenticación (login, sudo, ssh)
# syslog   (5)  — rsyslog mismo
# lpr      (6)  — impresión
# news     (7)  — noticias (NNTP)
# uucp     (8)  — UUCP
# cron     (9)  — cron
# authpriv (10) — autenticación privada
# ftp      (11) — FTP
# local0-7 (16-23) — uso personalizado

# local0-7 son para tus aplicaciones:
# local0 = mi app web
# local1 = mi base de datos custom
# ...
```

### Priority (prioridad/severidad) — Gravedad del mensaje

```bash
# Las prioridades, de más severa a menos:
# emerg   (0) — sistema inutilizable
# alert   (1) — acción inmediata necesaria
# crit    (2) — condición crítica
# err     (3) — error
# warning (4) — advertencia
# notice  (5) — normal pero significativo
# info    (6) — informacional
# debug   (7) — depuración

# Cuando especificas una prioridad en una regla,
# incluye ESA prioridad y todas las MÁS SEVERAS:
# auth.warning → auth.warning + auth.err + auth.crit + auth.alert + auth.emerg
```

### Action (acción) — Qué hacer con el mensaje

```bash
# Destinos posibles:
# /var/log/syslog         — escribir en un archivo
# /dev/console            — enviar a la consola
# @logserver:514          — enviar por UDP a un servidor
# @@logserver:514         — enviar por TCP a un servidor
# :omusql:                — escribir en base de datos
# ~                       — descartar (legacy, usar stop en su lugar)
# *                       — enviar a todos los terminales (wall)
```

## /etc/rsyslog.conf

```bash
# El archivo principal de configuración:
cat /etc/rsyslog.conf

# Estructura típica:
# 1. Módulos (modules)
# 2. Configuración global (global directives)
# 3. Templates
# 4. Reglas (rules)
# 5. Include de archivos adicionales
```

### Módulos

```bash
# Los módulos se cargan al inicio de rsyslog.conf:

# Módulos de input (recibir logs):
module(load="imuxsock")    # socket Unix /dev/log (apps locales)
module(load="imklog")      # logs del kernel (/dev/kmsg)
module(load="imjournal")   # leer directamente del journal (RHEL)
module(load="imtcp")       # recibir por TCP
module(load="imudp")       # recibir por UDP

# Módulos de output (enviar logs):
module(load="omfile")      # escribir a archivos (cargado por defecto)
module(load="omfwd")       # reenviar a otro servidor (cargado por defecto)
module(load="ommysql")     # escribir en MySQL
module(load="omelasticsearch") # enviar a Elasticsearch

# Módulos de parseo:
module(load="mmjsonparse") # parsear JSON dentro de mensajes
```

### Configuración global

```bash
# Directivas que afectan a todo rsyslog:

# Formato nuevo (RainerScript):
global(
    workDirectory="/var/lib/rsyslog"     # directorio de trabajo
    maxMessageSize="64k"                 # tamaño máximo de mensaje
)

# Formato legacy (también válido):
$WorkDirectory /var/lib/rsyslog
$MaxMessageSize 64k

# Otras directivas comunes:
$FileOwner syslog
$FileGroup adm
$FileCreateMode 0640
$DirCreateMode 0755
$Umask 0022
```

## Reglas — facilidad.prioridad acción

Las reglas son el corazón de rsyslog. Cada regla tiene un **selector**
(qué mensajes capturar) y una **acción** (qué hacer con ellos):

### Sintaxis básica

```bash
# facilidad.prioridad    acción
auth.info               /var/log/auth.log
# Todos los mensajes de auth con prioridad info o superior → auth.log

kern.*                  /var/log/kern.log
# Todos los mensajes del kernel (cualquier prioridad) → kern.log

*.emerg                 :omusql:*
# Mensajes de emergencia de cualquier facility → todos los terminales
```

### Operadores de prioridad

```bash
# .prioridad — esta prioridad y todas las superiores (más severas):
mail.warning            /var/log/mail.warn
# warning + err + crit + alert + emerg

# .=prioridad — SOLO esta prioridad exacta:
mail.=info              /var/log/mail.info
# Solo info, no warning ni superior

# .!prioridad — todo EXCEPTO esta y las superiores:
mail.!info              /var/log/mail.not-info
# Solo debug y notice (todo por debajo de info)
# NOTA: este operador es poco común y confuso, evitarlo

# .none — no capturar nada de esta facilidad:
*.*;auth.none           /var/log/syslog
# Todo excepto auth → syslog
```

### Múltiples facilidades

```bash
# Separar con coma:
auth,authpriv.*         /var/log/auth.log
# auth Y authpriv, cualquier prioridad

# Separar con punto y coma para diferentes selectores:
kern.warning;mail.err   /var/log/important.log
# kernel warnings+ O mail errors+
```

### Reglas típicas de Debian/Ubuntu

```bash
# /etc/rsyslog.d/50-default.conf (Debian):

auth,authpriv.*                 /var/log/auth.log
# Autenticación → auth.log

*.*;auth,authpriv.none          -/var/log/syslog
# Todo excepto auth → syslog (el - desactiva sync para rendimiento)

kern.*                          -/var/log/kern.log
# Kernel → kern.log

mail.*                          -/var/log/mail.log
# Correo → mail.log

*.emerg                         :omusql:*
# Emergencias → todos los terminales
```

### Reglas típicas de RHEL/Fedora

```bash
# /etc/rsyslog.conf (RHEL, todo en un archivo):

*.info;mail.none;authpriv.none;cron.none    /var/log/messages
# Todo info+ excepto mail, auth y cron → messages

authpriv.*                                   /var/log/secure
# Auth → secure

mail.*                                       -/var/log/maillog
# Correo → maillog

cron.*                                       /var/log/cron
# Cron → cron

*.emerg                                      :omusql:*
# Emergencias → todos

local7.*                                     /var/log/boot.log
# Boot → boot.log
```

## Drop-ins — /etc/rsyslog.d/

```bash
# rsyslog incluye archivos de un directorio drop-in:
# Al final de /etc/rsyslog.conf:
$IncludeConfig /etc/rsyslog.d/*.conf

# Los archivos se procesan en orden alfabético:
ls /etc/rsyslog.d/
# 20-ufw.conf
# 50-default.conf
# Primero se aplica 20-ufw, luego 50-default

# Convención de numeración:
# 00-19: módulos y configuración global
# 20-49: reglas específicas de aplicaciones
# 50-79: reglas por defecto del sistema
# 80-99: reglas finales (catch-all)
```

```bash
# Agregar una regla custom:
sudo tee /etc/rsyslog.d/30-myapp.conf << 'EOF'
# Logs de myapp a un archivo dedicado
local0.*    /var/log/myapp.log

# No incluir myapp en syslog:
# (agregar local0.none al selector de syslog en 50-default.conf)
EOF

# Verificar la sintaxis antes de reiniciar:
rsyslogd -N1
# rsyslogd: version 8.xxx, config validation run...
# rsyslogd: End of config validation run. Bye.

# Reiniciar rsyslog:
sudo systemctl restart rsyslog
```

## El guion (-) antes de archivos

```bash
# Escritura normal (con sync):
kern.*    /var/log/kern.log
# Cada escritura se sincroniza a disco inmediatamente (fsync)
# Más seguro: si el sistema crashea, los logs están en disco
# Más lento: cada mensaje causa I/O de disco

# Escritura sin sync (buffered):
kern.*    -/var/log/kern.log
# El kernel bufferea las escrituras
# Más rápido: menos I/O
# Riesgo: si crashea, los últimos logs pueden perderse

# Recomendación:
# - Sin guion para logs críticos (auth, emerg)
# - Con guion para logs de alto volumen (mail, debug)
```

## Enviar logs desde aplicaciones

### Con logger (desde scripts)

```bash
# logger envía un mensaje a syslog:
logger "Backup completado exitosamente"
# Aparece en /var/log/syslog con facility user, priority notice

# Especificar facility y prioridad:
logger -p local0.info "myapp: servicio iniciado"
# Usa facility local0, prioridad info

# Especificar tag (identificador):
logger -t myapp "conexión establecida"
# Mar 17 15:30:00 server myapp: conexión establecida

# Desde un script:
#!/bin/bash
logger -t backup -p local0.info "Iniciando backup"
/opt/backup/run.sh
if [ $? -eq 0 ]; then
    logger -t backup -p local0.info "Backup completado"
else
    logger -t backup -p local0.err "Backup FALLIDO"
fi
```

### Desde código C

```bash
# Las aplicaciones usan la API syslog():
# openlog("myapp", LOG_PID, LOG_LOCAL0);
# syslog(LOG_INFO, "mensaje: %s", variable);
# closelog();

# rsyslog recibe estos mensajes a través del socket /dev/log
```

## Verificar y diagnosticar

```bash
# Verificar la configuración:
rsyslogd -N1
# Reporta errores de sintaxis

# Ver la configuración efectiva:
rsyslogd -N1 -o full 2>&1 | head -50

# Modo debug (muy verboso):
rsyslogd -dn 2>&1 | head -100
# Muestra cómo se procesan los mensajes

# Probar que los logs llegan:
logger -t test "mensaje de prueba"
grep "mensaje de prueba" /var/log/syslog    # Debian
grep "mensaje de prueba" /var/log/messages  # RHEL

# Ver el estado del servicio:
systemctl status rsyslog
```

## Debian vs RHEL

| Aspecto | Debian/Ubuntu | RHEL/Fedora |
|---|---|---|
| Archivo principal | /etc/rsyslog.conf | /etc/rsyslog.conf |
| Reglas por defecto | /etc/rsyslog.d/50-default.conf | En rsyslog.conf directamente |
| Input de journal | imuxsock (socket) | imjournal (directo) |
| Log principal | /var/log/syslog | /var/log/messages |
| Log de auth | /var/log/auth.log | /var/log/secure |
| Log de correo | /var/log/mail.log | /var/log/maillog |
| Log de cron | En syslog | /var/log/cron |
| Propietario de logs | syslog:adm | root:root |
| Permisos | 0640 | 0600 |

---

## Ejercicios

### Ejercicio 1 — Explorar la configuración

```bash
# 1. ¿Qué módulos tiene cargados rsyslog?
grep -E "^\\\$ModLoad|^module\(" /etc/rsyslog.conf 2>/dev/null
grep -rE "^\\\$ModLoad|^module\(" /etc/rsyslog.d/ 2>/dev/null

# 2. ¿Cuántas reglas de enrutamiento hay?
grep -v '^#' /etc/rsyslog.conf /etc/rsyslog.d/*.conf 2>/dev/null | \
    grep -E '^\S+\.\S+' | wc -l

# 3. ¿Qué archivos de log están configurados?
grep -ohE '/var/log/\S+' /etc/rsyslog.conf /etc/rsyslog.d/*.conf 2>/dev/null | sort -u
```

### Ejercicio 2 — Probar el logging

```bash
# 1. Enviar un mensaje de prueba:
logger -t ejercicio -p local0.info "Prueba de rsyslog"

# 2. ¿Dónde apareció?
grep "Prueba de rsyslog" /var/log/syslog /var/log/messages 2>/dev/null

# 3. Enviar con diferentes prioridades y verificar:
logger -p user.err "Esto es un error"
logger -p user.debug "Esto es debug"
```

### Ejercicio 3 — Verificar sintaxis

```bash
# Verificar que la configuración actual es válida:
sudo rsyslogd -N1 2>&1
# ¿Hay warnings o errores?
```
