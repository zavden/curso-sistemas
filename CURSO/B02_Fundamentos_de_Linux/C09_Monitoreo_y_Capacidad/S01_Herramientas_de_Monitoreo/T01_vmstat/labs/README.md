# Lab — vmstat

## Objetivo

Usar vmstat para monitoreo de sistema en vivo: snapshot vs intervalo,
entender la primera linea (promedio desde boot), interpretar cada
grupo de columnas (procs, memory, swap, io, system, cpu), opciones
utiles (-S, -w, -t, -a, -d), patrones de diagnostico (CPU saturada,
presion de memoria, thrashing, I/O wait), y uso en scripts.

## Prerequisitos

- Entorno del curso levantado (`docker compose up -d`)

---

## Parte 1 — Lectura basica

### Objetivo

Ejecutar vmstat, entender snapshot vs intervalo y la primera linea.

### Paso 1.1: Snapshot unico

```bash
docker compose exec debian-dev bash -c '
echo "=== vmstat — snapshot ==="
echo ""
echo "Un vmstat sin argumentos muestra promedios desde el boot:"
echo ""
vmstat
echo ""
echo "IMPORTANTE: esta salida es un PROMEDIO desde que arranco"
echo "el sistema. No refleja lo que pasa ahora."
echo ""
echo "--- Uptime del sistema ---"
uptime
'
```

### Paso 1.2: Monitoreo con intervalo

```bash
docker compose exec debian-dev bash -c '
echo "=== vmstat con intervalo ==="
echo ""
echo "vmstat 2 5 → cada 2 segundos, 5 muestras:"
echo ""
vmstat 2 5
echo ""
echo "--- Observar ---"
echo "  Linea 1: PROMEDIO desde el boot (ignorar para diagnostico)"
echo "  Lineas 2-5: datos del INTERVALO (lo que importa)"
echo ""
echo "Si solo pones vmstat 2 (sin count), corre infinitamente"
echo "hasta Ctrl+C."
'
```

### Paso 1.3: Encabezados de columnas

```bash
docker compose exec debian-dev bash -c '
echo "=== Grupos de columnas ==="
echo ""
echo "vmstat organiza la salida en 6 grupos:"
echo ""
echo "  procs     r  b          procesos"
echo "  memory    swpd free buff cache   memoria"
echo "  swap      si  so         actividad de swap"
echo "  io        bi  bo         actividad de disco"
echo "  system    in  cs         interrupciones y context switches"
echo "  cpu       us sy id wa st porcentaje de CPU"
echo ""
echo "--- En vivo ---"
vmstat -w 1 3
echo ""
echo "La opcion -w (wide) hace las columnas mas anchas"
echo "para evitar que los numeros se peguen."
'
```

---

## Parte 2 — Interpretacion de columnas

### Objetivo

Interpretar cada grupo de columnas para diagnostico.

### Paso 2.1: procs — procesos

```bash
docker compose exec debian-dev bash -c '
echo "=== procs ==="
echo ""
echo "  r = Run queue: procesos ejecutandose o esperando CPU"
echo "  b = Blocked: procesos bloqueados esperando I/O"
echo ""
echo "--- Diagnostico ---"
CPUS=$(nproc)
echo "  Este sistema tiene $CPUS CPUs"
echo ""
echo "  r > $CPUS sostenido → CPU saturada"
echo "  b > 0 sostenido → problema de I/O"
echo ""
echo "--- Valores actuales ---"
vmstat 1 3 | tail -1 | awk -v cpus="$CPUS" '"'"'{
    printf "  r=%s (run queue)  b=%s (blocked)\n", $1, $2
    if ($1 > cpus) print "  ALERTA: r > CPUs"
    else print "  OK: r <= CPUs"
    if ($2 > 0) print "  NOTA: hay procesos bloqueados en I/O"
    else print "  OK: sin procesos bloqueados"
}'"'"'
echo ""
echo "--- Patrones ---"
echo "  r=1  b=0   normal (1 proceso corriendo, 0 bloqueados)"
echo "  r=12 b=0   CPU saturada (12 procesos esperan CPU)"
echo "  r=1  b=5   problema de I/O (5 procesos esperan disco)"
echo "  r=15 b=8   CPU e I/O saturados"
'
```

