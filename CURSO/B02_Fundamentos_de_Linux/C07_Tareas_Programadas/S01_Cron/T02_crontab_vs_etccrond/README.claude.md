# T02 — crontab vs /etc/cron.d

> **Objetivo:** Distinguir los dos scopes de cron (usuario vs sistema), entender /etc/crontab, /etc/cron.d/, los directorios periódicos, run-parts, la relación con anacron, y cómo auditar todas las tareas cron del sistema.

## Erratas corregidas respecto a `README.md`

| # | Línea | Error | Corrección |
|---|-------|-------|------------|
| 1 | 117–118 | Afirma que run-parts ignora "guiones bajos al inicio (_)" | El regex de Debian run-parts es `^[a-zA-Z0-9_-]+$` — acepta guiones bajos en cualquier posición. Lo que rechaza son puntos, tildes y otros caracteres especiales. |

---

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
     • Tarea se ejecuta como              • Tarea puede especificar
       el propietario del crontab           cualquier usuario
```

---

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

- Tareas personales del usuario (backups de su home, notificaciones)
- Aplicaciones que corren bajo un usuario específico
- Cuando el usuario no tiene acceso root

```bash
# Ejemplo: un desarrollador que quiere rotar sus logs:
crontab -e
# 0 0 * * * find /home/dev/logs -name "*.log" -mtime +7 -delete
```

---

## /etc/crontab — Crontab del sistema

```bash
cat /etc/crontab
```

```bash
# Formato (6 campos — incluye USUARIO):
# m  h  dom mon dow  user    command
SHELL=/bin/bash
PATH=/usr/local/sbin:/usr/local/bin:/sbin:/bin:/usr/sbin:/usr/bin
MAILTO=root

17 *  * * *  root    cd / && run-parts --report /etc/cron.hourly
25 6  * * *  root    test -x /usr/sbin/anacron || run-parts --report /etc/cron.daily
47 6  * * 7  root    test -x /usr/sbin/anacron || run-parts --report /etc/cron.weekly
52 6  1 * *  root    test -x /usr/sbin/anacron || run-parts --report /etc/cron.monthly
```

Diferencias con crontab de usuario:
1. El **sexto campo** es el usuario que ejecuta el comando
2. Se edita directamente con un editor (no con `crontab -e`)
3. Puede definir variables de entorno al inicio del archivo (`SHELL`, `PATH`, `MAILTO`)

---

## /etc/cron.d/ — Drop-ins del sistema

Directorio para archivos crontab individuales, con el mismo formato que `/etc/crontab` (incluyendo el campo de usuario):

```bash
ls /etc/cron.d/
# certbot         — renovación de certificados Let's Encrypt
# e2scrub_all     — verificación de filesystems ext4
# sysstat         — recopilación de estadísticas (sar)
# php             — limpieza de sesiones PHP
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

### Reglas para archivos en /etc/cron.d/

```bash
# 1. DEBEN incluir el campo de usuario (como /etc/crontab)
# 2. El nombre del archivo NO debe contener puntos (.)
#    - myapp     ✓
#    - myapp.cron ✗ (cron lo ignora en algunas implementaciones)
# 3. El archivo debe ser propiedad de root
# 4. Los permisos deben ser 0644 (no ejecutable)
```

> **Nota:** en Debian, el regex de `run-parts` para nombres válidos es `^[a-zA-Z0-9_-]+$`. Esto significa que acepta letras, números, guiones y guiones bajos en cualquier posición. Lo que rechaza son puntos, tildes y otros caracteres especiales.

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

---

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
# 2. NO tener extensión en Debian (run-parts rechaza archivos con .)
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
# - ¿El nombre tiene caracteres inválidos? (puntos, tildes)
```

```bash
# RHEL es menos estricto con las extensiones:
# run-parts en RHEL acepta archivos con .sh
# Pero por portabilidad, evitar extensiones
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

---

## anacron y los directorios cron.*

