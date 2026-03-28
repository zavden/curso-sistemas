# Lab — Configuración de logrotate

## Objetivo

Entender logrotate: que es y como se invoca (systemd timer o
cron.daily), /etc/logrotate.conf (defaults globales),
/etc/logrotate.d/ (configuraciones por aplicacion), anatomia de
un bloque, patrones de archivo (wildcard, multiples), crear una
configuracion custom, dry run (-d), forzar rotacion (-f), verbose
(-v), estado en /var/lib/logrotate/status, flujo de rotacion paso
a paso, errores comunes, y diferencias Debian vs RHEL.

## Prerequisitos

- Entorno del curso levantado (`docker compose up -d`)

---

## Parte 1 — Config y estructura

### Objetivo

Entender la configuracion de logrotate y como se invoca.

### Paso 1.1: Que es logrotate

```bash
docker compose exec debian-dev bash -c '
echo "=== logrotate ==="
echo ""
echo "Gestiona rotacion, compresion y eliminacion de logs."
echo "Sin logrotate, los logs crecen hasta llenar el disco."
echo ""
echo "  Sin logrotate:"
echo "    /var/log/syslog → 50GB → disco lleno"
echo ""
echo "  Con logrotate:"
echo "    /var/log/syslog        (actual, 10MB)"
echo "    /var/log/syslog.1      (ayer, 10MB)"
echo "    /var/log/syslog.2.gz   (anteayer, 2MB)"
echo "    /var/log/syslog.3.gz   (hace 3 dias, 2MB)"
echo ""
echo "--- logrotate NO es un daemon ---"
echo "Se ejecuta periodicamente via systemd timer o cron."
echo ""
echo "--- Version ---"
logrotate --version 2>&1 | head -1
'
```

### Paso 1.2: Como se invoca

```bash
docker compose exec debian-dev bash -c '
echo "=== Invocacion de logrotate ==="
echo ""
echo "--- Buscar systemd timer ---"
if systemctl cat logrotate.timer &>/dev/null 2>&1; then
    echo "Invocado por systemd timer:"
    systemctl cat logrotate.timer 2>/dev/null | grep -E "OnCalendar|Persistent|AccuracySec"
    echo ""
    echo "Estado:"
    systemctl status logrotate.timer 2>/dev/null | grep -E "Active|Trigger" | head -2
else
    echo "  (sin timer de logrotate)"
fi
echo ""
echo "--- Buscar cron ---"
if [[ -f /etc/cron.daily/logrotate ]]; then
    echo "Invocado por cron.daily:"
    cat /etc/cron.daily/logrotate
else
    echo "  (sin cron.daily/logrotate)"
fi
'
```

### Paso 1.3: /etc/logrotate.conf

```bash
docker compose exec debian-dev bash -c '
echo "=== /etc/logrotate.conf ==="
echo ""
echo "Archivo principal con defaults globales:"
echo ""
cat /etc/logrotate.conf
'
```

### Paso 1.4: /etc/logrotate.d/

```bash
docker compose exec debian-dev bash -c '
echo "=== /etc/logrotate.d/ ==="
echo ""
echo "Cada aplicacion instala su propia config de rotacion."
echo "Los archivos se procesan en orden alfabetico."
echo ""
echo "--- Archivos instalados ---"
ls /etc/logrotate.d/ 2>/dev/null
echo ""
echo "--- Total ---"
ls /etc/logrotate.d/ 2>/dev/null | wc -l
echo ""
echo "--- Ejemplo: rsyslog ---"
echo ""
cat /etc/logrotate.d/rsyslog 2>/dev/null || echo "(no encontrado)"
'
```

### Paso 1.5: Anatomia de un bloque

```bash
docker compose exec debian-dev bash -c '
echo "=== Anatomia de un bloque ==="
echo ""
cat << '\''EOF'\''
/ruta/al/archivo.log {         # archivo(s) a rotar
    directiva1                  # configuracion especifica
    directiva2
    prerotate                   # script ANTES de rotar
        comando
    endscript
    postrotate                  # script DESPUES de rotar
        comando
    endscript
}
EOF
echo ""
echo "--- Patrones de archivo ---"
echo ""
echo "Archivo especifico:"
echo "  /var/log/myapp.log { ... }"
echo ""
echo "Con wildcard:"
echo "  /var/log/myapp/*.log { ... }"
echo ""
echo "Multiples archivos:"
echo "  /var/log/myapp.log"
echo "  /var/log/myapp-error.log"
echo "  { ... }"
'
```

---

## Parte 2 — Crear y probar

### Objetivo

Crear una configuracion y probar con dry run y forzar.

### Paso 2.1: Configuracion basica

