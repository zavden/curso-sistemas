# T01 — Configuración de logrotate

> **Objetivo:** Entender cómo logrotate gestiona la rotación de logs, dominar su configuración y saber diagnosticar problemas comunes.

## Qué es logrotate

**logrotate** gestiona la **rotación, compresión y eliminación** de archivos de log. Sin rotación, los logs crecen indefinidamente hasta llenar el disco.

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                    SIN LOGROTATE                                            │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                              │
│   Día 1:    /var/log/syslog        10 MB                                   │
│   Día 10:   /var/log/syslog        100 MB                                  │
│   Día 30:   /var/log/syslog        300 MB                                 │
│   Día 60:   /var/log/syslog        600 MB → DISCO LLENO → sistema caído   │
│                                                                              │
└─────────────────────────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────────────────────────┐
│                    CON LOGROTATE                                            │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                              │
│   /var/log/syslog          (actual, 10MB)                                  │
│   /var/log/syslog.1        (ayer, 10MB)                                    │
│   /var/log/syslog.2.gz     (anteayer, 2MB comprimido)                       │
│   /var/log/syslog.3.gz     (hace 3 días, 2MB)                              │
│   /var/log/syslog.4.gz     (hace 4 días, 2MB)                              │
│   → se elimina syslog.5.gz (rotate=4, se guarda solo 4 versiones)           │
│                                                                              │
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

El archivo principal define defaults globales y luego incluye configuraciones específicas:

```bash
cat /etc/logrotate.conf
```

### Defaults globales

```bash
# Debian (típico):
weekly              # rotación semanal
rotate 4           # mantener 4 versiones
create             # crear archivo nuevo tras rotar
#compress          # comentar por defecto en Debian

include /etc/logrotate.d

# RHEL (típico):
weekly
rotate 4
create
dateext           # usar fecha en el nombre: syslog-20260317
#compress

include /etc/logrotate.d
```

### Entradas especiales (wtmp, btmp)

```bash
# Estas entradas tienen configuración inline:
/var/log/wtmp {
    monthly          # rotación mensual
    create 0664 root utmp   # permisos especiales
    minsize 1M       # solo rotar si > 1MB
    rotate 1          # solo 1 versión
}

/var/log/btmp {
    missingok         # no error si no existe
    monthly
    create 0600 root utmp
    rotate 1
}
```

### Diferencias Debian vs RHEL

| Aspecto | Debian/Ubuntu | RHEL/Fedora |
|---------|---------------|--------------|
| Nomenclatura | `syslog.1`, `syslog.2` | `syslog-20260317` (dateext) |
| Compress default | commented | commented |
| Permisos logs | `syslog:adm` | `root:root` |
| Ubicación estado | `/var/lib/logrotate/status` | `/var/lib/logrotate/status` |

---

## /etc/logrotate.d/

Cada aplicación instala su propia configuración:

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

    prerotate                  # script ANTES de renombrar
        comando1
        comando2
    endscript

    postrotate                 # script DESPUÉS de rotar
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
| `compress` | Comprimir versiones antiguas |
| `delaycompress` | No comprimir inmediatamente (dejar .1 sin comprimir) |
| `create MODE OWNER GROUP` | Permisos del nuevo archivo |
| `missingok` | No error si el archivo no existe |
| `notifempty` | No rotar si el archivo está vacío |
| `copytruncate` | Copiar y truncar (para apps que no soportan reload) |
| `sharedscripts` | Ejecutar scripts solo una vez (no por archivo) |
| `minsize N` | Solo rotar si el archivo es mayor a N |
| `maxsize N` | Rotar inmediatamente si supera N |
| `dateext` | Usar fecha en el nombre |
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

### Ejemplo con copytruncate (app que no.reload)

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
  mv myapp.log.1    →  myapp.log.2.gz   (AHORA se comprime)
  mv myapp.log      →  myapp.log.1       (no se comprime - delaycompress)

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
# "error: myapp:3 unknown option" → directiva no reconocida
# "error: stat of ... failed"     → archivo no existe (sin missingok)
```

---

## Estado de logrotate

```bash
# logrotate recuerda cuándo rotó cada archivo:
cat /var/lib/logrotate/status

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