En sistemas con anacron (Debian/Ubuntu por defecto), los directorios daily/weekly/monthly **no los ejecuta cron directamente** sino anacron:

```bash
# En /etc/crontab (Debian con anacron):
25 6  * * *   root  test -x /usr/sbin/anacron || run-parts --report /etc/cron.daily
47 6  * * 7   root  test -x /usr/sbin/anacron || run-parts --report /etc/cron.weekly
52 6  1 * *   root  test -x /usr/sbin/anacron || run-parts --report /etc/cron.monthly

# La condición: test -x /usr/sbin/anacron || run-parts ...
# Si anacron existe → cron NO ejecuta run-parts (anacron se encarga)
# Si anacron NO existe → cron ejecuta run-parts directamente

# cron.hourly siempre lo ejecuta cron (anacron no maneja intervalos < 1 día)
```

---

## Tabla comparativa

| Aspecto | `crontab -e` | `/etc/crontab` | `/etc/cron.d/` | `/etc/cron.daily/` |
|---|---|---|---|---|
| Scope | Usuario | Sistema | Sistema | Sistema |
| Campo usuario | No | Sí | Sí | N/A (root) |
| Cómo editar | `crontab -e` | Editor directo | Editor directo | Crear script |
| Quién lo usa | Usuarios | Sysadmin | Paquetes/apps | Scripts simples |
| Timing exacto | Sí | Sí | Sí | No (run-parts) |
| Sobrevive apt | Sí | Puede sobreescribirse | Gestionado por paquete | Gestionado por paquete |

### Regla general para decidir dónde poner una tarea

| Tarea | Dónde |
|-------|-------|
| Backup personal de ~/ | `crontab -e` (usuario) |
| Renovar certificados SSL | `/etc/cron.d/certbot` |
| Limpiar /tmp | `/etc/cron.daily/cleanup` |
| Script personal de API | `crontab -e` (usuario) |
| Backup de base de datos | `/etc/cron.d/db-backup` |
| Rotación de logs | `/etc/cron.daily/logrotate` |

---

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

---

## Debian vs RHEL

| Aspecto | Debian/Ubuntu | RHEL/Fedora |
|---|---|---|
| Daemon | `cron` | `crond` |
| Crontabs de usuario | `/var/spool/cron/crontabs/` | `/var/spool/cron/` |
| anacron | Instalado por defecto | Instalado por defecto |
| run-parts | Estricto (sin extensiones) | Más permisivo (acepta `.sh`) |
| Logs | `/var/log/syslog` (grep CRON) | `/var/log/cron` |

---

## Lab — crontab vs /etc/cron.d

### Parte 1 — Scopes de cron

#### Paso 1.1: Dos scopes

```bash
docker compose exec debian-dev bash -c '
echo "=== Dos scopes de cron ==="
echo ""
echo "  Crontabs de usuario          Crontab del sistema"
echo "  ──────────────────           ──────────────────"
echo "  /var/spool/cron/             /etc/crontab"
echo "  crontabs/<user>              /etc/cron.d/*"
echo ""
echo "  Sin campo de usuario         Con campo de usuario"
echo "  crontab -e para editar       Editar archivos directamente"
echo "  Tarea = UID del usuario      Tarea puede ser cualquier usuario"
echo ""
echo "--- Formato de usuario ---"
echo "*/5 * * * *  /home/user/scripts/check.sh"
echo "  5 campos + comando"
echo ""
echo "--- Formato del sistema ---"
echo "*/5 * * * * root  /usr/local/bin/check.sh"
echo "  5 campos + USUARIO + comando"
'
```

#### Paso 1.2: /etc/crontab

