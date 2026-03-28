# T03 — Forward a syslog

> **Objetivo:** Entender los mecanismos de forwarding entre journald y rsyslog, configurar cada enfoque, y diagnosticar problemas comunes.

## Contexto

En C06/S04/T03 cubrimos la coexistencia general. Aquí profundizamos en la configuración detallada y la resolución de problemas.

---

## Dos mecanismos de comunicación journald ↔ rsyslog

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                 MECANISMO 1: ForwardToSyslog (PUSH)                        │
│                          Debian/Ubuntu por defecto                          │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                              │
│   Aplicación                                                               │
│       │                                                                    │
│       ▼                                                                    │
│   journald                                                                 │
│       │                                                                    │
│       ├──► /var/log/journal/  (binario, indexado)                          │
│       │                                                                    │
│       └──► /dev/log ──────────► rsyslog (imuxsock)                       │
│              (socket)              │                                       │
│                                     ▼                                       │
│                              /var/log/syslog                               │
│                              /var/log/auth.log                             │
│                                                                              │
│   journald EMPUJA los mensajes a rsyslog via socket                       │
│   Depende de: ForwardToSyslog=yes en journald.conf                         │
└─────────────────────────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────────────────────────┐
│                 MECANISMO 2: imjournal (PULL)                              │
│                          RHEL/CentOS por defecto                           │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                              │
│   Aplicación                                                               │
│       │                                                                    │
│       ▼                                                                    │
│   journald ──► /var/log/journal/  (binario, indexado)                      │
│                                    │                                       │
│                          rsyslog (imjournal) ◄── lee directamente          │
│                                    │                                       │
│                                    ▼                                       │
│                              /var/log/messages                             │
│                              /var/log/secure                               │
│                                                                              │
│   rsyslog EXTRAE los mensajes del journal                                  │
│   NO depende de ForwardToSyslog                                            │
│   Lee todos los campos estructurados                                       │
└─────────────────────────────────────────────────────────────────────────────┘
```

---

## Configurar ForwardToSyslog

```bash
# /etc/systemd/journald.conf
[Journal]
ForwardToSyslog=yes      # default en la mayoría de distros
```

```bash
# Verificar estado:
grep -i ForwardToSyslog /etc/systemd/journald.conf
# Si está comentado, el default depende de la distro:
# Debian/Ubuntu: yes (default)
# RHEL 7+: yes (pero usa imjournal)
# Fedora: yes

# Ver configuración efectiva (incluye drop-ins):
systemd-analyze cat-config systemd/journald.conf | grep -i forward

# Recargar después de cambiar:
sudo systemctl restart systemd-journald
```

### Qué se envía con ForwardToSyslog

```
┌─────────────────────────────────────────────────────────────────────────────┐
│  LO QUE SÍ se envía:                                                       │
│    MESSAGE        — texto del log                                          │
│    PRIORITY       — nivel (0-7)                                           │
│    SYSLOG_FACILITY — facility (numérico)                                   │
│    SYSLOG_IDENTIFIER — identificador/tag                                   │
│    SYSLOG_PID     — PID                                                   │
│    timestamp      — hora del mensaje                                        │
│                                                                              │
│  LO QUE NO se envía:                                                       │
│    ✗ _SYSTEMD_UNIT — unidad de systemd                                    │
│    ✗ _EXE, _CMDLINE — información del ejecutable                          │
│    ✗ Campos personalizados (LogExtraFields)                                │
│    ✗ _BOOT_ID, _MACHINE_ID — metadatos del journal                        │
│                                                                              │
│  El mensaje llega a rsyslog como syslog tradicional:                       │
│  <pri>timestamp hostname identifier[PID]: MESSAGE                         │
└─────────────────────────────────────────────────────────────────────────────┘
```

### Otros destinos de forwarding

```bash
# /etc/systemd/journald.conf
[Journal]
ForwardToSyslog=yes      # a rsyslog (via /dev/log)
ForwardToKMsg=no         # al buffer del kernel (dmesg)
ForwardToConsole=no      # a una consola TTY
ForwardToWall=yes        # emergencias a todos los terminales (wall)
```

### Control de niveles por destino

```bash
# Limitar qué prioridades se envían a cada destino:
# MaxLevelStore=debug     — qué se guarda en el journal local
# MaxLevelSyslog=info     — qué se reenvía a syslog
# MaxLevelKMsg=notice     — qué se reenvía a kmsg (dmesg)
# MaxLevelConsole=info    — qué se reenvía a consola
# MaxLevelWall=emerg      — qué se envía a wall