```bash
docker compose exec debian-dev bash -c '
echo "=== Crear configuracion basica ==="
echo ""
cat << '\''EOF'\''
# /etc/logrotate.d/myapp
/var/log/myapp/*.log {
    daily
    rotate 7
    compress
    delaycompress
    missingok
    notifempty
    create 0640 myapp myapp
}
EOF
echo ""
echo "--- Directivas ---"
echo "  daily:          rotar cada dia"
echo "  rotate 7:       mantener 7 copias"
echo "  compress:       comprimir con gzip"
echo "  delaycompress:  no comprimir el mas reciente"
echo "  missingok:      no error si falta el archivo"
echo "  notifempty:     no rotar si esta vacio"
echo "  create:         crear nuevo con permisos especificos"
'
```

### Paso 2.2: Configuracion con postrotate

```bash
docker compose exec debian-dev bash -c '
echo "=== Con postrotate ==="
echo ""
cat << '\''EOF'\''
# /etc/logrotate.d/myapp
/var/log/myapp/app.log {
    daily
    rotate 30
    compress
    delaycompress
    missingok
    notifempty
    create 0640 myapp myapp
    sharedscripts
    postrotate
        systemctl reload myapp.service > /dev/null 2>&1 || true
    endscript
}
EOF
echo ""
echo "postrotate notifica a la app que reabra el archivo."
echo "sharedscripts ejecuta postrotate UNA vez (no por cada archivo)."
'
```

### Paso 2.3: Dry run

```bash
docker compose exec debian-dev bash -c '
echo "=== Dry run (-d) ==="
echo ""
echo "Ver que haria logrotate SIN hacer nada:"
echo ""
echo "--- Probar toda la config ---"
sudo logrotate -d /etc/logrotate.conf 2>&1 | head -30
echo "..."
echo ""
echo "--- Probar un archivo especifico ---"
echo "sudo logrotate -d /etc/logrotate.d/rsyslog"
echo ""
echo "logrotate -d tambien verifica la sintaxis."
echo "Si hay errores, los reporta."
'
```

### Paso 2.4: Forzar rotacion

```bash
docker compose exec debian-dev bash -c '
echo "=== Forzar rotacion (-f) ==="
echo ""
echo "Rotar inmediatamente (ignorar el periodo):"
echo ""
echo "sudo logrotate -f /etc/logrotate.d/myapp"
echo "  Rota aunque no haya pasado el periodo"
echo ""
echo "Con verbose:"
echo "sudo logrotate -fv /etc/logrotate.d/myapp"
echo "  -v muestra que hace paso a paso"
echo ""
echo "--- Probar con rsyslog ---"
sudo logrotate -fv /etc/logrotate.d/rsyslog 2>&1 | head -20 || \
    echo "(forzar rotacion requiere permisos)"
'
```

### Paso 2.5: Estado de logrotate

```bash
docker compose exec debian-dev bash -c '
echo "=== Estado de logrotate ==="
echo ""
echo "logrotate recuerda cuando roto cada archivo:"
echo ""
if [[ -f /var/lib/logrotate/status ]]; then
    echo "--- /var/lib/logrotate/status ---"
    head -15 /var/lib/logrotate/status
elif [[ -f /var/lib/logrotate.status ]]; then
    echo "--- /var/lib/logrotate.status ---"
    head -15 /var/lib/logrotate.status
else
    echo "(archivo de estado no encontrado)"
fi
echo ""
echo "--- Formato ---"
echo "  \"archivo\" YYYY-M-D-H:M:S"
echo ""
echo "Para forzar re-rotacion:"
echo "  1. Editar el archivo de estado y cambiar la fecha"
echo "  2. Eliminar la linea del archivo especifico"
echo "  3. Usar logrotate -f"
'
```

---

## Parte 3 — Flujo, errores y distros

### Objetivo

Entender el flujo de rotacion, errores comunes y diferencias.

### Paso 3.1: Flujo de rotacion paso a paso

```bash
docker compose exec debian-dev bash -c '
echo "=== Flujo de rotacion ==="
echo ""
echo "Con compress + delaycompress, rotate=3:"
echo ""
echo "--- Estado inicial ---"
echo "  myapp.log        (activo, 15MB)"
echo "  myapp.log.1      (ayer, sin comprimir)"
echo "  myapp.log.2.gz   (anteayer, comprimido)"
echo "  myapp.log.3.gz   (hace 3 dias)"
echo ""
echo "--- Paso 1: Eliminar el mas antiguo ---"
echo "  Se elimina myapp.log.3.gz (rotate=3)"
echo ""
echo "--- Paso 2: Renombrar en cascada ---"
echo "  myapp.log.2.gz → myapp.log.3.gz"
echo "  myapp.log.1    → myapp.log.2.gz  (ahora se comprime)"
echo "  myapp.log      → myapp.log.1     (delaycompress)"
echo ""
echo "--- Paso 3: Crear archivo nuevo (si create) ---"
echo "  Se crea myapp.log nuevo (vacio, con permisos de create)"
echo ""
echo "--- Paso 4: postrotate ---"
echo "  Se ejecuta el script postrotate"
echo ""
echo "--- Estado final ---"
echo "  myapp.log        (nuevo, vacio)"
echo "  myapp.log.1      (era activo, sin comprimir)"
echo "  myapp.log.2.gz   (comprimido)"
echo "  myapp.log.3.gz   (comprimido)"
echo ""
echo "--- Ver archivos rotados reales ---"
ls -lh /var/log/syslog* 2>/dev/null || \
    ls -lh /var/log/messages* 2>/dev/null || \
    echo "(sin archivos rotados)"
'
```