```bash
docker compose exec debian-dev bash -c '
echo "=== /etc/crontab ==="
echo ""
if [[ -f /etc/crontab ]]; then
    echo "--- Contenido ---"
    cat /etc/crontab
else
    echo "/etc/crontab no existe"
    echo ""
    echo "--- Contenido tipico ---"
    echo "SHELL=/bin/sh"
    echo "PATH=/usr/local/sbin:/usr/local/bin:/sbin:/bin:/usr/sbin:/usr/bin"
    echo ""
    echo "17 *  * * *  root  cd / && run-parts --report /etc/cron.hourly"
    echo "25 6  * * *  root  test -x /usr/sbin/anacron || run-parts --report /etc/cron.daily"
    echo "47 6  * * 7  root  test -x /usr/sbin/anacron || run-parts --report /etc/cron.weekly"
    echo "52 6  1 * *  root  test -x /usr/sbin/anacron || run-parts --report /etc/cron.monthly"
fi
echo ""
echo "--- Diferencias con crontab de usuario ---"
echo "1. Campo 6 = USUARIO que ejecuta el comando"
echo "2. Se edita directamente con un editor (no crontab -e)"
echo "3. Puede definir variables al inicio (SHELL, PATH, MAILTO)"
'
```

#### Paso 1.3: /etc/cron.d/

```bash
docker compose exec debian-dev bash -c '
echo "=== /etc/cron.d/ ==="
echo ""
echo "Directorio para archivos crontab individuales."
echo "Mismo formato que /etc/crontab (con campo de usuario)."
echo ""
echo "--- Archivos existentes ---"
if [[ -d /etc/cron.d ]]; then
    ls -la /etc/cron.d/ 2>/dev/null
    echo ""
    for f in /etc/cron.d/*; do
        if [[ -f "$f" ]]; then
            echo "--- $(basename "$f") ---"
            grep -v "^#" "$f" | grep -v "^$" | head -5
            echo ""
        fi
    done
else
    echo "(directorio no encontrado)"
fi
echo ""
echo "--- Reglas para /etc/cron.d/ ---"
echo "1. DEBEN incluir el campo de usuario"
echo "2. Nombre SIN puntos (myapp, no myapp.cron)"
echo "3. Propiedad de root"
echo "4. Permisos 0644 (no ejecutable)"
'
```

#### Paso 1.4: Ventajas de /etc/cron.d/

```bash
docker compose exec debian-dev bash -c '
echo "=== Ventajas de /etc/cron.d/ ==="
echo ""
echo "1. Modularidad:"
echo "   apt install certbot → crea /etc/cron.d/certbot"
echo "   apt remove certbot  → elimina /etc/cron.d/certbot"
echo "   Sin tocar /etc/crontab"
echo ""
echo "2. Gestion de configuracion:"
echo "   Ansible/Puppet despliegan archivos individuales"
echo ""
echo "3. Desactivacion rapida:"
echo "   chmod 000 /etc/cron.d/myapp    desactivar sin borrar"
echo "   chmod 644 /etc/cron.d/myapp    reactivar"
echo ""
echo "4. Control de versiones:"
echo "   Cada archivo se puede versionar independientemente"
'
```

---

### Parte 2 — Directorios periódicos

#### Paso 2.1: Directorios periódicos

```bash
docker compose exec debian-dev bash -c '
echo "=== Directorios periodicos ==="
echo ""
echo "--- /etc/cron.hourly ---"
ls /etc/cron.hourly/ 2>/dev/null || echo "  (vacio o no existe)"
echo ""
echo "--- /etc/cron.daily ---"
ls /etc/cron.daily/ 2>/dev/null || echo "  (vacio o no existe)"
echo ""
echo "--- /etc/cron.weekly ---"
ls /etc/cron.weekly/ 2>/dev/null || echo "  (vacio o no existe)"
echo ""
echo "--- /etc/cron.monthly ---"
ls /etc/cron.monthly/ 2>/dev/null || echo "  (vacio o no existe)"
echo ""
echo "--- Como funcionan ---"
echo "/etc/crontab contiene las lineas que los ejecutan:"
echo "  17 *  * * *  root  run-parts /etc/cron.hourly"
echo "  25 6  * * *  root  run-parts /etc/cron.daily"
echo "  47 6  * * 7  root  run-parts /etc/cron.weekly"
echo "  52 6  1 * *  root  run-parts /etc/cron.monthly"
echo ""
echo "run-parts ejecuta TODOS los scripts ejecutables del directorio"
echo "en orden alfabetico."
'
```

