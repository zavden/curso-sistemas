# T02 — Directivas de logrotate

> **Objetivo:** Dominar todas las directivas de logrotate para crear configuraciones de producción robustas y diagnosticar problemas.

## Erratas detectadas en el material fuente

| Archivo | Línea | Error | Corrección |
|---------|-------|-------|------------|
| README.max.md | 252 | `notifempty` marcado como `(default)` — el default es `ifempty` (rotar incluso si está vacío), como correctamente indica la tabla en la línea 439 del mismo archivo | `ifempty` es el default; `notifempty` es la directiva que se activa explícitamente |
| README.max.md | 140 | `create` sin argumentos "usa mode 0600, owner root, group root" — incorrecto | Sin argumentos, `create` hereda mode y ownership del archivo original (man logrotate) |

---

## Directivas de frecuencia

Controlan **cada cuánto** se rota un archivo:

```bash
# Por tiempo:
daily        # cada día
weekly       # cada semana (default)
monthly      # cada mes
yearly       # cada año

# Por tamaño (ignora el tiempo):
size 100M    # rotar cuando alcance 100MB
size 10k     # 10 kilobytes
size 1G      # 1 gigabyte

# size IGNORA daily/weekly — si el archivo no alcanza el tamaño, no se rota
```

```bash
# Combinación tiempo + tamaño:
maxsize 100M   # rotar SI ALCANZA 100M, INCLUSO si no ha pasado el período
minsize 1M     # NO rotar si es menor a 1M, aunque haya pasado el período

# Ejemplo:
/var/log/myapp.log {
    daily
    minsize 1M     # evitar rotaciones de archivos vacíos o tiny
    maxsize 500M   # rotar inmediatamente si crece mucho
    rotate 30
}
```

**Resumen:**

| Directiva | Comportamiento |
|-----------|---------------|
| `size N` | Solo rota por tamaño, ignora `daily`/`weekly` |
| `minsize N` | No rota si < N, **aunque** haya pasado el período |
| `maxsize N` | Rota si > N, **incluso** si no ha pasado el período |

---

## Directivas de retención

Controlan **cuántas** copias se mantienen:

```bash
# Por número de archivos:
rotate 7      # mantener 7 copias (más el actual)
rotate 0      # no mantener copias (eliminar al rotar)
rotate 365    # mantener un año (daily)

# Por antigüedad:
maxage 30     # eliminar copias de más de 30 días
              # independiente de rotate
```

```bash
# rotate vs maxage:
# rotate = contar archivos
# maxage = contar días

# Ejemplo — retención de 30 días:
/var/log/myapp.log {
    daily
    rotate 30          # mantener 30 archivos
    maxage 30          # redundante con rotate+daily, pero explícito
}

# Ejemplo — sin límite por conteo, eliminar por edad:
/var/log/myapp.log {
    daily
    rotate 999         # sin límite práctico por conteo
    maxage 90          # pero eliminar todo mayor a 90 días
}
```

---

## Directivas de compresión

```bash
compress           # comprimir con gzip (default: .gz)
nocompress         # no comprimir (default global)

# delaycompress — NO comprimir el recién rotado (.1):
# Útil porque:
#   - hay una ventana donde la app aún puede escribir en .1
#   - permite ver el log de ayer sin descomprimir
delaycompress
```

### Compresores alternativos

```bash
# xz (mejor compresión, más lento):
compresscmd /usr/bin/xz
uncompresscmd /usr/bin/unxz
compressext .xz
compressoptions -9    # máxima compresión

# zstd (rápido, buen ratio — recomendado para producción):
compresscmd /usr/bin/zstd
uncompresscmd /usr/bin/unzstd
compressext .zst
compressoptions -T0   # usar todos los cores
```

### Comparativa de compresores

| Compresor | Ratio | Velocidad | CPU | Extensión |
|-----------|-------|-----------|-----|-----------|
| gzip | ~70% | Alta | Baja | .gz |
| bzip2 | ~75% | Media | Alta | .bz2 |
| xz | ~80% | Baja | Muy alta | .xz |
| zstd | ~75% | Muy alta | Baja | .zst |

```bash
# Ejemplo con compress + delaycompress:
/var/log/myapp.log {
    daily
    rotate 30
    compress
    delaycompress

    # Resultado:
    # myapp.log        (actual)
    # myapp.log.1      (ayer — sin comprimir, acceso rápido)
    # myapp.log.2.gz   (anteayer — comprimido)
    # myapp.log.3.gz   (hace 3 días)
}
```

---

## Directivas de creación de archivo

### create — Renombrar y crear nuevo

```bash
# create es el mecanismo por defecto:
create                         # heredar mode y ownership del archivo original
create 0640 www-data adm       # especificar permisos, owner y group
nocreate                       # no crear archivo nuevo (la app lo creará)

# Flujo:
# 1. mv myapp.log → myapp.log.1 (rename)
# 2. touch myapp.log (vacío)
# 3. chown/chmod según create
# 4. La aplicación necesita reabrir el archivo (postrotate reload)
```

