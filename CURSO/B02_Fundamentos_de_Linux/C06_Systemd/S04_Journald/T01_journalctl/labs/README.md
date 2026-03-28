# Lab — journalctl

## Objetivo

Dominar journalctl para consultar el journal de systemd: filtros
por unidad, tiempo, prioridad y boot, formatos de salida (short,
verbose, json), follow en vivo, filtros avanzados con campos,
uso en scripts con JSON y jq, y gestion de espacio en disco.

## Prerequisitos

- Entorno del curso levantado (`docker compose up -d`)

---

## Parte 1 — Filtros basicos

### Objetivo

Usar los filtros principales de journalctl para encontrar logs
relevantes.

### Paso 1.1: Filtro por unidad

```bash
docker compose exec debian-dev bash -c '
echo "=== Filtro por unidad ==="
echo ""
echo "--- Comandos ---"
echo "journalctl -u nginx.service    logs de un servicio"
echo "journalctl -u nginx            .service es implicito"
echo "journalctl -u sshd -u nginx    multiples unidades (OR)"
echo ""
echo "--- Logs del kernel ---"
echo "journalctl -k                  equivale a dmesg con mas info"
echo "                               timestamps completos + persistencia"
echo ""
echo "--- Intentar ---"
journalctl -u systemd-journald -n 5 --no-pager 2>&1 || \
    echo "(journalctl no disponible — journal no esta corriendo)"
'
```

### Paso 1.2: Filtro por tiempo

```bash
docker compose exec debian-dev bash -c '
echo "=== Filtro por tiempo ==="
echo ""
echo "--- --since ---"
echo "journalctl --since \"2024-03-17 10:00:00\""
echo "journalctl --since \"1 hour ago\""
echo "journalctl --since \"today\""
echo "journalctl --since \"yesterday\""
echo ""
echo "--- --until ---"
echo "journalctl --until \"2024-03-17 12:00:00\""
echo "journalctl --until \"30 min ago\""
echo ""
echo "--- Rango ---"
echo "journalctl --since \"2024-03-17 08:00\" --until \"2024-03-17 12:00\""
echo ""
echo "--- Formatos de tiempo aceptados ---"
echo "  YYYY-MM-DD HH:MM:SS"
echo "  today, yesterday, tomorrow"
echo "  N hours ago, N min ago, N days ago"
echo "  HH:MM (hoy a esa hora)"
'
```

### Paso 1.3: Filtro por prioridad

```bash
docker compose exec debian-dev bash -c '
echo "=== Filtro por prioridad ==="
echo ""
echo "syslog define 8 niveles:"
echo "  0 = emerg    sistema inutilizable"
echo "  1 = alert    accion inmediata necesaria"
echo "  2 = crit     condicion critica"
echo "  3 = err      error"
echo "  4 = warning  advertencia"
echo "  5 = notice   normal pero significativo"
echo "  6 = info     informacional"
echo "  7 = debug    depuracion"
echo ""
echo "--- Filtrar ---"
echo "journalctl -p err              err + crit + alert + emerg"
echo "journalctl -p warning          warning y superiores"
echo "journalctl -p 3                equivale a -p err"
echo ""
echo "--- Rango ---"
echo "journalctl -p err..warning     solo err y warning"
echo ""
echo "--- Combinaciones utiles ---"
echo "journalctl -u nginx -p err -b  errores de nginx, boot actual"
echo "journalctl -k -p warning --since today  kernel warnings de hoy"
'
```

### Paso 1.4: Filtro por boot

```bash
docker compose exec debian-dev bash -c '
echo "=== Filtro por boot ==="
echo ""
echo "journalctl -b       boot actual"
echo "journalctl -b 0     equivalente"
echo "journalctl -b -1    boot anterior"
echo "journalctl -b -2    dos boots atras"
echo ""
echo "--- Listar boots ---"
echo "journalctl --list-boots"
echo "  -3 abc123 Mon 2024-03-14 08:00 — Mon 2024-03-14 23:59"
echo "  -2 def456 Tue 2024-03-15 08:00 — Tue 2024-03-15 23:59"
echo "  -1 ghi789 Wed 2024-03-16 08:00 — Wed 2024-03-16 23:59"
echo "   0 jkl012 Thu 2024-03-17 08:00 — Thu 2024-03-17 15:30"
echo ""
echo "journalctl -b abc123    boot especifico por ID"
echo ""
echo "--- Por PID/UID ---"
echo "journalctl _PID=1234     logs de un proceso especifico"
echo "journalctl _UID=1000     logs de un usuario"
echo "journalctl _COMM=sshd    logs por nombre de comando"
echo "journalctl _EXE=/usr/sbin/sshd   por ruta del ejecutable"
'
```

---

## Parte 2 — Salida y formatos

### Objetivo

Controlar el formato y la cantidad de salida de journalctl.

### Paso 2.1: Follow y lineas

