# Lab — Sintaxis crontab

## Objetivo

Dominar la sintaxis de crontab: los 5 campos de tiempo, caracteres
especiales (asterisco, coma, guion, barra), la regla OR entre dia
del mes y dia de la semana, cadenas especiales (@daily, @reboot),
gestion del crontab con crontab -e/-l/-r, redireccion de salida,
y errores comunes como el % y el PATH reducido.

## Prerequisitos

- Entorno del curso levantado (`docker compose up -d`)

---

## Parte 1 — Los 5 campos y caracteres especiales

### Objetivo

Entender la estructura de una linea crontab y los caracteres
especiales disponibles.

### Paso 1.1: Estructura de una linea crontab

```bash
docker compose exec debian-dev bash -c '
echo "=== Los 5 campos de crontab ==="
echo ""
echo "  ┌───────────── minuto       (0-59)"
echo "  │ ┌─────────── hora         (0-23)"
echo "  │ │ ┌───────── dia del mes  (1-31)"
echo "  │ │ │ ┌─────── mes          (1-12)"
echo "  │ │ │ │ ┌───── dia de la semana (0-7, 0 y 7 = domingo)"
echo "  │ │ │ │ │"
echo "  * * * * *  comando"
echo ""
echo "--- Valores validos ---"
echo "| Campo            | Rango  | Notas                          |"
echo "|------------------|--------|--------------------------------|"
echo "| Minuto           | 0-59   |                                |"
echo "| Hora             | 0-23   | 0 = medianoche                 |"
echo "| Dia del mes      | 1-31   | Dias inexistentes se ignoran   |"
echo "| Mes              | 1-12   | Tambien: jan, feb, mar...      |"
echo "| Dia de la semana | 0-7    | 0 y 7 = domingo. mon, tue...   |"
echo ""
echo "--- Ejemplos basicos ---"
echo "30 2 * * *    → Todos los dias a las 02:30"
echo "0 9 1 * *     → El dia 1 de cada mes a las 09:00"
echo "0 0 * * 0     → Todos los domingos a medianoche"
echo "0 6 * jan,jul mon   → Lunes de enero y julio a las 06:00"
'
```

### Paso 1.2: Caracteres especiales

```bash
docker compose exec debian-dev bash -c '
echo "=== Caracteres especiales ==="
echo ""
echo "--- * (asterisco) — cualquier valor ---"
echo "* * * * *    cada minuto"
echo "0 * * * *    cada hora en punto"
echo ""
echo "--- , (coma) — lista de valores ---"
echo "0 8,12,18 * * *    a las 08:00, 12:00 y 18:00"
echo "0 0 1,15 * *       dias 1 y 15 a medianoche"
echo ""
echo "--- - (guion) — rango ---"
echo "0 9-17 * * 1-5     lun-vie, 09:00 a 17:00 cada hora"
echo "30 6 1-7 * *       primeros 7 dias del mes a las 06:30"
echo ""
echo "--- / (barra) — intervalo (step) ---"
echo "*/5 * * * *        cada 5 minutos"
echo "0 */2 * * *        cada 2 horas en punto"
echo "*/10 9-17 * * 1-5  cada 10 min en horario laboral"
echo ""
echo "--- Combinaciones ---"
echo "0 8,12,18 1-15 */2 *   08:00/12:00/18:00, dias 1-15, meses pares"
echo "0-30/10 * * * *        minutos 0, 10, 20, 30 de cada hora"
'
```

### Paso 1.3: Leer expresiones cron

```bash
docker compose exec debian-dev bash -c '
echo "=== Ejercicio: leer expresiones cron ==="
echo ""
echo "Predice cuando se ejecuta cada una antes de ver la respuesta."
echo ""
echo "a) 30 4 * * *"
echo "b) 0 */6 * * 1-5"
echo "c) 15,45 9-17 * * *"
echo "d) 0 0 1 1,4,7,10 *"
echo "e) */5 * * * 0"
echo ""
echo "--- Respuestas ---"
echo "a) Todos los dias a las 04:30"
echo "b) Lun-vie a las 00:00, 06:00, 12:00, 18:00"
echo "c) Todos los dias a los minutos :15 y :45, de 09:00 a 17:00"
echo "d) El dia 1 de enero, abril, julio y octubre a medianoche"
echo "e) Cada 5 minutos los domingos"
'
```

