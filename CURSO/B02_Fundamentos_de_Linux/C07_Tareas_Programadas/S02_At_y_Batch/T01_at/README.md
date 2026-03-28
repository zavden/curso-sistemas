# T01 — at

## Qué es at

`at` ejecuta un comando **una sola vez** en un momento futuro. A diferencia
de cron (que repite tareas periódicamente), at es para ejecuciones únicas:

```bash
# cron  → "cada día a las 03:00"
# at    → "mañana a las 03:00" (una sola vez)

# Ejemplo rápido:
echo "/usr/local/bin/backup.sh" | at 03:00 tomorrow
# job 5 at Tue Mar 18 03:00:00 2026
```

## Instalación y servicio

```bash
# Debian/Ubuntu:
sudo apt install at
# El paquete se llama "at"

# RHEL/Fedora:
sudo dnf install at
# El paquete se llama "at"

# El daemon que ejecuta las tareas:
systemctl status atd
# Active: active (running)

# Habilitar si no está activo:
sudo systemctl enable --now atd
```

## Sintaxis de tiempo

### Formatos básicos

```bash
# Hora específica (hoy o mañana si ya pasó):
at 15:30
at 3:30 PM
at 3:30PM

# Fecha y hora:
at 15:30 2026-03-20
at 15:30 Mar 20
at 3:30 PM March 20, 2026

# Palabras clave de tiempo:
at now
at noon             # 12:00
at midnight         # 00:00
at teatime          # 16:00
at tomorrow
at now + 1 hour
at now + 30 minutes
at now + 2 days
at now + 1 week
```

### Expresiones relativas

```bash
# Sumar tiempo a una referencia:
at now + 5 minutes
at now + 2 hours
at now + 3 days
at now + 1 week

# Sumar a una hora específica:
at 10:00 + 2 days     # pasado mañana a las 10:00

# Unidades válidas: minutes, hours, days, weeks
# También acepta singular: minute, hour, day, week
```

### Formatos de fecha

```bash
# ISO (recomendado):
at 15:00 2026-03-20

# Americano:
at 15:00 03/20/2026     # MM/DD/YYYY

# Con nombre de mes:
at 15:00 Mar 20
at 15:00 March 20, 2026

# Día de la semana:
at 15:00 Friday
at 15:00 next Friday

# Si la hora ya pasó hoy, at la programa para mañana:
at 08:00    # si son las 10:00, se programa para mañana a las 08:00
```

## Crear tareas con at

### Modo interactivo

```bash
at 14:00 tomorrow
# at> /usr/local/bin/backup.sh
# at> echo "Backup completado" | mail -s "Backup" admin@example.com
# at> <Ctrl+D>
# job 7 at Wed Mar 18 14:00:00 2026

# at abre un prompt donde puedes escribir múltiples comandos
# Se ejecutan todos secuencialmente
# Ctrl+D (EOT) para terminar
```

### Desde stdin (pipe o heredoc)

```bash
# Con pipe:
echo "/usr/local/bin/deploy.sh" | at 22:00

# Múltiples comandos con pipe:
echo "cd /opt/myapp && git pull && systemctl restart myapp" | at 02:00

# Con heredoc:
at 03:00 tomorrow << 'EOF'
/usr/local/bin/backup.sh
/usr/local/bin/cleanup.sh
echo "Mantenimiento nocturno completado" | mail -s "Maint" admin@example.com
EOF
```

### Desde un archivo

```bash
# Crear el archivo con los comandos:
cat > /tmp/maintenance-tasks.sh << 'EOF'
#!/bin/bash
/usr/local/bin/backup.sh
/usr/local/bin/optimize-db.sh
logger "Mantenimiento programado completado"
EOF

# Programar con -f:
at -f /tmp/maintenance-tasks.sh 03:00 tomorrow
# job 8 at Wed Mar 18 03:00:00 2026
```

## Entorno de ejecución

at captura el entorno actual al momento de crear la tarea y lo
restaura al ejecutarla:

```bash
# A diferencia de cron, at preserva:
# - El directorio de trabajo actual (pwd)
# - Todas las variables de entorno
# - umask

# Esto significa que:
cd /opt/myapp
MY_VAR="hello"
echo 'echo "$MY_VAR from $(pwd)"' | at now + 1 minute
# Ejecutará en /opt/myapp con MY_VAR=hello

# El entorno se almacena en el archivo de la tarea:
# /var/spool/at/ o /var/spool/atjobs/
# Contiene un script shell completo con el entorno exportado
```