```bash
docker compose exec debian-dev bash -c '
echo "=== Follow y lineas ==="
echo ""
echo "--- Follow (como tail -f) ---"
echo "journalctl -f               seguimiento en vivo"
echo "journalctl -f -u nginx      follow solo nginx"
echo "journalctl -f -p err        follow solo errores"
echo ""
echo "--- Numero de lineas ---"
echo "journalctl -n 50            ultimas 50 lineas"
echo "journalctl -n 20 -u nginx   ultimas 20 de nginx"
echo ""
echo "--- Opciones de formato ---"
echo "journalctl --no-pager       sin less (para scripts)"
echo "journalctl --no-hostname    sin hostname"
echo "journalctl -r               orden inverso (mas recientes primero)"
echo ""
echo "--- Combinacion completa ---"
echo "journalctl -r -n 10 -u sshd --no-pager"
echo "  Ultimas 10 de sshd, mas recientes primero, sin pager"
'
```

### Paso 2.2: Formatos de salida (-o)

```bash
docker compose exec debian-dev bash -c '
echo "=== Formatos de salida ==="
echo ""
echo "--- short (default) ---"
echo "Mar 17 10:30:00 server sshd[1234]: Accepted publickey for user"
echo ""
echo "--- short-precise (con microsegundos) ---"
echo "Mar 17 10:30:00.123456 server sshd[1234]: ..."
echo ""
echo "--- short-iso (timestamp ISO 8601) ---"
echo "2024-03-17T10:30:00+0000 server sshd[1234]: ..."
echo ""
echo "--- verbose (todos los campos) ---"
echo "_BOOT_ID=def789..."
echo "_HOSTNAME=server"
echo "PRIORITY=6"
echo "_UID=0"
echo "_COMM=sshd"
echo "_EXE=/usr/sbin/sshd"
echo "_PID=1234"
echo "MESSAGE=Accepted publickey for user..."
echo "_TRANSPORT=syslog"
echo "SYSLOG_IDENTIFIER=sshd"
echo ""
echo "--- json (para procesamiento) ---"
echo "journalctl -o json -n 1"
echo "{\"__CURSOR\":\"s=abc...\",\"MESSAGE\":\"...\",\"PRIORITY\":\"6\",...}"
echo ""
echo "--- json-pretty (JSON formateado) ---"
echo "journalctl -o json-pretty -n 1"
echo ""
echo "--- cat (solo el mensaje) ---"
echo "journalctl -o cat -u nginx -n 5"
echo "Solo los mensajes, sin timestamp ni hostname"
'
```

### Paso 2.3: Comparar formatos

```bash
docker compose exec debian-dev bash -c '
echo "=== Comparar formatos con un mismo log ==="
echo ""
echo "--- short ---"
journalctl -o short -n 1 --no-pager 2>&1 || \
    echo "Mar 17 10:30:00 server systemd[1]: Started example service"
echo ""
echo "--- verbose ---"
journalctl -o verbose -n 1 --no-pager 2>&1 || \
    echo "_HOSTNAME=server"
echo "  PRIORITY=6"
echo "  _COMM=systemd"
echo "  MESSAGE=Started example service"
echo ""
echo "--- json-pretty ---"
journalctl -o json-pretty -n 1 --no-pager 2>&1 || \
    echo "{"
echo "  \"PRIORITY\": \"6\","
echo "  \"_COMM\": \"systemd\","
echo "  \"MESSAGE\": \"Started example service\""
echo "}"
echo ""
echo "--- cat ---"
journalctl -o cat -n 1 --no-pager 2>&1 || \
    echo "Started example service"
echo ""
echo "short = logs humanos"
echo "verbose = debug completo"
echo "json = procesamiento automatico"
echo "cat = solo el mensaje"
'
```

---

## Parte 3 — Campos y scripts

### Objetivo

Usar filtros avanzados con campos del journal y journalctl
en scripts.

### Paso 3.1: Campos del journal

```bash
docker compose exec debian-dev bash -c '
echo "=== Campos del journal ==="
echo ""
echo "Cada log es un conjunto de campos clave-valor"
echo ""
echo "--- Campos comunes ---"
echo "  _PID            PID del proceso"
echo "  _UID            UID del usuario"
echo "  _COMM           nombre del comando"
echo "  _EXE            ruta del ejecutable"
echo "  _HOSTNAME       hostname"
echo "  _TRANSPORT      como llego (journal, syslog, kernel, stdout)"
echo "  PRIORITY        nivel de prioridad (0-7)"
echo "  SYSLOG_IDENTIFIER  identificador de syslog"
echo "  _SYSTEMD_UNIT   unidad que genero el log"
echo "  MESSAGE         el mensaje"
echo ""
echo "--- Filtrar por campo ---"
echo "journalctl _TRANSPORT=kernel"
echo "journalctl SYSLOG_IDENTIFIER=sudo"
echo "journalctl _SYSTEMD_UNIT=docker.service"
echo ""
echo "--- Ver valores de un campo (-F) ---"
echo "journalctl -F _COMM      listar todos los comandos que generaron logs"
echo "journalctl -F PRIORITY   listar prioridades usadas"
echo ""
echo "--- Intentar ---"
journalctl -F _COMM --no-pager 2>&1 | head -10 || \
    echo "(journal no disponible)"
'
```

