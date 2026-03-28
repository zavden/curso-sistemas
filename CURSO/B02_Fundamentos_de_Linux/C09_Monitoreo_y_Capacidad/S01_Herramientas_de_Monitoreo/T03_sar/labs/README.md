# Lab — sar

## Objetivo

Usar sar (System Activity Reporter) para monitoreo historico:
paquete sysstat (instalacion, habilitacion, recopilacion via
timer/cron, datos en /var/log/sysstat o /var/log/sa), estadisticas
en vivo (-u CPU, -r memoria, -S swap, -W actividad swap, -b I/O,
-d por dispositivo, -n DEV/EDEV/SOCK red, -q load, -w context
switches), analisis historico (rangos, picos, correlacion),
formatos con sadf, y diferencias Debian vs RHEL.

## Prerequisitos

- Entorno del curso levantado (`docker compose up -d`)

---

## Parte 1 — Sysstat y configuracion

### Objetivo

Entender el paquete sysstat, su recopilacion de datos y configuracion.

### Paso 1.1: Instalar y verificar sysstat

```bash
docker compose exec debian-dev bash -c '
echo "=== sysstat ==="
echo ""
echo "sar es parte del paquete sysstat (junto con iostat, mpstat, etc.)"
echo ""
if command -v sar &>/dev/null; then
    echo "sysstat instalado:"
    sar -V 2>&1 | head -1
else
    echo "Instalando sysstat..."
    apt-get update -qq && apt-get install -y -qq sysstat 2>/dev/null
    echo "Instalado:"
    sar -V 2>&1 | head -1
fi
echo ""
echo "--- Diferencia con vmstat/iostat ---"
echo "  vmstat/iostat → que pasa AHORA"
echo "  sar           → que paso AYER a las 03:00"
echo ""
echo "sar almacena datos periodicamente y permite consultar"
echo "la actividad de horas, dias o semanas anteriores."
'
```

### Paso 1.2: Habilitacion

```bash
docker compose exec debian-dev bash -c '
echo "=== Habilitacion de sysstat ==="
echo ""
echo "--- Debian ---"
echo "En Debian, sysstat se habilita en /etc/default/sysstat:"
if [[ -f /etc/default/sysstat ]]; then
    cat /etc/default/sysstat
    echo ""
    ENABLED=$(grep -oP "ENABLED=\"\K[^\"]*" /etc/default/sysstat)
    echo "Estado: ENABLED=$ENABLED"
    [[ "$ENABLED" == "false" ]] && echo "  Para habilitar: ENABLED=\"true\""
else
    echo "  (archivo no encontrado)"
fi
echo ""
echo "--- RHEL ---"
echo "En RHEL, sysstat esta habilitado por defecto."
echo "Solo verificar: systemctl is-enabled sysstat"
'
```

### Paso 1.3: Recopilacion de datos

```bash
docker compose exec debian-dev bash -c '
echo "=== Recopilacion de datos ==="
echo ""
echo "sysstat recopila datos cada 10 minutos (por defecto):"
echo ""
echo "--- Via timer de systemd ---"
if systemctl cat sysstat-collect.timer &>/dev/null 2>&1; then
    systemctl cat sysstat-collect.timer 2>/dev/null | grep -E "OnCalendar|Description"
else
    echo "  (sin timer de sysstat)"
fi
echo ""
echo "--- Via cron ---"
if [[ -f /etc/cron.d/sysstat ]]; then
    echo "Cron job:"
    cat /etc/cron.d/sysstat
else
    echo "  (sin cron de sysstat)"
fi
echo ""
echo "--- Programas ---"
echo "  sa1: recopila datos (cada 10 min)"
echo "  sa2: genera resumen diario (23:53)"
'
```

### Paso 1.4: Donde se almacenan