#### Paso 2.2: Reglas de run-parts

```bash
docker compose exec debian-dev bash -c '
echo "=== Reglas de run-parts ==="
echo ""
echo "Los scripts DEBEN:"
echo "  1. Ser ejecutables (chmod +x)"
echo "  2. NO tener extension en Debian (run-parts ignora archivos con .)"
echo "     - backup      OK"
echo "     - backup.sh   IGNORADO en Debian"
echo "  3. Ser propiedad de root"
echo ""
echo "--- Verificar que ejecutaria run-parts ---"
if command -v run-parts &>/dev/null; then
    echo ""
    echo "cron.daily:"
    run-parts --test /etc/cron.daily 2>/dev/null || \
        echo "  (directorio no encontrado)"
    echo ""
    echo "cron.hourly:"
    run-parts --test /etc/cron.hourly 2>/dev/null || \
        echo "  (directorio no encontrado)"
else
    echo "run-parts no disponible"
fi
echo ""
echo "--- RHEL es menos estricto ---"
echo "run-parts en RHEL acepta archivos con .sh"
echo "Pero por portabilidad, evitar extensiones"
'
```

#### Paso 2.3: La relación con anacron

```bash
docker compose exec debian-dev bash -c '
echo "=== Relacion cron/anacron ==="
echo ""
echo "En Debian con anacron instalado:"
echo ""
echo "  test -x /usr/sbin/anacron || run-parts /etc/cron.daily"
echo ""
echo "  Si anacron existe → cron NO ejecuta run-parts"
echo "    (anacron se encarga)"
echo "  Si anacron NO existe → cron ejecuta directamente"
echo ""
echo "  cron.hourly siempre lo ejecuta cron"
echo "  (anacron no maneja intervalos < 1 dia)"
echo ""
echo "--- Verificar ---"
if [[ -x /usr/sbin/anacron ]]; then
    echo "anacron INSTALADO"
    echo "  → cron.daily/weekly/monthly los gestiona anacron"
    echo "  → cron.hourly lo gestiona cron"
else
    echo "anacron NO INSTALADO"
    echo "  → cron gestiona todo directamente"
fi
'
```

---

### Parte 3 — Auditoría y diferencias

#### Paso 3.1: Auditar todas las tareas cron

```bash
docker compose exec debian-dev bash -c '
echo "=== Auditar todas las tareas cron ==="
echo ""
echo "--- 1. Crontabs de usuarios ---"
for user in $(cut -d: -f1 /etc/passwd 2>/dev/null); do
    out=$(crontab -l -u "$user" 2>/dev/null | grep -v "^#" | grep -v "^$")
    if [[ -n "$out" ]]; then
        echo "  $user:"
        echo "    $out"
    fi
done
echo "  (si no aparece nada, no hay crontabs de usuario)"
echo ""
echo "--- 2. /etc/crontab ---"
if [[ -f /etc/crontab ]]; then
    grep -v "^#" /etc/crontab | grep -v "^$" | head -10
else
    echo "  (no existe)"
fi
echo ""
echo "--- 3. /etc/cron.d/ ---"
if [[ -d /etc/cron.d ]]; then
    for f in /etc/cron.d/*; do
        [[ -f "$f" ]] && echo "  $(basename "$f")"
    done
else
    echo "  (no existe)"
fi
echo ""
echo "--- 4. Scripts periodicos ---"
echo "  hourly:  $(ls /etc/cron.hourly/ 2>/dev/null | wc -l) scripts"
echo "  daily:   $(ls /etc/cron.daily/ 2>/dev/null | wc -l) scripts"
echo "  weekly:  $(ls /etc/cron.weekly/ 2>/dev/null | wc -l) scripts"
echo "  monthly: $(ls /etc/cron.monthly/ 2>/dev/null | wc -l) scripts"
'
```

