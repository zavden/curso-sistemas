# T02 — Directivas de logrotate

> **Objetivo:** Dominar todas las directivas de logrotate para crear configuraciones de producción robustas y diagnosticar problemas.

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

# size IGNORA daily/weekly si el archivo no alcanza el tamaño
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

---

## Directivas de retención

Controlan **cuántas** copias se mantienen:

```bash
# Por número de archivos:
rotate 7      # mantener 7 copias (más el actual)
rotate 0      # no mantener copias (solo truncate)
rotate 365    # mantener un año (daily)

# Por antigüedad:
maxage 30     # eliminar copias de más de 30 días
               # Se verifica DAILY, independiente de rotate
```

```bash
# rotate vs maxage:
# rotate = contar archivos
# maxage = contar días

# Ejemplo — retención de 30 días:
/var/log/myapp.log {
    daily
    rotate 30          # mantener 30 archivos
    maxage 30          # eliminar los de más de 30 días (redundante con rotate daily)
}

# Ejemplo — tamaño fijo, eliminar por edad:
/var/log/myapp.log {
    daily
    rotate 999         # sin límite por conteo
    maxage 90          # pero eliminar todo mayor a 90 días
}
```

---

## Directivas de compresión

```bash
compress           # comprimir con gzip (default .gz)
nocompress         # no comprimir

# delaycompress — NO comprimir el recién rotado (.1):
# Útil porque hay una ventana donde la app aún puede escribir en .1
# También permite ver el log de ayer sin descomprimir
delaycompress

# Compresor alternativo:
compresscmd /usr/bin/xz
uncompresscmd /usr/bin/unxz
compressext .xz

# Opciones del compresor:
compressoptions -9    # máxima compresión gzip
compressoptions -T0   # zstd: usar todos los cores
```

```bash
# Ejemplo — compresión óptima para logs de producción:
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

### Comparativa de compresores

| Compresor | Ratio | Velocidad | CPU | Extensión |
|-----------|-------|-----------|-----|-----------|
| gzip | 70% | ⚡⚡⚡ | ⭐ | .gz |
| bzip2 | 75% | ⚡⚡ | ⭐⭐⭐ | .bz2 |
| xz | 80% | ⚡ | ⭐⭐⭐⭐ | .xz |
| zstd | 75% | ⚡⚡⚡⚡ | ⭐ | .zst |

```bash
# zstd (recomendado para producción):
compresscmd /usr/bin/zstd
uncompresscmd /usr/bin/unzstd
compressext .zst
compressoptions -T0
```

---

## Directivas de creación de archivo

### create — Renombrar y crear nuevo

```bash
# Default: create (sin argumentos usa mode 0600, owner root, group root)
create
create 0640 www-data adm   # permisos, owner, group

# Flujo:
# 1. mv myapp.log → myapp.log.1
# 2. touch myapp.log (vacío)
# 3. chown www-data:adm myapp.log
# 4. chmod 0640 myapp.log
```

### copytruncate — Copiar y truncar

```bash
# Para aplicaciones que NO pueden reabrir archivos:
copytruncate

# Flujo:
# 1. cp myapp.log myapp.log.1
# 2. truncate -s 0 myapp.log
# 3. La app sigue escribiendo en el mismo fd (truncado)

