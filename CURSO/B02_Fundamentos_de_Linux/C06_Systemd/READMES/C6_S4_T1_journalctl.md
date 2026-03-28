# T01 — journalctl

## Errata sobre los materiales originales

| # | Archivo | Línea | Error | Corrección |
|---|---------|-------|-------|------------|
| 1 | max.md | 83 | Texto en ruso "Corruption обнаружен" en tabla de prioridades | Reemplazado por "Corrupción detectada" |
| 2 | max.md | 319-326 | MESSAGE_IDs fabricados (8d45620c..., b07f2cae..., 8d5e3f1a...) presentados como estándar de systemd | Corregido con IDs reales de systemd (documentados en `systemd.journal-fields(7)`) |
| 3 | max.md | 392, 407-410 | `journalctl -F SystemMaxUse` — `-F` lista valores de campos del journal, no directivas de `journald.conf` | Corregido: usar `systemd-analyze cat-config systemd/journald.conf` o leer `/etc/systemd/journald.conf` |
| 4 | max.md | 462 | `journalctl -o verbose | grep "^#"` para contar campos — las líneas en verbose empiezan con espacios, no con `#` | Corregido: `grep "^\s"` o contar con `wc -l` |

---

## Qué es el journal

**systemd-journald** es el servicio de logging de systemd. A diferencia de
syslog tradicional (archivos de texto plano), el journal almacena los logs en
un formato **binario estructurado** con campos indexados:

| | syslog tradicional | systemd journal |
|---|---|---|
| Formato | Texto plano | Binario indexado por campos |
| Ubicación | `/var/log/syslog`, `auth.log` | `/var/log/journal/` (persistente) |
| Consulta | `grep` + `awk` | `journalctl` con filtros |
| Estructura | Líneas libres | Campos clave=valor |
| Correlación | Manual | Automática (PID, UID, cgroup) |

### Qué captura el journal

```bash
# Ver todos los logs (pager interactivo con less):
journalctl

# El journal unifica logs de múltiples fuentes:
# stdout/stderr     → servicios gestionados por systemd
# syslog            → aplicaciones que usan syslog()
# kernel (dmesg)    → mensajes del kernel
# systemd interno   → arranque/parada de unidades
# /dev/kmsg         → logging del kernel en tiempo real
```

---

## Filtros básicos

### Por unidad (servicio)

```bash
# Logs de un servicio específico:
journalctl -u nginx.service
journalctl -u nginx              # .service es implícito

# Múltiples unidades (OR entre ellas):
journalctl -u sshd -u nginx

# Logs del kernel (equivale a dmesg, pero con timestamps y persistencia):
journalctl -k
journalctl _TRANSPORT=kernel     # forma larga equivalente
```

### Por tiempo

```bash
# Desde una fecha/hora:
journalctl --since "2024-03-17 10:00:00"
journalctl --since "1 hour ago"
journalctl --since "today"
journalctl --since "yesterday"

# Hasta una fecha/hora:
journalctl --until "2024-03-17 12:00:00"
journalctl --until "30 min ago"

# Rango:
journalctl --since "2024-03-17 08:00" --until "2024-03-17 12:00"

# Formatos de tiempo aceptados:
# "YYYY-MM-DD HH:MM:SS"
# "today", "yesterday"
# "N hours ago", "N min ago", "N days ago"
# "HH:MM" (hoy a esa hora)
```

### Por prioridad

syslog define **8 niveles de prioridad** (menor número = mayor severidad):

| Nivel | Nombre | Descripción | Uso típico |
|-------|--------|-------------|------------|
| 0 | `emerg` | Sistema inutilizable | Kernel panic |
| 1 | `alert` | Acción inmediata necesaria | Corrupción detectada |
| 2 | `crit` | Condición crítica | Error de hardware |
| 3 | `err` | Error de operación | "Connection refused" |
| 4 | `warning` | Situación anómala | "Port already in use" |
| 5 | `notice` | Normal pero significativo | Login exitoso de root |
| 6 | `info` | Informacional | Servicio iniciado/parado |
| 7 | `debug` | Depuración | Logs de desarrollo |

