# T01 — Consultas avanzadas

> **Fuentes:** `README.md`, `README.max.md`, `LABS.md`, `labs/README.md`
> **Regla aplicada:** 2 (`.max.md` sin `.claude.md` → crear `.claude.md`)

---

## Erratas detectadas

| # | Archivo | Línea(s) | Error | Corrección |
|---|---------|----------|-------|------------|
| 1 | README.max.md | 354 | "O(1) —索引 directo:" — Caracteres chinos "索引" | Debe ser **"índice directo"** en español |
| 2 | README.max.md | 477 | "Script que procesa logs增量mente" — Caracteres chinos "增量" | Debe ser **"incrementalmente"** |
| 3 | README.md | 255, 259, 262 | `--grep="Out of memory\|oom-killer"` — Usa `\|` para alternancia. `journalctl --grep` usa PCRE2/ERE donde `\|` coincide con un pipe literal, no con alternancia | Usar `\|` sin backslash: `--grep="Out of memory\|oom-killer"` → `--grep="Out of memory\|oom-killer"`. `README.max.md:244,247,250` usa correctamente `\|` sin escapar |
| 4 | README.md + README.max.md | 62–63 / 64–65 | `journalctl _SYSTEMD_UNIT=nginx*` presentado como wildcard/globbing. La sintaxis de campo raw (`CAMPO=valor`) usa coincidencia **exacta** | El globbing funciona con `-u nginx*` (que usa `fnmatch()`), pero **no** con `_SYSTEMD_UNIT=nginx*`. Para buscar, usar `journalctl -F _SYSTEMD_UNIT \| grep ^nginx` |

> **Nota sobre errata 3:** En PCRE2, `|` es alternancia y `\|` es un pipe literal. En POSIX ERE, `|` es alternancia. `journalctl --grep` usa PCRE2 en distros modernas (Debian 12+, Fedora 38+, Ubuntu 22.04+). La forma correcta en ambos engines es `|` sin backslash.

---

## Repaso rápido

En C06/S04 se cubrieron los filtros básicos. Aquí profundizamos en técnicas avanzadas para diagnóstico y monitoreo:

```bash
# Filtros básicos (repaso):
journalctl -u nginx              # por unidad
journalctl -p err                # por prioridad
journalctl -b -1                 # por boot anterior
journalctl --since "1 hour ago"  # por tiempo
journalctl -k                    # kernel
```

---

## Combinar filtros complejos

### AND implícito entre filtros

```bash
# Múltiples filtros se combinan con AND:
journalctl -u sshd -p err -b --since "2 hours ago"
#       ↑ sshd  ↑ err+  ↑ boot actual  ↑ últimas 2 horas
#       (todos deben cumplirse)

# Con campos explícitos:
journalctl _SYSTEMD_UNIT=nginx.service PRIORITY=3 _UID=0
# nginx.service AND prioridad err AND ejecutado como root

# Cada campo adicional restringe más el resultado
```

### OR entre unidades (-u)

```bash
# Múltiples -u se combinan con OR:
journalctl -u nginx -u php-fpm -u mysql
# nginx OR php-fpm OR mysql

# Ver toda la pila de una aplicación web:
journalctl -u nginx -u php-fpm -u mysql -p warning --since today
# Warnings+ de cualquiera de los tres, desde hoy
```

### OR con el operador `+`

```bash
# El + combina grupos de campos con OR:
journalctl _SYSTEMD_UNIT=sshd.service + _COMM=sudo
# Mensajes de sshd.service OR mensajes del comando sudo

# Dentro de cada grupo: AND | Entre grupos separados por +: OR
journalctl _SYSTEMD_UNIT=nginx.service _PID=1234 + _COMM=curl
# (nginx.service AND PID 1234) OR (comando curl)
```

### Listar valores de un campo (-F)

```bash
# Listar todos los valores de un campo:
journalctl -F _SYSTEMD_UNIT | grep -i docker
# docker.service
# docker.socket

journalctl -F _COMM | head -20
# Lista de los 20 comandos más recientes que han generado logs

# Para buscar unidades que empiecen con un patrón:
journalctl -u "nginx*"       # ✓ -u soporta globbing (fnmatch)
# NO usar: journalctl _SYSTEMD_UNIT=nginx*  ← coincidencia exacta, no glob
```