# ADVERTENCIA: puede perder logs entre cp y truncate
# USO: solo cuando la app no puede responder a reload/signals
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
│  create (DEFAULT)                                                          │
│  ─────────────────                                                         │
│  mv log → log.1                                                           │
│  touch log (nuevo)                                                        │
│  postrotate: reload → app escribe en log                                  │
│  ✓ Sin pérdida de logs                                                    │
│  ✗ La app debe reabrir archivos (reload)                                  │
├─────────────────────────────────────────────────────────────────────────────┤
│  copytruncate                                                             │
│  ───────────────                                                          │
│  cp log → log.1                                                           │
│  truncate -s 0 log                                                        │
│  → app sigue en el mismo fd, escribe en log (ahora vacío)               │
│  ✗ PUEDE perder logs (entre cp y truncate)                               │
│  ✓ Funciona con cualquier app (no necesita reload)                        │
├─────────────────────────────────────────────────────────────────────────────┤
│  copy                                                                     │
│  ────                                                                    │
│  cp log → log.backup                                                      │
│  → log NUNCA se modifica                                                  │
│  ✗ El log original sigue creciendo                                        │
│  ✓ Para archival sin afectar la app                                       │
└─────────────────────────────────────────────────────────────────────────────┘
```

```bash
# IMPORTANTE: create y copytruncate son MUTUAMENTE EXCLUYENTES
# Usar uno u otro, nunca ambos
```

---

## Directivas de nombrado

```bash
# Esquema numérico (default):
# myapp.log.1, myapp.log.2.gz, myapp.log.3.gz

# Con fecha (dateext):
dateext           # myapp.log-20260317
nodateext         # volver a esquema numérico

# Formato de fecha:
dateformat -%Y%m%d              # -20260317 (default)
dateformat -%Y%m%d-%H%M%S     # -20260317-153000 (con hora)
dateformat -%Y-%m-%d           # -2026-03-17

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
    # Resultado: myapp.log-20260317-030000 si rotó a las 3am
    # Si vuelve a rotar a las 3pm: myapp.log-20260317-150000
}
```

---

## Directivas de control

```bash
missingok          # no error si el archivo no existe
nomissingok        # reportar error (default)

notifempty         # no rotar si el archivo está vacío (default)
ifempty            # rotar incluso si está vacío

