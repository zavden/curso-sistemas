# T01 — Configuración de logrotate

## Qué es logrotate

logrotate gestiona la **rotación, compresión y eliminación** de archivos
de log. Sin rotación, los logs crecen indefinidamente hasta llenar el
disco:

```
Sin logrotate:
  /var/log/syslog → crece y crece → 50GB → disco lleno → sistema caído

Con logrotate:
  /var/log/syslog        (actual, 10MB)
  /var/log/syslog.1      (ayer, 10MB)
  /var/log/syslog.2.gz   (anteayer, 2MB comprimido)
  /var/log/syslog.3.gz   (hace 3 días, 2MB comprimido)
  /var/log/syslog.4.gz   (hace 4 días, 2MB comprimido)
  → se elimina syslog.5.gz (si rotate=4)
```

```bash
# logrotate no es un daemon — se ejecuta periódicamente:
# En sistemas modernos, un systemd timer o cron.daily lo invoca

# Verificar cómo se invoca:
systemctl cat logrotate.timer 2>/dev/null    # systemd timer
cat /etc/cron.daily/logrotate 2>/dev/null    # cron

# Versión:
logrotate --version
```

## Cómo se invoca logrotate

### Con systemd timer (Debian moderno, Fedora)

```bash
systemctl cat logrotate.timer
# [Timer]
# OnCalendar=daily
# AccuracySec=12h
# Persistent=true

systemctl cat logrotate.service
# [Service]
# Type=oneshot
# ExecStart=/usr/sbin/logrotate /etc/logrotate.conf

# Verificar cuándo se ejecutó por última vez:
systemctl status logrotate.timer
# Trigger: Wed 2026-03-18 00:00:00 UTC
# Last: Tue 2026-03-17 06:30:00 UTC
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

## /etc/logrotate.conf

El archivo principal define los **defaults globales** y luego incluye
archivos específicos por aplicación:

```bash
cat /etc/logrotate.conf
```

```bash
# /etc/logrotate.conf (Debian típico):

# Rotación semanal por defecto:
weekly

# Mantener 4 rotaciones:
rotate 4

# Crear archivo nuevo después de rotar:
create

# Comprimir logs rotados:
#compress     # comentado por defecto en Debian

# Incluir configuraciones por aplicación:
include /etc/logrotate.d

# Rotación de wtmp y btmp (quién inició sesión):
/var/log/wtmp {
    monthly
    create 0664 root utmp
    minsize 1M
    rotate 1
}

/var/log/btmp {
    missingok
    monthly
    create 0600 root utmp
    rotate 1
}
```

```bash
# /etc/logrotate.conf (RHEL típico):

weekly
rotate 4
create
dateext            # usar fecha en el nombre (syslog-20260317)
#compress
include /etc/logrotate.d

/var/log/wtmp {
    monthly
    create 0664 root utmp
    minsize 1M
    rotate 1
}

/var/log/btmp {
    missingok
    monthly
    create 0600 root utmp
    rotate 1
}
```

### Diferencia Debian vs RHEL

```bash
# Debian:
# - No usa dateext por defecto (syslog.1, syslog.2, ...)
# - compress comentado por defecto
# - Los logs suelen ser propiedad de syslog:adm

# RHEL:
# - Usa dateext por defecto (messages-20260317)
# - compress comentado por defecto
# - Los logs suelen ser propiedad de root:root
```

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

# Estos archivos se procesan en orden alfabético
# Cada uno define reglas para sus propios archivos de log
```

### Ejemplo — /etc/logrotate.d/rsyslog

```bash
cat /etc/logrotate.d/rsyslog
```

```bash
# Debian:
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
/var/log/messages
{
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

### Ejemplo — /etc/logrotate.d/nginx

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

## Anatomía de un bloque de configuración

```bash
# Estructura:
/ruta/al/archivo.log {           # archivo(s) a rotar
    directiva1                    # configuración específica
    directiva2
    prerotate                     # script ANTES de rotar
        comando
    endscript
    postrotate                    # script DESPUÉS de rotar
        comando
    endscript
}
```

### Patrones de archivo

```bash
# Archivo específico:
/var/log/myapp.log {
    ...
}