---

## Filtros de tiempo avanzados

### Rangos precisos

```bash
# Rango exacto:
journalctl --since "2026-03-17 08:00:00" --until "2026-03-17 12:00:00"

# Con zona horaria explícita:
journalctl --since "2026-03-17 08:00:00 UTC" --until "2026-03-17 12:00:00 UTC"

# Relativos combinados:
journalctl --since "3 hours ago" --until "1 hour ago"
# Ventana de 2 horas que terminó hace 1 hora

# Un segundo específico (correlacionar eventos):
journalctl --since "2026-03-17 10:30:45" --until "2026-03-17 10:30:46"
```

### Por cursor (procesamiento incremental)

Cada entrada del journal tiene un **cursor** único — una posición inmutable que no cambia aunque se roten los archivos:

```bash
# Obtener el cursor de la última entrada:
journalctl -n 1 -o json | jq -r '.__CURSOR'
# s=abc123;i=456;b=def789;m=1234567890;t=60abcdef12345;x=abcdef

# Leer desde un cursor en adelante:
journalctl --after-cursor="s=abc123;i=456;b=def789;..."
# Todas las entradas DESPUÉS de esa posición

journalctl --cursor="s=abc123;i=456;b=def789;..."
# Desde esa posición inclusive
```

Caso de uso: script de monitoreo que procesa logs incrementalmente:

```bash
CURSOR_FILE="/var/lib/mymonitor/cursor"
LAST_CURSOR=$(cat "$CURSOR_FILE" 2>/dev/null)

if [ -n "$LAST_CURSOR" ]; then
    journalctl --after-cursor="$LAST_CURSOR" -o json --no-pager
else
    journalctl --since "5 min ago" -o json --no-pager
fi | while read -r line; do
    echo "$line" | jq -r '.__CURSOR' > "$CURSOR_FILE"
    echo "$line" | jq -r '.MESSAGE'
done
```

---

## Formatos de salida avanzados

### Guía rápida de formatos

| Formato | Uso | Ejemplo de salida |
|---------|-----|-------------------|
| `short` | Default, estilo syslog clásico | `Mar 17 15:30:00 server sshd[1234]: mensaje` |
| `short-precise` | Con microsegundos | `Mar 17 15:30:00.123456 server sshd[1234]: mensaje` |
| `short-iso` | Timestamps ISO 8601 | `2026-03-17T15:30:00+0000 server sshd[1234]: mensaje` |
| `short-unix` | Timestamp Unix epoch | `1710689400.123456 server sshd[1234]: mensaje` |
| `verbose` | Todos los campos | Multi-línea con todos los metadata |
| `json` | Una línea JSON por entry | `{"MESSAGE":"...","PRIORITY":"6",...}` |
| `json-pretty` | JSON formateado | Multi-línea legible |
| `cat` | Solo el mensaje | `GET /index.html 200` (sin timestamp) |
| `export` | Formato binario | Para `systemd-journal-remote` |

### JSON para procesamiento automático

```bash
# JSON compacto (una línea por entrada):
journalctl -u nginx -o json --no-pager -n 5

# JSON formateado (legible):
journalctl -u nginx -o json-pretty -n 1

# Extraer solo el mensaje con jq:
journalctl -u nginx -o json --no-pager -n 5 | jq -r '.MESSAGE'

# Filtrar por contenido del mensaje:
journalctl -u nginx -o json --no-pager | \
    jq -r 'select(.MESSAGE | test("500|502|503")) | .MESSAGE'

# Filtrar por prioridad numérica:
journalctl -o json --no-pager | \
    jq -r 'select(.PRIORITY | tonumber | . <= 3) | "\(.MESSAGE)"'
```

### Verbose — Todos los campos

```bash
journalctl -u sshd -n 1 -o verbose
# Tue 2026-03-17 15:30:00.123456 UTC [s=abc;i=456;...]
#     _BOOT_ID=def789...
#     _HOSTNAME=server
#     PRIORITY=6
#     SYSLOG_FACILITY=4
#     SYSLOG_IDENTIFIER=sshd
#     _UID=0
#     _COMM=sshd
#     _EXE=/usr/sbin/sshd
#     _PID=1234
#     _SYSTEMD_UNIT=sshd.service
#     _SYSTEMD_CGROUP=/system.slice/sshd.service
#     _TRANSPORT=syslog
#     MESSAGE=Accepted publickey for user from 192.168.1.100 port 45678 ssh2

# verbose es ideal para:
# - Descubrir qué campos tiene un mensaje
# - Encontrar campos para filtrar
# - Depurar reglas de filtrado
```