shred              # sobrescribir archivos antes de eliminar (seguridad)
shredcycles 3      # veces que se sobrescribe
noshred            # eliminar normalmente (default)
```

---

## Scripts — prerotate, postrotate, firstaction, lastaction

### postrotate — Después de rotar

```bash
# Notificar a la app que reabra el archivo:
/var/log/nginx/*.log {
    postrotate
        [ -f /var/run/nginx.pid ] && kill -USR1 $(cat /var/run/nginx.pid)
    endscript
}

# Con systemctl:
/var/log/myapp.log {
    postrotate
        systemctl reload myapp > /dev/null 2>&1 || true
    endscript
}

# Con script rsyslog:
/var/log/rsyslog/*.log {
    postrotate
        /usr/lib/rsyslog/rsyslog-rotate
    endscript
}
```

### prerotate — Antes de rotar

```bash
# Ejecutar algo ANTES de la rotación:
/var/log/myapp.log {
    prerotate
        # Abortar si el servicio está en mantenimiento
        [ -f /var/run/myapp-maintenance ] && exit 1
    endscript
}
# Si prerotate falla (exit != 0), NO se rota nada
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
# Sin sharedscripts — se ejecuta POR CADA ARCHIVO:
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

# ⚠️ CON sharedscripts: si NINGÚN archivo necesita rotación,
# postrotate NO se ejecuta para ninguno
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
```

### tabooext — Extensiones ignoradas

```bash
# logrotate ignora estas extensiones en /etc/logrotate.d/:
# .dpkg-old, .dpkg-dist, .dpkg-new, .disabled, ~, .bak, .rpmsave, .rpmorig

# Agregar más extensiones a ignorar:
tabooext + .disabled .skip
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
        systemctl reload myapp > /dev/null 2>&1 || true
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
| `unknown option 'x'` | Typo o directiva no válida | Verificar sintaxis |
| App sigue en .1 | Falta postrotate/reload | Agregar postrotate con reload |
| duplicate log entry | Mismo archivo en dos bloques | Consolidar en un solo bloque |
| `bad rotation count 'N'` | rotate con valor inválido | Usar número positivo |

---

## Tabla resumen de directivas

```
FRECUENCIA:
  daily / weekly / monthly / yearly
  size N
  minsize N
  maxsize N

RETENCIÓN:
  rotate N
  maxage N

COMPRESIÓN:
  compress / nocompress
  delaycompress
  compresscmd /usr/bin/X
  compressext .X

CREACIÓN:
  create MODE OWNER GROUP
  copytruncate
  copy

NOMBRADO:
  dateext / nodateext
  dateformat FORMATO
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

## Ejercicios

### Ejercicio 1 — Analizar directivas de rsyslog

```bash
# Ver la configuración completa de rsyslog:
cat /etc/logrotate.d/rsyslog

# Identificar:
# 1. ¿Usa compress? ¿delaycompress?
# 2. ¿Cuántas rotaciones mantiene?
# 3. ¿Tiene postrotate? ¿Qué ejecuta?
# 4. ¿Usa sharedscripts?
# 5. ¿Hay alguna directiva inusual?
```

### Ejercicio 2 — Simular rotación completa

```bash
# 1. Dry run de toda la configuración:
sudo logrotate -dv /etc/logrotate.conf 2>&1 | grep -A5 "rotating"

# 2. Ver cuántos archivos se rotarían:
sudo logrotate -d /etc/logrotate.conf 2>&1 | grep "rotating pattern" | wc -l

# 3. Forzar rotación de rsyslog y observar:
sudo logrotate -fv /etc/logrotate.d/rsyslog

# 4. Ver los archivos antes y después:
ls -la /var/log/syslog*
```

### Ejercicio 3 — Crear configuración con zstd

```bash
# 1. Verificar que zstd está instalado:
which zstd || echo "No instalado"

# 2. Crear configuración:
cat << 'EOF' | sudo tee /etc/logrotate.d/myapp-zstd
/var/log/myapp/*.log {
    daily
    rotate 14
    compress
    delaycompress
    compresscmd /usr/bin/zstd
    compressext .zst
    compressoptions -T0
    missingok
    notifempty
    create 0640 root root
    sharedscripts
    postrotate
        echo "Rotado $(date)" >> /var/log/myapp-rotate.log
    endscript
}
EOF

# 3. Probar dry run:
sudo logrotate -dv /etc/logrotate.d/myapp-zstd 2>&1

# 4. Forzar rotación y verificar:
sudo logrotate -fv /etc/logrotate.d/myapp-zstd
ls -la /var/log/myapp/
```

### Ejercicio 4 — Comparar esquemas de nombrado

```bash
# 1. Ver cómo naming de tu sistema:
ls -la /var/log/syslog* /var/log/messages* 2>/dev/null

# 2. Si tienes dateext:
#    Ver fechas en los archivos

# 3. Si tienes esquema numérico:
#    Ver números de versión

# 4. Calcular cuántos días de logs retienes:
echo "Archivos sin comprimir:"
ls -lh /var/log/syslog* 2>/dev/null | grep -v gz | wc -l
echo "Archivos comprimidos:"
ls -lh /var/log/syslog* 2>/dev/null | grep gz | wc -l
```

### Ejercicio 5 — Diagnosticar problema simulado

```bash
# SIMULAR un problema: config sin missingok para archivo inexistente

# 1. Crear config para archivo que NO existe:
cat << 'EOF' | sudo tee /tmp/bad-logrotate.conf
/var/log/nonexistent-app.log {
    daily
    rotate 3
}
EOF

# 2. Ejecutar logrotate:
sudo logrotate -f /tmp/bad-logrotate.conf 2>&1

# 3. Ver el error:
# Debería dar "error: stat of ... failed"

# 4. Ahora con missingok:
cat << 'EOF' | sudo tee /tmp/good-logrotate.conf
/var/log/nonexistent-app.log {
    daily
    rotate 3
    missingok
}
EOF

# 5. Verificar que NO hay error:
sudo logrotate -f /tmp/good-logrotate.conf 2>&1
```
