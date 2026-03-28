# Lab — Directivas de logrotate

## Objetivo

Dominar las directivas de logrotate: frecuencia (daily, weekly,
size, minsize, maxsize), retencion (rotate, maxage), compresion
(compress, delaycompress, algoritmos alternativos), creacion de
archivo (create, copytruncate), nombrado (dateext, dateformat),
control (missingok, notifempty), scripts (prerotate, postrotate,
sharedscripts, firstaction, lastaction), directivas avanzadas (su,
tabooext), ejemplo completo de produccion, y diagnostico.

## Prerequisitos

- Entorno del curso levantado (`docker compose up -d`)

---

## Parte 1 — Frecuencia, retencion y compresion

### Objetivo

Controlar cuando rotar, cuantas copias mantener y como comprimir.

### Paso 1.1: Directivas de frecuencia

```bash
docker compose exec debian-dev bash -c '
echo "=== Frecuencia ==="
echo ""
echo "--- Por tiempo ---"
echo "  daily       cada dia"
echo "  weekly      cada semana"
echo "  monthly     cada mes"
echo "  yearly      cada ano"
echo ""
echo "--- Por tamano ---"
echo "  size 100M   rotar cuando alcance 100MB"
echo "  size 10k    10 kilobytes"
echo "  size 1G     1 gigabyte"
echo ""
echo "  size IGNORA la frecuencia"
echo "  Si size=100M y es daily, no rota hasta que llegue a 100M"
echo ""
echo "--- Combinacion tamano + tiempo ---"
echo "  maxsize 100M  rotar si llega a 100M AUNQUE no toque"
echo "  minsize 10M   NO rotar si tiene menos de 10M aunque toque"
echo ""
echo "--- Ejemplo ---"
cat << '\''EOF'\''
/var/log/myapp.log {
    daily
    minsize 1M      # no rotar si < 1MB (evitar rotaciones vacias)
    maxsize 500M    # rotar YA si llega a 500MB (no esperar al dia)
    rotate 30
}
EOF
echo ""
echo "--- Frecuencia en configs del sistema ---"
for f in /etc/logrotate.d/*; do
    FREQ=$(grep -m1 -oE "daily|weekly|monthly|yearly" "$f" 2>/dev/null)
    [[ -n "$FREQ" ]] && echo "  $(basename $f): $FREQ"
done
'
```

### Paso 1.2: Directivas de retencion

```bash
docker compose exec debian-dev bash -c '
echo "=== Retencion ==="
echo ""
echo "--- rotate N ---"
echo "  rotate 7     mantener 7 copias (+el actual)"
echo "  rotate 0     no mantener copias (eliminar al rotar)"
echo "  rotate 365   un ano de copias diarias"
echo ""
echo "--- maxage N ---"
echo "  maxage 30    eliminar copias de mas de 30 dias"
echo "  Independiente de rotate"
echo "  Util cuando la frecuencia es irregular"
echo ""
echo "--- Ejemplos ---"
cat << '\''EOF'\''
# Retencion por conteo:
/var/log/myapp.log {
    daily
    rotate 30
}

# Retencion por edad:
/var/log/myapp.log {
    daily
    rotate 999       # sin limite practico por conteo
    maxage 90        # pero eliminar todo > 90 dias
}
EOF
echo ""
echo "--- Retencion en el sistema ---"
grep -h "rotate " /etc/logrotate.conf /etc/logrotate.d/* 2>/dev/null | \
    grep -v "^#" | sort | uniq -c | sort -rn | head -5
'
```

### Paso 1.3: Directivas de compresion

```bash
docker compose exec debian-dev bash -c '
echo "=== Compresion ==="
echo ""
echo "--- compress / nocompress ---"
echo "  compress        comprimir con gzip (default: nocompress)"
echo "  nocompress       no comprimir"
echo ""
echo "--- delaycompress ---"
echo "  No comprimir el archivo recien rotado (.1)"
echo "  Comprimirlo en la siguiente rotacion (.2.gz)"
echo ""
echo "  Por que: util tener el log de ayer sin comprimir"
echo "  para acceso rapido. Ademas, algunos procesos pueden"
echo "  seguir escribiendo en .1 brevemente."
echo ""
echo "--- Algoritmos alternativos ---"
cat << '\''EOF'\''
# xz (mejor compresion, mas lento):
compresscmd /usr/bin/xz
uncompresscmd /usr/bin/unxz
compressext .xz
compressoptions -9

# zstd (rapido, buen ratio):
compresscmd /usr/bin/zstd
uncompresscmd /usr/bin/unzstd
compressext .zst
compressoptions -T0     # usar todos los cores
EOF
echo ""
echo "--- Resultado con compress + delaycompress ---"
echo "  myapp.log        (actual)"
echo "  myapp.log.1      (ayer, sin comprimir)"
echo "  myapp.log.2.gz   (anteayer, comprimido)"
echo "  myapp.log.3.gz   (hace 3 dias, comprimido)"
'
```

