# Lab — Filtros y templates

## Objetivo

Entender filtros avanzados y templates en rsyslog: filtros basados
en propiedades (:propiedad, operador, "valor"), operadores (isequal,
contains, startswith, regex), la accion stop, RainerScript
(if/then/else, and/or/not), propiedades del mensaje, templates
predefinidos y personalizados, templates con archivo dinamico
(dynaFile), templates JSON, y el orden de procesamiento de reglas.

## Prerequisitos

- Entorno del curso levantado (`docker compose up -d`)

---

## Parte 1 — Filtros de propiedades

### Objetivo

Filtrar mensajes por cualquier campo del mensaje syslog.

### Paso 1.1: Sintaxis de filtros de propiedades

```bash
docker compose exec debian-dev bash -c '
echo "=== Property-based filters ==="
echo ""
echo "Sintaxis:"
echo "  :propiedad, operador, \"valor\"    accion"
echo ""
echo "--- Ejemplos ---"
echo ":programname, isequal, \"sshd\"      /var/log/sshd.log"
echo "  Mensajes cuyo programa sea exactamente sshd"
echo ""
echo ":msg, contains, \"error\"            /var/log/errors.log"
echo "  Mensajes que contengan la palabra error"
echo ""
echo ":hostname, isequal, \"web01\"        /var/log/web01.log"
echo "  Mensajes del host web01 (util para log central)"
echo ""
echo ":fromhost-ip, startswith, \"192.168.\" /var/log/lan.log"
echo "  Mensajes de la red 192.168.x.x"
'
```

### Paso 1.2: Operadores disponibles

```bash
docker compose exec debian-dev bash -c '
echo "=== Operadores ==="
echo ""
echo "--- Comparacion de strings ---"
echo "  isequal      igual exacto"
echo "  startswith   empieza con"
echo "  contains     contiene"
echo "  contains_i   contiene (case-insensitive)"
echo ""
echo "--- Expresiones regulares ---"
echo "  regex        regex basica"
echo "  ereregex     ERE (extended regex)"
echo ""
echo "--- Negacion ---"
echo "  !contains    NO contiene"
echo "  !isequal     NO es igual"
echo ""
echo "--- Ejemplos ---"
echo ":programname, isequal, \"nginx\"           exactamente nginx"
echo ":programname, startswith, \"php\"           empieza con php"
echo ":msg, contains, \"failed\"                  contiene failed"
echo ":msg, !contains, \"debug\"                  NO contiene debug"
echo ":msg, regex, \"error|fail|crit\"            coincide regex"
echo ":msg, contains_i, \"ERROR\"                 case-insensitive"
'
```

### Paso 1.3: Propiedades del mensaje

```bash
docker compose exec debian-dev bash -c '
echo "=== Propiedades del mensaje ==="
echo ""
echo "| Propiedad              | Descripcion                     |"
echo "|------------------------|---------------------------------|"
echo "| msg                    | Texto del mensaje               |"
echo "| programname            | Programa que genero el log      |"
echo "| hostname               | Hostname del origen             |"
echo "| syslogfacility-text    | Facility como texto (kern,auth) |"
echo "| syslogseverity-text    | Prioridad como texto (err,info) |"
echo "| fromhost               | Host del que se recibio         |"
echo "| fromhost-ip            | IP del host origen              |"
echo "| timegenerated          | Cuando rsyslog recibio          |"
echo "| timereported           | Timestamp del mensaje original  |"
echo "| syslogtag              | Tag (ej: sshd[1234]:)           |"
echo "| pri                    | Prioridad numerica              |"
echo ""
echo "--- Ver propiedades en logs reales ---"
echo ""
echo "Un mensaje tipico:"
head -1 /var/log/syslog 2>/dev/null || head -1 /var/log/messages 2>/dev/null
echo ""
echo "  timestamp     = timegenerated/timereported"
echo "  hostname      = hostname"
echo "  tag[PID]:     = syslogtag / programname"
echo "  mensaje       = msg"
'
```

### Paso 1.4: La accion stop

