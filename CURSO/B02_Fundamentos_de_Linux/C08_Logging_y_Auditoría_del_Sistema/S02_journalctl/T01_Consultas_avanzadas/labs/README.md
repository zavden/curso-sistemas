# Lab — Consultas avanzadas

## Objetivo

Dominar consultas avanzadas de journalctl: combinar filtros
(AND implicito, OR entre -u, operador +), filtros de tiempo
(rangos exactos, cursores), formatos de salida (json, verbose,
cat, short-iso, short-precise), consultas de analisis (contar
errores, timelines, kernel, --grep), uso en scripts de monitoreo,
journals de otros sistemas, y rendimiento de consultas.

## Prerequisitos

- Entorno del curso levantado (`docker compose up -d`)

---

## Parte 1 — Filtros combinados

### Objetivo

Combinar filtros para consultas precisas.

### Paso 1.1: AND implicito entre filtros

```bash
docker compose exec debian-dev bash -c '
echo "=== AND implicito ==="
echo ""
echo "Multiples filtros se combinan con AND:"
echo ""
echo "journalctl -u sshd -p err -b --since \"2 hours ago\""
echo "  sshd AND err+ AND boot actual AND ultimas 2 horas"
echo ""
echo "--- Con campos explicitos ---"
echo "journalctl _SYSTEMD_UNIT=nginx.service PRIORITY=3 _UID=0"
echo "  nginx AND prioridad err AND ejecutado como root"
echo ""
echo "--- Probar ---"
echo ""
journalctl -p err -b --since "1 hour ago" --no-pager -n 10 2>/dev/null || \
    echo "(sin entradas o journal no disponible)"
echo ""
echo "Cada campo adicional RESTRINGE mas el resultado."
'
```

### Paso 1.2: OR entre unidades

```bash
docker compose exec debian-dev bash -c '
echo "=== OR entre unidades ==="
echo ""
echo "Multiples -u se combinan con OR:"
echo ""
echo "journalctl -u nginx -u php-fpm -u mysql"
echo "  nginx OR php-fpm OR mysql"
echo ""
echo "Util para ver toda la pila de una aplicacion:"
echo "journalctl -u nginx -u php-fpm -p warning --since today"
echo "  Warnings+ de cualquiera de los tres, desde hoy"
echo ""
echo "--- Probar con servicios del sistema ---"
UNITS=""
for u in cron rsyslog sshd systemd-logind; do
    if systemctl is-active "$u" &>/dev/null 2>&1 || \
       journalctl -u "$u" -n 1 --no-pager &>/dev/null 2>&1; then
        UNITS="$UNITS -u $u"
    fi
done
if [[ -n "$UNITS" ]]; then
    echo "Consultando:$UNITS"
    journalctl $UNITS -n 10 --no-pager 2>/dev/null
fi
'
```

### Paso 1.3: Operador + (OR entre grupos)

```bash
docker compose exec debian-dev bash -c '
echo "=== Operador + ==="
echo ""
echo "El + combina grupos de campos con OR:"
echo ""
echo "journalctl _SYSTEMD_UNIT=sshd.service + _COMM=sudo"
echo "  Mensajes de sshd.service OR mensajes del comando sudo"
echo ""
echo "Dentro de cada grupo: AND"
echo "Entre grupos separados por +: OR"
echo ""
echo "journalctl _SYSTEMD_UNIT=nginx.service _PID=1234 + _COMM=curl"
echo "  (nginx AND PID 1234) OR (comando curl)"
echo ""
echo "--- Listar valores disponibles ---"
echo ""
echo "Campos _SYSTEMD_UNIT disponibles:"
journalctl -F _SYSTEMD_UNIT 2>/dev/null | head -10
echo ""
echo "Campos _COMM disponibles:"
journalctl -F _COMM 2>/dev/null | head -10
'
```

### Paso 1.4: Filtros de tiempo avanzados