---

## Parte 2 — Creacion, nombrado y control

### Objetivo

Controlar como se crea el archivo nuevo y como se nombran los rotados.

### Paso 2.1: create

```bash
docker compose exec debian-dev bash -c '
echo "=== create ==="
echo ""
echo "Mecanismo por defecto:"
echo ""
echo "  create              crear archivo nuevo despues de rotar"
echo "  create 0640 www-data adm    con permisos, owner, group"
echo "  nocreate            no crear (la app lo creara)"
echo ""
echo "--- Flujo ---"
echo "  1. myapp.log → myapp.log.1 (rename)"
echo "  2. Se crea myapp.log nuevo (vacio)"
echo "  3. La app necesita reabrir (postrotate)"
echo ""
echo "--- Problema ---"
echo "  La app puede tener file descriptor abierto al viejo"
echo "  y seguir escribiendo en .1"
echo ""
echo "--- Solucion: postrotate ---"
cat << '\''EOF'\''
/var/log/myapp.log {
    create 0640 myapp myapp
    postrotate
        systemctl reload myapp > /dev/null 2>&1 || true
    endscript
}
EOF
'
```

### Paso 2.2: copytruncate

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
echo "  donde las lineas escritas se PIERDEN"
echo ""
echo "--- Cuando usar ---"
echo "  Apps legacy que abren el archivo una vez"
echo "  Apps de terceros sin control sobre su logging"
echo "  Apps que no soportan senales de reload"
echo ""
echo "--- create y copytruncate son MUTUAMENTE EXCLUYENTES ---"
echo "  create:       rename + create → necesita postrotate"
echo "  copytruncate: copy + truncate → no necesita postrotate"
'
```

### Paso 2.3: dateext

```bash
docker compose exec debian-dev bash -c '
echo "=== dateext ==="
echo ""
echo "Usar fecha en el nombre en lugar de numeros:"
echo ""
echo "--- Sin dateext (default Debian) ---"
echo "  myapp.log.1"
echo "  myapp.log.2.gz"
echo "  myapp.log.3.gz"
echo ""
echo "--- Con dateext ---"
echo "  myapp.log-20260317"
echo "  myapp.log-20260316.gz"
echo "  myapp.log-20260315.gz"
echo ""
echo "--- dateformat ---"
echo "  dateformat -%Y%m%d          default: -20260317"
echo "  dateformat -%Y-%m-%d        mas legible: -2026-03-17"
echo "  dateformat -%Y%m%d-%H%M%S   con hora (para maxsize)"
echo ""
echo "--- dateyesterday ---"
echo "  Nombrar con la fecha de ayer (no de la rotacion)"
echo ""
echo "--- Ver archivos rotados reales ---"
echo ""
echo "Este sistema usa:"
ls /var/log/syslog* /var/log/messages* 2>/dev/null | head -5
if ls /var/log/syslog-2* /var/log/messages-2* &>/dev/null 2>&1; then
    echo "  → dateext"
else
    echo "  → numeros"
fi
'
```

### Paso 2.4: Directivas de control

```bash
docker compose exec debian-dev bash -c '
echo "=== Directivas de control ==="
echo ""
echo "--- missingok / nomissingok ---"
echo "  missingok      ignorar si el archivo no existe"
echo "  nomissingok    reportar error (default)"
echo ""
echo "--- notifempty / ifempty ---"
echo "  notifempty     no rotar si esta vacio"
echo "  ifempty        rotar aunque este vacio (default)"
echo ""
echo "--- Buscar en configs del sistema ---"
echo ""
echo "Configs con missingok:"
grep -rl "missingok" /etc/logrotate.d/ 2>/dev/null | \
    while read f; do echo "  $(basename $f)"; done
echo ""
echo "Configs con notifempty:"
grep -rl "notifempty" /etc/logrotate.d/ 2>/dev/null | \
    while read f; do echo "  $(basename $f)"; done
'
```

---

## Parte 3 — Scripts y avanzadas

### Objetivo

Usar scripts de rotacion y directivas avanzadas.

### Paso 3.1: postrotate

```bash
docker compose exec debian-dev bash -c '
echo "=== postrotate ==="
echo ""
echo "Script que se ejecuta DESPUES de rotar:"
echo ""
echo "--- Ejemplos comunes ---"
echo ""
echo "nginx (USR1 para reabrir logs):"
echo "  [ -f /var/run/nginx.pid ] && kill -USR1 \$(cat /var/run/nginx.pid)"
echo ""
echo "rsyslog:"
echo "  /usr/lib/rsyslog/rsyslog-rotate"
echo ""
echo "Servicio systemd:"
echo "  systemctl reload myapp > /dev/null 2>&1 || true"
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

### Paso 3.2: sharedscripts

