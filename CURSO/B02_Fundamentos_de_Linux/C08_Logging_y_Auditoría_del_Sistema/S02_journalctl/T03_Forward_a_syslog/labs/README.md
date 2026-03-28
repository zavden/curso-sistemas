# Lab — Forward a syslog

## Objetivo

Entender el forwarding journald→rsyslog: los dos mecanismos
(ForwardToSyslog push con imuxsock vs imjournal pull), configurar
ForwardToSyslog, que campos se envian y cuales se pierden,
imjournal y acceso a campos del journal ($!), problemas comunes
(duplicados, mensajes perdidos, rate limiting), verificar el flujo
completo, escenarios de configuracion, y diferencias Debian vs RHEL.

## Prerequisitos

- Entorno del curso levantado (`docker compose up -d`)

---

## Parte 1 — Mecanismos de forwarding

### Objetivo

Entender los dos caminos para que rsyslog obtenga logs del journal.

### Paso 1.1: Dos mecanismos

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

### Paso 1.2: ForwardToSyslog (Debian)

```bash
docker compose exec debian-dev bash -c '
echo "=== ForwardToSyslog ==="
echo ""
echo "--- Configuracion ---"
echo "# /etc/systemd/journald.conf"
echo "[Journal]"
echo "ForwardToSyslog=yes"
echo ""
echo "--- Estado actual ---"
echo "Configuracion explicita:"
grep -i "ForwardToSyslog" /etc/systemd/journald.conf 2>/dev/null || \
    echo "  (no definido, usando default)"
echo ""
echo "Configuracion efectiva:"
systemd-analyze cat-config systemd/journald.conf 2>/dev/null | \
    grep -i "Forward" || echo "  (systemd-analyze no disponible)"
echo ""
echo "--- Que envia ForwardToSyslog ---"
echo "  SI envia: MESSAGE, PRIORITY, FACILITY, IDENTIFIER, PID"
echo "  NO envia: _SYSTEMD_UNIT, _EXE, _CMDLINE, LogExtraFields"
echo ""
echo "  Los campos estructurados del journal SE PIERDEN"
echo "  El mensaje llega como syslog tradicional"
echo ""
echo "--- Otros forwards ---"
echo "  ForwardToKMsg=no        al buffer del kernel"
echo "  ForwardToConsole=no     a una consola TTY"
echo "  ForwardToWall=yes       emergencias a terminales"
echo ""
echo "  MaxLevelSyslog=debug    nivel maximo a reenviar"
echo "  MaxLevelConsole=info    nivel maximo a consola"
'
```

### Paso 1.3: imjournal (RHEL)

```bash
docker compose exec alma-dev bash -c '
echo "=== imjournal (RHEL) ==="
echo ""
echo "rsyslog lee directamente del journal:"
echo ""
echo "--- Configuracion en rsyslog.conf ---"
grep -A5 "imjournal" /etc/rsyslog.conf 2>/dev/null | head -10
echo ""
echo "--- imjournal vs imuxsock ---"
echo ""
echo "| Aspecto              | imjournal (RHEL) | imuxsock (Debian) |"
echo "|----------------------|------------------|-------------------|"
echo "| Mecanismo            | Pull (lee journal)| Push (recibe socket)|"
echo "| Campos estructurados | SI (acceso a \$!) | NO (se pierden)   |"
echo "| Depende de Forward   | NO               | SI                |"
echo "| Recursos             | Mas (lee archivos)| Menos (socket)   |"
echo "| Journal completo     | SI               | Solo lo reenviado |"
echo ""
echo "--- Modulos cargados ---"
grep -rE "imuxsock|imjournal" /etc/rsyslog.conf /etc/rsyslog.d/ 2>/dev/null | \
    grep -v "^#"
'
```

### Paso 1.4: Verificar el flujo completo

```bash
docker compose exec debian-dev bash -c '
echo "=== Verificar flujo journald → rsyslog ==="
echo ""
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
    echo ""
    echo "   Diagnostico:"
    echo "   ForwardToSyslog:"
    grep -i "ForwardToSyslog" /etc/systemd/journald.conf 2>/dev/null || echo "     (default)"
    echo "   Modulos rsyslog:"
    grep -rE "imuxsock|imjournal" /etc/rsyslog.conf /etc/rsyslog.d/ 2>/dev/null | grep -v "^#" | head -3
fi
'
```

---

## Parte 2 — Problemas y diagnostico

### Objetivo

Identificar y resolver problemas comunes de forwarding.

### Paso 2.1: Mensajes duplicados

