# Lab — anacron

## Objetivo

Entender anacron: el problema que resuelve (tareas perdidas cuando
el equipo esta apagado), el formato de /etc/anacrontab (periodo,
delay, identificador, comando), los timestamps en /var/spool/anacron/,
las variables START_HOURS_RANGE y RANDOM_DELAY, como cron delega en
anacron, las diferencias entre Debian y RHEL, las limitaciones (solo
root, solo dias), y la comparacion con systemd timers.

## Prerequisitos

- Entorno del curso levantado (`docker compose up -d`)

---

## Parte 1 — Funcionamiento de anacron

### Objetivo

Entender que problema resuelve anacron y como funciona internamente.

### Paso 1.1: El problema

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

### Paso 1.2: /etc/anacrontab

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
echo ""
echo "--- Los 4 campos ---"
echo "1. Periodo: 1=diario, 7=semanal, @monthly=mensual"
echo "2. Delay: minutos de espera antes de ejecutar"
echo "3. Identificador: nombre unico para rastrear ejecucion"
echo "4. Comando: tipicamente run-parts sobre un directorio"
'
```

### Paso 1.3: Timestamps de ultima ejecucion

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
echo "  cron.daily:   20260317 → ejecuto el 17 marzo"
echo "  cron.weekly:  20260314 → ejecuto el 14 marzo"
echo "  cron.monthly: 20260301 → ejecuto el 1 marzo"
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

## Parte 2 — Configuracion y relacion con cron

### Objetivo

Configurar variables de anacron y entender como trabaja con cron.

### Paso 2.1: Variables de anacron

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

### Paso 2.2: Como cron delega en anacron

```bash
docker compose exec debian-dev bash -c '
echo "=== Relacion cron/anacron ==="
echo ""
echo "En /etc/crontab tipico de Debian:"
echo ""
echo "  25 6 * * * root test -x /usr/sbin/anacron || run-parts /etc/cron.daily"
echo ""
echo "Logica:"
echo "  1. cron evalua la linea a las 06:25"
echo "  2. test -x /usr/sbin/anacron → existe anacron?"
echo "     SI → el test es true → || NO se ejecuta"
echo "        anacron se encarga cuando se invoque"
echo "     NO → el test falla → || ejecuta run-parts"
echo "        cron ejecuta los scripts directamente"
echo ""
echo "  cron.hourly: siempre lo ejecuta cron"
echo "  (anacron no maneja intervalos < 1 dia)"
echo ""
echo "--- Quien invoca a anacron ---"
echo "1. systemd timer (Debian moderno):"
echo "   systemctl cat anacron.timer"
echo ""
echo "2. cron job (legacy):"
echo "   cat /etc/cron.d/anacron"
echo ""
echo "3. Integrado en crond (RHEL con cronie)"
echo ""
echo "--- Verificar ---"
systemctl is-active anacron.timer 2>/dev/null && \
    echo "Via systemd timer: ACTIVO" || true
cat /etc/cron.d/anacron 2>/dev/null && \
    echo "(via cron job)" || true
[[ ! -x /usr/sbin/anacron ]] && \
    echo "anacron no instalado — cron ejecuta todo directamente"
'
```

### Paso 2.3: Diferencias cron vs anacron

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

## Parte 3 — Limitaciones y alternativas

### Objetivo

Conocer las limitaciones de anacron y su relacion con systemd timers.

### Paso 3.1: Limitaciones

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
echo ""
echo "5. Agregar tarea custom a anacron:"
echo "   Opcion 1: script en /etc/cron.daily/ (mas comun)"
echo "   Opcion 2: linea en /etc/anacrontab"
echo "     3  20  integrity-check  /usr/local/bin/check.sh"
echo "     → Cada 3 dias, delay 20 min"
'
```

### Paso 3.2: Opciones de linea de comandos

```bash
docker compose exec debian-dev bash -c '
echo "=== Opciones de anacron ==="
echo ""
echo "anacron -f        force: ejecutar todo aunque no este pendiente"
echo "anacron -fn       force + now: sin delays"
echo "anacron -d        debug: no fork, mostrar mensajes"
echo "anacron -T        testear sintaxis de /etc/anacrontab"
echo "anacron -t FILE   usar anacrontab alternativo"
echo "anacron -S DIR    usar directorio de spool alternativo"
echo ""
echo "--- Verificar ---"
if command -v anacron &>/dev/null; then
    echo "Testear sintaxis:"
    anacron -T 2>&1 && echo "  Sintaxis OK" || echo "  Error de sintaxis"
else
    echo "anacron no disponible"
fi
'
```

### Paso 3.3: systemd timers como alternativa

```bash
docker compose exec debian-dev bash -c '
echo "=== systemd timers vs anacron ==="
echo ""
echo "Los systemd timers cubren los mismos casos:"
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

### Paso 3.4: Debian vs RHEL

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
echo "La configuracion sigue siendo /etc/anacrontab"
echo ""
if [[ -f /etc/anacrontab ]]; then
    echo "--- /etc/anacrontab ---"
    cat /etc/anacrontab | grep -v "^#" | grep -v "^$"
fi
'
```

---

## Limpieza final

No hay recursos que limpiar.

---

## Conceptos reforzados

1. anacron garantiza la ejecucion de tareas incluso si el equipo
   estaba apagado. Verifica timestamps en /var/spool/anacron/ y
   ejecuta lo pendiente con un delay.

2. /etc/anacrontab tiene 4 campos: periodo (dias), delay (minutos),
   identificador y comando. START_HOURS_RANGE y RANDOM_DELAY
   controlan cuando y como se ejecutan.

3. En Debian, cron delega cron.daily/weekly/monthly a anacron
   mediante `test -x /usr/sbin/anacron ||`. cron.hourly siempre
   lo ejecuta cron.

4. anacron tiene limitaciones: solo root, minimo 1 dia, sin hora
   exacta. Para mayor flexibilidad, usar systemd timers con
   Persistent=true.

5. En Debian, anacron es un paquete separado invocado por systemd
   timer o cron. En RHEL, esta integrado en cronie (crond lo
   maneja internamente).

6. La forma mas comun de agregar una tarea a anacron es crear un
   script ejecutable en /etc/cron.daily/ (sin extension en Debian).
