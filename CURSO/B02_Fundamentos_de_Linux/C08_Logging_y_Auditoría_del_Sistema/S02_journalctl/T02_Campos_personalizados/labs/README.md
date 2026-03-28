# Lab — Campos personalizados

## Objetivo

Entender los campos del journal: campos del sistema (prefijo _,
establecidos por journald, confiables), campos de usuario (sin
prefijo, establecidos por la app), campos internos (__CURSOR),
SYSLOG_IDENTIFIER vs _COMM, MESSAGE_ID para buscar tipos de
eventos, campos personalizados con systemd-cat y la API sd_journal,
_TRANSPORT, exploracion de campos con -F y -o verbose, y
SyslogIdentifier/LogExtraFields en unit files.

## Prerequisitos

- Entorno del curso levantado (`docker compose up -d`)

---

## Parte 1 — Tipos de campos

### Objetivo

Distinguir los tres tipos de campos del journal.

### Paso 1.1: Campos del sistema (prefijo _)

```bash
docker compose exec debian-dev bash -c '
echo "=== Campos del sistema (prefijo _) ==="
echo ""
echo "Los establece journald automaticamente."
echo "NO pueden ser falsificados por la aplicacion."
echo ""
echo "| Campo                | Descripcion                |"
echo "|----------------------|----------------------------|"
echo "| _PID                 | PID del proceso            |"
echo "| _UID                 | UID del usuario            |"
echo "| _GID                 | GID del grupo              |"
echo "| _COMM                | Nombre del comando         |"
echo "| _EXE                 | Ruta del ejecutable        |"
echo "| _CMDLINE             | Linea de comandos completa |"
echo "| _HOSTNAME            | Hostname                   |"
echo "| _TRANSPORT           | Como llego el log          |"
echo "| _SYSTEMD_UNIT        | Unidad de systemd          |"
echo "| _SYSTEMD_CGROUP      | cgroup                     |"
echo "| _BOOT_ID             | ID del boot actual         |"
echo "| _MACHINE_ID          | ID de la maquina           |"
echo ""
echo "Son CONFIABLES para auditoria (los establece el kernel/journald)."
echo ""
echo "--- Ver campos _ en una entrada real ---"
journalctl -n 1 -o verbose --no-pager 2>/dev/null | grep "^    _" | head -10
'
```

### Paso 1.2: Campos de usuario (sin prefijo)

```bash
docker compose exec debian-dev bash -c '
echo "=== Campos de usuario (sin prefijo _) ==="
echo ""
echo "Los establece la aplicacion:"
echo ""
echo "| Campo                | Descripcion               |"
echo "|----------------------|---------------------------|"
echo "| MESSAGE              | Texto del mensaje          |"
echo "| PRIORITY             | Prioridad (0-7)           |"
echo "| SYSLOG_FACILITY      | Facility syslog           |"
echo "| SYSLOG_IDENTIFIER    | Identificador (tag)       |"
echo "| SYSLOG_PID           | PID reportado por la app  |"
echo "| CODE_FILE            | Archivo fuente             |"
echo "| CODE_LINE            | Linea del codigo           |"
echo "| CODE_FUNC            | Funcion                    |"
echo ""
echo "--- Ver campos de usuario en una entrada ---"
journalctl -n 1 -o verbose --no-pager 2>/dev/null | \
    grep -E "^    [A-Z]" | grep -v "^    _" | head -10
'
```

### Paso 1.3: Campos internos (prefijo __)

```bash
docker compose exec debian-dev bash -c '
echo "=== Campos internos (prefijo __) ==="
echo ""
echo "Control interno de journald:"
echo ""
echo "| Campo                     | Descripcion               |"
echo "|---------------------------|---------------------------|"
echo "| __CURSOR                  | Posicion unica en journal |"
echo "| __REALTIME_TIMESTAMP      | Timestamp en microsegundos|"
echo "| __MONOTONIC_TIMESTAMP     | Timestamp monotonico      |"
echo ""
echo "Para uso interno. Solo __CURSOR es util (retomar lecturas)."
echo ""
echo "--- Ver ---"
journalctl -n 1 -o verbose --no-pager 2>/dev/null | grep "^    __" | head -5
'
```

### Paso 1.4: _TRANSPORT

```bash
docker compose exec debian-dev bash -c '
echo "=== _TRANSPORT ==="
echo ""
echo "Indica COMO llego el mensaje a journald:"
echo ""
echo "| Transport | Origen                                |"
echo "|-----------|---------------------------------------|"
echo "| journal   | API directa sd_journal_send()         |"
echo "| stdout    | stdout/stderr de servicio systemd     |"
echo "| syslog    | API syslog() o /dev/log               |"
echo "| kernel    | Mensajes del kernel (dmesg)           |"
echo "| audit     | Subsistema de auditoria               |"
echo "| driver    | Mensajes internos de journald         |"
echo ""
echo "--- Transports en este sistema ---"
journalctl -F _TRANSPORT 2>/dev/null || echo "(no disponible)"
echo ""
echo "--- Filtrar por transport ---"
echo "journalctl _TRANSPORT=kernel -p err    errores del kernel"
echo "journalctl _TRANSPORT=stdout -u nginx  stdout de nginx"
echo "journalctl _TRANSPORT=audit            mensajes de auditoria"
'
```

