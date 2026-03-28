# T01 — Sintaxis crontab

> **Objetivo:** Dominar la sintaxis de crontab: los 5 campos de tiempo, caracteres especiales, la regla OR entre día del mes y día de la semana, cadenas especiales, gestión de crontabs de usuario, redirección de salida, y errores comunes.

## Sin erratas

El `README.md` base no presenta errores técnicos.

---

## Qué es cron

cron es el planificador de tareas clásico de Unix. Un daemon (`cron` en Debian, `crond` en RHEL) lee tablas de tareas (crontabs) y ejecuta comandos en los momentos especificados:

```bash
# El daemon cron:
systemctl is-active cron        # Debian/Ubuntu
systemctl is-active crond       # RHEL/Fedora

# cron lee las tareas de:
# 1. /var/spool/cron/crontabs/<usuario>  — crontabs de usuario (Debian)
#    /var/spool/cron/<usuario>           — crontabs de usuario (RHEL)
# 2. /etc/crontab                        — crontab del sistema
# 3. /etc/cron.d/*                       — archivos drop-in del sistema
```

---

## Los 5 campos

Cada línea de un crontab define **cuándo** y **qué** ejecutar:

```
┌───────────── minuto       (0–59)
│ ┌─────────── hora         (0–23)
│ │ ┌───────── día del mes  (1–31)
│ │ │ ┌─────── mes          (1–12)
│ │ │ │ ┌───── día de la semana (0–7, donde 0 y 7 = domingo)
│ │ │ │ │
* * * * *  comando
```

```bash
# Ejemplos básicos:
30 2 * * *   /usr/local/bin/backup.sh
# Todos los días a las 02:30

0 9 1 * *    /usr/local/bin/report.sh
# El día 1 de cada mes a las 09:00

0 0 * * 0    /usr/local/bin/weekly-cleanup.sh
# Todos los domingos a medianoche
```

### Valores válidos por campo

| Campo | Rango | Notas |
|---|---|---|
| Minuto | 0–59 | |
| Hora | 0–23 | 0 = medianoche |
| Día del mes | 1–31 | Días inexistentes se ignoran silenciosamente (ej: 31 feb) |
| Mes | 1–12 | También acepta nombres: jan, feb, mar... (3 letras, inglés) |
| Día de la semana | 0–7 | 0 y 7 = domingo. También acepta nombres: sun, mon, tue... |

```bash
# Usando nombres de mes y día:
0 6 * jan,jul mon   /usr/local/bin/task.sh
# Lunes de enero y julio a las 06:00

# Los nombres no son case-sensitive en la mayoría de implementaciones
```

---

## Caracteres especiales

### Asterisco `*` — Cualquier valor

```bash
* * * * *    /usr/local/bin/every-minute.sh
# Cada minuto de cada hora de cada día

0 * * * *    /usr/local/bin/every-hour.sh
# Al minuto 0 de cada hora (cada hora en punto)
```

### Coma `,` — Lista de valores

```bash
0 8,12,18 * * *   /usr/local/bin/check.sh
# A las 08:00, 12:00 y 18:00

0 0 1,15 * *      /usr/local/bin/twice-monthly.sh
# Días 1 y 15 de cada mes a medianoche
```

### Guion `-` — Rango

```bash
0 9-17 * * 1-5    /usr/local/bin/business-hours.sh
# De lunes a viernes, de 09:00 a 17:00 (cada hora en punto)

30 6 1-7 * *      /usr/local/bin/first-week.sh
# Los primeros 7 días de cada mes a las 06:30
```

### Barra `/` — Intervalo (step)

```bash
*/5 * * * *       /usr/local/bin/every-5-min.sh
# Cada 5 minutos (0, 5, 10, 15, 20, 25, 30, 35, 40, 45, 50, 55)

0 */2 * * *       /usr/local/bin/every-2-hours.sh
# Cada 2 horas en punto (0, 2, 4, 6, 8, 10, 12, 14, 16, 18, 20, 22)

*/10 9-17 * * 1-5 /usr/local/bin/work-check.sh
# Cada 10 minutos durante horario laboral (lun-vie, 9-17)
```

### Combinaciones

```bash
# Los caracteres se pueden combinar:
0 8,12,18 1-15 */2 *   /usr/local/bin/complex.sh
# A las 08:00, 12:00 y 18:00
# Los primeros 15 días
# De meses pares (feb, abr, jun...)

# Rango con step:
0-30/10 * * * *   /usr/local/bin/task.sh
# Minutos 0, 10, 20, 30 de cada hora
```