```bash
docker compose exec debian-dev bash -c '
echo "=== Almacenamiento de datos ==="
echo ""
echo "Los datos se guardan en archivos binarios:"
echo ""
echo "--- Debian: /var/log/sysstat/ ---"
ls -la /var/log/sysstat/ 2>/dev/null || echo "  (directorio no existe)"
echo ""
echo "--- RHEL: /var/log/sa/ ---"
echo "(verificar en alma-dev)"
echo ""
echo "--- Formato de archivos ---"
echo "  saDD  → datos binarios del dia DD (sa01, sa02, ..., sa31)"
echo "  sarDD → resumenes de texto del dia DD"
echo ""
echo "Los archivos del mes anterior se sobreescriben."
echo "sa01 de marzo reemplaza sa01 de febrero."
echo ""
echo "--- Retencion ---"
if [[ -f /etc/sysstat/sysstat ]]; then
    grep "HISTORY" /etc/sysstat/sysstat 2>/dev/null
else
    echo "  HISTORY=28 (default: 28 dias)"
fi
'
```

---

## Parte 2 — Estadisticas en vivo

### Objetivo

Usar sar para capturar estadisticas en vivo de CPU, memoria, disco y red.

### Paso 2.1: CPU (-u)

```bash
docker compose exec debian-dev bash -c '
echo "=== sar -u (CPU) ==="
echo ""
echo "Equivalente a la seccion cpu de vmstat:"
echo ""
sar -u 1 5
echo ""
echo "--- Columnas ---"
echo "  %user    = tiempo en aplicaciones (= us de vmstat)"
echo "  %nice    = tiempo en procesos con nice"
echo "  %system  = tiempo en kernel (= sy de vmstat)"
echo "  %iowait  = esperando I/O (= wa de vmstat)"
echo "  %steal   = robado por hipervisor (= st de vmstat)"
echo "  %idle    = ocioso (= id de vmstat)"
echo ""
echo "--- Por CPU individual ---"
echo "sar -u -P ALL  (muestra cada CPU)"
echo "sar -u -P 0    (solo CPU 0)"
'
```

### Paso 2.2: Memoria (-r)

```bash
docker compose exec debian-dev bash -c '
echo "=== sar -r (memoria) ==="
echo ""
sar -r 1 3
echo ""
echo "--- Columnas clave ---"
echo "  kbmemfree  = RAM libre"
echo "  kbavail    = RAM disponible (incluyendo cache liberables)"
echo "  %memused   = porcentaje de RAM usada"
echo "  kbcached   = page cache"
echo "  %commit    = memoria comprometida vs total (RAM + swap)"
echo "  kbdirty    = paginas modificadas sin escribir a disco"
echo ""
echo "kbavail es lo que importa (como available de free)."
'
```

### Paso 2.3: Swap (-S y -W)

```bash
docker compose exec debian-dev bash -c '
echo "=== sar -S (uso de swap) ==="
echo ""
sar -S 1 3
echo ""
echo "  kbswpfree = swap libre"
echo "  kbswpused = swap usada"
echo "  %swpused  = porcentaje de swap usada"
echo ""
echo "=== sar -W (actividad de swap) ==="
echo ""
sar -W 1 3
echo ""
echo "  pswpin/s  = paginas/s leidas del swap (swap in)"
echo "  pswpout/s = paginas/s escritas al swap (swap out)"
echo ""
echo "Equivale a si/so de vmstat pero por pagina, no por KB."
'
```

### Paso 2.4: I/O de disco (-b y -d)

```bash
docker compose exec debian-dev bash -c '
echo "=== sar -b (I/O global) ==="
echo ""
sar -b 1 3
echo ""
echo "  tps    = IOPS totales"
echo "  rtps   = lecturas/s"
echo "  wtps   = escrituras/s"
echo "  bread/s = bloques leidos/s"
echo "  bwrtn/s = bloques escritos/s"
echo ""
echo "=== sar -d (por dispositivo) ==="
echo ""
sar -d 1 3
echo ""
echo "Similar a iostat -x pero con capacidad historica."
echo "Columnas: tps, rkB/s, wkB/s, await, %util"
'
```

### Paso 2.5: Red (-n DEV)