```bash
docker compose exec debian-dev bash -c '
echo "=== sharedscripts ==="
echo ""
echo "--- Sin sharedscripts ---"
echo "  /var/log/myapp/*.log {"
echo "      postrotate"
echo "          systemctl reload myapp  # se ejecuta N veces"
echo "      endscript"
echo "  }"
echo ""
echo "--- Con sharedscripts ---"
echo "  /var/log/myapp/*.log {"
echo "      sharedscripts"
echo "      postrotate"
echo "          systemctl reload myapp  # se ejecuta 1 vez"
echo "      endscript"
echo "  }"
echo ""
echo "IMPORTANTE: con sharedscripts, si ningun archivo necesita"
echo "rotacion, postrotate NO se ejecuta."
echo ""
echo "--- firstaction / lastaction ---"
cat << '\''EOF'\''
/var/log/myapp/*.log {
    sharedscripts
    firstaction
        logger "logrotate: iniciando rotacion de myapp"
    endscript
    postrotate
        systemctl reload myapp > /dev/null 2>&1 || true
    endscript
    lastaction
        logger "logrotate: rotacion de myapp completada"
    endscript
}
EOF
echo ""
echo "firstaction: UNA vez ANTES de procesar cualquier archivo"
echo "lastaction: UNA vez DESPUES de procesar todos los archivos"
'
```

### Paso 3.3: Directiva su

```bash
docker compose exec debian-dev bash -c '
echo "=== Directiva su ==="
echo ""
echo "Si los logs son de un usuario no-root:"
echo ""
cat << '\''EOF'\''
/var/log/myapp/*.log {
    su myapp myapp
    create 0640 myapp myapp
    ...
}
EOF
echo ""
echo "Ejecuta la rotacion como myapp:myapp."
echo "Necesario cuando /var/log/myapp/ es propiedad de myapp."
echo ""
echo "Sin su, logrotate puede fallar con:"
echo "  \"skipping because parent directory has insecure permissions\""
'
```

### Paso 3.4: Ejemplo de produccion

```bash
docker compose exec debian-dev bash -c '
echo "=== Ejemplo completo de produccion ==="
echo ""
cat << '\''EOF'\''
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
EOF
'
```

### Paso 3.5: Tabla de directivas

```bash
docker compose exec debian-dev bash -c '
echo "=== Tabla de directivas ==="
echo ""
echo "| Directiva        | Descripcion                     | Default      |"
echo "|------------------|---------------------------------|--------------|"
echo "| daily/weekly/... | Frecuencia de rotacion          | weekly       |"
echo "| rotate N         | Copias a mantener               | 4            |"
echo "| size N           | Rotar por tamano                | (no usado)   |"
echo "| minsize N        | No rotar si menor que N         | (no usado)   |"
echo "| maxsize N        | Rotar si mayor que N            | (no usado)   |"
echo "| maxage N         | Eliminar copias > N dias        | (no usado)   |"
echo "| compress         | Comprimir rotados               | nocompress   |"
echo "| delaycompress    | No comprimir el mas reciente    | (no usado)   |"
echo "| create m o g     | Crear nuevo con permisos        | create       |"
echo "| copytruncate     | Copiar y truncar                | (no usado)   |"
echo "| dateext          | Fecha en el nombre              | nodateext    |"
echo "| missingok        | Ignorar si no existe            | nomissingok  |"
echo "| notifempty       | No rotar si vacio               | ifempty      |"
echo "| sharedscripts    | Scripts una vez por bloque      | noshared     |"
echo "| su user group    | Ejecutar como otro usuario      | (root)       |"
'
```

### Paso 3.6: Diagnostico

```bash
docker compose exec debian-dev bash -c '
echo "=== Diagnostico ==="
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
echo "--- Errores recientes en este sistema ---"
journalctl -u logrotate --since today --no-pager 2>/dev/null | tail -5 || \
    echo "(sin logs de logrotate)"
echo ""
echo "--- Probar la config completa ---"
sudo logrotate -d /etc/logrotate.conf 2>&1 | \
    grep -E "error|warning|considering|rotating" | head -10
'
```

---

## Limpieza final

No hay recursos que limpiar.

---

## Conceptos reforzados

1. daily/weekly/monthly controlan cuando rotar. size rota por
   tamano (ignora frecuencia). minsize y maxsize combinan
   tamano con frecuencia.

2. rotate N mantiene N copias. maxage N elimina por edad.
   compress con delaycompress es el patron estandar.

3. create renombra y crea nuevo (necesita postrotate reload).
   copytruncate copia y trunca (no necesita reload pero puede
   perder lineas). Son mutuamente excluyentes.

4. dateext usa fecha en el nombre (-20260317). dateformat
   con hora (-%Y%m%d-%H%M%S) evita colisiones con maxsize.

5. sharedscripts ejecuta postrotate una vez por bloque
   (no por archivo). firstaction/lastaction se ejecutan
   al inicio y fin de todo el procesamiento.

6. su permite rotar como un usuario no-root. Necesario
   cuando el directorio de logs es propiedad de la app.