```bash
docker compose exec debian-dev bash -c '
echo "=== La accion stop ==="
echo ""
echo "stop termina el procesamiento del mensaje."
echo "El mensaje NO se evalua contra las reglas siguientes."
echo ""
echo "--- Sin stop ---"
echo ":programname, isequal, \"sshd\"   /var/log/sshd.log"
echo "*.*                               /var/log/syslog"
echo "  sshd aparece en sshd.log Y en syslog"
echo ""
echo "--- Con stop ---"
echo ":programname, isequal, \"sshd\"   /var/log/sshd.log"
echo ":programname, isequal, \"sshd\"   stop"
echo "*.*                               /var/log/syslog"
echo "  sshd aparece SOLO en sshd.log (stop evita syslog)"
echo ""
echo "--- Descartar mensajes ruidosos ---"
echo ":programname, isequal, \"systemd-resolved\" stop"
echo ":msg, contains, \"UFW BLOCK\"               stop"
echo "  Estos mensajes no llegan a ningun log"
echo ""
echo "--- Legacy (obsoleto) ---"
echo ":programname, isequal, \"sshd\"   ~"
echo "  ~ era el discard antiguo, reemplazado por stop"
'
```

### Paso 1.5: Filtros en la config real

```bash
docker compose exec debian-dev bash -c '
echo "=== Filtros en la config del sistema ==="
echo ""
echo "--- Buscar filtros de propiedades ---"
grep -rh "^:" /etc/rsyslog.conf /etc/rsyslog.d/ 2>/dev/null | \
    grep -v "^#" | head -10
echo ""
FOUND=$(grep -rch "^:" /etc/rsyslog.conf /etc/rsyslog.d/ 2>/dev/null | \
    awk "{s+=\$1} END {print s+0}")
echo "Total de filtros de propiedades encontrados: $FOUND"
echo ""
echo "--- Buscar reglas con stop ---"
grep -rh "stop" /etc/rsyslog.conf /etc/rsyslog.d/ 2>/dev/null | \
    grep -v "^#" | head -5
echo ""
echo "--- Enviar mensaje y rastrear ---"
logger -t filter-test "Este mensaje va a donde las reglas digan"
sleep 1
echo "Buscando en todos los logs:"
for f in /var/log/syslog /var/log/messages /var/log/auth.log /var/log/daemon.log; do
    [[ -f "$f" ]] || continue
    if grep -q "filter-test" "$f" 2>/dev/null; then
        echo "  ENCONTRADO en $f"
    fi
done
'
```

---

## Parte 2 — RainerScript

### Objetivo

Usar el lenguaje moderno de rsyslog para filtrado complejo.

### Paso 2.1: Sintaxis basica

```bash
docker compose exec debian-dev bash -c '
echo "=== RainerScript ==="
echo ""
echo "Lenguaje de configuracion moderno de rsyslog."
echo "Permite condiciones, logica compleja y mejor legibilidad."
echo ""
echo "--- if/then ---"
cat << '\''EOF'\''
if $programname == "sshd" then {
    action(type="omfile" file="/var/log/sshd.log")
    stop
}
EOF
echo ""
echo "--- if/then/else ---"
cat << '\''EOF'\''
if $syslogseverity <= 3 then {
    # err, crit, alert, emerg
    action(type="omfile" file="/var/log/critical.log")
} else {
    action(type="omfile" file="/var/log/normal.log")
}
EOF
echo ""
echo "--- Variables de prioridad (numericas) ---"
echo "  0=emerg, 1=alert, 2=crit, 3=err"
echo "  4=warning, 5=notice, 6=info, 7=debug"
echo "  <= 3 significa err y superiores"
'
```

### Paso 2.2: Operadores RainerScript

```bash
docker compose exec debian-dev bash -c '
echo "=== Operadores RainerScript ==="
echo ""
echo "--- Comparacion ---"
echo "  ==          igual"
echo "  !=          diferente"
echo "  <, >, <=, >=  numerico"
echo "  contains    contiene string"
echo "  startswith  empieza con"
echo "  regex       expresion regular"
echo ""
echo "--- Logicos ---"
echo "  and         Y logico"
echo "  or          O logico"
echo "  not         negacion"
echo ""
echo "--- Ejemplos ---"
cat << '\''EOF'\''
# nginx con warning o superior:
if $programname == "nginx" and $syslogseverity <= 4 then {
    action(type="omfile" file="/var/log/nginx-warnings.log")
}

# Mensajes con error o fail:
if $msg contains "error" or $msg contains "fail" then {
    action(type="omfile" file="/var/log/problems.log")
}

# Todo excepto systemd y kernel:
if not ($programname == "systemd" or $programname == "kernel") then {
    action(type="omfile" file="/var/log/apps.log")
}
EOF
'
```