---

## Consultas de análisis

### Contar errores por servicio

```bash
journalctl -p err --since today -o json --no-pager | \
    jq -r '._SYSTEMD_UNIT // .SYSLOG_IDENTIFIER // "unknown"' | \
    sort | uniq -c | sort -rn | head -10
#   45 nginx.service
#   12 php-fpm.service
#    3 sshd.service
#    1 unknown
```

> `//` en jq es el operador "alternative": usa `_SYSTEMD_UNIT`, si no existe usa `SYSLOG_IDENTIFIER`, si tampoco existe usa `"unknown"`.

### Timeline de un incidente

```bash
# TODO lo que pasó en un rango de 5 minutos:
journalctl --since "2026-03-17 14:00" --until "2026-03-17 14:05" \
    --no-pager -o short-precise

# Solo errores y warnings:
journalctl --since "2026-03-17 14:00" --until "2026-03-17 14:05" \
    -p warning --no-pager -o short-precise

# Con contexto de múltiples servicios:
journalctl --since "2026-03-17 14:00" --until "2026-03-17 14:05" \
    -u nginx -u php-fpm -u mysql --no-pager -o short-precise
```

### Correlacionar eventos (antes y después)

```bash
# ¿Qué pasó justo después del deploy?
journalctl -u myapp --since "2026-03-17 14:00" -p err -n 1
# Primer error después de las 14:00

# ¿Qué pasó justo ANTES de que el servicio fallara?
journalctl -u myapp --until "2026-03-17 14:05" -n 20 --no-pager
# Últimas 20 líneas antes de las 14:05
```

### Mensajes del kernel específicos

```bash
# OOM Killer (Out of Memory):
journalctl -k --grep="Out of memory|oom-killer|Killed process"

# Errores de disco:
journalctl -k --grep="I/O error|EXT4-fs error|XFS.*error"

# Errores de red:
journalctl -k --grep="link is not ready|carrier lost|NIC Link"

# Problemas de hardware:
journalctl -k --grep="hardware error|PCIe|ECC"
```

### --grep (filtro en el campo MESSAGE)

```bash
# Filtra por regex en MESSAGE:
journalctl --grep="failed|error|timeout" -p warning --since today
# NOTA: usar | sin backslash (journalctl usa PCRE2/ERE)

# Case insensitive:
journalctl --grep="connection refused" --case-sensitive=no -u nginx

# Contexto (journalctl no lo soporta, usar grep externo):
journalctl -u myapp --no-pager | grep -B2 -A2 "FATAL"
# -B2 = 2 líneas antes, -A2 = 2 líneas después
```

---

## Uso en scripts de monitoreo

### Verificar errores recientes de un servicio

```bash
#!/bin/bash
# Uso: check-errors.sh <servicio> [minutos]
SERVICE="${1:-nginx}"
MINUTES="${2:-5}"

ERRORS=$(journalctl -u "$SERVICE" -p err --since "$MINUTES min ago" \
    -q --no-pager 2>/dev/null | wc -l)

if [ "$ERRORS" -gt 0 ]; then
    echo "ALERTA: $SERVICE tiene $ERRORS errores en los últimos $MINUTES min"
    journalctl -u "$SERVICE" -p err --since "$MINUTES min ago" \
        --no-pager -o short-precise | tail -5
    exit 1
fi
echo "OK: $SERVICE sin errores recientes"
```

### Extraer métricas de logs

```bash
# Contar códigos HTTP de nginx:
journalctl -u nginx -o cat --since "1 hour ago" --no-pager | \
    grep -oP '" \K[0-9]{3}' | sort | uniq -c | sort -rn
#  5432 200
#   123 304
#    45 404
#     2 500

# Contar logins SSH exitosos por usuario:
journalctl -u sshd --grep="Accepted" --since today -o cat --no-pager | \
    grep -oP 'for \K\S+' | sort | uniq -c | sort -rn
#    15 admin
#     3 deploy
```

