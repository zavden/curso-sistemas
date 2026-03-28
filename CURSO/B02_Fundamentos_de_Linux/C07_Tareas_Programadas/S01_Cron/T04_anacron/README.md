# T04 — anacron

## El problema que resuelve anacron

cron asume que el sistema está **siempre encendido**. Si una tarea diaria
está programada para las 03:00 y el equipo está apagado a esa hora, la
tarea no se ejecuta y no se recupera:

```
cron:
  03:00 → ejecutar backup diario
  Si el equipo estaba apagado a las 03:00 → la tarea se pierde

anacron:
  "ejecutar backup diario"
  Si no se ejecutó hoy → ejecutarlo ahora (con un delay aleatorio)
  Registrar la fecha de última ejecución para no repetir
```

```bash
# anacron garantiza que las tareas se ejecuten:
# - No importa si el equipo estuvo apagado
# - Verifica la última ejecución y ejecuta las pendientes
# - Agrega un delay aleatorio para evitar picos de carga

# Perfecto para:
# - Laptops que se suspenden/apagan regularmente
# - Estaciones de trabajo con horarios variables
# - Máquinas virtuales que no están siempre activas
```

## Cómo funciona

```
                     anacron
                        │
                        ▼
            Lee /etc/anacrontab
                        │
               ┌────────┼────────┐
               │        │        │
          "daily"  "weekly" "monthly"
               │        │        │
               ▼        ▼        ▼
       ¿Cuándo fue la última ejecución?
       (lee /var/spool/anacron/cron.{daily,weekly,monthly})
               │
               ├── Hace menos del período → no hacer nada
               │
               └── Hace más del período → ejecutar
                        │
                        ▼
                   Esperar delay
                        │
                        ▼
                   Ejecutar los scripts
                        │
                        ▼
                   Actualizar timestamp
                   en /var/spool/anacron/
```

```bash
# Los timestamps de última ejecución:
cat /var/spool/anacron/cron.daily
# 20260317

cat /var/spool/anacron/cron.weekly
# 20260314

cat /var/spool/anacron/cron.monthly
# 20260301

# Son simplemente la fecha (YYYYMMDD) de la última ejecución
```

## /etc/anacrontab

```bash
cat /etc/anacrontab
```

```bash
# Formato:
# período(días)  delay(minutos)  identificador  comando

# /etc/anacrontab (Debian/Ubuntu):
SHELL=/bin/sh
PATH=/usr/local/sbin:/usr/local/bin:/sbin:/bin:/usr/sbin:/usr/bin
HOME=/root
LOGNAME=root

1    5    cron.daily    run-parts --report /etc/cron.daily
7   10    cron.weekly   run-parts --report /etc/cron.weekly
@monthly 15  cron.monthly  run-parts --report /etc/cron.monthly
```

### Los 4 campos

```bash
# Campo 1: Período (en días)
# 1        = diario
# 7        = semanal
# 30       = mensual
# @monthly = mensual (último día del mes anterior como referencia)

# Campo 2: Delay (en minutos)
# Después de que anacron decide ejecutar, espera N minutos
# Evita que todas las tareas se ejecuten simultáneamente
# Reduce picos de I/O y CPU

# Campo 3: Identificador
# Nombre único para rastrear la última ejecución
# Se usa como nombre de archivo en /var/spool/anacron/

# Campo 4: Comando
# El comando a ejecutar (típicamente run-parts sobre un directorio)
```

### Variables de entorno

```bash
# anacron acepta las mismas variables que crontab:
SHELL=/bin/bash
PATH=/usr/local/bin:/usr/bin:/bin
HOME=/root
LOGNAME=root
MAILTO=admin@example.com

# START_HOURS_RANGE — horas válidas para ejecutar tareas:
START_HOURS_RANGE=3-22
# Solo ejecutar entre las 03:00 y las 22:00
# Si anacron se invoca fuera de este rango, espera

# RANDOM_DELAY — delay aleatorio adicional (minutos):
RANDOM_DELAY=45
# Agrega entre 0 y 45 minutos al delay del campo 2
# Útil para que múltiples máquinas no hagan lo mismo a la vez
```