### Paso 1.4: La regla OR entre dia del mes y dia de la semana

```bash
docker compose exec debian-dev bash -c '
echo "=== Dia del mes Y dia de la semana: OR ==="
echo ""
echo "Cuando se especifican AMBOS, cron los combina con OR:"
echo ""
echo "  0 9 15 * 5"
echo "  → el dia 15 de cada mes O cualquier viernes"
echo "  → NO: el dia 15 solo si cae en viernes"
echo ""
echo "Esto es contraintuitivo. Los demas campos son AND:"
echo "  0 9 * 6 1"
echo "  → todos los lunes de junio a las 09:00 (mes AND dia semana)"
echo ""
echo "--- Ejemplo de error comun ---"
echo "Quiero: el primer viernes de cada mes"
echo "Escribo: 0 9 1-7 * 5"
echo "Obtengo: dias 1-7 OR viernes (no solo el primer viernes)"
echo ""
echo "--- Solucion correcta ---"
echo "0 9 1-7 * * [ \"\$(date +\\%u)\" = 5 ] && /path/to/script.sh"
echo "  Ejecuta dias 1-7, pero verifica que sea viernes (%u: 5=vie)"
'
```

---

## Parte 2 — Cadenas especiales y gestion

### Objetivo

Usar las cadenas especiales de cron y gestionar crontabs de usuario.

### Paso 2.1: Cadenas especiales

```bash
docker compose exec debian-dev bash -c '
echo "=== Cadenas especiales ==="
echo ""
echo "| Cadena     | Equivalente     | Significado                |"
echo "|------------|-----------------|----------------------------|"
echo "| @reboot    | (especial)      | Al arrancar el sistema     |"
echo "| @yearly    | 0 0 1 1 *       | 1 de enero a medianoche    |"
echo "| @annually  | 0 0 1 1 *       | Alias de @yearly           |"
echo "| @monthly   | 0 0 1 * *       | Dia 1 de cada mes          |"
echo "| @weekly    | 0 0 * * 0       | Domingo a medianoche       |"
echo "| @daily     | 0 0 * * *       | Cada dia a medianoche      |"
echo "| @midnight  | 0 0 * * *       | Alias de @daily            |"
echo "| @hourly    | 0 * * * *       | Cada hora en punto         |"
echo ""
echo "--- @reboot ---"
echo "@reboot es especialmente util:"
echo "  @reboot sleep 30 && /usr/local/bin/startup-check.sh"
echo "  Se ejecuta una vez al iniciar crond"
echo "  El sleep da tiempo a que los servicios esten listos"
'
```

### Paso 2.2: Gestion de crontabs de usuario

```bash
docker compose exec debian-dev bash -c '
echo "=== Gestion de crontabs ==="
echo ""
echo "--- Comandos ---"
echo "crontab -e       editar el crontab del usuario actual"
echo "crontab -l       listar el crontab actual"
echo "crontab -r       eliminar TODO el crontab (sin confirmacion)"
echo "crontab -ri      eliminar con confirmacion (si disponible)"
echo ""
echo "--- Como root, gestionar otro usuario ---"
echo "crontab -u www-data -l     listar el de www-data"
echo "crontab -u www-data -e     editar el de www-data"
echo ""
echo "--- Donde se almacenan ---"
echo "Debian: /var/spool/cron/crontabs/<usuario>"
echo "RHEL:   /var/spool/cron/<usuario>"
echo ""
echo "NUNCA editar estos archivos directamente."
echo "Usar siempre crontab -e (valida la sintaxis)."
echo ""
echo "--- Verificar ---"
echo "Crontab actual:"
crontab -l 2>/dev/null || echo "  (no crontab definido)"
echo ""
echo "Directorio de crontabs:"
ls /var/spool/cron/crontabs/ 2>/dev/null || \
    ls /var/spool/cron/ 2>/dev/null || \
    echo "  (directorio no encontrado)"
'
```