```bash
# Problema: la app tiene el fd abierto al archivo viejo
# Solución — postrotate para notificar:
/var/log/myapp.log {
    create 0640 myapp myapp
    postrotate
        systemctl reload myapp > /dev/null 2>&1 || true
    endscript
}
```

### copytruncate — Copiar y truncar

```bash
# Para aplicaciones que NO pueden reabrir archivos:
copytruncate

# Flujo:
# 1. cp myapp.log myapp.log.1 (copia)
# 2. truncate -s 0 myapp.log (vaciar el original)
# 3. La app sigue escribiendo en el mismo fd (truncado)

# Ventaja: no necesita postrotate/reload
# Desventaja: puede perder logs entre cp y truncate

# Cuándo usar:
# - Apps legacy que abren el archivo una vez y no lo cierran
# - Apps de terceros sin control sobre su logging
# - Apps que no soportan señales de reload
```

### copy — Solo copiar

```bash
# Copia sin modificar el original:
copy

# El archivo sigue creciendo indefinidamente
# Útil para archival sin rotación real
# Se combina con maxsize para rotar por tamaño
```

### Comparativa: create vs copytruncate vs copy

```
┌─────────────────────────────────────────────────────────────────────────────┐
│  create (DEFAULT)                                                           │
│  ─────────────────                                                          │
│  mv log → log.1                                                             │
│  touch log (nuevo)                                                          │
│  postrotate: reload → app escribe en log                                    │
│  + Sin pérdida de logs                                                      │
│  - La app debe reabrir archivos (reload)                                    │
├─────────────────────────────────────────────────────────────────────────────┤
│  copytruncate                                                               │
│  ───────────────                                                            │
│  cp log → log.1                                                             │
│  truncate -s 0 log                                                          │
│  → app sigue en el mismo fd, escribe en log (ahora vacío)                   │
│  - PUEDE perder logs (entre cp y truncate)                                  │
│  + Funciona con cualquier app (no necesita reload)                          │
├─────────────────────────────────────────────────────────────────────────────┤
│  copy                                                                       │
│  ────                                                                       │
│  cp log → log.backup                                                        │
│  → log NUNCA se modifica                                                    │
│  - El log original sigue creciendo                                          │
│  + Para archival sin afectar la app                                         │
└─────────────────────────────────────────────────────────────────────────────┘
```

**`create` y `copytruncate` son mutuamente excluyentes.** Usar ambos es un error.

---

## Directivas de nombrado

```bash
# Esquema numérico (default):
# myapp.log.1, myapp.log.2.gz, myapp.log.3.gz

# Con fecha:
dateext           # myapp.log-20260317
nodateext         # volver a esquema numérico

# Formato de fecha:
dateformat -%Y%m%d              # -20260317 (default con dateext)
dateformat -%Y%m%d-%H%M%S      # -20260317-153000 (con hora)
dateformat -%Y-%m-%d            # -2026-03-17

# dateyesterday: usar fecha de AYER:
dateyesterday     # útil para logs que se rotan temprano en la mañana
```

```bash
# dateext + maxsize necesita hora para evitar colisiones:
/var/log/myapp.log {
    daily
    maxsize 500M
    dateext
    dateformat -%Y%m%d-%H%M%S
    # Si rota a las 3am: myapp.log-20260317-030000
    # Si vuelve a rotar a las 3pm: myapp.log-20260317-150000
}
```

---

## Directivas de control

```bash
missingok          # no error si el archivo no existe
nomissingok        # reportar error (default)

notifempty         # no rotar si el archivo está vacío
ifempty            # rotar incluso si está vacío (default)

shred              # sobrescribir archivos antes de eliminar (seguridad)
shredcycles 3      # veces que se sobrescribe
noshred            # eliminar normalmente (default)
```

---

## Scripts — prerotate, postrotate, firstaction, lastaction

### postrotate — Después de rotar

```bash
# Notificar a la app que reabra el archivo:

# nginx (USR1 para reabrir logs):
/var/log/nginx/*.log {
    postrotate
        [ -f /var/run/nginx.pid ] && kill -USR1 $(cat /var/run/nginx.pid)
    endscript
}

# rsyslog:
/var/log/rsyslog/*.log {
    postrotate
        /usr/lib/rsyslog/rsyslog-rotate
    endscript
}

# Servicio systemd:
/var/log/myapp.log {
    postrotate
        systemctl reload myapp > /dev/null 2>&1 || true
    endscript
}
```

### prerotate — Antes de rotar

```bash
# Ejecutar algo ANTES de la rotación:
/var/log/myapp.log {
    prerotate
        # Abortar si el servicio está en mantenimiento:
        [ -f /var/run/myapp-maintenance ] && exit 1
    endscript
}
# Si prerotate falla (exit != 0), la rotación se aborta
```

### firstaction y lastaction

