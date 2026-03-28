# Lab — Senales

## Objetivo

Entender las senales Unix: listar senales disponibles, enviar
SIGTERM y SIGKILL, observar SIGHUP, pausar/resumir con
SIGSTOP/SIGCONT, y entender el comportamiento especial de
senales para PID 1 en contenedores.

## Prerequisitos

- Entorno del curso levantado (`docker compose up -d`)

---

## Parte 1 — Senales basicas

### Objetivo

Listar senales, enviar SIGTERM y SIGKILL, y entender la
diferencia fundamental entre ambas.

### Paso 1.1: Listar senales disponibles

```bash
docker compose exec debian-dev bash -c '
echo "=== Senales disponibles ==="
kill -l
echo ""
echo "=== Las mas importantes ==="
echo " 1) SIGHUP   — terminal cerrada / reload de configuracion"
echo " 2) SIGINT   — Ctrl+C (interrumpir)"
echo " 3) SIGQUIT  — Ctrl+\\ (quit con core dump)"
echo " 9) SIGKILL  — matar inmediatamente (no se puede capturar)"
echo "13) SIGPIPE  — escribir a pipe roto"
echo "15) SIGTERM  — terminar (default de kill, se puede capturar)"
echo "17) SIGCHLD  — un hijo termino"
echo "18) SIGCONT  — continuar un proceso detenido"
echo "19) SIGSTOP  — detener (no se puede capturar)"
echo "20) SIGTSTP  — Ctrl+Z (detener, se puede capturar)"
'
```

Hay dos senales que no se pueden capturar ni ignorar: SIGKILL (9)
y SIGSTOP (19). Todas las demas pueden tener un handler.

### Paso 1.2: SIGTERM — terminacion educada

```bash
docker compose exec debian-dev bash -c '
echo "=== SIGTERM (15) — senal por defecto de kill ==="
sleep 300 &
PID=$!
echo "Proceso sleep con PID: $PID"
ps -o pid,stat,cmd -p $PID

echo ""
echo "=== Enviar SIGTERM ==="
kill $PID
sleep 0.5
echo "Despues de SIGTERM:"
ps -o pid,stat,cmd -p $PID 2>/dev/null || echo "Proceso ya no existe"
echo ""
echo "SIGTERM pide al proceso que termine limpiamente"
echo "El proceso puede capturar SIGTERM y hacer cleanup"
wait $PID 2>/dev/null
'
```

`kill PID` sin especificar senal envia SIGTERM (15). Es la forma
correcta de terminar un proceso — le da oportunidad de cerrar
archivos, liberar recursos, etc.

### Paso 1.3: SIGKILL — terminacion forzada

```bash
docker compose exec debian-dev bash -c '
echo "=== SIGKILL (9) — matar inmediatamente ==="
sleep 300 &
PID=$!
echo "Proceso sleep con PID: $PID"

echo ""
echo "=== Enviar SIGKILL ==="
kill -9 $PID
sleep 0.5
echo "Despues de SIGKILL:"
ps -o pid,stat,cmd -p $PID 2>/dev/null || echo "Proceso ya no existe"
echo ""
echo "SIGKILL es procesada por el kernel, NO por el proceso"
echo "El proceso no puede capturarla, ignorarla, ni hacer cleanup"
echo "Solo usar como ultimo recurso"
wait $PID 2>/dev/null
'
```

### Paso 1.4: SIGTERM vs SIGKILL — por que importa el orden

```bash
docker compose exec debian-dev bash -c '
echo "=== Orden correcto de terminacion ==="
echo "1. kill PID          (SIGTERM — pedir que termine)"
echo "2. esperar unos segundos"
echo "3. kill -9 PID       (SIGKILL — si no termino, forzar)"
echo ""
echo "=== Incorrecto ==="
echo "kill -9 PID directamente"
echo "  - El proceso no puede hacer cleanup"
echo "  - Archivos temporales quedan sin borrar"
echo "  - Conexiones de red no se cierran limpiamente"
echo "  - Locks no se liberan"
echo "  - Transacciones de base de datos quedan incompletas"
echo ""
echo "SIGKILL es el ultimo recurso, no el primero"
'
```

### Paso 1.5: SIGINT — Ctrl+C

