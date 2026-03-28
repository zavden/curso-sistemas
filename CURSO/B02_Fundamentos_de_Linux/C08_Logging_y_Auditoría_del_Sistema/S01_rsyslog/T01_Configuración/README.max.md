# T01 — Configuración de rsyslog

> **Objetivo:** Dominar la arquitectura de rsyslog, sus módulos, la sintaxis de reglas (facility.priority action), y saber configurar el enrutamiento de logs.

## Qué es rsyslog

**rsyslog** (Remote Syslog) es el daemon de logging más extendido en Linux. Es el descendiente moderno de syslogd original (existen desde Unix de los 80s). Recibe mensajes de log de múltiples fuentes y los enruta a destinos diversos:

```
┌────────────────────────────────────────────────────────────────────┐
│                         FUENTES DE LOG                             │
│                                                                    │
│  Aplicaciones ──► syslog() ──┬──► /dev/log ──► imuxsock ──┐     │
│  (openlog)                   │                              │     │
│                              └──► /dev/kmsg ──► imklog ──┬─┘     │
│                                                            │      │
│  journald ──► ForwardToSyslog ──┬──────────────────────► │      │
│                                  │                          │      │
│  Red ──► TCP/UDP ───────────────┼──► imtcp/imudp ────────┘      │
│                                                                    │
│                           rsyslogd (daemon)                       │
│                                  │                                 │
│              ┌───────────────────┼───────────────────┐             │
│              │                   │                   │             │
│              ▼                   ▼                   ▼             │
│       /var/log/syslog     @@servidor:514      :ommysql:*          │
│       /var/log/auth.log   (reenvío TCP)       (base de datos)     │
│       /var/log/kern.log   omelasticsearch                        │
│              │                   (Elasticsearch)                   │
│              ▼                                                  │
│       logrotate                                                 │
└────────────────────────────────────────────────────────────────────┘
```

```bash
# Verificar estado:
systemctl status rsyslog

# Versión:
rsyslogd -v
# rsyslogd 8.2306.0
```

---

## El modelo syslog (RFC 5424)

Cada mensaje syslog tiene **tres componentes**:

```
┌─────────────────────────────────────────────────────────────────┐
│  FACILITY (0-23)     →  QUIÉN genera el mensaje                │
│  PRIORITY (0-7)      →  GRAVEDAD del mensaje                   │
│  ACTION              →  QUÉ hacer con el mensaje                │
└─────────────────────────────────────────────────────────────────┘
```

### Facility — Quién genera el log

| Código | Nombre | Descripción | Uso típico |
|--------|--------|-------------|------------|
| 0 | `kern` | Kernel | Mensajes del kernel (dmesg) |
| 1 | `user` | Usuario | Programas de usuario (default si no se especifica) |
| 2 | `mail` | Correo | Sendmail, Postfix, Exim |
| 3 | `daemon` | Daemons | Servicios del sistema (cups, bluetooth) |
| 4 | `auth` | Autenticación | login, su, sudo, ssh |
| 5 | `syslog` | Syslog | Mensajes internos del propio rsyslog |
| 6 | `lpr` | Impresión | CUPS, dememonios de impresión |
| 7 | `news` | News | Servidores NNTP (usado raramente) |
| 8 | `uucp` | UUCP | UUCP (usado raramente) |
| 9 | `cron` | Cron | демон cron |
| 10 | `authpriv` | Auth privado | sshd, pam (mensajes sensibles) |
| 11 | `ftp` | FTP | vsftpd, proftpd |
| 12-15 | — | Reservados | |
| 16-23 | `local0-local7` | **Custom** | Para tus aplicaciones |

```bash
# Las facilities local0-7 son para uso personalizado:
# local0 = mi aplicación web
# local1 = mi base de datos custom
# local2 = mi API
logger -p local0.info "mensaje de mi app"
```

### Priority — Gravedad del mensaje

| Código | Nombre | Descripción | Ejemplo |
|--------|--------|-------------|---------|
| 0 | `emerg` | Sistema inutilizable | Kernel panic |
| 1 | `alert` | Acción inmediata necesaria | Disco lleno imminent |
| 2 | `crit` | Condición crítica | Hardware error detectado |
| 3 | `err` | Error de operación | "Connection refused" |
| 4 | `warning` | Advertencia | "Retry operation" |
| 5 | `notice` | Normal pero significativo | Root login exitoso |
| 6 | `info` | Informacional | "Service started" |
| 7 | `debug` | Depuración | Logs de trace |