### Paso 2.3: Escribir expresiones cron

```bash
docker compose exec debian-dev bash -c '
echo "=== Ejercicio: escribir expresiones cron ==="
echo ""
echo "Escribe la expresion antes de ver la respuesta."
echo ""
echo "a) Cada dia a las 03:15"
echo "b) Cada lunes y miercoles a las 08:00"
echo "c) Cada 15 minutos de lunes a viernes"
echo "d) El 31 de diciembre a las 23:59"
echo "e) Cada 2 horas entre las 06:00 y las 22:00"
echo ""
echo "--- Respuestas ---"
echo "a) 15 3 * * *"
echo "b) 0 8 * * 1,3"
echo "c) */15 * * * 1-5"
echo "d) 59 23 31 12 *"
echo "e) 0 6-22/2 * * *"
'
```

### Paso 2.4: Verificar cron

```bash
docker compose exec debian-dev bash -c '
echo "=== Verificar que cron esta activo ==="
echo ""
echo "--- Daemon ---"
echo "Debian: cron"
echo "RHEL:   crond"
echo ""
if systemctl is-active cron 2>/dev/null | grep -q active; then
    echo "cron: ACTIVO"
elif systemctl is-active crond 2>/dev/null | grep -q active; then
    echo "crond: ACTIVO"
else
    echo "cron/crond: NO ACTIVO (normal en contenedores)"
fi
echo ""
echo "--- Logs de cron ---"
echo "Debian: grep CRON /var/log/syslog"
echo "RHEL:   grep cron /var/log/cron"
echo "Ambos:  journalctl -u cron (o crond)"
echo ""
echo "--- Control de acceso ---"
echo "/etc/cron.allow:"
cat /etc/cron.allow 2>/dev/null || echo "  (no existe)"
echo "/etc/cron.deny:"
cat /etc/cron.deny 2>/dev/null || echo "  (no existe)"
echo ""
echo "Si solo /etc/cron.deny existe (vacio): todos pueden usar cron"
echo "Si /etc/cron.allow existe: solo los listados pueden usar cron"
'
```

---

## Parte 3 — Salida y errores comunes

### Objetivo

Entender la salida de cron y los errores mas frecuentes.

### Paso 3.1: Redireccion de salida

```bash
docker compose exec debian-dev bash -c '
echo "=== Redireccion de salida ==="
echo ""
echo "Por defecto, cron envia stdout+stderr por mail al usuario."
echo "Si no hay MTA configurado, la salida se pierde."
echo ""
echo "--- Redirigir a log ---"
echo "0 2 * * * /path/backup.sh >> /var/log/backup.log 2>&1"
echo "  >> = append (no sobreescribir)"
echo "  2>&1 = stderr al mismo archivo que stdout"
echo ""
echo "--- Descartar toda la salida ---"
echo "0 2 * * * /path/backup.sh > /dev/null 2>&1"
echo "  CUIDADO: si falla, no queda rastro"
echo ""
echo "--- Solo capturar errores ---"
echo "0 2 * * * /path/backup.sh > /dev/null 2>> /var/log/errors.log"
echo ""
echo "--- Buena practica ---"
echo "0 2 * * * /path/backup.sh >> /var/log/backup.log 2>&1"
echo "  Siempre redirigir. El script debe incluir sus propios timestamps."
'
```

### Paso 3.2: El % tiene significado especial

```bash
docker compose exec debian-dev bash -c '
echo "=== Error: el % en crontab ==="
echo ""
echo "En crontab, el caracter % se interpreta como nueva linea."
echo "Lo que sigue al primer % se pasa como stdin al comando."
echo ""
echo "--- INCORRECTO ---"
echo "0 2 * * * echo \"Backup \$(date +%Y-%m-%d)\" >> /var/log/cron.log"
echo "  %Y, %m, %d se interpretan como nuevas lineas → FALLA"
echo ""
echo "--- CORRECTO: escapar con backslash ---"
echo "0 2 * * * echo \"Backup \$(date +\\%Y-\\%m-\\%d)\" >> /var/log/cron.log"
echo ""
echo "--- CORRECTO: mover la logica a un script ---"
echo "0 2 * * * /usr/local/bin/backup.sh"
echo "  Dentro del script, usar % libremente"
echo ""
echo "--- Demostracion ---"
echo "Linea crontab con % sin escapar:"
echo "  0 * * * * echo hola%mundo%fin"
echo "Se interpreta como:"
echo "  echo hola"
echo "  con stdin: mundo\\nfin"
'
```