```bash
# Filtrar por prioridad (incluye el nivel indicado Y los más severos):
journalctl -p err              # err + crit + alert + emerg (3,2,1,0)
journalctl -p warning          # warning + err + crit + alert + emerg
journalctl -p 3                # equivale a -p err

# Rango de prioridades específicas:
journalctl -p err..warning     # solo err y warning (3 y 4, no 2 ni 0)
```

### Por boot

```bash
# Boot actual:
journalctl -b
journalctl -b 0                # equivalente

# Boot anterior:
journalctl -b -1

# Dos boots atrás:
journalctl -b -2

# Listar todos los boots registrados:
journalctl --list-boots
# -3 abc123... Mon 2024-03-14 08:00 — Mon 2024-03-14 23:59
# -2 def456... Tue 2024-03-15 08:00 — Tue 2024-03-15 23:59
# -1 ghi789... Wed 2024-03-16 08:00 — Wed 2024-03-16 23:59
#  0 jkl012... Thu 2024-03-17 08:00 — *currently running*

# Boot específico por ID:
journalctl -b abc123def456
```

### Por proceso (PID, UID, GID, ejecutable)

```bash
# Por PID:
journalctl _PID=1234

# Por UID del proceso:
journalctl _UID=1000

# Por GID:
journalctl _GID=1000

# Por nombre del comando:
journalctl _COMM=sshd

# Por ruta del ejecutable:
journalctl _EXE=/usr/sbin/sshd
```

---

## Combinación de filtros

Los filtros se combinan con **AND implícito**:

```bash
# nginx + errores + boot actual:
journalctl -u nginx -p err -b

# kernel + warnings y superiores + última hora:
journalctl -k -p warning --since "1 hour ago"

# sshd + boot anterior:
journalctl -u sshd -b -1

# Errores de cualquier servicio en la última hora:
journalctl -p err --since "1 hour ago"
```

**Excepción**: múltiples `-u` son **OR** entre sí:

```bash
# Logs de nginx O de sshd:
journalctl -u nginx -u sshd

# OR con campos diferentes — operador +:
journalctl _COMM=nginx + _COMM=sshd
```

---

## Opciones de salida

### Seguimiento en vivo (follow)

```bash
# Como tail -f para el journal:
journalctl -f
journalctl -f -u nginx          # follow solo nginx
journalctl -f -p err            # follow solo errores
journalctl -f -u nginx -u php-fpm -p warning   # combinar
```

### Control de volumen

```bash
# Últimas N líneas (como tail -n):
journalctl -n 50
journalctl -n 20 -u nginx

# Más recientes primero (orden inverso):
journalctl -r
journalctl -r -n 10 -u sshd

# Sin pager (para scripts o piping):
journalctl --no-pager -n 100

# Sin hostname en la salida:
journalctl --no-hostname -n 10

# Quiet — no imprime "-- No entries --" cuando no hay resultados:
journalctl -u nonexistent -q
```

### Formatos de salida (-o)

```bash
# short (default) — estilo syslog clásico:
journalctl -o short
# Mar 17 10:30:00 server sshd[1234]: Accepted publickey for user...

# short-precise — con microsegundos:
journalctl -o short-precise
# Mar 17 10:30:00.123456 server sshd[1234]: ...

# short-iso — timestamp ISO 8601:
journalctl -o short-iso
# 2024-03-17T10:30:00+0000 server sshd[1234]: ...

# short-full — fecha completa:
journalctl -o short-full
# Thu 2024-03-17 10:30:00.123456 server sshd[1234]: ...

# verbose — TODOS los campos del entry (ideal para debugging):
journalctl -o verbose -n 1
#     _BOOT_ID=def789...
#     _MACHINE_ID=abc123...
#     _HOSTNAME=server
#     PRIORITY=6
#     _UID=0
#     _COMM=sshd
#     _EXE=/usr/sbin/sshd
#     _PID=1234
#     MESSAGE=Accepted publickey for user
#     _TRANSPORT=syslog
#     SYSLOG_IDENTIFIER=sshd

# json — un objeto JSON por línea (ideal para procesamiento):
journalctl -o json -n 1

# json-pretty — JSON formateado (legible):
journalctl -o json-pretty -n 1

# cat — SOLO el mensaje, sin metadata:
journalctl -o cat -u nginx -n 5
```

