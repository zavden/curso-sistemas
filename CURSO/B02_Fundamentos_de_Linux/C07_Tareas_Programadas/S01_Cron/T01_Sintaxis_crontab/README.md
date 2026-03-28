# T01 — Sintaxis crontab

## Qué es cron

cron es el planificador de tareas clásico de Unix. Un daemon (`crond` en
RHEL, `cron` en Debian) lee tablas de tareas (crontabs) y ejecuta comandos
en los momentos especificados:

```bash
# El daemon cron:
systemctl is-active cron        # Debian/Ubuntu
systemctl is-active crond       # RHEL/Fedora

# cron lee las tareas de:
# 1. /var/spool/cron/crontabs/<usuario>  — crontabs de usuario (Debian)
#    /var/spool/cron/<usuario>           — crontabs de usuario (RHEL)
# 2. /etc/crontab                        — crontab del sistema
# 3. /etc/cron.d/*                       — archivos drop-in del sistema
```

## Los 5 campos

Cada línea de un crontab define **cuándo** y **qué** ejecutar:

```
┌───────────── minuto       (0–59)
│ ┌─────────── hora         (0–23)
│ │ ┌───────── día del mes  (1–31)
│ │ │ ┌─────── mes          (1–12)
│ │ │ │ ┌───── día de la semana (0–7, donde 0 y 7 = domingo)
│ │ │ │ │
* * * * *  comando
```

```bash
# Ejemplos básicos:
30 2 * * *   /usr/local/bin/backup.sh
# Todos los días a las 02:30

0 9 1 * *    /usr/local/bin/report.sh
# El día 1 de cada mes a las 09:00

0 0 * * 0    /usr/local/bin/weekly-cleanup.sh
# Todos los domingos a medianoche
```

### Valores válidos por campo

| Campo | Rango | Notas |
|---|---|---|
| Minuto | 0–59 | |
| Hora | 0–23 | 0 = medianoche |
| Día del mes | 1–31 | Se ignoran silenciosamente los días que no existen (ej: 31 feb) |
| Mes | 1–12 | También acepta nombres: jan, feb, mar... (3 letras, inglés) |
| Día de la semana | 0–7 | 0 y 7 = domingo. También acepta nombres: sun, mon, tue... |

```bash
# Usando nombres de mes y día:
0 6 * jan,jul mon   /usr/local/bin/task.sh
# Lunes de enero y julio a las 06:00

# Los nombres no son case-sensitive en la mayoría de implementaciones:
0 6 * JAN,JUL MON   /usr/local/bin/task.sh
# Equivalente
```

## Caracteres especiales

### Asterisco `*` — Cualquier valor

```bash
* * * * *    /usr/local/bin/every-minute.sh
# Cada minuto de cada hora de cada día

0 * * * *    /usr/local/bin/every-hour.sh
# Al minuto 0 de cada hora (es decir, cada hora en punto)
```

### Coma `,` — Lista de valores

```bash
0 8,12,18 * * *   /usr/local/bin/check.sh
# A las 08:00, 12:00 y 18:00

0 0 1,15 * *      /usr/local/bin/bimonthly.sh
# Días 1 y 15 de cada mes a medianoche
```

### Guion `-` — Rango

```bash
0 9-17 * * 1-5    /usr/local/bin/business-hours.sh
# De lunes a viernes, de 09:00 a 17:00 (cada hora en punto)

30 6 1-7 * *      /usr/local/bin/first-week.sh
# Los primeros 7 días de cada mes a las 06:30
```

### Barra `/` — Intervalo (step)

```bash
*/5 * * * *       /usr/local/bin/every-5-min.sh
# Cada 5 minutos (0, 5, 10, 15, 20, 25, 30, 35, 40, 45, 50, 55)

0 */2 * * *       /usr/local/bin/every-2-hours.sh
# Cada 2 horas en punto (0, 2, 4, 6, 8, 10, 12, 14, 16, 18, 20, 22)

*/10 9-17 * * 1-5 /usr/local/bin/work-check.sh
# Cada 10 minutos durante horario laboral (lun-vie, 9-17)
```

