# Lab — iostat

## Objetivo

Usar iostat para diagnostico de I/O de disco: salida basica (tps,
throughput), estadisticas extendidas (-x) con metricas clave (r/s,
w/s, r_await, w_await, aqu-sz, %util), interpretar await como
indicador principal, diferencia de %util en HDD vs SSD, tipo de
carga (secuencial vs aleatoria), diagnostico de disco saturado,
por particion, y flujo de diagnostico completo con vmstat.

## Prerequisitos

- Entorno del curso levantado (`docker compose up -d`)

---

## Parte 1 — Salida basica y extendida

### Objetivo

Ejecutar iostat y entender las dos vistas (basica y extendida).

### Paso 1.1: Verificar iostat

```bash
docker compose exec debian-dev bash -c '
echo "=== iostat ==="
echo ""
echo "iostat es parte del paquete sysstat."
echo ""
if command -v iostat &>/dev/null; then
    echo "iostat instalado:"
    iostat -V 2>&1 | head -1
else
    echo "Instalando sysstat..."
    apt-get update -qq && apt-get install -y -qq sysstat 2>/dev/null
    echo "Instalado:"
    iostat -V 2>&1 | head -1
fi
'
```

### Paso 1.2: Salida basica

```bash
docker compose exec debian-dev bash -c '
echo "=== iostat — salida basica ==="
echo ""
iostat
echo ""
echo "--- Secciones ---"
echo "  avg-cpu: resumen de CPU (como vmstat)"
echo "  Device:  estadisticas por dispositivo"
echo ""
echo "--- Columnas de disco ---"
echo "  tps       = Transfers Per Second (IOPS)"
echo "  kB_read/s = KB leidos por segundo"
echo "  kB_wrtn/s = KB escritos por segundo"
echo "  kB_dscd/s = KB descartados (TRIM en SSD)"
echo "  kB_read   = KB leidos totales"
echo "  kB_wrtn   = KB escritos totales"
echo ""
echo "NOTA: como vmstat, la primera ejecucion muestra"
echo "promedios desde el boot."
'
```

### Paso 1.3: Solo disco / solo CPU

```bash
docker compose exec debian-dev bash -c '
echo "=== Solo disco (-d) ==="
echo ""
iostat -d 1 3
echo ""
echo "=== Solo CPU (-c) ==="
echo ""
iostat -c 1 3
echo ""
echo "-d muestra solo dispositivos, -c solo CPU."
echo "Utiles para enfocarse en un aspecto."
'
```

### Paso 1.4: Estadisticas extendidas (-x)

```bash
docker compose exec debian-dev bash -c '
echo "=== iostat -x (extendidas) ==="
echo ""
echo "Las estadisticas extendidas son las que realmente"
echo "necesitas para diagnostico:"
echo ""
iostat -x 1 3
echo ""
echo "--- Columnas clave ---"
echo "  r/s, w/s     = IOPS de lectura y escritura"
echo "  rkB/s, wkB/s = throughput (KB/s)"
echo "  r_await       = latencia promedio de lectura (ms)"
echo "  w_await       = latencia promedio de escritura (ms)"
echo "  aqu-sz        = tamaño promedio de la cola"
echo "  %util         = porcentaje de tiempo ocupado"
'
```

### Paso 1.5: Opciones de formato

```bash
docker compose exec debian-dev bash -c '
echo "=== Opciones de formato ==="
echo ""
echo "--- Human-readable (-h) ---"
iostat -h 1 2 2>/dev/null || echo "(requiere version reciente de sysstat)"
echo ""
echo "--- En MB/s (-m) ---"
iostat -m -d 1 2
echo ""
echo "--- Con timestamp (-t) ---"
iostat -t -d 1 2
'
```

---

## Parte 2 — Metricas clave

### Objetivo

Interpretar await, %util y aqu-sz para diagnostico.

### Paso 2.1: await — tiempo de respuesta

```bash
docker compose exec debian-dev bash -c '
echo "=== await ==="
echo ""
echo "r_await y w_await son las metricas MAS IMPORTANTES."
echo "Miden cuanto tarda cada operacion de I/O (en ms)."
echo ""
echo "--- Valores de referencia ---"
echo "  SSD NVMe:      < 0.5 ms  (excelente)"
echo "  SSD SATA:      < 2 ms    (bueno)"
echo "  HDD 7200 RPM:  5-10 ms   (normal)"
echo "  HDD 5400 RPM:  10-20 ms  (normal)"
echo ""
echo "--- Alertas ---"
echo "  > 20 ms en SSD  → algo esta mal"
echo "  > 50 ms en HDD  → disco lento o saturado"
echo "  > 100 ms         → problema serio"
echo ""
echo "--- Valores actuales ---"
iostat -x 1 2 | tail -n +7 | grep -v "^$" | \
    awk "NR>1 && \$1 !~ /^Device/ && \$1 !~ /^avg/ && \$1 !~ /^Linux/ {
        if (NF >= 10) printf \"  %s: r_await=%.1f ms  w_await=%.1f ms\n\", \$1, \$6, \$12
    }" 2>/dev/null || echo "  (extraer await del formato disponible)"
echo ""
echo "await alto + %util alto = disco SATURADO"
echo "await alto + %util bajo = problema de hardware"
'
```

