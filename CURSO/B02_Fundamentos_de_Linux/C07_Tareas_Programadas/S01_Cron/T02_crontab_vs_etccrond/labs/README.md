# Lab — crontab vs /etc/cron.d

## Objetivo

Entender los dos scopes de cron: crontabs de usuario (crontab -e)
vs crontab del sistema (/etc/crontab y /etc/cron.d/), el campo de
usuario que diferencia ambos formatos, los directorios periodicos
(/etc/cron.daily, weekly, monthly), las reglas de run-parts, la
relacion con anacron, y como auditar todas las tareas cron del
sistema.

## Prerequisitos

- Entorno del curso levantado (`docker compose up -d`)

---

## Parte 1 — Scopes de cron

### Objetivo

Distinguir entre crontabs de usuario y crontabs del sistema.

### Paso 1.1: Dos scopes

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

### Paso 1.2: /etc/crontab

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

### Paso 1.3: /etc/cron.d/

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

### Paso 1.4: Ventajas de /etc/cron.d/

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
echo ""
echo "--- Ejemplo de archivo ---"
echo "# /etc/cron.d/myapp"
echo "SHELL=/bin/bash"
echo "PATH=/usr/local/bin:/usr/bin:/bin"
echo "MAILTO=admin@example.com"
echo ""
echo "0 2 * * * myappuser /opt/myapp/bin/backup.sh"
echo "0 4 * * 0 root /opt/myapp/bin/cleanup.sh"
'
```

---

## Parte 2 — Directorios periodicos

### Objetivo

Entender los directorios /etc/cron.{hourly,daily,weekly,monthly}
y las reglas de run-parts.

### Paso 2.1: Directorios periodicos

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

### Paso 2.2: Reglas de run-parts

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

### Paso 2.3: La relacion con anacron

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

### Paso 2.4: Ejemplo de script en cron.daily

```bash
docker compose exec debian-dev bash -c '
echo "=== Ejemplo: agregar un script diario ==="
echo ""
echo "--- Crear el script ---"
echo "cat > /etc/cron.daily/myapp-cleanup << '\'EOF'\''"
echo "#!/bin/bash"
echo "find /var/tmp/myapp -type f -mtime +7 -delete 2>/dev/null"
echo "logger \"myapp-cleanup: completado\""
echo "EOF"
echo ""
echo "--- Hacerlo ejecutable ---"
echo "chmod +x /etc/cron.daily/myapp-cleanup"
echo ""
echo "--- Verificar que run-parts lo detecta ---"
echo "run-parts --test /etc/cron.daily | grep myapp"
echo ""
echo "--- Probar manualmente ---"
echo "run-parts --verbose /etc/cron.daily"
echo ""
echo "--- Nota ---"
echo "Nombre sin extension (.sh) para Debian."
echo "Propiedad de root, permisos ejecutables."
'
```

---

## Parte 3 — Auditoria y diferencias

### Objetivo

Listar todas las tareas cron del sistema y comparar Debian vs RHEL.

### Paso 3.1: Auditar todas las tareas cron

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

### Paso 3.2: Donde poner cada tarea

```bash
docker compose exec debian-dev bash -c '
echo "=== Donde poner cada tarea ==="
echo ""
echo "| Tarea                        | Donde                      |"
echo "|------------------------------|----------------------------|"
echo "| Backup personal de ~/        | crontab -e (usuario)       |"
echo "| Renovar certificados SSL     | /etc/cron.d/certbot        |"
echo "| Limpiar /tmp                 | /etc/cron.daily/cleanup    |"
echo "| Script personal de API       | crontab -e (usuario)       |"
echo "| Backup de base de datos      | /etc/cron.d/db-backup      |"
echo "| Rotacion de logs             | /etc/cron.daily/logrotate  |"
echo ""
echo "--- Regla general ---"
echo "  Tarea personal → crontab -e"
echo "  Tarea del sistema → /etc/cron.d/"
echo "  Tarea periodica simple → /etc/cron.daily/"
echo "  Tarea de paquete → paquete la instala en /etc/cron.d/"
'
```

### Paso 3.3: Tabla comparativa

```bash
docker compose exec debian-dev bash -c '
echo "=== Tabla comparativa ==="
echo ""
echo "| Aspecto        | crontab -e    | /etc/crontab | /etc/cron.d/ | cron.daily/ |"
echo "|----------------|---------------|--------------|--------------|-------------|"
echo "| Scope          | Usuario       | Sistema      | Sistema      | Sistema     |"
echo "| Campo usuario  | No            | Si           | Si           | N/A (root)  |"
echo "| Como editar    | crontab -e    | Editor       | Editor       | Crear script|"
echo "| Quien lo usa   | Usuarios      | Sysadmin     | Paquetes     | Scripts     |"
echo "| Timing exacto  | Si            | Si           | Si           | No          |"
echo "| Sobrevive apt  | Si            | Puede cambiar| Gestionado   | Gestionado  |"
'
```

### Paso 3.4: Debian vs RHEL

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

## Limpieza final

No hay recursos que limpiar.

---

## Conceptos reforzados

1. cron tiene dos scopes: crontabs de usuario (crontab -e, 5 campos)
   y crontabs del sistema (/etc/crontab y /etc/cron.d/, 6 campos
   con usuario).

2. /etc/cron.d/ es modular: cada paquete/aplicacion tiene su
   propio archivo. Ventajas: instalacion/desinstalacion limpia,
   gestion de configuracion, desactivacion rapida.

3. Los directorios /etc/cron.{hourly,daily,weekly,monthly} ejecutan
   scripts con run-parts. En Debian, los scripts NO deben tener
   extension y deben ser ejecutables.

4. Con anacron instalado, cron delega cron.daily/weekly/monthly a
   anacron. Solo cron.hourly lo ejecuta cron directamente.

5. Para auditar tareas cron: revisar crontabs de usuarios,
   /etc/crontab, /etc/cron.d/, y los directorios periodicos.

6. Debian usa `cron` y /var/spool/cron/crontabs/. RHEL usa `crond`
   y /var/spool/cron/. run-parts en Debian es estricto con nombres.