```bash
docker compose exec debian-dev bash -c '
echo "=== Mensajes duplicados ==="
echo ""
echo "Si AMBOS mecanismos estan activos, rsyslog recibe"
echo "el mensaje por DOS caminos:"
echo "  1. journald → /dev/log → imuxsock → rsyslog"
echo "  2. rsyslog → imjournal → lee del journal"
echo ""
echo "--- Verificar si hay duplicacion ---"
echo "Modulos cargados:"
grep -rE "imuxsock|imjournal" /etc/rsyslog.conf /etc/rsyslog.d/ 2>/dev/null | \
    grep -v "^#"
echo ""
HAS_UXSOCK=$(grep -rc "imuxsock" /etc/rsyslog.conf /etc/rsyslog.d/ 2>/dev/null | awk -F: "{s+=\$NF} END {print s+0}")
HAS_JOURNAL=$(grep -rc "imjournal" /etc/rsyslog.conf /etc/rsyslog.d/ 2>/dev/null | awk -F: "{s+=\$NF} END {print s+0}")
if [[ "$HAS_UXSOCK" -gt 0 ]] && [[ "$HAS_JOURNAL" -gt 0 ]]; then
    echo "ATENCION: ambos imuxsock e imjournal cargados"
    echo "  Posible duplicacion de mensajes"
else
    echo "OK: solo un mecanismo activo"
fi
echo ""
echo "--- Solucion ---"
echo "  Si usas imjournal: ForwardToSyslog=no"
echo "  Si usas imuxsock: no cargar imjournal"
'
```

### Paso 2.2: Mensajes perdidos

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
    echo "   (default: debug = todo se reenvia)"
echo ""
echo "3. Rate limiting en journald"
journalctl -u systemd-journald --grep="Suppressed" --no-pager \
    --since "1 hour ago" 2>/dev/null | tail -3
SUPPRESSED=$(journalctl -u systemd-journald --grep="Suppressed" \
    --no-pager --since "1 hour ago" 2>/dev/null | wc -l)
echo "   Mensajes suprimidos en la ultima hora: $SUPPRESSED"
echo ""
echo "4. Rate limiting en rsyslog"
grep -r "ratelimit" /etc/rsyslog.conf /etc/rsyslog.d/ 2>/dev/null | \
    grep -v "^#" | head -3 || echo "   (sin rate limiting explicito)"
echo ""
echo "5. El servicio escribe a stdout (journal lo captura)"
echo "   pero no usa syslog() (no llega a /dev/log)"
'
```

### Paso 2.3: Rate limiting

```bash
docker compose exec debian-dev bash -c '
echo "=== Rate limiting ==="
echo ""
echo "--- En journald ---"
echo "Configuracion:"
grep -iE "RateLimit" /etc/systemd/journald.conf 2>/dev/null || \
    echo "  (defaults: RateLimitIntervalSec=30s, RateLimitBurst=10000)"
echo ""
echo "Para aumentar:"
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

---

## Parte 3 — Escenarios y comparacion

### Objetivo

Entender los escenarios de configuracion tipicos.

### Paso 3.1: Solo journald

```bash
docker compose exec debian-dev bash -c '
echo "=== Escenario: solo journald ==="
echo ""
cat << '\''EOF'\''
# /etc/systemd/journald.conf
[Journal]
Storage=persistent
ForwardToSyslog=no
SystemMaxUse=1G
Compress=yes
EOF
echo ""
echo "Deshabilitar rsyslog:"
echo "  systemctl disable --now rsyslog"
echo ""
echo "--- Ventajas ---"
echo "  Menos procesos, menos I/O"
echo "  Consultas indexadas con journalctl"
echo "  Campos estructurados preservados"
echo ""
echo "--- Desventajas ---"
echo "  Sin reenvio remoto nativo robusto"
echo "  Herramientas legacy no leen el journal"
echo "  Necesita systemd-journal-upload para remoto"
echo ""
echo "--- Uso tipico ---"
echo "  Contenedores, servidores modernos, entornos cloud"
'
```

### Paso 3.2: Solo rsyslog

```bash
docker compose exec debian-dev bash -c '
echo "=== Escenario: solo rsyslog ==="
echo ""
cat << '\''EOF'\''
# /etc/systemd/journald.conf
[Journal]
Storage=volatile
ForwardToSyslog=yes
RuntimeMaxUse=50M
EOF
echo ""
echo "journald solo mantiene un buffer en RAM."
echo "Todo se persiste a traves de rsyslog."
echo ""
echo "--- Ventajas ---"
echo "  Control total con rsyslog"
echo "  logrotate para rotacion"
echo "  Herramientas legacy funcionan"
echo "  Envio remoto robusto (TCP, TLS, RELP)"
echo ""
echo "--- Desventajas ---"
echo "  Pierdes busqueda estructurada del journal"
echo "  Sin campos indexados"
echo "  Sin -o json con campos del sistema"
echo ""
echo "--- Uso tipico ---"
echo "  Compatibilidad con infraestructura legacy"
'
```