---

## Día del mes Y día de la semana — OR, no AND

Un detalle crítico y fuente frecuente de errores:

```bash
# Cuando se especifican AMBOS (día del mes y día de la semana),
# cron los combina con OR:
0 9 15 * 5   /usr/local/bin/task.sh
# Ejecuta: el día 15 de cada mes O cualquier viernes
# NO: el día 15 solo si cae en viernes

# Esto es contraintuitivo. Los demás campos se combinan con AND:
0 9 * 6 1    /usr/local/bin/task.sh
# Todos los lunes de junio a las 09:00 (mes AND día de semana)
# Funciona con AND porque el día del mes es * (no especificado)

# Para "el primer viernes de cada mes" necesitas lógica en el comando:
0 9 1-7 * 5  /usr/local/bin/first-friday.sh
# INCORRECTO: ejecuta días 1-7 OR viernes (no solo el primer viernes)

# Solución con test en el comando:
0 9 1-7 * *  [ "$(date +\%u)" = 5 ] && /usr/local/bin/first-friday.sh
# Solo los primeros 7 días, y verifica que sea viernes (%u: 1=lun, 5=vie)
```

**Regla:** la combinación OR se activa **solo** cuando ambos campos (día del mes y día de la semana) tienen valores concretos (no `*`). Si uno de los dos es `*`, el otro actúa como filtro normal (AND con el resto).

---

## Cadenas especiales

Muchas implementaciones de cron soportan alias para expresiones comunes:

| Cadena | Equivalente | Significado |
|--------|-------------|-------------|
| `@reboot` | (especial) | Al arrancar el sistema (cuando crond inicia) |
| `@yearly` | `0 0 1 1 *` | 1 de enero a medianoche |
| `@annually` | `0 0 1 1 *` | Alias de `@yearly` |
| `@monthly` | `0 0 1 * *` | Día 1 de cada mes a medianoche |
| `@weekly` | `0 0 * * 0` | Domingo a medianoche |
| `@daily` | `0 0 * * *` | Cada día a medianoche |
| `@midnight` | `0 0 * * *` | Alias de `@daily` |
| `@hourly` | `0 * * * *` | Cada hora en punto |

```bash
# @reboot es especialmente útil:
@reboot  sleep 30 && /usr/local/bin/startup-check.sh
# Se ejecuta una vez al iniciar crond
# El sleep da tiempo a que los servicios estén listos
```

---

## Gestionar crontabs de usuario

```bash
# Editar el crontab del usuario actual:
crontab -e
# Abre el crontab en $EDITOR (o vi por defecto)
# Al guardar, cron valida la sintaxis y lo instala

# Ver el crontab del usuario actual:
crontab -l

# Eliminar el crontab del usuario actual:
crontab -r
# CUIDADO: elimina TODO el crontab sin confirmación

# Eliminar con confirmación (si está disponible):
crontab -ri

# Como root, gestionar el crontab de otro usuario:
sudo crontab -u www-data -l
sudo crontab -u www-data -e
```

### Dónde se almacenan

```bash
# Debian/Ubuntu:
ls /var/spool/cron/crontabs/
# Un archivo por usuario que tenga crontab

# RHEL/Fedora:
ls /var/spool/cron/
# Un archivo por usuario

# NUNCA editar estos archivos directamente
# Usar siempre crontab -e (valida la sintaxis)
```

### Control de acceso

```bash
# /etc/cron.allow — si existe, SOLO estos usuarios pueden usar cron
# /etc/cron.deny  — si existe (y no hay cron.allow), estos usuarios NO pueden

# Comportamiento cuando ninguno existe:
# Debian: solo root puede usar cron
# RHEL:   todos los usuarios pueden usar cron

# En la práctica, Debian incluye un /etc/cron.deny vacío por defecto,
# lo que permite a todos los usuarios usar cron
```

---

## Salida y redirección

Por defecto, cron envía la salida (stdout y stderr) por **correo** al usuario propietario del crontab:

```bash
# Si el comando produce salida, cron intenta enviar un mail
# Si no hay MTA configurado, la salida se pierde silenciosamente

# Redirigir stdout y stderr a un log:
0 2 * * * /usr/local/bin/backup.sh >> /var/log/backup.log 2>&1
# >> = append (no sobreescribir)
# 2>&1 = redirigir stderr a stdout (mismo archivo)

# Descartar toda la salida:
0 2 * * * /usr/local/bin/backup.sh > /dev/null 2>&1
# Cuidado: si el script falla, no queda rastro

# Solo capturar errores:
0 2 * * * /usr/local/bin/backup.sh > /dev/null 2>> /var/log/backup-errors.log

# Buena práctica: siempre redirigir la salida
# Mejor que el script incluya sus propios timestamps
```

