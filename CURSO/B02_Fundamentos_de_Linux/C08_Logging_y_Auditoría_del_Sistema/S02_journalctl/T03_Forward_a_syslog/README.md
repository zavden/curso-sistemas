# T03 — Forward a syslog

## Contexto

En C06/S04/T03 cubrimos la coexistencia journald-rsyslog de forma general.
Aquí profundizamos en la configuración detallada del forwarding, los
problemas que pueden surgir y cómo diagnosticarlos.

## Mecanismos de forwarding

Existen dos caminos para que rsyslog obtenga los logs del journal:

```
Mecanismo 1: ForwardToSyslog (push)
────────────────────────────────────
journald ──── /dev/log ────→ rsyslog (imuxsock)
   │              (socket)       │
   │                             └─→ /var/log/syslog
   └─→ /var/log/journal/


Mecanismo 2: imjournal (pull)
─────────────────────────────
journald ──→ /var/log/journal/
                    │
              rsyslog (imjournal)
                    │
                    └─→ /var/log/messages
```

```bash
# Mecanismo 1 — ForwardToSyslog (Debian):
# journald envía una copia de cada mensaje al socket /dev/log
# rsyslog escucha en /dev/log con el módulo imuxsock
# Es un PUSH: journald empuja los mensajes a rsyslog

# Mecanismo 2 — imjournal (RHEL):
# rsyslog lee directamente los archivos del journal
# No depende de ForwardToSyslog
# Es un PULL: rsyslog extrae los mensajes del journal
```

## Configurar ForwardToSyslog

```ini
# /etc/systemd/journald.conf
[Journal]
ForwardToSyslog=yes
```

```bash
# Verificar el estado actual:
grep -i ForwardToSyslog /etc/systemd/journald.conf
# Si está comentado o ausente, el default depende de la distro:
# Debian/Ubuntu: yes (default)
# RHEL 7+: yes (pero se usa imjournal, así que no importa mucho)
# Fedora: yes

# Verificar empíricamente:
systemd-analyze cat-config systemd/journald.conf 2>/dev/null | grep -i forward
# Muestra la configuración efectiva (incluyendo drop-ins)

# Después de cambiar:
sudo systemctl restart systemd-journald
```

### Qué se envía con ForwardToSyslog

```bash
# ForwardToSyslog envía:
# - El texto del mensaje (MESSAGE)
# - La prioridad (PRIORITY)
# - La facility (SYSLOG_FACILITY)
# - El identifier (SYSLOG_IDENTIFIER)
# - El PID (SYSLOG_PID o _PID)
# - El timestamp

# ForwardToSyslog NO envía:
# - Campos estructurados del journal (_SYSTEMD_UNIT, _EXE, _CMDLINE, etc.)
# - Campos personalizados (LogExtraFields)
# - Los metadatos del journal se PIERDEN en el forwarding

# El mensaje llega a rsyslog como un mensaje syslog tradicional:
# <prioridad>timestamp hostname identifier[PID]: MESSAGE
```

### Otros forwarding

```bash
# journald puede reenviar a otros destinos además de syslog:

# [Journal]
# ForwardToKMsg=no       — reenviar al buffer del kernel (kmsg)
# ForwardToConsole=no    — reenviar a una consola TTY
# ForwardToWall=yes      — reenviar emergencias a todos los terminales

# Si ForwardToConsole=yes:
# TTYPath=/dev/console   — a qué TTY enviar
# MaxLevelConsole=info   — nivel máximo a reenviar

# Niveles máximos por destino:
# MaxLevelStore=debug    — qué guardar en el journal
# MaxLevelSyslog=debug   — qué reenviar a syslog
# MaxLevelKMsg=notice    — qué reenviar a kmsg
# MaxLevelConsole=info   — qué reenviar a consola
# MaxLevelWall=emerg     — qué reenviar a wall
```

## Configurar imjournal (RHEL)

```bash
# En RHEL, rsyslog lee directamente del journal con imjournal:
# /etc/rsyslog.conf

module(load="imjournal"
    StateFile="imjournal.state"      # rastrea la posición de lectura
    ratelimit.interval="600"          # rate limiting: ventana de 600s
    ratelimit.burst="20000"           # máx 20000 mensajes por ventana
    IgnorePreviousMessages="off"      # procesar mensajes anteriores al arranque
    DefaultSeverity="5"               # prioridad por defecto si no hay PRIORITY
    DefaultFacility="user"            # facility por defecto si no hay SYSLOG_FACILITY
)
```

