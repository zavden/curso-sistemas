# T01 — Configuración de logrotate

> **Objetivo:** Entender cómo logrotate gestiona la rotación de logs, dominar su configuración global y por aplicación, y saber diagnosticar problemas comunes.

## Erratas detectadas en el material fuente

| Archivo | Línea | Error | Corrección |
|---------|-------|-------|------------|
| README.max.md | 489 | `copytruncate ( альтернатива)` — palabra en ruso "альтернатива" (alternativa) | Debe ser `copytruncate (alternativa)` |
| README.max.md | 317 | `app que no.reload` — texto truncado/garbled | Debe ser "app que no soporta reload" |

---

## Qué es logrotate

**logrotate** gestiona la **rotación, compresión y eliminación** de archivos de log. Sin rotación, los logs crecen indefinidamente hasta llenar el disco.

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                    SIN LOGROTATE                                            │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                             │
│   Día 1:    /var/log/syslog        10 MB                                    │
│   Día 10:   /var/log/syslog        100 MB                                   │
│   Día 30:   /var/log/syslog        300 MB                                   │
│   Día 60:   /var/log/syslog        600 MB → DISCO LLENO → sistema caído    │
│                                                                             │
└─────────────────────────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────────────────────────┐
│                    CON LOGROTATE                                            │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                             │
│   /var/log/syslog          (actual, 10MB)                                   │
│   /var/log/syslog.1        (ayer, 10MB)                                     │
│   /var/log/syslog.2.gz     (anteayer, 2MB comprimido)                       │
│   /var/log/syslog.3.gz     (hace 3 días, 2MB)                              │
│   /var/log/syslog.4.gz     (hace 4 días, 2MB)                              │
│   → se elimina syslog.5.gz (rotate=4, se guardan solo 4 versiones)         │
│                                                                             │
└─────────────────────────────────────────────────────────────────────────────┘
```

```bash
# logrotate NO es un daemon — se ejecuta periódicamente via:
# - systemd timer (Debian 10+, Fedora)
# - cron.daily (sistemas legacy)

# Ver cómo se invoca:
systemctl cat logrotate.timer 2>/dev/null    # systemd timer
cat /etc/cron.daily/logrotate 2>/dev/null    # cron

# Versión:
logrotate --version
```

---

## Cómo se invoca logrotate

### Con systemd timer (Debian moderno, Fedora)

```bash
systemctl cat logrotate.timer
# [Timer]
# OnCalendar=daily
# AccuracySec=12h        # puede ejecutarse hasta 12h después
# Persistent=true        # ejecutar si el sistema estuvo apagado

systemctl cat logrotate.service
# [Service]
# Type=oneshot          # se ejecuta y termina
# ExecStart=/usr/sbin/logrotate /etc/logrotate.conf

# Ver cuándo se ejecutó:
systemctl status logrotate.timer
# Trigger: Wed 2026-03-18 00:00:00
# Last:    Tue 2026-03-17 06:30:00
```

### Con cron (sistemas legacy)

```bash
cat /etc/cron.daily/logrotate
# #!/bin/sh
# /usr/sbin/logrotate /etc/logrotate.conf
# EXITVALUE=$?
# if [ $EXITVALUE != 0 ]; then
#     /usr/bin/logger -t logrotate "ALERT exited abnormally with [$EXITVALUE]"
# fi
```

---

## /etc/logrotate.conf

El archivo principal define defaults globales y luego incluye configuraciones específicas por aplicación:

### Defaults globales

```bash
# Debian (típico):
weekly              # rotación semanal
rotate 4            # mantener 4 versiones
create              # crear archivo nuevo tras rotar
#compress           # comentado por defecto en Debian

include /etc/logrotate.d

# RHEL (típico):
weekly
rotate 4
create
dateext             # usar fecha en el nombre: syslog-20260317
#compress

include /etc/logrotate.d
```

### Entradas especiales (wtmp, btmp)

```bash
# Estas entradas tienen configuración inline en logrotate.conf:
/var/log/wtmp {
    monthly          # rotación mensual
    create 0664 root utmp   # permisos especiales
    minsize 1M       # solo rotar si > 1MB
    rotate 1         # solo 1 versión
}

