# Lab — Variables de entorno en cron

## Objetivo

Entender el entorno reducido de cron y como causa la mayoria de
errores: PATH minimo, SHELL=/bin/sh, MAILTO, HOME, los errores
clasicos (comando no encontrado, bashisms en sh, variables no
expandidas, locale), tecnicas de depuracion (capturar entorno,
reproducir manualmente, wrapper de logging), y buenas practicas
para scripts confiables en cron.

## Prerequisitos

- Entorno del curso levantado (`docker compose up -d`)

---

## Parte 1 — Entorno de cron

### Objetivo

Entender que variables tiene cron y como difieren de una terminal
interactiva.

### Paso 1.1: El entorno minimo de cron

```bash
docker compose exec debian-dev bash -c '
echo "=== El entorno minimo de cron ==="
echo ""
echo "cron NO ejecuta los comandos en un shell interactivo."
echo "El entorno es extremadamente reducido:"
echo ""
echo "--- Lo que tiene cron ---"
echo "HOME=/home/user"
echo "LOGNAME=user"
echo "PATH=/usr/bin:/bin            ← PATH minimo"
echo "SHELL=/bin/sh                 ← sh, no bash"
echo "LANG=                        ← vacio o no definido"
echo ""
echo "--- Lo que tiene tu terminal ---"
echo "PATH=$PATH"
echo "SHELL=$SHELL"
echo "LANG=$LANG"
echo ""
echo "--- Comparacion ---"
echo "Terminal: $(echo $PATH | tr ':' '\n' | wc -l) directorios en PATH"
echo "Cron: 2 directorios en PATH (/usr/bin:/bin)"
echo ""
echo "La causa #1 de \"funciona en terminal pero no en cron\" es el entorno."
'
```

### Paso 1.2: Variables configurables en crontab

```bash
docker compose exec debian-dev bash -c '
echo "=== Variables en crontab ==="
echo ""
echo "--- PATH ---"
echo "El default es insuficiente para la mayoria de tareas."
echo "Definir al inicio del crontab:"
echo "  PATH=/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin"
echo ""
echo "--- SHELL ---"
echo "Default: /bin/sh (POSIX shell)"
echo "Bashisms no funcionan: [[ ]], arrays, <()"
echo "Cambiar: SHELL=/bin/bash"
echo ""
echo "--- MAILTO ---"
echo "cron envia stdout+stderr por mail al usuario."
echo "Cambiar destinatario: MAILTO=admin@example.com"
echo "Multiples: MAILTO=\"admin@example.com,ops@example.com\""
echo "Deshabilitar: MAILTO=\"\""
echo ""
echo "--- HOME ---"
echo "Default: directorio home del usuario."
echo "Cambiar: HOME=/opt/myapp"
echo ""
echo "--- Verificar MTA ---"
which sendmail 2>/dev/null && echo "MTA disponible" || \
    echo "Sin MTA — la salida de cron se descartara"
'
```

### Paso 1.3: Variables en /etc/cron.d/

```bash
docker compose exec debian-dev bash -c '
echo "=== Variables en /etc/cron.d/ ==="
echo ""
echo "Los archivos de /etc/cron.d/ aceptan las mismas variables:"
echo ""
echo "--- Ejemplo: /etc/cron.d/myapp ---"
echo "SHELL=/bin/bash"
echo "PATH=/opt/myapp/bin:/usr/local/bin:/usr/bin:/bin"
echo "MAILTO=ops@example.com"
echo "HOME=/opt/myapp"
echo ""
echo "0 */4 * * * myappuser /opt/myapp/bin/health-check.sh"
echo ""
echo "--- Scope de las variables ---"
echo "Las variables definidas en un archivo de /etc/cron.d/:"
echo "  Solo aplican a las tareas de ESE archivo"
echo "  No afectan a otros archivos en /etc/cron.d/"
echo "  No afectan a crontabs de usuario"
echo ""
echo "Las variables de /etc/crontab:"
echo "  Solo aplican a las tareas de /etc/crontab"
echo "  No se heredan a /etc/cron.d/ ni a crontabs de usuario"
'
```

---

## Parte 2 — Errores comunes por entorno

### Objetivo

Identificar y resolver los errores mas frecuentes causados por el
entorno reducido de cron.

### Paso 2.1: Comando no encontrado