---

## Filtros avanzados con campos

Cada entrada del journal es un conjunto de **campos clave-valor**. Se puede
filtrar por cualquier campo:

```bash
# Campos más comunes:
# _PID              — PID del proceso
# _UID              — UID del usuario
# _GID              — GID del grupo
# _COMM             — nombre del comando
# _EXE              — ruta absoluta del ejecutable
# _CMDLINE          — línea de comandos completa
# _HOSTNAME         — hostname del sistema
# _TRANSPORT        — origen: journal, syslog, kernel, stdout, driver
# PRIORITY          — nivel 0-7
# SYSLOG_FACILITY   — facility de syslog (auth, daemon, local0-7...)
# SYSLOG_IDENTIFIER — nombre del programa (como aparece en syslog)
# _SYSTEMD_UNIT     — unidad de systemd que gobierna el proceso
# _SYSTEMD_CGROUP   — cgroup del proceso
# MESSAGE           — el mensaje de log
# MESSAGE_ID        — ID único del mensaje (para mensajes estructurados)
```

```bash
# Filtrar por campo:
journalctl _TRANSPORT=kernel
journalctl SYSLOG_IDENTIFIER=sudo
journalctl _SYSTEMD_UNIT=docker.service

# Combinar campos (AND):
journalctl _COMM=sshd _HOSTNAME=server1

# Ver todos los valores únicos de un campo:
journalctl -F _COMM              # ¿qué comandos han generado logs?
journalctl -F PRIORITY           # ¿qué prioridades existen?
journalctl -F _SYSTEMD_UNIT      # ¿qué unidades han logueado?
```

### MESSAGE_ID — Mensajes estructurados

Algunos mensajes de systemd tienen un `MESSAGE_ID` estándar que permite
identificarlos sin depender del texto del mensaje:

```bash
# Buscar mensajes por MESSAGE_ID:
journalctl MESSAGE_ID=39f53479d3a045ac8e11786248231fbf

# MESSAGE_IDs reales de systemd (documentados en systemd.journal-fields(7)):
# 7d4958e842da4a758f6c1cdc7b36dcc5 → unit started
# de5b426a63be47a7b6ac3eaac82e2f6f → unit stopped
# be02cf6855d2428ba40df7e9d022f03d → unit reloading
# d34d037fff1847e6ae669a370e694725 → unit failed
# 39f53479d3a045ac8e11786248231fbf → unit starting

# Útil para buscar eventos sin depender del texto (que puede cambiar
# entre versiones de systemd o locale)
```

---

## Uso en scripts

```bash
# Verificar si un servicio tiene errores recientes:
if journalctl -u myapp -p err --since "5 min ago" -q --no-pager | grep -q .; then
    echo "myapp tiene errores recientes" >&2
fi
# -q = quiet (no imprime "-- No entries --" cuando no hay resultados)

# Extraer mensajes de error en JSON y procesarlos con jq:
journalctl -u nginx -o json --since "1 hour ago" --no-pager | \
    jq -r 'select(.PRIORITY == "3") | .MESSAGE'

# Contar errores por servicio (boot actual):
journalctl -p err -b -o json --no-pager | \
    jq -r '._SYSTEMD_UNIT // "unknown"' | sort | uniq -c | sort -rn

# Monitoreo en tiempo real con alertas:
journalctl -f -u myapp -o json | while read -r line; do
    PRIORITY=$(echo "$line" | jq -r '.PRIORITY')
    if [[ "$PRIORITY" -le 3 ]]; then
        MSG=$(echo "$line" | jq -r '.MESSAGE')
        echo "ALERTA: $MSG" >&2
    fi
done
```

---

## Espacio en disco