---

## Errores comunes

### El `%` tiene significado especial

```bash
# En crontab, el carácter % se interpreta como nueva línea
# y lo que sigue al primer % se pasa como stdin al comando

# INCORRECTO:
0 2 * * * echo "Backup $(date +%Y-%m-%d)" >> /var/log/cron.log
# Falla: %Y, %m, %d se interpretan como nuevas líneas

# CORRECTO — escapar con backslash:
0 2 * * * echo "Backup $(date +\%Y-\%m-\%d)" >> /var/log/cron.log

# CORRECTO — mover la lógica a un script:
0 2 * * * /usr/local/bin/backup.sh
# Dentro del script, usar % libremente
```

### Rutas absolutas y PATH mínimo

```bash
# cron NO carga el profile del usuario
# El PATH es mínimo:
#   Debian: PATH=/usr/bin:/bin
#   RHEL:   PATH=/usr/bin:/bin:/usr/sbin:/sbin

# INCORRECTO:
0 * * * * my-script.sh

# CORRECTO:
0 * * * * /usr/local/bin/my-script.sh

# O definir PATH al inicio del crontab:
PATH=/usr/local/bin:/usr/bin:/bin
0 * * * * my-script.sh
```

### Permisos y shebang

```bash
# El script debe ser ejecutable:
chmod +x /usr/local/bin/backup.sh

# Si el script no tiene shebang (#!/bin/bash), se ejecuta con /bin/sh
# Bashisms como [[ ]], arrays, <() fallarán

# Reproducir el entorno de cron para depuración:
env -i HOME=$HOME PATH=/usr/bin:/bin SHELL=/bin/sh \
    /bin/sh -c "/path/to/script.sh"
```

---

## Verificar que cron funciona

```bash
# 1. Verificar que el daemon está activo:
systemctl status cron     # Debian
systemctl status crond    # RHEL

# 2. Probar con una tarea de 1 minuto:
crontab -e
# Agregar:
# * * * * * echo "cron works $(date)" >> /tmp/cron-test.log
# Esperar 1-2 minutos y verificar:
cat /tmp/cron-test.log

# 3. Revisar los logs de cron:
grep CRON /var/log/syslog        # Debian
grep cron /var/log/cron          # RHEL
journalctl -u cron --since "5 min ago"   # Debian con journal
journalctl -u crond --since "5 min ago"  # RHEL con journal
```

---

## Lab — Sintaxis crontab

### Parte 1 — Los 5 campos y caracteres especiales

#### Paso 1.1: Estructura de una línea crontab

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

#### Paso 1.2: Caracteres especiales

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

#### Paso 1.3: La regla OR

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

### Parte 2 — Cadenas especiales y gestión

#### Paso 2.1: Cadenas especiales

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

#### Paso 2.2: Gestión de crontabs de usuario

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

#### Paso 2.3: Verificar cron

```bash
docker compose exec debian-dev bash -c '
echo "=== Verificar que cron esta activo ==="
echo ""
if systemctl is-active cron 2>/dev/null | grep -q active; then
    echo "cron: ACTIVO"
elif systemctl is-active crond 2>/dev/null | grep -q active; then
    echo "crond: ACTIVO"
else
    echo "cron/crond: NO ACTIVO (normal en contenedores)"
fi
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

### Parte 3 — Salida y errores comunes

#### Paso 3.1: Redirección de salida

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
echo "Siempre redirigir. El script debe incluir sus propios timestamps."
'
```

#### Paso 3.2: El `%` tiene significado especial

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
'
```

#### Paso 3.3: PATH reducido y rutas absolutas

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
echo "--- Reproducir el entorno de cron para depuracion ---"
echo "env -i HOME=\$HOME PATH=/usr/bin:/bin SHELL=/bin/sh \\"
echo "    /bin/sh -c \"/path/to/script.sh\""
'
```

---

## Ejercicios

### Ejercicio 1 — Leer expresiones cron

¿Cuándo se ejecuta cada una de estas expresiones?

```
a) 30 4 * * *
b) 0 */6 * * 1-5
c) 15,45 9-17 * * *
d) 0 0 1 1,4,7,10 *
e) */5 * * * 0
```

