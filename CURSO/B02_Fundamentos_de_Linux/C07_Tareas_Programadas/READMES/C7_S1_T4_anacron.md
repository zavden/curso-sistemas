# T04 — anacron

> **Objetivo:** Entender anacron: el problema que resuelve (tareas perdidas por apagado), el formato de `/etc/anacrontab`, timestamps, variables especiales (`START_HOURS_RANGE`, `RANDOM_DELAY`), la relación con cron, limitaciones, y comparación con systemd timers.

## Erratas corregidas respecto a `README.md`

| # | Línea | Error | Corrección |
|---|-------|-------|------------|
| 1 | 105 | `@monthly` descrito como "último día del mes anterior como referencia" | `@monthly` verifica si el mes actual difiere del mes almacenado en el timestamp. Si es un mes distinto, ejecuta la tarea. No tiene relación con el último día del mes anterior. |

---

## El problema que resuelve anacron

cron asume que el sistema está **siempre encendido**. Si una tarea diaria está programada para las 03:00 y el equipo está apagado a esa hora, la tarea no se ejecuta y no se recupera:

```
cron:
  03:00 → ejecutar backup diario
  Si el equipo estaba apagado a las 03:00 → la tarea se pierde

anacron:
  "ejecutar backup diario"
  Si no se ejecutó hoy → ejecutarlo ahora (con un delay aleatorio)
  Registrar la fecha de última ejecución para no repetir
```

Perfecto para:
- Laptops que se suspenden/apagan regularmente
- Estaciones de trabajo con horarios variables
- Máquinas virtuales que no están siempre activas

---

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

---

## /etc/anacrontab

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

| Campo | Descripción | Ejemplo |
|-------|-------------|---------|
| Período | En días. `@monthly` = una vez por mes calendario | `1`, `7`, `30`, `@monthly` |
| Delay | Minutos de espera antes de ejecutar. Evita picos de I/O | `5`, `10`, `15` |
| Identificador | Nombre único. Se usa como archivo en `/var/spool/anacron/` | `cron.daily` |
| Comando | El comando a ejecutar | `run-parts --report /etc/cron.daily` |

### Variables de entorno

```bash
# anacron acepta las mismas variables que crontab, más dos específicas:

# START_HOURS_RANGE — horas válidas para ejecutar tareas:
START_HOURS_RANGE=3-22
# Solo ejecutar entre las 03:00 y las 22:00
# Si anacron se invoca fuera de este rango, espera

# RANDOM_DELAY — delay aleatorio adicional (minutos):
RANDOM_DELAY=45
# Agrega entre 0 y 45 minutos al delay del campo 2
# Útil para que múltiples máquinas no hagan lo mismo a la vez

# Delay total = delay del campo 2 + random(0, RANDOM_DELAY)
```

---

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

# cron.hourly siempre lo ejecuta cron (anacron no maneja intervalos < 1 día)
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

Se complementan: cron para precisión, anacron para garantía de ejecución.

---

## anacron en Debian vs RHEL

### Debian/Ubuntu

```bash
# anacron es un paquete separado (instalado por defecto):
dpkg -l anacron

# Se invoca mediante systemd timer o /etc/cron.d/anacron
systemctl status anacron.timer

# anacron ejecuta run-parts sobre /etc/cron.{daily,weekly,monthly}
# cron ejecuta /etc/cron.hourly (anacron no maneja horas)
```

### RHEL/Fedora

```bash
# anacron está integrado en cronie (cronie-anacron):
rpm -q cronie-anacron

# crond maneja anacron internamente:
# No hay daemon anacron separado
# crond detecta tareas anacron y las ejecuta

# La configuración sigue siendo /etc/anacrontab
# Los timestamps siguen en /var/spool/anacron/
```

---

## Opciones de línea de comandos

```bash
sudo anacron -f     # force: ejecutar todas aunque no estén pendientes
sudo anacron -fn    # force + now: sin delays
sudo anacron -d     # debug: no fork al background, mostrar mensajes
sudo anacron -T     # testear sintaxis de /etc/anacrontab
sudo anacron -t /path/to/alt-anacrontab    # anacrontab alternativo
sudo anacron -S /path/to/spool             # directorio de spool alternativo
```

---

## Agregar tareas a anacron