```bash
# Ver cuánto espacio usa el journal:
journalctl --disk-usage
# Archived and active journals take up 256.0M in the file system.

# Limpiar por tamaño (mantener ≤ 100MB):
sudo journalctl --vacuum-size=100M

# Limpiar por tiempo (solo últimos 7 días):
sudo journalctl --vacuum-time=7d

# Limpiar por número de archivos:
sudo journalctl --vacuum-files=5

# Rotar + limpiar (cerrar journal activo antes de limpiar):
sudo journalctl --rotate && sudo journalctl --vacuum-size=100M

# Verificar integridad (detectar archivos corruptos):
journalctl --verify
```

### Configuración de retención (journald.conf)

```bash
# Ver la configuración efectiva:
systemd-analyze cat-config systemd/journald.conf 2>/dev/null || \
    cat /etc/systemd/journald.conf

# Directivas principales en [Journal]:
# SystemMaxUse=500M        — máximo espacio en disco
# SystemMaxFileSize=50M    — máximo tamaño por archivo individual
# MaxRetentionSec=30day    — máxima retención temporal
# Storage=persistent       — persistent|volatile|auto|none
```

---

## Quick reference

| Objetivo | Comando |
|----------|---------|
| Ver todo | `journalctl --no-pager` |
| Errores del boot actual | `journalctl -b -p err --no-pager` |
| Seguir en vivo | `journalctl -f` |
| Últimas 50 líneas | `journalctl -n 50 --no-pager` |
| Logs de nginx (errores) | `journalctl -u nginx -p err` |
| Boot anterior | `journalctl -b -1 --no-pager` |
| Rango de fechas | `journalctl --since "2024-03-01" --until "2024-03-02"` |
| Solo mensajes | `journalctl -u nginx -o cat` |
| JSON para procesar | `journalctl -u nginx -o json --no-pager` |
| Espacio usado | `journalctl --disk-usage` |
| Limpiar >7 días | `sudo journalctl --vacuum-time=7d` |

---

## Ejercicios

### Ejercicio 1 — Filtros básicos por unidad y prioridad

```bash
# 1. Ver errores (err+) del boot actual:
journalctl -b -p err --no-pager

# 2. Ver las últimas 20 líneas de sshd:
journalctl -u sshd -n 20 --no-pager

# 3. Ver logs del kernel de la última hora:
journalctl -k --since "1 hour ago" --no-pager
```

<details><summary>Predicción</summary>

- Los errores del boot probablemente serán pocos (0-10 en un sistema
  saludable) o muchos si hay servicios mal configurados
- `-p err` incluye err (3), crit (2), alert (1) y emerg (0) — no solo err
- Los logs de sshd mostrarán conexiones (Accepted/Rejected) y eventos
  de sesión
- Los logs del kernel mostrarán eventos de hardware, drivers, y
  posiblemente mensajes de firewall

</details>

### Ejercicio 2 — Filtros por tiempo y boot

```bash
# 1. ¿Cuántos boots tiene registrados el journal?
journalctl --list-boots --no-pager

# 2. Ver logs del boot anterior (si existe):
journalctl -b -1 -n 20 --no-pager 2>/dev/null || echo "Sin boot anterior"

# 3. Logs de hoy, solo warnings:
journalctl --since today -p warning --no-pager | tail -10
```

<details><summary>Predicción</summary>

- La cantidad de boots depende de si el journal es persistente (en
  `/var/log/journal/`) o volátil (en `/run/log/journal/` — solo boot
  actual)
- Si es volátil, `--list-boots` mostrará solo el boot 0 y `-b -1` fallará
- Si es persistente, mostrará todos los boots desde que se habilitó la
  persistencia
- En un contenedor Docker, probablemente solo habrá un boot

</details>

### Ejercicio 3 — Formatos de salida

```bash
# Comparar la misma entrada en 4 formatos:
echo "=== short ===" && journalctl -n 1 -o short --no-pager
echo "=== verbose ===" && journalctl -n 1 -o verbose --no-pager
echo "=== json-pretty ===" && journalctl -n 1 -o json-pretty --no-pager
echo "=== cat ===" && journalctl -n 1 -o cat --no-pager
```

<details><summary>Predicción</summary>