```bash
docker compose exec debian-dev bash -c '
echo "=== Filtros de tiempo ==="
echo ""
echo "--- Rango exacto ---"
echo "journalctl --since \"2026-03-17 08:00\" --until \"2026-03-17 12:00\""
echo ""
echo "--- Relativos combinados ---"
echo "journalctl --since \"3 hours ago\" --until \"1 hour ago\""
echo "  Ventana de 2 horas que termino hace 1 hora"
echo ""
echo "--- Solo un segundo (correlacionar) ---"
echo "journalctl --since \"2026-03-17 10:30:45\" --until \"2026-03-17 10:30:46\""
echo ""
echo "--- Probar ---"
echo ""
echo "Ultimos 30 minutos:"
journalctl --since "30 min ago" --no-pager -n 5 2>/dev/null || \
    echo "(sin entradas)"
'
```

### Paso 1.5: Cursores

```bash
docker compose exec debian-dev bash -c '
echo "=== Cursores ==="
echo ""
echo "Cada entrada tiene un cursor unico (posicion):"
echo ""
CURSOR=$(journalctl -n 1 -o json --no-pager 2>/dev/null | \
    python3 -c "import json,sys; print(json.load(sys.stdin).get(\"__CURSOR\",\"N/A\"))" 2>/dev/null || echo "N/A")
echo "Cursor de la ultima entrada:"
echo "  $CURSOR" | head -c 80
echo "..."
echo ""
echo "--- Uso ---"
echo "journalctl --after-cursor=\"CURSOR\""
echo "  Entradas DESPUES de esa posicion"
echo ""
echo "journalctl --cursor=\"CURSOR\""
echo "  Desde esa posicion inclusive"
echo ""
echo "--- Caso de uso ---"
echo "  Script de monitoreo que procesa logs incrementalmente:"
echo "  1. Lee cursor de archivo"
echo "  2. Consulta --after-cursor"
echo "  3. Procesa nuevas entradas"
echo "  4. Guarda nuevo cursor"
'
```

---

## Parte 2 — Formatos de salida

### Objetivo

Usar los formatos de salida para diferentes necesidades.

### Paso 2.1: JSON

```bash
docker compose exec debian-dev bash -c '
echo "=== JSON ==="
echo ""
echo "--- json (compacto, una linea por entrada) ---"
journalctl -n 1 -o json --no-pager 2>/dev/null | head -c 200
echo "..."
echo ""
echo ""
echo "--- json-pretty (formateado) ---"
journalctl -n 1 -o json-pretty --no-pager 2>/dev/null | head -15
echo "..."
echo ""
echo "--- Extraer con jq ---"
echo "journalctl -o json --no-pager -n 5 | jq -r \".MESSAGE\""
echo ""
journalctl -o json --no-pager -n 3 2>/dev/null | \
    python3 -c "
import json, sys
for line in sys.stdin:
    try:
        d = json.loads(line)
        print(d.get(\"MESSAGE\", \"(sin mensaje)\")[:80])
    except: pass
" 2>/dev/null || echo "(jq/python3 no disponible)"
'
```

### Paso 2.2: Verbose

```bash
docker compose exec debian-dev bash -c '
echo "=== Verbose ==="
echo ""
echo "Muestra TODOS los campos de cada entrada:"
echo ""
journalctl -n 1 -o verbose --no-pager 2>/dev/null | head -25
echo "..."
echo ""
echo "--- Uso ---"
echo "  Entender que campos tiene un mensaje"
echo "  Descubrir campos para filtrar"
echo "  Depurar problemas con reglas de filtrado"
'
```

### Paso 2.3: Otros formatos