**Regla importante:** Cuando especificas una prioridad en una regla, incluye **ESA prioridad y todas las MÁS SEVERAS**:

```
mail.warning  →  warning + err + crit + alert + emerg
                (info y debug se excluyen)
```

### Action — Qué hacer con el mensaje

| Action | Descripción |
|--------|-------------|
| `/var/log/file` | Escribir a archivo |
| `-/var/log/file` | Escribir sin sync (buffered, más rápido) |
| `@servidor:514` | Reenviar por UDP |
| `@@servidor:514` | Reenviar por TCP |
| `*` | Enviar a todos los terminales (wall) |
| `:ommysql:...` | Escribir a MySQL |
| `:omelasticsearch:...` | Enviar a Elasticsearch |
| `~` o `stop` | Descartar el mensaje |

---

## /etc/rsyslog.conf

```bash
# Estructura del archivo de configuración:
cat /etc/rsyslog.conf

# Secciones típicas (en orden):
# 1. Módulos (module)
# 2. Configuración global (global)
# 3. Templates (template)
# 4. Reglas (facility.priority action)
# 5. Includes (include)
```

### Módulos

Los módulos extienden la funcionalidad de rsyslog:

```bash
# Módulos de INPUT (cómo recibe logs):
module(load="imuxsock")       # Socket Unix /dev/log (aplicaciones locales)
module(load="imklog")         # Logs del kernel vía /dev/kmsg
module(load="imjournal")      # Leer directamente del journal (RHEL)
module(load="imtcp")          # Recibir por TCP (para remote logging)
module(load="imudp")          # Recibir por UDP

# Módulos de OUTPUT (a dónde envía logs):
module(load="omfile")         # Escribir a archivos (cargado por defecto)
module(load="omfwd")          # Reenviar a otro servidor (cargado por defecto)
module(load="omusock")        # Enviar a Unix socket
module(load="ommysql")        # MySQL output
module(load="ompgsql")        # PostgreSQL output
module(load="omelasticsearch")# Elasticsearch output
module(load="omkafka")        # Kafka output

# Módulos de PARSING (procesar mensajes):
module(load="mmjsonparse")    # Parsear JSON dentro de mensajes
module(load="mmnormalize")    # Normalizar mensajes con liblognorm
```

### Configuración global

```bash
# RainerScript (nuevo, preferido):
global(
    workDirectory="/var/lib/rsyslog"    # directorio de trabajo
    maxMessageSize="64k"                # tamaño máximo de mensaje
    defaultNetstreamDriverCAFile="/etc/rsyslog.d/ca.pem"
)

# Legacy (estilo antiguo, aún válido):
$WorkDirectory /var/lib/rsyslog
$MaxMessageSize 64k

# Permisos de archivos creados:
$FileOwner syslog
$FileGroup adm
$FileCreateMode 0640
$DirCreateMode 0755
$Umask 0022
```

---

## Reglas — sintaxis facility.priority action

Las reglas son el mecanismo central de rsyslog. Cada regla especifica:
- **Selector**: qué mensajes capturar (`facility.priority`)
- **Action**: qué hacer con ellos

### Sintaxis básica

```bash
# formato:  facility.priority    action
auth.info                   /var/log/auth.log
# auth con priority info+ → /var/log/auth.log

kern.*                      /var/log/kern.log
# kernel de cualquier priority → /var/log/kern.log

*.emerg                     *
# cualquier facility, priority emerg → todos los terminales
```

### Operadores de prioridad

```
┌──────────────────────────────────────────────────────────────────┐
│  SIN OPERADOR:  ".prioridad"                                     │
│  → Esta prioridad Y todas las más severas (menor código)         │
│  mail.err   →  err + crit + alert + emerg                        │
│                                                                  │
│  IGUAL:     ".=prioridad"                                        │
│  → SOLO esta prioridad exacta                                    │
│  mail.=info  →  solo info, nada más                             │
│                                                                  │
│  EXCEPTO:   ".!prioridad"                                        │
│  → Todo EXCEPTO esta prioridad y las más severas                │
│  mail.!err  →  debug + notice + warning                         │
│  ⚠️ Operador confuso, evitar uso                                 │
│                                                                  │
│  NONE:      "facility.none"                                      │
│  → No capturar nada de esta facility                            │
│  *.*;auth.none  →  todo excepto auth                            │
└──────────────────────────────────────────────────────────────────┘
```

