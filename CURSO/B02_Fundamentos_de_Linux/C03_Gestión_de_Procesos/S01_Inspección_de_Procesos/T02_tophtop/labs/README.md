# Lab — top/htop

## Objetivo

Monitorear procesos en tiempo real: interpretar las 5 lineas de
cabecera de top (load average, tareas, CPU, memoria, swap), usar
comandos interactivos, modo batch para scripting, y htop como
alternativa moderna.

## Prerequisitos

- Entorno del curso levantado (`docker compose up -d`)

---

## Parte 1 — Cabecera de top

### Objetivo

Interpretar cada linea de la cabecera de top para entender el
estado del sistema.

### Paso 1.1: Capturar una instantanea de top

```bash
docker compose exec debian-dev bash -c '
echo "=== Instantanea de top (modo batch, 1 iteracion) ==="
top -bn1 | head -12
'
```

Las primeras 5 lineas son la cabecera. Cada una muestra un aspecto
del sistema.

### Paso 1.2: Linea 1 — uptime y load average

```bash
docker compose exec debian-dev bash -c '
echo "=== Linea 1: uptime ==="
top -bn1 | head -1
echo ""
echo "Campos:"
echo "  top - HH:MM:SS  hora actual"
echo "  up X days/min   tiempo encendido"
echo "  N users          sesiones activas"
echo "  load average: X, Y, Z"
echo "    X = carga ultimo 1 minuto"
echo "    Y = carga ultimos 5 minutos"
echo "    Z = carga ultimos 15 minutos"

echo ""
echo "=== Comparar con uptime ==="
uptime

echo ""
echo "=== Load average directamente ==="
cat /proc/loadavg
echo "Campos: 1min 5min 15min running/total last_pid"
'
```

El load average indica cuantos procesos estan en estado R (running)
o D (disk sleep) en promedio. En un sistema con N CPUs, un load de
N significa 100% de utilizacion.

### Paso 1.3: Linea 2 — tareas

```bash
docker compose exec debian-dev bash -c '
echo "=== Linea 2: tareas ==="
top -bn1 | sed -n "2p"
echo ""
echo "Campos:"
echo "  total    — procesos en el sistema"
echo "  running  — en CPU o cola (estado R)"
echo "  sleeping — esperando evento (estado S)"
echo "  stopped  — detenidos con SIGSTOP/Ctrl+Z (estado T)"
echo "  zombie   — terminados sin wait() del padre (estado Z)"

echo ""
echo "=== Verificar con ps ==="
echo "Total:    $(ps -e --no-headers | wc -l)"
echo "Running:  $(ps -eo stat --no-headers | grep -c "^R")"
echo "Sleeping: $(ps -eo stat --no-headers | grep -c "^S")"
echo "Stopped:  $(ps -eo stat --no-headers | grep -c "^T")"
echo "Zombie:   $(ps -eo stat --no-headers | grep -c "^Z")"
'
```

### Paso 1.4: Linea 3 — CPU breakdown

```bash
docker compose exec debian-dev bash -c '
echo "=== Linea 3: CPU ==="
top -bn1 | sed -n "3p"
echo ""
echo "Campos (% de CPU):"
echo "  us — user space (procesos del usuario)"
echo "  sy — system/kernel (llamadas al sistema)"
echo "  ni — nice (procesos con prioridad alterada)"
echo "  id — idle (CPU sin usar)"
echo "  wa — wait I/O (esperando disco/red)"
echo "  hi — hardware interrupts"
echo "  si — software interrupts"
echo "  st — steal (tiempo robado por hypervisor, VMs)"

echo ""
echo "=== Que importa ==="
echo "us + sy = uso real de CPU"
echo "id alto = CPU libre"
echo "wa alto = cuello de botella en disco"
echo "st alto = el hypervisor esta sobrecargado"
'
```

### Paso 1.5: Lineas 4-5 — memoria