## Relación entre cron y anacron

En la mayoría de distribuciones modernas, cron y anacron trabajan juntos:

```bash
# /etc/crontab (Debian):
25 6 * * *   root  test -x /usr/sbin/anacron || run-parts --report /etc/cron.daily

# Lógica:
# 1. cron evalúa la línea a las 06:25
# 2. test -x /usr/sbin/anacron → ¿existe anacron?
#    - SÍ → el test es true → || no se ejecuta → cron NO ejecuta run-parts
#      (anacron se encargará cuando se invoque)
#    - NO → el test falla → || ejecuta run-parts → cron ejecuta los scripts

# ¿Quién invoca a anacron?
# - En Debian: el timer de systemd (anacron.timer) o cron.daily
# - En RHEL: cronie-anacron integrado en crond
```

### Cómo se invoca anacron

```bash
# Opción 1 — systemd timer (Debian moderno):
systemctl cat anacron.timer 2>/dev/null
# [Timer]
# OnCalendar=*-*-* 07:30:00
# Persistent=true
# RandomizedDelaySec=5min

# Opción 2 — cron job (Debian legacy / RHEL):
cat /etc/cron.d/anacron 2>/dev/null
# 30 7 * * * root /usr/sbin/anacron -s

# Opción 3 — integrado en crond (RHEL con cronie):
# crond ejecuta anacron internamente al detectar tareas pendientes

# En cualquier caso, anacron se ejecuta periódicamente y verifica
# qué tareas tienen pendientes
```

### Diferencias cron vs anacron

| Aspecto | cron | anacron |
|---|---|---|
| Precisión | Minuto exacto | Solo días |
| Equipo apagado | Pierde la tarea | Ejecuta al encender |
| Daemon | Sí (siempre corriendo) | No (se invoca y termina) |
| Usuarios | Cualquiera | Solo root |
| Mínimo intervalo | 1 minuto | 1 día |
| Hora exacta | Sí | No (delay aleatorio) |
| Uso principal | Servidores 24/7 | Laptops, escritorios |

## anacron en RHEL vs Debian

### Debian/Ubuntu

```bash
# anacron es un paquete separado (instalado por defecto):
dpkg -l anacron
# ii  anacron  2.3-...

# Se invoca mediante systemd timer:
systemctl status anacron.timer
# Active: active (waiting)
# Trigger: tomorrow at 07:30:00

# O mediante /etc/cron.d/anacron:
cat /etc/cron.d/anacron
# 30 7 * * * root test -x /etc/init.d/anacron && /usr/sbin/anacron -s

# anacron ejecuta run-parts sobre /etc/cron.{daily,weekly,monthly}
# cron ejecuta /etc/cron.hourly (anacron no maneja horas)
```

### RHEL/Fedora

```bash
# anacron está integrado en cronie (cronie-anacron):
rpm -q cronie-anacron
# cronie-anacron-1.x.x

# crond maneja anacron internamente:
# No hay daemon anacron separado
# crond detecta tareas anacron y las ejecuta

# La configuración sigue siendo /etc/anacrontab
# Los timestamps siguen en /var/spool/anacron/
```

## Opciones de línea de comandos

```bash
# Ejecutar anacron manualmente:
sudo anacron -f
# -f = force: ejecutar todas las tareas aunque no estén pendientes

sudo anacron -fn
# -f = force
# -n = now: ignorar delays (no esperar)

sudo anacron -d
# -d = debug: no fork al background, mostrar mensajes

# Solo verificar qué se ejecutaría (sin ejecutar):
sudo anacron -T
# Testear sintaxis de /etc/anacrontab

# Ejecutar con un anacrontab alternativo:
sudo anacron -t /path/to/alt-anacrontab

# Usar un directorio de spool alternativo:
sudo anacron -S /path/to/spool
```

