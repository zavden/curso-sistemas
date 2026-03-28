# T02 — Directivas de logrotate

## Directivas de frecuencia

Controlan **cada cuánto** se rota un archivo:

```bash
# Rotación por tiempo:
daily       # cada día
weekly      # cada semana
monthly     # cada mes
yearly      # cada año

# Rotación por tamaño:
size 100M   # rotar cuando el archivo alcance 100MB
size 10k    # 10 kilobytes
size 1G     # 1 gigabyte

# size IGNORA la frecuencia — rota solo cuando se alcanza el tamaño
# No importa si se configuró daily; si el archivo no llega a 100M,
# no se rota

# Combinación tamaño + tiempo:
maxsize 100M  # rotar si alcanza 100M, INCLUSO si no ha pasado el período
minsize 10M   # NO rotar si no ha alcanzado 10M, aunque haya pasado el período

# Ejemplo práctico:
/var/log/myapp.log {
    daily
    minsize 1M     # no rotar si tiene menos de 1MB (evitar rotaciones vacías)
    maxsize 500M   # rotar inmediatamente si llega a 500MB (no esperar al día)
    rotate 30
}
```

## Directivas de retención

Controlan **cuántas** copias se mantienen:

```bash
# Número de rotaciones a mantener:
rotate 7     # mantener 7 copias (más el archivo actual)
rotate 0     # no mantener copias (eliminar al rotar)
rotate 365   # mantener un año de copias diarias

# Eliminar por antigüedad:
maxage 30    # eliminar copias de más de 30 días
             # independiente de rotate
             # útil cuando la frecuencia de rotación es irregular
```

```bash
# Ejemplo — retención de 30 días máximo:
/var/log/myapp.log {
    daily
    rotate 30
    maxage 30    # redundante con rotate 30 si es daily, pero explícito
}

# Ejemplo — retención por tamaño, no por cuenta:
/var/log/myapp.log {
    daily
    rotate 999   # sin límite práctico por conteo
    maxage 90    # pero eliminar todo lo mayor a 90 días
}
```

## Directivas de compresión

```bash
# Comprimir archivos rotados:
compress          # comprimir con gzip por defecto
nocompress        # no comprimir

# Retrasar la compresión una rotación:
delaycompress     # no comprimir el archivo recién rotado (.1)
                  # comprimirlo en la siguiente rotación (.2.gz)

# ¿Por qué delaycompress?
# Porque algunos procesos pueden seguir escribiendo en .1
# brevemente después de la rotación
# Y porque es útil tener el log de ayer sin comprimir para acceso rápido

# Algoritmo de compresión:
compresscmd /usr/bin/xz       # usar xz en lugar de gzip
uncompresscmd /usr/bin/unxz   # para descomprimir
compressext .xz               # extensión del archivo comprimido
compressoptions -9             # opciones del compresor (máxima compresión)

# Ejemplo con zstd (más rápido que gzip, mejor ratio):
compresscmd /usr/bin/zstd
uncompresscmd /usr/bin/unzstd
compressext .zst
compressoptions -T0    # usar todos los cores
```

```bash
# Ejemplo completo:
/var/log/myapp.log {
    daily
    rotate 30
    compress
    delaycompress
    # Resultado:
    # myapp.log       (actual)
    # myapp.log.1     (ayer, sin comprimir)
    # myapp.log.2.gz  (anteayer, comprimido)
    # myapp.log.3.gz  (hace 3 días, comprimido)
}
```

## Directivas de creación de archivo

### create — Renombrar y crear nuevo

```bash
# create es el mecanismo por defecto:
create              # crear archivo nuevo después de rotar
create 0640 www-data adm    # con permisos, owner y group específicos
nocreate            # no crear archivo nuevo (la aplicación lo creará)

# Flujo con create:
# 1. myapp.log → myapp.log.1 (rename)
# 2. Se crea myapp.log nuevo (vacío, con permisos de create)
# 3. La aplicación necesita reabrir el archivo (postrotate reload)
```

```bash
# Problema con create: la aplicación puede tener el file descriptor
# abierto al archivo viejo (ahora .1) y seguir escribiendo ahí

# Solución — postrotate para notificar a la aplicación:
/var/log/myapp.log {
    create 0640 myapp myapp
    postrotate
        systemctl reload myapp > /dev/null 2>&1 || true
    endscript
}

# Después del reload, la aplicación reabre el archivo
# y escribe en el nuevo myapp.log
```

### copytruncate — Copiar y truncar

