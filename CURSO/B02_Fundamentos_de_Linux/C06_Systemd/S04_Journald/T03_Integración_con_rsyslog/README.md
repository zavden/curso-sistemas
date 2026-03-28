# T03 — Integración con rsyslog

## Dos sistemas de logging

En la mayoría de distribuciones Linux, **journald y rsyslog coexisten**:

```
Aplicaciones / Servicios
        │
        ▼
   systemd-journald        ← captura TODO (stdout, stderr, syslog(), kernel)
        │
        ├─── /var/log/journal/    (binario, indexado)
        │
        └─── ForwardToSyslog=yes
                │
                ▼
           rsyslog              ← recibe los logs reenviados
                │
                └─── /var/log/syslog, /var/log/auth.log, etc.  (texto plano)
```

```bash
# Verificar que ambos están activos:
systemctl is-active systemd-journald
# active

systemctl is-active rsyslog
# active (en Debian/Ubuntu)
# active (en RHEL si está instalado)

# journald siempre está presente (es parte de systemd)
# rsyslog puede no estar instalado (especialmente en contenedores)
```

## Por qué coexisten

### Journal — Ventajas

- **Estructurado**: cada log es un conjunto de campos clave-valor
- **Indexado**: búsquedas rápidas por unidad, PID, prioridad, tiempo
- **Centralizado**: un solo lugar para kernel, servicios, syslog
- **Integridad**: Seal verifica que los logs no se han manipulado
- **Rotación automática**: gestión de espacio sin logrotate

### rsyslog — Ventajas

- **Texto plano**: legible directamente con cat, grep, tail
- **Reenvío remoto**: TCP/UDP/RELP a un servidor central
- **Filtros complejos**: RainerScript, property-based filters
- **Compatibilidad**: herramientas legacy esperan /var/log/syslog
- **Archivado**: formatos flexibles, integración con almacenamiento externo

### La práctica habitual

```bash
# journald captura todo y lo almacena en formato binario
# rsyslog recibe los logs reenviados y los escribe en texto plano

# El admin usa:
# - journalctl para consultas rápidas, filtros por servicio, debug
# - /var/log/syslog para scripts legacy, herramientas de monitoreo
# - rsyslog para reenviar a un servidor central de logs
```

## Configuración del forwarding

### journald → rsyslog

```ini
# /etc/systemd/journald.conf
[Journal]
ForwardToSyslog=yes      # default en la mayoría de distros
```

```bash
# journald envía los logs a /dev/log (socket de syslog)
# rsyslog escucha en ese socket y los procesa

# Verificar que el forwarding está activo:
grep ForwardToSyslog /etc/systemd/journald.conf
# Si está comentado, el default es "yes" en la mayoría de distros
```

### rsyslog lee del journal (imjournal)

En RHEL/CentOS 7+, rsyslog tiene un módulo para leer directamente del
journal en lugar de depender del forwarding:

```bash
# /etc/rsyslog.conf (RHEL)
module(load="imjournal"
    StateFile="imjournal.state"
    ratelimit.interval="600"
    ratelimit.burst="20000")

# imjournal lee directamente de /var/log/journal/
# No depende de ForwardToSyslog
# Ventaja: accede a todos los campos estructurados del journal
```

```bash
# Debian/Ubuntu usa el socket tradicional:
# /etc/rsyslog.conf
module(load="imuxsock")    # escucha en /dev/log
module(load="imklog")      # logs del kernel
# Depende de ForwardToSyslog=yes en journald.conf
```

## Archivos de log que mantiene rsyslog

```bash
# Debian/Ubuntu:
/var/log/syslog           # todos los logs (excepto auth)
/var/log/auth.log         # autenticación (login, sudo, ssh)
/var/log/kern.log         # mensajes del kernel
/var/log/mail.log         # correo
/var/log/daemon.log       # daemons
/var/log/user.log         # aplicaciones de usuario

# RHEL/Fedora:
/var/log/messages         # todos los logs (equivale a syslog)
/var/log/secure           # autenticación (equivale a auth.log)
/var/log/maillog          # correo
/var/log/cron             # cron
/var/log/boot.log         # mensajes de boot
```

```bash
# Ver la configuración de rsyslog que define estos archivos:
cat /etc/rsyslog.d/50-default.conf     # Debian/Ubuntu
cat /etc/rsyslog.conf                   # RHEL (todo en un archivo)

# Ejemplo de reglas (Debian):
# auth,authpriv.*          /var/log/auth.log
# *.*;auth,authpriv.none   /var/log/syslog
# kern.*                   /var/log/kern.log
```

## journalctl vs archivos de texto