### Paso 2.2: memory — memoria

```bash
docker compose exec debian-dev bash -c '
echo "=== memory ==="
echo ""
echo "  swpd  = memoria en swap (KB)"
echo "  free  = RAM libre (no usada para nada)"
echo "  buff  = buffers del kernel (metadata de filesystem)"
echo "  cache = page cache (archivos leidos del disco)"
echo ""
echo "--- Valores actuales ---"
vmstat -S M 1 2 | tail -1 | awk '"'"'{
    printf "  swpd=%sMB  free=%sMB  buff=%sMB  cache=%sMB\n", $3, $4, $5, $6
    avail = $4 + $5 + $6
    printf "  Disponible aprox: %sMB (free + buff + cache)\n", avail
}'"'"'
echo ""
echo "--- Interpretacion ---"
echo "  free bajo NO es problema — Linux usa RAM libre como cache"
echo "  Lo que importa es: free + buff + cache (disponible)"
echo "  buff y cache se liberan automaticamente si se necesita RAM"
echo ""
echo "  swpd > 0: hubo presion de memoria en algun momento"
echo "  swpd > 0 con si/so = 0: swap viejo, no hay problema ahora"
echo ""
echo "--- Comparar con free ---"
free -m | head -2
echo ""
echo "La columna available de free es la referencia correcta."
'
```

### Paso 2.3: swap — actividad de swap

```bash
docker compose exec debian-dev bash -c '
echo "=== swap ==="
echo ""
echo "  si = Swap In:  KB/s leidos desde swap (disco → RAM)"
echo "  so = Swap Out: KB/s escritos a swap (RAM → disco)"
echo ""
echo "--- Valores actuales ---"
vmstat 1 3 | tail -1 | awk '"'"'{
    printf "  si=%s  so=%s\n", $7, $8
    if ($7 == 0 && $8 == 0) print "  OK: sin actividad de swap"
    else if ($8 > 0 && $7 == 0) print "  NOTA: escribiendo a swap (presion de memoria)"
    else if ($7 > 0 && $8 > 0) print "  ALERTA: thrashing (si Y so altos)"
}'"'"'
echo ""
echo "--- Patrones ---"
echo "  si=0   so=0     ideal (sin swap)"
echo "  si=0   so=500   escribiendo 500KB/s a swap (falta RAM)"
echo "  si=200 so=300   THRASHING — el sistema mueve paginas"
echo "                  entre RAM y disco constantemente"
echo ""
echo "THRASHING = si Y so altos simultaneamente"
echo "El sistema gasta mas tiempo moviendo paginas que trabajando."
echo "Solucion: agregar RAM o reducir carga."
'
```

### Paso 2.4: io — actividad de disco

```bash
docker compose exec debian-dev bash -c '
echo "=== io ==="
echo ""
echo "  bi = Blocks In:  bloques/s leidos (disco → RAM)"
echo "  bo = Blocks Out: bloques/s escritos (RAM → disco)"
echo "  Un bloque = 1024 bytes (1 KB)"
echo ""
echo "--- Valores actuales ---"
vmstat 1 3 | tail -1 | awk '"'"'{
    printf "  bi=%s  bo=%s\n", $9, $10
    if ($9 < 100 && $10 < 100) print "  I/O bajo (normal en idle)"
    else if ($9 > 1000) print "  Lectura intensa"
    else if ($10 > 1000) print "  Escritura intensa"
}'"'"'
echo ""
echo "--- Patrones ---"
echo "  bi=5     bo=10     servidor idle"
echo "  bi=5000  bo=50     lectura intensa (BD, cache fria)"
echo "  bi=10    bo=8000   escritura intensa (backup, logging)"
echo "  bi=5000  bo=5000   I/O alto bidireccional (copia)"
'
```

### Paso 2.5: system — interrupciones y context switches