```bash
# copytruncate es para aplicaciones que NO pueden reabrir archivos:
copytruncate    # copiar el contenido a .1, luego truncar el original

# Flujo con copytruncate:
# 1. cp myapp.log myapp.log.1 (copia)
# 2. truncate myapp.log (vaciar el original)
# 3. La aplicación sigue escribiendo en el mismo archivo (truncado)

# Ventaja: no necesita postrotate/reload
# Desventaja: hay una ventana entre la copia y el truncamiento
# donde las líneas escritas se pierden

# Cuándo usar copytruncate:
# - Aplicaciones que no soportan señales de reload
# - Aplicaciones legacy que abren el archivo una vez y no lo cierran
# - Aplicaciones de terceros sin control sobre su logging
```

```bash
# IMPORTANTE: create y copytruncate son mutuamente excluyentes
# No usar ambos a la vez

# create:     rename + create new → necesita postrotate
# copytruncate: copy + truncate  → no necesita postrotate
```

### copy — Solo copiar

```bash
# copy: copia el archivo pero NO lo modifica ni trunca
copy    # crear una copia del log, dejar el original intacto

# Útil cuando quieres archivar pero no rotar
# El archivo original sigue creciendo
# Se usa con maxsize o scripts personalizados
```

## Directivas de nombrado

```bash
# Por defecto, los archivos rotados se nombran con un número:
# myapp.log.1, myapp.log.2.gz, myapp.log.3.gz

# Usar fecha en el nombre:
dateext         # myapp.log-20260317, myapp.log-20260316.gz
nodateext       # volver al esquema numérico

# Formato de fecha:
dateformat -%Y%m%d         # -20260317 (default con dateext)
dateformat -%Y%m%d-%H%M%S  # -20260317-153000 (útil con maxsize)
dateformat -%Y-%m-%d       # -2026-03-17

# dateyesterday — usar la fecha de AYER en el nombre:
dateyesterday   # el log de ayer se nombra con la fecha de ayer
                # (por defecto usa la fecha de la rotación, que es hoy)
```

```bash
# Ejemplo con dateext:
/var/log/myapp.log {
    daily
    rotate 30
    compress
    delaycompress
    dateext
    dateformat -%Y%m%d

    # Resultado:
    # myapp.log                     (actual)
    # myapp.log-20260317            (ayer, sin comprimir)
    # myapp.log-20260316.gz         (anteayer, comprimido)
    # myapp.log-20260315.gz         (hace 3 días)
}
```

```bash
# Con dateext + maxsize, el dateformat necesita incluir hora
# para evitar colisiones si se rota más de una vez al día:
/var/log/myapp.log {
    daily
    maxsize 500M
    rotate 30
    compress
    dateext
    dateformat -%Y%m%d-%H%M%S
}
```

## Directivas de control

```bash
# No reportar error si el archivo no existe:
missingok       # ignorar archivos que no existen
nomissingok     # reportar error (default)

# No rotar si el archivo está vacío:
notifempty      # no rotar archivos de 0 bytes
ifempty         # rotar incluso si está vacío (default)

# Ejecutar aunque el archivo no haya cambiado:
# (no hay directiva — logrotate siempre rota si toca el período)
```

## Scripts — prerotate y postrotate

### postrotate — Después de rotar

```bash
# El uso más común: notificar a la aplicación que reabra el log:
/var/log/nginx/*.log {
    postrotate
        # nginx reabre los logs al recibir USR1:
        [ -f /var/run/nginx.pid ] && kill -USR1 $(cat /var/run/nginx.pid)
    endscript
}

/var/log/rsyslog/*.log {
    postrotate
        # rsyslog reabre los archivos con HUP:
        /usr/lib/rsyslog/rsyslog-rotate
    endscript
}

/var/log/myapp.log {
    postrotate
        # systemd reload:
        systemctl reload myapp > /dev/null 2>&1 || true
    endscript
}
```

### prerotate — Antes de rotar

```bash
# Ejecutar algo ANTES de la rotación:
/var/log/myapp.log {
    prerotate
        # Verificar que el servicio no está en mantenimiento:
        [ -f /var/run/myapp-maintenance ] && exit 1
    endscript
}

# Si prerotate falla (exit != 0), la rotación se aborta
```

### firstaction y lastaction

```bash
# firstaction: se ejecuta UNA VEZ antes de procesar CUALQUIER archivo
# lastaction: se ejecuta UNA VEZ después de procesar TODOS los archivos

/var/log/myapp/*.log {
    sharedscripts
    firstaction
        logger "logrotate: iniciando rotación de myapp"
    endscript
    postrotate
        systemctl reload myapp > /dev/null 2>&1 || true
    endscript
    lastaction
        logger "logrotate: rotación de myapp completada"
    endscript
}
```