### Paso 2.2: %util — utilizacion

```bash
docker compose exec debian-dev bash -c '
echo "=== %util ==="
echo ""
echo "Porcentaje de tiempo que el dispositivo tuvo I/O en curso."
echo ""
echo "--- En HDD ---"
echo "  %util ~100% → disco saturado, no puede mas"
echo "  El throughput esta al maximo"
echo ""
echo "--- En SSD y RAID ---"
echo "  %util ~100% NO significa saturacion necesariamente"
echo "  Porque atienden multiples requests en paralelo"
echo "  Un SSD NVMe puede tener %util=100% con capacidad libre"
echo ""
echo "  La clave: mirar await JUNTO con %util"
echo "  %util alto + await bajo → el disco rinde bien"
echo "  %util alto + await alto → disco saturado"
echo ""
echo "--- Valores actuales ---"
iostat -x 1 2 | grep -E "^[a-z]|^nvme" | tail -n +2 | \
    awk "{printf \"  %s: %%util=%s\n\", \$1, \$NF}" 2>/dev/null || \
    echo "  (ver ultima columna de iostat -x)"
'
```

### Paso 2.3: aqu-sz — tamano de cola

```bash
docker compose exec debian-dev bash -c '
echo "=== aqu-sz ==="
echo ""
echo "Average Queue Size: cuantos requests esperan en promedio."
echo ""
echo "  aqu-sz < 1    → pocas operaciones pendientes (bien)"
echo "  aqu-sz 1-4    → carga moderada"
echo "  aqu-sz > 4    → posible saturacion"
echo "  aqu-sz > 10   → el disco no da abasto"
echo ""
echo "En RAID o SSD NVMe, aqu-sz alto es mas tolerable"
echo "porque procesan multiples operaciones en paralelo."
echo ""
echo "--- Valores actuales ---"
iostat -x 1 2 | grep -E "^[a-z]|^nvme" | tail -n +2 | \
    awk "{printf \"  %s: aqu-sz=%s\n\", \$1, \$(NF-1)}" 2>/dev/null || \
    echo "  (ver penultima columna de iostat -x)"
'
```

### Paso 2.4: Tipo de carga

```bash
docker compose exec debian-dev bash -c '
echo "=== Tipo de carga ==="
echo ""
echo "--- Carga secuencial (backup, streaming) ---"
echo "  r/s o w/s BAJO, pero rkB/s o wkB/s ALTO"
echo "  rareq-sz o wareq-sz GRANDE (128KB+)"
echo "  Grandes bloques leidos/escritos secuencialmente"
echo ""
echo "--- Carga aleatoria (base de datos, web) ---"
echo "  r/s o w/s ALTO, pero rkB/s o wkB/s MODERADO"
echo "  rareq-sz o wareq-sz PEQUENO (4-16KB)"
echo "  Muchas operaciones pequenas en posiciones aleatorias"
echo ""
echo "  HDD rinde MAL con I/O aleatorio (seek time)"
echo "  SSD rinde BIEN con ambos tipos"
echo ""
echo "--- Identificar en la salida ---"
echo "Si IOPS alto y throughput bajo → aleatorio (requests pequenos)"
echo "Si IOPS bajo y throughput alto → secuencial (requests grandes)"
echo ""
echo "--- Ver la salida completa ---"
iostat -x 1 2 | tail -n +7 | head -10
'
```

---

## Parte 3 — Diagnostico

### Objetivo

Diagnosticar problemas de disco y flujo completo.

### Paso 3.1: Disco saturado

```bash
docker compose exec debian-dev bash -c '
echo "=== Diagnostico: disco saturado ==="
echo ""
echo "Senales de disco saturado:"
echo "  %util > 90%"
echo "  await > 50ms (HDD) o > 10ms (SSD)"
echo "  aqu-sz > 4"
echo ""
echo "--- Causas comunes ---"
echo "  - App haciendo I/O excesivo"
echo "  - Swap (verificar con vmstat si/so)"
echo "  - Disco lento o fallando"
echo "  - RAID en reconstruccion"
echo ""
echo "--- Siguiente paso: iotop ---"
echo "  iotop -o       ver procesos con I/O activo"
echo "  iotop -oP      acumulado por proceso"
echo ""
echo "--- Verificar si iotop esta disponible ---"
if command -v iotop &>/dev/null; then
    echo "iotop disponible"
else
    echo "iotop no instalado (apt install iotop)"
fi
'
```