```bash
docker compose exec debian-dev bash -c '
echo "=== sar -n DEV (interfaces de red) ==="
echo ""
sar -n DEV 1 3
echo ""
echo "--- Columnas ---"
echo "  IFACE    = interfaz de red"
echo "  rxpck/s  = paquetes recibidos/s"
echo "  txpck/s  = paquetes transmitidos/s"
echo "  rxkB/s   = KB recibidos/s"
echo "  txkB/s   = KB transmitidos/s"
echo "  %ifutil  = utilizacion de la interfaz"
echo ""
echo "--- Errores de red ---"
echo "sar -n EDEV  (errores: rxerr, txerr, rxdrop, txdrop)"
echo ""
echo "--- Sockets ---"
echo "sar -n SOCK  (totsck, tcpsck, udpsck)"
'
```

### Paso 2.6: Load y context switches (-q y -w)

```bash
docker compose exec debian-dev bash -c '
echo "=== sar -q (load average) ==="
echo ""
sar -q 1 3
echo ""
echo "  runq-sz  = run queue (= r de vmstat)"
echo "  plist-sz = total de procesos/threads"
echo "  ldavg-1/5/15 = load average"
echo "  blocked  = procesos bloqueados (= b de vmstat)"
echo ""
echo "=== sar -w (context switches) ==="
echo ""
sar -w 1 3
echo ""
echo "  proc/s  = procesos creados/s (fork)"
echo "  cswch/s = context switches/s"
'
```

---

## Parte 3 — Historico y formatos

### Objetivo

Consultar datos historicos y exportar en diferentes formatos.

### Paso 3.1: Consultas historicas

```bash
docker compose exec debian-dev bash -c '
echo "=== Consultas historicas ==="
echo ""
echo "sar lee datos historicos de archivos saDD:"
echo ""
echo "--- Comandos ---"
echo "  sar               CPU de hoy"
echo "  sar -1             CPU de ayer"
echo "  sar -f /var/log/sysstat/sa15   CPU del dia 15"
echo ""
echo "--- Con rango horario ---"
echo "  sar -s 08:00:00 -e 12:00:00   solo 08:00 a 12:00"
echo ""
echo "--- Intentar consulta historica ---"
if sar 2>/dev/null | head -5 | grep -q "^[0-9]"; then
    echo "Datos de hoy:"
    sar | head -10
else
    echo "(sin datos historicos — sysstat necesita estar habilitado"
    echo " y haber recopilado al menos 10 minutos de datos)"
fi
'
```

### Paso 3.2: Encontrar picos

```bash
docker compose exec debian-dev bash -c '
echo "=== Encontrar picos ==="
echo ""
echo "--- Ejemplos de consultas ---"
echo ""
echo "Cuando tuvo la CPU mas carga ayer:"
echo "  sar -u -1 | sort -k4 -rn | head -5"
echo ""
echo "Cuando hubo mas I/O wait:"
echo "  sar -u -1 | sort -k6 -rn | head -5"
echo ""
echo "Cuando se uso mas swap:"
echo "  sar -W -1 | grep -v Average | sort -k3 -rn | head -5"
echo ""
echo "--- Correlacionar un incidente ---"
echo ""
echo "\"El servidor estuvo lento ayer entre 14:00 y 15:00\":"
echo "  sar -u -s 14:00:00 -e 15:00:00 -1    # CPU"
echo "  sar -r -s 14:00:00 -e 15:00:00 -1    # Memoria"
echo "  sar -d -s 14:00:00 -e 15:00:00 -1    # Disco"
echo "  sar -n DEV -s 14:00:00 -e 15:00:00 -1  # Red"
echo ""
echo "--- Todo de una vez ---"
echo "  sar -A -s 14:00:00 -e 15:00:00 -1 > /tmp/incident.txt"
echo "  -A = ALL (todas las estadisticas)"
'
```

### Paso 3.3: sadf — formatos de exportacion

```bash
docker compose exec debian-dev bash -c '
echo "=== sadf — formatos de exportacion ==="
echo ""
echo "sadf convierte los datos binarios de sar a otros formatos."
echo ""
echo "--- Formatos disponibles ---"
echo "  sadf -j   JSON"
echo "  sadf -d   CSV (separado por punto y coma)"
echo "  sadf -g   SVG (grafico)"
echo ""
echo "--- Ejemplos ---"
echo "  sadf -j /var/log/sysstat/sa17 -- -u | jq ."
echo "  sadf -d /var/log/sysstat/sa17 -- -u"
echo "  sadf -g /var/log/sysstat/sa17 -- -u > /tmp/cpu.svg"
echo ""
echo "--- Verificar sadf ---"
if command -v sadf &>/dev/null; then
    sadf -V 2>&1 | head -1
else
    echo "  sadf no encontrado"
fi
'
```

