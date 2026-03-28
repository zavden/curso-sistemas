# T01 — at

> **Objetivo:** Dominar el comando `at` para ejecución única programada: sintaxis de tiempo, crear tareas (pipe, heredoc, archivo), gestionar la cola (`atq`, `atrm`, `at -c`), colas de prioridad, control de acceso, captura de entorno, y casos de uso comunes.

## Sin erratas

El `README.md` base no presenta errores técnicos.

---

## Qué es at

`at` ejecuta un comando **una sola vez** en un momento futuro. A diferencia de cron (que repite tareas periódicamente), at es para ejecuciones únicas:

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

# RHEL/Fedora:
sudo dnf install at

# El daemon que ejecuta las tareas:
systemctl status atd

# Habilitar si no está activo:
sudo systemctl enable --now atd
```

---

## Sintaxis de tiempo

### Formatos básicos

```bash
# Hora específica (hoy o mañana si ya pasó):
at 15:30
at 3:30 PM

# Palabras clave de tiempo:
at now               # ahora
at noon              # 12:00
at midnight          # 00:00
at teatime           # 16:00
at tomorrow          # mañana

# Expresiones relativas:
at now + 5 minutes
at now + 2 hours
at now + 3 days
at now + 1 week

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

# Combinar hora con offset:
at 10:00 + 2 days       # pasado mañana a las 10:00

# Si la hora ya pasó hoy, at la programa para mañana:
at 08:00    # si son las 10:00, se programa para mañana a las 08:00
```

---

## Crear tareas con at

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
# Programar con -f:
at -f /tmp/maintenance-tasks.sh 03:00 tomorrow
# job 8 at Wed Mar 18 03:00:00 2026
```

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

---

## Entorno de ejecución

at captura el entorno actual al momento de crear la tarea y lo restaura al ejecutarla:

```bash
# A diferencia de cron, at preserva:
# - El directorio de trabajo actual (pwd)
# - Todas las variables de entorno
# - umask

cd /opt/myapp
MY_VAR="hello"
echo 'echo "$MY_VAR from $(pwd)"' | at now + 1 minute
# Ejecutará en /opt/myapp con MY_VAR=hello
```

```bash
# Sin embargo, at NO hereda:
# - La sesión de terminal (no hay TTY)
# - DISPLAY (aplicaciones X11/GUI no funcionan)
# - El shell interactivo (se ejecuta con /bin/sh por defecto)

# Si necesitas bash:
echo '/bin/bash -c "source ~/.bashrc && my-command"' | at now + 5 min

# O usar -f con un script que tenga shebang:
at -f /path/to/script.sh 15:00    # el shebang del script se respeta
```

### Comparación con cron

| Aspecto | at | cron |
|---------|-----|------|
| Captura entorno | Sí (completo) | No (PATH mínimo) |
| Directorio | pwd actual | HOME del usuario |
| SHELL | /bin/sh | /bin/sh |
| Variables | Todas las del momento | Solo las definidas en crontab |

---

## Gestionar la cola — atq

```bash
# Listar todas las tareas pendientes:
atq
# 7    Wed Mar 18 14:00:00 2026 a user
# 8    Wed Mar 18 03:00:00 2026 a user
# 12   Thu Mar 19 10:00:00 2026 a root

# Columnas:
# ID   Fecha/hora programada      Cola  Usuario

# La cola "a" es la cola por defecto de at
# La cola "b" es la cola de batch
# Las colas c-z se pueden usar con at -q

# Como root, ver las tareas de todos los usuarios:
sudo atq
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
```

## Eliminar tareas — atrm

```bash
# Eliminar una tarea por su ID:
atrm 7

# Eliminar múltiples:
atrm 7 8 12

# Verificar que se eliminó:
atq

# Alternativas:
at -r 7      # equivale a atrm 7 (en algunos sistemas)
at -d 7      # otra alternativa
```

---

## Colas de at

at soporta múltiples colas identificadas con letras (a-z):

| Cola | Nice | Uso |
|------|------|-----|
| a | 0 | at normal (default) |
| b | 2 | batch |
| c | 4 | prioridad reducida |
| ... | ... | ... |
| z | 50 | prioridad muy baja |

```bash
# Especificar una cola:
at -q c 15:00
# Usa la cola "c" (nice 4)

# Mayor letra = mayor nice = menor prioridad de CPU
```