- `short`: una línea con timestamp, hostname, proceso y mensaje
- `verbose`: decenas de campos (10-25+) — PID, UID, COMM, EXE, TRANSPORT,
  etc. Cada campo en su propia línea indentada con espacios
- `json-pretty`: los mismos campos que verbose pero en formato JSON con
  llaves, comillas y comas
- `cat`: solo el texto del mensaje, sin ningún metadato — útil para
  extraer el contenido puro

</details>

### Ejercicio 4 — Explorar campos con -F

```bash
# 1. ¿Qué comandos han generado logs?
journalctl -F _COMM --no-pager | head -20

# 2. ¿Qué prioridades existen en el journal?
journalctl -F PRIORITY --no-pager

# 3. ¿Qué transports se han usado?
journalctl -F _TRANSPORT --no-pager

# 4. ¿Qué unidades de systemd han logueado?
journalctl -F _SYSTEMD_UNIT --no-pager | head -10
```

<details><summary>Predicción</summary>

- `_COMM`: lista de nombres de procesos (sshd, systemd, kernel, cron, etc.)
- `PRIORITY`: números del 0 al 7 (los que existan en los logs)
- `_TRANSPORT`: típicamente journal, syslog, kernel, stdout, driver
- `_SYSTEMD_UNIT`: todos los servicios que han escrito logs — será una
  lista larga si el sistema tiene muchos servicios
- `-F` solo muestra valores **únicos** — es como `sort -u` sobre todos
  los valores de ese campo

</details>

### Ejercicio 5 — Combinación AND y OR

```bash
# 1. AND implícito — logs de sshd, solo errores, boot actual:
journalctl -u sshd -p err -b --no-pager

# 2. OR con múltiples -u:
journalctl -u sshd -u cron -n 10 --no-pager

# 3. OR con operador + (campos):
journalctl _COMM=sshd + _COMM=cron -n 10 --no-pager

# Pregunta: ¿Cuál es la diferencia entre #2 y #3?
```

<details><summary>Predicción</summary>

- `#1` mostrará solo errores de sshd del boot actual — probablemente
  pocas o ninguna entrada si sshd funciona bien
- `#2` y `#3` darán resultados similares pero no idénticos:
  - `-u sshd` filtra por `_SYSTEMD_UNIT=sshd.service` (logs del servicio
    systemd incluyendo subprocesos)
  - `_COMM=sshd` filtra por el nombre del comando (solo procesos que se
    llamen exactamente "sshd")
  - Un subproceso de sshd podría tener un `_COMM` diferente pero seguir
    bajo la misma `_SYSTEMD_UNIT`

</details>

### Ejercicio 6 — Follow en vivo

```bash
# 1. Seguir todos los logs en vivo (Ctrl+C para salir):
# journalctl -f

# 2. Seguir solo errores:
# journalctl -f -p err

# 3. Generar un log y verlo aparecer:
# Terminal 1: journalctl -f -t test-log
# Terminal 2: logger -t test-log "Este es un mensaje de prueba"
# Terminal 2: logger -t test-log -p user.err "Este es un error de prueba"

# En un solo comando para probar:
logger -t test-log "Mensaje de prueba desde ejercicio 6" && \
    journalctl -t test-log -n 1 --no-pager
```

<details><summary>Predicción</summary>

- `logger` envía mensajes al journal vía syslog
- `-t test-log` establece el SYSLOG_IDENTIFIER
- El mensaje aparecerá inmediatamente en `journalctl -f -t test-log`
- Con `-p user.err`, el mensaje tendrá PRIORITY=3 y aparecerá en
  `journalctl -f -p err`
- `journalctl -t` es equivalente a `journalctl SYSLOG_IDENTIFIER=`

</details>

### Ejercicio 7 — JSON y jq

```bash
# 1. Ver una entrada en JSON:
journalctl -o json-pretty -n 1 --no-pager

# 2. Extraer solo el mensaje de las últimas 5 entradas:
journalctl -o json -n 5 --no-pager | jq -r '.MESSAGE'

# 3. Contar entradas por prioridad:
journalctl -b -o json --no-pager | \
    jq -r '.PRIORITY' | sort | uniq -c | sort -rn
```