### Opción 1 — Script en /etc/cron.daily/ (más común)

```bash
# Crear el script:
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

### Opción 2 — Línea en /etc/anacrontab (tareas custom)

```bash
# Agregar al final de /etc/anacrontab:
# Verificación de integridad cada 3 días
3    20    integrity-check    /usr/local/bin/integrity-check.sh

# Esto crea una tarea que:
# - Se ejecuta cada 3 días
# - Con un delay de 20 minutos (+ RANDOM_DELAY si está definido)
# - Se rastrea con el identificador "integrity-check"
# - El timestamp se guarda en /var/spool/anacron/integrity-check
```

---

## Limitaciones de anacron

```bash
# 1. Solo root puede usar anacron
#    No hay "anacrontab de usuario"
#    Alternativa: systemd user timers

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

---

## systemd timers como alternativa moderna

| Aspecto | anacron | systemd timer |
|---------|---------|---------------|
| Tareas perdidas | Sí (siempre) | Sí (`Persistent=true`) |
| Precisión | Solo días | Microsegundos |
| Delay aleatorio | `RANDOM_DELAY` | `RandomizedDelaySec` |
| Logs | syslog | journal (integrado) |
| Usuarios | Solo root | User units también |
| Complejidad | Baja (1 archivo) | Media (2 archivos: `.timer` + `.service`) |

anacron sigue siendo relevante porque:
1. Es más simple para casos básicos
2. Viene preconfigurado para `/etc/cron.{daily,weekly,monthly}`
3. No requiere crear dos archivos
4. Ecosistema enorme de paquetes que instalan scripts en `cron.daily/`

---

## Lab — anacron

### Parte 1 — Funcionamiento de anacron

#### Paso 1.1: El problema

```bash
docker compose exec debian-dev bash -c '
echo "=== El problema que resuelve anacron ==="
echo ""
echo "cron asume que el sistema esta SIEMPRE encendido."
echo ""
echo "  cron:"
echo "  03:00 → ejecutar backup diario"
echo "  Si el equipo estaba apagado a las 03:00 → tarea PERDIDA"
echo ""
echo "  anacron:"
echo "  \"ejecutar backup diario\""
echo "  Si no se ejecuto hoy → ejecutarlo ahora (con delay)"
echo "  Registrar la fecha para no repetir"
echo ""
echo "--- Casos de uso ---"
echo "  Laptops que se suspenden/apagan"
echo "  Estaciones de trabajo con horarios variables"
echo "  VMs que no estan siempre activas"
echo ""
echo "--- Verificar si anacron esta instalado ---"
which anacron 2>/dev/null && echo "anacron: INSTALADO" || \
    echo "anacron: NO INSTALADO"
'
```

#### Paso 1.2: /etc/anacrontab

```bash
docker compose exec debian-dev bash -c '
echo "=== /etc/anacrontab ==="
echo ""
echo "--- Formato ---"
echo "periodo(dias)  delay(min)  identificador  comando"
echo ""
if [[ -f /etc/anacrontab ]]; then
    echo "--- Contenido actual ---"
    cat /etc/anacrontab
else
    echo "--- Contenido tipico ---"
    echo "SHELL=/bin/sh"
    echo "PATH=/usr/local/sbin:/usr/local/bin:/sbin:/bin:/usr/sbin:/usr/bin"
    echo "HOME=/root"
    echo "LOGNAME=root"
    echo ""
    echo "1    5    cron.daily    run-parts --report /etc/cron.daily"
    echo "7   10    cron.weekly   run-parts --report /etc/cron.weekly"
    echo "@monthly 15  cron.monthly  run-parts --report /etc/cron.monthly"
fi
'
```

#### Paso 1.3: Timestamps de última ejecución