---

## Parte 2 — Identifiers y MESSAGE_ID

### Objetivo

Usar SYSLOG_IDENTIFIER y MESSAGE_ID para filtrar eventos.

### Paso 2.1: SYSLOG_IDENTIFIER

```bash
docker compose exec debian-dev bash -c '
echo "=== SYSLOG_IDENTIFIER ==="
echo ""
echo "El tag del mensaje syslog. Lo que ves entre hostname y PID:"
echo ""
echo "  Mar 17 15:30:00 server sshd[1234]: mensaje"
echo "                         ^^^^"
echo "                         SYSLOG_IDENTIFIER"
echo ""
echo "--- Filtrar ---"
echo "journalctl SYSLOG_IDENTIFIER=sshd"
echo "journalctl SYSLOG_IDENTIFIER=sudo"
echo ""
echo "--- Listar identifiers ---"
journalctl -F SYSLOG_IDENTIFIER 2>/dev/null | head -15
'
```

### Paso 2.2: SYSLOG_IDENTIFIER vs _COMM

```bash
docker compose exec debian-dev bash -c '
echo "=== SYSLOG_IDENTIFIER vs _COMM ==="
echo ""
echo "_COMM = nombre del ejecutable (lo pone journald, CONFIABLE)"
echo "SYSLOG_IDENTIFIER = tag que la app se pone a si misma"
echo ""
echo "Pueden diferir:"
echo "  _COMM=apache2  SYSLOG_IDENTIFIER=apache2   (coinciden)"
echo "  _COMM=python3  SYSLOG_IDENTIFIER=myapp     (difieren)"
echo "  _COMM=bash     SYSLOG_IDENTIFIER=backup.sh (difieren)"
echo ""
echo "Para filtrar por lo que la app dice ser: SYSLOG_IDENTIFIER"
echo "Para filtrar por lo que realmente es: _COMM o _EXE"
echo ""
echo "--- Establecer SYSLOG_IDENTIFIER ---"
echo "  logger -t myapp \"mensaje\"              con logger"
echo "  echo \"msg\" | systemd-cat -t myapp      con systemd-cat"
echo "  SyslogIdentifier=myapp                  en unit file"
echo ""
echo "--- Probar ---"
logger -t lab-ident "Mensaje de prueba"
sleep 1
echo "Enviado con: logger -t lab-ident"
echo ""
journalctl SYSLOG_IDENTIFIER=lab-ident -n 1 -o verbose --no-pager 2>/dev/null | \
    grep -E "SYSLOG_IDENTIFIER|_COMM|MESSAGE" | head -5
'
```

### Paso 2.3: MESSAGE_ID

```bash
docker compose exec debian-dev bash -c '
echo "=== MESSAGE_ID ==="
echo ""
echo "UUID que identifica un TIPO de mensaje."
echo "Permite buscar una clase de evento sin depender del texto."
echo ""
echo "--- MESSAGE_IDs comunes de systemd ---"
echo "  39f53479... → Unit started (arrancada)"
echo "  be02cf68... → Unit stopped (detenida)"
echo "  7d4958e8... → Unit failed (fallida)"
echo "  98268866... → Unit reloaded"
echo "  8d45620c... → New user session"
echo ""
echo "--- Buscar servicios arrancados ---"
journalctl MESSAGE_ID=39f53479d3a045ac8e11786248231fbf -b -n 5 --no-pager 2>/dev/null || \
    echo "(sin resultados)"
echo ""
echo "--- Buscar servicios fallidos ---"
journalctl MESSAGE_ID=7d4958e842da4a758f6c1cdc7b36dcc5 -b --no-pager 2>/dev/null | head -5
FAILED=$(journalctl MESSAGE_ID=7d4958e842da4a758f6c1cdc7b36dcc5 -b --no-pager 2>/dev/null | wc -l)
echo ""
echo "Servicios fallidos en este boot: $FAILED"
echo ""
echo "--- Catalogo ---"
echo "journalctl --catalog | head -50"
'
```

---

## Parte 3 — Campos custom y exploracion

### Objetivo

Enviar campos personalizados y explorar el journal.

### Paso 3.1: systemd-cat

```bash
docker compose exec debian-dev bash -c '
echo "=== systemd-cat ==="
echo ""
echo "Conecta la salida de un comando al journal:"
echo ""
echo "--- Enviar mensaje ---"
echo "mensaje personalizado desde lab" | systemd-cat -t lab-custom -p info 2>/dev/null
echo "Enviado con: echo \"...\" | systemd-cat -t lab-custom -p info"
echo ""
sleep 1
echo "--- Verificar ---"
journalctl SYSLOG_IDENTIFIER=lab-custom -n 1 --no-pager 2>/dev/null || \
    echo "(systemd-cat no disponible)"
echo ""
echo "--- Ver todos los campos ---"
journalctl SYSLOG_IDENTIFIER=lab-custom -n 1 -o verbose --no-pager 2>/dev/null | head -15
echo ""
echo "--- Ejecutar comando con salida al journal ---"
echo "systemd-cat -t backup /opt/backup/run.sh"
echo "  Todo stdout/stderr va al journal con tag backup"
'
```

