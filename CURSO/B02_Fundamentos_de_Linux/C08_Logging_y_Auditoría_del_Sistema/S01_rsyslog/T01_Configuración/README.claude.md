# T01 — Configuración de rsyslog

## Erratas detectadas en el material base

| Archivo | Línea(s) | Error | Corrección |
|---|---|---|---|
| README.md | 164, 218, 239 | `*.emerg :omusql:*` como acción para "todos los terminales (wall)" | `:omusql:` no existe como módulo rsyslog. La acción correcta para wall es `*` (asterisco solo) o `:omusrmsg:*` en formato moderno. `:ommysql:` sería MySQL. |
| README.max.md | 200, 299, 323 | Mismo error: `*.emerg :omusql:*` | Mismo — la acción correcta es `*`. |
| README.md | 180–181 | `mail.!info` descrito como "Solo debug y notice" | `!info` excluye info y todo lo más severo (notice, warning, err...). Solo queda **debug**. Notice(5) es más severo que info(6). |
| README.max.md | 218 | `mail.!err → debug + notice + warning` | Falta `info`. `!err` excluye err y superiores. Quedan: **debug + info + notice + warning**. |
| README.max.md | 68 | `dememonios de impresión` | **demonios** de impresión (typo) |
| README.max.md | 71 | `демон cron` | Texto en ruso. Correcto: **daemon cron** |
| README.max.md | 90 | `Disco lleno imminent` | `imminent` es inglés. Correcto: **Disco lleno inminente** |

---

## Qué es rsyslog

**rsyslog** (Remote Syslog) es el daemon de logging más extendido en Linux.
Recibe mensajes de log de múltiples fuentes y los enruta a destinos diversos:

```
Aplicaciones ── syslog() ──┐
Kernel ── /dev/kmsg ───────┤
journald ── ForwardTo... ──┼──→ rsyslogd ──→ /var/log/syslog
Remoto ── TCP/UDP ─────────┘                  /var/log/auth.log
                                               /var/log/kern.log
                                               servidor remoto
                                               base de datos
```

```bash
# Verificar estado:
systemctl status rsyslog

# Versión:
rsyslogd -v
```

---

## El modelo syslog (RFC 5424)

Cada mensaje syslog tiene **tres componentes**: facility (quién genera),
priority (gravedad) y action (qué hacer).

### Facility — Quién genera el log

| Código | Nombre | Descripción |
|---|---|---|
| 0 | `kern` | Kernel (mensajes de dmesg) |
| 1 | `user` | Programas de usuario (default) |
| 2 | `mail` | Sistema de correo (Postfix, Sendmail) |
| 3 | `daemon` | Daemons del sistema |
| 4 | `auth` | Autenticación (login, su, sudo, ssh) |
| 5 | `syslog` | Mensajes internos de rsyslog |
| 6 | `lpr` | Impresión |
| 7 | `news` | Servidores NNTP (raramente usado) |
| 8 | `uucp` | UUCP (raramente usado) |
| 9 | `cron` | Daemon cron |
| 10 | `authpriv` | Auth privado (sshd, pam — mensajes sensibles) |
| 11 | `ftp` | FTP (vsftpd, proftpd) |
| 12–15 | — | Reservados |
| 16–23 | `local0`–`local7` | **Uso personalizado** para tus aplicaciones |

```bash
# Facilities local0-7 son para uso personalizado:
# local0 = mi aplicación web
# local1 = mi base de datos custom
logger -p local0.info "mensaje de mi app"
```

### Priority — Gravedad del mensaje

| Código | Nombre | Descripción | Ejemplo |
|---|---|---|---|
| 0 | `emerg` | Sistema inutilizable | Kernel panic |
| 1 | `alert` | Acción inmediata necesaria | Disco lleno inminente |
| 2 | `crit` | Condición crítica | Error de hardware |
| 3 | `err` | Error de operación | "Connection refused" |
| 4 | `warning` | Advertencia | "Retry operation" |
| 5 | `notice` | Normal pero significativo | Root login exitoso |
| 6 | `info` | Informacional | "Service started" |
| 7 | `debug` | Depuración | Logs de trace |