# Con wildcard:
/var/log/myapp/*.log {
    ...
}

# Múltiples archivos:
/var/log/myapp.log
/var/log/myapp-error.log
/var/log/myapp-access.log
{
    ...
}

# Múltiples con wildcard:
/var/log/myapp/*.log
/var/log/myapp-extra/*.log
{
    ...
}
```

## Crear una configuración para tu aplicación

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

### Ejemplo con postrotate

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

### Ejemplo con copytruncate

```bash
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

## Probar y verificar

### Dry run (sin ejecutar)

```bash
# Ver qué haría logrotate sin hacer nada:
sudo logrotate -d /etc/logrotate.conf
# -d = debug (dry run)
# Muestra qué archivos rotaría, qué scripts ejecutaría, etc.

# Probar solo un archivo de configuración:
sudo logrotate -d /etc/logrotate.d/myapp
```

### Forzar rotación

```bash
# Forzar rotación inmediata (ignorar el período):
sudo logrotate -f /etc/logrotate.d/myapp
# Rota aunque no se haya alcanzado el período

# Forzar toda la configuración:
sudo logrotate -f /etc/logrotate.conf

# Con verbose:
sudo logrotate -fv /etc/logrotate.d/myapp
# -v = verbose (mostrar qué hace)
```

### Verificar la sintaxis

```bash
# logrotate -d también verifica la sintaxis:
sudo logrotate -d /etc/logrotate.d/myapp 2>&1 | head -20
# Si hay errores de sintaxis, los reporta

# Errores comunes:
# "error: bad rotation count" → rotate tiene un valor inválido
# "error: myapp:3 unknown option" → directiva no reconocida
# "error: stat of /var/log/myapp.log failed" → archivo no existe (sin missingok)
```

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
# "/var/log/nginx/access.log" 2026-3-17-6:30:0

# Si necesitas forzar una re-rotación, puedes:
# 1. Editar el archivo de estado y cambiar la fecha
# 2. Eliminar la línea del archivo específico
# 3. Usar logrotate -f

# Ubicación configurable:
# logrotate -s /var/lib/logrotate/myapp-status /etc/logrotate.d/myapp
```

## Flujo de rotación paso a paso

```bash
# Cuando logrotate rota /var/log/myapp.log con compress + delaycompress:

# Estado inicial:
# /var/log/myapp.log      (activo, 15MB)
# /var/log/myapp.log.1    (rotado ayer, sin comprimir)
# /var/log/myapp.log.2.gz (de anteayer, comprimido)
# /var/log/myapp.log.3.gz (hace 3 días)

# Paso 1 — Eliminar el más antiguo (si rotate=3):
# Se elimina /var/log/myapp.log.3.gz

# Paso 2 — Renombrar en cascada:
# myapp.log.2.gz → myapp.log.3.gz
# myapp.log.1    → myapp.log.2.gz  (ahora se comprime)
# myapp.log      → myapp.log.1     (delaycompress: no comprimir aún)

# Paso 3 — Crear archivo nuevo (si create):
# Se crea /var/log/myapp.log nuevo (vacío, con permisos de create)

# Paso 4 — postrotate:
# Se ejecuta el script postrotate (ej: recargar el servicio)

# Estado final:
# /var/log/myapp.log      (nuevo, vacío)
# /var/log/myapp.log.1    (el que era activo, sin comprimir)
# /var/log/myapp.log.2.gz (comprimido)
# /var/log/myapp.log.3.gz (comprimido)
```

## Errores comunes

### Archivo no existe

```bash
# Error:
# error: stat of /var/log/myapp.log failed: No such file or directory

# Solución: agregar missingok
/var/log/myapp.log {
    missingok    # no reportar error si el archivo no existe
    ...
}
```

### Permisos incorrectos después de rotar

```bash
# Problema: el archivo rotado tiene permisos de root
# y la aplicación no puede escribir en el nuevo archivo

# Solución: especificar permisos con create:
create 0640 myapp myapp
# modo owner group
```

### La aplicación sigue escribiendo en el archivo rotado

```bash
# Problema: después de renombrar myapp.log → myapp.log.1,
# la aplicación tiene un file descriptor abierto al archivo viejo
# y sigue escribiendo en myapp.log.1

# Solución 1 — postrotate para recargar:
postrotate
    systemctl reload myapp.service > /dev/null 2>&1 || true
endscript

# Solución 2 — copytruncate (si la app no soporta reload):
copytruncate
# Copia el archivo y luego trunca el original
# La app sigue escribiendo en el mismo archivo (truncado)
# Desventaja: puede perder líneas escritas entre copy y truncate
```

---

## Ejercicios

### Ejercicio 1 — Explorar la configuración

```bash
# 1. ¿Cuántos archivos de configuración hay?
ls /etc/logrotate.d/ | wc -l

# 2. ¿Cuáles son los defaults globales?
grep -v '^#' /etc/logrotate.conf | grep -v '^$' | head -10

# 3. ¿Cómo se invoca logrotate?
systemctl status logrotate.timer 2>/dev/null || \
    cat /etc/cron.daily/logrotate 2>/dev/null || \
    echo "No encontrado"
```

### Ejercicio 2 — Estado de rotación

```bash
# ¿Cuándo se rotaron los logs por última vez?
cat /var/lib/logrotate/status 2>/dev/null | head -10 || \
    cat /var/lib/logrotate.status 2>/dev/null | head -10
```

### Ejercicio 3 — Dry run

```bash
# Simular una rotación y ver qué pasaría:
sudo logrotate -d /etc/logrotate.d/rsyslog 2>&1 | head -30
```