```bash
docker compose exec debian-dev bash -c '
echo "=== system ==="
echo ""
echo "  in = Interrupts: interrupciones por segundo"
echo "  cs = Context Switches: cambios de contexto por segundo"
echo ""
echo "--- Valores actuales ---"
vmstat 1 3 | tail -1 | awk '"'"'{
    printf "  in=%s  cs=%s\n", $11, $12
    if ($12 > 50000) print "  NOTA: context switches altos"
    else print "  Normal"
}'"'"'
echo ""
echo "--- Que significan ---"
echo "  Interrupts: timer, disco, red, hardware"
echo "  Context switches: el kernel cambia entre procesos"
echo ""
echo "  cs altos pueden indicar:"
echo "    - Demasiados procesos compitiendo por CPU"
echo "    - Lock contention (threads esperando)"
echo "    - Interrupciones de red excesivas"
'
```

### Paso 2.6: cpu — porcentaje de CPU

```bash
docker compose exec debian-dev bash -c '
echo "=== cpu ==="
echo ""
echo "  us = User:    tiempo en codigo de aplicaciones"
echo "  sy = System:  tiempo en codigo del kernel"
echo "  id = Idle:    tiempo ocioso"
echo "  wa = Wait IO: esperando I/O de disco"
echo "  st = Stolen:  robado por hipervisor (VMs)"
echo ""
echo "  Los 5 valores suman ~100%"
echo ""
echo "--- Valores actuales ---"
vmstat 1 3 | tail -1 | awk '"'"'{
    printf "  us=%s%%  sy=%s%%  id=%s%%  wa=%s%%  st=%s%%\n", $13, $14, $15, $16, $17
    if ($15 > 80) print "  → Sistema tranquilo (mucho idle)"
    else if ($13 > 70) print "  → CPU ocupada por aplicaciones"
    else if ($16 > 20) print "  → Disco es el cuello de botella"
    else if ($17 > 10) print "  → VM con tiempo robado"
}'"'"'
echo ""
echo "--- Patrones de diagnostico ---"
echo "  us alto + id bajo      → app consume CPU (optimizar codigo)"
echo "  sy alto                → demasiadas syscalls o I/O kernel"
echo "  wa alto                → disco lento o I/O excesivo"
echo "  st alto                → VM sin recursos (hipervisor saturado)"
echo "  us+sy alto + r > nproc → necesitas mas CPU"
'
```

---

## Parte 3 — Opciones y scripts

### Objetivo

Usar opciones avanzadas y vmstat en scripts de monitoreo.

### Paso 3.1: Opciones utiles

```bash
docker compose exec debian-dev bash -c '
echo "=== Opciones de vmstat ==="
echo ""
echo "--- -S M (unidades en MB) ---"
vmstat -S M 1 2 | tail -1 | head -1
echo "(memoria en MB en lugar de KB)"
echo ""
echo "--- -t (con timestamp) ---"
vmstat -t 1 3
echo ""
echo "--- -a (activo/inactivo) ---"
echo "Reemplaza buff/cache con inact/active:"
vmstat -a 1 2
echo ""
echo "--- -d (estadisticas de disco) ---"
vmstat -d 2>/dev/null | head -5
echo ""
echo "--- -w (wide — columnas mas anchas) ---"
vmstat -w 1 2
'
```

### Paso 3.2: Patrones de uso

```bash
docker compose exec debian-dev bash -c '
echo "=== Patrones de uso ==="
echo ""
echo "--- 1. Monitoreo rapido de CPU ---"
echo "vmstat 1 5"
echo "Mirar: r (run queue), us+sy (CPU), wa (I/O wait)"
echo "Si r > nproc Y id < 5% → CPU saturada"
echo "Si wa > 20% → disco es el cuello de botella"
echo ""
echo "--- 2. Detectar presion de memoria ---"
echo "vmstat 1 5"
echo "Mirar: si, so (swap), swpd, free"
echo "si/so > 0 sostenido → RAM insuficiente"
echo "swpd > 0 pero si/so = 0 → hubo swap pero ya no"
echo ""
echo "--- 3. Capturar baseline ---"
echo "vmstat 5 > /tmp/vmstat-baseline.txt &"
echo "Dejar corriendo, luego comparar con el incidente"
echo ""
echo "--- 4. Durante un incidente ---"
echo "vmstat 1 60 | tee /tmp/vmstat-incident.txt"
echo "1 segundo de intervalo, 60 muestras"
echo ""
echo "--- Captura de 5 segundos ---"
vmstat -t 1 5
'
```