<details><summary>Predicción</summary>

- **a)** Todos los días a las 04:30.
- **b)** De lunes a viernes a las 00:00, 06:00, 12:00 y 18:00.
- **c)** Todos los días, a los minutos :15 y :45, de 09:00 a 17:00 (es decir, 09:15, 09:45, 10:15, 10:45... 17:15, 17:45).
- **d)** El día 1 de enero, abril, julio y octubre a medianoche (trimestralmente).
- **e)** Cada 5 minutos los domingos.

</details>

### Ejercicio 2 — Escribir expresiones cron

Escribe la expresión cron para cada caso:

```
a) Cada día a las 03:15
b) Cada lunes y miércoles a las 08:00
c) Cada 15 minutos de lunes a viernes
d) El 31 de diciembre a las 23:59
e) Cada 2 horas entre las 06:00 y las 22:00
```

<details><summary>Predicción</summary>

- **a)** `15 3 * * *`
- **b)** `0 8 * * 1,3`
- **c)** `*/15 * * * 1-5`
- **d)** `59 23 31 12 *`
- **e)** `0 6-22/2 * * *` — Esto se ejecuta a las 06:00, 08:00, 10:00, 12:00, 14:00, 16:00, 18:00, 20:00 y 22:00.

</details>

### Ejercicio 3 — La trampa del OR

¿Qué hace exactamente esta expresión?

```
0 9 15 * 5   /usr/local/bin/task.sh
```

<details><summary>Predicción</summary>

Se ejecuta el día 15 de cada mes **O** cualquier viernes, a las 09:00. **No** se ejecuta "el día 15 solo si cae en viernes".

Esto ocurre porque cuando se especifican tanto día del mes como día de la semana con valores concretos (no `*`), cron los combina con OR.

Para ejecutar solo el día 15 si es viernes:
```bash
0 9 15 * *  [ "$(date +\%u)" = 5 ] && /usr/local/bin/task.sh
```

</details>

### Ejercicio 4 — Verificar el daemon cron

¿Está cron activo en tu sistema? ¿Hay crontabs definidos?

```bash
# 1. Estado del daemon:
systemctl is-active cron 2>/dev/null || systemctl is-active crond 2>/dev/null

# 2. Crontab del usuario actual:
crontab -l 2>/dev/null || echo "No crontab"

# 3. Crontabs de otros usuarios:
sudo ls /var/spool/cron/crontabs/ 2>/dev/null || \
    sudo ls /var/spool/cron/ 2>/dev/null

# 4. Crontab del sistema:
cat /etc/crontab
```

<details><summary>Predicción</summary>

- `cron` (Debian) o `crond` (RHEL) debería mostrar `active`. En contenedores Docker puede no estar corriendo.
- `crontab -l` probablemente mostrará "no crontab for user" en un sistema limpio.
- `/etc/crontab` mostrará la estructura del crontab del sistema, que incluye un campo extra para el usuario que ejecuta el comando (a diferencia de los crontabs de usuario).
- El directorio de spool tendrá un archivo por cada usuario que haya creado un crontab con `crontab -e`.

</details>

### Ejercicio 5 — Escapar el `%`

¿Por qué falla esta línea de crontab y cómo se corrige?

```
0 2 * * * echo "Backup $(date +%Y-%m-%d)" >> /var/log/backup.log
```

<details><summary>Predicción</summary>

**Falla porque** en crontab el `%` se interpreta como salto de línea. La línea se rompe así:
- Comando: `echo "Backup $(date +`
- stdin: `Y-%m-%d)" >> /var/log/backup.log`

**Correcciones:**

1. Escapar cada `%` con `\`:
```
0 2 * * * echo "Backup $(date +\%Y-\%m-\%d)" >> /var/log/backup.log
```

2. Mover la lógica a un script donde `%` funciona normalmente:
```
0 2 * * * /usr/local/bin/backup.sh
```

La opción 2 es preferible porque es más limpia y no requiere recordar escapar caracteres.

</details>

### Ejercicio 6 — PATH mínimo

¿Qué PATH tiene cron? Verifícalo.

```bash
# Agregar temporalmente al crontab:
crontab -e
# Añadir:
# * * * * * echo "PATH=$PATH" > /tmp/cron-path.txt

# Esperar 1 minuto y verificar:
cat /tmp/cron-path.txt

# Comparar con tu PATH de sesión normal:
echo "PATH=$PATH"