### Paso 2.3: Ejemplo completo RainerScript

```bash
docker compose exec debian-dev bash -c '
echo "=== Ejemplo completo ==="
echo ""
echo "Archivo: /etc/rsyslog.d/30-custom.conf"
echo ""
cat << '\''EOF'\''
# Separar logs por aplicacion:
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

# Alertas criticas a archivo separado:
if $syslogseverity <= 2 then {
    action(type="omfile"
           file="/var/log/critical-alerts.log"
           sync="on")
}
EOF
echo ""
echo "--- Buscar RainerScript en el sistema ---"
grep -rh "^if " /etc/rsyslog.conf /etc/rsyslog.d/ 2>/dev/null | \
    grep -v "^#" | head -5 || echo "(sin reglas RainerScript)"
'
```

### Paso 2.4: Filtrado de seguridad

```bash
docker compose exec debian-dev bash -c '
echo "=== Filtrado de seguridad ==="
echo ""
echo "Capturar todos los eventos de autenticacion:"
echo ""
cat << '\''EOF'\''
# /etc/rsyslog.d/20-security.conf
if $programname == "sshd" or
   $programname == "sudo" or
   $programname == "su" or
   $programname == "login" or
   $msg contains "authentication" then {
    action(type="omfile"
           file="/var/log/security-audit.log"
           sync="on")
    # Sin stop: tambien van a auth.log por la regla normal
}
EOF
echo ""
echo "sync=\"on\" asegura que los logs criticos llegan a disco"
echo "No usar stop: queremos que aparezcan en auth.log tambien"
'
```

---

## Parte 3 — Templates

### Objetivo

Controlar el formato de los mensajes de log.

### Paso 3.1: Templates predefinidos

```bash
docker compose exec debian-dev bash -c '
echo "=== Templates predefinidos ==="
echo ""
echo "--- RSYSLOG_TraditionalFileFormat (default) ---"
echo "  Mar 17 15:30:00 server sshd[1234]: mensaje"
echo "  Formato clasico de syslog"
echo ""
echo "--- RSYSLOG_FileFormat ---"
echo "  2026-03-17T15:30:00.123456+00:00 server sshd[1234]: mensaje"
echo "  Con timestamp ISO 8601 y microsegundos"
echo ""
echo "--- RSYSLOG_SyslogProtocol23Format ---"
echo "  Formato RFC 5424 completo (envio remoto moderno)"
echo ""
echo "--- Verificar que template usa este sistema ---"
head -3 /var/log/syslog 2>/dev/null || head -3 /var/log/messages 2>/dev/null
echo ""
echo "Si el timestamp es \"Mar 17 15:30:00\" → TraditionalFileFormat"
echo "Si el timestamp es \"2026-03-17T15:30:00\" → FileFormat"
'
```

### Paso 3.2: Crear templates personalizados

```bash
docker compose exec debian-dev bash -c '
echo "=== Templates personalizados ==="
echo ""
echo "--- Sintaxis ---"
cat << '\''EOF'\''
template(name="MyFormat" type="string"
    string="%timegenerated:::date-rfc3339% %HOSTNAME% %programname%: %msg%\n"
)

# Usar en una regla:
*.* action(type="omfile" file="/var/log/custom.log" template="MyFormat")
EOF
echo ""
echo "--- Propiedades en templates ---"
echo "  %msg%                    el mensaje"
echo "  %HOSTNAME%               hostname"
echo "  %programname%            nombre del programa"
echo "  %syslogseverity-text%    prioridad como texto"
echo "  %syslogfacility-text%    facility como texto"
echo "  %timegenerated%          timestamp"
echo "  %fromhost-ip%            IP de origen"
echo "  %syslogtag%              tag (programa[PID]:)"
echo ""
echo "--- Modificadores ---"
echo "  %msg:::drop-last-lf%          quitar salto de linea final"
echo "  %timegenerated:::date-rfc3339%  formato RFC 3339"
echo "  %msg:1:50%                     primeros 50 caracteres"
echo "  %programname:::uppercase%      en mayusculas"
'
```