```bash
docker compose exec debian-dev bash -c '
echo "=== Timestamps de anacron ==="
echo ""
echo "anacron registra cuando ejecuto cada tarea:"
echo "Directorio: /var/spool/anacron/"
echo ""
if [[ -d /var/spool/anacron ]]; then
    echo "--- Contenido ---"
    for f in /var/spool/anacron/*; do
        if [[ -f "$f" ]]; then
            echo "  $(basename "$f"): $(cat "$f")"
        fi
    done
    [[ ! -f /var/spool/anacron/cron.daily ]] && \
        echo "  (sin timestamps)"
else
    echo "/var/spool/anacron/ no existe"
fi
echo ""
echo "--- Formato ---"
echo "Cada archivo contiene una fecha: YYYYMMDD"
echo ""
echo "--- Flujo ---"
echo "1. anacron lee el timestamp de la ultima ejecucion"
echo "2. Si han pasado mas dias que el periodo → ejecutar"
echo "3. Esperar el delay especificado"
echo "4. Ejecutar el comando"
echo "5. Actualizar el timestamp"
'
```

---

### Parte 2 — Configuración y relación con cron

#### Paso 2.1: Variables de anacron

```bash
docker compose exec debian-dev bash -c '
echo "=== Variables de anacrontab ==="
echo ""
echo "Acepta las mismas variables que crontab, mas:"
echo ""
echo "--- START_HOURS_RANGE ---"
echo "START_HOURS_RANGE=3-22"
echo "  Solo ejecutar entre las 03:00 y las 22:00"
echo "  Si anacron se invoca fuera del rango, espera"
echo ""
echo "--- RANDOM_DELAY ---"
echo "RANDOM_DELAY=45"
echo "  Agrega entre 0 y 45 minutos al delay del campo 2"
echo "  Evita que multiples maquinas hagan lo mismo a la vez"
echo ""
echo "--- Verificar configuracion actual ---"
if [[ -f /etc/anacrontab ]]; then
    grep -E "^(START_HOURS|RANDOM_DELAY|SHELL|PATH)" /etc/anacrontab 2>/dev/null || \
        echo "  (solo defaults)"
else
    echo "  (anacrontab no encontrado)"
fi
'
```

#### Paso 2.2: Cómo cron delega en anacron

```bash
docker compose exec debian-dev bash -c '
echo "=== Relacion cron/anacron ==="
echo ""
echo "En /etc/crontab tipico de Debian:"
echo ""
echo "  25 6 * * * root test -x /usr/sbin/anacron || run-parts /etc/cron.daily"
echo ""
echo "Logica:"
echo "  Si anacron existe → cron NO ejecuta run-parts"
echo "    (anacron se encarga cuando se invoque)"
echo "  Si anacron NO existe → cron ejecuta directamente"
echo ""
echo "  cron.hourly: siempre lo ejecuta cron"
echo "  (anacron no maneja intervalos < 1 dia)"
echo ""
echo "--- Quien invoca a anacron ---"
systemctl is-active anacron.timer 2>/dev/null && \
    echo "Via systemd timer: ACTIVO" || true
cat /etc/cron.d/anacron 2>/dev/null && \
    echo "(via cron job)" || true
[[ ! -x /usr/sbin/anacron ]] && \
    echo "anacron no instalado — cron ejecuta todo directamente"
'
```

#### Paso 2.3: Diferencias cron vs anacron

```bash
docker compose exec debian-dev bash -c '
echo "=== cron vs anacron ==="
echo ""
echo "| Aspecto            | cron              | anacron            |"
echo "|--------------------|-------------------|--------------------|"
echo "| Precision          | Minuto exacto     | Solo dias          |"
echo "| Equipo apagado     | Pierde la tarea   | Ejecuta al encender|"
echo "| Daemon             | Si (siempre)      | No (se invoca)     |"
echo "| Usuarios           | Cualquiera        | Solo root          |"
echo "| Minimo intervalo   | 1 minuto          | 1 dia              |"
echo "| Hora exacta        | Si                | No (delay)         |"
echo "| Uso principal      | Servidores 24/7   | Laptops, escritorio|"
echo ""
echo "Se complementan: cron para precision, anacron para garantia."
'
```

---

### Parte 3 — Limitaciones y alternativas

#### Paso 3.1: Limitaciones

```bash
docker compose exec debian-dev bash -c '
echo "=== Limitaciones de anacron ==="
echo ""
echo "1. Solo root puede usar anacron"
echo "   No hay anacrontab de usuario"
echo "   Alternativa: systemd user timers"
echo ""
echo "2. Precision minima de 1 dia"
echo "   No puedes: \"cada 4 horas con garantia\""
echo "   Alternativa: systemd timers con Persistent=true"
echo ""
echo "3. No soporta horas especificas"
echo "   \"Ejecutar a las 03:00\" → no es posible"
echo "   anacron dice: \"una vez al dia, cuando pueda\""
echo ""
echo "4. START_HOURS_RANGE puede bloquear"
echo "   Si el equipo se enciende fuera del rango,"
echo "   anacron no ejecuta hasta estar dentro"
'
```