### Paso 3.2: SyslogIdentifier y LogExtraFields en units

```bash
docker compose exec debian-dev bash -c '
echo "=== SyslogIdentifier en unit files ==="
echo ""
echo "En un .service, controla los campos del journal:"
echo ""
cat << '\''EOF'\''
[Service]
SyslogIdentifier=myapp
# → SYSLOG_IDENTIFIER=myapp

SyslogFacility=local0
# → SYSLOG_FACILITY=16 (local0)

SyslogLevelPrefix=true
# → Si la app imprime "<3>error", journald interpreta
#   <3> como prioridad err
EOF
echo ""
echo "=== LogExtraFields ==="
echo ""
cat << '\''EOF'\''
[Service]
SyslogIdentifier=myapp
LogExtraFields=APP_VERSION=2.1.0
LogExtraFields=ENVIRONMENT=production
LogExtraFields=TEAM=backend
EOF
echo ""
echo "Ahora se puede filtrar:"
echo "  journalctl ENVIRONMENT=production"
echo "  journalctl TEAM=backend -p err"
echo ""
echo "--- Buscar LogExtraFields en el sistema ---"
grep -r "LogExtraFields" /etc/systemd/system/ /usr/lib/systemd/system/ 2>/dev/null | \
    head -5 || echo "(sin LogExtraFields configurados)"
'
```

### Paso 3.3: Exploracion de campos

```bash
docker compose exec debian-dev bash -c '
echo "=== Explorar campos ==="
echo ""
echo "--- Listar todos los campos de un servicio ---"
journalctl -n 1 -o json-pretty --no-pager 2>/dev/null | \
    python3 -c "
import json, sys
try:
    d = json.load(sys.stdin)
    for k in sorted(d.keys()):
        print(f\"  {k}\")
except: print(\"  (no disponible)\")
" 2>/dev/null
echo ""
echo "--- Valores unicos de un campo ---"
echo ""
echo "Prioridades usadas:"
journalctl -F PRIORITY -b 2>/dev/null | sort | while read p; do
    case "$p" in
        0) echo "  $p = emerg" ;;
        1) echo "  $p = alert" ;;
        2) echo "  $p = crit" ;;
        3) echo "  $p = err" ;;
        4) echo "  $p = warning" ;;
        5) echo "  $p = notice" ;;
        6) echo "  $p = info" ;;
        7) echo "  $p = debug" ;;
        *) echo "  $p" ;;
    esac
done
echo ""
echo "UIDs que generan logs:"
journalctl -F _UID -b 2>/dev/null | head -5
'
```

### Paso 3.4: Campos de codigo fuente

```bash
docker compose exec debian-dev bash -c '
echo "=== Campos de codigo fuente ==="
echo ""
echo "Algunas aplicaciones incluyen informacion del codigo:"
echo "  CODE_FILE=src/handler.c"
echo "  CODE_LINE=142"
echo "  CODE_FUNC=handle_request"
echo ""
echo "--- Buscar en el sistema ---"
journalctl -b -o verbose --no-pager 2>/dev/null | \
    grep "CODE_FILE" | head -5 || echo "(sin campos CODE_FILE)"
echo ""
echo "--- systemd los incluye en sus mensajes ---"
echo "journalctl -u systemd-logind -o verbose -n 1 | grep CODE_"
journalctl -o verbose -n 1 --no-pager 2>/dev/null | \
    grep "CODE_" | head -3 || echo "(no encontrado)"
'
```

---

## Limpieza final

No hay recursos que limpiar.

---

## Conceptos reforzados

1. Campos con _ (sistema) los establece journald y son
   confiables. Campos sin _ (usuario) los establece la app.
   Campos con __ son internos de journald.

2. SYSLOG_IDENTIFIER es el tag (lo pone la app).
   _COMM es el ejecutable real (lo pone journald).
   Pueden diferir — usar _COMM para auditoria.

3. MESSAGE_ID es un UUID que identifica un tipo de evento.
   Permite buscar "servicios que fallaron" sin depender
   del texto del mensaje.

4. systemd-cat conecta stdout/stderr al journal con un
   SYSLOG_IDENTIFIER personalizado. Ideal para scripts.

5. LogExtraFields en unit files agrega campos personalizados
   a todas las entradas de un servicio. Permite filtrar por
   ENVIRONMENT, TEAM, APP_VERSION, etc.

6. journalctl -F CAMPO lista valores unicos de un campo.
   journalctl -o verbose muestra todos los campos de una
   entrada. Ambos son esenciales para exploracion.