---

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
#    → todos pueden (RHEL, at.deny vacío existe por defecto)

# root siempre puede usar at

# Verificar:
ls -la /etc/at.allow /etc/at.deny 2>/dev/null
```

---

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

---

## Casos de uso comunes

### Programar un reinicio

```bash
echo "shutdown -r now" | sudo at now + 2 hours
# Cancelar si cambias de opinión:
sudo atrm 15
```

### Verificación post-despliegue

```bash
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
echo 'find /tmp/build-* -mtime +1 -delete 2>/dev/null' | at 04:00 tomorrow
```

### Recordatorio

```bash
# wall envía un mensaje a todos los terminales activos:
echo 'echo "Reunión en 15 minutos" | wall' | sudo at 09:45
```

---

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
# No hay historial de tareas completadas
```

---

## at vs cron vs systemd timers

| Aspecto | at | cron | systemd timer |
|---|---|---|---|
| Ejecución | Una vez | Repetitiva | Repetitiva o una vez |
| Precisión | Minuto | Minuto | Segundo |
| Captura entorno | Sí | No | Definido en .service |
| Historial | No | No | Sí (journal) |
| Persistencia | Sí (disco) | Sí | Sí (Persistent=) |
| Usuarios | Cualquiera | Cualquiera | Root o user units |

```bash
# Alternativa moderna con systemd:
systemd-run --on-active=2h /usr/local/bin/task.sh
# Crea un timer transient que ejecuta en 2 horas
# Equivale a: echo /usr/local/bin/task.sh | at now + 2 hours
```

---

## Lab — at

### Parte 1 — Sintaxis y creación

#### Paso 1.1: at vs cron

```bash
docker compose exec debian-dev bash -c '
echo "=== at vs cron ==="
echo ""
echo "cron → \"cada dia a las 03:00\" (repetitiva)"
echo "at   → \"manana a las 03:00\" (una sola vez)"
echo ""
echo "--- Verificar instalacion ---"
which at 2>/dev/null && echo "at: instalado" || \
    echo "at: no instalado (paquete: at)"
echo ""
echo "--- Daemon ---"
systemctl is-active atd 2>/dev/null && echo "atd: ACTIVO" || \
    echo "atd: NO ACTIVO (normal en contenedores)"
'
```

#### Paso 1.2: Formatos de tiempo

```bash
docker compose exec debian-dev bash -c '
echo "=== Formatos de tiempo de at ==="
echo ""
echo "--- Hora especifica ---"
echo "at 15:30               hoy (o manana si ya paso)"
echo "at 3:30 PM             formato 12 horas"
echo ""
echo "--- Palabras clave ---"
echo "at now                 ahora"
echo "at noon                12:00"
echo "at midnight            00:00"
echo "at teatime             16:00"
echo "at tomorrow            manana"
echo ""
echo "--- Expresiones relativas ---"
echo "at now + 5 minutes     en 5 minutos"
echo "at now + 2 hours       en 2 horas"
echo "at now + 3 days        en 3 dias"
echo "at now + 1 week        en 1 semana"
echo ""
echo "--- Fecha y hora ---"
echo "at 15:00 2026-03-20    ISO (recomendado)"
echo "at 15:00 Mar 20        nombre de mes"
echo "at 15:00 Friday        dia de la semana"
echo "at 15:00 next Friday   proximo viernes"
'
```

#### Paso 1.3: Crear tareas

```bash
docker compose exec debian-dev bash -c '
echo "=== Crear tareas con at ==="
echo ""
echo "--- Desde pipe ---"
echo "echo \"/usr/local/bin/backup.sh\" | at 03:00"
echo ""
echo "--- Desde heredoc ---"
echo "at 03:00 tomorrow << EOF"
echo "/usr/local/bin/backup.sh"
echo "/usr/local/bin/cleanup.sh"
echo "EOF"
echo ""
echo "--- Desde archivo (-f) ---"
echo "at -f /tmp/tasks.sh 03:00 tomorrow"
echo ""
echo "--- Modo interactivo ---"
echo "at 14:00 tomorrow"
echo "at> /usr/local/bin/backup.sh"
echo "at> <Ctrl+D>"
echo ""
echo "--- Forzar mail de notificacion ---"
echo "at -m 15:00    enviar mail al completar (aun sin salida)"
'
```

---

### Parte 2 — Gestión de tareas

