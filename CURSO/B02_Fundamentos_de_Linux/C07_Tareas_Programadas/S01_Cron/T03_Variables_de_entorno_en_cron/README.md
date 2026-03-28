# T03 — Variables de entorno en cron

## El problema fundamental

cron no ejecuta los comandos en un shell interactivo ni de login. El
entorno es **extremadamente reducido** comparado con lo que tienes cuando
abres una terminal:

```bash
# En tu terminal interactiva:
env | wc -l
# 50-80 variables (PATH completo, LANG, HOME, DISPLAY, etc.)

# En cron:
# Solo unas pocas variables están definidas
# La causa #1 de "funciona en terminal pero no en cron" es el entorno
```

### Qué entorno tiene cron

```bash
# Verificar empíricamente — agregar al crontab:
* * * * * env > /tmp/cron-env.txt

# Después de 1 minuto:
cat /tmp/cron-env.txt
# HOME=/home/user
# LOGNAME=user
# PATH=/usr/bin:/bin           ← PATH mínimo
# SHELL=/bin/sh                ← sh, no bash
# LANG=                       ← vacío o no definido

# Comparar con tu terminal:
echo $PATH
# /usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin:...
# Muchas más rutas
```

## Variables que se pueden definir en el crontab

### PATH

```bash
# El PATH por defecto de cron es insuficiente para la mayoría de tareas:
# PATH=/usr/bin:/bin (Debian)
# PATH=/usr/bin:/bin:/usr/sbin:/sbin (RHEL)

# Definir PATH al inicio del crontab:
PATH=/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin

# O por tarea específica:
0 2 * * * PATH=/opt/myapp/bin:$PATH /opt/myapp/bin/backup.sh

# Ejemplo de error por PATH:
# El script usa "aws" (en /usr/local/bin/aws):
0 3 * * * /home/user/scripts/s3-backup.sh
# Falla porque /usr/local/bin no está en el PATH de cron
# El script llama "aws s3 sync ..." y obtiene "aws: command not found"

# Soluciones:
# 1. Definir PATH en el crontab:
PATH=/usr/local/bin:/usr/bin:/bin
0 3 * * * /home/user/scripts/s3-backup.sh

# 2. Usar rutas absolutas en el script:
# /usr/local/bin/aws s3 sync ...

# 3. Cargar el profile en el script (última opción):
# source /home/user/.profile
```

### SHELL

```bash
# Por defecto, cron usa /bin/sh (POSIX shell):
# Esto significa que bashisms no funcionan:
# - [[ ]] (doble corchete)
# - arrays
# - process substitution <()
# - $(( )) puede variar

# Cambiar el shell para todo el crontab:
SHELL=/bin/bash

# O solo para una tarea:
0 2 * * * /bin/bash -c 'source ~/.bashrc && my-command'

# Verificar qué shell está usando cron:
* * * * * echo "SHELL=$SHELL" >> /tmp/cron-shell.txt
```

### MAILTO

```bash
# cron envía un email al propietario del crontab si el comando
# produce salida (stdout o stderr)

# Cambiar el destinatario:
MAILTO=admin@example.com

# Múltiples destinatarios:
MAILTO="admin@example.com,ops@example.com"

# Deshabilitar emails completamente:
MAILTO=""

# MAILTO solo funciona si hay un MTA (Mail Transfer Agent) configurado
# (postfix, sendmail, etc.)
# Si no hay MTA, la salida simplemente se descarta

# Comprobar si hay MTA:
which sendmail 2>/dev/null && echo "MTA disponible" || echo "Sin MTA"
```

### HOME

```bash
# cron establece HOME al directorio home del usuario
# Se puede sobreescribir:
HOME=/opt/myapp

# Útil cuando el script depende de archivos relativos:
0 2 * * * ./run-backup.sh
# Ejecuta /opt/myapp/run-backup.sh (relativo a HOME)
```

### Otras variables

```bash
# LANG y LC_* — afectan el locale:
# En cron, LANG puede estar vacío → salida en inglés/ASCII
# Si el script necesita un locale específico:
LANG=es_ES.UTF-8
LC_ALL=es_ES.UTF-8

# O mejor, definirlo en el script:
export LANG=es_ES.UTF-8

# DISPLAY — para aplicaciones gráficas (raro en cron):
# No está definido en cron → las aplicaciones X11 fallan
```

## Variables de entorno en /etc/cron.d/

Los archivos en `/etc/cron.d/` aceptan las mismas variables que
`/etc/crontab`:

```bash
# /etc/cron.d/myapp
SHELL=/bin/bash
PATH=/opt/myapp/bin:/usr/local/bin:/usr/bin:/bin
MAILTO=ops@example.com
HOME=/opt/myapp

0 */4 * * * myappuser /opt/myapp/bin/health-check.sh
```

```bash
# Las variables definidas en un archivo de /etc/cron.d/:
# - Solo aplican a las tareas de ESE archivo
# - No afectan a otros archivos en /etc/cron.d/
# - No afectan a crontabs de usuario

# Las variables definidas en /etc/crontab:
# - Solo aplican a las tareas de /etc/crontab
# - No se "heredan" a /etc/cron.d/ ni a crontabs de usuario
```

## Errores comunes por entorno

### Error 1: Comando no encontrado

```bash
# Síntoma en logs:
# CRON: (user) CMD (/home/user/backup.sh)
# /bin/sh: node: not found

# Causa: node está en /usr/local/bin, que no está en el PATH de cron

# Solución A — PATH en crontab:
PATH=/usr/local/bin:/usr/bin:/bin
0 2 * * * /home/user/backup.sh

# Solución B — ruta absoluta en el script:
# Cambiar "node script.js" por "/usr/local/bin/node script.js"

# Solución C — which para encontrar la ruta:
which node
# /usr/local/bin/node
# Usar esa ruta en el script
```