### Paso 3.3: Template con archivo dinamico

```bash
docker compose exec debian-dev bash -c '
echo "=== Template con dynaFile ==="
echo ""
echo "Un template puede definir el NOMBRE del archivo:"
echo ""
cat << '\''EOF'\''
# Archivo por hostname:
template(name="PerHostLog" type="string"
    string="/var/log/remote/%HOSTNAME%/%programname%.log"
)

# Usar con dynaFile:
*.* action(type="omfile" dynaFile="PerHostLog")

# Crea archivos como:
#   /var/log/remote/web01/nginx.log
#   /var/log/remote/web01/sshd.log
#   /var/log/remote/db01/postgresql.log
EOF
echo ""
echo "--- Template por fecha ---"
cat << '\''EOF'\''
template(name="DailyLog" type="string"
    string="/var/log/daily/%$year%-%$month%-%$day%.log"
)
EOF
echo ""
echo "--- Buscar templates en el sistema ---"
grep -rhi "template" /etc/rsyslog.conf /etc/rsyslog.d/ 2>/dev/null | \
    grep -v "^#" | head -5 || echo "(sin templates personalizados)"
'
```

### Paso 3.4: Template JSON

```bash
docker compose exec debian-dev bash -c '
echo "=== Template JSON ==="
echo ""
echo "Para enviar logs estructurados (ej: a Elasticsearch):"
echo ""
cat << '\''EOF'\''
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
EOF
echo ""
echo "Resultado:"
echo "{\"@timestamp\":\"2026-03-17T15:30:00+00:00\",\"host\":\"server\",\"severity\":\"info\",\"program\":\"nginx\",\"message\":\"GET /index 200\"}"
'
```

### Paso 3.5: Orden de procesamiento

```bash
docker compose exec debian-dev bash -c '
echo "=== Orden de procesamiento ==="
echo ""
echo "rsyslog procesa las reglas EN ORDEN de arriba a abajo."
echo "Un mensaje se evalua contra TODAS las reglas (salvo stop)."
echo ""
echo "--- Ejemplo ---"
echo "auth.*                    /var/log/auth.log     # regla 1"
echo "*.*;auth.none             /var/log/syslog       # regla 2"
echo ""
echo "auth.log recibe mensajes de auth (regla 1)"
echo "syslog recibe todo EXCEPTO auth (regla 2, por auth.none)"
echo ""
echo "Sin auth.none en regla 2, auth apareceria en AMBOS archivos."
echo ""
echo "--- Orden de archivos ---"
echo "  1. /etc/rsyslog.conf se procesa primero"
echo "  2. Luego los archivos de /etc/rsyslog.d/ en orden alfabetico"
echo ""
echo "--- Archivos y su orden ---"
echo "rsyslog.conf (primero)"
ls /etc/rsyslog.d/*.conf 2>/dev/null | while read f; do
    echo "  $(basename $f)"
done
echo ""
echo "Las reglas con stop detienen el procesamiento de ese mensaje."
echo "Colocar stop DESPUES de la accion, no antes."
'
```

---

## Limpieza final

No hay recursos que limpiar.

---

## Conceptos reforzados

1. Los filtros de propiedades (:propiedad, operador, "valor")
   permiten filtrar por cualquier campo: programname, msg,
   hostname, fromhost-ip, syslogseverity-text, etc.

2. La accion stop detiene el procesamiento de un mensaje.
   Sin stop, el mensaje se evalua contra todas las reglas
   siguientes y puede aparecer en multiples archivos.

3. RainerScript (if/then/else) permite logica compleja con
   operadores and, or, not. Las prioridades son numericas
   (0=emerg a 7=debug).

4. Templates controlan el formato de salida. Los predefinidos
   son TraditionalFileFormat (clasico) y FileFormat (ISO 8601).

5. dynaFile permite crear archivos dinamicos basados en
   propiedades (%HOSTNAME%, %programname%). Ideal para
   servidores de log centralizados.

6. rsyslog procesa reglas en orden. Los archivos de
   /etc/rsyslog.d/ se procesan en orden alfabetico.
   Usar numeracion (20-, 50-) para controlar el orden.
