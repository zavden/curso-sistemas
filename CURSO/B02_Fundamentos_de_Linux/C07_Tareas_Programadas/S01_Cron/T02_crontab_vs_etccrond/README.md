# T02 — crontab vs /etc/cron.d

## Dos scopes: usuario y sistema

cron tiene dos mecanismos independientes para definir tareas:

```
                            cron daemon
                           ┌───────────┐
                           │           │
              ┌────────────┤   crond   ├────────────┐
              │            │           │            │
              ▼            └───────────┘            ▼
     Crontabs de usuario                  Crontab del sistema
     ──────────────────                   ──────────────────
     /var/spool/cron/                     /etc/crontab
     crontabs/<user>                      /etc/cron.d/*
                                          /etc/cron.{hourly,daily,...}/
     • Sin campo de usuario               • Con campo de usuario
     • crontab -e para editar             • Editar archivos directamente
     • Una tarea = un usuario             • Una tarea puede especificar
                                            cualquier usuario
```

## Crontabs de usuario

```bash
# Cada usuario gestiona su propio crontab:
crontab -e        # editar
crontab -l        # listar

# Formato (5 campos + comando, SIN campo de usuario):
*/5 * * * *  /home/user/scripts/check.sh

# Las tareas se ejecutan con el UID del propietario del crontab
# El entorno es mínimo (PATH reducido, sin variables de sesión)
```

### Cuándo usar crontabs de usuario

```bash
# - Tareas personales del usuario (backups de su home, notificaciones)
# - Aplicaciones que corren bajo un usuario específico
# - Cuando el usuario no tiene acceso root

# Ejemplo: un desarrollador que quiere rotar sus logs:
crontab -e
# 0 0 * * * find /home/dev/logs -name "*.log" -mtime +7 -delete
```

## /etc/crontab — Crontab del sistema

```bash
cat /etc/crontab
```

```bash
# Formato (6 campos — incluye USUARIO):
# m  h  dom mon dow  user    command
17 *  * * *  root    cd / && run-parts --report /etc/cron.hourly
25 6  * * *  root    test -x /usr/sbin/anacron || run-parts --report /etc/cron.daily
47 6  * * 7  root    test -x /usr/sbin/anacron || run-parts --report /etc/cron.weekly
52 6  1 * *  root    test -x /usr/sbin/anacron || run-parts --report /etc/cron.monthly
```

```bash
# Diferencias con crontab de usuario:
# 1. El sexto campo es el USUARIO que ejecuta el comando
# 2. Se edita directamente con un editor (no con crontab -e)
# 3. Puede definir variables de entorno al inicio del archivo:
SHELL=/bin/bash
PATH=/usr/local/sbin:/usr/local/bin:/sbin:/bin:/usr/sbin:/usr/bin
MAILTO=root
```

## /etc/cron.d/ — Drop-ins del sistema

Directorio para archivos crontab individuales, con el mismo formato que
`/etc/crontab` (incluyendo el campo de usuario):

```bash
ls /etc/cron.d/
# certbot         — renovación de certificados Let's Encrypt
# e2scrub_all     — verificación de filesystems ext4
# sysstat         — recopilación de estadísticas (sar)
# php             — limpieza de sesiones PHP

# Ejemplo — /etc/cron.d/certbot:
cat /etc/cron.d/certbot
# 0 */12 * * * root test -x /usr/bin/certbot -a \! -d /run/systemd/system && perl -e 'sleep int(rand(43200))' && certbot -q renew
```

### Formato de archivos en /etc/cron.d/

```bash
# /etc/cron.d/myapp
SHELL=/bin/bash
PATH=/usr/local/bin:/usr/bin:/bin
MAILTO=admin@example.com

# Backup cada noche:
0 2 * * * myappuser /opt/myapp/bin/backup.sh

# Limpieza semanal:
0 4 * * 0 root /opt/myapp/bin/cleanup.sh
```