### Paso 3.2: Por particion

```bash
docker compose exec debian-dev bash -c '
echo "=== iostat por particion ==="
echo ""
echo "--- Dispositivos disponibles ---"
iostat -d 1 2 | grep -E "^[a-z]|^nvme" | tail -n +2 | \
    awk "{print \"  \" \$1}" | sort -u
echo ""
echo "--- Con -p ALL (todas las particiones) ---"
iostat -x -p ALL 1 2 2>/dev/null | tail -n +7 | head -15
echo ""
echo "Util para identificar que filesystem genera mas I/O."
echo "Ejemplo: si sda1 (/) tiene poco I/O pero sda2 (/var)"
echo "tiene mucho → logs o BD en /var son la causa."
'
```

### Paso 3.3: Lectura vs escritura

```bash
docker compose exec debian-dev bash -c '
echo "=== Lectura vs escritura ==="
echo ""
echo "Comparar r_await vs w_await, r/s vs w/s:"
echo ""
iostat -x 1 2 | tail -n +7 | head -10
echo ""
echo "--- Si el problema es de lectura ---"
echo "  BD leyendo mucho (cache fria, queries sin indice)"
echo "  App leyendo archivos grandes"
echo ""
echo "--- Si el problema es de escritura ---"
echo "  Logging excesivo"
echo "  BD escribiendo (commits, checkpoints)"
echo "  Backup en progreso"
echo "  Swap out (falta de RAM)"
'
```

### Paso 3.4: Flujo completo de diagnostico

```bash
docker compose exec debian-dev bash -c '
echo "=== Flujo completo de diagnostico ==="
echo ""
echo "1. vmstat → hay I/O wait?"
echo "   vmstat 1 5  (mirar wa)"
echo ""
WA=$(vmstat 1 2 | tail -1 | awk "{print \$16}")
echo "   wa actual: ${WA}%"
echo ""
echo "2. iostat -x → que disco esta saturado?"
echo "   iostat -x 1 5  (mirar await, %util)"
echo ""
echo "3. iotop → que proceso genera el I/O?"
echo "   sudo iotop -o"
echo ""
echo "--- Resumen actual del sistema ---"
echo ""
echo "CPU:"
vmstat 1 2 | tail -1 | awk '"'"'{printf "  us=%s%% sy=%s%% id=%s%% wa=%s%%\n", $13, $14, $15, $16}'"'"'
echo ""
echo "Disco:"
iostat -d 1 2 | grep -E "^[a-z]|^nvme" | tail -n +2 | \
    awk "{printf \"  %s: tps=%s  kB_read/s=%s  kB_wrtn/s=%s\n\", \$1, \$2, \$3, \$4}" 2>/dev/null
'
```

### Paso 3.5: Tabla comparativa

```bash
docker compose exec debian-dev bash -c '
echo "=== Tabla comparativa ==="
echo ""
echo "| Metrica | Que mide | Alerta si |"
echo "|---------|----------|-----------|"
echo "| tps     | IOPS totales | Depende del disco |"
echo "| r/s     | Lecturas por segundo | Alto + await alto |"
echo "| w/s     | Escrituras por segundo | Alto + await alto |"
echo "| rkB/s   | Throughput lectura | Depende del disco |"
echo "| wkB/s   | Throughput escritura | Depende del disco |"
echo "| r_await | Latencia lectura (ms) | >20ms SSD, >50ms HDD |"
echo "| w_await | Latencia escritura (ms) | >20ms SSD, >50ms HDD |"
echo "| aqu-sz  | Cola promedio | > 4 |"
echo "| %util   | Tiempo ocupado | > 90% + await alto |"
echo ""
echo "--- Flujo de diagnostico ---"
echo "  vmstat (wa) → iostat -x (que disco) → iotop (que proceso)"
'
```

---

## Limpieza final

No hay recursos que limpiar.

---

## Conceptos reforzados

1. iostat es parte de sysstat. La primera ejecucion muestra
   promedios desde el boot, las siguientes son del intervalo.

2. r_await y w_await son las metricas mas importantes. Miden
   la latencia real de cada operacion de I/O en milisegundos.

3. %util 100% en HDD = saturacion. En SSD/RAID no
   necesariamente. La clave es await + %util juntos.

4. Carga secuencial = IOPS bajo + throughput alto.
   Carga aleatoria = IOPS alto + throughput moderado.
   HDD rinde mal con aleatorio, SSD bien con ambos.

5. aqu-sz > 4 indica posible saturacion. En RAID/SSD
   es mas tolerable por paralelismo.

6. Flujo de diagnostico: vmstat (wa) para detectar I/O
   wait → iostat -x para identificar el disco → iotop
   para encontrar el proceso responsable.