### sharedscripts

```bash
# Por defecto, postrotate se ejecuta UNA VEZ POR CADA ARCHIVO rotado:
/var/log/myapp/*.log {
    postrotate
        systemctl reload myapp    # se ejecuta N veces (1 por cada .log)
    endscript
}

# Con sharedscripts, se ejecuta UNA SOLA VEZ para todo el bloque:
/var/log/myapp/*.log {
    sharedscripts
    postrotate
        systemctl reload myapp    # se ejecuta 1 vez
    endscript
}

# IMPORTANTE: con sharedscripts, si algún archivo no necesita rotación,
# postrotate NO se ejecuta para ninguno
# nosharedscripts restaura el comportamiento por defecto
```

## Directivas avanzadas

### su — Ejecutar como otro usuario

```bash
# Si logrotate corre como root pero los logs son de otro usuario:
/var/log/myapp/*.log {
    su myapp myapp    # ejecutar la rotación como myapp:myapp
    create 0640 myapp myapp
    ...
}

# Necesario cuando los logs están en directorios
# propiedad de un usuario no-root
```

### tabooext — Extensiones ignoradas

```bash
# logrotate ignora archivos con ciertas extensiones en /etc/logrotate.d/:
# .dpkg-old, .dpkg-dist, .dpkg-new, .disabled, ~, .bak, .rpmsave, .rpmorig

# Agregar extensiones a ignorar:
tabooext + .disabled .skip
```

### include

```bash
# Incluir archivos o directorios adicionales:
include /etc/logrotate.d
include /opt/myapp/logrotate.conf
```

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

## Diagnóstico de problemas

```bash
# 1. Dry run para ver qué haría:
sudo logrotate -dv /etc/logrotate.d/myapp

# 2. Forzar rotación con verbose:
sudo logrotate -fv /etc/logrotate.d/myapp

# 3. Verificar el estado:
grep myapp /var/lib/logrotate/status 2>/dev/null || \
    grep myapp /var/lib/logrotate.status 2>/dev/null

# 4. Ver errores recientes:
journalctl -u logrotate --since today --no-pager

# 5. Problemas comunes:
# - "error: skipping ... because parent directory has insecure permissions"
#   → El directorio padre tiene permisos 777 o es writable por otros
#   → Solución: chmod 755 /var/log/myapp/ o usar su directive

# - "error: stat of /var/log/myapp.log failed"
#   → El archivo no existe → agregar missingok

# - Archivo rotado pero la app sigue escribiendo en .1
#   → Falta postrotate con reload/signal
#   → O usar copytruncate
```

## Tabla de directivas

| Directiva | Descripción | Default |
|---|---|---|
| daily/weekly/monthly/yearly | Frecuencia de rotación | weekly |
| rotate N | Copias a mantener | 4 |
| size N | Rotar por tamaño | (no usado) |
| minsize N | No rotar si es menor que N | (no usado) |
| maxsize N | Rotar si es mayor que N aunque no toque | (no usado) |
| maxage N | Eliminar copias mayores a N días | (no usado) |
| compress | Comprimir copias rotadas | nocompress |
| delaycompress | No comprimir la copia más reciente | (no usado) |
| create mode owner group | Crear archivo nuevo con permisos | create |
| copytruncate | Copiar y truncar en lugar de renombrar | (no usado) |
| dateext | Usar fecha en el nombre | nodateext |
| missingok | Ignorar archivos que no existen | nomissingok |
| notifempty | No rotar archivos vacíos | ifempty |
| sharedscripts | Ejecutar scripts una vez para todo el bloque | nosharedscripts |
| su user group | Ejecutar como otro usuario | (root) |

---

## Ejercicios

### Ejercicio 1 — Leer configuraciones existentes

```bash
# Examinar la configuración de rsyslog:
cat /etc/logrotate.d/rsyslog 2>/dev/null || echo "No encontrado"

# ¿Usa compress? ¿delaycompress? ¿postrotate?
```

### Ejercicio 2 — Dry run

```bash
# Simular la rotación de todos los logs:
sudo logrotate -d /etc/logrotate.conf 2>&1 | grep -E "rotating|considering" | head -20

# ¿Cuántos archivos se rotarían?
sudo logrotate -d /etc/logrotate.conf 2>&1 | grep -c "rotating pattern"
```

### Ejercicio 3 — Verificar archivos rotados

```bash
# ¿Cómo se ven los archivos rotados en tu sistema?
ls -lh /var/log/syslog* /var/log/messages* 2>/dev/null
# ¿Usan dateext o números?
# ¿Están comprimidos?
# ¿Cuántas rotaciones hay?
```
