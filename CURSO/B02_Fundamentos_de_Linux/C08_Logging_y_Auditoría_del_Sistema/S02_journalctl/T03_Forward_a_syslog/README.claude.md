# T03 — Forward a syslog

> **Objetivo:** Entender los mecanismos de forwarding entre journald y rsyslog, configurar cada enfoque, diagnosticar problemas comunes y elegir el escenario adecuado.

## Erratas detectadas en el material fuente

| Archivo | Línea | Error | Corrección |
|---------|-------|-------|------------|
| README.max.md | 142 | `IgnorePreviousMessages="off"` con comentario "no procesar mensajes previos al arranque" — el valor `off` desactiva la opción de ignorar, por lo que los mensajes previos **SÍ** se procesan; el comentario dice lo contrario | El comentario correcto es "sí procesar mensajes previos al arranque" (como aparece en README.md:119) |

---

## Contexto

En C06/S04/T03 cubrimos la coexistencia general journald-rsyslog. Aquí profundizamos en la configuración detallada del forwarding, los problemas que pueden surgir y cómo diagnosticarlos.

---

## Dos mecanismos de comunicación journald ↔ rsyslog

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                 MECANISMO 1: ForwardToSyslog (PUSH)                        │
│                          Debian/Ubuntu por defecto                          │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                             │
│   Aplicación                                                                │
│       │                                                                     │
│       ▼                                                                     │
│   journald                                                                  │
│       │                                                                     │
│       ├──► /var/log/journal/  (binario, indexado)                           │
│       │                                                                     │
│       └──► /dev/log ──────────► rsyslog (imuxsock)                          │
│              (socket)              │                                        │
│                                    ▼                                        │
│                              /var/log/syslog                                │
│                              /var/log/auth.log                              │
│                                                                             │
│   journald EMPUJA los mensajes a rsyslog via socket                        │
│   Depende de: ForwardToSyslog=yes en journald.conf                          │
└─────────────────────────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────────────────────────┐
│                 MECANISMO 2: imjournal (PULL)                               │
│                          RHEL/CentOS por defecto                            │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                             │
│   Aplicación                                                                │
│       │                                                                     │
│       ▼                                                                     │
│   journald ──► /var/log/journal/  (binario, indexado)                       │
│                                    │                                        │
│                          rsyslog (imjournal) ◄── lee directamente           │
│                                    │                                        │
│                                    ▼                                        │
│                              /var/log/messages                              │
│                              /var/log/secure                                │
│                                                                             │
│   rsyslog EXTRAE los mensajes del journal                                   │
│   NO depende de ForwardToSyslog                                             │
│   Lee todos los campos estructurados                                        │
└─────────────────────────────────────────────────────────────────────────────┘
```

**Resumen:**

- **Push (ForwardToSyslog):** journald envía una copia de cada mensaje al socket `/dev/log`; rsyslog la recibe con el módulo `imuxsock`. Usado en Debian/Ubuntu.
- **Pull (imjournal):** rsyslog lee directamente los archivos binarios del journal en `/var/log/journal/`. Usado en RHEL/CentOS/Fedora.

---

## Configurar ForwardToSyslog

```ini
# /etc/systemd/journald.conf
[Journal]
ForwardToSyslog=yes      # default en Debian/Ubuntu
```

```bash
# Verificar estado actual:
grep -i ForwardToSyslog /etc/systemd/journald.conf