```bash
# Se ejecutan UNA SOLA VEZ para todo el bloque:
/var/log/myapp/*.log {
    sharedscripts          # NECESARIO para que funcione
    firstaction
        logger "logrotate: iniciando rotación myapp"
    endscript
    postrotate
        systemctl reload myapp > /dev/null 2>&1 || true
    endscript
    lastaction
        logger "logrotate: rotación myapp completada"
    endscript
}
```

### sharedscripts vs nosharedscripts

```bash
# Sin sharedscripts — postrotate se ejecuta POR CADA ARCHIVO:
/var/log/myapp/*.log {
    postrotate
        systemctl reload myapp    # se ejecuta N veces (1 por cada .log)
    endscript
}

# Con sharedscripts — se ejecuta UNA SOLA VEZ:
/var/log/myapp/*.log {
    sharedscripts
    postrotate
        systemctl reload myapp    # se ejecuta 1 vez
    endscript
}

# IMPORTANTE: con sharedscripts, si NINGÚN archivo necesita rotación,
# postrotate NO se ejecuta para ninguno
# nosharedscripts restaura el comportamiento por defecto
```

---

## Directivas avanzadas

### su — Ejecutar como otro usuario

```bash
# logrotate corre como root, pero los logs pueden ser de otro usuario:
/var/log/myapp/*.log {
    su myapp myapp    # ejecutar la rotación como myapp:myapp
    create 0640 myapp myapp
    ...
}

# Necesario cuando /var/log/myapp/ es propiedad de un usuario no-root
# Sin su, puede fallar con:
#   "skipping because parent directory has insecure permissions"
```

### tabooext — Extensiones ignoradas

```bash
# logrotate ignora estas extensiones en /etc/logrotate.d/:
# .dpkg-old, .dpkg-dist, .dpkg-new, .disabled, ~, .bak, .rpmsave, .rpmorig

# Agregar más extensiones a ignorar:
tabooext + .disabled .skip
```

### include

```bash
# Incluir archivos o directorios adicionales:
include /etc/logrotate.d
include /opt/myapp/logrotate.conf
```

---

## Ejemplo completo de producción

```bash
# /etc/logrotate.d/myapp
/var/log/myapp/app.log
/var/log/myapp/error.log
/var/log/myapp/access.log
{
    daily
    rotate 30
    maxage 90

    compress
    delaycompress
    compresscmd /usr/bin/zstd
    compressext .zst
    compressoptions -T0

    missingok
    notifempty

    create 0640 myapp myapp

    dateext
    dateformat -%Y%m%d

    sharedscripts
    postrotate
        systemctl reload myapp.service > /dev/null 2>&1 || true
    endscript
}
```

---

## Diagnóstico de problemas

```bash
# 1. Dry run — ver qué haría sin ejecutar:
sudo logrotate -dv /etc/logrotate.d/myapp

# 2. Forzar rotación con verbose:
sudo logrotate -fv /etc/logrotate.d/myapp

# 3. Ver estado de rotación:
grep myapp /var/lib/logrotate/status

# 4. Logs de logrotate (si corre via systemd):
journalctl -u logrotate --since today --no-pager
```

### Errores comunes

| Error | Causa | Solución |
|-------|-------|----------|
| `stat of ... failed: No such file` | Archivo no existe | Agregar `missingok` |
| `skipping ... parent directory has insecure permissions` | Directorio con 777 | `chmod 755 /var/log/dir/` o usar `su` |
| `unknown option 'x'` | Typo o directiva inválida | Verificar sintaxis |
| App sigue escribiendo en .1 | Falta postrotate/reload | Agregar postrotate con reload |
| `duplicate log entry` | Mismo archivo en dos bloques | Consolidar en un solo bloque |
| `bad rotation count 'N'` | rotate con valor inválido | Usar número positivo |

---

## Tabla resumen de directivas

| Directiva | Descripción | Default |
|-----------|-------------|---------|
| `daily`/`weekly`/`monthly`/`yearly` | Frecuencia de rotación | `weekly` |
| `rotate N` | Copias a mantener | `4` |
| `size N` | Rotar por tamaño | (no usado) |
| `minsize N` | No rotar si menor que N | (no usado) |
| `maxsize N` | Rotar si mayor que N | (no usado) |
| `maxage N` | Eliminar copias > N días | (no usado) |
| `compress` | Comprimir rotados | `nocompress` |
| `delaycompress` | No comprimir el más reciente | (no usado) |
| `create mode owner group` | Crear nuevo con permisos | `create` (hereda permisos) |
| `copytruncate` | Copiar y truncar | (no usado) |
| `dateext` | Fecha en el nombre | `nodateext` |
| `missingok` | Ignorar si no existe | `nomissingok` |
| `notifempty` | No rotar si vacío | `ifempty` |
| `sharedscripts` | Scripts una vez por bloque | `nosharedscripts` |
| `su user group` | Ejecutar como otro usuario | (root) |

---

## Quick reference