### Paso 3.3: PATH reducido y rutas absolutas

```bash
docker compose exec debian-dev bash -c '
echo "=== Error: PATH reducido ==="
echo ""
echo "cron NO carga el profile del usuario."
echo "El PATH es minimo:"
echo "  Debian: PATH=/usr/bin:/bin"
echo "  RHEL:   PATH=/usr/bin:/bin:/usr/sbin:/sbin"
echo ""
echo "--- Problema ---"
echo "0 * * * * my-script.sh"
echo "  → /bin/sh: my-script.sh: not found"
echo ""
echo "--- Soluciones ---"
echo "1. Rutas absolutas:"
echo "   0 * * * * /usr/local/bin/my-script.sh"
echo ""
echo "2. Definir PATH al inicio del crontab:"
echo "   PATH=/usr/local/bin:/usr/bin:/bin"
echo "   0 * * * * my-script.sh"
echo ""
echo "3. Rutas absolutas DENTRO del script:"
echo "   Cambiar \"node\" por \"/usr/local/bin/node\""
echo ""
echo "--- Verificar PATH de cron ---"
echo "Agregar temporalmente al crontab:"
echo "  * * * * * echo PATH=\$PATH > /tmp/cron-path.txt"
'
```

### Paso 3.4: Permisos y otros errores

```bash
docker compose exec debian-dev bash -c '
echo "=== Otros errores comunes ==="
echo ""
echo "--- Permisos ---"
echo "El script debe ser ejecutable:"
echo "  chmod +x /usr/local/bin/backup.sh"
echo ""
echo "--- Script sin shebang ---"
echo "Si el script no tiene #!/bin/bash, se ejecuta con /bin/sh"
echo "Los bashisms fallan:"
echo "  [[ ]]           → Syntax error"
echo "  arrays           → Syntax error"
echo "  <()              → Syntax error"
echo ""
echo "--- Diagnostico rapido ---"
echo "1. Verificar que cron esta activo:"
echo "   systemctl is-active cron"
echo ""
echo "2. Agregar tarea de prueba:"
echo "   * * * * * echo test > /tmp/cron-test.txt 2>&1"
echo ""
echo "3. Verificar logs:"
echo "   grep CRON /var/log/syslog"
echo "   journalctl -u cron --since \"5 min ago\""
echo ""
echo "4. Reproducir el entorno de cron:"
echo "   env -i HOME=\$HOME PATH=/usr/bin:/bin SHELL=/bin/sh \\"
echo "       /bin/sh -c \"/path/to/script.sh\""
'
```

---

## Limpieza final

No hay recursos que limpiar.

---

## Conceptos reforzados

1. Cada linea de crontab tiene 5 campos (minuto, hora, dia del mes,
   mes, dia de la semana) seguidos del comando. Los campos aceptan
   `*`, coma, guion y `/` para definir patrones.

2. Cuando se especifican dia del mes Y dia de la semana, cron los
   combina con OR (no AND). Esto es contraintuitivo y fuente comun
   de errores.

3. Las cadenas especiales (@daily, @hourly, @reboot, etc.) son
   alias para expresiones comunes. @reboot ejecuta una vez al
   iniciar el daemon cron.

4. `crontab -e` edita, `-l` lista, `-r` elimina. Nunca editar
   los archivos de spool directamente. `-u usuario` gestiona el
   crontab de otro usuario (requiere root).

5. En crontab, el `%` se interpreta como nueva linea. Hay que
   escaparlo con `\%` o mover la logica a un script.

6. El PATH de cron es minimo (/usr/bin:/bin). Siempre usar rutas
   absolutas o definir PATH al inicio del crontab.