```bash
docker compose exec debian-dev bash -c '
echo "=== Comparar formatos ==="
echo ""
for fmt in short short-iso short-precise short-unix cat; do
    echo "--- $fmt ---"
    journalctl -n 1 -o "$fmt" --no-pager 2>/dev/null || echo "(no disponible)"
    echo ""
done
echo ""
echo "--- Cuando usar cada uno ---"
echo "  short:          lectura humana (default)"
echo "  short-iso:      timestamps parseables por herramientas"
echo "  short-precise:  microsegundos (correlacionar eventos)"
echo "  short-unix:     timestamp numerico (procesamiento)"
echo "  cat:            solo mensaje, sin metadata (scripts)"
echo "  json:           procesamiento automatico"
echo "  verbose:        depuracion (ver todos los campos)"
echo "  export:         copias binarias, systemd-journal-remote"
'
```

---

## Parte 3 — Analisis y scripts

### Objetivo

Usar journalctl para analisis y monitoreo.

### Paso 3.1: Contar errores por servicio

```bash
docker compose exec debian-dev bash -c '
echo "=== Contar errores por servicio ==="
echo ""
echo "Comando:"
echo "journalctl -p err --since today -o json --no-pager |"
echo "  jq -r \"._SYSTEMD_UNIT // .SYSLOG_IDENTIFIER // \\\"unknown\\\"\" |"
echo "  sort | uniq -c | sort -rn | head -10"
echo ""
echo "--- Resultado en este sistema ---"
journalctl -p err --since today -o json --no-pager 2>/dev/null | \
    python3 -c "
import json, sys
from collections import Counter
c = Counter()
for line in sys.stdin:
    try:
        d = json.loads(line)
        unit = d.get(\"_SYSTEMD_UNIT\") or d.get(\"SYSLOG_IDENTIFIER\") or \"unknown\"
        c[unit] += 1
    except: pass
for k, v in c.most_common(10):
    print(f\"  {v:4d} {k}\")
if not c:
    print(\"  (sin errores hoy)\")
" 2>/dev/null || echo "(python3 no disponible)"
'
```

### Paso 3.2: --grep

```bash
docker compose exec debian-dev bash -c '
echo "=== --grep ==="
echo ""
echo "Filtra por regex dentro del campo MESSAGE:"
echo ""
echo "journalctl --grep=\"failed|error|timeout\" -p warning --since today"
echo ""
echo "--- Probar ---"
journalctl --grep="error\|fail" --since "1 hour ago" --no-pager -n 5 2>/dev/null || \
    echo "(sin coincidencias o --grep no soportado)"
echo ""
echo "--- Case insensitive ---"
echo "journalctl --grep=\"connection refused\" --case-sensitive=no"
echo ""
echo "--- Rendimiento ---"
echo "  --grep es LENTO (recorre todos los mensajes)"
echo "  Acotar primero con otros filtros:"
echo ""
echo "  LENTO:  journalctl --grep=\"error\""
echo "  RAPIDO: journalctl -u nginx -p warning --since \"1h ago\" --grep=\"error\""
'
```

### Paso 3.3: Mensajes del kernel

```bash
docker compose exec debian-dev bash -c '
echo "=== Consultas de kernel ==="
echo ""
echo "--- OOM (Out of Memory) ---"
echo "journalctl -k --grep=\"Out of memory|oom-killer|Killed process\""
journalctl -k --grep="Out of memory\|oom-killer\|Killed process" \
    --no-pager -n 3 2>/dev/null || echo "  (sin OOM)"
echo ""
echo "--- Errores de disco ---"
echo "journalctl -k --grep=\"I/O error|EXT4-fs error|XFS.*error\""
journalctl -k --grep="I/O error\|EXT4-fs error\|XFS.*error" \
    --no-pager -n 3 2>/dev/null || echo "  (sin errores de disco)"
echo ""
echo "--- Errores de red ---"
echo "journalctl -k --grep=\"link is not ready|carrier lost\""
journalctl -k --grep="link is not ready\|carrier lost" \
    --no-pager -n 3 2>/dev/null || echo "  (sin errores de red)"
echo ""
echo "--- Todos los errores del kernel en este boot ---"
COUNT=$(journalctl -k -p err -b --no-pager 2>/dev/null | wc -l)
echo "Total errores del kernel: $COUNT"
'
```