#### Paso 3.2: Debian vs RHEL

```bash
docker compose exec debian-dev bash -c '
echo "=== Debian ==="
echo ""
echo "Daemon: cron"
echo "Crontabs: /var/spool/cron/crontabs/"
echo "run-parts: estricto (sin extensiones)"
echo "Logs: /var/log/syslog (grep CRON)"
echo ""
echo "Verificar:"
ls /var/spool/cron/crontabs/ 2>/dev/null || echo "  (directorio no encontrado)"
'
```

```bash
docker compose exec alma-dev bash -c '
echo "=== AlmaLinux ==="
echo ""
echo "Daemon: crond"
echo "Crontabs: /var/spool/cron/"
echo "run-parts: mas permisivo (acepta .sh)"
echo "Logs: /var/log/cron"
echo ""
echo "Verificar:"
ls /var/spool/cron/ 2>/dev/null || echo "  (directorio no encontrado)"
echo ""
echo "Logs de cron:"
ls -la /var/log/cron 2>/dev/null || echo "  (archivo no encontrado)"
'
```

---

## Ejercicios

### Ejercicio 1 — Explorar /etc/crontab

¿Qué contiene el crontab del sistema?

```bash
cat /etc/crontab
```

<details><summary>Predicción</summary>

Verás variables de entorno al inicio (`SHELL`, `PATH`, `MAILTO`) seguidas de líneas con **6 campos** (los 5 de tiempo + el campo de usuario). El contenido típico son las líneas que ejecutan `run-parts` sobre los directorios periódicos (`/etc/cron.hourly`, `.daily`, `.weekly`, `.monthly`). Nota la condición `test -x /usr/sbin/anacron ||` que delega a anacron si está instalado.

</details>

### Ejercicio 2 — Diferencia de formato

¿Cuál es la diferencia fundamental entre un crontab de usuario y `/etc/crontab`?

```bash
# Crontab de usuario:
crontab -l 2>/dev/null

# Crontab del sistema:
head -10 /etc/crontab
```

<details><summary>Predicción</summary>

La diferencia clave es el **campo de usuario** (el sexto campo). En un crontab de usuario (`crontab -e`), las líneas tienen 5 campos de tiempo + comando. En `/etc/crontab` y `/etc/cron.d/`, las líneas tienen 5 campos de tiempo + **usuario** + comando. Omitir el campo de usuario en `/etc/cron.d/` causa que cron interprete el nombre del usuario como parte del comando, lo cual falla silenciosamente.

</details>

### Ejercicio 3 — Explorar /etc/cron.d/

¿Qué archivos drop-in existen y qué hacen?

```bash
# Listar archivos:
ls -la /etc/cron.d/ 2>/dev/null

# Ver contenido de cada uno:
for f in /etc/cron.d/*; do
    [ -f "$f" ] && echo "=== $(basename $f) ===" && \
    grep -v '^#' "$f" | grep -v '^$'
done
```

<details><summary>Predicción</summary>

Verás archivos creados por paquetes instalados (como `certbot`, `e2scrub_all`, `sysstat`, etc.). Cada archivo tiene el mismo formato que `/etc/crontab`: 5 campos de tiempo + usuario + comando. Los nombres **no tienen extensión** (no `.cron` ni `.conf`). Los permisos son `0644` y el propietario es root.

</details>

### Ejercicio 4 — run-parts y los directorios periódicos

¿Qué scripts se ejecutan diariamente?

```bash
# Ver qué ejecutaría run-parts:
run-parts --test /etc/cron.daily 2>/dev/null

# Comparar con el contenido real:
ls -la /etc/cron.daily/ 2>/dev/null
```

<details><summary>Predicción</summary>