#### Paso 3.2: systemd timers como alternativa

```bash
docker compose exec debian-dev bash -c '
echo "=== systemd timers vs anacron ==="
echo ""
echo "| Aspecto           | anacron          | systemd timer      |"
echo "|-------------------|------------------|--------------------|"
echo "| Tareas perdidas   | Si (siempre)     | Si (Persistent=)   |"
echo "| Precision         | Solo dias        | Microsegundos      |"
echo "| Delay aleatorio   | RANDOM_DELAY     | RandomizedDelaySec |"
echo "| Logs              | syslog           | journal (integrado)|"
echo "| Usuarios          | Solo root        | User units tambien |"
echo "| Complejidad       | Baja             | Media (2 archivos) |"
echo ""
echo "anacron sigue siendo relevante porque:"
echo "  1. Mas simple para casos basicos"
echo "  2. Preconfigurado para cron.daily/weekly/monthly"
echo "  3. No requiere crear .timer + .service"
echo "  4. Paquetes instalan scripts en cron.daily/ directamente"
'
```

#### Paso 3.3: Debian vs RHEL

```bash
docker compose exec debian-dev bash -c '
echo "=== anacron en Debian ==="
echo ""
if dpkg -l anacron 2>/dev/null | grep -q "^ii"; then
    echo "anacron: instalado (paquete separado)"
else
    echo "anacron: no instalado (paquete: anacron)"
fi
echo ""
echo "Invocacion: systemd timer o /etc/cron.d/anacron"
echo "anacron ejecuta run-parts sobre cron.daily/weekly/monthly"
echo "cron.hourly siempre lo ejecuta cron"
'
```

```bash
docker compose exec alma-dev bash -c '
echo "=== anacron en AlmaLinux ==="
echo ""
rpm -q cronie-anacron 2>/dev/null && \
    echo "cronie-anacron: instalado (integrado en crond)" || \
    echo "cronie-anacron: no instalado"
echo ""
echo "En RHEL, crond maneja anacron internamente."
echo "No hay daemon anacron separado."
echo ""
if [[ -f /etc/anacrontab ]]; then
    echo "--- /etc/anacrontab ---"
    cat /etc/anacrontab | grep -v "^#" | grep -v "^$"
fi
'
```

---

## Ejercicios

### Ejercicio 1 — Verificar anacron

¿Está anacron instalado? ¿Cuándo se ejecutaron las tareas por última vez?

```bash
# 1. ¿Instalado?
which anacron 2>/dev/null && echo "Instalado" || echo "No instalado"

# 2. Timestamps:
for f in /var/spool/anacron/*; do
    echo "$(basename "$f"): $(cat "$f" 2>/dev/null)"
done

# 3. ¿Quién invoca a anacron?
systemctl is-active anacron.timer 2>/dev/null && echo "Via systemd timer"
cat /etc/cron.d/anacron 2>/dev/null
```

<details><summary>Predicción</summary>

- En Debian, anacron estará instalado como paquete separado. Los timestamps en `/var/spool/anacron/` mostrarán fechas YYYYMMDD de la última ejecución de `cron.daily`, `cron.weekly` y `cron.monthly`.
- La invocación será via `anacron.timer` (systemd) o `/etc/cron.d/anacron` (cron job).
- En contenedores Docker, anacron puede no estar instalado o no tener timestamps (nunca se ha ejecutado).

</details>

### Ejercicio 2 — Leer /etc/anacrontab

¿Qué tareas gestiona anacron y con qué configuración?

```bash
cat /etc/anacrontab 2>/dev/null
```

<details><summary>Predicción</summary>

Verás variables de entorno (SHELL, PATH, HOME) seguidas de 3 líneas:
- `1  5  cron.daily  run-parts --report /etc/cron.daily` — diario, delay 5 min
- `7  10  cron.weekly  run-parts --report /etc/cron.weekly` — semanal, delay 10 min
- `@monthly  15  cron.monthly  run-parts --report /etc/cron.monthly` — mensual, delay 15 min

