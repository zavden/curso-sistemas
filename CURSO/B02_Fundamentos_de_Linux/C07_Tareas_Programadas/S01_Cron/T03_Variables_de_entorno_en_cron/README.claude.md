# T03 — Variables de entorno en cron

> **Objetivo:** Entender el entorno reducido de cron (PATH, SHELL, MAILTO, HOME, LANG), los errores más comunes que causa, técnicas de depuración, y buenas prácticas para scripts confiables en cron.

## Erratas corregidas respecto a `README.md`

| # | Línea | Error | Corrección |
|---|-------|-------|------------|
| 1 | 80 | `$(( )) puede variar` — sugiere que la expansión aritmética es inestable en sh | `$(( ))` es POSIX estándar y funciona consistentemente en todos los shells POSIX. Lo que no funciona en sh son `let`, `(( ))` sin `$`, y `declare -i`. |

---

## El problema fundamental

cron no ejecuta los comandos en un shell interactivo ni de login. El entorno es **extremadamente reducido** comparado con lo que tienes al abrir una terminal:

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

---

## Variables que se pueden definir en el crontab

### PATH

```bash
# El PATH por defecto de cron es insuficiente para la mayoría de tareas:
# PATH=/usr/bin:/bin (Debian)
# PATH=/usr/bin:/bin:/usr/sbin:/sbin (RHEL)

# Definir PATH al inicio del crontab:
PATH=/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin

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

# 3. Cargar el profile en el script (última opción, frágil):
# source /home/user/.profile
```

### SHELL

```bash
# Por defecto, cron usa /bin/sh (POSIX shell):
# Esto significa que bashisms no funcionan:
# - [[ ]] (doble corchete)
# - arrays: arr=(one two three)
# - process substitution: <()
# - let, (( )) sin $
# Nota: $(( )) SÍ funciona (es POSIX estándar)

# Cambiar el shell para todo el crontab:
SHELL=/bin/bash

# O solo para una tarea:
0 2 * * * /bin/bash -c 'source ~/.bashrc && my-command'
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

# cron usa HOME como directorio de trabajo, así que:
0 2 * * * ./run-backup.sh
# Ejecuta /opt/myapp/run-backup.sh (relativo a HOME)
```

### LANG y LC_*

```bash
# En cron, LANG puede estar vacío → salida en inglés/ASCII
# Si el script necesita un locale específico:
LANG=es_ES.UTF-8
LC_ALL=es_ES.UTF-8

# O mejor, definirlo dentro del script:
export LANG=es_ES.UTF-8
```

---

## Variables de entorno en /etc/cron.d/

Los archivos en `/etc/cron.d/` aceptan las mismas variables que `/etc/crontab`:

```bash
# /etc/cron.d/myapp
SHELL=/bin/bash
PATH=/opt/myapp/bin:/usr/local/bin:/usr/bin:/bin
MAILTO=ops@example.com
HOME=/opt/myapp

0 */4 * * * myappuser /opt/myapp/bin/health-check.sh
```

```bash
# Scope de las variables:
# - Las variables de un archivo de /etc/cron.d/ solo aplican a ESE archivo
# - No afectan a otros archivos en /etc/cron.d/
# - No afectan a crontabs de usuario
#
# Las variables de /etc/crontab:
# - Solo aplican a las tareas de /etc/crontab
# - No se "heredan" a /etc/cron.d/ ni a crontabs de usuario
```

---

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
# Síntoma: el script genera archivos con nombres vacíos o rutas rotas

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
# Síntoma: caracteres especiales se corrompen, fechas en inglés
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

# Solución — ser explícito:
#!/bin/bash
export HOME=/home/myuser
cd "$HOME" || exit 1
# Ahora ~/.myapp.conf es /home/myuser/.myapp.conf
```

---

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

# Uso en crontab:
# 0 2 * * * /usr/local/bin/cron-wrapper.sh /home/user/backup.sh
# Registra: entorno, comando, salida, exit code, timestamps
```

---

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