```bash
# imjournal vs imuxsock:

# imjournal (RHEL):
# + Lee TODOS los campos estructurados del journal
# + No depende de ForwardToSyslog
# + Accede al journal completo (incluyendo mensajes anteriores)
# + Puede usar campos del journal en templates
# - Consume más recursos (lee archivos del journal)
# - Puede quedarse atrás si el journal crece rápido

# imuxsock (Debian):
# + Más ligero (recibe por socket)
# + Modelo tradicional de syslog
# - Solo recibe lo que journald reenvía
# - Pierde campos estructurados
# - Depende de ForwardToSyslog=yes
```

### Acceder a campos del journal desde imjournal

```bash
# Con imjournal, rsyslog puede acceder a los campos del journal
# usando la notación $!:

# Template que usa campos del journal:
template(name="JournalFormat" type="string"
    string="%timegenerated:::date-rfc3339% %$!_HOSTNAME% %$!_SYSTEMD_UNIT% [%$!_PID%]: %msg%\n"
)

# Filtrar por campos del journal:
if $!_SYSTEMD_UNIT == "nginx.service" then {
    action(type="omfile" file="/var/log/nginx-from-journal.log"
           template="JournalFormat")
    stop
}

# Esto es exclusivo de imjournal — imuxsock no tiene acceso a $!
```

## Problemas comunes y diagnóstico

### Mensajes duplicados

```bash
# Problema: el mismo mensaje aparece dos veces en /var/log/syslog

# Causa: si AMBOS imuxsock e imjournal están cargados,
# Y ForwardToSyslog=yes, rsyslog recibe el mensaje por DOS caminos:
# 1. journald → /dev/log → imuxsock → rsyslog
# 2. rsyslog → imjournal → lee del journal

# Solución 1 — Solo un mecanismo:
# Si usas imjournal: ForwardToSyslog=no
# Si usas imuxsock: no cargar imjournal

# Solución 2 — Verificar qué módulos están cargados:
grep -rE "imuxsock|imjournal" /etc/rsyslog.conf /etc/rsyslog.d/ 2>/dev/null
# Si ambos están cargados, desactivar uno
```

### Mensajes perdidos

```bash
# Problema: algunos mensajes del journal no aparecen en /var/log/syslog

# Causa 1 — Rate limiting en journald:
journalctl -u systemd-journald --grep="Suppressed" --no-pager
# "Suppressed N messages from unit.service"
# journald suprimió mensajes → no llegaron a rsyslog

# Solución: ajustar rate limiting en journald.conf:
# [Journal]
# RateLimitIntervalSec=30s
# RateLimitBurst=10000

# Causa 2 — Rate limiting en rsyslog:
grep -r "ratelimit" /etc/rsyslog.conf /etc/rsyslog.d/ 2>/dev/null
# imjournal tiene su propio rate limiting

# Causa 3 — MaxLevelSyslog filtra prioridades:
grep -i MaxLevelSyslog /etc/systemd/journald.conf
# Si MaxLevelSyslog=warning, los mensajes info y debug no se reenvían

# Causa 4 — imjournal se quedó atrás:
# Si el journal crece más rápido de lo que imjournal lee,
# puede saltarse mensajes
```

### El journal tiene mensajes que syslog no

```bash
# journald captura TODO:
journalctl -u myapp -n 10 --no-pager
# 10 mensajes

# Pero syslog puede no tener todos:
grep myapp /var/log/syslog | tail -10
# Menos mensajes

# Diferencias posibles:
# 1. ForwardToSyslog=no → syslog no recibe nada de journald
# 2. MaxLevelSyslog limita las prioridades reenviadas
# 3. Rate limiting en journald o rsyslog
# 4. rsyslog no tiene una regla que capture esos mensajes
# 5. El servicio escribe a stdout (capturado por journald)
#    pero no usa syslog() (no llega a /dev/log)
```

### Verificar el flujo completo