```bash
docker compose exec debian-dev bash -c '
echo "=== Error: comando no encontrado ==="
echo ""
echo "--- Sintoma en logs ---"
echo "CRON: (user) CMD (/home/user/backup.sh)"
echo "/bin/sh: node: not found"
echo ""
echo "--- Causa ---"
echo "El comando esta en /usr/local/bin (o similar)"
echo "pero /usr/local/bin no esta en el PATH de cron"
echo ""
echo "--- Demostrar ---"
echo "PATH de cron:"
echo "/usr/bin:/bin"
echo ""
echo "Buscar comandos que NO estan en ese PATH:"
for cmd in python3 node pip3 npm go; do
    full=$(which "$cmd" 2>/dev/null)
    if [[ -n "$full" ]]; then
        if [[ "$full" != /usr/bin/* && "$full" != /bin/* ]]; then
            echo "  $cmd → $full (NO estaria en PATH de cron)"
        else
            echo "  $cmd → $full (OK en PATH de cron)"
        fi
    fi
done
echo ""
echo "--- Soluciones ---"
echo "1. PATH en crontab: PATH=/usr/local/bin:/usr/bin:/bin"
echo "2. Ruta absoluta: /usr/local/bin/node script.js"
echo "3. which para encontrar la ruta: which node"
'
```

### Paso 2.2: Bashisms en sh

```bash
docker compose exec debian-dev bash -c '
echo "=== Error: bashisms en sh ==="
echo ""
echo "--- Sintoma ---"
echo "/bin/sh: Syntax error: \"(\" unexpected"
echo "/bin/sh: [[: not found"
echo ""
echo "--- Causa ---"
echo "cron usa /bin/sh (POSIX shell). Sintaxis de bash no funciona:"
echo "  [[ \$result == \"ok\" ]]   → error"
echo "  arr=(one two three)      → error"
echo "  <(command)               → error"
echo ""
echo "--- Verificar la diferencia ---"
echo "sh es:"
ls -la /bin/sh
echo ""
echo "bash es:"
ls -la /bin/bash
echo ""
echo "--- Soluciones ---"
echo "1. Shebang en el script: #!/bin/bash"
echo "2. SHELL en crontab: SHELL=/bin/bash"
echo "3. Invocar explicito: 0 2 * * * /bin/bash /path/script.sh"
'
```

### Paso 2.3: Variables no expandidas y locale

```bash
docker compose exec debian-dev bash -c '
echo "=== Error: variables no expandidas ==="
echo ""
echo "--- Sintoma ---"
echo "El script genera archivos con nombres vacios o rutas rotas"
echo ""
echo "--- Causa ---"
echo "BACKUP_DIR=\$HOME/backups"
echo "Si HOME no esta definido → BACKUP_DIR=/backups"
echo ""
echo "--- Solucion ---"
echo "En el script, definir todo explicitamente:"
echo "  HOME=/home/user"
echo "  BACKUP_DIR=\"\$HOME/backups\""
echo ""
echo "O verificar que existen:"
echo "  : \"\${HOME:?HOME no definido}\""
echo ""
echo "=== Error: locale ==="
echo ""
echo "--- Sintoma ---"
echo "Caracteres especiales corruptos, fechas en ingles"
echo ""
echo "--- Causa ---"
echo "LANG no esta definido en cron"
echo ""
echo "--- Solucion ---"
echo "En el script:"
echo "  export LANG=es_ES.UTF-8"
echo "  export LC_ALL=es_ES.UTF-8"
echo ""
echo "--- Verificar locales disponibles ---"
locale -a 2>/dev/null | grep -i "es_\|en_" | head -5 || \
    echo "(locale no disponible)"
'
```

---

## Parte 3 — Depuracion y buenas practicas

### Objetivo

Aprender tecnicas de depuracion y buenas practicas para scripts
en cron.

### Paso 3.1: Capturar el entorno de cron

```bash
docker compose exec debian-dev bash -c '
echo "=== Capturar el entorno de cron ==="
echo ""
echo "--- Diagnostico ---"
echo "Agregar temporalmente al crontab:"
echo "  * * * * * env | sort > /tmp/cron-env.txt 2>&1"
echo ""
echo "Despues de 1 minuto, comparar con tu terminal:"
echo "  diff <(cat /tmp/cron-env.txt) <(env | sort)"
echo ""
echo "--- Variables clave a comparar ---"
echo "  PATH: el mas critico"
echo "  SHELL: sh vs bash"
echo "  LANG: afecta locale"
echo "  HOME: directorio de trabajo"
echo "  USER/LOGNAME: identidad"
echo ""
echo "No olvidar eliminar la linea de diagnostico despues."
'
```