```bash
docker compose exec debian-dev bash -c '
echo "=== SIGINT (2) — equivalente a Ctrl+C ==="
sleep 300 &
PID=$!
echo "Proceso sleep con PID: $PID"

echo ""
echo "=== Enviar SIGINT ==="
kill -INT $PID
sleep 0.5
ps -o pid,stat,cmd -p $PID 2>/dev/null || echo "Proceso terminado por SIGINT"
echo ""
echo "SIGINT se envia a todo el foreground process group"
echo "Ctrl+C en la terminal envia SIGINT al grupo de foreground"
wait $PID 2>/dev/null
'
```

---

## Parte 2 — SIGHUP, SIGSTOP, SIGCONT

### Objetivo

Usar SIGHUP para reload, SIGSTOP para pausar, y SIGCONT para
resumir procesos.

### Paso 2.1: SIGHUP — doble uso

```bash
docker compose exec debian-dev bash -c '
echo "=== SIGHUP (1) — dos significados ==="
echo ""
echo "Significado original:"
echo "  La terminal se cerro (hangup)"
echo "  El proceso pierde su terminal de control"
echo "  Comportamiento default: terminar"
echo ""
echo "Significado moderno (daemons):"
echo "  Recargar configuracion sin reiniciar"
echo "  Ejemplo: kill -HUP \$(pidof nginx)"
echo "  nginx relee nginx.conf sin perder conexiones activas"

echo ""
echo "=== Demostrar SIGHUP termina un proceso normal ==="
sleep 300 &
PID=$!
echo "sleep PID: $PID"
kill -HUP $PID
sleep 0.5
ps -o pid,stat,cmd -p $PID 2>/dev/null || echo "sleep terminado por SIGHUP"
wait $PID 2>/dev/null
'
```

### Paso 2.2: SIGSTOP y SIGCONT — pausar y resumir

```bash
docker compose exec debian-dev bash -c '
echo "=== SIGSTOP (19) — congelar un proceso ==="
sleep 300 &
PID=$!
echo "sleep PID: $PID, estado inicial:"
ps -o pid,stat,cmd -p $PID

echo ""
echo "=== Enviar SIGSTOP ==="
kill -STOP $PID
sleep 0.5
echo "Despues de SIGSTOP:"
ps -o pid,stat,cmd -p $PID
echo "(estado T = stopped)"

echo ""
echo "=== Enviar SIGCONT ==="
kill -CONT $PID
sleep 0.5
echo "Despues de SIGCONT:"
ps -o pid,stat,cmd -p $PID
echo "(vuelve a S = sleeping)"

kill $PID
wait $PID 2>/dev/null
'
```

SIGSTOP no se puede capturar — el kernel congela el proceso
inmediatamente. SIGCONT lo despierta. Util para debugging o
para pausar procesos que consumen demasiados recursos.

### Paso 2.3: SIGTSTP vs SIGSTOP

```bash
docker compose exec debian-dev bash -c '
echo "=== SIGTSTP (20) vs SIGSTOP (19) ==="
echo ""
echo "SIGTSTP:"
echo "  - Lo que envia Ctrl+Z"
echo "  - El proceso PUEDE capturarla e ignorarla"
echo "  - Numero: 20"
echo ""
echo "SIGSTOP:"
echo "  - No se puede capturar ni ignorar"
echo "  - Siempre congela el proceso"
echo "  - Numero: 19"
echo ""
echo "En la practica, Ctrl+Z (SIGTSTP) funciona para la mayoria"
echo "de programas. SIGSTOP es para casos donde SIGTSTP falla."
'
```

### Paso 2.4: Resumen de senales clave

```bash
docker compose exec debian-dev bash -c '
echo "=== Resumen ==="
printf "%-10s %-5s %-12s %-10s %s\n" "Senal" "Num" "Capturar?" "Default" "Uso"
printf "%-10s %-5s %-12s %-10s %s\n" "--------" "---" "----------" "--------" "---"
printf "%-10s %-5s %-12s %-10s %s\n" "SIGHUP"   "1"  "Si"        "Terminar" "Reload config"
printf "%-10s %-5s %-12s %-10s %s\n" "SIGINT"   "2"  "Si"        "Terminar" "Ctrl+C"
printf "%-10s %-5s %-12s %-10s %s\n" "SIGQUIT"  "3"  "Si"        "Core dump" "Ctrl+\\"
printf "%-10s %-5s %-12s %-10s %s\n" "SIGKILL"  "9"  "NO"        "Terminar" "Forzar muerte"
printf "%-10s %-5s %-12s %-10s %s\n" "SIGTERM"  "15" "Si"        "Terminar" "kill PID"
printf "%-10s %-5s %-12s %-10s %s\n" "SIGSTOP"  "19" "NO"        "Pausar"   "Congelar"
printf "%-10s %-5s %-12s %-10s %s\n" "SIGTSTP"  "20" "Si"        "Pausar"   "Ctrl+Z"
printf "%-10s %-5s %-12s %-10s %s\n" "SIGCONT"  "18" "Si"        "Resumir"  "Despertar"
printf "%-10s %-5s %-12s %-10s %s\n" "SIGCHLD"  "17" "Si"        "Ignorar"  "Hijo termino"
printf "%-10s %-5s %-12s %-10s %s\n" "SIGPIPE"  "13" "Si"        "Terminar" "Pipe roto"
'
```