```bash
# Paso 1 — Enviar un mensaje de prueba:
logger -t forward-test -p local0.info "Test forwarding $(date +%s)"

# Paso 2 — Verificar que journald lo tiene:
journalctl SYSLOG_IDENTIFIER=forward-test -n 1 --no-pager

# Paso 3 — Verificar que rsyslog lo recibió:
grep "forward-test" /var/log/syslog 2>/dev/null || \
grep "forward-test" /var/log/messages 2>/dev/null || \
echo "NO encontrado en archivos de rsyslog"

# Paso 4 — Si no llegó a rsyslog, diagnosticar:
echo "ForwardToSyslog:"
grep -i ForwardToSyslog /etc/systemd/journald.conf
echo "MaxLevelSyslog:"
grep -i MaxLevelSyslog /etc/systemd/journald.conf
echo "Módulos de rsyslog:"
grep -rE "imuxsock|imjournal" /etc/rsyslog.conf /etc/rsyslog.d/ 2>/dev/null
```

## Escenarios de configuración

### Solo journald (servidores modernos, contenedores)

```ini
# /etc/systemd/journald.conf
[Journal]
Storage=persistent
ForwardToSyslog=no
SystemMaxUse=1G
Compress=yes
```

```bash
# Deshabilitar rsyslog:
sudo systemctl stop rsyslog
sudo systemctl disable rsyslog

# Ventajas: menos procesos, menos I/O, consultas con journalctl
# Desventajas: sin reenvío remoto nativo robusto,
#              herramientas legacy no pueden leer el journal
```

### Solo rsyslog (compatibilidad máxima)

```ini
# /etc/systemd/journald.conf
[Journal]
Storage=volatile
ForwardToSyslog=yes
RuntimeMaxUse=50M
```

```bash
# journald solo mantiene un buffer en RAM
# Todo se persiste a través de rsyslog
# Ventaja: control total con rsyslog, logrotate, herramientas legacy
# Desventaja: pierdes búsqueda estructurada del journal
```

### Ambos (producción típica)

```ini
# /etc/systemd/journald.conf
[Journal]
Storage=persistent
ForwardToSyslog=yes
SystemMaxUse=1G
Compress=yes
MaxLevelSyslog=info
```

```bash
# journald guarda todo con búsqueda indexada
# rsyslog recibe copias para:
# - Archivos de texto plano (herramientas legacy)
# - Reenvío remoto a servidor central
# - Integración con herramientas de monitoreo

# Es la configuración más común en producción
```

## Migrar de rsyslog a journal-only

```bash
# 1. Verificar que el journal es persistente:
ls -d /var/log/journal/ && echo "OK: persistente"

# 2. Verificar que los scripts/herramientas no dependen de /var/log/syslog:
grep -r "/var/log/syslog\|/var/log/messages" /etc/ /opt/ /usr/local/ 2>/dev/null

# 3. Configurar journald:
sudo tee /etc/systemd/journald.conf.d/persistent.conf << 'EOF'
[Journal]
Storage=persistent
SystemMaxUse=2G
Compress=yes
ForwardToSyslog=no
EOF

# 4. Reiniciar journald:
sudo systemctl restart systemd-journald

# 5. Deshabilitar rsyslog:
sudo systemctl disable --now rsyslog

# 6. Verificar que journalctl funciona:
journalctl --since "5 min ago" --no-pager

# 7. Para reenvío remoto sin rsyslog:
# Usar systemd-journal-upload/systemd-journal-remote
```

---

## Ejercicios

### Ejercicio 1 — Identificar el mecanismo

```bash
# ¿Tu sistema usa ForwardToSyslog o imjournal?
echo "=== ForwardToSyslog ==="
grep -i ForwardToSyslog /etc/systemd/journald.conf 2>/dev/null || echo "Default (probablemente yes)"

echo "=== Módulos de rsyslog ==="
grep -rE "imuxsock|imjournal" /etc/rsyslog.conf /etc/rsyslog.d/ 2>/dev/null | grep -v '^#'
```

### Ejercicio 2 — Verificar el flujo

```bash
# Enviar un mensaje y rastrearlo:
MSG="forward-test-$(date +%s)"
logger -t fwd-test "$MSG"

echo "=== En journal ==="
journalctl SYSLOG_IDENTIFIER=fwd-test -n 1 --no-pager 2>/dev/null

echo "=== En syslog ==="
grep "$MSG" /var/log/syslog /var/log/messages 2>/dev/null || echo "No encontrado"
```

### Ejercicio 3 — Configuración actual

```bash
# ¿Cuál es la configuración efectiva de forwarding?
systemd-analyze cat-config systemd/journald.conf 2>/dev/null | \
    grep -iE "Forward|MaxLevel" || \
    grep -iE "Forward|MaxLevel" /etc/systemd/journald.conf 2>/dev/null
```