```bash
# Reglas para archivos en /etc/cron.d/:
# 1. DEBEN incluir el campo de usuario (como /etc/crontab)
# 2. El nombre del archivo NO debe contener puntos (.)
#    - myapp     ✓
#    - myapp.cron ✗ (cron lo ignora en algunas implementaciones)
# 3. El archivo debe ser propiedad de root
# 4. Los permisos deben ser 0644 (no ejecutable)

# Debian es estricto con el nombre — run-parts ignora archivos con:
# puntos (.), guiones bajos al inicio (_), o extensiones
```

### Ventajas de /etc/cron.d/ sobre /etc/crontab

```bash
# 1. Modularidad — cada paquete/aplicación tiene su propio archivo:
#    apt install certbot → crea /etc/cron.d/certbot
#    apt remove certbot  → elimina /etc/cron.d/certbot
#    Sin tocar /etc/crontab

# 2. Gestión de configuración — herramientas como Ansible/Puppet
#    pueden desplegar archivos individuales sin conflictos

# 3. Control de versiones — cada archivo se puede versionar
#    independientemente

# 4. Desactivación rápida:
chmod 000 /etc/cron.d/myapp    # desactivar sin borrar
chmod 644 /etc/cron.d/myapp    # reactivar
```

## /etc/cron.{hourly,daily,weekly,monthly}

Directorios para scripts que se ejecutan con la periodicidad indicada:

```bash
ls /etc/cron.daily/
# apt-compat        — limpieza de apt
# dpkg              — backup de la lista de paquetes
# logrotate         — rotación de logs
# man-db            — reconstruir la cache de man

ls /etc/cron.hourly/
ls /etc/cron.weekly/
ls /etc/cron.monthly/
```

### Cómo funcionan

```bash
# /etc/crontab contiene las líneas que ejecutan estos directorios:
# 17 * * * *  root  run-parts /etc/cron.hourly
# 25 6 * * *  root  run-parts /etc/cron.daily
# 47 6 * * 7  root  run-parts /etc/cron.weekly
# 52 6 1 * *  root  run-parts /etc/cron.monthly

# run-parts ejecuta TODOS los scripts ejecutables del directorio
# en orden alfabético
```

### Reglas para scripts en estos directorios

```bash
# Los scripts DEBEN:
# 1. Ser ejecutables (chmod +x)
# 2. NO tener extensión (run-parts ignora archivos con .)
#    - backup      ✓
#    - backup.sh   ✗ (run-parts lo ignora en Debian)
# 3. Ser propiedad de root

# Verificar qué ejecutaría run-parts:
run-parts --test /etc/cron.daily
# /etc/cron.daily/apt-compat
# /etc/cron.daily/dpkg
# /etc/cron.daily/logrotate
# /etc/cron.daily/man-db

# Si un script no aparece en --test, revisar:
# - ¿Tiene extensión? (renombrar sin extensión)
# - ¿Es ejecutable? (chmod +x)
# - ¿El nombre tiene caracteres inválidos?
```

```bash
# RHEL es menos estricto con las extensiones:
# run-parts en RHEL acepta archivos con .sh
# Pero por portabilidad, evitar extensiones

# Verificar la versión de run-parts:
run-parts --version 2>/dev/null
# Si es debianutils: estricto con nombres
# Si es otro: puede ser más permisivo
```

### Ejemplo: agregar un script diario

```bash
# Crear el script:
sudo tee /etc/cron.daily/myapp-cleanup << 'EOF'
#!/bin/bash
# Limpiar archivos temporales de myapp mayores a 7 días
find /var/tmp/myapp -type f -mtime +7 -delete 2>/dev/null
logger "myapp-cleanup: limpieza completada"
EOF

# Hacerlo ejecutable:
sudo chmod +x /etc/cron.daily/myapp-cleanup

# Verificar que run-parts lo detecta:
run-parts --test /etc/cron.daily | grep myapp
# /etc/cron.daily/myapp-cleanup

# Probar manualmente:
sudo run-parts --verbose /etc/cron.daily
```

## anacron y los directorios cron.*

En sistemas con anacron (Debian/Ubuntu por defecto), los directorios
daily/weekly/monthly NO los ejecuta cron directamente sino anacron:

```bash
# En /etc/crontab (Debian con anacron):
17 *  * * *   root  cd / && run-parts --report /etc/cron.hourly
25 6  * * *   root  test -x /usr/sbin/anacron || run-parts --report /etc/cron.daily
47 6  * * 7   root  test -x /usr/sbin/anacron || run-parts --report /etc/cron.weekly
52 6  1 * *   root  test -x /usr/sbin/anacron || run-parts --report /etc/cron.monthly

# La condición: test -x /usr/sbin/anacron || run-parts ...
# Si anacron existe → cron NO ejecuta run-parts (anacron se encarga)
# Si anacron NO existe → cron ejecuta run-parts directamente

# cron.hourly siempre lo ejecuta cron (anacron no maneja intervalos < 1 día)
```

## Tabla comparativa

| Aspecto | crontab -e | /etc/crontab | /etc/cron.d/ | /etc/cron.daily/ |
|---|---|---|---|---|
| Scope | Usuario | Sistema | Sistema | Sistema |
| Campo usuario | No | Sí | Sí | N/A (root) |
| Cómo editar | crontab -e | Editor directo | Editor directo | Crear script |
| Quién lo usa | Usuarios | Sysadmin | Paquetes/apps | Scripts simples |
| Timing exacto | Sí | Sí | Sí | No (run-parts) |
| Sobrevive apt | Sí | Puede sobreescribirse | Gestionado por paquete | Gestionado por paquete |

## Listar TODAS las tareas cron del sistema

```bash
# 1. Crontabs de todos los usuarios:
for user in $(cut -d: -f1 /etc/passwd); do
    crontab -l -u "$user" 2>/dev/null | grep -v '^#' | grep -v '^$' | \
        while read -r line; do
            echo "$user: $line"
        done
done

# 2. Crontab del sistema:
grep -v '^#' /etc/crontab | grep -v '^$'

# 3. Drop-ins:
for f in /etc/cron.d/*; do
    echo "--- $f ---"
    grep -v '^#' "$f" | grep -v '^$'
done

# 4. Scripts en los directorios periódicos:
echo "--- hourly ---"; ls /etc/cron.hourly/ 2>/dev/null
echo "--- daily ---";  ls /etc/cron.daily/ 2>/dev/null
echo "--- weekly ---"; ls /etc/cron.weekly/ 2>/dev/null
echo "--- monthly ---"; ls /etc/cron.monthly/ 2>/dev/null
```

## Debian vs RHEL

| Aspecto | Debian/Ubuntu | RHEL/Fedora |
|---|---|---|
| Daemon | cron | crond |
| Crontabs de usuario | /var/spool/cron/crontabs/ | /var/spool/cron/ |
| anacron | Instalado por defecto | Instalado por defecto |
| run-parts | Estricto (sin extensiones) | Más permisivo |
| Logs | /var/log/syslog (grep CRON) | /var/log/cron |

---

## Ejercicios

### Ejercicio 1 — Explorar el sistema

```bash
# 1. ¿Qué hay en /etc/crontab?
cat /etc/crontab

# 2. ¿Cuántos archivos hay en /etc/cron.d/?
ls /etc/cron.d/ 2>/dev/null | wc -l

# 3. ¿Qué scripts se ejecutan diariamente?
run-parts --test /etc/cron.daily 2>/dev/null
```

### Ejercicio 2 — Identificar el scope

```bash
# ¿Dónde pondrías cada tarea?
# a) Backup de /home del usuario "dev" cada noche
# b) Renovación de certificados SSL cada 12 horas
# c) Limpieza de /tmp todos los domingos
# d) Script personal que chequea un API
```

### Ejercicio 3 — Auditar tareas

```bash
# Listar TODAS las tareas cron programadas en el sistema:
echo "=== User crontabs ===" && \
for user in $(cut -d: -f1 /etc/passwd); do
    out=$(crontab -l -u "$user" 2>/dev/null | grep -v '^#' | grep -v '^$')
    [ -n "$out" ] && echo "$user:" && echo "$out"
done
echo "=== /etc/crontab ===" && grep -v '^#' /etc/crontab | grep -v '^$'
echo "=== /etc/cron.d/ ===" && cat /etc/cron.d/* 2>/dev/null | grep -v '^#' | grep -v '^$'
```