# 6. Escapar % con \% o mover la lógica a scripts
```

---

## Lab — Variables de entorno en cron

### Parte 1 — Entorno de cron

#### Paso 1.1: El entorno mínimo de cron

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
echo "Terminal: $(echo $PATH | tr ":" "\n" | wc -l) directorios en PATH"
echo "Cron: 2 directorios en PATH (/usr/bin:/bin)"
echo ""
echo "La causa #1 de \"funciona en terminal pero no en cron\" es el entorno."
'
```

#### Paso 1.2: Variables configurables en crontab

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

#### Paso 1.3: Variables en /etc/cron.d/

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

### Parte 2 — Errores comunes por entorno

#### Paso 2.1: Comando no encontrado

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
echo "PATH de cron: /usr/bin:/bin"
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

#### Paso 2.2: Bashisms en sh

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

#### Paso 2.3: Variables no expandidas y locale

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
'
```

---

### Parte 3 — Depuración y buenas prácticas

#### Paso 3.1: Reproducir el entorno de cron

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
'
```

#### Paso 3.2: Buenas prácticas

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

## Ejercicios

### Ejercicio 1 — Capturar el entorno de cron

¿Qué variables tiene cron y en qué se diferencian de tu terminal?

```bash
# Agregar temporalmente al crontab:
crontab -e
# * * * * * env | sort > /tmp/cron-env.txt 2>&1

# Después de 1 minuto, comparar:
diff <(cat /tmp/cron-env.txt) <(env | sort) | head -30

# No olvidar eliminar la línea después
```

<details><summary>Predicción</summary>

El diff mostrará enormes diferencias:
- **cron** tendrá ~5 variables: `HOME`, `LOGNAME`, `PATH=/usr/bin:/bin`, `SHELL=/bin/sh`, y quizás `LANG` vacío.
- **Tu terminal** tendrá 50-80 variables: PATH completo con muchos directorios, SHELL=/bin/bash o /bin/zsh, LANG con locale configurado, DISPLAY, TERM, EDITOR, SSH_*, XDG_*, y todas las variables de tu `.bashrc`/`.profile`.

La diferencia más crítica es PATH: cron tiene 2 directorios vs los 8-15 de tu terminal.

</details>

### Ejercicio 2 — Identificar errores de entorno

¿Por qué fallaría cada tarea en cron?

```
a) 0 * * * * python3 ~/scripts/check.py
b) 0 2 * * * echo "Backup $(date +%Y-%m-%d)" >> /var/log/backup.log
c) 0 3 * * * if [[ -f /tmp/lock ]]; then echo "locked"; fi
```

<details><summary>Predicción</summary>

- **a)** Doble error: (1) `python3` puede no estar en `/usr/bin:/bin` (está en `/usr/local/bin`), y (2) `~` no siempre se expande en sh. Corrección: `0 * * * * /usr/bin/python3 /home/user/scripts/check.py`
- **b)** El `%` en `+%Y-%m-%d` se interpreta como salto de línea en crontab. Corrección: escapar con `\%` → `+\%Y-\%m-\%d`, o mover a un script.
- **c)** `[[ ]]` es bashism y cron usa `/bin/sh`. Corrección: usar `[ -f /tmp/lock ]` (POSIX), o poner `SHELL=/bin/bash` al inicio del crontab, o mover a un script con `#!/bin/bash`.

</details>

### Ejercicio 3 — Reproducir entorno cron

Ejecuta un comando simulando el entorno de cron.

```bash
# Tu terminal normal:
echo "PATH=$PATH"
which python3 2>/dev/null

# Simulando cron:
env -i HOME="$HOME" LOGNAME="$USER" PATH=/usr/bin:/bin SHELL=/bin/sh \
    /bin/sh -c 'echo "PATH=$PATH"; which python3 2>/dev/null || echo "python3: not found"'
```

<details><summary>Predicción</summary>

