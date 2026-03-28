# Lab — nice y renice

## Objetivo

Gestionar prioridades de procesos: lanzar con nice, cambiar
con renice, entender la relacion nice-PR, las restricciones
de usuarios, y ionice para prioridad de I/O.

## Prerequisitos

- Entorno del curso levantado (`docker compose up -d`)

---

## Parte 1 — nice

### Objetivo

Lanzar procesos con prioridad alterada y entender los valores
de nice.

### Paso 1.1: Valor de nice por defecto

```bash
docker compose exec debian-dev bash -c '
echo "=== Nice por defecto ==="
nice
echo "(0 es el valor por defecto)"

echo ""
echo "=== Rango de nice ==="
echo "-20 (mayor prioridad) ... 0 (default) ... +19 (menor prioridad)"
echo ""
echo "=== Relacion con PR (priority en top) ==="
echo "PR = 20 + nice"
echo "nice -20 → PR  0 (maxima prioridad)"
echo "nice   0 → PR 20 (normal)"
echo "nice +19 → PR 39 (minima prioridad)"
'
```

### Paso 1.2: Lanzar con nice

```bash
docker compose exec debian-dev bash -c '
echo "=== Lanzar con baja prioridad (nice +10) ==="
nice -n 10 sleep 300 &
PID_LOW=$!

echo "=== Lanzar con prioridad normal ==="
sleep 301 &
PID_NORM=$!

echo ""
echo "=== Comparar ==="
ps -o pid,ni,pri,cmd -p $PID_LOW,$PID_NORM
echo ""
echo "NI = nice value, PRI = kernel priority"

kill $PID_LOW $PID_NORM 2>/dev/null
wait 2>/dev/null
'
```

### Paso 1.3: Restricciones de usuario

```bash
docker compose exec debian-dev bash -c '
echo "=== Usuarios normales solo pueden SUBIR nice (bajar prioridad) ==="
echo ""
echo "=== Como root: nice negativo funciona ==="
nice -n -5 sleep 300 &
PID_HIGH=$!
ps -o pid,ni,user,cmd -p $PID_HIGH
kill $PID_HIGH
wait $PID_HIGH 2>/dev/null

echo ""
echo "=== Regla ==="
echo "Usuarios normales: solo nice 0 a +19 (bajar prioridad)"
echo "Root:              nice -20 a +19 (puede subir prioridad)"
echo ""
echo "=== Motivo ==="
echo "Si cualquier usuario pudiera subir prioridad,"
echo "podria acaparar la CPU del sistema"
'
```

### Paso 1.4: nice con diferentes valores

```bash
docker compose exec debian-dev bash -c '
echo "=== Multiples procesos con diferente nice ==="
nice -n -10 sleep 300 &
P1=$!
nice -n 0 sleep 301 &
P2=$!
nice -n 10 sleep 302 &
P3=$!
nice -n 19 sleep 303 &
P4=$!

echo ""
ps -o pid,ni,pri,stat,cmd -p $P1,$P2,$P3,$P4
echo ""
echo "El scheduler CFS asigna mas tiempo de CPU a procesos"
echo "con menor nice (mayor prioridad)"
echo ""
echo "Con un solo proceso, nice no tiene efecto visible"
echo "El efecto se nota cuando hay competencia por CPU"

kill $P1 $P2 $P3 $P4 2>/dev/null
wait 2>/dev/null
'
```

---

## Parte 2 — renice

### Objetivo

Cambiar la prioridad de procesos en ejecucion.

### Paso 2.1: renice basico

```bash
docker compose exec debian-dev bash -c '
echo "=== Crear proceso con prioridad normal ==="
sleep 300 &
PID=$!
echo "Antes:"
ps -o pid,ni,cmd -p $PID

echo ""
echo "=== renice a +10 ==="
renice 10 -p $PID
echo "Despues:"
ps -o pid,ni,cmd -p $PID

echo ""
echo "=== renice a -5 (requiere root) ==="
renice -5 -p $PID
echo "Despues:"
ps -o pid,ni,cmd -p $PID

kill $PID
wait $PID 2>/dev/null
'
```

### Paso 2.2: renice por usuario

```bash
docker compose exec debian-dev bash -c '
echo "=== renice por usuario ==="
echo "renice +5 -u usuario  — cambia TODOS los procesos del usuario"
echo ""
echo "=== Usuarios normales y renice ==="
echo "Un usuario normal puede:"
echo "  - Subir nice de SUS procesos (0→19)"
echo "  - NO puede bajar nice (19→0) — necesita root"
echo "  - NO puede cambiar nice de procesos de otro usuario"
echo ""
echo "Root puede:"
echo "  - Cambiar nice de cualquier proceso"
echo "  - En cualquier direccion (-20 a +19)"
echo "  - De cualquier usuario"
'
```