#### Paso 2.1: Listar tareas (atq)

```bash
docker compose exec debian-dev bash -c '
echo "=== Listar tareas: atq ==="
echo ""
echo "--- Formato ---"
echo "ID   Fecha/hora programada      Cola  Usuario"
echo " 7   Wed Mar 18 14:00:00 2026    a     user"
echo ""
echo "--- Colas ---"
echo "a = cola por defecto de at"
echo "b = cola de batch"
echo "c-z = colas adicionales (mayor letra = menor prioridad)"
echo ""
echo "--- Verificar ---"
if command -v atq &>/dev/null; then
    echo "Cola actual:"
    atq 2>/dev/null || echo "  (sin tareas o atd no activo)"
else
    echo "atq no disponible"
fi
'
```

#### Paso 2.2: Inspeccionar y eliminar

```bash
docker compose exec debian-dev bash -c '
echo "=== Inspeccionar y eliminar ==="
echo ""
echo "--- Ver contenido de una tarea ---"
echo "at -c 7"
echo "  Muestra el script completo:"
echo "  - Entorno capturado (variables exportadas)"
echo "  - Directorio de trabajo"
echo "  - umask"
echo "  - Comandos del usuario al final"
echo ""
echo "Solo ver los comandos:"
echo "  at -c 7 | tail -5"
echo ""
echo "--- Eliminar tareas ---"
echo "atrm 7           eliminar tarea 7"
echo "atrm 7 8 12      eliminar multiples"
'
```

---

### Parte 3 — Entorno y control de acceso

#### Paso 3.1: Captura de entorno

```bash
docker compose exec debian-dev bash -c '
echo "=== Captura de entorno ==="
echo ""
echo "A diferencia de cron, at captura el entorno actual:"
echo ""
echo "at preserva:"
echo "  - El directorio de trabajo actual (pwd)"
echo "  - Todas las variables de entorno"
echo "  - umask"
echo ""
echo "at NO hereda:"
echo "  - La sesion de terminal (no hay TTY)"
echo "  - DISPLAY (no funciona para GUI)"
echo "  - El shell interactivo (usa /bin/sh)"
echo ""
echo "--- Comparacion con cron ---"
echo "| Aspecto        | at             | cron               |"
echo "|----------------|----------------|--------------------|"
echo "| Captura entorno| Si (completo)  | No (PATH minimo)   |"
echo "| Directorio     | pwd actual     | HOME del usuario   |"
echo "| Variables      | Todas          | Solo las definidas  |"
'
```

#### Paso 3.2: Control de acceso

```bash
docker compose exec debian-dev bash -c '
echo "=== Control de acceso ==="
echo ""
echo "/etc/at.allow → si existe, solo estos usuarios"
echo "/etc/at.deny  → si existe (y no hay at.allow), estos NO"
echo ""
echo "--- Verificar ---"
echo "/etc/at.allow:"
cat /etc/at.allow 2>/dev/null || echo "  (no existe)"
echo "/etc/at.deny:"
cat /etc/at.deny 2>/dev/null || echo "  (no existe)"
echo ""
echo "root siempre puede usar at."
'
```

#### Paso 3.3: Alternativa moderna

```bash
docker compose exec debian-dev bash -c '
echo "=== Alternativa moderna: systemd-run ==="
echo ""
echo "systemd-run --on-active=2h /usr/local/bin/task.sh"
echo "  Crea un timer transient que ejecuta en 2 horas"
echo "  Equivale a: echo /usr/local/bin/task.sh | at now + 2 hours"
echo ""
echo "Ventajas sobre at:"
echo "  - Logs en el journal"
echo "  - Historial de ejecucion"
echo "  - Dependencias de systemd"
echo "  - No requiere atd"
'
```

---

## Ejercicios

### Ejercicio 1 — Verificar instalación

¿Está `at` instalado y `atd` corriendo?

```bash
which at 2>/dev/null && echo "at: instalado" || echo "at: no instalado"
systemctl is-active atd 2>/dev/null
```

<details><summary>Predicción</summary>

- `at` puede o no estar instalado (no viene por defecto en todas las distribuciones ni en contenedores).
- Si está instalado, `atd` debe estar activo para que las tareas se ejecuten. En contenedores Docker, `atd` probablemente no estará corriendo.
- Instalar con `sudo apt install at` (Debian) o `sudo dnf install at` (RHEL). Activar con `sudo systemctl enable --now atd`.