- En tu terminal, `which python3` encontrará python3 (probablemente en `/usr/bin/python3` o `/usr/local/bin/python3`).
- En el entorno simulado de cron (`PATH=/usr/bin:/bin`), `which python3` encontrará python3 **solo si** está en `/usr/bin/`. Si está en `/usr/local/bin/`, mostrará "not found".
- Esta técnica con `env -i` es la forma más rápida de depurar — si falla aquí, fallará en cron.

</details>

### Ejercicio 4 — Scope de variables

¿Las variables de un crontab afectan a otro?

```bash
# /etc/cron.d/app-a tiene:
# PATH=/opt/app-a/bin:/usr/bin:/bin
# 0 * * * * appuser /opt/app-a/run.sh

# /etc/cron.d/app-b tiene:
# (sin PATH definido)
# 0 * * * * appuser /opt/app-b/run.sh

# ¿Qué PATH usa app-b?
```

<details><summary>Predicción</summary>

`app-b` usará el PATH por defecto de cron (`/usr/bin:/bin`), **no** el PATH de `app-a`. Cada archivo en `/etc/cron.d/` tiene su propio scope de variables. Las variables definidas en un archivo no afectan a otros archivos, ni a crontabs de usuario, ni a `/etc/crontab`.

Si `app-b` necesita `/opt/app-b/bin` en su PATH, debe definirlo en su propio archivo.

</details>

### Ejercicio 5 — SHELL y bashisms

Demuestra la diferencia entre sh y bash.

```bash
# Probar con sh:
/bin/sh -c 'arr=(one two three); echo ${arr[1]}'
# → error

# Probar con bash:
/bin/bash -c 'arr=(one two three); echo ${arr[1]}'
# → two

# Probar [[ ]]:
/bin/sh -c '[[ "hello" == "hello" ]] && echo "match"'
# → error

/bin/bash -c '[[ "hello" == "hello" ]] && echo "match"'
# → match
```

<details><summary>Predicción</summary>

- Con `/bin/sh`: arrays y `[[ ]]` producirán errores de sintaxis ("Syntax error" o "not found").
- Con `/bin/bash`: ambos funcionarán correctamente.
- Esto es exactamente lo que pasa en cron: por defecto usa `/bin/sh`, así que cualquier bashism falla.
- Solución: agregar `SHELL=/bin/bash` al crontab, o usar `#!/bin/bash` en los scripts.

En Debian, `/bin/sh` es un enlace simbólico a `dash` (no bash), que es un shell POSIX estricto.

</details>

### Ejercicio 6 — MAILTO

¿Qué pasa con la salida de una tarea cron?

```bash
# Sin redirección ni MAILTO:
0 2 * * * /opt/myapp/backup.sh
# → cron intenta enviar mail al usuario

# Con MAILTO vacío:
MAILTO=""
0 2 * * * /opt/myapp/backup.sh
# → la salida se descarta

# Con redirección:
0 2 * * * /opt/myapp/backup.sh >> /var/log/backup.log 2>&1
# → la salida va al archivo

# ¿Hay MTA configurado?
which sendmail 2>/dev/null && echo "MTA disponible" || echo "Sin MTA"
```

<details><summary>Predicción</summary>

- Sin redirección y sin MTA: la salida del comando se **pierde silenciosamente**. Cron intenta enviar mail, falla, y no hay log de que el comando produjo salida.
- Con `MAILTO=""`: la salida se descarta explícitamente (cron ni intenta enviar mail).
- Con redirección `>> log 2>&1`: la salida va al archivo, independientemente de MAILTO.
- La mayoría de servidores modernos no tienen MTA configurado. **Por eso es imprescindible redirigir siempre la salida.**

</details>

### Ejercicio 7 — HOME como directorio de trabajo

¿Qué directorio de trabajo tiene un comando en cron?

```bash
# Agregar temporalmente al crontab:
# * * * * * pwd > /tmp/cron-pwd.txt

# Después de 1 minuto:
cat /tmp/cron-pwd.txt
```

<details><summary>Predicción</summary>

El directorio de trabajo será el `HOME` del usuario (ej: `/home/user` o `/root`). Si se define `HOME=/opt/myapp` en el crontab, el directorio de trabajo cambiará a `/opt/myapp`.