```
FRECUENCIA:
  daily / weekly / monthly / yearly
  size N       — solo por tamaño (ignora tiempo)
  minsize N    — no rotar si < N aunque toque
  maxsize N    — rotar si > N aunque no toque

RETENCIÓN:
  rotate N     — mantener N copias
  maxage N     — eliminar copias > N días

COMPRESIÓN:
  compress / nocompress
  delaycompress
  compresscmd /usr/bin/zstd
  compressext .zst
  compressoptions -T0

CREACIÓN:
  create MODE OWNER GROUP    — renombrar + crear (necesita postrotate)
  copytruncate              — copiar + truncar (no necesita postrotate)
  copy                      — solo copiar (el original no se toca)

NOMBRADO:
  dateext / nodateext
  dateformat -%Y%m%d
  dateyesterday

CONTROL:
  missingok / nomissingok
  notifempty / ifempty
  shred / noshred

SCRIPTS:
  sharedscripts / nosharedscripts
  prerotate ... endscript
  postrotate ... endscript
  firstaction ... endscript
  lastaction ... endscript

AVANZADAS:
  su USER GROUP
  tabooext + .ext
  include FILE
```

---

## Labs

### Lab Parte 1 — Frecuencia, retención y compresión

**Paso 1.1: Directivas de frecuencia**

```bash
docker compose exec debian-dev bash -c '
echo "=== Frecuencia ==="
echo ""
echo "--- Por tiempo ---"
echo "  daily       cada día"
echo "  weekly      cada semana"
echo "  monthly     cada mes"
echo "  yearly      cada año"
echo ""
echo "--- Por tamaño ---"
echo "  size 100M   rotar cuando alcance 100MB"
echo "  size IGNORA la frecuencia"
echo ""
echo "--- Combinación tamaño + tiempo ---"
echo "  maxsize 100M  rotar si llega a 100M AUNQUE no toque"
echo "  minsize 10M   NO rotar si tiene menos de 10M aunque toque"
echo ""
echo "--- Frecuencia en configs del sistema ---"
for f in /etc/logrotate.d/*; do
    FREQ=$(grep -m1 -oE "daily|weekly|monthly|yearly" "$f" 2>/dev/null)
    [[ -n "$FREQ" ]] && echo "  $(basename $f): $FREQ"
done
'
```

**Paso 1.2: Directivas de retención**

```bash
docker compose exec debian-dev bash -c '
echo "=== Retención ==="
echo ""
echo "--- rotate N ---"
echo "  rotate 7     mantener 7 copias (+el actual)"
echo "  rotate 0     no mantener copias (eliminar al rotar)"
echo "  rotate 365   un año de copias diarias"
echo ""
echo "--- maxage N ---"
echo "  maxage 30    eliminar copias de más de 30 días"
echo "  Independiente de rotate"
echo ""
echo "--- Retención en el sistema ---"
grep -h "rotate " /etc/logrotate.conf /etc/logrotate.d/* 2>/dev/null | \
    grep -v "^#" | sort | uniq -c | sort -rn | head -5
'
```

**Paso 1.3: Directivas de compresión**

```bash
docker compose exec debian-dev bash -c '
echo "=== Compresión ==="
echo ""
echo "--- compress / nocompress ---"
echo "  compress        comprimir con gzip (default: nocompress)"
echo "  nocompress      no comprimir"
echo ""
echo "--- delaycompress ---"
echo "  No comprimir el archivo recién rotado (.1)"
echo "  Comprimirlo en la siguiente rotación (.2.gz)"
echo ""
echo "--- Algoritmos alternativos ---"
echo "  xz:   compresscmd /usr/bin/xz   (mejor ratio, más lento)"
echo "  zstd: compresscmd /usr/bin/zstd  (rápido, buen ratio)"
echo ""
echo "--- Resultado con compress + delaycompress ---"
echo "  myapp.log        (actual)"
echo "  myapp.log.1      (ayer, sin comprimir)"
echo "  myapp.log.2.gz   (anteayer, comprimido)"
echo "  myapp.log.3.gz   (hace 3 días, comprimido)"
'
```

### Lab Parte 2 — Creación, nombrado y control

**Paso 2.1: create**

```bash
docker compose exec debian-dev bash -c '
echo "=== create ==="
echo ""
echo "Mecanismo por defecto:"
echo ""
echo "  create              heredar permisos del archivo original"
echo "  create 0640 www-data adm    especificar permisos, owner, group"
echo "  nocreate            no crear (la app lo creará)"
echo ""
echo "--- Flujo ---"
echo "  1. myapp.log → myapp.log.1 (rename)"
echo "  2. Se crea myapp.log nuevo (vacío)"
echo "  3. La app necesita reabrir (postrotate)"
echo ""
echo "--- Problema ---"
echo "  La app puede tener file descriptor abierto al viejo"
echo "  y seguir escribiendo en .1"
echo ""
echo "--- Solución: postrotate ---"
echo "  postrotate"
echo "      systemctl reload myapp > /dev/null 2>&1 || true"
echo "  endscript"
'
```

**Paso 2.2: copytruncate**