</details>

### Ejercicio 2 — Formatos de tiempo

¿Son válidas estas expresiones? ¿Qué harían?

```
a) at teatime tomorrow
b) at now + 30 seconds
c) at noon next Friday
d) at 25:00
e) at midnight + 1 day
```

<details><summary>Predicción</summary>

- **a)** Válida. Programa para mañana a las 16:00 (`teatime` = 16:00).
- **b)** Inválida. `at` no soporta la unidad `seconds`. El mínimo es `minutes`.
- **c)** Válida. Programa para el próximo viernes a las 12:00.
- **d)** Inválida. 25:00 no es una hora válida (rango 0-23).
- **e)** Válida. Programa para mañana a medianoche (`midnight` = 00:00, + 1 day).

</details>

### Ejercicio 3 — Crear y listar tareas

Crea una tarea, verifica que está en la cola, y elimínala.

```bash
# 1. Crear:
echo 'echo "at funciona: $(date)" > /tmp/at-test.txt' | at now + 2 minutes

# 2. Listar:
atq

# 3. Ver contenido:
at -c $(atq | tail -1 | awk '{print $1}') | tail -5

# 4. Eliminar:
atrm $(atq | tail -1 | awk '{print $1}')

# 5. Verificar eliminación:
atq
```

<details><summary>Predicción</summary>

- `at` confirmará con `job N at DATE`. `atq` mostrará la tarea con su ID, fecha, cola "a" y usuario.
- `at -c ID` mostrará un script largo con todas las variables de entorno exportadas, seguido del comando `echo "at funciona..."` al final.
- `atrm ID` eliminará la tarea sin salida (silencio = éxito). `atq` ya no la mostrará.
- Si `atd` no está corriendo, `at` creará la tarea pero no se ejecutará hasta que se active `atd`.

</details>

### Ejercicio 4 — Captura de entorno

Demuestra que `at` captura el entorno actual.

```bash
# 1. Establecer una variable y directorio:
cd /tmp
export MI_VARIABLE="test-desde-at"

# 2. Crear tarea que use ambos:
echo 'echo "VAR=$MI_VARIABLE PWD=$(pwd)" > /tmp/at-env-test.txt' | at now + 1 minute

# 3. Verificar con at -c:
at -c $(atq | tail -1 | awk '{print $1}') | grep MI_VARIABLE

# 4. Después de 1 minuto:
# cat /tmp/at-env-test.txt
```

<details><summary>Predicción</summary>

- `at -c` mostrará `MI_VARIABLE` entre las variables exportadas al inicio del script.
- Después de ejecutarse, `/tmp/at-env-test.txt` contendrá `VAR=test-desde-at PWD=/tmp`.
- Esto demuestra que `at` captura tanto las variables de entorno como el directorio de trabajo actual. **cron no hace esto** — en cron, `MI_VARIABLE` no existiría y el directorio sería `HOME`.

</details>

### Ejercicio 5 — Crear tarea con heredoc

Programa múltiples comandos con heredoc.

```bash
at now + 5 minutes << 'EOF'
echo "=== Tarea at ===" > /tmp/at-heredoc.txt
echo "Fecha: $(date)" >> /tmp/at-heredoc.txt
echo "Usuario: $(whoami)" >> /tmp/at-heredoc.txt
echo "Directorio: $(pwd)" >> /tmp/at-heredoc.txt
EOF
```

<details><summary>Predicción</summary>

- `at` aceptará los 4 comandos y creará un job.
- Al ejecutarse, `/tmp/at-heredoc.txt` contendrá la fecha, el usuario (el mismo que creó la tarea), y el directorio de trabajo (el pwd de cuando se creó la tarea).
- Notar las comillas simples en `'EOF'`: esto evita que `$(date)`, `$(whoami)` etc. se expandan al crear la tarea. Se expandirán al ejecutarse.

</details>

### Ejercicio 6 — Crear tarea desde archivo

Programa una tarea usando la opción `-f`.

```bash
# 1. Crear el archivo:
cat > /tmp/at-script.sh << 'EOF'
#!/bin/bash
logger "at-script: ejecutado a $(date)"
echo "Script ejecutado" > /tmp/at-file-test.txt
EOF

# 2. Programar:
at -f /tmp/at-script.sh now + 2 minutes

# 3. Verificar:
atq
```