### Paso 3.3: Ambos (produccion tipica)

```bash
docker compose exec debian-dev bash -c '
echo "=== Escenario: ambos (produccion tipica) ==="
echo ""
cat << '\''EOF'\''
# /etc/systemd/journald.conf
[Journal]
Storage=persistent
ForwardToSyslog=yes
SystemMaxUse=1G
Compress=yes
MaxLevelSyslog=info
EOF
echo ""
echo "journald guarda todo con busqueda indexada."
echo "rsyslog recibe copias para:"
echo "  - Archivos de texto plano"
echo "  - Reenvio remoto a servidor central"
echo "  - Integracion con monitoreo"
echo ""
echo "Es la configuracion mas comun en produccion."
echo ""
echo "--- Este sistema ---"
echo ""
echo "journald:"
grep -E "^(Storage|Forward|SystemMax|Compress)" \
    /etc/systemd/journald.conf 2>/dev/null || echo "  (defaults)"
echo ""
echo "rsyslog:"
systemctl is-active rsyslog 2>/dev/null && echo "  activo" || echo "  no activo"
echo ""
echo "Journal persistente:"
ls -d /var/log/journal/ 2>/dev/null && echo "  si" || echo "  no (volatile)"
'
```

### Paso 3.4: Debian vs RHEL

```bash
docker compose exec debian-dev bash -c '
echo "=== Debian ==="
echo ""
echo "Mecanismo: ForwardToSyslog + imuxsock"
echo "ForwardToSyslog:"
grep -i "ForwardToSyslog" /etc/systemd/journald.conf 2>/dev/null || echo "  yes (default)"
echo "Modulo rsyslog:"
grep -E "imuxsock|imjournal" /etc/rsyslog.conf 2>/dev/null | grep -v "^#" | head -3
'
echo ""
docker compose exec alma-dev bash -c '
echo "=== RHEL ==="
echo ""
echo "Mecanismo: imjournal (pull)"
echo "ForwardToSyslog:"
grep -i "ForwardToSyslog" /etc/systemd/journald.conf 2>/dev/null || echo "  yes (default)"
echo "Modulo rsyslog:"
grep -E "imuxsock|imjournal" /etc/rsyslog.conf 2>/dev/null | grep -v "^#" | head -3
'
```

### Paso 3.5: Tabla comparativa

```bash
docker compose exec debian-dev bash -c '
echo "=== Tabla comparativa ==="
echo ""
echo "| Aspecto          | Solo journal | Solo rsyslog | Ambos        |"
echo "|------------------|--------------|--------------|--------------|"
echo "| Busqueda         | journalctl   | grep/awk     | Ambos        |"
echo "| Campos estruct.  | Si           | No           | Solo journal |"
echo "| Envio remoto     | journal-upload| TCP/TLS/RELP| rsyslog      |"
echo "| Rotacion         | Automatica   | logrotate    | Ambos        |"
echo "| Texto plano      | No           | Si           | Si           |"
echo "| I/O de disco     | Menor        | Mayor        | Mayor        |"
echo "| Complejidad      | Baja         | Media        | Media-alta   |"
echo "| Uso tipico       | Contenedores | Legacy       | Produccion   |"
'
```

---

## Limpieza final

No hay recursos que limpiar.

---

## Conceptos reforzados

1. Dos mecanismos: ForwardToSyslog (push, journald envia a
   /dev/log, imuxsock recibe) y imjournal (pull, rsyslog lee
   del journal). Debian usa el primero, RHEL el segundo.

2. ForwardToSyslog pierde campos estructurados del journal.
   imjournal los preserva y permite acceso con $!.

3. Si ambos mecanismos estan activos, hay duplicacion.
   Solucion: ForwardToSyslog=no con imjournal, o no cargar
   imjournal con imuxsock.

4. Rate limiting en journald (RateLimitBurst) y rsyslog
   (ratelimit.burst en imjournal) pueden causar perdida
   de mensajes.

5. Tres escenarios tipicos: solo journald (moderno,
   contenedores), solo rsyslog (legacy), ambos (produccion
   tipica con reenvio remoto).

6. Para verificar el flujo: enviar con logger, buscar en
   journalctl, buscar en /var/log/syslog o messages.
   Si falta, revisar ForwardToSyslog, MaxLevelSyslog y
   modulos de rsyslog.