/var/log/btmp {
    missingok        # no error si no existe
    monthly
    create 0600 root utmp
    rotate 1
}
```

### Diferencias Debian vs RHEL

| Aspecto | Debian/Ubuntu | RHEL/Fedora |
|---------|---------------|-------------|
| Nomenclatura | `syslog.1`, `syslog.2` | `syslog-20260317` (dateext) |
| Compress default | Comentado | Comentado |
| Permisos logs | `syslog:adm` | `root:root` |
| Ubicación estado | `/var/lib/logrotate/status` | `/var/lib/logrotate/status` |

---

## /etc/logrotate.d/

Cada aplicación instala su propia configuración de rotación:

```bash
ls /etc/logrotate.d/
# alternatives
# apt
# dpkg
# nginx
# rsyslog
# ufw
# ...

# Se procesan en orden alfabético (por el include)
```

### Ejemplo: /etc/logrotate.d/rsyslog (Debian)

```bash
/var/log/syslog
/var/log/mail.info
/var/log/mail.warn
/var/log/mail.err
/var/log/mail.log
/var/log/daemon.log
/var/log/kern.log
/var/log/auth.log
/var/log/user.log
/var/log/lpr.log
/var/log/cron.log
/var/log/debug
/var/log/messages {
    rotate 4
    weekly
    missingok
    notifempty
    compress
    delaycompress
    sharedscripts
    postrotate
        /usr/lib/rsyslog/rsyslog-rotate
    endscript
}
```

### Ejemplo: /etc/logrotate.d/nginx

```bash
/var/log/nginx/*.log {
    daily
    missingok
    rotate 14
    compress
    delaycompress
    notifempty
    create 0640 www-data adm
    sharedscripts
    prerotate
        if [ -d /etc/logrotate.d/httpd-prerotate ]; then
            run-parts /etc/logrotate.d/httpd-prerotate
        fi
    endscript
    postrotate
        invoke-rc.d nginx rotate >/dev/null 2>&1
    endscript
}
```

---

## Anatomía de un bloque de configuración

```bash
/ruta/al/archivo.log {        # archivo(s) a rotar
    directiva1                  # configuración
    directiva2

    prerotate                   # script ANTES de renombrar
        comando1
        comando2
    endscript

    postrotate                  # script DESPUÉS de rotar
        comando1
    endscript
}
```

### Patrones de archivo

```bash
# Archivo específico:
/var/log/myapp.log { ... }

# Con wildcard:
/var/log/myapp/*.log { ... }

# Múltiples archivos en un bloque:
/var/log/myapp.log
/var/log/myapp-error.log
/var/log/myapp-access.log {
    ...
}

# Múltiples con wildcards:
/var/log/myapp/*.log
/var/log/myapp-extra/*.log {
    ...
}
```

---

## Directivas principales

| Directiva | Descripción |
|-----------|-------------|
| `daily` / `weekly` / `monthly` | Frecuencia de rotación |
| `rotate N` | Número de versiones a mantener |
| `compress` | Comprimir versiones antiguas (gzip) |
| `delaycompress` | No comprimir inmediatamente (dejar `.1` sin comprimir) |
| `create MODE OWNER GROUP` | Permisos del nuevo archivo |
| `missingok` | No error si el archivo no existe |
| `notifempty` | No rotar si el archivo está vacío |
| `copytruncate` | Copiar y truncar (para apps que no soportan reload) |
| `sharedscripts` | Ejecutar scripts solo una vez (no por archivo) |
| `minsize N` | Solo rotar si el archivo es mayor a N |
| `maxsize N` | Rotar inmediatamente si supera N |
| `dateext` | Usar fecha en el nombre (`syslog-20260317`) |
| `dateyesterday` | Usar fecha de ayer (para dateext) |

---

## Crear configuración para tu aplicación

### Ejemplo básico

```bash
sudo tee /etc/logrotate.d/myapp << 'EOF'
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
```

### Ejemplo con postrotate (recargar servicio)

```bash
sudo tee /etc/logrotate.d/myapp << 'EOF'
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
```

### Ejemplo con copytruncate (app que no soporta reload)

```bash
# copytruncate = copia contenido, luego trunca el original
# Útil para aplicaciones que no pueden responder a signals

sudo tee /etc/logrotate.d/myapp << 'EOF'
/var/log/myapp/app.log {
    daily
    rotate 14
    compress
    delaycompress
    missingok
    notifempty
    copytruncate
}
EOF
```

---

## Flujo de rotación paso a paso

```
ESTADO INICIAL (rotate=3, compress, delaycompress):
  /var/log/myapp.log        (15MB, activo)
  /var/log/myapp.log.1      (rotado ayer, SIN comprimir - delaycompress)
  /var/log/myapp.log.2.gz   (comprimido)
  /var/log/myapp.log.3.gz   (comprimido)

═══════════════════════════════════════════════════════════════

PASO 1: Eliminar el más antiguo
  rm /var/log/myapp.log.3.gz

PASO 2: Cascada de renombrado
  mv myapp.log.2.gz  →  myapp.log.3.gz
  mv myapp.log.1     →  myapp.log.2.gz   (AHORA se comprime)
  mv myapp.log       →  myapp.log.1       (no se comprime - delaycompress)

PASO 3: Crear nuevo archivo (si create)
  touch /var/log/myapp.log
  chown myapp:myapp /var/log/myapp.log
  chmod 0640 /var/log/myapp.log

PASO 4: Ejecutar postrotate
  systemctl reload myapp.service

═══════════════════════════════════════════════════════════════

ESTADO FINAL:
  /var/log/myapp.log        (0 bytes, nuevo)
  /var/log/myapp.log.1      (15MB, SIN comprimir - listo para next rotation)
  /var/log/myapp.log.2.gz   (comprimido)
  /var/log/myapp.log.3.gz   (comprimido)
```

---

## Probar y verificar

### Dry run (sin ejecutar)

```bash
# Ver qué haría sin hacer nada:
sudo logrotate -d /etc/logrotate.conf
# -d = debug (dry run)
# Muestra cada paso que ejecutaría

# Probar solo un archivo:
sudo logrotate -d /etc/logrotate.d/myapp
```

### Forzar rotación

```bash
# Forzar rotación inmediata (ignorar período):
sudo logrotate -f /etc/logrotate.conf

# Forzar solo un archivo:
sudo logrotate -f /etc/logrotate.d/myapp

# Con verbose:
sudo logrotate -fv /etc/logrotate.d/myapp
```

### Verificar sintaxis

```bash
sudo logrotate -d /etc/logrotate.d/myapp 2>&1 | head -20

# Errores comunes:
# "error: bad rotation count"      → rotate tiene valor inválido
# "error: myapp:3 unknown option"  → directiva no reconocida
# "error: stat of ... failed"      → archivo no existe (sin missingok)
```

---

## Estado de logrotate

```bash
# logrotate recuerda cuándo rotó cada archivo:
cat /var/lib/logrotate/status
# O:
cat /var/lib/logrotate.status    # ubicación alternativa

# Contenido:
# logrotate state -- version 2
# "/var/log/syslog" 2026-3-17-6:30:0
# "/var/log/auth.log" 2026-3-17-6:30:0
# "/var/log/nginx/access.log" 2026-3-18-0:0:0

# Para forzar re-rotación:
# Opción 1: editar fecha en el archivo de estado
# Opción 2: eliminar línea del archivo específico
# Opción 3: usar logrotate -f
```

---

## Errores comunes y soluciones

### Error: stat of /var/log/myapp.log failed

```
CAUSA: El archivo no existe y no tiene missingok
SOLUCIÓN:
  /var/log/myapp.log {
      missingok    # no reportar error si no existe
  }
```

### Permisos incorrectos después de rotar

```
CAUSA: create no especifica los permisos correctos
       La aplicación no puede escribir en el nuevo archivo

SOLUCIÓN:
  create 0640 www-data adm
  # modo owner group
```

### La app sigue escribiendo en el archivo rotado

```
CAUSA: Después de renombrar myapp.log → myapp.log.1,
       la app tiene fd abierto al archivo viejo

SOLUCIONES:
  1. postrotate: recargar/reiniciar el servicio
     postrotate
         systemctl reload myapp
     endscript

  2. copytruncate: copiar contenido y truncar
     copytruncate
     # ADVERTENCIA: puede perder líneas entre copy y truncate
```

### Permisos inseguros del directorio padre

```
CAUSA: El directorio tiene permisos demasiado abiertos
       Error: "skipping because parent directory has insecure permissions"

SOLUCIÓN: chmod 755 /var/log/myapp/ o usar su/su en la directiva
```

### copytruncate vs postrotate

```
┌─────────────────────────────────────────────────────────────────────────────┐
│  postrotate (RECOMENDADO)                                                   │
│  ─────────────────────────────────                                          │
│  1. mv myapp.log → myapp.log.1                                             │
│  2. touch myapp.log (nuevo)                                                 │
│  3. postrotate: recargar servicio                                           │
│  4. App abre nuevo fd hacia myapp.log                                       │
│  + No hay pérdida de logs                                                   │
│  + La app debe soportar signals o reload                                    │
├─────────────────────────────────────────────────────────────────────────────┤
│  copytruncate (alternativa)                                                 │
│  ─────────────────────────────────                                          │
│  1. cp myapp.log → myapp.log.1                                              │
│  2. truncate myapp.log (vaciar original)                                    │
│  3. App sigue escribiendo en el mismo fd (truncado)                         │
│  - PUEDE perder logs (entre copy y truncate)                                │
│  + Funciona con cualquier app (no necesita reload)                          │
└─────────────────────────────────────────────────────────────────────────────┘
```

**`create` y `copytruncate` son mutuamente excluyentes.** Usar ambos es un error.

---

## Quick reference

```
FRECUENCIA:
  daily    weekly    monthly
  minsize  maxsize

RETENCIÓN:
  rotate N       — mantener N versiones
  compress       — comprimir (.gz)
  delaycompress  — comprimir .1 al siguiente ciclo

ARCHIVO:
  create MODE OWNER GROUP
  copytruncate
  missingok       — no error si no existe
  notifempty      — no rotar si vacío

SCRIPTS:
  sharedscripts   — ejecutar scripts 1 vez (no por archivo)
  prerotate ... endscript
  postrotate ... endscript

NOMENCLATURA:
  dateext         — syslog-20260317 en vez de syslog.1

DIAGNÓSTICO:
  logrotate -d    — dry run (ver qué haría)
  logrotate -f    — forzar rotación
  logrotate -fv   — forzar con verbose
  /var/lib/logrotate/status — estado de rotaciones
```

---

## Labs

### Lab Parte 1 — Config y estructura

**Paso 1.1: Qué es logrotate**

```bash
docker compose exec debian-dev bash -c '
echo "=== logrotate ==="
echo ""
echo "Gestiona rotación, compresión y eliminación de logs."
echo "Sin logrotate, los logs crecen hasta llenar el disco."
echo ""
echo "  Sin logrotate:"
echo "    /var/log/syslog → 50GB → disco lleno"
echo ""
echo "  Con logrotate:"
echo "    /var/log/syslog        (actual, 10MB)"
echo "    /var/log/syslog.1      (ayer, 10MB)"
echo "    /var/log/syslog.2.gz   (anteayer, 2MB)"
echo "    /var/log/syslog.3.gz   (hace 3 días, 2MB)"
echo ""
echo "--- logrotate NO es un daemon ---"
echo "Se ejecuta periódicamente via systemd timer o cron."
echo ""
echo "--- Versión ---"
logrotate --version 2>&1 | head -1
'
```

**Paso 1.2: Cómo se invoca**

```bash
docker compose exec debian-dev bash -c '
echo "=== Invocación de logrotate ==="
echo ""
echo "--- Buscar systemd timer ---"
if systemctl cat logrotate.timer >/dev/null 2>&1; then
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

**Paso 1.3: /etc/logrotate.conf**

```bash
docker compose exec debian-dev bash -c '
echo "=== /etc/logrotate.conf ==="
echo ""
echo "Archivo principal con defaults globales:"
echo ""
cat /etc/logrotate.conf
'
```

**Paso 1.4: /etc/logrotate.d/**

```bash
docker compose exec debian-dev bash -c '
echo "=== /etc/logrotate.d/ ==="
echo ""
echo "Cada aplicación instala su propia config de rotación."
echo "Los archivos se procesan en orden alfabético."
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

**Paso 1.5: Anatomía de un bloque**

```bash
docker compose exec debian-dev bash -c '
echo "=== Anatomía de un bloque ==="
echo ""
echo "/ruta/al/archivo.log {         # archivo(s) a rotar"
echo "    directiva1                  # configuración específica"
echo "    directiva2"
echo "    prerotate                   # script ANTES de rotar"
echo "        comando"
echo "    endscript"
echo "    postrotate                  # script DESPUÉS de rotar"
echo "        comando"
echo "    endscript"
echo "}"
echo ""
echo "--- Patrones de archivo ---"
echo ""
echo "Archivo específico:"
echo "  /var/log/myapp.log { ... }"
echo ""
echo "Con wildcard:"
echo "  /var/log/myapp/*.log { ... }"
echo ""
echo "Múltiples archivos:"
echo "  /var/log/myapp.log"
echo "  /var/log/myapp-error.log"
echo "  { ... }"
'
```

### Lab Parte 2 — Crear y probar

**Paso 2.1: Configuración básica**

```bash
docker compose exec debian-dev bash -c '
echo "=== Crear configuración básica ==="
echo ""
echo "# /etc/logrotate.d/myapp"
echo "/var/log/myapp/*.log {"
echo "    daily"
echo "    rotate 7"
echo "    compress"
echo "    delaycompress"
echo "    missingok"
echo "    notifempty"
echo "    create 0640 myapp myapp"
echo "}"
echo ""
echo "--- Directivas ---"
echo "  daily:          rotar cada día"
echo "  rotate 7:       mantener 7 copias"
echo "  compress:       comprimir con gzip"
echo "  delaycompress:  no comprimir el más reciente"
echo "  missingok:      no error si falta el archivo"
echo "  notifempty:     no rotar si está vacío"
echo "  create:         crear nuevo con permisos específicos"
'
```

**Paso 2.2: Configuración con postrotate**

```bash
docker compose exec debian-dev bash -c '
echo "=== Con postrotate ==="
echo ""
echo "# /etc/logrotate.d/myapp"
echo "/var/log/myapp/app.log {"
echo "    daily"
echo "    rotate 30"
echo "    compress"
echo "    delaycompress"
echo "    missingok"
echo "    notifempty"
echo "    create 0640 myapp myapp"
echo "    sharedscripts"
echo "    postrotate"
echo "        systemctl reload myapp.service > /dev/null 2>&1 || true"
echo "    endscript"
echo "}"
echo ""
echo "postrotate notifica a la app que reabra el archivo."
echo "sharedscripts ejecuta postrotate UNA vez (no por cada archivo)."
'
```

**Paso 2.3: Dry run**

```bash
docker compose exec debian-dev bash -c '
echo "=== Dry run (-d) ==="
echo ""
echo "Ver qué haría logrotate SIN hacer nada:"
echo ""
echo "--- Probar toda la config ---"
sudo logrotate -d /etc/logrotate.conf 2>&1 | head -30
echo "..."
echo ""
echo "--- Probar un archivo específico ---"
echo "sudo logrotate -d /etc/logrotate.d/rsyslog"
echo ""
echo "logrotate -d también verifica la sintaxis."
echo "Si hay errores, los reporta."
'
```

**Paso 2.4: Forzar rotación**

```bash
docker compose exec debian-dev bash -c '
echo "=== Forzar rotación (-f) ==="
echo ""
echo "Rotar inmediatamente (ignorar el período):"
echo ""
echo "sudo logrotate -f /etc/logrotate.d/myapp"
echo "  Rota aunque no haya pasado el período"
echo ""
echo "Con verbose:"
echo "sudo logrotate -fv /etc/logrotate.d/myapp"
echo "  -v muestra qué hace paso a paso"
echo ""
echo "--- Probar con rsyslog ---"
sudo logrotate -fv /etc/logrotate.d/rsyslog 2>&1 | head -20 || \
    echo "(forzar rotación requiere permisos)"
'
```

**Paso 2.5: Estado de logrotate**

```bash
docker compose exec debian-dev bash -c '
echo "=== Estado de logrotate ==="
echo ""
echo "logrotate recuerda cuándo rotó cada archivo:"
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
echo "Para forzar re-rotación:"
echo "  1. Editar el archivo de estado y cambiar la fecha"
echo "  2. Eliminar la línea del archivo específico"
echo "  3. Usar logrotate -f"
'
```

### Lab Parte 3 — Flujo, errores y distros

**Paso 3.1: Flujo de rotación paso a paso**

```bash
docker compose exec debian-dev bash -c '
echo "=== Flujo de rotación ==="
echo ""
echo "Con compress + delaycompress, rotate=3:"
echo ""
echo "--- Estado inicial ---"
echo "  myapp.log        (activo, 15MB)"
echo "  myapp.log.1      (ayer, sin comprimir)"
echo "  myapp.log.2.gz   (anteayer, comprimido)"
echo "  myapp.log.3.gz   (hace 3 días)"
echo ""
echo "--- Paso 1: Eliminar el más antiguo ---"
echo "  Se elimina myapp.log.3.gz (rotate=3)"
echo ""
echo "--- Paso 2: Renombrar en cascada ---"
echo "  myapp.log.2.gz → myapp.log.3.gz"
echo "  myapp.log.1    → myapp.log.2.gz  (ahora se comprime)"
echo "  myapp.log      → myapp.log.1     (delaycompress)"
echo ""
echo "--- Paso 3: Crear archivo nuevo (si create) ---"
echo "  Se crea myapp.log nuevo (vacío, con permisos de create)"
echo ""
echo "--- Paso 4: postrotate ---"
echo "  Se ejecuta el script postrotate"
echo ""
echo "--- Estado final ---"
echo "  myapp.log        (nuevo, vacío)"
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

**Paso 3.2: create vs copytruncate**

```bash
docker compose exec debian-dev bash -c '
echo "=== create vs copytruncate ==="
echo ""
echo "--- create (default) ---"
echo "  1. Renombrar myapp.log → myapp.log.1"
echo "  2. Crear myapp.log nuevo (vacío)"
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
echo "  Desventaja: puede perder líneas entre copy y truncate"
echo ""
echo "--- Cuándo usar cada uno ---"
echo "  create:       apps que soportan reload (nginx, rsyslog)"
echo "  copytruncate: apps legacy que no reabren archivos"
echo ""
echo "  create y copytruncate son mutuamente excluyentes."
'
```

**Paso 3.3: Errores comunes**

```bash
docker compose exec debian-dev bash -c '
echo "=== Errores comunes ==="
echo ""
echo "1. Archivo no existe"
echo "   Error: stat of /var/log/myapp.log failed"
echo "   Solución: agregar missingok"
echo ""
echo "2. Permisos incorrectos después de rotar"
echo "   La app no puede escribir en el nuevo archivo"
echo "   Solución: create 0640 myapp myapp"
echo ""
echo "3. La app sigue escribiendo en el rotado"
echo "   La app tiene file descriptor abierto al .1"
echo "   Solución: postrotate con reload, o copytruncate"
echo ""
echo "4. Permisos inseguros del directorio padre"
echo "   Error: skipping because parent directory has"
echo "          insecure permissions"
echo "   Solución: chmod 755 /var/log/myapp/"
echo ""
echo "--- Diagnosticar ---"
echo "  logrotate -dv /etc/logrotate.d/myapp"
echo "  journalctl -u logrotate --since today"
'
```

**Paso 3.4: Debian vs RHEL**

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
grep "dateext" /etc/logrotate.conf 2>/dev/null || echo "  no (números)"
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

## Ejercicios

### Ejercicio 1 — Explorar la configuración global

Examina la configuración de logrotate de tu sistema.

```bash
echo "=== Defaults globales ==="
grep -v '^#' /etc/logrotate.conf | grep -v '^$' | head -10

echo ""
echo "=== Archivos en logrotate.d ==="
ls /etc/logrotate.d/ | wc -l
ls /etc/logrotate.d/

echo ""
echo "=== Invocación ==="
systemctl status logrotate.timer 2>/dev/null || \
    cat /etc/cron.daily/logrotate 2>/dev/null || \
    echo "No encontrado"
```

**Predicción:**

<details><summary>¿Qué defaults globales esperas ver?</summary>

En Debian: `weekly`, `rotate 4`, `create`, `include /etc/logrotate.d`. `compress` estará comentado. En RHEL, además aparecerá `dateext`. Cada paquete instalado (rsyslog, apt, dpkg, etc.) tendrá su propio archivo en `/etc/logrotate.d/`.
</details>

### Ejercicio 2 — Estado de rotación

Consulta cuándo se rotaron los logs por última vez.

```bash
echo "=== Estado de logrotate ==="
if [[ -f /var/lib/logrotate/status ]]; then
    head -15 /var/lib/logrotate/status
elif [[ -f /var/lib/logrotate.status ]]; then
    head -15 /var/lib/logrotate.status
else
    echo "Archivo de estado no encontrado"
fi

echo ""
echo "=== Archivos rotados de syslog ==="
ls -lh /var/log/syslog* 2>/dev/null || ls -lh /var/log/messages* 2>/dev/null
```

**Predicción:**

<details><summary>¿Qué formato tiene el archivo de estado?</summary>

El archivo comienza con `logrotate state -- version 2`, seguido de líneas con el formato `"ruta/al/archivo" YYYY-M-D-H:M:S`. Cada línea registra la última vez que se rotó ese archivo. Si un archivo no aparece, significa que nunca ha sido rotado (o que el estado se limpió).
</details>

### Ejercicio 3 — Dry run

Simula una rotación y analiza qué haría logrotate.

```bash
echo "=== Dry run de rsyslog ==="
sudo logrotate -d /etc/logrotate.d/rsyslog 2>&1 | head -40
```

**Predicción:**

<details><summary>¿Qué muestra el dry run?</summary>

Muestra cada archivo que evaluaría, si cumple las condiciones para rotar (tamaño, tiempo, vacío), y qué acciones tomaría: renombrar, comprimir, crear nuevo, ejecutar postrotate. Con `-d` no ejecuta nada, solo simula. También reporta errores de sintaxis si los hay. Los archivos que no necesitan rotación se marcan como "does not need rotating".
</details>

### Ejercicio 4 — Analizar configuración de rsyslog

Lee y entiende la configuración de rotación de rsyslog.

```bash
echo "=== /etc/logrotate.d/rsyslog ==="
cat /etc/logrotate.d/rsyslog 2>/dev/null || echo "No encontrado"
```

**Predicción:**

<details><summary>¿Por qué usa sharedscripts y postrotate?</summary>

`sharedscripts` hace que el script postrotate se ejecute **una sola vez** después de rotar todos los archivos del bloque (no una vez por archivo). Sin `sharedscripts`, si el bloque tiene 12 archivos, postrotate se ejecutaría 12 veces. El postrotate llama a `/usr/lib/rsyslog/rsyslog-rotate` que envía una señal a rsyslog para que reabra los archivos de log (equivalente a `kill -HUP`). Esto es necesario porque rsyslog mantiene file descriptors abiertos.
</details>

### Ejercicio 5 — Crear una configuración custom

Crea una configuración de logrotate para una aplicación ficticia.

```bash
# Crear directorio y archivo de prueba:
sudo mkdir -p /var/log/testapp
echo "Log de prueba $(date)" | sudo tee /var/log/testapp/app.log

# Crear configuración:
sudo tee /etc/logrotate.d/testapp << 'EOF'
/var/log/testapp/*.log {
    daily
    rotate 5
    compress
    delaycompress
    missingok
    notifempty
    create 0640 root root
}
EOF

# Verificar sintaxis con dry run:
sudo logrotate -d /etc/logrotate.d/testapp 2>&1
```

**Predicción:**

<details><summary>¿Qué dirá el dry run sobre este archivo?</summary>

Mostrará que encontró `/var/log/testapp/app.log`, evaluará si necesita rotación (probablemente "does not need rotating" si se acaba de crear y la frecuencia es `daily`). Si la sintaxis es correcta, no habrá errores. El dry run no modifica ningún archivo, solo muestra lo que haría.
</details>

### Ejercicio 6 — Forzar rotación

Fuerza la rotación del archivo creado en el ejercicio anterior.

```bash
# Añadir contenido para que no esté vacío (notifempty):
for i in $(seq 1 100); do
    echo "Línea de log $i — $(date)" | sudo tee -a /var/log/testapp/app.log > /dev/null
done

# Forzar rotación con verbose:
sudo logrotate -fv /etc/logrotate.d/testapp 2>&1

# Ver resultado:
echo ""
echo "=== Archivos resultantes ==="
ls -lh /var/log/testapp/
```

**Predicción:**

<details><summary>¿Qué archivos existirán después de la primera rotación forzada?</summary>

Después de forzar:
- `/var/log/testapp/app.log` — archivo nuevo, vacío (creado por `create`)
- `/var/log/testapp/app.log.1` — el contenido original (sin comprimir por `delaycompress`)

Si forzaras una segunda rotación:
- `app.log` — nuevo vacío
- `app.log.1` — vacío de la rotación anterior (sin comprimir, `delaycompress`)
- `app.log.2.gz` — el contenido original (ahora sí comprimido)
</details>

### Ejercicio 7 — copytruncate vs create

Observa la diferencia entre los dos métodos de rotación.

```bash
# Escribir contenido:
echo "Contenido antes de rotar $(date)" | sudo tee /var/log/testapp/app.log

# Ver el inode actual:
echo "=== Inode ANTES de rotar ==="
ls -li /var/log/testapp/app.log

# Forzar rotación (usa create por defecto):
sudo logrotate -fv /etc/logrotate.d/testapp 2>&1 | tail -5

echo ""
echo "=== Inode DESPUÉS de rotar ==="
ls -li /var/log/testapp/app.log
ls -li /var/log/testapp/app.log.1 2>/dev/null
```

**Predicción:**

<details><summary>¿Cambió el inode de app.log después de rotar con create?</summary>

Sí. Con `create`, logrotate renombra el archivo original (`mv app.log app.log.1`) y crea uno nuevo (`touch app.log`). El nuevo `app.log` tiene un **inode diferente**. Por eso las apps necesitan postrotate/reload: tienen un fd abierto al inode viejo (ahora `app.log.1`) y siguen escribiendo allí.

Con `copytruncate`, el inode de `app.log` **no cambiaría** porque se copia el contenido y se trunca el original. La app sigue con el mismo fd al mismo inode.
</details>

### Ejercicio 8 — Diagnosticar errores

Simula errores comunes y diagnostícalos.

```bash
# Error 1: archivo no existe sin missingok
sudo tee /tmp/test-logrotate.conf << 'EOF'
/var/log/nonexistent-file.log {
    daily
    rotate 3
}
EOF

echo "=== Sin missingok ==="
sudo logrotate -d /tmp/test-logrotate.conf 2>&1 | grep -i error

# Error 2: con missingok
sudo tee /tmp/test-logrotate.conf << 'EOF'
/var/log/nonexistent-file.log {
    daily
    rotate 3
    missingok
}
EOF

echo ""
echo "=== Con missingok ==="
sudo logrotate -d /tmp/test-logrotate.conf 2>&1 | grep -iE "error|missing"
```

**Predicción:**

<details><summary>¿Qué diferencia hay entre ambas ejecuciones?</summary>

Sin `missingok`: logrotate reporta `error: stat of /var/log/nonexistent-file.log failed: No such file or directory` y sale con código de error.

Con `missingok`: logrotate no reporta error; simplemente omite el archivo inexistente silenciosamente. Esto es importante para archivos que no siempre existen (p. ej., logs de un servicio que a veces no está instalado).
</details>

### Ejercicio 9 — Nomenclatura con dateext

Observa cómo `dateext` cambia los nombres de los archivos rotados.

```bash
# Crear config con dateext:
sudo tee /tmp/test-dateext.conf << 'EOF'
/var/log/testapp/app.log {
    daily
    rotate 5
    compress
    delaycompress
    missingok
    notifempty
    create 0640 root root
    dateext
}
EOF

# Escribir contenido:
echo "Test dateext $(date)" | sudo tee /var/log/testapp/app.log

# Simular:
echo "=== Dry run con dateext ==="
sudo logrotate -d /tmp/test-dateext.conf 2>&1 | grep -E "renaming|dateext|rotating"
```

**Predicción:**

<details><summary>¿Cómo se llamará el archivo rotado con dateext?</summary>

En vez de `app.log.1`, se llamará `app.log-20260325` (con la fecha de hoy). Con `dateyesterday` añadido, usaría la fecha de ayer: `app.log-20260324`. Esto hace más fácil identificar cuándo se creó cada rotación sin mirar timestamps del filesystem. Es el default en RHEL pero no en Debian.
</details>

### Ejercicio 10 — Limpieza y resumen

Limpia los archivos de prueba y resume lo aprendido.

```bash
# Limpiar:
sudo rm -rf /var/log/testapp/
sudo rm -f /etc/logrotate.d/testapp
sudo rm -f /tmp/test-logrotate.conf /tmp/test-dateext.conf

# Verificar limpieza:
ls /var/log/testapp/ 2>/dev/null || echo "testapp limpio"
ls /etc/logrotate.d/testapp 2>/dev/null || echo "config limpia"

# Resumen:
echo ""
echo "=== Resumen ==="
echo "1. logrotate se invoca por systemd timer o cron (no es un daemon)"
echo "2. /etc/logrotate.conf = defaults globales"
echo "3. /etc/logrotate.d/ = configs por aplicación"
echo "4. logrotate -d = dry run (verificar sin ejecutar)"
echo "5. logrotate -f = forzar rotación"
echo "6. create (renombrar+crear) vs copytruncate (copiar+truncar)"
echo "7. delaycompress = no comprimir la copia más reciente"
echo "8. Estado en /var/lib/logrotate/status"
```

**Predicción:**

<details><summary>¿Qué directivas usarías para una app en producción que soporta reload?</summary>

```
/var/log/myapp/*.log {
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
```

`daily` + `rotate 30` = 30 días de retención. `compress` + `delaycompress` = comprimir todo excepto el más reciente (para análisis rápido). `sharedscripts` + `postrotate` = recargar una sola vez. `missingok` + `notifempty` = robusto ante archivos faltantes o vacíos.
</details>