### Procesamiento en stream (follow + jq)

```bash
# Monitorear errores en tiempo real:
journalctl -f -u myapp -o json | while read -r line; do
    PRIO=$(echo "$line" | jq -r '.PRIORITY')
    if [ "$PRIO" -le 3 ]; then
        MSG=$(echo "$line" | jq -r '.MESSAGE')
        UNIT=$(echo "$line" | jq -r '._SYSTEMD_UNIT')
        echo "$(date): ERROR en $UNIT: $MSG" >> /var/log/alerts.log
    fi
done
```

---

## Acceder a journals de otros sistemas

```bash
# Journal de otro sistema montado (diagnóstico de sistema que no arranca):
journalctl --directory=/mnt/broken-system/var/log/journal/

# Archivo de journal individual:
journalctl --file=/path/to/system.journal

# Combinar múltiples directorios (merge):
journalctl --directory=/var/log/journal/ \
           --directory=/mnt/old/var/log/journal/

# Con filtros aplicados:
journalctl --directory=/mnt/broken/var/log/journal/ -p err -b -1
```

---

## Rendimiento de consultas

```
┌─────────────────────────────────────────────────────────────────────┐
│                 ÍNDICES DEL JOURNAL                                 │
├─────────────────────────────────────────────────────────────────────┤
│                                                                     │
│  O(1) — índice directo:                                            │
│    _SYSTEMD_UNIT, _UID, _PID, PRIORITY, SYSLOG_IDENTIFIER          │
│    → Filtrar por estos campos es MUY RÁPIDO                        │
│                                                                     │
│  O(log n) — búsqueda binaria:                                      │
│    --since, --until (rangos de tiempo)                              │
│    → Rápido gracias al índice temporal                              │
│                                                                     │
│  O(n) — recorrido completo:                                        │
│    --grep (recorre todos los mensajes)                              │
│    → LENTO en journals grandes — siempre acotar antes              │
│                                                                     │
└─────────────────────────────────────────────────────────────────────┘
```

```bash
# MAL (recorre todo el journal):
journalctl --grep="error" --no-pager

# BIEN (acota primero, luego grep):
journalctl -u nginx -p warning --since "1 hour ago" --grep="error" --no-pager
```

Patrón: acotar por unidad (`-u`) → por tiempo (`--since`) → por prioridad (`-p`) → luego `--grep`.

---

## Quick reference

```
FILTROS COMBINADOS:
  -u x -u y -u z             → OR entre unidades
  CAMPO1=v1 CAMPO2=v2        → AND entre campos
  CAMPO1=v1 + CAMPO2=v2      → OR entre grupos
  -u "nginx*"                → globbing (solo con -u, no con CAMPO=)

TIEMPO:
  --since "1 hour ago"       → relativo
  --since "2026-03-17"       → absoluto
  --since X --until Y        → rango
  --after-cursor "s=..."     → incremental (retomar)

FORMATOS:
  -o short, short-precise, short-iso, short-unix
  -o verbose                 → todos los campos
  -o json, json-pretty       → procesamiento automático
  -o cat                     → solo mensaje

GREP:
  --grep="pattern"           → regex en MESSAGE (PCRE2: usar | no \|)
  --case-sensitive=no        → case insensitive
  Acotar con -u/-p/--since antes de --grep

OTROS SISTEMAS:
  --directory=/mnt/var/log/journal/
  --file=/path/to/system.journal
```

---

## Labs

### Parte 1 — Filtros combinados

**Paso 1.1: AND implícito**

Múltiples filtros se combinan con AND. Cada campo adicional restringe más el resultado:

```bash
journalctl -u sshd -p err -b --since "2 hours ago"
# sshd AND err+ AND boot actual AND últimas 2 horas

journalctl _SYSTEMD_UNIT=nginx.service PRIORITY=3 _UID=0
# nginx AND prioridad err AND ejecutado como root
```

**Paso 1.2: OR entre unidades**

Múltiples `-u` se combinan con OR:

```bash
journalctl -u nginx -u php-fpm -u mysql
# nginx OR php-fpm OR mysql

journalctl -u nginx -u php-fpm -u mysql -p warning --since today
# Warnings+ de cualquiera de los tres (-u es OR, -p es AND)
```