### Paso 3.3: vmstat en scripts

```bash
docker compose exec debian-dev bash -c '
echo "=== vmstat en scripts ==="
echo ""
echo "--- Extraer I/O wait ---"
WA=$(vmstat 1 2 | tail -1 | awk "{print \$16}")
echo "I/O wait actual: ${WA}%"
if [ "$WA" -gt 30 ] 2>/dev/null; then
    echo "  ALERTA: I/O wait supera 30%"
else
    echo "  OK: I/O wait normal"
fi
echo ""
echo "--- Extraer run queue ---"
CPUS=$(nproc)
RQ=$(vmstat 1 2 | tail -1 | awk "{print \$1}")
echo "Run queue: $RQ  CPUs: $CPUS"
if [ "$RQ" -gt "$CPUS" ] 2>/dev/null; then
    echo "  ALERTA: run queue $RQ > $CPUS CPUs"
else
    echo "  OK: run queue dentro de capacidad"
fi
echo ""
echo "--- Verificar swap activo ---"
SO=$(vmstat 1 2 | tail -1 | awk "{print \$8}")
echo "Swap out: ${SO} KB/s"
if [ "$SO" -gt 0 ] 2>/dev/null; then
    echo "  ALERTA: el sistema esta swappeando"
else
    echo "  OK: sin swap out"
fi
echo ""
echo "--- Script de resumen ---"
echo "vmstat 1 2 | tail -1 | awk permite extraer cualquier columna"
echo "Columnas: 1=r 2=b 3=swpd 4=free 5=buff 6=cache"
echo "          7=si 8=so 9=bi 10=bo 11=in 12=cs"
echo "          13=us 14=sy 15=id 16=wa 17=st"
'
```

### Paso 3.4: Resumen de vmstat

```bash
docker compose exec debian-dev bash -c '
echo "=== Resumen de vmstat ==="
echo ""
echo "| Columna | Que mide | Alerta si |"
echo "|---------|----------|-----------|"
echo "| r       | Run queue (procesos esperando CPU) | r > nproc |"
echo "| b       | Blocked (procesos esperando I/O) | b > 0 sostenido |"
echo "| swpd    | Swap usado (KB) | > 0 con so > 0 |"
echo "| free    | RAM libre | Usar available de free |"
echo "| si/so   | Swap in/out (KB/s) | > 0 sostenido |"
echo "| bi/bo   | Disco lectura/escritura (bloques/s) | Depende del disco |"
echo "| in      | Interrupciones/s | Relativo |"
echo "| cs      | Context switches/s | > 50000 investigar |"
echo "| us      | CPU usuario (%) | us+sy > 90% |"
echo "| sy      | CPU kernel (%) | sy > 30% |"
echo "| id      | CPU idle (%) | < 5% |"
echo "| wa      | I/O wait (%) | > 20% |"
echo "| st      | Stolen (%) | > 10% |"
echo ""
echo "La PRIMERA LINEA de vmstat siempre es el promedio desde el boot."
echo "Ignorarla para diagnostico. Las siguientes son del intervalo."
'
```

---

## Limpieza final

No hay recursos que limpiar.

---

## Conceptos reforzados

1. vmstat sin argumentos muestra promedios desde el boot.
   Con intervalo (vmstat 2 5), muestra datos del periodo.
   La primera linea siempre es el promedio historico.

2. r (run queue) > nproc indica CPU saturada.
   b (blocked) > 0 indica procesos esperando I/O.

3. si/so > 0 sostenido indica presion de memoria.
   si Y so altos simultaneamente es thrashing.

4. wa (I/O wait) > 20% indica que el disco es el
   cuello de botella. Combinar con iostat para detalle.

5. Los 5 valores de CPU suman ~100%. id alto = sistema
   tranquilo. st alto = VM sin recursos.

6. vmstat -w evita columnas pegadas. -t agrega timestamp.
   -S M muestra en MB. -a muestra activo/inactivo.