```bash
# Selector simple:
mail.warning       /var/log/mail.warn
# Equivale a: mail.warning + mail.err + mail.crit + mail.alert + mail.emerg

# Prioridad exacta:
mail.=info        /var/log/mail.info
# Solo info, no notice/warning/err/etc.

# Excluir facility:
*.*;auth.none     /var/log/syslog
# Todo excepto auth → syslog
```

### Múltiples selectores

```bash
# Múltiples facilities (separadas por coma):
auth,authpriv.*    /var/log/auth.log
# auth Y authpriv, cualquier priority

# Múltiples selectores (separados por punto y coma):
kern.warning;mail.err   /var/log/important.log
# kernel warning+  O  mail err+

# El punto y coma permite combinar reglas independientes:
# IMPORTANTE: se evalúan en orden, el primero que hace match gana
```

### El guion (-) para sync

```bash
# SIN guion (sync inmediato — seguro pero lento):
kern.*    /var/log/kern.log
# Cada mensaje hace fsync() → se escribe a disco inmediatamente
# Si el sistema crashea, los logs están seguros
# Pero: I/O de disco por cada mensaje

# CON guion (buffered — rápido pero con riesgo):
kern.*    -/var/log/kern.log
# El kernel bufferea las escrituras
# Menos I/O → más rendimiento
# Riesgo: si crashea, los últimos mensajes pueden perderse

# RECOMENDACIÓN:
#  Sin guion → logs críticos (auth, emerg, kern en producción)
#  Con guion  → logs de alto volumen (mail, daemon, debug)
```

### Reglas típicas de Debian/Ubuntu

```bash
cat /etc/rsyslog.d/50-default.conf

# auth/authpriv → archivo dedicado (más seguro, permisos restrictivos)
auth,authpriv.*                 /var/log/auth.log

# Todo al syslog principal EXCEPTO auth/authpriv
# El "-" significa: no sync inmediato (rendimiento)
*.*;auth,authpriv.none          -/var/log/syslog

# Kernel
kern.*                          -/var/log/kern.log

# Correo
mail.*                          -/var/log/mail.log
mail.err                        /var/log/mail.err

# News (NNTP)
news.*                          -/var/log/news/

# Emergencias → todos los usuarios (wall)
*.emerg                         :omusql:*
```

### Reglas típicas de RHEL/Fedora

```bash
cat /etc/rsyslog.conf

# Messages = todo excepto mail, authpriv, cron
*.info;mail.none;authpriv.none;cron.none    /var/log/messages

# Auth privado → secure
authpriv.*                                   /var/log/secure

# Correo
mail.*                                       -/var/log/maillog

# Cron
cron.*                                       /var/log/cron

# Boot messages
local7.*                                     /var/log/boot.log

# Emergencias
*.emerg                                      :omusql:*
```

---

## Drop-ins — /etc/rsyslog.d/

```bash
# Al final de rsyslog.conf:
$IncludeConfig /etc/rsyslog.d/*.conf

# Archivos se procesan en orden ALFABÉTICO:
ls /etc/rsyslog.d/
# 20-ufw.conf       ← primero
# 50-default.conf   ← después
# 60-myapp.conf     ← último

# Convención de numeración:
# 00-19  → módulos y configuración global
# 20-49  → reglas específicas de aplicaciones
# 50-79  → reglas por defecto del sistema
# 80-99  → reglas finales (catch-all, overrides)
```

```bash
# Crear una regla custom para tu aplicación:
sudo tee /etc/rsyslog.d/30-myapp.conf << 'EOF'
# Mi aplicación usa local0
local0.*    -/var/log/myapp.log

# Excluir local0 del syslog general:
*.*;local0.none    -/var/log/syslog
EOF

# Validar ANTES de reiniciar:
sudo rsyslogd -N1
# rsyslogd: version 8.xxx, config validation run...
# rsyslogd: End of config validation run. Bye.

# Si pasa la validación, aplicar:
sudo systemctl restart rsyslog
```

---

## Templates — formateo avanzado de salida

Los templates controlan el formato de los mensajes de salida:

```bash
# Template legacy (estilo antiguo):
$ActionFileDefaultTemplate RSYSLOG_TraditionalFileFormat

# Template moderno (ISO 8601 con timezone):
$ActionFileDefaultTemplate RSYSLOG_FileFormat

# Template personalizado:
$template myFormat,"%timegenerated% %hostname% %syslogtag%%msg%\n"
$ActionFileDefaultTemplate myFormat

# RainerScript:
template(name="myFormat" type="string"
    string="%timegenerated% %hostname% %syslogtag%%msg%\n")
```