`run-parts --test` mostrará solo los scripts que **cumplen las reglas**: ejecutables, sin extensión (en Debian), propiedad de root. Si un archivo tiene extensión `.sh`, `run-parts --test` **no lo listará** en Debian aunque aparezca con `ls`. Scripts típicos: `apt-compat`, `dpkg`, `logrotate`, `man-db`.

</details>

### Ejercicio 5 — La trampa de la extensión

¿Por qué un script con extensión `.sh` podría no ejecutarse en `/etc/cron.daily/`?

```bash
# Crear dos scripts de prueba:
sudo touch /etc/cron.daily/test-noext
sudo touch /etc/cron.daily/test-ext.sh
sudo chmod +x /etc/cron.daily/test-noext /etc/cron.daily/test-ext.sh

# Verificar cuál detecta run-parts:
run-parts --test /etc/cron.daily 2>/dev/null | grep test

# Limpiar:
sudo rm -f /etc/cron.daily/test-noext /etc/cron.daily/test-ext.sh
```

<details><summary>Predicción</summary>

En **Debian**, solo `test-noext` aparecerá en `run-parts --test`. El archivo `test-ext.sh` será ignorado porque el punto en `.sh` no cumple el regex `^[a-zA-Z0-9_-]+$`. En **RHEL**, ambos aparecerían porque su `run-parts` es más permisivo. Por portabilidad, siempre nombrar scripts sin extensión en estos directorios.

</details>

### Ejercicio 6 — Verificar anacron

¿Quién gestiona los directorios periódicos en tu sistema, cron o anacron?

```bash
# ¿Está anacron instalado?
test -x /usr/sbin/anacron && echo "anacron instalado" || echo "anacron NO instalado"

# Verificar la condición en /etc/crontab:
grep anacron /etc/crontab 2>/dev/null
```

<details><summary>Predicción</summary>

Si anacron está instalado (default en Debian y RHEL), verás las líneas `test -x /usr/sbin/anacron || run-parts ...`. Como el test es verdadero (`anacron` existe), cron **no ejecuta** `run-parts` para daily/weekly/monthly — anacron se encarga. Solo `cron.hourly` lo ejecuta cron directamente, porque anacron no maneja intervalos menores a 1 día.

</details>

### Ejercicio 7 — Auditar todas las tareas

Lista todas las tareas cron programadas en el sistema.

```bash
echo "=== Crontabs de usuarios ==="
for user in $(cut -d: -f1 /etc/passwd); do
    out=$(crontab -l -u "$user" 2>/dev/null | grep -v '^#' | grep -v '^$')
    [ -n "$out" ] && echo "$user:" && echo "$out"
done

echo "=== /etc/crontab ==="
grep -v '^#' /etc/crontab 2>/dev/null | grep -v '^$'

echo "=== /etc/cron.d/ ==="
for f in /etc/cron.d/*; do
    [ -f "$f" ] && echo "--- $(basename $f) ---" && \
    grep -v '^#' "$f" | grep -v '^$'
done

echo "=== Scripts periódicos ==="
for d in hourly daily weekly monthly; do
    echo "  $d: $(ls /etc/cron.$d/ 2>/dev/null | tr '\n' ' ')"
done
```

<details><summary>Predicción</summary>