### Paso 3.4: Timeline de un incidente

```bash
docker compose exec debian-dev bash -c '
echo "=== Timeline de un incidente ==="
echo ""
echo "Para investigar que paso en un rango de tiempo:"
echo ""
echo "--- Todo en 5 minutos ---"
echo "journalctl --since \"14:00\" --until \"14:05\" -o short-precise"
echo ""
echo "--- Solo errores y warnings ---"
echo "journalctl --since \"14:00\" --until \"14:05\" -p warning"
echo ""
echo "--- Multiples servicios ---"
echo "journalctl --since \"14:00\" --until \"14:05\" -u nginx -u php-fpm"
echo ""
echo "--- Probar con los ultimos 5 minutos ---"
journalctl --since "5 min ago" -o short-precise --no-pager -n 10 2>/dev/null || \
    echo "(sin entradas)"
'
```

### Paso 3.5: Journals de otros sistemas

```bash
docker compose exec debian-dev bash -c '
echo "=== Journals de otros sistemas ==="
echo ""
echo "--- Leer journal de otro sistema montado ---"
echo "journalctl --directory=/mnt/broken-system/var/log/journal/"
echo "  Util para diagnostico de sistemas que no arrancan"
echo ""
echo "--- Leer archivo individual ---"
echo "journalctl --file=/path/to/system.journal"
echo ""
echo "--- Merge de multiples directorios ---"
echo "journalctl --directory=/dir1 --directory=/dir2"
echo ""
echo "--- Journal de este sistema ---"
JDIR=$(ls -d /var/log/journal/*/ 2>/dev/null | head -1)
if [[ -n "$JDIR" ]]; then
    echo "Directorio: $JDIR"
    ls -lh "$JDIR" | head -5
    echo ""
    echo "Tamano total:"
    du -sh "$JDIR" 2>/dev/null
else
    echo "Journal en /run/log/journal/ (volatile)"
    ls -d /run/log/journal/*/ 2>/dev/null | head -1
fi
'
```

### Paso 3.6: Rendimiento de consultas

```bash
docker compose exec debian-dev bash -c '
echo "=== Rendimiento ==="
echo ""
echo "El journal esta indexado:"
echo ""
echo "  _SYSTEMD_UNIT  → indice directo O(1)"
echo "  PRIORITY        → indice directo O(1)"
echo "  tiempo          → busqueda binaria O(log n)"
echo "  --grep          → recorre todos O(n) LENTO"
echo ""
echo "--- Buenas practicas ---"
echo "  1. Acotar por unidad (-u) primero"
echo "  2. Acotar por tiempo (--since)"
echo "  3. Acotar por prioridad (-p)"
echo "  4. Usar --grep SOLO al final"
echo ""
echo "--- Tamano del journal ---"
journalctl --disk-usage 2>/dev/null || echo "(disk-usage no disponible)"
'
```

---

## Limpieza final

No hay recursos que limpiar.

---

## Conceptos reforzados

1. Multiples filtros se combinan con AND. Multiples -u se
   combinan con OR. El operador + combina grupos con OR.

2. Los cursores permiten procesamiento incremental: guardar
   la posicion y retomar con --after-cursor.

3. Formatos clave: json para procesamiento, verbose para
   depuracion, cat para scripts, short-precise para
   correlacion de eventos.

4. --grep filtra por regex en MESSAGE pero es lento. Acotar
   primero con -u, -p y --since para mejor rendimiento.

5. journalctl -k con --grep permite buscar eventos de
   kernel especificos (OOM, errores de disco, red).

6. --directory y --file permiten leer journals de otros
   sistemas, util para diagnostico de sistemas que no
   arrancan.