**Paso 1.3: Operador +**

El `+` combina grupos de campos con OR:

```bash
journalctl _SYSTEMD_UNIT=sshd.service + _COMM=sudo
# sshd.service OR comando sudo

journalctl _SYSTEMD_UNIT=nginx.service _PID=1234 + _COMM=curl
# (nginx AND PID 1234) OR (comando curl)
```

Listar valores disponibles con `-F`:

```bash
journalctl -F _SYSTEMD_UNIT | head -10   # unidades con logs
journalctl -F _COMM | head -10           # comandos con logs
```

**Paso 1.4: Filtros de tiempo avanzados**

```bash
# Rango exacto:
journalctl --since "2026-03-17 08:00" --until "2026-03-17 12:00"

# Relativos combinados:
journalctl --since "3 hours ago" --until "1 hour ago"

# Un segundo específico:
journalctl --since "2026-03-17 10:30:45" --until "2026-03-17 10:30:46"
```

**Paso 1.5: Cursores**

Cada entrada tiene un cursor único (posición inmutable):

```bash
# Obtener cursor:
journalctl -n 1 -o json | jq -r '.__CURSOR'

# Leer desde un cursor:
journalctl --after-cursor="s=abc123;..."   # después de esa posición
journalctl --cursor="s=abc123;..."         # desde esa posición inclusive
```

### Parte 2 — Formatos de salida

**Paso 2.1: JSON**

```bash
# Compacto (una línea por entry):
journalctl -n 1 -o json --no-pager

# Formateado (legible):
journalctl -n 1 -o json-pretty --no-pager

# Extraer campos con jq:
journalctl -o json --no-pager -n 5 | jq -r '.MESSAGE'
```

**Paso 2.2: Verbose**

```bash
journalctl -n 1 -o verbose --no-pager
# Muestra TODOS los campos: _BOOT_ID, _HOSTNAME, PRIORITY,
# SYSLOG_IDENTIFIER, _UID, _COMM, _EXE, _PID, _SYSTEMD_UNIT,
# _TRANSPORT, MESSAGE, etc.
```

Ideal para descubrir qué campos tiene un mensaje y encontrar campos para filtrar.

**Paso 2.3: Comparar formatos**

```bash
for fmt in short short-precise short-iso short-unix cat json; do
    echo "=== $fmt ==="
    journalctl -n 1 -o "$fmt" --no-pager 2>/dev/null | head -5
    echo
done
```

Cuándo usar cada uno:
- `short` — lectura humana (default)
- `short-precise` — correlacionar eventos (microsegundos)
- `short-iso` — timestamps parseables por herramientas
- `cat` — solo mensaje, sin metadata (alimentar scripts)
- `json` — procesamiento automático con jq
- `verbose` — depuración (ver todos los campos)

### Parte 3 — Análisis y scripts

**Paso 3.1: Contar errores por servicio**

```bash
journalctl -p err --since today -o json --no-pager | \
    jq -r '._SYSTEMD_UNIT // .SYSLOG_IDENTIFIER // "unknown"' | \
    sort | uniq -c | sort -rn | head -10
```

**Paso 3.2: --grep**

Filtra por regex en el campo MESSAGE. Usa PCRE2/ERE — `|` es alternancia directamente:

```bash
journalctl --grep="failed|error|timeout" -p warning --since today

# Case insensitive:
journalctl --grep="connection refused" --case-sensitive=no -u nginx
```

`--grep` es O(n) — siempre acotar con `-u`, `-p`, `--since` primero.

**Paso 3.3: Mensajes del kernel**

```bash
journalctl -k --grep="Out of memory|oom-killer|Killed process"  # OOM
journalctl -k --grep="I/O error|EXT4-fs error|XFS.*error"       # disco
journalctl -k --grep="link is not ready|carrier lost"            # red
```

**Paso 3.4: Timeline de un incidente**

```bash
# Todo en un rango:
journalctl --since "14:00" --until "14:05" -o short-precise --no-pager

# Solo errores/warnings:
journalctl --since "14:00" --until "14:05" -p warning --no-pager

# Múltiples servicios:
journalctl --since "14:00" --until "14:05" -u nginx -u php-fpm --no-pager
```