```bash
docker compose exec debian-dev bash -c '
echo "=== copytruncate ==="
echo ""
echo "Para apps que NO pueden reabrir archivos:"
echo ""
echo "--- Flujo ---"
echo "  1. cp myapp.log myapp.log.1 (copia)"
echo "  2. truncate myapp.log (vaciar el original)"
echo "  3. La app sigue escribiendo en el mismo archivo"
echo ""
echo "--- Ventaja ---"
echo "  No necesita postrotate/reload"
echo ""
echo "--- Desventaja ---"
echo "  Hay una ventana entre copy y truncate"
echo "  donde las líneas escritas se PIERDEN"
echo ""
echo "--- Cuándo usar ---"
echo "  Apps legacy que abren el archivo una vez"
echo "  Apps de terceros sin control sobre su logging"
echo "  Apps que no soportan señales de reload"
echo ""
echo "--- create y copytruncate son MUTUAMENTE EXCLUYENTES ---"
'
```

**Paso 2.3: dateext**

```bash
docker compose exec debian-dev bash -c '
echo "=== dateext ==="
echo ""
echo "--- Sin dateext (default Debian) ---"
echo "  myapp.log.1"
echo "  myapp.log.2.gz"
echo ""
echo "--- Con dateext ---"
echo "  myapp.log-20260317"
echo "  myapp.log-20260316.gz"
echo ""
echo "--- dateformat ---"
echo "  -%Y%m%d          default: -20260317"
echo "  -%Y-%m-%d        más legible: -2026-03-17"
echo "  -%Y%m%d-%H%M%S   con hora (para maxsize)"
echo ""
echo "--- dateyesterday ---"
echo "  Nombrar con la fecha de ayer (no de la rotación)"
echo ""
echo "--- Este sistema usa ---"
ls /var/log/syslog* /var/log/messages* 2>/dev/null | head -5
if ls /var/log/syslog-2* /var/log/messages-2* >/dev/null 2>&1; then
    echo "  → dateext"
else
    echo "  → números"
fi
'
```

**Paso 2.4: Directivas de control**

```bash
docker compose exec debian-dev bash -c '
echo "=== Directivas de control ==="
echo ""
echo "--- missingok / nomissingok ---"
echo "  missingok      ignorar si el archivo no existe"
echo "  nomissingok    reportar error (default)"
echo ""
echo "--- notifempty / ifempty ---"
echo "  notifempty     no rotar si está vacío"
echo "  ifempty        rotar aunque esté vacío (default)"
echo ""
echo "--- Configs con missingok ---"
grep -rl "missingok" /etc/logrotate.d/ 2>/dev/null | \
    while read f; do echo "  $(basename $f)"; done
echo ""
echo "--- Configs con notifempty ---"
grep -rl "notifempty" /etc/logrotate.d/ 2>/dev/null | \
    while read f; do echo "  $(basename $f)"; done
'
```

### Lab Parte 3 — Scripts y avanzadas

**Paso 3.1: postrotate**

```bash
docker compose exec debian-dev bash -c '
echo "=== postrotate ==="
echo ""
echo "Script que se ejecuta DESPUÉS de rotar:"
echo ""
echo "--- Ejemplos comunes ---"
echo "  nginx:   kill -USR1 (reabrir logs)"
echo "  rsyslog: /usr/lib/rsyslog/rsyslog-rotate"
echo "  systemd: systemctl reload myapp"
echo ""
echo "--- postrotate en configs reales ---"
for f in /etc/logrotate.d/*; do
    if grep -q "postrotate" "$f" 2>/dev/null; then
        echo ""
        echo "=== $(basename $f) ==="
        sed -n "/postrotate/,/endscript/p" "$f"
    fi
done
'
```

**Paso 3.2: sharedscripts**

```bash
docker compose exec debian-dev bash -c '
echo "=== sharedscripts ==="
echo ""
echo "--- Sin sharedscripts ---"
echo "  postrotate se ejecuta N veces (1 por cada archivo)"
echo ""
echo "--- Con sharedscripts ---"
echo "  postrotate se ejecuta 1 vez para todo el bloque"
echo ""
echo "IMPORTANTE: con sharedscripts, si ningún archivo necesita"
echo "rotación, postrotate NO se ejecuta."
echo ""
echo "--- firstaction / lastaction ---"
echo "  firstaction: UNA vez ANTES de procesar cualquier archivo"
echo "  lastaction: UNA vez DESPUÉS de procesar todos los archivos"
'
```

**Paso 3.3: Directiva su**

```bash
docker compose exec debian-dev bash -c '
echo "=== Directiva su ==="
echo ""
echo "Si los logs son de un usuario no-root:"
echo ""
echo "  /var/log/myapp/*.log {"
echo "      su myapp myapp"
echo "      create 0640 myapp myapp"
echo "      ..."
echo "  }"
echo ""
echo "Ejecuta la rotación como myapp:myapp."
echo "Necesario cuando /var/log/myapp/ es propiedad de myapp."
echo ""
echo "Sin su, logrotate puede fallar con:"
echo "  \"skipping because parent directory has insecure permissions\""
'
```

**Paso 3.4: Ejemplo de producción**