### Paso 2.3: Verificar con top

```bash
docker compose exec debian-dev bash -c '
echo "=== Ver prioridades con top (batch mode) ==="
nice -n 15 sleep 300 &
P1=$!
nice -n -10 sleep 301 &
P2=$!
sleep 302 &
P3=$!
sleep 0.5

echo "=== Columnas NI y PR en top ==="
top -bn1 -p $P1,$P2,$P3 | tail -n +7 | head -5
echo ""
echo "NI = nice, PR = priority (20+nice)"
echo "Menor PR = mas tiempo de CPU"

kill $P1 $P2 $P3 2>/dev/null
wait 2>/dev/null
'
```

---

## Parte 3 — ionice

### Objetivo

Gestionar la prioridad de I/O de un proceso.

### Paso 3.1: Clases de ionice

```bash
docker compose exec debian-dev bash -c '
echo "=== Clases de scheduling de I/O ==="
echo ""
echo "Clase 1 — Realtime (RT)"
echo "  Acceso prioritario al disco"
echo "  Solo root"
echo "  Puede causar starvation de otros procesos"
echo ""
echo "Clase 2 — Best-effort (BE) [default]"
echo "  Prioridad proporcional"
echo "  8 niveles (0=mas alta, 7=mas baja)"
echo "  Default: nivel derivado de nice (nice/5 + 2, aprox)"
echo ""
echo "Clase 3 — Idle"
echo "  Solo cuando nadie mas necesita I/O"
echo "  Ideal para backups, indexacion"
echo "  No necesita root"
'
```

### Paso 3.2: Verificar y cambiar clase

```bash
docker compose exec debian-dev bash -c '
echo "=== ionice actual del shell ==="
ionice -p $$
echo ""

echo "=== Lanzar con clase idle ==="
ionice -c3 sleep 300 &
PID_IDLE=$!

echo "=== Lanzar con best-effort nivel 0 (alta) ==="
ionice -c2 -n0 sleep 301 &
PID_HIGH=$!

echo ""
echo "=== Verificar ==="
echo "Idle:"
ionice -p $PID_IDLE
echo "Best-effort nivel 0:"
ionice -p $PID_HIGH

kill $PID_IDLE $PID_HIGH 2>/dev/null
wait 2>/dev/null
'
```

### Paso 3.3: ionice en la practica

```bash
docker compose exec debian-dev bash -c '
echo "=== Caso practico: backup con baja prioridad ==="
echo "nice -n 19 ionice -c3 tar czf /backup/data.tar.gz /data"
echo ""
echo "  nice -n 19  — menor prioridad de CPU"
echo "  ionice -c3  — menor prioridad de I/O"
echo "  El backup no interferira con el trabajo normal"
echo ""
echo "=== Caso practico: operacion critica ==="
echo "ionice -c1 -n0 dd if=/dev/sda of=/backup/disk.img"
echo ""
echo "  ionice -c1 -n0 — prioridad maxima de I/O"
echo "  Solo root puede usar clase realtime"
echo ""
echo "=== Limitacion ==="
echo "ionice depende del scheduler de I/O del kernel"
echo "Con el scheduler mq-deadline (comun en NVMe), ionice"
echo "puede no tener efecto — revisar el scheduler activo:"
cat /sys/block/*/queue/scheduler 2>/dev/null | head -3 \
    || echo "(no disponible en contenedor)"
'
```

---

## Limpieza final

No hay recursos que limpiar.

---

## Conceptos reforzados

1. **nice** va de -20 (mayor prioridad) a +19 (menor). Los
   usuarios normales solo pueden subir nice (bajar prioridad).
   La prioridad del kernel es PR = 20 + nice.

2. **renice** cambia la prioridad en caliente. Los usuarios
   normales solo pueden subir nice de sus propios procesos,
   no bajarlo.

3. El efecto de nice se nota cuando hay **competencia por CPU**.
   Con un solo proceso, el scheduler le da toda la CPU
   independientemente del nice.

4. **ionice** controla la prioridad de I/O con 3 clases: realtime
   (maxima), best-effort (proporcional, default), idle (solo
   cuando no hay otro I/O).

5. El patron `nice -n 19 ionice -c3` es ideal para tareas de
   background como backups que no deben interferir con el trabajo
   normal.

6. ionice depende del scheduler de I/O del kernel. Con schedulers
   modernos como mq-deadline (NVMe), puede no tener efecto.