**Paso 3.5: Journals de otros sistemas**

```bash
journalctl --directory=/mnt/broken-system/var/log/journal/  # sistema montado
journalctl --file=/path/to/system.journal                    # archivo individual
```

Ver tamaño del journal: `journalctl --disk-usage`

**Paso 3.6: Rendimiento**

Campos indexados (`_SYSTEMD_UNIT`, `PRIORITY`) → O(1). Tiempo → O(log n). `--grep` → O(n). Siempre acotar antes de `--grep`.

---

## Ejercicios

### Ejercicio 1 — AND vs OR

¿Qué devuelve cada consulta?

```bash
# Consulta A:
journalctl -u sshd -u nginx -p err --since today

# Consulta B:
journalctl _SYSTEMD_UNIT=sshd.service PRIORITY=3

# Consulta C:
journalctl _SYSTEMD_UNIT=sshd.service + _SYSTEMD_UNIT=nginx.service
```

<details><summary>Predicción</summary>

**Consulta A:** Mensajes de `sshd` **OR** `nginx` (múltiples `-u` = OR), **AND** prioridad `err` o superior, **AND** desde hoy. Las `-u` son OR entre sí, pero `-p` y `--since` se aplican como AND al resultado.

**Consulta B:** Mensajes de `sshd.service` **AND** prioridad exactamente `3` (err). Solo err, no crit ni alert ni emerg (PRIORITY=3 es coincidencia exacta, no "3 o superior" como `-p err`).

**Consulta C:** Mensajes de `sshd.service` **OR** `nginx.service`. El `+` separa dos grupos, cada grupo con un solo campo. Equivale a `journalctl -u sshd -u nginx`.

</details>

### Ejercicio 2 — Operador + complejo

Interpreta esta consulta:

```bash
journalctl _SYSTEMD_UNIT=nginx.service _PID=5678 + _COMM=sudo _UID=0
```

<details><summary>Predicción</summary>

Dos grupos separados por `+`:
- **Grupo 1:** `_SYSTEMD_UNIT=nginx.service` AND `_PID=5678` — mensajes de nginx con PID 5678
- **Grupo 2:** `_COMM=sudo` AND `_UID=0` — mensajes del comando sudo ejecutado como root

Resultado: **(nginx con PID 5678) OR (sudo como root)**

Dentro de cada grupo, los campos son AND. Entre grupos, `+` es OR.

</details>

### Ejercicio 3 — Filtros de tiempo

¿Qué rango de tiempo cubre cada consulta?

```bash
# Consulta A:
journalctl --since "3 hours ago" --until "1 hour ago"

# Consulta B:
journalctl --since "2026-03-25 14:00" --until "2026-03-25 14:00:01"

# Consulta C:
journalctl -b --since "10 min ago"
```

<details><summary>Predicción</summary>

**Consulta A:** Ventana de **2 horas** que terminó hace 1 hora. Si son las 15:00, cubre de 12:00 a 14:00.

**Consulta B:** Exactamente **1 segundo** — de las 14:00:00 a las 14:00:01 del 25 de marzo. Útil para correlacionar un evento puntual.

**Consulta C:** Los últimos **10 minutos** del **boot actual**. `-b` restringe al boot actual, `--since "10 min ago"` restringe al tiempo. Ambos filtros son AND.

</details>

### Ejercicio 4 — Formatos de salida

Para cada necesidad, ¿qué formato usarías?

1. Alimentar un script que solo necesita el texto del mensaje
2. Descubrir qué campos tiene un mensaje para crear filtros
3. Correlacionar eventos entre dos servicios con precisión de microsegundos
4. Enviar logs a un pipeline de procesamiento (jq, Python)
5. Lectura rápida de logs por un humano

<details><summary>Predicción</summary>

1. **`-o cat`** — Solo el mensaje, sin timestamp, hostname ni metadata
2. **`-o verbose`** — Muestra todos los campos disponibles (PRIORITY, _UID, _COMM, _EXE, etc.)
3. **`-o short-precise`** — Timestamps con microsegundos para ordenar eventos con precisión
4. **`-o json`** — Una línea JSON por entrada, procesable con `jq` o `json.loads()` en Python
5. **`-o short`** — El formato default, estilo syslog clásico, compacto y legible