<details><summary>Predicción</summary>

- El JSON tendrá campos como `__CURSOR`, `__REALTIME_TIMESTAMP`,
  `_BOOT_ID`, `MESSAGE`, `PRIORITY`, `_COMM`, etc.
- El campo PRIORITY es un **string** ("3", "6"), no un número — por eso
  jq usa `select(.PRIORITY == "3")` con comillas
- El conteo por prioridad mostrará que la mayoría de entradas son
  `6` (info) o `5` (notice), con pocas de `3` (err) o menores

</details>

### Ejercicio 8 — Espacio en disco

```bash
# 1. ¿Cuánto espacio usa el journal?
journalctl --disk-usage

# 2. ¿Dónde se almacenan los archivos?
ls -lh /var/log/journal/ 2>/dev/null || \
    ls -lh /run/log/journal/ 2>/dev/null || \
    echo "No se encontró directorio del journal"

# 3. Verificar integridad:
journalctl --verify 2>&1 | tail -5
```

<details><summary>Predicción</summary>

- `--disk-usage` mostrará el tamaño total (típicamente 50-500MB)
- Si existe `/var/log/journal/` → almacenamiento **persistente** (sobrevive
  reboots)
- Si solo existe `/run/log/journal/` → almacenamiento **volátil** (solo RAM,
  se pierde al reiniciar)
- Dentro del directorio habrá un subdirectorio con el machine-id y archivos
  `.journal` (activo) y `.journal~` (archivados)
- `--verify` reportará PASS para cada archivo o FAIL si hay corrupción

</details>

### Ejercicio 9 — Script de verificación de errores

```bash
# Crear un one-liner que reporte servicios con errores hoy:
journalctl -p err --since today -o json --no-pager 2>/dev/null | \
    jq -r '._SYSTEMD_UNIT // "_sin_unidad"' | \
    sort | uniq -c | sort -rn | head -10

# ¿Qué servicio tiene más errores?
```

<details><summary>Predicción</summary>

- La salida mostrará una lista de unidades con la cantidad de errores,
  ordenada de más a menos
- Si el sistema está saludable, habrá pocas o ninguna entrada
- Servicios con muchos errores son candidatos para investigación con
  `journalctl -u SERVICIO -p err --since today`
- `// "_sin_unidad"` en jq es un fallback: si `_SYSTEMD_UNIT` no existe
  (ej: logs del kernel), muestra "_sin_unidad" en lugar de null

</details>

### Ejercicio 10 — Diagnóstico de un servicio caído

Un servicio `myapp.service` se reinicia constantemente. ¿Cómo diagnosticarlo
con journalctl?

```bash
# 1. Ver los últimos eventos del servicio:
journalctl -u myapp -n 50 --no-pager

# 2. Solo errores:
journalctl -u myapp -p err --no-pager

# 3. Ver el patrón de reinicio (buscar Started/Stopped/Failed):
journalctl -u myapp --no-pager | grep -E "(Started|Stopped|Failed|Main process exited)"

# 4. Ver el último mensaje antes de cada crash:
journalctl -u myapp -o short-precise --no-pager | grep -B 2 "Failed"

# 5. ¿El OOM killer lo mató?
journalctl -k --no-pager | grep -i "oom.*myapp\|myapp.*killed"
```

<details><summary>Predicción</summary>

- Si el servicio se reinicia constantemente, veremos un patrón repetitivo:
  `Started → (mensajes de la app) → Main process exited → Failed → Started`
- `-o short-precise` da timestamps con microsegundos para ver el intervalo
  entre restarts (si es constante, probablemente `RestartSec=` es bajo)
- Los errores más comunes:
  - Exit code != 0 → bug en la aplicación
  - Signal 9 (SIGKILL) → OOM killer o `TimeoutStopSec` excedido
  - Signal 11 (SIGSEGV) → crash de la aplicación (segfault)
- Si el kernel tiene mensajes OOM para myapp, la solución es aumentar
  `MemoryMax=` o reducir el consumo de memoria de la app

</details>