```bash
docker compose exec debian-dev bash -c '
echo "=== Lineas 4-5: memoria ==="
top -bn1 | sed -n "4,5p"
echo ""
echo "Linea 4 (RAM):"
echo "  total   — RAM total del sistema"
echo "  free    — RAM completamente libre"
echo "  used    — RAM usada por procesos"
echo "  buff/cache — cache del kernel (disponible si se necesita)"
echo ""
echo "Linea 5 (Swap):"
echo "  total — swap total"
echo "  free  — swap libre"
echo "  used  — swap usada (si es alta, falta RAM)"
echo "  avail Mem — RAM realmente disponible (free + cache recuperable)"

echo ""
echo "=== Comparar con free ==="
free -h
echo ""
echo "available es lo que importa, no free"
echo "(el kernel usa RAM libre para cache, y la devuelve cuando se necesita)"
'
```

---

## Parte 2 — top interactivo y batch

### Objetivo

Usar top en modo interactivo con comandos de teclado, y en modo
batch para capturar datos automaticamente.

### Paso 2.1: Comandos interactivos clave

```bash
docker compose exec debian-dev bash -c '
echo "=== Comandos interactivos de top ==="
echo "(ejecutar top manualmente y probar cada uno)"
echo ""
echo "Ordenar:"
echo "  P — ordenar por CPU (default)"
echo "  M — ordenar por memoria"
echo "  T — ordenar por tiempo acumulado"
echo "  N — ordenar por PID"
echo ""
echo "Filtrar:"
echo "  u — filtrar por usuario"
echo "  o — filtrar por campo (ej: COMMAND=sleep)"
echo "  = — quitar filtros"
echo ""
echo "Vista:"
echo "  1 — mostrar cada CPU por separado"
echo "  H — mostrar threads individuales"
echo "  c — mostrar path completo del comando"
echo "  V — vista de arbol (padre-hijo)"
echo ""
echo "Acciones:"
echo "  k — enviar senal a un proceso (kill)"
echo "  r — cambiar prioridad (renice)"
echo "  q — salir"
echo ""
echo "  d o s — cambiar intervalo de refresco"
echo "  W — guardar configuracion"
'
```

Para probar estos comandos, ejecuta `docker compose exec -it
debian-dev top` en tu terminal.

### Paso 2.2: Modo batch para scripting

```bash
docker compose exec debian-dev bash -c '
echo "=== Modo batch: -bn1 ==="
echo "(1 iteracion, sin interactividad, para scripting)"
echo ""
echo "=== Top 5 procesos por CPU ==="
top -bn1 -o %CPU | tail -n +8 | head -5

echo ""
echo "=== Top 5 procesos por memoria ==="
top -bn1 -o %MEM | tail -n +8 | head -5
'
```

`-b` activa modo batch (salida lineal, sin colores, sin
interactividad). `-n1` limita a 1 iteracion. `-o %CPU` ordena
por CPU.

### Paso 2.3: Capturar multiples iteraciones

```bash
docker compose exec debian-dev bash -c '
echo "=== 3 iteraciones con intervalo de 1 segundo ==="
echo "(util para ver tendencias)"
top -bn3 -d1 | grep "^%Cpu"
echo ""
echo "Cada linea %Cpu es una iteracion"
echo "La primera refleja el promedio desde el arranque"
echo "Las siguientes son intervalos reales de 1 segundo"
'
```

### Paso 2.4: Extraer informacion especifica

```bash
docker compose exec debian-dev bash -c '
echo "=== Extraer solo load average ==="
top -bn1 | head -1 | grep -oP "load average: .*"

echo ""
echo "=== Extraer uso de memoria ==="
top -bn1 | grep "MiB Mem"

echo ""
echo "=== Procesos de root ordenados por RSS ==="
top -bn1 -u root | tail -n +8 | head -5
echo "(filtrar por usuario con -u en batch)"
'
```

---

## Parte 3 — htop

### Objetivo

Explorar htop como alternativa moderna a top, con mejor interfaz
y funcionalidades adicionales.

### Paso 3.1: Verificar disponibilidad