```bash
# Templates para output remoto:
template(name="forwardFormat" type="string"
    string="<%PRI%>%PROTOCOL-VERSION% %TIMESTAMP% %HOSTNAME% %APP-NAME% %PROCID% %MSGID% %STRUCTURED-DATA% %msg%\n")
*.* @@remote-server:514;forwardFormat
```

---

## Enviar logs desde aplicaciones

### logger (desde scripts)

```bash
# Enviar mensaje simple (facility=user, priority=notice):
logger "Backup completado"

# Especificar facility y prioridad:
logger -p local0.info "mi app: servicio iniciado"

# Especificar tag (identificador en syslogtag):
logger -t myapp "conexión establecida"
# Salida: Mar 17 15:30:00 hostname myapp: conexión establecida

# Combinar:
logger -t backup -p local0.err "Backup FALLIDO con código $?"
```

```bash
# Script ejemplo completo:
#!/bin/bash
logger -t backup -p local0.info "Iniciando backup de /datos"
if tar czf /backup/datos.tar.gz /datos 2>/tmp/backup-error.log; then
    logger -t backup -p local0.info "Backup completado exitosamente"
else
    ERROR=$(cat /tmp/backup-error.log)
    logger -t backup -p local0.err "Backup FALLIDO: $ERROR"
fi
```

### Desde código C

```c
#include <syslog.h>

openlog("myapp", LOG_PID, LOG_LOCAL0);
syslog(LOG_INFO, "Aplicación iniciada");
syslog(LOG_ERR, "Error de conexión: %s", strerror(errno));
closelog();

// LOG_PID    → incluir PID en el mensaje
// LOG_LOCAL0 → facility local0
// openlog es opcional (se llama implícitamente en syslog si no se llamó)
```

### Desde Python

```python
import logging
import logging.handlers

logger = logging.getLogger('myapp')
logger.setLevel(logging.INFO)

# Syslog handler
handler = logging.handlers.SysLogHandler(address='/dev/log',
                                          facility=logging.handlers.SysLogHandler.LOG_LOCAL0)
logger.addHandler(handler)

logger.info("Mensaje de info")
logger.error("Mensaje de error")
```

---

## Verificar y diagnosticar

```bash
# Validar configuración:
sudo rsyslogd -N1
# 0 errores = syntax OK

# Ver configuración efectiva completa:
rsyslogd -N1 -o full 2>&1 | head -50

# Modo debug (muy detallado):
sudo rsyslogd -dn 2>&1 | head -100
# Muestra cómo se procesa cada mensaje

# Probar que los logs llegan:
logger -t test "mensaje de prueba"
grep "mensaje de prueba" /var/log/syslog       # Debian
grep "mensaje de prueba" /var/log/messages     # RHEL

# Ver estado del servicio:
systemctl status rsyslog
```

### Troubleshooting común

```bash
# PROBLEMA: Logs no aparecen donde esperado

# 1. Verificar que rsyslog está corriendo:
systemctl is-active rsyslog

# 2. Validar configuración:
sudo rsyslogd -N1

# 3. Verificar que el archivo existe y tiene permisos:
ls -la /var/log/auth.log

# 4. Verificar que hay espacio en disco:
df -h /var/log

# 5. Verificar que no hay otro proceso escribiendo en el mismo archivo:
lsof /var/log/syslog

# 6. Probar con logger:
logger -p auth.info "test"
tail -1 /var/log/auth.log
```

---

## Comparación: Debian vs RHEL

| Aspecto | Debian/Ubuntu | RHEL/Fedora |
|---------|---------------|-------------|
| Archivo principal | `/etc/rsyslog.conf` | `/etc/rsyslog.conf` |
| Reglas por defecto | `/etc/rsyslog.d/50-default.conf` | Dentro de rsyslog.conf |
| Input de journal | `imuxsock` (socket) | `imjournal` (directo) |
| Log principal | `/var/log/syslog` | `/var/log/messages` |
| Auth log | `/var/log/auth.log` | `/var/log/secure` |
| Correo log | `/var/log/mail.log` | `/var/log/maillog` |
| Cron log | `/var/log/cron.log` | `/var/log/cron` |
| Boot log | `/var/log/boot.log` | `/var/log/boot.log` (local7) |
| Dueño de archivos | `syslog:adm` | `root:root` |
| Permisos default | `0640` | `0600` |

---

## Quick reference