Esto es importante para scripts que usan rutas relativas: `./config.ini` se resolverá relativo a HOME. Por eso es buena práctica hacer `cd "$HOME"` explícitamente al inicio del script, para que el comportamiento sea predecible.

</details>

### Ejercicio 8 — Wrapper de logging

¿Cómo implementarías un wrapper que capture el entorno y la salida de cualquier tarea cron?

```bash
# Crear el wrapper:
cat << 'EOF'
#!/bin/bash
# /usr/local/bin/cron-wrapper.sh
LOG="/var/log/cron-jobs/$(basename "$1").log"
mkdir -p "$(dirname "$LOG")"
echo "=== $(date) ===" >> "$LOG"
echo "CMD: $*" >> "$LOG"
"$@" >> "$LOG" 2>&1
echo "EXIT: $?" >> "$LOG"
EOF

# Uso en crontab:
# 0 2 * * * /usr/local/bin/cron-wrapper.sh /home/user/backup.sh
```

<details><summary>Predicción</summary>

El wrapper:
1. Crea un archivo de log nombrado según el script (`backup.sh.log`).
2. Registra timestamp y comando.
3. Ejecuta el script redirigiendo stdout y stderr al log.
4. Registra el exit code.

Cada ejecución añade al log (append con `>>`), así puedes ver el historial. Si el script falla, tendrás el error capturado. Es mucho más útil que `> /dev/null 2>&1` porque conserva la evidencia de fallos.

</details>

### Ejercicio 9 — Script autosuficiente

Escribe un script que funcione tanto en terminal como en cron sin configuración adicional.

```bash
#!/bin/bash
# /home/user/scripts/daily-report.sh

# Autosuficiencia: definir todo lo necesario
export PATH=/usr/local/bin:/usr/bin:/bin
export LANG=en_US.UTF-8
export HOME=/home/user

cd "$HOME" || exit 1

# Usar rutas absolutas para comandos externos
/usr/local/bin/python3 /home/user/scripts/generate-report.py \
    >> /var/log/daily-report.log 2>&1
```

<details><summary>Predicción</summary>

Este script funcionará en ambos entornos porque:
1. **Shebang** `#!/bin/bash` — no depende de SHELL del crontab.
2. **PATH** definido explícitamente — no depende del PATH de cron.
3. **LANG** definido — locale predecible.
4. **HOME** y `cd` — directorio de trabajo explícito.
5. **Rutas absolutas** — no depende de que python3 esté en PATH.
6. **Redirección** — captura la salida en un log.

Es redundante? Sí. Pero esa redundancia es lo que hace que funcione de forma confiable en cron.

</details>

### Ejercicio 10 — Diagnóstico completo

Un script funciona en tu terminal pero falla en cron. ¿Cuáles son los pasos de diagnóstico?

<details><summary>Predicción</summary>

**Checklist de diagnóstico:**

1. **Reproducir el entorno de cron:**
   ```bash
   env -i HOME=$HOME LOGNAME=$USER PATH=/usr/bin:/bin SHELL=/bin/sh \
       /bin/sh -c '/path/to/script.sh'
   ```

2. **Verificar PATH:** ¿El script usa comandos fuera de `/usr/bin:/bin`? → Agregar PATH al crontab o usar rutas absolutas.

3. **Verificar SHELL:** ¿El script usa bashisms? → Agregar `#!/bin/bash` al shebang o `SHELL=/bin/bash` al crontab.

4. **Verificar `%`:** ¿La línea del crontab tiene `%` sin escapar? → Escapar con `\%` o mover a script.

5. **Verificar permisos:** ¿El script es ejecutable? → `chmod +x`

6. **Revisar logs:** `grep CRON /var/log/syslog` o `journalctl -u cron`

7. **Capturar salida:** Redirigir `>> /tmp/debug.log 2>&1` para ver errores.

8. **Verificar variables:** ¿El script depende de HOME, LANG u otras variables no definidas en cron?

Si todo falla, usar el wrapper de logging para capturar el entorno completo y la salida.

</details>