Los delays crecientes (5, 10, 15) evitan que todas las tareas se ejecuten simultáneamente. `@monthly` verifica si el mes actual difiere del mes almacenado en el timestamp.

</details>

### Ejercicio 3 — La delegación cron → anacron

¿Cómo sabe cron que no debe ejecutar `cron.daily` cuando anacron está instalado?

```bash
grep anacron /etc/crontab 2>/dev/null
```

<details><summary>Predicción</summary>

Verás líneas como:
```
25 6 * * * root test -x /usr/sbin/anacron || run-parts --report /etc/cron.daily
```

La lógica: `test -x /usr/sbin/anacron` verifica si anacron existe y es ejecutable. Si sí (true), el `||` **no** ejecuta `run-parts` (porque el OR ya tiene un lado verdadero). Si no (false), `run-parts` se ejecuta como fallback. Así cron delega en anacron sin duplicar la ejecución.

`cron.hourly` **no** tendrá esta condición — siempre lo ejecuta cron porque anacron no maneja intervalos menores a 1 día.

</details>

### Ejercicio 4 — START_HOURS_RANGE

¿Qué pasa si anacron se invoca fuera del rango horario?

```bash
# Verificar el rango actual:
grep START_HOURS_RANGE /etc/anacrontab 2>/dev/null || \
    echo "Default: 3-22"
```

<details><summary>Predicción</summary>

Si `START_HOURS_RANGE=3-22`, anacron solo ejecutará tareas entre las 03:00 y las 22:00. Si se invoca a las 02:00 (fuera del rango), esperará hasta las 03:00 para empezar. Si se invoca a las 23:00, no ejecutará nada.

Esto protege contra ejecución nocturna en equipos que se dejan encendidos, pero puede ser problemático si el equipo solo se enciende por la noche. En ese caso, ampliar el rango o usar `anacron -f` para forzar.

</details>

### Ejercicio 5 — RANDOM_DELAY

¿Por qué existe el delay aleatorio y cómo se calcula el delay total?

```bash
# Verificar:
grep RANDOM_DELAY /etc/anacrontab 2>/dev/null

# Si RANDOM_DELAY=45 y el campo delay es 5:
# Delay total = 5 + random(0, 45) = entre 5 y 50 minutos
```

<details><summary>Predicción</summary>

- `RANDOM_DELAY` existe para evitar que muchas máquinas ejecuten las mismas tareas al mismo tiempo (el "thundering herd problem"). En un entorno con 100 servidores, sin delay aleatorio todos harían `logrotate` y `apt update` al mismo instante.
- El delay total es: **delay del campo 2** + **random(0, RANDOM_DELAY)**.
- Si no está definido, el delay aleatorio es 0 y solo se usa el delay del campo.
- Esto significa que no puedes predecir la hora exacta de ejecución — solo que ocurrirá "en algún momento" dentro de un rango.

</details>

### Ejercicio 6 — Opciones de anacron

¿Cómo forzar la ejecución inmediata de todas las tareas?

```bash
# Forzar todas las tareas sin delay:
sudo anacron -fn

# Solo verificar sintaxis:
sudo anacron -T

# Debug (ver lo que haría):
sudo anacron -d
```

<details><summary>Predicción</summary>

- `anacron -fn`: `-f` fuerza la ejecución de todas las tareas aunque no estén pendientes, `-n` ignora los delays. Las tareas se ejecutan inmediatamente.
- `anacron -T`: solo verifica la sintaxis de `/etc/anacrontab`. Si hay errores, los reporta. No ejecuta nada.
- `anacron -d`: modo debug, no se va al background, muestra mensajes de qué está haciendo. Útil para diagnosticar por qué una tarea no se ejecuta.

</details>

### Ejercicio 7 — Agregar tarea personalizada

¿Cuál es la forma más común de agregar una tarea a anacron?

```bash
# Opción 1 — Script en /etc/cron.daily/ (más común):
sudo tee /etc/cron.daily/mycheck << 'EOF'
#!/bin/bash
logger "mycheck: verificacion diaria completada"
EOF
sudo chmod +x /etc/cron.daily/mycheck

# Verificar:
run-parts --test /etc/cron.daily | grep mycheck
```