### Combinaciones

```bash
# Los caracteres se pueden combinar:
0 8,12,18 1-15 */2 *   /usr/local/bin/complex.sh
# A las 08:00, 12:00 y 18:00
# Los primeros 15 días
# De meses pares (feb, abr, jun...)

# Rango con step:
0-30/10 * * * *   /usr/local/bin/task.sh
# Minutos 0, 10, 20, 30 de cada hora
```

## Día del mes Y día de la semana — OR, no AND

Un detalle crítico y fuente frecuente de errores:

```bash
# Cuando se especifican AMBOS (día del mes y día de la semana),
# cron los combina con OR:
0 9 15 * 5   /usr/local/bin/task.sh
# Ejecuta: el día 15 de cada mes O cualquier viernes
# NO: el día 15 solo si cae en viernes

# Esto es contraintuitivo. Los demás campos se combinan con AND:
0 9 * 6 1    /usr/local/bin/task.sh
# Todos los lunes de junio a las 09:00 (mes AND día de semana)

# Para "el primer viernes de cada mes" necesitas lógica en el script:
0 9 1-7 * 5  /usr/local/bin/first-friday.sh
# Ejecuta: días 1-7 O viernes (no solo el primer viernes)

# Solución con test en el comando:
0 9 1-7 * *  [ "$(date +\%u)" = 5 ] && /usr/local/bin/first-friday.sh
# Solo los primeros 7 días, y verifica que sea viernes (%u: 1=lun, 5=vie)
```

## Cadenas especiales

Muchas implementaciones de cron soportan alias para expresiones comunes:

```bash
@reboot     /usr/local/bin/on-start.sh
# Al arrancar el sistema (cuando crond inicia)

@yearly     /usr/local/bin/annual.sh
# Equivale a: 0 0 1 1 *  (1 de enero a medianoche)
# Alias: @annually

@monthly    /usr/local/bin/monthly.sh
# Equivale a: 0 0 1 * *  (día 1 de cada mes)

@weekly     /usr/local/bin/weekly.sh
# Equivale a: 0 0 * * 0  (domingo a medianoche)

@daily      /usr/local/bin/daily.sh
# Equivale a: 0 0 * * *  (medianoche)
# Alias: @midnight

@hourly     /usr/local/bin/hourly.sh
# Equivale a: 0 * * * *  (cada hora en punto)
```

```bash
# @reboot es especialmente útil:
@reboot  sleep 30 && /usr/local/bin/startup-check.sh
# Esperar 30 segundos después del boot y ejecutar
# El sleep da tiempo a que los servicios estén listos
```

## Gestionar crontabs de usuario

```bash
# Editar el crontab del usuario actual:
crontab -e
# Abre el crontab en $EDITOR (o vi por defecto)
# Al guardar, cron valida la sintaxis y lo instala

# Ver el crontab del usuario actual:
crontab -l
# Muestra las líneas del crontab (sin comentarios del sistema)

# Eliminar el crontab del usuario actual:
crontab -r
# CUIDADO: elimina TODO el crontab sin confirmación

# Eliminar con confirmación (si está disponible):
crontab -ri
# Pide confirmación antes de eliminar

# Como root, gestionar el crontab de otro usuario:
sudo crontab -u www-data -l
sudo crontab -u www-data -e
```

### Dónde se almacenan

```bash
# Debian/Ubuntu:
ls /var/spool/cron/crontabs/
# Un archivo por usuario que tenga crontab

# RHEL/Fedora:
ls /var/spool/cron/
# Un archivo por usuario

# NUNCA editar estos archivos directamente
# Usar siempre crontab -e (valida la sintaxis)
```

## Salida y redirección

Por defecto, cron envía la salida (stdout y stderr) por **correo** al
usuario propietario del crontab:

```bash
# Si el comando produce salida, cron intenta enviar un mail
# Si no hay MTA configurado, la salida se pierde silenciosamente

# Redirigir stdout a un log:
0 2 * * * /usr/local/bin/backup.sh >> /var/log/backup.log 2>&1
# >> = append (no sobreescribir)
# 2>&1 = redirigir stderr a stdout (mismo archivo)

# Descartar toda la salida:
0 2 * * * /usr/local/bin/backup.sh > /dev/null 2>&1
# Cuidado: si el script falla, no hay rastro

# Solo capturar errores:
0 2 * * * /usr/local/bin/backup.sh > /dev/null 2>> /var/log/backup-errors.log

# Con timestamp:
0 2 * * * /usr/local/bin/backup.sh >> /var/log/backup.log 2>&1
# Mejor que el script incluya sus propios timestamps en la salida
```

## Errores comunes

### El % tiene significado especial

```bash
# En crontab, el carácter % se interpreta como nueva línea
# y lo que sigue al primer % se pasa como stdin al comando

# INCORRECTO:
0 2 * * * echo "Backup $(date +%Y-%m-%d)" >> /var/log/cron.log
# Falla: %Y, %m, %d se interpretan como nuevas líneas

# CORRECTO — escapar con backslash:
0 2 * * * echo "Backup $(date +\%Y-\%m-\%d)" >> /var/log/cron.log

# CORRECTO — mover la lógica a un script:
0 2 * * * /usr/local/bin/backup.sh
# Y en el script usar % libremente
```

### Rutas absolutas

```bash
# cron NO carga el profile del usuario
# El PATH es mínimo (típicamente /usr/bin:/bin)

# INCORRECTO:
0 * * * * my-script.sh

# CORRECTO:
0 * * * * /usr/local/bin/my-script.sh

# O definir PATH al inicio del crontab:
PATH=/usr/local/bin:/usr/bin:/bin
0 * * * * my-script.sh
```

### Permisos

```bash
# El script debe ser ejecutable:
chmod +x /usr/local/bin/backup.sh

# El usuario debe tener permiso para usar cron:
# /etc/cron.allow — si existe, solo estos usuarios pueden usar cron
# /etc/cron.deny  — si existe (y no hay cron.allow), estos usuarios NO pueden
# Si ninguno existe: solo root (Debian) o todos (RHEL)
```

## Verificar que cron funciona

```bash
# 1. Verificar que el daemon está activo:
systemctl status cron     # Debian
systemctl status crond    # RHEL

# 2. Probar con una tarea de 1 minuto:
crontab -e
# Agregar:
# * * * * * echo "cron works $(date)" >> /tmp/cron-test.log
# Esperar 1-2 minutos y verificar:
cat /tmp/cron-test.log

# 3. Revisar los logs de cron:
grep CRON /var/log/syslog        # Debian
grep cron /var/log/cron          # RHEL
journalctl -u cron --since "5 min ago"   # Debian con journal
journalctl -u crond --since "5 min ago"  # RHEL con journal
```

---

## Ejercicios

### Ejercicio 1 — Leer expresiones cron

```bash
# ¿Cuándo se ejecuta cada una?
# a) 30 4 * * *
# b) 0 */6 * * 1-5
# c) 15,45 9-17 * * *
# d) 0 0 1 1,4,7,10 *
# e) */5 * * * 0
```

### Ejercicio 2 — Escribir expresiones cron

```bash
# Escribir la expresión cron para:
# a) Cada día a las 03:15
# b) Cada lunes y miércoles a las 08:00
# c) Cada 15 minutos de lunes a viernes
# d) El último día del año a las 23:59
# e) Cada 2 horas entre las 06:00 y las 22:00
```

### Ejercicio 3 — Listar y verificar

```bash
# ¿Tienes algún crontab definido?
crontab -l 2>/dev/null || echo "No crontab"

# ¿Hay crontabs de otros usuarios?
sudo ls /var/spool/cron/crontabs/ 2>/dev/null || \
    sudo ls /var/spool/cron/ 2>/dev/null
```