```bash
docker compose exec debian-dev bash -c '
echo "=== htop disponible? ==="
which htop 2>/dev/null && htop --version | head -1 || echo "htop no instalado"

echo ""
echo "=== Diferencias clave con top ==="
echo "1. Barras de CPU/memoria en colores"
echo "2. Scroll vertical y horizontal"
echo "3. Mouse funcional (click para ordenar, seleccionar)"
echo "4. Busqueda con / (no solo filtro)"
echo "5. Vista de arbol activada con F5"
echo "6. Matar proceso con F9 (menu de senales)"
echo "7. Configuracion visual con F2"
'
```

### Paso 3.2: htop en modo no interactivo

```bash
docker compose exec debian-dev bash -c '
if command -v htop &>/dev/null; then
    echo "=== htop: captura de procesos ==="
    htop --no-color --batch-mode 2>/dev/null | head -20 \
        || htop -C 2>/dev/null | head -20 \
        || echo "(htop no soporta batch mode en esta version)"
else
    echo "htop no esta instalado"
    echo "En un sistema real: apt install htop (Debian) o dnf install htop (RHEL)"
fi
'
```

### Paso 3.3: Teclas de funcion de htop

```bash
docker compose exec debian-dev bash -c '
echo "=== Teclas de funcion de htop ==="
echo "F1  — Ayuda"
echo "F2  — Setup (configurar columnas, colores, metros)"
echo "F3  — Buscar proceso por nombre (resalta)"
echo "F4  — Filtrar (muestra solo los que coinciden)"
echo "F5  — Vista de arbol"
echo "F6  — Ordenar por columna"
echo "F7  — Bajar prioridad (nice +1)"
echo "F8  — Subir prioridad (nice -1)"
echo "F9  — Matar proceso (menu de senales)"
echo "F10 — Salir"

echo ""
echo "=== Navegacion ==="
echo "Flechas     — mover cursor"
echo "Space       — marcar proceso (para operaciones multiples)"
echo "/ o F3      — buscar"
echo "\\ o F4      — filtrar"
echo "t o F5      — toggle vista de arbol"
echo "p           — toggle ruta completa del programa"
echo "H           — toggle threads"
'
```

### Paso 3.4: Comparacion practica

```bash
docker compose exec debian-dev bash -c '
echo "=== Comparacion top vs htop ==="
printf "%-20s %-20s %-20s\n" "Aspecto" "top" "htop"
printf "%-20s %-20s %-20s\n" "----------" "----------" "----------"
printf "%-20s %-20s %-20s\n" "Instalacion" "Siempre disponible" "Requiere instalar"
printf "%-20s %-20s %-20s\n" "Colores" "Basicos" "Rico, configurable"
printf "%-20s %-20s %-20s\n" "Mouse" "No" "Si"
printf "%-20s %-20s %-20s\n" "Scroll" "No" "Si (vertical y hor)"
printf "%-20s %-20s %-20s\n" "Arbol" "V (algunas ver)" "F5 (siempre)"
printf "%-20s %-20s %-20s\n" "Batch mode" "-bn1 (robusto)" "Limitado"
printf "%-20s %-20s %-20s\n" "Para scripts" "Preferido" "No recomendado"
printf "%-20s %-20s %-20s\n" "Para humanos" "Funcional" "Preferido"
'
```

Para diagnostico interactivo, htop. Para scripting y automatizacion,
`top -bn1`.

---

## Limpieza final

No hay recursos que limpiar.

---

## Conceptos reforzados

1. La **cabecera de top** tiene 5 lineas: uptime/load, tareas,
   CPU breakdown (us/sy/ni/id/wa/hi/si/st), RAM, y swap. Cada una
   revela un aspecto diferente del sistema.

2. El **load average** indica procesos en R o D. Compararlo con el
   numero de CPUs: load > CPUs = sistema sobrecargado.

3. En el breakdown de CPU, `id` es CPU libre, `wa` indica I/O
   lento, `st` indica problemas de hypervisor.

4. **`top -bn1`** es la forma estandar de capturar datos de top
   para scripting. `-b` = batch, `-n1` = una iteracion.

5. htop ofrece mejor experiencia interactiva (colores, mouse,
   scroll, arbol), pero top esta siempre disponible y es mas
   robusto para automatizacion.

6. `available` en la linea de memoria es mas relevante que `free`,
   porque el kernel usa RAM libre para cache y la devuelve cuando
   se necesita.