### Paso 3.2: Reproducir el entorno de cron

```bash
docker compose exec debian-dev bash -c '
echo "=== Reproducir el entorno de cron ==="
echo ""
echo "Ejecutar un comando con el entorno minimo de cron:"
echo ""
echo "  env -i HOME=\$HOME LOGNAME=\$USER PATH=/usr/bin:/bin SHELL=/bin/sh \\"
echo "      /bin/sh -c \"/path/to/script.sh\""
echo ""
echo "--- Demostrar ---"
echo "Terminal normal:"
echo "  PATH=$(echo $PATH | cut -c1-60)..."
echo "  SHELL=$SHELL"
echo ""
echo "Simulando entorno cron:"
env -i HOME="$HOME" LOGNAME="$USER" PATH=/usr/bin:/bin SHELL=/bin/sh \
    /bin/sh -c "echo \"  PATH=\$PATH\"; echo \"  SHELL=\$SHELL\""
echo ""
echo "Si el script falla con env -i, fallara en cron."
echo "Si funciona con env -i pero no en cron, el problema es otro"
echo "(permisos, timing, concurrencia)."
'
```

### Paso 3.3: Wrapper de logging

```bash
docker compose exec debian-dev bash -c '
echo "=== Wrapper de logging para cron ==="
echo ""
echo "Un wrapper que captura todo:"
echo ""
cat << '\''WRAPPER'\''
#!/bin/bash
# /usr/local/bin/cron-wrapper.sh
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
WRAPPER
echo ""
echo "--- Uso en crontab ---"
echo "0 2 * * * /usr/local/bin/cron-wrapper.sh /home/user/backup.sh"
echo ""
echo "Registra: entorno, comando, salida, exit code, timestamps."
'
```

### Paso 3.4: Buenas practicas

```bash
docker compose exec debian-dev bash -c '
echo "=== Buenas practicas ==="
echo ""
echo "1. SIEMPRE definir PATH, SHELL, MAILTO al inicio del crontab:"
echo "   SHELL=/bin/bash"
echo "   PATH=/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin"
echo "   MAILTO=admin@example.com"
echo ""
echo "2. Usar rutas absolutas para TODO:"
echo "   0 2 * * * /usr/local/bin/python3 /opt/myapp/backup.py"
echo ""
echo "3. Incluir shebang correcto en los scripts:"
echo "   #!/bin/bash  (o #!/usr/bin/env bash)"
echo ""
echo "4. El script debe ser autosuficiente:"
echo "   #!/bin/bash"
echo "   export PATH=/usr/local/bin:/usr/bin:/bin"
echo "   export LANG=en_US.UTF-8"
echo "   export HOME=/home/myuser"
echo "   cd \"\$HOME\" || exit 1"
echo ""
echo "5. SIEMPRE redirigir la salida:"
echo "   0 2 * * * /path/backup.sh >> /var/log/backup.log 2>&1"
echo ""
echo "6. Escapar % con \\% o mover logica a scripts"
'
```

---

## Limpieza final

No hay recursos que limpiar.

---

## Conceptos reforzados

1. cron ejecuta con un entorno minimo: PATH=/usr/bin:/bin,
   SHELL=/bin/sh, LANG vacio. Esta es la causa #1 de errores.

2. Se pueden definir PATH, SHELL, MAILTO y HOME al inicio del
   crontab. Cada archivo de /etc/cron.d/ tiene su propio scope
   de variables.

3. Los errores mas comunes: comando no encontrado (PATH), bashisms
   en sh (SHELL), variables no expandidas (HOME), y locale
   incorrecto (LANG).

4. Para depurar: capturar entorno con `env > /tmp/cron-env.txt`,
   reproducir con `env -i`, y usar un wrapper de logging.

5. Buenas practicas: shebang correcto, rutas absolutas, script
   autosuficiente en su entorno, siempre redirigir salida.

6. `env -i HOME=$HOME PATH=/usr/bin:/bin SHELL=/bin/sh /bin/sh -c`
   reproduce el entorno de cron para testing sin esperar 1 minuto.