<details><summary>Predicción</summary>

La forma más común es crear un script en `/etc/cron.daily/` porque anacron ya ejecuta `run-parts` sobre ese directorio. El script:
- Debe ser ejecutable (`chmod +x`)
- En Debian, **no** debe tener extensión (no `.sh`)
- Debe ser propiedad de root
- `run-parts --test` confirmará que lo detecta

Para tareas con periodicidad no estándar (ej: cada 3 días), se agrega una línea directamente en `/etc/anacrontab`:
```
3  20  mi-tarea  /usr/local/bin/mi-tarea.sh
```

</details>

### Ejercicio 8 — cron vs anacron: cuándo usar cada uno

¿Qué herramienta usarías para cada caso?

```
a) Backup de base de datos a las 03:00 en un servidor 24/7
b) Actualización de paquetes en una laptop que se apaga cada noche
c) Script que se ejecuta cada 5 minutos
d) Rotación de logs diaria en un servidor que podría reiniciarse
e) Verificación de integridad cada 3 días
```

<details><summary>Predicción</summary>

- **a)** **cron** — necesita hora exacta (03:00) y el servidor está siempre encendido. `0 3 * * * /path/to/backup.sh`
- **b)** **anacron** — la laptop puede estar apagada a la hora programada. anacron ejecutará la tarea al encender.
- **c)** **cron** — anacron no puede manejar intervalos menores a 1 día. `*/5 * * * * /path/to/script.sh`
- **d)** **anacron** (o systemd timer con `Persistent=true`) — si el servidor se reinicia justo cuando toca rotar, anacron lo ejecutará después. De hecho, `logrotate` ya se ejecuta via `/etc/cron.daily/` que lo gestiona anacron.
- **e)** **anacron** con línea custom en `/etc/anacrontab`: `3  20  integrity  /path/to/check.sh`

</details>

### Ejercicio 9 — Debian vs RHEL

¿Cómo difiere la implementación de anacron entre distribuciones?

```bash
# Debian:
docker compose exec debian-dev bash -c '
dpkg -l anacron 2>/dev/null | grep "^ii" && echo "Paquete separado"
systemctl is-active anacron.timer 2>/dev/null
'

# RHEL:
docker compose exec alma-dev bash -c '
rpm -q cronie-anacron 2>/dev/null && echo "Integrado en cronie"
'
```

<details><summary>Predicción</summary>

- **Debian**: anacron es un **paquete separado** (`anacron`). Se invoca externamente via systemd timer (`anacron.timer`) o cron job (`/etc/cron.d/anacron`). Es un binario independiente que se ejecuta, verifica tareas pendientes, las ejecuta, y termina.
- **RHEL**: anacron está **integrado en cronie** (`cronie-anacron`). No hay daemon separado — `crond` maneja las tareas de anacron internamente. La configuración sigue siendo `/etc/anacrontab` y los timestamps siguen en `/var/spool/anacron/`.
- En ambos casos, la configuración y el resultado son los mismos. Solo difiere el mecanismo de invocación.

</details>

### Ejercicio 10 — anacron vs systemd timers

¿En qué situaciones un systemd timer es mejor que anacron?

<details><summary>Predicción</summary>

**systemd timers superan a anacron cuando:**

1. **Necesitas precisión menor a 1 día** — anacron solo trabaja en días, systemd timers pueden usar `OnCalendar=*-*-* *:00/15:00` (cada 15 min) con `Persistent=true` para garantizar ejecución.

2. **Usuarios no-root necesitan tareas con garantía** — anacron es solo root. systemd user timers (`~/.config/systemd/user/`) permiten a cualquier usuario crear tareas persistentes.

3. **Necesitas logs integrados** — systemd timers se registran en el journal con toda la información estructurada. anacron solo deja logs en syslog.

4. **Necesitas dependencias** — un timer puede depender de `network-online.target` para esperar conectividad. anacron no tiene mecanismo de dependencias.

**anacron sigue siendo mejor cuando:**
- Solo necesitas daily/weekly/monthly sin complicaciones
- Quieres aprovechar el ecosistema de scripts en `/etc/cron.daily/`
- No quieres crear dos archivos (`.timer` + `.service`) por tarea

</details>