## Agregar tareas a anacron

```bash
# Opción 1 — Agregar un script a /etc/cron.daily/ (más común):
sudo tee /etc/cron.daily/myapp-maintenance << 'EOF'
#!/bin/bash
# Mantenimiento diario de myapp
/opt/myapp/bin/maintenance --cleanup --optimize
logger "myapp-maintenance: completado"
EOF
sudo chmod +x /etc/cron.daily/myapp-maintenance

# anacron ejecutará este script automáticamente
# (porque ejecuta run-parts /etc/cron.daily)
```

```bash
# Opción 2 — Agregar una línea en /etc/anacrontab (tareas custom):
sudo tee -a /etc/anacrontab << 'EOF'

# Verificación de integridad cada 3 días
3    20    integrity-check    /usr/local/bin/integrity-check.sh
EOF

# Esto crea una tarea que:
# - Se ejecuta cada 3 días
# - Con un delay de 20 minutos
# - Se rastrea con el identificador "integrity-check"
# - El timestamp se guarda en /var/spool/anacron/integrity-check
```

## Limitaciones de anacron

```bash
# 1. Solo root puede usar anacron
#    No hay "anacrontab de usuario"
#    Alternativa para usuarios: systemd user timers

# 2. Precisión mínima de 1 día
#    No puedes programar "cada 4 horas con garantía de ejecución"
#    Alternativa: systemd timers con Persistent=true

# 3. No soporta horas específicas
#    "Ejecutar el backup a las 03:00" → no es posible con anacron
#    anacron dice "ejecutar una vez al día, cuando pueda"
#    El delay solo controla cuántos minutos esperar después de decidir ejecutar

# 4. START_HOURS_RANGE puede bloquear ejecuciones
#    Si el equipo se enciende fuera del rango, anacron no ejecuta nada
#    hasta que se esté dentro del rango (o se invoque con -f)
```

## systemd timers como alternativa moderna

```bash
# Los systemd timers cubren los mismos casos que anacron + cron:
# - Persistent=true → ejecutar tareas perdidas (como anacron)
# - OnCalendar → precisión de segundo (como cron)
# - RandomizedDelaySec → delay aleatorio (como anacron RANDOM_DELAY)
# - Logs integrados en journalctl
# - Sin límites de solo-root

# anacron sigue siendo relevante porque:
# 1. Es más simple para casos básicos
# 2. Viene preconfigurado para /etc/cron.{daily,weekly,monthly}
# 3. No requiere crear dos archivos (.timer + .service)
# 4. Ecosistema enorme de paquetes que instalan scripts en cron.daily/
```

---

## Ejercicios

### Ejercicio 1 — Estado de anacron

```bash
# 1. ¿Está anacron instalado?
which anacron 2>/dev/null && echo "Instalado" || echo "No instalado"

# 2. ¿Cuándo se ejecutaron las tareas por última vez?
for f in /var/spool/anacron/*; do
    echo "$(basename "$f"): $(cat "$f" 2>/dev/null)"
done

# 3. ¿Quién invoca a anacron?
systemctl is-active anacron.timer 2>/dev/null && echo "Via systemd timer"
cat /etc/cron.d/anacron 2>/dev/null && echo "Via cron"
```

### Ejercicio 2 — Configuración actual

```bash
# Ver el anacrontab actual:
cat /etc/anacrontab 2>/dev/null

# ¿Cuál es el rango horario permitido?
grep START_HOURS_RANGE /etc/anacrontab 2>/dev/null || echo "Default: 3-22"

# ¿Cuál es el delay aleatorio máximo?
grep RANDOM_DELAY /etc/anacrontab 2>/dev/null || echo "No definido"
```

### Ejercicio 3 — Verificar la relación cron/anacron

```bash
# ¿cron delega en anacron?
grep anacron /etc/crontab 2>/dev/null
# Si hay "test -x /usr/sbin/anacron ||" → cron delega en anacron
# Si no → cron ejecuta directamente
```