```
SELECTOR SYNTAX:
  facility.priority    → priority y superiores (más severas)
  facility.=priority  → SOLO esa priority exacta
  facility.!priority → todo EXCEPTO esa y superiores
  facility.none       → excluir esta facility
  *.*                 → todos los mensajes

FACILITIES:
  kern, user, mail, daemon, auth, syslog, cron, authpriv, local0-7

PRIORITIES (más severas primero):
  emerg(0) > alert(1) > crit(2) > err(3) > warning(4) > notice(5) > info(6) > debug(7)

ACTIONS:
  /var/log/file          → archivo (sync)
  -/var/log/file         → archivo (buffered, más rápido)
  @@server:514           → TCP
  @server:514            → UDP
  *                       → todos los terminales (wall)
  stop/~                 → descartar
```

---

## Ejercicios

### Ejercicio 1 — Explorar la configuración actual

```bash
# 1. ¿Qué módulos tiene cargados?
grep -E "^module\(" /etc/rsyslog.conf /etc/rsyslog.d/*.conf 2>/dev/null
grep -E "^\$ModLoad" /etc/rsyslog.conf /etc/rsyslog.d/*.conf 2>/dev/null

# 2. ¿Cuántas reglas hay?
grep -v '^#' /etc/rsyslog.conf /etc/rsyslog.d/*.conf 2>/dev/null | \
    grep -E '^\S+\.\S+' | wc -l

# 3. ¿Qué archivos de log están configurados?
grep -ohE '/var/log/\S+' /etc/rsyslog.conf /etc/rsyslog.d/*.conf 2>/dev/null | sort -u

# 4. ¿Cuál es la configuración global?
grep -E "^\$WorkDirectory|^\$MaxMessageSize|^\$FileCreateMode" \
    /etc/rsyslog.conf /etc/rsyslog.d/*.conf 2>/dev/null
```

### Ejercicio 2 — Probar logging con logger

```bash
# 1. Enviar mensajes de prueba:
logger -t ejercicio -p user.info "Info message"
logger -t ejercicio -p user.warning "Warning message"
logger -t ejercicio -p user.err "Error message"

# 2. Verificar dónde aparecen:
grep "ejercicio" /var/log/syslog    # Debian
grep "ejercicio" /var/log/messages  # RHEL

# 3. Probar con facility local personalizada:
logger -t myapp -p local0.info "Custom app message"
grep "myapp" /var/log/syslog 2>/dev/null || echo "No aparece en syslog (OK si local0 se excluyó)"
```

### Ejercicio 3 — Validar configuración y errores comunes

```bash
# Validar la configuración actual:
sudo rsyslogd -N1 2>&1
# ¿Hay warnings o errores?

# Simular un error de sintaxis:
# Intentar cargar una configuración con error:
echo "auth.info /var/log/nonexistent/dir.log" | sudo tee /tmp/test.conf
sudo rsyslogd -N1 -f /tmp/test.conf 2>&1
# Debería dar error

# Limpiar:
rm /tmp/test.conf
```

### Ejercicio 4 — Crear regla para aplicación custom

```bash
# Objetivo: que los logs de tu aplicación vayan a /var/log/myapp.log
# sin mezclarse con syslog general

# 1. Crear el archivo de configuración:
cat << 'EOF' | sudo tee /etc/rsyslog.d/60-myapp-custom.conf
# Mi aplicación custom usa local1
local1.*    -/var/log/myapp.log

# Excluir local1 del syslog general:
*.*;local1.none    -/var/log/syslog
EOF

# 2. Validar:
sudo rsyslogd -N1

# 3. Crear el archivo y dar permisos:
sudo touch /var/log/myapp.log
sudo chown syslog:adm /var/log/myapp.log

# 4. Reiniciar:
sudo systemctl restart rsyslog

# 5. Probar:
logger -t myapp -p local1.info "Myapp message test"
grep myapp /var/log/myapp.log

# 6. Verificar que NO aparece en syslog general:
grep local1 /var/log/syslog && echo "PROBLEMA: local1 aparece en syslog" || echo "OK"
```

### Ejercicio 5 — Comparar Debian vs RHEL

```bash
# En sistemas Debian:
# Ver la diferencia entre syslog y messages:
# Debian → syslog es "todo", messages no existe
# RHEL → messages es "todo", syslog no existe

# Identificar el tipo de sistema:
if [ -f /var/log/syslog ]; then
    echo "Distribución basada en Debian"
elif [ -f /var/log/messages ]; then
    echo "Distribución basada en RHEL"
fi

# Ver qué facility va a cada archivo:
grep -E "auth\.|authpriv\.|mail\.|kern\." /etc/rsyslog.d/*.conf 2>/dev/null | head -10
```