**Regla clave:** al especificar una prioridad en una regla, incluye **esa
prioridad y todas las más severas** (número menor):

```
mail.warning  →  warning(4) + err(3) + crit(2) + alert(1) + emerg(0)
                 (info y debug se excluyen)
```

### Action — Qué hacer con el mensaje

| Acción | Descripción |
|---|---|
| `/var/log/file` | Escribir a archivo (con fsync) |
| `-/var/log/file` | Escribir a archivo sin fsync (buffered, más rápido) |
| `@servidor:514` | Reenviar por UDP |
| `@@servidor:514` | Reenviar por TCP |
| `*` | Enviar a todos los terminales (wall) |
| `:ommysql:...` | Escribir a MySQL |
| `:omelasticsearch:...` | Enviar a Elasticsearch |
| `stop` | Descartar el mensaje (legacy: `~`) |

---

## /etc/rsyslog.conf

```bash
# Estructura del archivo de configuración (en orden):
# 1. Módulos (module)
# 2. Configuración global (global)
# 3. Templates (template)
# 4. Reglas (facility.priority action)
# 5. Includes ($IncludeConfig)
```

### Módulos

```bash
# INPUT (cómo recibe logs):
module(load="imuxsock")         # Socket Unix /dev/log (apps locales)
module(load="imklog")           # Logs del kernel vía /dev/kmsg
module(load="imjournal")        # Leer directamente del journal (RHEL)
module(load="imtcp")            # Recibir por TCP (remote logging)
module(load="imudp")            # Recibir por UDP

# OUTPUT (a dónde envía logs):
module(load="omfile")           # Escribir a archivos (default, cargado automáticamente)
module(load="omfwd")            # Reenviar a otro servidor (default, cargado automáticamente)
module(load="ommysql")          # MySQL
module(load="ompgsql")          # PostgreSQL
module(load="omelasticsearch")  # Elasticsearch
module(load="omkafka")          # Kafka

# PARSING (procesar mensajes):
module(load="mmjsonparse")      # Parsear JSON dentro de mensajes
module(load="mmnormalize")      # Normalizar con liblognorm
```

### Configuración global

```bash
# RainerScript (formato nuevo, preferido):
global(
    workDirectory="/var/lib/rsyslog"
    maxMessageSize="64k"
)

# Legacy (formato antiguo, aún válido):
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

## Reglas — facility.priority action

Las reglas son el corazón de rsyslog: un **selector** (qué capturar) y una
**acción** (qué hacer).

### Sintaxis básica

```bash
# facility.priority    action
auth.info              /var/log/auth.log
# auth con prioridad info+ → auth.log

kern.*                 /var/log/kern.log
# todo del kernel (cualquier prioridad) → kern.log

*.emerg                *
# emergencias de cualquier facility → todos los terminales (wall)
```

### Operadores de prioridad

```
.prioridad       esta prioridad y todas las más severas
                 mail.err  →  err + crit + alert + emerg

.=prioridad      SOLO esta prioridad exacta
                 mail.=info  →  solo info

.!prioridad      todo EXCEPTO esta prioridad y las más severas
                 mail.!err  →  warning + notice + info + debug
                 mail.!info →  solo debug
                 (operador confuso, evitar su uso)

.none            no capturar nada de esta facility
                 *.*;auth.none  →  todo excepto auth
```

### Múltiples facilidades y selectores

```bash
# Múltiples facilities con coma:
auth,authpriv.*         /var/log/auth.log
# auth Y authpriv, cualquier prioridad

# Múltiples selectores con punto y coma:
kern.warning;mail.err   /var/log/important.log
# kernel warning+  O  mail err+
```

### El guion (-) — escritura buffered

```bash
# Sin guion (sync inmediato, más seguro):
kern.*    /var/log/kern.log
# Cada mensaje hace fsync() → escritura a disco inmediata
# Si crashea, los logs están seguros — pero más I/O