</details>

### Ejercicio 5 — Análisis con jq

Escribe un pipeline que extraiga del journal:
1. Solo mensajes de `sshd`
2. Con prioridad `warning` o superior
3. Desde hoy
4. Muestre solo el timestamp y el mensaje, separados por tab

<details><summary>Predicción</summary>

```bash
journalctl -u sshd -p warning --since today -o json --no-pager | \
    jq -r '"\(.__REALTIME_TIMESTAMP | tonumber / 1000000 | strftime("%H:%M:%S"))\t\(.MESSAGE)"'
```

Alternativa más simple:

```bash
journalctl -u sshd -p warning --since today -o json --no-pager | \
    jq -r '[.SYSLOG_TIMESTAMP, .MESSAGE] | @tsv'
```

O sin jq, usando `-o short-precise` y `awk`:

```bash
journalctl -u sshd -p warning --since today -o short-precise --no-pager | \
    awk '{print $1" "$2"\t"substr($0, index($0,$5))}'
```

La ventaja de `-o json` + `jq` es el acceso estructurado a campos individuales sin depender del formato de texto.

</details>

### Ejercicio 6 — --grep y rendimiento

¿Cuál de estas consultas es más eficiente y por qué?

```bash
# Consulta A:
journalctl --grep="connection refused" --no-pager

# Consulta B:
journalctl -u nginx --since "1 hour ago" --grep="connection refused" --no-pager

# Consulta C:
journalctl -u nginx --since "1 hour ago" --no-pager | grep "connection refused"
```

<details><summary>Predicción</summary>

**Consulta A:** La **más lenta**. `--grep` es O(n) y sin otros filtros recorre TODO el journal.

**Consulta B:** **Rápida**. Primero acota por unidad (O(1), indexado) y tiempo (O(log n), indexado), luego aplica `--grep` solo al subconjunto filtrado.

**Consulta C:** Similar rendimiento a B. journalctl filtra por unidad y tiempo (rápido), luego el `grep` externo filtra el texto. La diferencia con B es que `--grep` de journalctl filtra ANTES de formatear la salida, mientras que `grep` externo filtra DESPUÉS.

**B es la mejor opción:** todo el filtrado ocurre dentro de journalctl, evitando formatear y transmitir entradas que serán descartadas.

</details>

### Ejercicio 7 — Cursor para monitoreo

¿Qué problema tiene este script y cómo lo arreglarías?

```bash
#!/bin/bash
while true; do
    journalctl -p err --since "5 min ago" -o json --no-pager | \
        jq -r '.MESSAGE' | while read -r msg; do
            echo "ERROR: $msg" >> /var/log/monitor.log
        done
    sleep 300
done
```

<details><summary>Predicción</summary>

**Problema:** Cada iteración consulta los últimos 5 minutos. Si el script tarda más de 5 minutos en una iteración, pierde mensajes del intervalo no cubierto. Si tarda menos, procesa mensajes duplicados (los que caen en el solape entre iteraciones).

**Solución:** Usar cursores para procesamiento incremental:

```bash
#!/bin/bash
CURSOR_FILE="/var/lib/monitor/cursor"

while true; do
    LAST_CURSOR=$(cat "$CURSOR_FILE" 2>/dev/null)

    if [ -n "$LAST_CURSOR" ]; then
        CMD="journalctl --after-cursor=$LAST_CURSOR -p err -o json --no-pager"
    else
        CMD="journalctl --since '5 min ago' -p err -o json --no-pager"
    fi

    $CMD | while read -r line; do
        echo "$line" | jq -r '.__CURSOR' > "$CURSOR_FILE"
        MSG=$(echo "$line" | jq -r '.MESSAGE')
        echo "ERROR: $MSG" >> /var/log/monitor.log
    done
    sleep 300
done
```

Con `--after-cursor`, cada iteración procesa exactamente las entradas nuevas desde la última posición. No hay duplicados ni huecos.

</details>

### Ejercicio 8 — Diagnóstico de incidente

Un servicio `myapp` falló a las 14:03. Escribe los comandos para:

1. Ver las últimas 30 líneas de myapp antes del fallo
2. Ver el primer error de myapp después de las 14:00
3. Ver TODO lo que pasó en el sistema entre 14:00 y 14:05
4. Ver solo errores del kernel en ese rango (OOM, disco)

<details><summary>Predicción</summary>

```bash
# 1. Últimas 30 líneas antes del fallo:
journalctl -u myapp --until "2026-03-25 14:03" -n 30 --no-pager

# 2. Primer error después de las 14:00:
journalctl -u myapp --since "2026-03-25 14:00" -p err -n 1

# 3. Todo el sistema entre 14:00 y 14:05:
journalctl --since "2026-03-25 14:00" --until "2026-03-25 14:05" \
    --no-pager -o short-precise

# 4. Errores del kernel (OOM, disco):
journalctl -k --since "2026-03-25 14:00" --until "2026-03-25 14:05" \
    --grep="Out of memory|oom-killer|Killed process|I/O error" --no-pager
```

Orden de investigación: primero los logs del servicio (1, 2), luego el contexto del sistema (3), luego causas raíz en el kernel (4). Si el kernel muestra OOM, el servicio fue matado por falta de memoria.

</details>

### Ejercicio 9 — Journals remotos

Un servidor no arranca. Montas su disco en `/mnt/broken`. ¿Cómo consultas sus logs?

<details><summary>Predicción</summary>

```bash
# 1. Encontrar el directorio del journal:
ls /mnt/broken/var/log/journal/
# Debería mostrar un directorio con el machine-id

# 2. Ver los últimos logs del sistema:
journalctl --directory=/mnt/broken/var/log/journal/ -n 50 --no-pager

# 3. Ver el último boot (que falló):
journalctl --directory=/mnt/broken/var/log/journal/ -b -0 --no-pager

# 4. Ver errores del kernel en el último boot:
journalctl --directory=/mnt/broken/var/log/journal/ -b -0 -k -p err --no-pager

# 5. Listar boots disponibles:
journalctl --directory=/mnt/broken/var/log/journal/ --list-boots

# 6. Comparar el boot que falló (-b 0) con el anterior (-b -1):
journalctl --directory=/mnt/broken/var/log/journal/ -b -1 -p err --no-pager
```

`--directory` aplica todos los filtros normales de journalctl al journal remoto. Ideal para diagnóstico post-mortem.

</details>

### Ejercicio 10 — Script de monitoreo completo

Escribe un script que:
1. Cuente errores de los últimos 5 minutos para cada servicio
2. Si algún servicio tiene más de 10 errores, imprima una alerta con los 3 más recientes
3. Salga con código 1 si hay alertas, 0 si no

<details><summary>Predicción</summary>

```bash
#!/bin/bash
# monitor-errors.sh
THRESHOLD=10
MINUTES=5
ALERT=0

# Obtener servicios con errores en los últimos N minutos:
declare -A ERROR_COUNT

while IFS= read -r unit; do
    count=$(journalctl -u "$unit" -p err --since "$MINUTES min ago" \
        -q --no-pager 2>/dev/null | wc -l)
    if [ "$count" -gt 0 ]; then
        ERROR_COUNT["$unit"]=$count
    fi
done < <(journalctl -p err --since "$MINUTES min ago" -o json --no-pager 2>/dev/null | \
    jq -r '._SYSTEMD_UNIT // empty' | sort -u)

# Evaluar y alertar:
for unit in "${!ERROR_COUNT[@]}"; do
    count=${ERROR_COUNT[$unit]}
    if [ "$count" -gt "$THRESHOLD" ]; then
        ALERT=1
        echo "ALERTA: $unit tiene $count errores en los últimos $MINUTES min"
        journalctl -u "$unit" -p err --since "$MINUTES min ago" \
            --no-pager -o short-precise -n 3
        echo "---"
    fi
done

if [ "$ALERT" -eq 0 ]; then
    echo "OK: Sin alertas (threshold=$THRESHOLD errores en $MINUTES min)"
fi
exit $ALERT
```

Puntos clave:
- Primero obtiene la lista de servicios con errores (una sola consulta al journal)
- Luego cuenta errores por servicio individualmente
- `-q` suprime mensajes informativos de journalctl
- Umbral configurable (`THRESHOLD`)
- Código de salida útil para integración con cron o monitoreo externo

</details>