```bash
# Sin embargo, at NO hereda:
# - La sesión de terminal (no hay TTY)
# - Variables de display (DISPLAY no funciona para GUI)
# - El shell interactivo (se ejecuta con /bin/sh por defecto)

# Si necesitas bash:
echo '/bin/bash -c "source ~/.bashrc && my-command"' | at now + 5 min

# O usar -f con un script que tenga shebang:
at -f /path/to/script.sh 15:00    # el shebang del script se respeta
```

## Gestionar la cola — atq

```bash
# Listar todas las tareas pendientes:
atq
# 7    Wed Mar 18 14:00:00 2026 a user
# 8    Wed Mar 18 03:00:00 2026 a user
# 12   Thu Mar 19 10:00:00 2026 a root

# Columnas:
# ID   Fecha/hora programada      Cola  Usuario
#  7   Wed Mar 18 14:00:00 2026    a     user

# La cola "a" es la cola por defecto de at
# La cola "b" es la cola de batch
# Las colas c-z se pueden usar con at -q
```

```bash
# Como root, ver las tareas de todos los usuarios:
sudo atq

# Solo ver las propias:
atq
```

## Ver el contenido de una tarea

```bash
# Ver qué ejecutará una tarea pendiente:
at -c 7
# Muestra el script completo, incluyendo:
# - El entorno capturado (variables exportadas)
# - El directorio de trabajo
# - umask
# - Los comandos del usuario al final

# Solo ver los comandos (omitir el preámbulo del entorno):
at -c 7 | tail -5
# /usr/local/bin/backup.sh
# echo "Backup completado" | mail -s "Backup" admin@example.com
```

## Eliminar tareas — atrm

```bash
# Eliminar una tarea por su ID:
atrm 7
# Sin salida = éxito

# Eliminar múltiples:
atrm 7 8 12

# Verificar que se eliminó:
atq
# (la tarea 7 ya no aparece)

# Alternativa:
at -r 7      # equivale a atrm 7 (en algunos sistemas)
at -d 7      # otra alternativa
```

## Colas de at

at soporta múltiples colas identificadas con letras (a-z):

```bash
# Cola por defecto: a
at 15:00
# Usa la cola "a"

# Especificar una cola:
at -q c 15:00
# Usa la cola "c"

# La cola "b" está reservada para batch (ver T02)

# Las colas de mayor letra tienen menor prioridad (nice):
# Cola a → nice 0
# Cola b → nice 2 (batch)
# Cola c → nice 4
# Cola z → nice 50
# Mayor nice = menor prioridad de CPU

# Ver la cola de cada tarea:
atq
# 7    Wed Mar 18 14:00:00 2026 a user    ← cola "a"
# 8    Wed Mar 18 14:00:00 2026 c user    ← cola "c"
```

## Control de acceso

```bash
# Dos archivos controlan quién puede usar at:
# /etc/at.allow  — si existe, solo estos usuarios pueden usar at
# /etc/at.deny   — si existe (y no hay at.allow), estos NO pueden

# Lógica de evaluación:
# 1. Si /etc/at.allow existe → solo los usuarios listados pueden usar at
# 2. Si /etc/at.allow NO existe pero /etc/at.deny SÍ:
#    → todos pueden EXCEPTO los listados en at.deny
# 3. Si NINGUNO existe:
#    → solo root puede usar at (Debian)
#    → todos pueden usar at (RHEL, at.deny vacío existe por defecto)

# Verificar:
ls -la /etc/at.allow /etc/at.deny 2>/dev/null
cat /etc/at.deny 2>/dev/null
```

```bash
# Debian/Ubuntu:
# /etc/at.deny existe por defecto (vacío → todos pueden usar at)

# RHEL/Fedora:
# /etc/at.deny existe por defecto (vacío → todos pueden usar at)

# Restringir a usuarios específicos:
sudo bash -c 'echo "admin" > /etc/at.allow'
sudo bash -c 'echo "deploy" >> /etc/at.allow'
# Ahora solo admin y deploy pueden usar at
# root siempre puede usar at
```

## Salida y notificación