# Ejemplo: syslog solo recibe warning y superior:
# MaxLevelSyslog=warning
# (info, notice, debug se guardan en journal pero NO se reenvían a syslog)
```

---

## Configurar imjournal (RHEL)

```bash
# /etc/rsyslog.conf — módulo imjournal:
module(load="imjournal"
    StateFile="imjournal.state"         # posición de lectura (persiste entre reinicios)
    ratelimit.interval="600"            # ventana de rate limiting (segundos)
    ratelimit.burst="20000"             # máximo mensajes por ventana
    IgnorePreviousMessages="off"         # no procesar mensajes previos al arranque
    DefaultSeverity="5"                 # prioridad por defecto si no hay PRIORITY (notice)
    DefaultFacility="user"               # facility por defecto si no hay SYSLOG_FACILITY
)
```

### imjournal vs imuxsock

| Aspecto | imjournal (RHEL) | imuxsock (Debian) |
|---------|-----------------|-------------------|
| Mecanismo | **Pull** — lee del journal | **Push** — recibe por socket |
| Depende de ForwardToSyslog | No | Sí |
| Acceso a campos estructurados | **Sí** (`$!campo`) | No |
| Campos journal disponibles | Todos (`_SYSTEMD_UNIT`, `_EXE`...) | Solo syslog tradicional |
| Recursos | Más alto (lectura de archivos) | Más bajo (socket) |
| Rate limiting | Configurable en el módulo | Sistema tradicional de syslog |
| Estado de lectura | Persistente (`StateFile`) | No stateful |

### Acceder a campos del journal desde rsyslog

```bash
# Con imjournal, rsyslog puede usar $! para acceder a campos:
# $!_SYSTEMD_UNIT   — unidad de systemd
# $!_HOSTNAME        — hostname
# $!_PID             — PID

# Template que usa campos del journal:
template(name="JournalFormat" type="string"
    string="%timegenerated:::date-rfc3339% %$!_HOSTNAME% %$!_SYSTEMD_UNIT% [%$!_PID%]: %msg%\n"
)

# Filtrar por campo del journal:
if $!_SYSTEMD_UNIT == "nginx.service" then {
    action(type="omfile" file="/var/log/nginx-from-journal.log"
           template="JournalFormat")
    stop
}

# Esto es EXCLUSIVO de imjournal — imuxsock NO tiene acceso a $!
```

---

## Problemas comunes y diagnóstico

### Problema: Mensajes duplicados

```
SÍNTOMA: el mismo mensaje aparece DOS veces en /var/log/syslog

CAUSA: ambos mecanismos están activos simultáneamente:
  1. journald → ForwardToSyslog=yes → /dev/log → imuxsock → rsyslog
  2. rsyslog → imjournal → lee /var/log/journal/

  El mensaje llega por DOS caminos distintos

SOLUCIÓN:
  Opción A: Usar solo imjournal → ForwardToSyslog=no
  Opción B: Usar solo imuxsock → NO cargar imjournal