### Paso 3.4: Debian vs RHEL

```bash
docker compose exec debian-dev bash -c '
echo "=== Debian ==="
echo ""
echo "Config:"
[[ -f /etc/default/sysstat ]] && cat /etc/default/sysstat || echo "  (no encontrado)"
echo ""
echo "Datos en:"
ls /var/log/sysstat/ 2>/dev/null || echo "  /var/log/sysstat/ (vacio o no existe)"
echo ""
echo "Timer/cron:"
[[ -f /etc/cron.d/sysstat ]] && head -3 /etc/cron.d/sysstat || echo "  (sin cron)"
'
echo ""
docker compose exec alma-dev bash -c '
echo "=== RHEL ==="
echo ""
echo "Estado del servicio:"
systemctl is-enabled sysstat 2>/dev/null || echo "  (verificar)"
echo ""
echo "Datos en:"
ls /var/log/sa/ 2>/dev/null || echo "  /var/log/sa/ (vacio o no existe)"
echo ""
echo "Timer:"
systemctl cat sysstat-collect.timer 2>/dev/null | grep -E "OnCalendar" || echo "  (sin timer)"
'
```

### Paso 3.5: Tabla de opciones de sar

```bash
docker compose exec debian-dev bash -c '
echo "=== Tabla de opciones de sar ==="
echo ""
echo "| Opcion | Que muestra | Columnas clave |"
echo "|--------|-------------|----------------|"
echo "| -u     | CPU | %user, %system, %iowait, %idle |"
echo "| -r     | Memoria | kbavail, %memused, kbdirty |"
echo "| -S     | Uso de swap | kbswpused, %swpused |"
echo "| -W     | Actividad swap | pswpin/s, pswpout/s |"
echo "| -b     | I/O global | tps, bread/s, bwrtn/s |"
echo "| -d     | I/O por disco | await, %util |"
echo "| -n DEV | Red interfaces | rxkB/s, txkB/s, %ifutil |"
echo "| -n EDEV| Red errores | rxerr/s, rxdrop/s |"
echo "| -n SOCK| Sockets | totsck, tcpsck |"
echo "| -q     | Load average | ldavg-1/5/15, runq-sz |"
echo "| -w     | Context switches | cswch/s, proc/s |"
echo "| -A     | TODO | Todas las estadisticas |"
echo ""
echo "Modificadores:"
echo "  -s HH:MM:SS  desde hora"
echo "  -e HH:MM:SS  hasta hora"
echo "  -1           datos de ayer"
echo "  -f archivo   leer archivo especifico"
'
```

---

## Limpieza final

No hay recursos que limpiar.

---

## Conceptos reforzados

1. sar recopila datos historicos (cada 10 min por defecto)
   a diferencia de vmstat/iostat que muestran datos en vivo.
   Los datos se guardan en /var/log/sysstat/ (Debian) o
   /var/log/sa/ (RHEL).

2. En Debian hay que habilitar sysstat (ENABLED="true" en
   /etc/default/sysstat). En RHEL esta habilitado por defecto.

3. sar cubre CPU (-u), memoria (-r), swap (-S/-W), disco
   (-b/-d), red (-n DEV/EDEV/SOCK), load (-q) y context
   switches (-w). -A muestra todo.

4. Para analisis historico: sar -1 (ayer), sar -f saDD
   (dia especifico), -s/-e (rango horario).

5. sadf convierte los datos binarios a JSON (-j), CSV (-d)
   o SVG (-g) para procesamiento automatizado o graficos.

6. El flujo de diagnostico de un incidente pasado: usar
   sar con rango horario para correlacionar CPU, memoria,
   disco y red en la ventana del problema.