### Paso 3.2: Combinacion de filtros

```bash
docker compose exec debian-dev bash -c '
echo "=== Combinacion de filtros ==="
echo ""
echo "Los filtros se combinan con AND implicito:"
echo ""
echo "  journalctl -u nginx -p err -b"
echo "  → nginx AND errores AND boot actual"
echo ""
echo "  journalctl -k -p warning --since today"
echo "  → kernel AND warning+ AND hoy"
echo ""
echo "--- OR: multiples -u ---"
echo "  journalctl -u nginx -u sshd"
echo "  → nginx OR sshd (multiples -u son OR)"
echo ""
echo "--- OR con campos ---"
echo "  journalctl _COMM=nginx + _COMM=sshd"
echo "  → el operador + es OR entre campos"
echo ""
echo "--- Filtros utiles ---"
echo "  journalctl -p err --since \"1 hour ago\"  errores recientes"
echo "  journalctl -u sshd -b -1                 sshd del boot anterior"
echo "  journalctl -k --since today              kernel de hoy"
'
```

### Paso 3.3: Uso en scripts

```bash
docker compose exec debian-dev bash -c '
echo "=== journalctl en scripts ==="
echo ""
echo "--- Verificar errores recientes ---"
cat << '\''SCRIPT'\''
#!/usr/bin/env bash
if journalctl -u myapp -p err --since "5 min ago" -q --no-pager | grep -q .; then
    echo "myapp tiene errores recientes" >&2
fi
# -q = quiet (no muestra "-- No entries --")
SCRIPT
echo ""
echo "--- Extraer logs JSON para procesamiento ---"
cat << '\''SCRIPT'\''
journalctl -u nginx -o json --since "1 hour ago" --no-pager | \
    jq -r 'select(.PRIORITY == "3") | .MESSAGE'
SCRIPT
echo ""
echo "--- Contar errores por servicio ---"
cat << '\''SCRIPT'\''
journalctl -p err --since today -o json --no-pager | \
    jq -r '._SYSTEMD_UNIT // "unknown"' | sort | uniq -c | sort -rn
SCRIPT
echo ""
echo "--- Monitorear y alertar ---"
cat << '\''SCRIPT'\''
journalctl -f -u myapp -o json | while read -r line; do
    PRIORITY=$(echo "$line" | jq -r '.PRIORITY')
    if [[ "$PRIORITY" -le 3 ]]; then
        MSG=$(echo "$line" | jq -r '.MESSAGE')
        echo "ALERTA: $MSG"
    fi
done
SCRIPT
'
```

### Paso 3.4: Espacio en disco

```bash
docker compose exec debian-dev bash -c '
echo "=== Espacio en disco ==="
echo ""
echo "--- Ver uso ---"
echo "journalctl --disk-usage"
echo "  Archived and active journals take up 256.0M"
echo ""
echo "--- Limpiar por tamano ---"
echo "journalctl --vacuum-size=100M"
echo "  Elimina journals hasta que total <= 100MB"
echo ""
echo "--- Limpiar por tiempo ---"
echo "journalctl --vacuum-time=7d"
echo "  Elimina journals de mas de 7 dias"
echo ""
echo "--- Limpiar por archivos ---"
echo "journalctl --vacuum-files=5"
echo "  Mantiene solo 5 archivos"
echo ""
echo "--- Rotar + limpiar ---"
echo "journalctl --rotate --vacuum-size=100M"
echo "  Cierra journal activo y limpia"
echo ""
echo "--- Verificar integridad ---"
echo "journalctl --verify"
echo "  PASS: /var/log/journal/.../system.journal"
echo ""
echo "--- Intentar ---"
journalctl --disk-usage 2>&1 || \
    echo "(journal no disponible)"
'
```

---

## Limpieza final

No hay recursos que limpiar.

---

## Conceptos reforzados

1. Los filtros basicos de journalctl: `-u` (unidad), `--since/
   --until` (tiempo), `-p` (prioridad), `-b` (boot), `-k` (kernel),
   `_PID`/`_UID`/`_COMM` (proceso).

2. Los filtros se combinan con AND implicito. Multiples `-u` son
   OR. El operador `+` permite OR entre campos diferentes.

3. `-f` (follow) es como tail -f. `-n` limita lineas. `-r` invierte
   el orden. `--no-pager` es necesario en scripts.

4. Los formatos de salida: `short` (humano), `verbose` (todos los
   campos), `json`/`json-pretty` (procesamiento), `cat` (solo
   mensaje).

5. `-F CAMPO` lista valores unicos de un campo. Cada log tiene
   campos estructurados (_PID, _COMM, PRIORITY, MESSAGE, etc.)
   que permiten filtros precisos.

6. `--vacuum-size=`, `--vacuum-time=`, `--vacuum-files=` limpian
   journals antiguos. `--verify` comprueba integridad. `--rotate`
   fuerza rotacion.