### Paso 3.2: create vs copytruncate

```bash
docker compose exec debian-dev bash -c '
echo "=== create vs copytruncate ==="
echo ""
echo "--- create (default) ---"
echo "  1. Renombrar myapp.log → myapp.log.1"
echo "  2. Crear myapp.log nuevo (vacio)"
echo "  3. La app NECESITA reabrir (postrotate reload)"
echo ""
echo "  Problema: la app puede tener file descriptor abierto"
echo "  al archivo viejo y seguir escribiendo en .1"
echo ""
echo "--- copytruncate ---"
echo "  1. Copiar myapp.log → myapp.log.1"
echo "  2. Truncar myapp.log (vaciar)"
echo "  3. La app sigue escribiendo en el mismo archivo"
echo ""
echo "  Ventaja: no necesita postrotate/reload"
echo "  Desventaja: puede perder lineas entre copy y truncate"
echo ""
echo "--- Cuando usar cada uno ---"
echo "  create:       apps que soportan reload (nginx, rsyslog)"
echo "  copytruncate: apps legacy que no reabren archivos"
echo ""
echo "  create y copytruncate son mutuamente excluyentes."
'
```

### Paso 3.3: Errores comunes

```bash
docker compose exec debian-dev bash -c '
echo "=== Errores comunes ==="
echo ""
echo "1. Archivo no existe"
echo "   Error: stat of /var/log/myapp.log failed"
echo "   Solucion: agregar missingok"
echo ""
echo "2. Permisos incorrectos despues de rotar"
echo "   La app no puede escribir en el nuevo archivo"
echo "   Solucion: create 0640 myapp myapp"
echo ""
echo "3. La app sigue escribiendo en el rotado"
echo "   La app tiene file descriptor abierto al .1"
echo "   Solucion: postrotate con reload, o copytruncate"
echo ""
echo "4. Permisos inseguros del directorio padre"
echo "   Error: skipping because parent directory has"
echo "          insecure permissions"
echo "   Solucion: chmod 755 /var/log/myapp/ o usar su"
echo ""
echo "--- Diagnosticar ---"
echo "  logrotate -dv /etc/logrotate.d/myapp"
echo "  journalctl -u logrotate --since today"
'
```

### Paso 3.4: Debian vs RHEL

```bash
docker compose exec debian-dev bash -c '
echo "=== Debian ==="
echo ""
echo "Defaults:"
grep -v "^#" /etc/logrotate.conf | grep -v "^$" | grep -v "^/" | head -5
echo ""
echo "Archivos rotados:"
ls /var/log/syslog* 2>/dev/null | head -5
echo ""
echo "Usa dateext:"
grep "dateext" /etc/logrotate.conf 2>/dev/null || echo "  no (numeros)"
'
echo ""
docker compose exec alma-dev bash -c '
echo "=== RHEL ==="
echo ""
echo "Defaults:"
grep -v "^#" /etc/logrotate.conf | grep -v "^$" | grep -v "^/" | head -5
echo ""
echo "Archivos rotados:"
ls /var/log/messages* 2>/dev/null | head -5
echo ""
echo "Usa dateext:"
grep "dateext" /etc/logrotate.conf 2>/dev/null || echo "  no"
'
```

---

## Limpieza final

No hay recursos que limpiar.

---

## Conceptos reforzados

1. logrotate no es un daemon. Se invoca periodicamente
   via systemd timer (logrotate.timer) o cron.daily.

2. /etc/logrotate.conf define defaults globales.
   /etc/logrotate.d/ contiene configs por aplicacion
   (modularidad, gestion de paquetes).

3. Dry run (logrotate -d) verifica sintaxis y muestra
   que haria sin hacer nada. Forzar (logrotate -f) rota
   inmediatamente ignorando el periodo.

4. El estado se guarda en /var/lib/logrotate/status con
   la fecha de ultima rotacion de cada archivo.

5. create renombra y crea nuevo (necesita postrotate).
   copytruncate copia y trunca (no necesita reload pero
   puede perder lineas). Son mutuamente excluyentes.

6. Debian no usa dateext por defecto (syslog.1, .2).
   RHEL usa dateext (messages-20260317). Los logs en
   Debian son syslog:adm, en RHEL root:root.