```bash
docker compose exec debian-dev bash -c '
echo "=== Ejemplo completo de producción ==="
echo ""
echo "# /etc/logrotate.d/myapp"
echo "/var/log/myapp/app.log"
echo "/var/log/myapp/error.log"
echo "/var/log/myapp/access.log"
echo "{"
echo "    daily"
echo "    rotate 30"
echo "    maxage 90"
echo "    compress"
echo "    delaycompress"
echo "    missingok"
echo "    notifempty"
echo "    create 0640 myapp myapp"
echo "    dateext"
echo "    dateformat -%Y%m%d"
echo "    sharedscripts"
echo "    postrotate"
echo "        systemctl reload myapp.service > /dev/null 2>&1 || true"
echo "    endscript"
echo "}"
'
```

**Paso 3.5: Diagnóstico**

```bash
docker compose exec debian-dev bash -c '
echo "=== Diagnóstico ==="
echo ""
echo "1. Dry run con verbose:"
echo "   sudo logrotate -dv /etc/logrotate.d/myapp"
echo ""
echo "2. Forzar con verbose:"
echo "   sudo logrotate -fv /etc/logrotate.d/myapp"
echo ""
echo "3. Ver estado:"
echo "   grep myapp /var/lib/logrotate/status"
echo ""
echo "4. Errores recientes:"
echo "   journalctl -u logrotate --since today"
echo ""
echo "--- Probar la config completa ---"
sudo logrotate -d /etc/logrotate.conf 2>&1 | \
    grep -E "error|warning|considering|rotating" | head -10
'
```

---

## Ejercicios

### Ejercicio 1 — Analizar directivas de rsyslog

Lee la configuración de rotación de rsyslog e identifica cada directiva.

```bash
cat /etc/logrotate.d/rsyslog 2>/dev/null || echo "No encontrado"
```

**Predicción:**

<details><summary>¿Qué directivas clave esperas encontrar?</summary>

En Debian: `rotate 4`, `weekly`, `missingok`, `notifempty`, `compress`, `delaycompress`, `sharedscripts`, y `postrotate` con `/usr/lib/rsyslog/rsyslog-rotate`. `sharedscripts` es crítico porque el bloque tiene ~12 archivos; sin él, el postrotate se ejecutaría 12 veces innecesariamente. `delaycompress` mantiene `syslog.1` sin comprimir para acceso rápido al log de ayer.
</details>

### Ejercicio 2 — size vs minsize vs maxsize

Explica la diferencia con un escenario concreto.

```bash
cat << 'EOF'
# Escenario: archivo de 500KB, frecuencia daily, aún no ha pasado el día

# ¿Se rota con estas configuraciones?

# Config A:
size 1M
# → NO se rota (500KB < 1M, size ignora el tiempo)

# Config B:
daily
minsize 1M
# → NO se rota (500KB < 1M, minsize bloquea aunque toque)

# Config C:
weekly
maxsize 1M
# → NO se rota (500KB < 1M y no ha pasado la semana)

# ¿Y si el archivo tiene 2MB?

# Config A: SÍ (2MB > 1M)
# Config B: SÍ si ha pasado el día (2MB > 1M y toca)
# Config C: SÍ inmediatamente (2MB > 1M, maxsize fuerza rotación)
EOF
```

**Predicción:**

<details><summary>¿Cuándo usarías cada uno en producción?</summary>

- **`size`:** cuando solo importa el tamaño, no el tiempo (p. ej., logs de batch jobs que pueden no ejecutarse diariamente)
- **`minsize`:** con `daily`, para evitar rotar archivos tiny que apenas tuvieron actividad ese día — ahorra espacio en rotaciones vacías
- **`maxsize`:** con `daily`/`weekly`, para proteger contra explosiones de logs que llenen el disco antes del período — es un "circuit breaker"

En producción, `daily` + `minsize 1M` + `maxsize 500M` es una combinación robusta.
</details>

### Ejercicio 3 — Compresores alternativos

Verifica qué compresores hay disponibles y compara.

```bash
echo "=== Compresores disponibles ==="
for cmd in gzip bzip2 xz zstd; do
    which $cmd >/dev/null 2>&1 && echo "  $cmd: disponible" || echo "  $cmd: NO disponible"
done

echo ""
echo "=== Comparar compresión de un log ==="
LOG=$(ls /var/log/syslog /var/log/messages 2>/dev/null | head -1)
if [[ -n "$LOG" ]]; then
    SIZE=$(stat -c%s "$LOG" 2>/dev/null)
    echo "Archivo: $LOG ($SIZE bytes)"
fi
```

**Predicción:**

<details><summary>¿Qué compresor elegirías para producción?</summary>

**zstd** es la mejor opción para producción moderna: ratio de compresión comparable a gzip, pero significativamente más rápido en compresión y descompresión. Con `-T0` usa todos los cores disponibles. gzip sigue siendo el default porque está universalmente disponible y las herramientas como `zgrep`, `zcat` lo soportan nativamente. Para logs que raramente se consultan, xz ofrece el mejor ratio pero es mucho más lento.
</details>