---

## Parte 3 — Senales en contenedores

### Objetivo

Entender como las senales se comportan de forma diferente para
PID 1 dentro de un contenedor.

### Paso 3.1: PID 1 tiene proteccion especial

```bash
docker compose exec debian-dev bash -c '
echo "=== PID 1 en este contenedor ==="
ps -p 1 -o pid,user,cmd
echo ""
echo "=== Intentar enviar SIGTERM a PID 1 ==="
kill -TERM 1 2>&1 || true
echo ""
echo "PID 1 dentro del contenedor:"
ps -p 1 -o pid,stat,cmd
echo ""
echo "El kernel protege PID 1:"
echo "  - No entrega senales que PID 1 no haya registrado un handler"
echo "  - SIGTERM puede ser ignorada si PID 1 no tiene handler"
echo "  - Solo SIGKILL puede matarlo (y eso mata el contenedor)"
'
```

### Paso 3.2: Procesos normales vs PID 1

```bash
docker compose exec debian-dev bash -c '
echo "=== Proceso normal recibe senales normalmente ==="
sleep 300 &
PID=$!
echo "sleep PID: $PID (no es PID 1)"
kill -TERM $PID
sleep 0.5
ps -o pid,stat,cmd -p $PID 2>/dev/null || echo "sleep terminado (esperado)"
wait $PID 2>/dev/null

echo ""
echo "=== PID 1 puede ignorar SIGTERM ==="
echo "PID 1 sigue vivo:"
ps -p 1 -o pid,stat,cmd
echo ""
echo "Esta es la razon por la que docker stop tarda hasta 10s:"
echo "1. docker stop envia SIGTERM"
echo "2. Si PID 1 no tiene handler, la ignora"
echo "3. Docker espera 10s (StopTimeout)"
echo "4. Docker envia SIGKILL"
echo ""
echo "Solucion: docker run --init (usa tini como PID 1)"
'
```

### Paso 3.3: Senales por nombre vs numero

```bash
docker compose exec debian-dev bash -c '
echo "=== Tres formas de enviar SIGTERM ==="
echo "kill -15 PID"
echo "kill -TERM PID"
echo "kill -SIGTERM PID"
echo "(las tres son equivalentes)"

echo ""
echo "=== Demostrar ==="
sleep 300 & P1=$!
sleep 301 & P2=$!
sleep 302 & P3=$!

kill -15 $P1
kill -TERM $P2
kill -SIGTERM $P3
sleep 0.5

echo "Todos terminados:"
ps -o pid,stat,cmd -p $P1,$P2,$P3 2>/dev/null || echo "(ninguno existe)"
wait 2>/dev/null
'
```

---

## Limpieza final

No hay recursos que limpiar.

---

## Conceptos reforzados

1. **SIGTERM** (15) pide terminar limpiamente — el proceso puede
   capturarla y hacer cleanup. Es el default de `kill`.

2. **SIGKILL** (9) es procesada por el kernel — el proceso no la
   puede capturar. Solo usar como ultimo recurso despues de SIGTERM.

3. **SIGSTOP** (19) congela un proceso (no captuable). **SIGCONT**
   (18) lo reanuda. Util para pausar procesos temporalmente.

4. **SIGHUP** (1) tiene doble uso: originalmente "terminal cerrada",
   modernamente "recargar configuracion" en daemons.

5. **PID 1** tiene proteccion especial del kernel: solo recibe
   senales para las que tiene handler registrado. Por eso
   `docker stop` puede tardar 10s antes de enviar SIGKILL.

6. El orden correcto es SIGTERM primero, esperar, SIGKILL solo
   si no responde. Nunca SIGKILL directamente.