# Verificar qué módulos están activos:
grep -rE "imuxsock|imjournal" /etc/rsyslog.conf /etc/rsyslog.d/*.conf

# Si ambos están, desactivar uno
```

### Problema: Mensajes perdidos

```
SÍNTOMA: el journal tiene mensajes que NO aparecen en syslog

CAUSAS POSIBLES:

  1. Rate limiting en journald
     → journalctl -u systemd-journald --grep="Suppressed" --no-pager
     → "Suppressed N messages from unit.service"

  2. MaxLevelSyslog filtra prioridades
     → grep MaxLevelSyslog /etc/systemd/journald.conf
     → Si es "warning", info y debug NO se reenvían

  3. Rate limiting en imjournal
     → Configurable con ratelimit.interval y ratelimit.burst

  4. imjournal se quedó atrás
     → Si el journal crece más rápido de lo que imjournal lee
     → Solución: ajustar performance o usar ForwardToSyslog

  5. El mensaje vino de stdout (journal) y no de syslog
     → journald captura stdout/stderr
     → Si la app no usa syslog(), el mensaje puede no llegar a rsyslog
```

### Problema: journal tiene más que syslog

```bash
# El journal captura TODO, syslog puede tener menos:

# 1. ForwardToSyslog=no
#    journald NO envía nada a syslog

# 2. MaxLevelSyslog por debajo de la prioridad del mensaje
#    journalctl -F PRIORITY --since today | sort | uniq -c

# 3. Rate limiting
#    journalctl -u systemd-journald --grep=Suppressed --no-pager

# 4. La app escribe a stdout (journald lo captura)
#    pero NO usa syslog() (no llega a rsyslog)
#    Verificar: journalctl _TRANSPORT=stdout -u myapp
```

### Verificar el flujo completo

```bash
# PASO 1: Enviar mensaje de prueba
MSG="fwd-test-$(date +%s)"
logger -t fwd-test -p local0.info "$MSG"

# PASO 2: Verificar que journald lo tiene
echo "=== journalctl ==="
journalctl SYSLOG_IDENTIFIER=fwd-test -n 1 --no-pager

# PASO 3: Verificar que syslog lo recibió
echo "=== archivos de syslog ==="
grep "$MSG" /var/log/syslog /var/log/messages 2>/dev/null || echo "NO encontrado en syslog"

# PASO 4: Si NO llegó a syslog, diagnosticar:
echo "=== ForwardToSyslog ==="
grep -i ForwardToSyslog /etc/systemd/journald.conf || echo "(default: yes)"

echo "=== MaxLevelSyslog ==="
grep -i MaxLevelSyslog /etc/systemd/journald.conf || echo "(default: debug)"

echo "=== módulos de rsyslog ==="
grep -rE "imuxsock|imjournal" /etc/rsyslog.conf /etc/rsyslog.d/*.conf

echo "=== Rate limiting de journald ==="
grep -i "RateLimit" /etc/systemd/journald.conf
```

---

## Escenarios de configuración

### Escenario 1: Solo journald (contenedores, embedded)

```bash
# /etc/systemd/journald.conf
[Journal]
Storage=persistent
ForwardToSyslog=no          # NO reenviar a syslog
SystemMaxUse=1G
Compress=yes

# Deshabilitar rsyslog:
sudo systemctl stop rsyslog
sudo systemctl disable rsyslog

# Ventajas:
#   + Menos procesos
#   + Menos I/O de disco
#   + journalctl para todo
#   + Búsqueda estructurada

# Desventajas:
#   - Sin reenvío remoto robusto nativo
#   - Herramientas legacy que esperan /var/log/syslog no funcionan
#   - Solución: systemd-journal-upload para reenvío remoto
```

### Escenario 2: Solo rsyslog (legacy)

```bash
# /etc/systemd/journald.conf
[Journal]
Storage=volatile            # solo RAM, no persiste
ForwardToSyslog=yes
RuntimeMaxUse=50M

# journald actúa como buffer temporal
# Todo se persiste a través de rsyslog

# Ventajas:
#   + Control total con rsyslog
#   + logrotate para políticas complejas
#   + Herramientas legacy funcionan

# Desventajas:
#   - Pierdes búsqueda estructurada del journal
#   - No hay logs de boots anteriores
```

### Escenario 3: Ambos (producción típica)

```bash
# /etc/systemd/journald.conf
[Journal]
Storage=persistent
ForwardToSyslog=yes
SystemMaxUse=1G
SystemMaxFileSize=100M
Compress=yes
MaxLevelSyslog=info        # reenviar info y superior a syslog

# journald:
#   + Guarda todo con indexación
#   + Búsqueda rápida con journalctl
#   + Logs de boots anteriores

# rsyslog:
#   + Archivos de texto plano
#   + Reenvío remoto a servidor central
#   + Integración con ELK/Splunk
```

---

## Migrar de rsyslog a journal-only

```bash
# 1. Verificar que el journal es persistente:
ls -d /var/log/journal/ && echo "OK: persistente"

# 2. Buscar dependencias de /var/log/syslog:
grep -r "/var/log/syslog\|/var/log/messages" \
    /etc/ /opt/ /usr/local/ 2>/dev/null | grep -v ".conf:" | head -10

# 3. Configurar journald para persistencia:
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

# 6. Verificar:
journalctl --since "5 min ago" --no-pager

# 7. Para reenvío remoto sin rsyslog:
#    systemd-journal-upload → systemd-journal-remote
```

---

## Quick reference

```
MECANISMOS:
  ForwardToSyslog  → journald empuja a /dev/log → imuxsock
  imjournal        → rsyslog extrae de /var/log/journal/

DIAGNÓSTICO:
  journalctl SYSLOG_IDENTIFIER=x -n 1       → verificar que journal lo tiene
  grep x /var/log/syslog /var/log/messages  → verificar que syslog lo tiene
  grep -rE "imuxsock|imjournal" /etc/rsyslog.d/

PROBLEMAS COMUNES:
  Duplicados    → ambos mecanismos activos (desactivar uno)
  Perdidos      → rate limiting o MaxLevelSyslog
  Diferencias   → stdout (journal) vs syslog() (syslog)

IMJOURNAL:
  $!campo       → acceder a campos del journal desde rsyslog
  StateFile     → persiste posición de lectura entre reinicios
```

---

## Ejercicios

### Ejercicio 1 — Identificar el mecanismo de tu sistema

```bash
# 1. ¿ForwardToSyslog?
echo "=== ForwardToSyslog ==="
grep -i ForwardToSyslog /etc/systemd/journald.conf 2>/dev/null || \
    echo "(default: yes — verificar con systemd-analyze)"

# 2. ¿Qué módulo de input usa rsyslog?
echo "=== Módulos de rsyslog ==="
grep -rE "^module\(|imuxsock|imjournal" /etc/rsyslog.conf /etc/rsyslog.d/*.conf 2>/dev/null

# 3. ¿Qué archivo de syslog principal existe?
ls -la /var/log/syslog /var/log/messages 2>/dev/null
```

### Ejercicio 2 — Verificar el flujo completo

```bash
# Enviar mensaje de prueba y rastrear su camino:
MSG="forward-verify-$(date +%s)"
logger -t fwd-verify -p local0.info "$MSG"

echo "=== 1. ¿Está en journal? ==="
journalctl SYSLOG_IDENTIFIER=fwd-verify -n 1 --no-pager 2>/dev/null

echo "=== 2. ¿Está en syslog? ==="
grep "$MSG" /var/log/syslog /var/log/messages 2>/dev/null || echo "NO encontrado"

echo "=== 3. ¿Con qué priority llegó? ==="
journalctl SYSLOG_IDENTIFIER=fwd-verify -o json-pretty -n 1 --no-pager 2>/dev/null | grep PRIORITY
```

### Ejercicio 3 — Diagnosticar diferencias journal vs syslog

```bash
# Objetivo: entender por qué journal y syslog pueden diferir

# 1. Ver todos los PRIORITYs en el journal:
echo "=== Priorities en journal ==="
journalctl -F PRIORITY --since today | sort | uniq -c | sort -rn | head -5

# 2. Ver MaxLevelSyslog:
echo "=== MaxLevelSyslog ==="
grep MaxLevelSyslog /etc/systemd/journald.conf 2>/dev/null || echo "(default: debug)"

# 3. Ver si hay rate limiting suprimido:
echo "=== Rate limiting en journald ==="
journalctl -u systemd-journald --grep="Suppressed" --since today --no-pager 2>/dev/null | tail -5
```

### Ejercicio 4 — Simular configuración duplicada

> ⚠️ **Solo en entorno de pruebas**

```bash
# 1. Verificar que NO hay duplicados actualmente:
#    (ambos mecanismos NO deberían estar activos al mismo tiempo)

# 2. Forzar duplicación (temporalmente):
#    Editar /etc/rsyslog.conf y añadir imjournal aunque ya tengas imuxsock
#    O dejar ForwardToSyslog=yes Y tener imjournal cargado

# 3. Enviar mensajes y observar duplicación:
logger -t dup-test "mensaje de prueba"
grep "dup-test" /var/log/syslog /var/log/messages 2>/dev/null | wc -l
# Si es >1, hay duplicación

# 4. Corregir: dejar solo un mecanismo
```

### Ejercicio 5 — Migración gradual

```bash
# Simular una migración de rsyslog a journal-only:

# 1. Ver estado actual:
systemctl is-active rsyslog
ls -d /var/log/journal/ 2>/dev/null && echo "Persistente" || echo "Volátil"

# 2. Configurar journald para persistencia (sin tocar rsyslog):
sudo tee /etc/systemd/journald.conf.d/migrate.conf << 'EOF'
[Journal]
Storage=persistent
ForwardToSyslog=yes   # mantener rsyslog funcionando
SystemMaxUse=500M
EOF

# 3. Después de migrar scripts a journalctl:
#    Cambiar ForwardToSyslog=no

# 4. Verificar que todo funciona con journalctl:
journalctl --since "5 min ago" -n 10 --no-pager
```