```bash
# Lo que puedes hacer con journalctl pero NO con grep en texto plano:

# 1. Filtrar por servicio (sin saber el formato del log):
journalctl -u nginx
# vs: grep nginx /var/log/syslog (puede coincidir con otras cosas)

# 2. Filtrar por PID:
journalctl _PID=1234
# vs: grep "1234" /var/log/syslog (coincide con timestamps, IPs, etc.)

# 3. Filtrar por boot:
journalctl -b -1
# vs: grep por timestamp en syslog (complicado, propenso a errores)

# 4. Logs del kernel con contexto del servicio:
journalctl -k -u docker
# vs: dmesg | grep docker (sin timestamps precisos)

# 5. Formato JSON para procesamiento:
journalctl -o json -u nginx | jq '.MESSAGE'
# vs: parsear texto plano con awk/sed (frágil)
```

```bash
# Lo que puedes hacer con archivos de texto pero es más difícil con journalctl:

# 1. Leer logs de un sistema que no arranca (boot desde USB):
cat /mnt/broken-system/var/log/syslog
# vs: journalctl --directory=/mnt/broken-system/var/log/journal/

# 2. Enviar logs a herramientas externas (Logstash, Fluentd, etc.):
# Muchas herramientas esperan archivos de texto

# 3. Rotación personalizada con logrotate (configuración granular)

# 4. Scripts legacy que hacen grep/awk sobre /var/log/syslog
```

## Escenarios de configuración

### Solo journald (sin rsyslog)

```bash
# En contenedores y sistemas minimalistas:
sudo systemctl stop rsyslog
sudo systemctl disable rsyslog

# Asegurar persistencia del journal:
# /etc/systemd/journald.conf
[Journal]
Storage=persistent
SystemMaxUse=500M

# Ventaja: menos procesos, menos I/O de disco
# Desventaja: sin reenvío remoto nativo (hay systemd-journal-upload)
```

### Solo rsyslog (journal mínimo)

```bash
# Si necesitas rsyslog con su ecosistema completo:
# /etc/systemd/journald.conf
[Journal]
Storage=volatile
ForwardToSyslog=yes
RuntimeMaxUse=50M

# journald solo mantiene logs en RAM como buffer
# Todo se persiste a través de rsyslog
# Ventaja: control total con rsyslog, logrotate
# Desventaja: pierdes las capacidades de búsqueda del journal
```

### Ambos (configuración típica de producción)

```bash
# El escenario más común:
# /etc/systemd/journald.conf
[Journal]
Storage=persistent
ForwardToSyslog=yes
SystemMaxUse=1G
Compress=yes

# journald persiste TODO con indexación y búsqueda rápida
# rsyslog recibe copias y:
#   - Escribe en texto plano para herramientas legacy
#   - Reenvía a un servidor central de logs
#   - Alimenta herramientas de monitoreo

# logrotate se encarga de rotar los archivos de texto de rsyslog
```

## Reenvío remoto

### Con rsyslog

```bash
# Servidor receptor (/etc/rsyslog.conf):
module(load="imtcp")
input(type="imtcp" port="514")

# Cliente que envía (/etc/rsyslog.d/remote.conf):
*.* @@logserver.example.com:514
# @  = UDP
# @@ = TCP
```

### Con systemd-journal-upload (alternativa sin rsyslog)

```bash
# systemd tiene su propio mecanismo de reenvío:
# systemd-journal-upload → systemd-journal-remote

# En el servidor receptor:
sudo systemctl enable systemd-journal-remote.socket

# En el cliente:
# /etc/systemd/journal-upload.conf
[Upload]
URL=http://logserver.example.com:19532

sudo systemctl enable systemd-journal-upload
```

## Debian vs RHEL

| Aspecto | Debian/Ubuntu | RHEL/Fedora |
|---|---|---|
| rsyslog instalado | Sí (por defecto) | Sí (por defecto) |
| Módulo de rsyslog | imuxsock (socket) | imjournal (directo) |
| Logs principales | /var/log/syslog | /var/log/messages |
| Logs de auth | /var/log/auth.log | /var/log/secure |
| Journal persistente | Sí (auto, directorio existe) | Sí (persistent por defecto) |
| ForwardToSyslog | yes (default) | yes (pero usa imjournal) |

---

## Ejercicios

### Ejercicio 1 — Verificar la coexistencia

```bash
# ¿Están ambos activos?
systemctl is-active systemd-journald rsyslog 2>/dev/null

# ¿Existe forwarding?
grep -i "ForwardToSyslog" /etc/systemd/journald.conf 2>/dev/null || \
    echo "ForwardToSyslog no definido (default: yes)"
```

### Ejercicio 2 — Comparar outputs

```bash
# Comparar la misma información en journal vs texto:
echo "--- journalctl ---"
journalctl -u sshd -n 3 --no-pager 2>/dev/null

echo "--- archivo de texto ---"
grep sshd /var/log/auth.log 2>/dev/null | tail -3 || \
grep sshd /var/log/secure 2>/dev/null | tail -3
```

### Ejercicio 3 — Archivos de log

```bash
# ¿Qué archivos de log mantiene rsyslog?
ls -lh /var/log/*.log /var/log/messages /var/log/secure 2>/dev/null

# ¿Cuánto espacio usan en total?
du -sh /var/log/ 2>/dev/null
```