```bash
# Por defecto, at envía la salida (stdout + stderr) por mail
# al usuario que creó la tarea

# Enviar mail incluso si no hay salida:
at -m 15:00 << 'EOF'
/usr/local/bin/task.sh > /dev/null 2>&1
EOF
# -m fuerza el envío de mail al completar (notificación de ejecución)

# Redirigir salida a un log (práctica recomendada):
echo '/usr/local/bin/task.sh >> /var/log/at-tasks.log 2>&1' | at 15:00
```

## Casos de uso comunes

### Programar un reinicio

```bash
# Reiniciar el servidor en 2 horas:
echo "shutdown -r now" | sudo at now + 2 hours
# job 15 at Tue Mar 17 17:00:00 2026

# Verificar que está programado:
sudo atq
# 15   Tue Mar 17 17:00:00 2026 a root

# Cancelar si cambias de opinión:
sudo atrm 15
```

### Tarea post-despliegue

```bash
# Después de un deploy, verificar en 10 minutos:
at now + 10 minutes << 'EOF'
STATUS=$(curl -s -o /dev/null -w "%{http_code}" http://localhost:8080/health)
if [ "$STATUS" != "200" ]; then
    echo "ALERTA: health check falló (HTTP $STATUS)" | \
        mail -s "Deploy health check FAILED" ops@example.com
fi
EOF
```

### Limpieza diferida

```bash
# Eliminar archivos temporales mañana:
echo 'find /tmp/build-* -mtime +1 -delete 2>/dev/null' | at 04:00 tomorrow

# Desactivar una cuenta temporalmente:
echo 'usermod -L tempuser' | sudo at 18:00 Friday
```

### Recordatorio

```bash
# Recordarse algo (requiere terminal notificaciones o mail):
echo 'echo "Reunión en 15 minutos" | wall' | sudo at 09:45
# wall envía un mensaje a todos los terminales activos

# O con notify-send (si hay sesión gráfica):
echo 'DISPLAY=:0 notify-send "Reunión en 15 min"' | at 09:45
```

## Dónde se almacenan las tareas

```bash
# Las tareas pendientes se almacenan en:
ls /var/spool/at/          # Debian
ls /var/spool/atjobs/      # Algunos sistemas

# Cada tarea es un archivo ejecutable con:
# 1. El entorno capturado (exportado como variables)
# 2. cd al directorio de trabajo original
# 3. Los comandos del usuario

# Las tareas ejecutadas se eliminan automáticamente
# No hay historial de tareas completadas (a diferencia de systemd timers)
```

## at vs cron vs systemd timers

| Aspecto | at | cron | systemd timer |
|---|---|---|---|
| Ejecución | Una vez | Repetitiva | Repetitiva o una vez |
| Precisión | Minuto | Minuto | Segundo |
| Captura entorno | Sí | No | Definido en .service |
| Historial | No | No | Sí (journal) |
| Persistencia | Sí (disco) | Sí | Sí (Persistent=) |
| Usuarios | Cualquiera | Cualquiera | Root o user units |

---

## Ejercicios

### Ejercicio 1 — Crear y listar tareas

```bash
# 1. Programar una tarea para dentro de 2 minutos:
echo 'echo "at funciona: $(date)" > /tmp/at-test.txt' | at now + 2 minutes

# 2. Verificar que está en la cola:
atq

# 3. Ver qué ejecutará:
at -c $(atq | tail -1 | awk '{print $1}')

# 4. Esperar 2 minutos y verificar:
# cat /tmp/at-test.txt
```

### Ejercicio 2 — Formatos de tiempo

```bash
# ¿Son válidas estas expresiones? Probar con un comando inofensivo:
# a) at teatime tomorrow
# b) at now + 30 seconds
# c) at noon next Friday
# d) at 25:00
# e) at midnight + 1 day
```

### Ejercicio 3 — Cola y control de acceso

```bash
# 1. ¿Qué archivo de control de acceso existe en tu sistema?
ls -la /etc/at.allow /etc/at.deny 2>/dev/null

# 2. ¿Puedes usar at como usuario normal?
echo "echo test" | at now + 1 hour 2>/dev/null && \
    echo "Sí, puedo usar at" && atrm $(atq | tail -1 | awk '{print $1}') || \
    echo "No puedo usar at"
```