### copytruncate vs postrotate

```
┌─────────────────────────────────────────────────────────────────────────────┐
│  postrotate (RECOMENDADO)                                                  │
│  ─────────────────────────────────                                          │
│  1. mv myapp.log → myapp.log.1                                           │
│  2. touch myapp.log (nuevo)                                              │
│  3. postrotate: recargar servicio                                        │
│  4. App abre nuevo fd hacia myapp.log                                     │
│  ✓ No hay pérdida de logs                                                 │
│  ✓ La app debe soportar signals o reload                                 │
├─────────────────────────────────────────────────────────────────────────────┤
│  copytruncate ( альтернатива)                                            │
│  ─────────────────────────────────                                        │
│  1. cp myapp.log → myapp.log.1                                          │
│  2. truncate myapp.log (vaciar original)                                 │
│  3. App sigue escribiendo en el mismo fd (truncado)                     │
│  ✗ PUEDE perder logs (entre copy y truncate)                             │
│  ✓ Funciona con cualquier app (no necesita reload)                       │
└─────────────────────────────────────────────────────────────────────────────┘
```

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
```

---

## Ejercicios

### Ejercicio 1 — Explorar la configuración

```bash
# 1. ¿Cuántos archivos de configuración?
ls /etc/logrotate.d/ | wc -l

# 2. Defaults globales:
grep -v '^#' /etc/logrotate.conf | grep -v '^$'

# 3. ¿Cómo se invoca?
systemctl status logrotate.timer 2>/dev/null || \
    cat /etc/cron.daily/logrotate 2>/dev/null

# 4. ¿Qué directivas usa nginx?
cat /etc/logrotate.d/nginx
```

### Ejercicio 2 — Estado de rotación

```bash
# ¿Cuándo se rotaron los logs por última vez?
cat /var/lib/logrotate/status 2>/dev/null | grep -E "syslog|nginx" | head -5

# ¿Cuántos archivos .gz hay de syslog?
ls /var/log/syslog* 2>/dev/null
```

### Ejercicio 3 — Dry run

```bash
# Simular rotación de rsyslog:
sudo logrotate -d /etc/logrotate.d/rsyslog 2>&1 | head -40

# Simular rotación de nginx:
sudo logrotate -d /etc/logrotate.d/nginx 2>&1 | head -40
```

### Ejercicio 4 — Crear configuración para una app

```bash
# Objetivo: crear configuración para /var/log/myapp.log

# 1. Crear archivo de prueba:
sudo mkdir -p /var/log/myapp
sudo logger -f /dev/null -t myapp -p user.info "test"
ls -la /var/log/myapp/

# 2. Crear configuración:
sudo tee /etc/logrotate.d/myapp << 'EOF'
/var/log/myapp/*.log {
    daily
    rotate 7
    compress
    delaycompress
    missingok
    notifempty
    create 0640 root root
    sharedscripts
    postrotate
        echo "Rotación completada $(date)" >> /var/log/myapp-rotation.log
    endscript
}
EOF

# 3. Probar dry run:
sudo logrotate -d /etc/logrotate.d/myapp 2>&1

# 4. Forzar primera rotación:
sudo logrotate -f /etc/logrotate.d/myapp
ls -la /var/log/myapp/
```

### Ejercicio 5 — Diagnosticar problemas

```bash
# Simular error "stat failed":
# Crear config sin missingok para archivo inexistente:
sudo tee /tmp/test-logrotate.conf << 'EOF'
/var/log/nonexistent-file.log {
    daily
    rotate 3
}
EOF

sudo logrotate -d /tmp/test-logrotate.conf 2>&1 | grep -i error

# Ahora con missingok:
sudo tee /tmp/test-logrotate.conf << 'EOF'
/var/log/nonexistent-file.log {
    daily
    rotate 3
    missingok
}
EOF

sudo logrotate -d /tmp/test-logrotate.conf 2>&1 | grep -i error
```