Obtendrás un inventario completo de todas las tareas cron del sistema:
- **Crontabs de usuarios**: probablemente vacío en un sistema limpio.
- **/etc/crontab**: las líneas de run-parts para los directorios periódicos.
- **/etc/cron.d/**: archivos de paquetes instalados (certbot, sysstat, etc.).
- **Scripts periódicos**: logrotate, dpkg, man-db en daily; posiblemente vacío en hourly/weekly/monthly.

Este es un ejercicio esencial para sysadmins — saber qué se ejecuta automáticamente en un sistema.

</details>

### Ejercicio 8 — Decidir dónde poner cada tarea

¿En cuál de los 4 mecanismos colocarías cada tarea?

```
a) Backup del home del usuario "dev" cada noche a las 02:00
b) Renovación automática de certificados SSL cada 12 horas
c) Limpieza de /tmp cada domingo
d) Script personal que consulta una API cada 5 minutos
e) Rotación de logs del sistema
```

<details><summary>Predicción</summary>

- **a)** `crontab -e` como usuario `dev` → tarea personal, no requiere root.
- **b)** `/etc/cron.d/certbot` → tarea del sistema, gestionada por el paquete certbot. Necesita timing exacto (cada 12h).
- **c)** `/etc/cron.weekly/cleanup-tmp` o `/etc/cron.d/cleanup-tmp` → tarea del sistema. Si solo necesita "una vez por semana" sin hora exacta, el directorio periódico basta. Si necesita un día/hora específico, usar `/etc/cron.d/`.
- **d)** `crontab -e` como el usuario que lo necesita → tarea personal, no del sistema.
- **e)** `/etc/cron.daily/logrotate` → ya viene así por defecto. logrotate se instala como script en cron.daily.

</details>

### Ejercicio 9 — Desactivar sin borrar

¿Cómo desactivar temporalmente un archivo en `/etc/cron.d/` sin eliminarlo?

```bash
# Opción 1: Quitar permisos de lectura
sudo chmod 000 /etc/cron.d/myapp
# Reactivar:
sudo chmod 644 /etc/cron.d/myapp

# Opción 2: Renombrar añadiendo extensión
sudo mv /etc/cron.d/myapp /etc/cron.d/myapp.disabled
# Reactivar:
sudo mv /etc/cron.d/myapp.disabled /etc/cron.d/myapp
```

<details><summary>Predicción</summary>

Ambas opciones funcionan:
- **chmod 000**: cron no puede leer el archivo, así que lo ignora. Es rápido y reversible.
- **Renombrar con extensión**: como `myapp.disabled` contiene un punto, cron lo ignora (en Debian al menos). Es más explícito visualmente.

La opción 2 es más clara en auditorías porque `ls /etc/cron.d/` revela inmediatamente que está desactivado. La opción 1 requiere inspeccionar permisos.

</details>

### Ejercicio 10 — Comparar Debian vs RHEL

Identifica las diferencias de cron entre ambas familias de distribuciones.

```bash
# En Debian:
docker compose exec debian-dev bash -c '
echo "=== Debian ==="
echo "Daemon: $(systemctl is-active cron 2>/dev/null || echo "no activo")"
echo "Spool: $(ls /var/spool/cron/crontabs/ 2>/dev/null | wc -l) crontabs"
echo "cron.d: $(ls /etc/cron.d/ 2>/dev/null | wc -l) archivos"
echo "cron.daily: $(ls /etc/cron.daily/ 2>/dev/null | wc -l) scripts"
'

# En RHEL:
docker compose exec alma-dev bash -c '
echo "=== AlmaLinux ==="
echo "Daemon: $(systemctl is-active crond 2>/dev/null || echo "no activo")"
echo "Spool: $(ls /var/spool/cron/ 2>/dev/null | wc -l) crontabs"
echo "cron.d: $(ls /etc/cron.d/ 2>/dev/null | wc -l) archivos"
echo "cron.daily: $(ls /etc/cron.daily/ 2>/dev/null | wc -l) scripts"
'
```

<details><summary>Predicción</summary>

| Aspecto | Debian | RHEL/AlmaLinux |
|---------|--------|----------------|
| Daemon | `cron` | `crond` |
| Spool | `/var/spool/cron/crontabs/` | `/var/spool/cron/` |
| run-parts | Estricto (sin extensiones) | Permisivo (acepta `.sh`) |
| Logs | `/var/log/syslog` (CRON) | `/var/log/cron` |

Ambos tienen anacron por defecto. Los contenedores pueden no tener cron activo, pero la estructura de directorios suele existir.

</details>