# Si está comentado, el default depende de la distro:
# Debian/Ubuntu: yes (compilado como default)
# RHEL 7+: yes (pero usa imjournal, así que no importa mucho)
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
│    MESSAGE        — texto del log                                           │
│    PRIORITY       — nivel (0-7)                                             │
│    SYSLOG_FACILITY — facility (numérico)                                    │
│    SYSLOG_IDENTIFIER — identificador/tag                                    │
│    SYSLOG_PID     — PID                                                     │
│    timestamp      — hora del mensaje                                        │
│                                                                             │
│  LO QUE NO se envía:                                                        │
│    ✗ _SYSTEMD_UNIT — unidad de systemd                                      │
│    ✗ _EXE, _CMDLINE — información del ejecutable                            │
│    ✗ Campos personalizados (LogExtraFields)                                  │
│    ✗ _BOOT_ID, _MACHINE_ID — metadatos del journal                          │
│                                                                             │
│  El mensaje llega a rsyslog como syslog tradicional:                        │
│  <pri>timestamp hostname identifier[PID]: MESSAGE                           │
└─────────────────────────────────────────────────────────────────────────────┘
```

**Consecuencia clave:** al usar ForwardToSyslog, los campos estructurados del journal **se pierden**. El mensaje se reduce al formato syslog tradicional de una sola línea de texto.

### Otros destinos de forwarding

```ini
# /etc/systemd/journald.conf
[Journal]
ForwardToSyslog=yes      # a rsyslog (via /dev/log)
ForwardToKMsg=no         # al buffer del kernel (dmesg)
ForwardToConsole=no      # a una consola TTY
ForwardToWall=yes        # emergencias a todos los terminales (wall)
```

Si `ForwardToConsole=yes`, se puede configurar:

```ini
TTYPath=/dev/console     # a qué TTY enviar
```

### Control de niveles por destino (MaxLevel)

```ini
# Limitar qué prioridades se envían a cada destino:
MaxLevelStore=debug      # qué se guarda en el journal local
MaxLevelSyslog=debug     # qué se reenvía a syslog
MaxLevelKMsg=notice      # qué se reenvía a kmsg (dmesg)
MaxLevelConsole=info     # qué se reenvía a consola
MaxLevelWall=emerg       # qué se envía a wall
```

**Ejemplo práctico:** si `MaxLevelSyslog=warning`, los mensajes con prioridad `info`, `notice` y `debug` se guardan en el journal pero **no** se reenvían a rsyslog.

---

## Configurar imjournal (RHEL)

```bash
# /etc/rsyslog.conf — módulo imjournal:
module(load="imjournal"
    StateFile="imjournal.state"         # posición de lectura (persiste entre reinicios)
    ratelimit.interval="600"            # ventana de rate limiting (segundos)
    ratelimit.burst="20000"             # máximo mensajes por ventana
    IgnorePreviousMessages="off"        # off = SÍ procesar mensajes previos al arranque
    DefaultSeverity="5"                 # prioridad por defecto si no hay PRIORITY (notice)
    DefaultFacility="user"              # facility por defecto si no hay SYSLOG_FACILITY
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

### Problema 1: Mensajes duplicados

```
SÍNTOMA: el mismo mensaje aparece DOS veces en /var/log/syslog

CAUSA: ambos mecanismos están activos simultáneamente:
  1. journald → ForwardToSyslog=yes → /dev/log → imuxsock → rsyslog
  2. rsyslog → imjournal → lee /var/log/journal/

  El mensaje llega por DOS caminos distintos

SOLUCIÓN:
  Opción A: Usar solo imjournal → ForwardToSyslog=no
  Opción B: Usar solo imuxsock → NO cargar imjournal
```

```bash
# Verificar qué módulos están activos:
grep -rE "imuxsock|imjournal" /etc/rsyslog.conf /etc/rsyslog.d/*.conf 2>/dev/null

# Si ambos están, desactivar uno
```

### Problema 2: Mensajes perdidos

```
SÍNTOMA: el journal tiene mensajes que NO aparecen en syslog

CAUSAS POSIBLES:

  1. ForwardToSyslog=no
     → journald no envía nada a rsyslog

  2. MaxLevelSyslog filtra prioridades
     → grep MaxLevelSyslog /etc/systemd/journald.conf
     → Si es "warning", info y debug NO se reenvían

  3. Rate limiting en journald
     → journalctl -u systemd-journald --grep="Suppressed" --no-pager
     → "Suppressed N messages from unit.service"

  4. Rate limiting en imjournal
     → Configurable con ratelimit.interval y ratelimit.burst

  5. imjournal se quedó atrás
     → Si el journal crece más rápido de lo que imjournal lee

  6. El mensaje vino de stdout (journal lo captura)
     → Si la app no usa syslog(), no llega a rsyslog vía ForwardToSyslog
     → Verificar: journalctl _TRANSPORT=stdout -u myapp
```

### Problema 3: journal tiene más que syslog

```bash
# Diferencias posibles:
# 1. ForwardToSyslog=no
# 2. MaxLevelSyslog por debajo de la prioridad del mensaje
# 3. Rate limiting en journald o rsyslog
# 4. La app escribe a stdout (journald lo captura) pero no usa syslog()

# Ver distribución de prioridades:
journalctl -F PRIORITY --since today | sort | uniq -c

# Mensajes suprimidos por rate limiting:
journalctl -u systemd-journald --grep=Suppressed --no-pager --since today
```

### Verificar el flujo completo

```bash
# PASO 1: Enviar mensaje de prueba
MSG="fwd-test-$(date +%s)"
logger -t fwd-test -p local0.info "$MSG"

# PASO 2: Verificar que journald lo tiene
journalctl SYSLOG_IDENTIFIER=fwd-test -n 1 --no-pager

# PASO 3: Verificar que syslog lo recibió
grep "$MSG" /var/log/syslog /var/log/messages 2>/dev/null || echo "NO encontrado"

# PASO 4: Si NO llegó a syslog, diagnosticar:
echo "ForwardToSyslog:"
grep -i ForwardToSyslog /etc/systemd/journald.conf || echo "(default)"
echo "MaxLevelSyslog:"
grep -i MaxLevelSyslog /etc/systemd/journald.conf || echo "(default: debug)"
echo "Módulos de rsyslog:"
grep -rE "imuxsock|imjournal" /etc/rsyslog.conf /etc/rsyslog.d/*.conf 2>/dev/null
```

---

## Escenarios de configuración

### Escenario 1: Solo journald (contenedores, servidores modernos)

```ini
# /etc/systemd/journald.conf
[Journal]
Storage=persistent
ForwardToSyslog=no          # NO reenviar a syslog
SystemMaxUse=1G
Compress=yes
```

```bash
# Deshabilitar rsyslog:
sudo systemctl disable --now rsyslog
```

| Ventajas | Desventajas |
|----------|-------------|
| Menos procesos, menos I/O | Sin reenvío remoto nativo robusto |
| Consultas indexadas con journalctl | Herramientas legacy no leen el journal |
| Campos estructurados preservados | Alternativa remota: `systemd-journal-upload` |

**Uso típico:** contenedores, servidores cloud, entornos modernos.

### Escenario 2: Solo rsyslog (compatibilidad legacy)

```ini
# /etc/systemd/journald.conf
[Journal]
Storage=volatile            # solo RAM, no persiste
ForwardToSyslog=yes
RuntimeMaxUse=50M
```

journald actúa como buffer temporal; todo se persiste a través de rsyslog.

| Ventajas | Desventajas |
|----------|-------------|
| Control total con rsyslog | Se pierde búsqueda estructurada |
| logrotate para rotación | Sin logs de boots anteriores |
| Herramientas legacy funcionan | Sin campos indexados |
| Envío remoto robusto (TCP/TLS/RELP) | |

**Uso típico:** infraestructura legacy, servidores con integración a ELK/Splunk vía rsyslog.

### Escenario 3: Ambos (producción típica)

```ini
# /etc/systemd/journald.conf
[Journal]
Storage=persistent
ForwardToSyslog=yes
SystemMaxUse=1G
SystemMaxFileSize=100M
Compress=yes
MaxLevelSyslog=info        # reenviar info y superior a syslog
```

| journald | rsyslog |
|----------|---------|
| Guarda todo con indexación | Archivos de texto plano |
| Búsqueda rápida con journalctl | Reenvío remoto a servidor central |
| Logs de boots anteriores | Integración con ELK/Splunk |

**Uso típico:** producción, la configuración más común.

### Tabla comparativa de escenarios

| Aspecto | Solo journal | Solo rsyslog | Ambos |
|---------|-------------|-------------|-------|
| Búsqueda | journalctl | grep/awk | Ambos |
| Campos estruct. | Sí | No | Solo journal |
| Envío remoto | journal-upload | TCP/TLS/RELP | rsyslog |
| Rotación | Automática | logrotate | Ambos |
| Texto plano | No | Sí | Sí |
| I/O de disco | Menor | Mayor | Mayor |
| Complejidad | Baja | Media | Media-alta |
| Uso típico | Contenedores | Legacy | Producción |

### Debian vs RHEL

```bash
# --- Debian ---
# Mecanismo: ForwardToSyslog=yes + imuxsock
grep -i "ForwardToSyslog" /etc/systemd/journald.conf
grep -E "imuxsock|imjournal" /etc/rsyslog.conf | grep -v "^#"

# --- RHEL ---
# Mecanismo: imjournal (pull)
grep -E "imuxsock|imjournal" /etc/rsyslog.conf | grep -v "^#"
```

---

## Migrar de rsyslog a journal-only

```bash
# 1. Verificar que el journal es persistente:
ls -d /var/log/journal/ && echo "OK: persistente"

# 2. Buscar dependencias de archivos syslog:
grep -r "/var/log/syslog\|/var/log/messages" \
    /etc/ /opt/ /usr/local/ 2>/dev/null | head -10

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
  grep x /var/log/syslog /var/log/messages   → verificar que syslog lo tiene
  grep -rE "imuxsock|imjournal" /etc/rsyslog.d/

PROBLEMAS COMUNES:
  Duplicados    → ambos mecanismos activos (desactivar uno)
  Perdidos      → rate limiting, MaxLevelSyslog, o ForwardToSyslog=no
  Diferencias   → stdout (journal) vs syslog() (rsyslog)

IMJOURNAL:
  $!campo       → acceder a campos del journal desde rsyslog
  StateFile     → persiste posición de lectura entre reinicios

FORWARDING:
  ForwardToSyslog=yes     → reenviar a rsyslog
  MaxLevelSyslog=info     → solo info y superior a syslog
  ForwardToWall=yes       → emergencias a todos los terminales
```

---

## Labs

### Lab Parte 1 — Mecanismos de forwarding

**Paso 1.1: Los dos mecanismos**

```bash
docker compose exec debian-dev bash -c '
echo "=== Dos mecanismos de forwarding ==="
echo ""
echo "--- Mecanismo 1: ForwardToSyslog (push) ---"
echo "  journald ── /dev/log ──→ rsyslog (imuxsock)"
echo "     │           (socket)       │"
echo "     │                          └─→ /var/log/syslog"
echo "     └─→ /var/log/journal/"
echo ""
echo "  journald ENVIA copia a rsyslog via socket"
echo "  rsyslog escucha con imuxsock"
echo "  Usado en: Debian/Ubuntu"
echo ""
echo "--- Mecanismo 2: imjournal (pull) ---"
echo "  journald ──→ /var/log/journal/"
echo "                      │"
echo "                rsyslog (imjournal)"
echo "                      │"
echo "                      └─→ /var/log/messages"
echo ""
echo "  rsyslog LEE directamente del journal"
echo "  No depende de ForwardToSyslog"
echo "  Usado en: RHEL/Fedora"
'
```

**Paso 1.2: ForwardToSyslog (Debian)**

```bash
docker compose exec debian-dev bash -c '
echo "=== ForwardToSyslog ==="
echo ""
echo "--- Estado actual ---"
echo "Configuración explícita:"
grep -i "ForwardToSyslog" /etc/systemd/journald.conf 2>/dev/null || \
    echo "  (no definido, usando default)"
echo ""
echo "Configuración efectiva:"
systemd-analyze cat-config systemd/journald.conf 2>/dev/null | \
    grep -i "Forward" || echo "  (systemd-analyze no disponible)"
echo ""
echo "--- Qué envía ForwardToSyslog ---"
echo "  SÍ envía: MESSAGE, PRIORITY, FACILITY, IDENTIFIER, PID"
echo "  NO envía: _SYSTEMD_UNIT, _EXE, _CMDLINE, LogExtraFields"
echo ""
echo "  Los campos estructurados del journal SE PIERDEN"
'
```

**Paso 1.3: imjournal (RHEL)**

```bash
docker compose exec alma-dev bash -c '
echo "=== imjournal (RHEL) ==="
echo ""
echo "rsyslog lee directamente del journal:"
echo ""
echo "--- Configuración en rsyslog.conf ---"
grep -A5 "imjournal" /etc/rsyslog.conf 2>/dev/null | head -10
echo ""
echo "--- Módulos cargados ---"
grep -rE "imuxsock|imjournal" /etc/rsyslog.conf /etc/rsyslog.d/ 2>/dev/null | \
    grep -v "^#"
'
```

**Paso 1.4: Verificar el flujo completo**

```bash
docker compose exec debian-dev bash -c '
echo "=== Verificar flujo journald → rsyslog ==="
MSG="fwd-test-$(date +%s)"
echo "1. Enviando mensaje de prueba..."
logger -t fwd-test "$MSG"
sleep 1
echo ""
echo "2. En journal:"
journalctl SYSLOG_IDENTIFIER=fwd-test -n 1 --no-pager 2>/dev/null || \
    echo "   (no encontrado en journal)"
echo ""
echo "3. En archivos de rsyslog:"
if grep -q "$MSG" /var/log/syslog 2>/dev/null; then
    echo "   ENCONTRADO en /var/log/syslog"
    grep "$MSG" /var/log/syslog | tail -1
elif grep -q "$MSG" /var/log/messages 2>/dev/null; then
    echo "   ENCONTRADO en /var/log/messages"
    grep "$MSG" /var/log/messages | tail -1
else
    echo "   NO encontrado en archivos de rsyslog"
    echo "   ForwardToSyslog:"
    grep -i "ForwardToSyslog" /etc/systemd/journald.conf 2>/dev/null || echo "     (default)"
fi
'
```

### Lab Parte 2 — Problemas y diagnóstico

**Paso 2.1: Mensajes duplicados**

```bash
docker compose exec debian-dev bash -c '
echo "=== Mensajes duplicados ==="
echo ""
echo "Si AMBOS mecanismos están activos, rsyslog recibe"
echo "el mensaje por DOS caminos:"
echo "  1. journald → /dev/log → imuxsock → rsyslog"
echo "  2. rsyslog → imjournal → lee del journal"
echo ""
echo "--- Verificar si hay duplicación ---"
echo "Módulos cargados:"
grep -rE "imuxsock|imjournal" /etc/rsyslog.conf /etc/rsyslog.d/ 2>/dev/null | \
    grep -v "^#"
echo ""
HAS_UXSOCK=$(grep -rc "imuxsock" /etc/rsyslog.conf /etc/rsyslog.d/ 2>/dev/null | awk -F: "{s+=\$NF} END {print s+0}")
HAS_JOURNAL=$(grep -rc "imjournal" /etc/rsyslog.conf /etc/rsyslog.d/ 2>/dev/null | awk -F: "{s+=\$NF} END {print s+0}")
if [[ "$HAS_UXSOCK" -gt 0 ]] && [[ "$HAS_JOURNAL" -gt 0 ]]; then
    echo "ATENCIÓN: ambos imuxsock e imjournal cargados"
    echo "  Posible duplicación de mensajes"
else
    echo "OK: solo un mecanismo activo"
fi
echo ""
echo "--- Solución ---"
echo "  Si usas imjournal: ForwardToSyslog=no"
echo "  Si usas imuxsock: no cargar imjournal"
'
```

**Paso 2.2: Mensajes perdidos**

```bash
docker compose exec debian-dev bash -c '
echo "=== Mensajes perdidos ==="
echo ""
echo "Causas de que journal tenga mensajes que syslog no:"
echo ""
echo "1. ForwardToSyslog=no"
grep -i "ForwardToSyslog" /etc/systemd/journald.conf 2>/dev/null || \
    echo "   (default, probablemente yes)"
echo ""
echo "2. MaxLevelSyslog filtra prioridades"
grep -i "MaxLevelSyslog" /etc/systemd/journald.conf 2>/dev/null || \
    echo "   (default: debug = todo se reenvía)"
echo ""
echo "3. Rate limiting en journald"
journalctl -u systemd-journald --grep="Suppressed" --no-pager \
    --since "1 hour ago" 2>/dev/null | tail -3
SUPPRESSED=$(journalctl -u systemd-journald --grep="Suppressed" \
    --no-pager --since "1 hour ago" 2>/dev/null | wc -l)
echo "   Mensajes suprimidos en la última hora: $SUPPRESSED"
echo ""
echo "4. Rate limiting en rsyslog"
grep -r "ratelimit" /etc/rsyslog.conf /etc/rsyslog.d/ 2>/dev/null | \
    grep -v "^#" | head -3 || echo "   (sin rate limiting explícito)"
echo ""
echo "5. El servicio escribe a stdout (journal lo captura)"
echo "   pero no usa syslog() (no llega a /dev/log)"
'
```

**Paso 2.3: Rate limiting**

```bash
docker compose exec debian-dev bash -c '
echo "=== Rate limiting ==="
echo ""
echo "--- En journald ---"
echo "Configuración:"
grep -iE "RateLimit" /etc/systemd/journald.conf 2>/dev/null || \
    echo "  (defaults: RateLimitIntervalSec=30s, RateLimitBurst=10000)"
echo ""
echo "Para ajustar:"
echo "  [Journal]"
echo "  RateLimitIntervalSec=30s"
echo "  RateLimitBurst=10000"
echo ""
echo "--- En rsyslog (imjournal) ---"
grep -A2 "imjournal" /etc/rsyslog.conf 2>/dev/null | \
    grep -i "ratelimit" || echo "  (defaults de imjournal)"
echo ""
echo "Para ajustar:"
echo "  module(load=\"imjournal\""
echo "      ratelimit.interval=\"600\""
echo "      ratelimit.burst=\"20000\")"
'
```

### Lab Parte 3 — Escenarios y comparación

**Paso 3.1: Solo journald**

```bash
docker compose exec debian-dev bash -c '
echo "=== Escenario: solo journald ==="
echo ""
echo "# /etc/systemd/journald.conf"
echo "[Journal]"
echo "Storage=persistent"
echo "ForwardToSyslog=no"
echo "SystemMaxUse=1G"
echo "Compress=yes"
echo ""
echo "Deshabilitar rsyslog:"
echo "  systemctl disable --now rsyslog"
echo ""
echo "Ventajas: menos procesos, menos I/O, búsqueda indexada"
echo "Desventajas: sin reenvío remoto robusto, herramientas legacy incompatibles"
echo "Uso típico: contenedores, servidores cloud"
'
```

**Paso 3.2: Solo rsyslog**

```bash
docker compose exec debian-dev bash -c '
echo "=== Escenario: solo rsyslog ==="
echo ""
echo "# /etc/systemd/journald.conf"
echo "[Journal]"
echo "Storage=volatile"
echo "ForwardToSyslog=yes"
echo "RuntimeMaxUse=50M"
echo ""
echo "journald solo mantiene un buffer en RAM."
echo "Todo se persiste a través de rsyslog."
echo ""
echo "Ventajas: control total con rsyslog, logrotate, envío remoto robusto"
echo "Desventajas: sin búsqueda estructurada, sin campos indexados"
echo "Uso típico: compatibilidad con infraestructura legacy"
'
```

**Paso 3.3: Ambos (producción típica)**

```bash
docker compose exec debian-dev bash -c '
echo "=== Escenario: ambos (producción típica) ==="
echo ""
echo "# /etc/systemd/journald.conf"
echo "[Journal]"
echo "Storage=persistent"
echo "ForwardToSyslog=yes"
echo "SystemMaxUse=1G"
echo "Compress=yes"
echo "MaxLevelSyslog=info"
echo ""
echo "journald guarda todo con búsqueda indexada."
echo "rsyslog recibe copias para texto plano, reenvío remoto, monitoreo."
echo ""
echo "Es la configuración más común en producción."
echo ""
echo "--- Este sistema ---"
echo "journald:"
grep -E "^(Storage|Forward|SystemMax|Compress)" \
    /etc/systemd/journald.conf 2>/dev/null || echo "  (defaults)"
echo ""
echo "rsyslog:"
systemctl is-active rsyslog 2>/dev/null && echo "  activo" || echo "  no activo"
echo ""
echo "Journal persistente:"
ls -d /var/log/journal/ 2>/dev/null && echo "  sí" || echo "  no (volatile)"
'
```

**Paso 3.4: Debian vs RHEL**

```bash
docker compose exec debian-dev bash -c '
echo "=== Debian ==="
echo "Mecanismo: ForwardToSyslog + imuxsock"
echo "ForwardToSyslog:"
grep -i "ForwardToSyslog" /etc/systemd/journald.conf 2>/dev/null || echo "  yes (default)"
echo "Módulo rsyslog:"
grep -E "imuxsock|imjournal" /etc/rsyslog.conf 2>/dev/null | grep -v "^#" | head -3
'
echo ""
docker compose exec alma-dev bash -c '
echo "=== RHEL ==="
echo "Mecanismo: imjournal (pull)"
echo "ForwardToSyslog:"
grep -i "ForwardToSyslog" /etc/systemd/journald.conf 2>/dev/null || echo "  yes (default)"
echo "Módulo rsyslog:"
grep -E "imuxsock|imjournal" /etc/rsyslog.conf 2>/dev/null | grep -v "^#" | head -3
'
```

---

## Ejercicios

### Ejercicio 1 — Identificar el mecanismo

Determina qué mecanismo de forwarding usa tu sistema.

```bash
echo "=== ForwardToSyslog ==="
grep -i ForwardToSyslog /etc/systemd/journald.conf 2>/dev/null || echo "(default)"

echo "=== Módulos de rsyslog ==="
grep -rE "imuxsock|imjournal" /etc/rsyslog.conf /etc/rsyslog.d/*.conf 2>/dev/null | grep -v "^#"

echo "=== Archivo de syslog ==="
ls -la /var/log/syslog /var/log/messages 2>/dev/null
```

**Predicción:**

<details><summary>¿Qué esperas ver en un sistema Debian?</summary>

En Debian:
- `ForwardToSyslog` probablemente comentado (default: `yes`)
- `imuxsock` cargado en rsyslog (no `imjournal`)
- `/var/log/syslog` existente

El mecanismo es **push**: journald empuja a `/dev/log`, rsyslog escucha con `imuxsock`.
</details>

### Ejercicio 2 — Verificar el flujo

Envía un mensaje de prueba y rastrea su camino completo.

```bash
MSG="flow-test-$(date +%s)"
logger -t flow-test -p local0.info "$MSG"
sleep 1

echo "=== 1. ¿Está en journal? ==="
journalctl SYSLOG_IDENTIFIER=flow-test -n 1 --no-pager

echo "=== 2. ¿Está en syslog? ==="
grep "$MSG" /var/log/syslog /var/log/messages 2>/dev/null || echo "NO encontrado"

echo "=== 3. ¿Con qué priority? ==="
journalctl SYSLOG_IDENTIFIER=flow-test -o json-pretty -n 1 --no-pager | grep PRIORITY
```

**Predicción:**

<details><summary>¿Aparecerá en ambos (journal y syslog)?</summary>

Sí, si `ForwardToSyslog=yes` (default en Debian) y rsyslog está activo. `logger` escribe al socket syslog, journald lo captura Y lo reenvía a rsyslog. El `PRIORITY` será `"6"` (info, ya que usamos `-p local0.info`).
</details>

### Ejercicio 3 — Campos que se pierden en el forwarding

Compara los campos disponibles en journal vs lo que llega a syslog.

```bash
logger -t field-test "test campos $(date +%s)"
sleep 1

echo "=== Campos en journal (verbose) ==="
journalctl SYSLOG_IDENTIFIER=field-test -n 1 -o verbose --no-pager

echo ""
echo "=== Línea en syslog ==="
grep "field-test" /var/log/syslog 2>/dev/null | tail -1 || \
grep "field-test" /var/log/messages 2>/dev/null | tail -1
```

**Predicción:**

<details><summary>¿Qué campos del journal NO aparecerán en syslog?</summary>

El journal muestra campos como `_PID`, `_UID`, `_GID`, `_COMM`, `_EXE`, `_CMDLINE`, `_SYSTEMD_UNIT`, `_BOOT_ID`, `_MACHINE_ID`, `_TRANSPORT`, etc. En syslog solo queda la línea tradicional: `<timestamp> <hostname> <identifier>[<PID>]: <MESSAGE>`. Todos los campos con prefijo `_` se pierden en el forwarding vía ForwardToSyslog.
</details>

### Ejercicio 4 — MaxLevelSyslog

Observa cómo `MaxLevelSyslog` filtra qué prioridades llegan a rsyslog.

```bash
echo "=== MaxLevelSyslog actual ==="
grep -i MaxLevelSyslog /etc/systemd/journald.conf 2>/dev/null || \
    echo "(default: debug — todo se reenvía)"

echo ""
echo "=== Distribución de prioridades en el journal (hoy) ==="
journalctl --since today -o json --no-pager 2>/dev/null | \
    jq -r '.PRIORITY // "sin-priority"' 2>/dev/null | \
    sort | uniq -c | sort -rn
```

**Predicción:**

<details><summary>Si MaxLevelSyslog=warning, ¿qué prioridades llegan a syslog?</summary>

Solo prioridades 0 (emerg), 1 (alert), 2 (crit), 3 (err) y 4 (warning) llegarían a rsyslog. Las prioridades 5 (notice), 6 (info) y 7 (debug) se guardan en el journal pero NO se reenvían. Esto reduce la carga en rsyslog y el tamaño de los archivos de texto plano, manteniendo la capacidad de investigar con journalctl cuando se necesita más detalle.
</details>

### Ejercicio 5 — Detectar duplicación

Verifica si tu sistema tiene ambos mecanismos activos simultáneamente.

```bash
echo "=== Módulos de input de rsyslog ==="
grep -rE "imuxsock|imjournal" /etc/rsyslog.conf /etc/rsyslog.d/ 2>/dev/null | grep -v "^#"

echo ""
echo "=== ForwardToSyslog ==="
grep -i ForwardToSyslog /etc/systemd/journald.conf 2>/dev/null || echo "(default: yes)"

echo ""
echo "=== Riesgo de duplicación ==="
HAS_UXSOCK=$(grep -rc "imuxsock" /etc/rsyslog.conf /etc/rsyslog.d/ 2>/dev/null | awk -F: '{s+=$NF} END {print s+0}')
HAS_JOURNAL=$(grep -rc "imjournal" /etc/rsyslog.conf /etc/rsyslog.d/ 2>/dev/null | awk -F: '{s+=$NF} END {print s+0}')
if [[ "$HAS_UXSOCK" -gt 0 ]] && [[ "$HAS_JOURNAL" -gt 0 ]]; then
    echo "ATENCIÓN: ambos imuxsock e imjournal cargados → posible duplicación"
else
    echo "OK: solo un mecanismo activo"
fi
```

**Predicción:**

<details><summary>¿Tu sistema tiene riesgo de duplicación?</summary>

Normalmente no. Debian carga solo `imuxsock`, RHEL solo `imjournal`. El riesgo aparece si alguien añade manualmente el módulo contrario sin desactivar el existente. La solución es: con `imjournal` → `ForwardToSyslog=no`; con `imuxsock` → no cargar `imjournal`.
</details>

### Ejercicio 6 — Rate limiting en journald

Investiga la configuración de rate limiting y si se han suprimido mensajes.

```bash
echo "=== Rate limiting en journald ==="
grep -iE "RateLimit" /etc/systemd/journald.conf 2>/dev/null || \
    echo "(defaults: RateLimitIntervalSec=30s, RateLimitBurst=10000)"

echo ""
echo "=== Mensajes suprimidos (última hora) ==="
journalctl -u systemd-journald --grep="Suppressed" --since "1 hour ago" --no-pager 2>/dev/null | tail -5
SUPPRESSED=$(journalctl -u systemd-journald --grep="Suppressed" --since "1 hour ago" --no-pager 2>/dev/null | wc -l)
echo "Total de eventos de supresión: $SUPPRESSED"

echo ""
echo "=== Rate limiting en imjournal ==="
grep -A3 "imjournal" /etc/rsyslog.conf 2>/dev/null | grep -i "ratelimit" || \
    echo "(defaults de imjournal o no usa imjournal)"
```

**Predicción:**

<details><summary>¿Es probable que haya mensajes suprimidos en un entorno de lab?</summary>

En un entorno de lab con poco tráfico, es improbable. Los defaults (`RateLimitBurst=10000` por cada 30 segundos) son muy permisivos. El rate limiting se convierte en problema en producción cuando un servicio emite miles de mensajes por segundo (p. ej., un bucle de errores). Si hay supresión, el journal registra "Suppressed N messages from &lt;unit&gt;".
</details>

### Ejercicio 7 — Template con campos del journal (imjournal)

Escribe un template de rsyslog que use campos del journal vía `$!`.

```bash
# Este template SOLO funciona con imjournal (RHEL), no con imuxsock (Debian):
cat << 'EOF'
# Template con campos del journal:
template(name="JournalEnriched" type="string"
    string="%timegenerated:::date-rfc3339% %$!_HOSTNAME% %$!_SYSTEMD_UNIT% [%$!_PID%]: %msg%\n"
)

# Uso en un filtro:
if $!_SYSTEMD_UNIT == "sshd.service" then {
    action(type="omfile" file="/var/log/sshd-journal.log"
           template="JournalEnriched")
    stop
}
EOF
```

**Predicción:**

<details><summary>¿Funcionará este template en Debian con imuxsock?</summary>

No. `$!` solo está disponible con `imjournal`, que lee los campos estructurados del journal. En Debian con `imuxsock`, los campos como `$!_SYSTEMD_UNIT` estarán vacíos porque ForwardToSyslog solo envía el mensaje en formato syslog tradicional. Para usar este template necesitas RHEL/CentOS con imjournal, o cambiar la configuración de Debian para usar imjournal en lugar de imuxsock.
</details>

### Ejercicio 8 — Escenario de producción

Diseña la configuración `journald.conf` para un servidor de producción que necesita: journal persistente, reenvío a rsyslog para texto plano y remoto, pero sin reenviar mensajes debug.

```bash
cat << 'EOF'
# /etc/systemd/journald.conf
[Journal]
Storage=persistent
ForwardToSyslog=yes
SystemMaxUse=1G
SystemMaxFileSize=100M
Compress=yes
MaxLevelSyslog=info        # reenviar info y superior, debug NO
MaxLevelStore=debug        # guardar todo en journal
ForwardToWall=yes          # emergencias a terminales
EOF
```

**Predicción:**

<details><summary>¿Qué efecto tiene MaxLevelSyslog=info en esta configuración?</summary>

Los mensajes de prioridad `debug` (7) no se reenvían a rsyslog, pero sí se guardan en el journal (`MaxLevelStore=debug`). Esto significa que `journalctl -p debug` encuentra todo, pero en `/var/log/syslog` solo aparecen mensajes info (6) y superiores. Reduce el I/O de rsyslog y el tamaño de los archivos de texto plano, manteniendo la capacidad de investigar con journalctl cuando se necesita detalle.
</details>

### Ejercicio 9 — Diagnóstico completo de forwarding

Ejecuta un diagnóstico completo del flujo de forwarding de tu sistema.

```bash
echo "=== DIAGNÓSTICO COMPLETO DE FORWARDING ==="
echo ""

echo "1. Mecanismo:"
FTS=$(grep -i "^ForwardToSyslog" /etc/systemd/journald.conf 2>/dev/null | cut -d= -f2)
echo "   ForwardToSyslog=${FTS:-"(default)"}"

echo ""
echo "2. Módulos de rsyslog:"
grep -rE "imuxsock|imjournal" /etc/rsyslog.conf /etc/rsyslog.d/ 2>/dev/null | grep -v "^#"

echo ""
echo "3. MaxLevelSyslog:"
MLS=$(grep -i "^MaxLevelSyslog" /etc/systemd/journald.conf 2>/dev/null | cut -d= -f2)
echo "   MaxLevelSyslog=${MLS:-"(default: debug)"}"

echo ""
echo "4. Test de flujo:"
MSG="diag-$(date +%s)"
logger -t diag-test "$MSG"
sleep 1
IN_JOURNAL=$(journalctl SYSLOG_IDENTIFIER=diag-test -n 1 --no-pager 2>/dev/null | grep -c "$MSG")
IN_SYSLOG=$(grep -c "$MSG" /var/log/syslog /var/log/messages 2>/dev/null | awk -F: '{s+=$NF} END {print s+0}')
echo "   En journal: $([[ $IN_JOURNAL -gt 0 ]] && echo SÍ || echo NO)"
echo "   En syslog:  $([[ $IN_SYSLOG -gt 0 ]] && echo SÍ || echo NO)"

echo ""
echo "5. Rate limiting (suprimidos última hora):"
journalctl -u systemd-journald --grep="Suppressed" --since "1 hour ago" --no-pager 2>/dev/null | tail -3 || echo "   Ninguno"
```

**Predicción:**

<details><summary>¿Cuál es el flujo esperado en Debian con defaults?</summary>

1. `ForwardToSyslog=yes` (default)
2. Módulo: `imuxsock` (no `imjournal`)
3. `MaxLevelSyslog=debug` (default: todo se reenvía)
4. El mensaje aparece en journal (SÍ) y en syslog (SÍ)
5. Sin supresiones en un entorno de lab

El flujo es: `logger` → journald → `/dev/log` (ForwardToSyslog) → rsyslog (imuxsock) → `/var/log/syslog`.
</details>

### Ejercicio 10 — Planificar una migración

Evalúa si tu sistema podría migrar a journal-only y qué pasos serían necesarios.

```bash
echo "=== EVALUACIÓN PARA MIGRACIÓN A JOURNAL-ONLY ==="
echo ""

echo "1. ¿Journal persistente?"
ls -d /var/log/journal/ 2>/dev/null && echo "   SÍ" || echo "   NO — necesita Storage=persistent"

echo ""
echo "2. ¿rsyslog activo?"
systemctl is-active rsyslog 2>/dev/null && echo "   SÍ (activo)" || echo "   NO (inactivo o no instalado)"

echo ""
echo "3. Dependencias de archivos syslog:"
DEPS=$(grep -r "/var/log/syslog\|/var/log/messages" /etc/ 2>/dev/null | grep -v "rsyslog" | grep -v "logrotate" | wc -l)
echo "   $DEPS archivo(s) fuera de rsyslog/logrotate referencian syslog/messages"
if [[ $DEPS -gt 0 ]]; then
    grep -r "/var/log/syslog\|/var/log/messages" /etc/ 2>/dev/null | grep -v "rsyslog" | grep -v "logrotate" | head -5
fi

echo ""
echo "4. Pasos de migración:"
echo "   a) Configurar Storage=persistent"
echo "   b) Establecer ForwardToSyslog=no"
echo "   c) Reiniciar systemd-journald"
echo "   d) Migrar scripts de grep/awk a journalctl"
echo "   e) systemctl disable --now rsyslog"
echo "   f) Verificar con journalctl --since '5 min ago'"
echo "   g) Para reenvío remoto: systemd-journal-upload"
```

**Predicción:**

<details><summary>¿Qué es lo más probable que bloquee una migración?</summary>

Los bloqueantes más comunes son:

1. **Scripts que parsean `/var/log/syslog`:** cron jobs, alertas, herramientas de monitoreo que usan `grep` sobre archivos de texto. Necesitan reescribirse para usar `journalctl`.
2. **Reenvío remoto:** rsyslog tiene reenvío remoto robusto (TCP, TLS, RELP con colas en disco). `systemd-journal-upload/remote` existe pero es menos maduro.
3. **Integración con plataformas de log:** Splunk, ELK, etc., suelen tener plugins para archivos syslog que habría que reconfigurar.

En entornos de contenedores o cloud, la migración es trivial porque las apps ya escriben a stdout y los logs van al journal directamente.
</details>