### Error 2: Bashisms en sh

```bash
# Síntoma:
# /bin/sh: Syntax error: "(" unexpected

# Causa: el script usa sintaxis de bash pero cron usa /bin/sh

# El script tiene:
# if [[ $result == "ok" ]]; then ...
# arr=(one two three)

# Solución A — shebang en el script:
#!/bin/bash     # NO #!/bin/sh

# Solución B — SHELL en crontab:
SHELL=/bin/bash

# Solución C — invocar explícitamente:
0 2 * * * /bin/bash /home/user/script.sh
```

### Error 3: Variables no expandidas

```bash
# Síntoma: el script genera archivos con nombres vacíos o rotos

# El script hace:
# BACKUP_DIR=$HOME/backups
# Y $HOME no está definido en cron → BACKUP_DIR=/backups

# Solución — definir las variables necesarias al inicio del script:
#!/bin/bash
HOME=/home/user
BACKUP_DIR="$HOME/backups"

# O verificar que existen:
#!/bin/bash
: "${HOME:?HOME no definido}"
```

### Error 4: Locale y encoding

```bash
# Síntoma: caracteres especiales se corrompen, dates en inglés
# cuando se esperan en español

# Causa: LANG no está definido en cron

# Solución en el script:
export LANG=es_ES.UTF-8
export LC_ALL=es_ES.UTF-8
date +"%A %d de %B de %Y"
# Resultado: lunes 17 de marzo de 2025
```

### Error 5: Archivos de configuración no encontrados

```bash
# Síntoma: la aplicación no encuentra su .rc o .conf

# Causa: el directorio de trabajo y HOME pueden diferir de lo esperado

# El script o la aplicación busca ~/.myapp.conf
# pero HOME no apunta a donde esperas

# Solución — ser explícito:
#!/bin/bash
export HOME=/home/myuser
cd "$HOME" || exit 1
# Ahora ~/.myapp.conf es /home/myuser/.myapp.conf
```

## Técnicas de depuración

### Verificar el entorno completo

```bash
# Crear un crontab de diagnóstico:
crontab -e
# Agregar:
* * * * * env | sort > /tmp/cron-env-$(date +\%s).txt 2>&1

# Esperar 1 minuto, luego comparar:
diff <(sort /tmp/cron-env-*.txt | head -50) <(env | sort)
```

### Reproducir el entorno de cron manualmente

```bash
# Ejecutar un comando con el entorno mínimo de cron:
env -i HOME="$HOME" LOGNAME="$USER" PATH=/usr/bin:/bin SHELL=/bin/sh \
    /bin/sh -c '/home/user/scripts/backup.sh'

# Si falla aquí, fallará en cron
# Si funciona aquí pero no en cron, el problema es otro
# (permisos, timing, concurrencia)
```

### Wrapper de logging para cron

```bash
# Crear un wrapper que capture todo:
# /usr/local/bin/cron-wrapper.sh
#!/bin/bash
LOG="/var/log/cron-jobs/$(basename "$1").log"
echo "=== $(date) ===" >> "$LOG"
echo "ENV:" >> "$LOG"
env | sort >> "$LOG"
echo "CMD: $*" >> "$LOG"
echo "---" >> "$LOG"
"$@" >> "$LOG" 2>&1
STATUS=$?
echo "EXIT: $STATUS" >> "$LOG"
echo "===" >> "$LOG"
exit $STATUS

# Uso en crontab:
0 2 * * * /usr/local/bin/cron-wrapper.sh /home/user/backup.sh
```

## Buenas prácticas

```bash
# 1. Definir SIEMPRE PATH, SHELL, MAILTO al inicio del crontab:
SHELL=/bin/bash
PATH=/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin
MAILTO=admin@example.com

# 2. Usar rutas absolutas para TODO:
0 2 * * * /usr/local/bin/python3 /opt/myapp/backup.py

# 3. Incluir shebang correcto en los scripts:
#!/bin/bash  # O #!/usr/bin/env bash

# 4. El script debe ser autosuficiente en su entorno:
#!/bin/bash
export PATH=/usr/local/bin:/usr/bin:/bin
export LANG=en_US.UTF-8
export HOME=/home/myuser
cd "$HOME" || exit 1
# ... resto del script

# 5. Redirigir siempre la salida:
0 2 * * * /opt/myapp/backup.sh >> /var/log/myapp-backup.log 2>&1
```

---

## Ejercicios

### Ejercicio 1 — Diagnosticar el entorno

```bash
# 1. Capturar el entorno de cron (agregar temporalmente al crontab):
#    * * * * * env | sort > /tmp/cron-env.txt 2>&1
# 2. Después de 1 minuto, comparar con tu entorno:
#    diff <(cat /tmp/cron-env.txt) <(env | sort) | head -20
```

### Ejercicio 2 — Identificar el error

```bash
# ¿Por qué fallaría cada tarea en cron?
# a) 0 * * * * python3 ~/scripts/check.py
# b) 0 2 * * * echo "Backup $(date +%Y-%m-%d)" >> /var/log/backup.log
# c) 0 3 * * * if [[ -f /tmp/lock ]]; then echo "locked"; fi
```

### Ejercicio 3 — Reproducir entorno cron

```bash
# Ejecutar un comando como si estuvieras en cron:
env -i HOME="$HOME" LOGNAME="$USER" PATH=/usr/bin:/bin SHELL=/bin/sh \
    /bin/sh -c 'echo "PATH=$PATH"; echo "SHELL=$SHELL"; which python3 2>/dev/null || echo "python3 not in PATH"'
```