<details><summary>Predicción</summary>

- `at -f` lee los comandos del archivo especificado. No necesita permisos de ejecución en el archivo (at lo lee como texto).
- El shebang `#!/bin/bash` del archivo **sí se respeta** cuando se usa `-f`, a diferencia de cuando se pasa por pipe (que usa `/bin/sh`).
- Tras ejecutarse, `logger` enviará un mensaje al journal/syslog, y se creará `/tmp/at-file-test.txt`.

</details>

### Ejercicio 7 — Colas de prioridad

¿Qué efecto tiene cambiar de cola?

```bash
# Cola default (a, nice 0):
echo 'echo "cola a"' | at -q a now + 10 minutes

# Cola c (nice 4):
echo 'echo "cola c"' | at -q c now + 10 minutes

# Verificar:
atq
```

<details><summary>Predicción</summary>

- `atq` mostrará ambas tareas, una con cola "a" y otra con cola "c".
- La tarea en cola "c" se ejecutará con `nice 4`, lo que le da menor prioridad de CPU que la de cola "a" (nice 0).
- En la práctica, la diferencia solo importa si hay contención de CPU. Para tareas intensivas que no deben interferir con el sistema, usar colas más altas (mayor letra = mayor nice = menor prioridad).
- La cola "b" está reservada para `batch`.

</details>

### Ejercicio 8 — Control de acceso

¿Quién puede usar `at` en tu sistema?

```bash
echo "=== Control de acceso ==="
echo "/etc/at.allow:"
cat /etc/at.allow 2>/dev/null || echo "  (no existe)"
echo "/etc/at.deny:"
cat /etc/at.deny 2>/dev/null || echo "  (no existe)"
```

<details><summary>Predicción</summary>

- En la mayoría de distribuciones, `/etc/at.deny` existe y está vacío → todos los usuarios pueden usar `at`.
- `/etc/at.allow` probablemente no existe.
- Si quisieras restringir el acceso: crear `/etc/at.allow` con los usuarios permitidos. La existencia de `at.allow` hace que `at.deny` se ignore.
- root siempre puede usar `at`, independientemente de estos archivos.

</details>

### Ejercicio 9 — Caso de uso: reinicio programado

Programa un reinicio para dentro de 2 horas y luego cancélalo.

```bash
# 1. Programar (NO ejecutar realmente si es un sistema en uso):
echo "shutdown -r now" | sudo at now + 2 hours

# 2. Verificar:
sudo atq

# 3. Cancelar:
sudo atrm $(sudo atq | tail -1 | awk '{print $1}')

# 4. Confirmar cancelación:
sudo atq
```

<details><summary>Predicción</summary>

- `at` creará el job y mostrará el ID y la hora programada.
- `atq` mostrará la tarea con usuario "root" (porque se usó `sudo`).
- `atrm` eliminará la tarea silenciosamente. `atq` confirmará que la cola está vacía.
- Este patrón es muy útil en producción: programar un reinicio con margen para cancelar si se detecta un problema. Es más seguro que `shutdown -r +120` porque `at` registra el job en disco (sobrevive a un crash del proceso shutdown).

</details>

### Ejercicio 10 — at vs systemd-run

Compara `at` con su alternativa moderna.

```bash
# Con at:
echo 'echo "hola desde at" > /tmp/at-vs-systemd.txt' | at now + 2 minutes

# Equivalente con systemd-run:
sudo systemd-run --on-active=2m /bin/bash -c 'echo "hola desde systemd" > /tmp/systemd-vs-at.txt'

# Ver el timer transient creado:
systemctl list-timers --all | grep run-
```

<details><summary>Predicción</summary>

- `at` crea un job silenciosamente, sin logs hasta que se ejecuta.
- `systemd-run` crea un **timer transient** y un **service transient** que aparecen en `systemctl list-timers`. La ejecución se registra en el journal con `journalctl -u run-XXXX.service`.
- Ventajas de `systemd-run`: logs integrados, historial, dependencias, no requiere `atd`.
- Ventajas de `at`: más simple, captura automática del entorno, sintaxis de tiempo más amigable (`tomorrow`, `teatime`), no requiere root para tareas de usuario.
- En sistemas modernos, `systemd-run --on-active=` es la alternativa recomendada, pero `at` sigue siendo válido y más accesible.

</details>