# Limpiar:
crontab -r   # o quitar la línea con crontab -e
rm /tmp/cron-path.txt
```

<details><summary>Predicción</summary>

- El PATH de cron será muy reducido: `/usr/bin:/bin` (Debian) o `/usr/bin:/bin:/usr/sbin:/sbin` (RHEL).
- Tu PATH de sesión normal incluirá muchos más directorios como `/usr/local/bin`, `/home/user/.local/bin`, etc.
- Por eso los scripts en cron deben usar rutas absolutas o definir PATH al inicio del crontab.

</details>

### Ejercicio 7 — Cadenas especiales

¿A qué expresión cron equivale cada cadena?

```
a) @daily
b) @weekly
c) @monthly
d) @hourly
e) @yearly
```

<details><summary>Predicción</summary>

- **a)** `@daily` = `0 0 * * *` — Cada día a medianoche
- **b)** `@weekly` = `0 0 * * 0` — Cada domingo a medianoche
- **c)** `@monthly` = `0 0 1 * *` — El día 1 de cada mes a medianoche
- **d)** `@hourly` = `0 * * * *` — Cada hora en punto (minuto 0)
- **e)** `@yearly` = `0 0 1 1 *` — El 1 de enero a medianoche

`@reboot` no tiene equivalente numérico: se ejecuta una vez cuando el daemon cron arranca.

</details>

### Ejercicio 8 — Logs de cron

¿Dónde registra cron la ejecución de tareas?

```bash
# Debian:
grep CRON /var/log/syslog | tail -5

# RHEL:
grep cron /var/log/cron | tail -5

# Ambos (con journal):
journalctl -u cron --since "1 hour ago" --no-pager 2>/dev/null || \
journalctl -u crond --since "1 hour ago" --no-pager 2>/dev/null
```

<details><summary>Predicción</summary>

- En **Debian**, los logs de cron van a `/var/log/syslog` con el identificador "CRON". Cada ejecución registra el usuario y el comando.
- En **RHEL**, los logs van a `/var/log/cron` como archivo dedicado.
- Con journalctl se pueden filtrar por la unidad `cron` o `crond`.
- Cada línea de log mostrará: timestamp, hostname, "CRON[PID]", "(usuario)", "CMD (comando)".
- Si una tarea falla, cron **no** registra el error en estos logs — solo registra que intentó ejecutar el comando. Los errores del comando van a la salida del propio cron job (mail o redirección).

</details>

### Ejercicio 9 — Control de acceso

¿Quién puede usar cron en tu sistema?

```bash
# Verificar archivos de control:
echo "=== /etc/cron.allow ==="
cat /etc/cron.allow 2>/dev/null || echo "(no existe)"

echo "=== /etc/cron.deny ==="
cat /etc/cron.deny 2>/dev/null || echo "(no existe)"
```

<details><summary>Predicción</summary>

- Si `/etc/cron.allow` existe → **solo** los usuarios listados pueden usar cron.
- Si `/etc/cron.allow` no existe pero `/etc/cron.deny` existe → todos pueden usar cron **excepto** los listados en deny.
- Si `/etc/cron.deny` existe y está vacío → todos pueden usar cron (esta es la configuración default de Debian).
- Si ninguno existe → solo root (Debian) o todos (RHEL), según la implementación.
- La regla: `cron.allow` tiene prioridad sobre `cron.deny`. Si `cron.allow` existe, `cron.deny` se ignora.

</details>

### Ejercicio 10 — Expresión compleja

Escribe una expresión cron para: "cada 30 minutos durante horario laboral (08:00-18:00), de lunes a viernes, excepto en agosto".

<details><summary>Predicción</summary>

No se puede hacer completamente con cron. La parte del horario y días de la semana es sencilla:

```
*/30 8-17 * * 1-5   /usr/local/bin/task.sh
```

Esto ejecuta a los minutos :00 y :30 de 08:00 a 17:30, de lunes a viernes (también ejecutaría a las 18:00 si usáramos 8-18, por eso usamos 8-17 para que el último sea 17:30).

Pero cron **no tiene operador de exclusión** para meses. Para excluir agosto hay dos opciones:

1. Listar todos los meses excepto agosto:
```
*/30 8-17 * 1-7,9-12 1-5   /usr/local/bin/task.sh
```

2. Agregar condición en el comando:
```
*/30 8-17 * * 1-5  [ "$(date +\%m)" != "08" ] && /usr/local/bin/task.sh
```

La opción 1 es más limpia y eficiente (cron ni siquiera evalúa en agosto).

</details>