### Ejercicio 4 — create vs copytruncate: inodes

Demuestra la diferencia en el comportamiento del inode.

```bash
# Crear archivo de prueba:
sudo mkdir -p /var/log/testdir
echo "test $(date)" | sudo tee /var/log/testdir/test.log

echo "=== Inode ANTES ==="
ls -li /var/log/testdir/test.log

# Con create: el inode CAMBIA (nuevo archivo)
# Con copytruncate: el inode NO CAMBIA (mismo archivo, truncado)

# Simular create:
echo "Simulando create:"
ORIGINAL_INODE=$(stat -c%i /var/log/testdir/test.log)
sudo mv /var/log/testdir/test.log /var/log/testdir/test.log.1
echo "nuevo" | sudo tee /var/log/testdir/test.log > /dev/null
NEW_INODE=$(stat -c%i /var/log/testdir/test.log)
echo "  Inode original: $ORIGINAL_INODE"
echo "  Inode nuevo:    $NEW_INODE"
echo "  ¿Cambió? $([[ $ORIGINAL_INODE != $NEW_INODE ]] && echo SÍ || echo NO)"

# Limpiar:
sudo rm -rf /var/log/testdir
```

**Predicción:**

<details><summary>¿Por qué importa el inode?</summary>

Las aplicaciones mantienen un file descriptor (fd) apuntando al **inode**, no al nombre del archivo. Con `create`, el archivo se renombra (mismo inode → `.1`) y se crea uno nuevo (inode diferente). La app sigue con su fd al inode viejo, escribiendo en `.1`. Por eso necesita `postrotate` para reabrir.

Con `copytruncate`, el archivo se copia y se trunca — **mismo inode**. La app sigue escribiendo en el mismo fd/inode, que ahora está vacío. No necesita reload, pero hay una ventana de pérdida entre copy y truncate.
</details>

### Ejercicio 5 — dateext y dateformat

Observa cómo dateext cambia la nomenclatura.

```bash
# Crear config con dateext:
sudo mkdir -p /var/log/testdate
echo "test $(date)" | sudo tee /var/log/testdate/app.log

sudo tee /tmp/test-dateext.conf << 'EOF'
/var/log/testdate/app.log {
    daily
    rotate 3
    dateext
    dateformat -%Y%m%d
    missingok
    create 0640 root root
}
EOF

echo "=== Dry run con dateext ==="
sudo logrotate -d /tmp/test-dateext.conf 2>&1

# Limpiar:
sudo rm -rf /var/log/testdate /tmp/test-dateext.conf
```

**Predicción:**

<details><summary>¿Cómo se llamará el archivo rotado?</summary>

Se llamará `app.log-20260325` (con la fecha de hoy). Si además tuviera `dateyesterday`, usaría `app.log-20260324`. Con `dateformat -%Y%m%d-%H%M%S`, incluiría la hora: `app.log-20260325-060000`. La ventaja de dateext es que los nombres son autoexplicativos — no hay que adivinar cuándo se rotó un archivo.
</details>

### Ejercicio 6 — sharedscripts en acción

Observa la diferencia de comportamiento con y sin sharedscripts.

```bash
# Sin sharedscripts: postrotate por CADA archivo
cat << 'EOF'
# Si hay 3 archivos .log en /var/log/myapp/:
/var/log/myapp/*.log {
    postrotate
        echo "reload ejecutado" >> /tmp/rotate-count.log
    endscript
}
# → postrotate se ejecuta 3 veces
# → /tmp/rotate-count.log tiene 3 líneas
EOF

echo "---"

# Con sharedscripts: postrotate UNA vez
cat << 'EOF'
/var/log/myapp/*.log {
    sharedscripts
    postrotate
        echo "reload ejecutado" >> /tmp/rotate-count.log
    endscript
}
# → postrotate se ejecuta 1 vez
# → /tmp/rotate-count.log tiene 1 línea
EOF
```

**Predicción:**

<details><summary>¿Por qué sharedscripts es casi siempre la opción correcta?</summary>

Porque el postrotate típico es un `systemctl reload` o `kill -HUP` que le dice al servicio que reabra sus archivos de log. Hacer reload 12 veces (una por cada archivo) es innecesario y puede causar problemas de rendimiento o incluso interrupciones de servicio. Un solo reload es suficiente para que la app reabra **todos** sus archivos. La excepción sería si cada archivo pertenece a un servicio diferente — pero eso debería ser bloques separados.
</details>

### Ejercicio 7 — maxage vs rotate

Entiende cuándo difieren.

```bash
cat << 'EOF'
# Con daily + rotate 30:
# → Se mantienen exactamente 30 archivos
# → Si logrotate no corrió 3 días, al volver hay 33 archivos
#   (los 3 días sin rotar acumularon en el actual, no se borraron los viejos)
# → rotate solo cuenta archivos, no edad

# Con daily + rotate 30 + maxage 30:
# → Se mantienen máximo 30 archivos
# → ADEMÁS, se eliminan archivos mayores a 30 días por fecha
# → Más robusto contra acumulación si logrotate se pausa

# Cuándo maxage importa:
# - Sistemas que se apagan periódicamente
# - Logrotate que a veces falla
# - Regulaciones que exigen eliminación por edad, no por conteo
EOF
```