# Con guion (buffered, más rápido):
kern.*    -/var/log/kern.log
# El kernel bufferea las escrituras — menos I/O
# Riesgo: si crashea, los últimos logs pueden perderse
```

**Recomendación:**
- Sin guion → logs críticos (auth, emerg)
- Con guion → logs de alto volumen (mail, daemon, debug)

---

## Reglas típicas por distribución

### Debian/Ubuntu (`/etc/rsyslog.d/50-default.conf`)

```bash
auth,authpriv.*                 /var/log/auth.log
# Autenticación → archivo dedicado

*.*;auth,authpriv.none          -/var/log/syslog
# Todo excepto auth → syslog (con - para rendimiento)

kern.*                          -/var/log/kern.log
mail.*                          -/var/log/mail.log
mail.err                        /var/log/mail.err

*.emerg                         *
# Emergencias → todos los terminales
```

### RHEL/Fedora (en `/etc/rsyslog.conf` directamente)

```bash
*.info;mail.none;authpriv.none;cron.none    /var/log/messages
# Todo info+ excepto mail, auth y cron → messages

authpriv.*                                   /var/log/secure
mail.*                                       -/var/log/maillog
cron.*                                       /var/log/cron
local7.*                                     /var/log/boot.log

*.emerg                                      *
# Emergencias → todos los terminales
```

---

## Drop-ins — /etc/rsyslog.d/

```bash
# Al final de rsyslog.conf:
$IncludeConfig /etc/rsyslog.d/*.conf

# Los archivos se procesan en orden alfabético
```

| Rango | Uso |
|---|---|
| 00–19 | Módulos y configuración global |
| 20–49 | Reglas específicas de aplicaciones |
| 50–79 | Reglas por defecto del sistema |
| 80–99 | Reglas finales (catch-all) |

### Crear una regla custom

```bash
# Logs de mi aplicación a un archivo dedicado:
sudo tee /etc/rsyslog.d/30-myapp.conf << 'EOF'
local0.*    -/var/log/myapp.log
EOF

# Validar ANTES de reiniciar:
sudo rsyslogd -N1

# Aplicar:
sudo systemctl restart rsyslog
```

**Nota:** para evitar que `local0` aparezca también en syslog, agregar
`local0.none` al selector de syslog en `50-default.conf`.

---

## Templates — formateo de salida

Los templates controlan el formato de los mensajes:

```bash
# Templates predefinidos:
$ActionFileDefaultTemplate RSYSLOG_TraditionalFileFormat    # formato clásico
$ActionFileDefaultTemplate RSYSLOG_FileFormat               # ISO 8601 con timezone

# Template personalizado (legacy):
$template myFormat,"%timegenerated% %hostname% %syslogtag%%msg%\n"
$ActionFileDefaultTemplate myFormat

# Template personalizado (RainerScript):
template(name="myFormat" type="string"
    string="%timegenerated% %hostname% %syslogtag%%msg%\n")
```

---

## Enviar logs desde aplicaciones

### logger (desde scripts)

```bash
# Mensaje simple (facility=user, priority=notice):
logger "Backup completado"

# Con facility y prioridad:
logger -p local0.info "mi app: servicio iniciado"

# Con tag (identificador):
logger -t myapp "conexión establecida"
# Salida: Mar 17 15:30:00 hostname myapp: conexión establecida

# Combinar:
logger -t backup -p local0.err "Backup FALLIDO con código $?"
```

```bash
# Ejemplo en script:
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
```

### Desde Python

```python
import logging
import logging.handlers

logger = logging.getLogger('myapp')
logger.setLevel(logging.INFO)

handler = logging.handlers.SysLogHandler(
    address='/dev/log',
    facility=logging.handlers.SysLogHandler.LOG_LOCAL0
)
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

# Probar que los logs llegan:
logger -t test "mensaje de prueba"
grep "mensaje de prueba" /var/log/syslog       # Debian
grep "mensaje de prueba" /var/log/messages     # RHEL

# Estado del servicio:
systemctl status rsyslog
```

### Troubleshooting común

1. **Verificar que rsyslog está corriendo:** `systemctl is-active rsyslog`
2. **Validar configuración:** `sudo rsyslogd -N1`
3. **Verificar permisos del archivo:** `ls -la /var/log/auth.log`
4. **Verificar espacio en disco:** `df -h /var/log`
5. **Verificar quién escribe en el archivo:** `lsof /var/log/syslog`
6. **Probar con logger:** `logger -p auth.info "test" && tail -1 /var/log/auth.log`

---

## Debian vs RHEL

| Aspecto | Debian/Ubuntu | RHEL/Fedora |
|---|---|---|
| Archivo principal | `/etc/rsyslog.conf` | `/etc/rsyslog.conf` |
| Reglas por defecto | `/etc/rsyslog.d/50-default.conf` | Dentro de rsyslog.conf |
| Input de journal | `imuxsock` (socket) | `imjournal` (directo) |
| Log principal | `/var/log/syslog` | `/var/log/messages` |
| Auth log | `/var/log/auth.log` | `/var/log/secure` |
| Correo log | `/var/log/mail.log` | `/var/log/maillog` |
| Cron log | En syslog | `/var/log/cron` |
| Boot log | `/var/log/boot.log` | `/var/log/boot.log` (local7) |
| Propietario | `syslog:adm` | `root:root` |
| Permisos | `0640` | `0600` |

---

## Quick reference

```
SELECTOR SYNTAX:
  facility.priority    → priority y superiores (más severas)
  facility.=priority   → SOLO esa priority exacta
  facility.!priority   → todo EXCEPTO esa y superiores
  facility.none        → excluir esta facility
  *.*                  → todos los mensajes

FACILITIES:
  kern, user, mail, daemon, auth, syslog, cron, authpriv, local0-7

PRIORITIES (más severas primero):
  emerg(0) > alert(1) > crit(2) > err(3) > warning(4) > notice(5) > info(6) > debug(7)

ACTIONS:
  /var/log/file          → archivo (sync)
  -/var/log/file         → archivo (buffered, más rápido)
  @@server:514           → TCP
  @server:514            → UDP
  *                      → todos los terminales (wall)
  stop                   → descartar
```

---

## Laboratorio

### Parte 1 — Modelo syslog y configuración

#### Paso 1.1: rsyslog en el sistema

```bash
docker compose exec debian-dev bash -c '
echo "=== rsyslog ==="
echo ""
echo "--- Estado del servicio ---"
systemctl is-active rsyslog 2>/dev/null || echo "(rsyslog no activo)"
echo ""
echo "--- Version ---"
rsyslogd -v 2>&1 | head -2
echo ""
echo "--- Archivos de log ---"
ls -la /var/log/syslog /var/log/auth.log /var/log/kern.log 2>/dev/null || \
    ls -la /var/log/messages /var/log/secure 2>/dev/null || \
    echo "(archivos de log no encontrados)"
'
```

#### Paso 1.2: Facility, priority, action

```bash
docker compose exec debian-dev bash -c '
echo "=== Modelo syslog ==="
echo ""
echo "--- Facility (quien genera) ---"
echo "  kern(0) user(1) mail(2) daemon(3) auth(4)"
echo "  syslog(5) cron(9) authpriv(10) local0-7(16-23)"
echo ""
echo "--- Priority (gravedad, mas severa primero) ---"
echo "  emerg(0) alert(1) crit(2) err(3)"
echo "  warning(4) notice(5) info(6) debug(7)"
echo ""
echo "  Regla: facility.priority captura esa y todas las mas severas"
echo "  auth.warning → warning + err + crit + alert + emerg"
echo ""
echo "--- Action (destino) ---"
echo "  /var/log/file     archivo (con fsync)"
echo "  -/var/log/file    archivo (buffered, rapido)"
echo "  @@servidor:514    TCP"
echo "  @servidor:514     UDP"
echo "  *                 todos los terminales (wall)"
echo "  stop              descartar"
'
```

#### Paso 1.3: Estructura de rsyslog.conf

```bash
docker compose exec debian-dev bash -c '
echo "=== /etc/rsyslog.conf ==="
echo ""
echo "Estructura: modules → global → templates → rules → includes"
echo ""
if [[ -f /etc/rsyslog.conf ]]; then
    echo "--- Modulos cargados ---"
    grep -E "^\\\$ModLoad|^module\(load" /etc/rsyslog.conf 2>/dev/null | head -10
    echo ""
    echo "--- Directivas globales ---"
    grep -E "^\\\$|^global\(" /etc/rsyslog.conf 2>/dev/null | head -10
    echo ""
    echo "--- Include ---"
    grep -i "include" /etc/rsyslog.conf 2>/dev/null
fi
'
```

#### Paso 1.4: Módulos

```bash
docker compose exec debian-dev bash -c '
echo "=== Modulos de rsyslog ==="
echo ""
echo "--- Input ---"
echo "  imuxsock    socket /dev/log (apps locales, Debian)"
echo "  imklog      kernel /dev/kmsg"
echo "  imjournal   journal (RHEL)"
echo "  imtcp/imudp recepcion remota"
echo ""
echo "--- Output ---"
echo "  omfile      archivos (default)"
echo "  omfwd       reenvio (default)"
echo "  ommysql     MySQL"
echo ""
echo "--- Cargados en este sistema ---"
grep -rhE "ModLoad|module\(load" /etc/rsyslog.conf /etc/rsyslog.d/ 2>/dev/null | \
    grep -v "^#" | sort -u || echo "(sin modulos encontrados)"
'
```

---

### Parte 2 — Reglas y operadores

#### Paso 2.1: Reglas básicas

```bash
docker compose exec debian-dev bash -c '
echo "=== Reglas de enrutamiento ==="
echo ""
echo "Formato: facility.priority    action"
echo ""
echo "  auth.info             /var/log/auth.log"
echo "  kern.*                /var/log/kern.log"
echo "  *.emerg               *"
echo ""
echo "--- Multiples facilities (coma) ---"
echo "  auth,authpriv.*       /var/log/auth.log"
echo ""
echo "--- Multiples selectores (punto y coma) ---"
echo "  kern.warning;mail.err /var/log/important.log"
'
```

#### Paso 2.2: Operadores de prioridad

```bash
docker compose exec debian-dev bash -c '
echo "=== Operadores de prioridad ==="
echo ""
echo "--- .prioridad (esta y superiores) ---"
echo "  mail.warning → warning + err + crit + alert + emerg"
echo ""
echo "--- .=prioridad (solo esta exacta) ---"
echo "  mail.=info → SOLO info"
echo ""
echo "--- .!prioridad (todo EXCEPTO esta y superiores) ---"
echo "  mail.!err → warning + notice + info + debug"
echo "  mail.!info → SOLO debug"
echo "  (operador confuso, evitar su uso)"
echo ""
echo "--- .none (excluir facility) ---"
echo "  *.*;auth.none → todo excepto auth"
echo ""
echo "--- Reglas reales de este sistema ---"
if [[ -f /etc/rsyslog.d/50-default.conf ]]; then
    echo ""
    grep -v "^#" /etc/rsyslog.d/50-default.conf 2>/dev/null | grep -v "^$"
elif [[ -f /etc/rsyslog.conf ]]; then
    grep -E "^\S+\.\S+\s" /etc/rsyslog.conf 2>/dev/null | head -15
fi
'
```

#### Paso 2.3: Reglas Debian vs RHEL

```bash
docker compose exec debian-dev bash -c '
echo "=== Reglas tipicas — Debian ==="
echo ""
echo "auth,authpriv.*                 /var/log/auth.log"
echo "*.*;auth,authpriv.none          -/var/log/syslog"
echo "kern.*                          -/var/log/kern.log"
echo "mail.*                          -/var/log/mail.log"
echo "*.emerg                         *"
'
echo ""
docker compose exec alma-dev bash -c '
echo "=== Reglas tipicas — RHEL ==="
echo ""
echo "*.info;mail.none;authpriv.none;cron.none   /var/log/messages"
echo "authpriv.*                                  /var/log/secure"
echo "mail.*                                      -/var/log/maillog"
echo "cron.*                                      /var/log/cron"
echo "*.emerg                                     *"
echo ""
echo "--- Reglas reales ---"
grep -E "^\S+\.\S+\s" /etc/rsyslog.conf 2>/dev/null | grep -v "^#" | head -10
'
```

#### Paso 2.4: Drop-ins y el guion (-)

```bash
docker compose exec debian-dev bash -c '
echo "=== Drop-ins /etc/rsyslog.d/ ==="
echo ""
echo "Archivos procesados en orden alfabetico."
echo "  00-19: modulos/config  20-49: apps"
echo "  50-79: defaults        80-99: catch-all"
echo ""
echo "--- Archivos en este sistema ---"
ls -la /etc/rsyslog.d/ 2>/dev/null || echo "(no encontrado)"
echo ""
echo "=== El guion (-) ==="
echo ""
echo "Sin guion: fsync por cada mensaje (seguro, lento)"
echo "Con guion: buffered (rapido, riesgo si crashea)"
echo ""
echo "  Sin guion → logs criticos (auth, emerg)"
echo "  Con guion → logs de alto volumen (mail, debug)"
echo ""
echo "--- En este sistema ---"
echo "Buffered (con -):"
grep -rh "^\S.*\s\+-/" /etc/rsyslog.conf /etc/rsyslog.d/ 2>/dev/null | \
    grep -v "^#" | head -5 || echo "  (ninguno)"
'
```

---

### Parte 3 — logger y diagnóstico

#### Paso 3.1: logger — enviar mensajes

```bash
docker compose exec debian-dev bash -c '
echo "=== logger ==="
echo ""
logger "Mensaje de prueba desde el lab"
echo "Enviado: logger \"Mensaje de prueba desde el lab\""
sleep 1
echo ""
echo "--- Verificar ---"
grep "Mensaje de prueba desde el lab" /var/log/syslog 2>/dev/null | tail -1 || \
    grep "Mensaje de prueba desde el lab" /var/log/messages 2>/dev/null | tail -1 || \
    echo "(mensaje no encontrado)"
echo ""
echo "--- Con facility, prioridad y tag ---"
logger -t backup -p local0.info "backup completado"
sleep 1
grep "backup" /var/log/syslog 2>/dev/null | tail -1 || \
    echo "(buscar en el log correspondiente)"
'
```

#### Paso 3.2: logger con diferentes prioridades

```bash
docker compose exec debian-dev bash -c '
echo "=== logger con prioridades ==="
echo ""
for prio in emerg alert crit err warning notice info debug; do
    logger -t lab-test -p "user.$prio" "Prueba prioridad: $prio"
    echo "  Enviado: user.$prio"
done
sleep 1
echo ""
echo "--- Donde aparecieron ---"
for logfile in /var/log/syslog /var/log/messages; do
    [[ -f "$logfile" ]] || continue
    COUNT=$(grep -c "lab-test.*Prueba prioridad" "$logfile" 2>/dev/null || echo 0)
    [[ "$COUNT" -gt 0 ]] && echo "  $logfile: $COUNT mensajes"
done
'
```

#### Paso 3.3: Verificar configuración

```bash
docker compose exec debian-dev bash -c '
echo "=== Verificar configuracion ==="
echo ""
echo "--- rsyslogd -N1 (validar sintaxis) ---"
rsyslogd -N1 2>&1
echo ""
echo "--- Archivos de log configurados ---"
grep -rohE "/var/log/\S+" /etc/rsyslog.conf /etc/rsyslog.d/*.conf 2>/dev/null | sort -u
echo ""
echo "--- Archivos existentes ---"
ls /var/log/*.log /var/log/syslog /var/log/messages 2>/dev/null
'
```

#### Paso 3.4: Debian vs RHEL

```bash
docker compose exec debian-dev bash -c '
echo "=== Debian ==="
echo "Input: $(grep -rhE "imuxsock|imjournal" /etc/rsyslog.conf /etc/rsyslog.d/ 2>/dev/null | grep -v "^#" | head -1)"
echo "Log principal: /var/log/syslog"
ls -la /var/log/syslog 2>/dev/null | awk "{print \"Propietario: \" \$3 \":\" \$4 \" Permisos: \" \$1}"
'
echo ""
docker compose exec alma-dev bash -c '
echo "=== RHEL ==="
echo "Input: $(grep -rhE "imuxsock|imjournal" /etc/rsyslog.conf /etc/rsyslog.d/ 2>/dev/null | grep -v "^#" | head -1)"
echo "Log principal: /var/log/messages"
ls -la /var/log/messages 2>/dev/null | awk "{print \"Propietario: \" \$3 \":\" \$4 \" Permisos: \" \$1}"
'
```

---

## Ejercicios

### Ejercicio 1 — Componentes de un mensaje syslog

¿Cuáles son los tres componentes de un mensaje syslog? Si un programa hace
`logger -p auth.warning "intento fallido"`, ¿cuál es la facility, la
priority y dónde iría en Debian?

<details><summary>Predicción</summary>

Los tres componentes: **facility** (quién genera), **priority** (gravedad) y
**action** (destino).

Para `logger -p auth.warning "intento fallido"`:
- **Facility:** `auth` (4)
- **Priority:** `warning` (4)
- **Destino en Debian:** `/var/log/auth.log` porque la regla
  `auth,authpriv.* /var/log/auth.log` captura todos los mensajes de auth
  independientemente de la prioridad.

</details>

### Ejercicio 2 — Regla con operador

¿Qué mensajes captura `mail.=info /var/log/mail.info`? ¿Y
`mail.info /var/log/mail.log`?

<details><summary>Predicción</summary>

- **`mail.=info`**: captura **solo** mensajes de mail con prioridad info
  exacta. El operador `=` limita a esa prioridad.
- **`mail.info`**: captura mensajes de mail con prioridad info **y todas las
  más severas** (info + notice + warning + err + crit + alert + emerg).

Sin el `=`, una regla incluye la prioridad indicada y todas las superiores
(más severas).

</details>

### Ejercicio 3 — Operador de exclusión

¿Qué prioridades captura `mail.!err`? ¿Y `mail.!info`?

<details><summary>Predicción</summary>

- **`mail.!err`**: excluye err(3) y todas las más severas (crit, alert,
  emerg). Captura: **warning(4) + notice(5) + info(6) + debug(7)**.
- **`mail.!info`**: excluye info(6) y todas las más severas (notice,
  warning, err, crit, alert, emerg). Captura: solo **debug(7)**.

El operador `!` es confuso porque la escala numérica va al revés de la
intuición. Se recomienda evitarlo.

</details>

### Ejercicio 4 — Excluir facilities

¿Qué hace la regla `*.*;auth,authpriv.none -/var/log/syslog`?

<details><summary>Predicción</summary>

- `*.*` → captura todos los mensajes de todas las facilities y prioridades
- `;auth,authpriv.none` → **excepto** los de auth y authpriv
- `-/var/log/syslog` → escribe en syslog con escritura **buffered** (el guion
  desactiva fsync para rendimiento)

Resultado: todos los mensajes del sistema van a syslog, excepto los de
autenticación (que van a su propio archivo `/var/log/auth.log`). Es la
regla estándar de Debian para el log principal.

</details>

### Ejercicio 5 — El guion (-)

¿Cuál es la diferencia entre `kern.* /var/log/kern.log` y
`kern.* -/var/log/kern.log`? ¿Cuándo usarías cada uno?

<details><summary>Predicción</summary>

- **Sin guion:** cada escritura hace `fsync()` — sincroniza a disco
  inmediatamente. Más seguro (si crashea, los logs están en disco) pero más
  lento (I/O por cada mensaje).
- **Con guion (-):** escritura buffered, sin `fsync()`. Más rápido pero si
  el sistema crashea, los últimos mensajes pueden perderse.

Usar **sin guion** para logs críticos (auth, emerg) donde perder mensajes
es inaceptable. Usar **con guion** para logs de alto volumen (mail, debug,
daemon) donde el rendimiento importa más.

</details>

### Ejercicio 6 — Drop-ins

¿Cómo harías para que los logs de tu aplicación (usando facility `local1`)
vayan a `/var/log/myapp.log` sin mezclarse con syslog?

<details><summary>Predicción</summary>

Crear `/etc/rsyslog.d/30-myapp.conf`:
```bash
local1.*    -/var/log/myapp.log
```

Y modificar la regla de syslog en `50-default.conf` para excluir local1:
```bash
*.*;auth,authpriv.none;local1.none    -/var/log/syslog
```

Luego validar y reiniciar:
```bash
sudo rsyslogd -N1
sudo systemctl restart rsyslog
```

El número 30 asegura que la regla de `myapp` se procese antes que las
reglas por defecto (50+).

</details>

### Ejercicio 7 — Diferencias Debian vs RHEL

¿Dónde buscarías los logs de autenticación en Debian? ¿Y en RHEL? ¿Qué
módulo de input usa cada uno?

<details><summary>Predicción</summary>

| Aspecto | Debian | RHEL |
|---|---|---|
| Auth logs | `/var/log/auth.log` | `/var/log/secure` |
| Input module | `imuxsock` (socket `/dev/log`) | `imjournal` (lee del journal) |
| Log principal | `/var/log/syslog` | `/var/log/messages` |

Debian recibe logs vía socket Unix tradicional. RHEL lee directamente del
journal de systemd con el módulo `imjournal`.

</details>

### Ejercicio 8 — logger para scripts

Escribe un fragmento de script que use `logger` para registrar el inicio,
éxito o fallo de una operación, usando facility `local2` y tag `deploy`.

<details><summary>Predicción</summary>

```bash
#!/bin/bash
logger -t deploy -p local2.info "Iniciando despliegue"

if /opt/deploy/run.sh; then
    logger -t deploy -p local2.info "Despliegue completado exitosamente"
else
    logger -t deploy -p local2.err "Despliegue FALLIDO con código $?"
fi
```

Los mensajes aparecerán con el tag `deploy` y se pueden enrutar a un
archivo dedicado creando una regla `local2.* /var/log/deploy.log` en un
drop-in.

</details>

### Ejercicio 9 — Validar configuración

¿Qué comando usas para validar la sintaxis de rsyslog antes de reiniciar?
¿Por qué es importante hacerlo?

<details><summary>Predicción</summary>

```bash
sudo rsyslogd -N1
```

Es importante porque un error de sintaxis en la configuración haría que
rsyslog **no arranque** tras el restart, dejando el sistema **sin logging**.
`-N1` valida sin iniciar el daemon — si reporta errores, se corrigen antes
de reiniciar. Esto es especialmente crítico en servidores de producción donde
perder logging puede violar requisitos de compliance.

</details>

### Ejercicio 10 — Interpretar regla compleja

¿Qué hace esta regla?

```
*.info;mail.none;authpriv.none;cron.none    /var/log/messages
```

<details><summary>Predicción</summary>

Captura mensajes de **todas las facilities** (`*`) con prioridad **info o
superior** (info + notice + warning + err + crit + alert + emerg), **excepto**:
- `mail.none` — excluye correo (va a su propio archivo)
- `authpriv.none` — excluye autenticación privada (va a `/var/log/secure`)
- `cron.none` — excluye cron (va a `/var/log/cron`)

Los escribe en `/var/log/messages` **con sync** (sin guion, más seguro).

Es la regla principal de RHEL: `/var/log/messages` recibe todo excepto mail,
auth y cron, que tienen archivos dedicados. En Debian el equivalente es
`*.*;auth,authpriv.none -/var/log/syslog`.

</details>