**Predicción:**

<details><summary>¿Cuándo rotate y maxage dan resultados diferentes?</summary>

Difieren cuando la frecuencia de rotación no es constante. Si logrotate no corrió durante 5 días y luego rota, `rotate 30` simplemente mantiene los 30 archivos más recientes (descartando el 31). Pero `maxage 30` verifica las fechas reales de cada archivo y elimina los que tienen más de 30 días, independientemente de cuántos haya. En producción estable con daily, son redundantes. Pero `maxage` añade una segunda red de seguridad.
</details>

### Ejercicio 8 — Directiva su

Entiende cuándo y por qué usar `su`.

```bash
echo "=== Directorios de logs y sus permisos ==="
ls -ld /var/log/ 2>/dev/null
ls -ld /var/log/syslog 2>/dev/null
ls -ld /var/log/nginx/ 2>/dev/null

echo ""
echo "=== ¿Alguna config usa su? ==="
grep -rl "^[[:space:]]*su " /etc/logrotate.d/ 2>/dev/null | while read f; do
    echo "$(basename $f):"
    grep "su " "$f"
done
echo "(si no hay salida, ninguna config usa su)"
```

**Predicción:**

<details><summary>¿Cuándo es necesaria la directiva su?</summary>

Cuando el directorio de logs pertenece a un usuario no-root (p. ej., `myapp:myapp`). logrotate corre como root pero verifica los permisos del directorio padre por seguridad. Si el directorio es propiedad de otro usuario y tiene permisos como 775, logrotate rechaza rotar con "insecure permissions". La directiva `su myapp myapp` le indica que la rotación se ejecute como ese usuario, resolviendo el conflicto de permisos.
</details>

### Ejercicio 9 — Prerotate para validación

Usa prerotate como guarda de seguridad.

```bash
cat << 'EOF'
# prerotate puede abortar la rotación si algo no está bien:

/var/log/myapp/app.log {
    daily
    rotate 30
    compress
    prerotate
        # No rotar si hay mantenimiento:
        [ -f /var/run/myapp-maintenance ] && exit 1

        # No rotar si el backup no se completó:
        [ -f /tmp/backup-in-progress ] && exit 1
    endscript
    postrotate
        systemctl reload myapp > /dev/null 2>&1 || true
    endscript
}

# Si prerotate sale con exit != 0, la rotación se ABORTA
# Útil para sincronizar rotación con backups o ventanas de mantenimiento
EOF
```

**Predicción:**

<details><summary>¿Qué pasa si prerotate falla?</summary>

La rotación **no se ejecuta** para ese archivo. El log sigue creciendo sin rotar. Esto es útil como mecanismo de seguridad (no rotar durante un backup, no rotar si el servicio está en mantenimiento), pero hay que tener cuidado: si el prerotate falla permanentemente, el log crecerá sin límite. Una buena práctica es combinar prerotate con `maxsize` como límite de seguridad.
</details>

### Ejercicio 10 — Config de producción completa

Diseña y valida una configuración de producción.

```bash
# Crear la config:
sudo mkdir -p /var/log/prodapp
echo "Log de producción $(date)" | sudo tee /var/log/prodapp/app.log

sudo tee /etc/logrotate.d/prodapp << 'EOF'
/var/log/prodapp/*.log {
    daily
    rotate 30
    maxage 90
    minsize 1M
    maxsize 500M

    compress
    delaycompress

    missingok
    notifempty

    create 0640 root root

    dateext
    dateformat -%Y%m%d

    sharedscripts
    postrotate
        echo "Rotación completada $(date)" >> /var/log/prodapp/rotation.log
    endscript
}
EOF

# Validar con dry run:
echo "=== Dry run ==="
sudo logrotate -d /etc/logrotate.d/prodapp 2>&1

# Limpiar:
sudo rm -rf /var/log/prodapp
sudo rm -f /etc/logrotate.d/prodapp
```

**Predicción:**

<details><summary>¿Por qué esta configuración es robusta para producción?</summary>

Combina múltiples capas de protección:
- **`daily` + `minsize 1M`:** no rotar archivos tiny (evitar rotaciones vacías)
- **`maxsize 500M`:** rotar inmediatamente si explota (circuit breaker)
- **`rotate 30` + `maxage 90`:** retención por conteo y edad (doble red)
- **`compress` + `delaycompress`:** ahorrar disco pero mantener ayer accesible
- **`missingok` + `notifempty`:** robusto ante archivos faltantes o vacíos
- **`dateext`:** nombres autoexplicativos
- **`sharedscripts`:** un solo reload
- **`create 0640`:** permisos explícitos

Lo que faltaría: `compresscmd /usr/bin/zstd` si zstd está disponible, y ajustar `create` al usuario real de la app.
</details>
