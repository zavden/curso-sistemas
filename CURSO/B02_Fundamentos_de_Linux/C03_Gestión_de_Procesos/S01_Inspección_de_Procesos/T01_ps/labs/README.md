# Lab — ps

## Objetivo

Inspeccionar procesos con ps: comparar los formatos BSD (aux) y
System V (-ef), interpretar codigos de estado STAT, usar formato
personalizado con -o, y filtrar procesos con selectores.

## Prerequisitos

- Entorno del curso levantado (`docker compose up -d`)

---

## Parte 1 — ps aux vs ps -ef

### Objetivo

Comparar los dos formatos principales de ps y entender que campos
muestra cada uno.

### Paso 1.1: ps aux (formato BSD)

```bash
docker compose exec debian-dev bash -c '
echo "=== ps aux (formato BSD) ==="
ps aux | head -5

echo ""
echo "Campos:"
echo "USER  PID %CPU %MEM   VSZ   RSS TTY STAT START TIME COMMAND"
echo "  |    |    |    |     |     |    |    |    |    |      |"
echo "  |    |    |    |     |     |    |    |    |    |      comando completo"
echo "  |    |    |    |     |     |    |    |    |    CPU acumulada"
echo "  |    |    |    |     |     |    |    |    hora de inicio"
echo "  |    |    |    |     |     |    |    estado (STAT)"
echo "  |    |    |    |     |     |    terminal"
echo "  |    |    |    |     |     memoria fisica (KB)"
echo "  |    |    |    |     memoria virtual (KB)"
echo "  |    |    |    % de RAM"
echo "  |    |    % de CPU"
echo "  |    process ID"
echo "  usuario"
'
```

`ps aux` muestra **todos los procesos** de **todos los usuarios**
sin necesidad de terminal. `a` = todos los usuarios, `u` = formato
orientado al usuario, `x` = incluir procesos sin terminal.

### Paso 1.2: ps -ef (formato System V)

```bash
docker compose exec debian-dev bash -c '
echo "=== ps -ef (formato System V) ==="
ps -ef | head -5

echo ""
echo "Campos:"
echo "UID   PID  PPID  C STIME TTY  TIME     CMD"
echo " |     |    |    |   |    |     |        |"
echo " |     |    |    |   |    |     |        comando"
echo " |     |    |    |   |    |     CPU acumulada"
echo " |     |    |    |   |    terminal"
echo " |     |    |    |   hora de inicio"
echo " |     |    |    uso de CPU (%)"
echo " |     |    PID del padre"
echo " |     process ID"
echo " usuario"
'
```

`ps -ef` incluye el **PPID** (PID del padre) que `ps aux` no muestra.
`-e` = todos los procesos, `-f` = formato completo.

### Paso 1.3: Diferencia clave — PPID

```bash
docker compose exec debian-dev bash -c '
echo "=== ps aux NO muestra PPID ==="
ps aux | head -1
echo "(no hay columna PPID)"

echo ""
echo "=== ps -ef SI muestra PPID ==="
ps -ef | head -1
echo "(PPID esta presente)"

echo ""
echo "=== Ejemplo: quien es el padre de cada proceso ==="
ps -ef | head -10
echo ""
echo "El PPID permite reconstruir el arbol de procesos"
'
```

### Paso 1.4: Sin opciones — solo tu sesion

```bash
docker compose exec debian-dev bash -c '
echo "=== ps sin opciones ==="
ps
echo ""
echo "Solo muestra procesos de tu terminal actual"
echo "(mucho mas limitado que aux o -ef)"
'
```

### Paso 1.5: Contar procesos

Antes de ejecutar, predice: en un contenedor basico,
cuantos procesos esperas ver?

```bash
docker compose exec debian-dev bash -c '
echo "=== Total de procesos ==="
echo "ps aux: $(ps aux --no-headers | wc -l) procesos"
echo "ps -ef: $(ps -ef --no-headers | wc -l) procesos"
echo ""
echo "(ambos deben mostrar el mismo numero)"
'
```

En un contenedor hay pocos procesos comparado con un host.

---

## Parte 2 — Codigos STAT

### Objetivo

Interpretar los codigos de estado que ps muestra en la columna STAT.

### Paso 2.1: Estados basicos

```bash
docker compose exec debian-dev bash -c '
echo "=== Codigo STAT: primera letra ==="
echo "R  — Running (ejecutando o en cola de ejecucion)"
echo "S  — Sleeping (esperando un evento, interruptible)"
echo "D  — Disk sleep (I/O, no se puede interrumpir)"
echo "Z  — Zombie (termino pero el padre no hizo wait)"
echo "T  — Stopped (detenido con Ctrl+Z o SIGSTOP)"
echo "t  — Tracing stop (detenido por un debugger)"
echo "I  — Idle (kernel thread inactivo)"

echo ""
echo "=== Procesos actuales por estado ==="
ps aux --no-headers | awk "{print \$8}" | cut -c1 | sort | uniq -c | sort -rn
'
```

La mayoria de procesos estan en estado S (sleeping). Es normal —
los procesos pasan la mayor parte del tiempo esperando eventos.

### Paso 2.2: Modificadores de STAT

```bash
docker compose exec debian-dev bash -c '
echo "=== Modificadores (segunda letra+) ==="
echo "s  — session leader (lider de sesion)"
echo "l  — multi-threaded (tiene threads)"
echo "+  — foreground process group"
echo "<  — alta prioridad (nice negativo)"
echo "N  — baja prioridad (nice positivo)"
echo "L  — paginas locked en memoria"

echo ""
echo "=== Ejemplos reales ==="
ps aux | awk "NR==1 || \$8 ~ /s/ || \$8 ~ /l/ || \$8 ~ /\\+/" | head -15
'
```

### Paso 2.3: Crear procesos en diferentes estados

```bash
docker compose exec debian-dev bash -c '
echo "=== Crear proceso sleeping (S) ==="
sleep 300 &
SLEEP_PID=$!
ps -o pid,stat,cmd -p $SLEEP_PID
echo "(sleep esta en estado S — esperando que pase el tiempo)"

echo ""
echo "=== Crear proceso stopped (T) ==="
sleep 301 &
STOP_PID=$!
kill -STOP $STOP_PID
ps -o pid,stat,cmd -p $STOP_PID
echo "(despues de SIGSTOP esta en estado T)"

echo ""
echo "=== Resumir y verificar ==="
kill -CONT $STOP_PID
ps -o pid,stat,cmd -p $STOP_PID
echo "(despues de SIGCONT vuelve a S)"

kill $SLEEP_PID $STOP_PID 2>/dev/null
wait 2>/dev/null
'
```

### Paso 2.4: Ver el proceso de ps mismo

```bash
docker compose exec debian-dev bash -c '
echo "=== Estado de ps mientras ejecuta ==="
ps aux | grep "[p]s aux"
echo ""
echo "ps se muestra a si mismo en estado R (running)"
echo "(es el unico proceso que seguro esta en R en ese instante)"
'
```

El truco `[p]s` en grep evita que grep se muestre a si mismo en
los resultados.

---

## Parte 3 — Formato personalizado y selectores

### Objetivo

Usar -o para mostrar solo las columnas necesarias y selectores
para filtrar procesos especificos.

### Paso 3.1: Formato personalizado con -o

```bash
docker compose exec debian-dev bash -c '
echo "=== Solo PID, PPID, estado, y comando ==="
ps -eo pid,ppid,stat,cmd | head -10

echo ""
echo "=== Con etiquetas personalizadas ==="
ps -eo pid,ppid,stat,user,cmd --sort=-pcpu | head -10
echo "(ordenado por uso de CPU descendente)"
'
```

`-o` permite elegir exactamente que columnas mostrar. Se combina
con `-e` (todos los procesos) o con selectores.

### Paso 3.2: Selectores de procesos

```bash
docker compose exec debian-dev bash -c '
echo "=== Por usuario (-u) ==="
ps -u root -o pid,user,stat,cmd | head -10
echo ""
echo "(solo procesos del usuario root)"

echo ""
echo "=== Por nombre de comando (-C) ==="
sleep 200 &
SPID=$!
ps -C sleep -o pid,ppid,cmd
echo "(busca por nombre exacto del ejecutable)"
kill $SPID 2>/dev/null

echo ""
echo "=== Por PID (-p) ==="
ps -p 1 -o pid,ppid,user,stat,cmd
echo "(informacion de un PID especifico)"
'
```

### Paso 3.3: Ordenar resultados

```bash
docker compose exec debian-dev bash -c '
echo "=== Por uso de memoria (descendente) ==="
ps -eo pid,user,vsz,rss,cmd --sort=-rss | head -10
echo "(--sort=-rss ordena por RSS descendente)"

echo ""
echo "=== Por PID (ascendente) ==="
ps -eo pid,ppid,cmd --sort=pid | head -10
echo "(--sort=pid ordena por PID ascendente)"
'
```

El prefijo `-` indica orden descendente. Sin prefijo es ascendente.

### Paso 3.4: VSZ vs RSS

```bash
docker compose exec debian-dev bash -c '
echo "=== VSZ vs RSS ==="
ps -eo pid,vsz,rss,cmd --sort=-rss | head -10

echo ""
echo "VSZ (Virtual Size):"
echo "  Memoria virtual total solicitada por el proceso"
echo "  Incluye: codigo, datos, librerias, stack, heap, mmap"
echo "  Incluye paginas no cargadas en RAM"
echo ""
echo "RSS (Resident Set Size):"
echo "  Memoria fisica realmente usada"
echo "  Solo paginas en RAM (no swap, no compartidas contabilizadas por proceso)"
echo ""
echo "VSZ siempre >= RSS"
echo "RSS es lo que importa para saber cuanta RAM consume un proceso"
'
```

### Paso 3.5: pstree

```bash
docker compose exec debian-dev bash -c '
echo "=== pstree (arbol de procesos) ==="
pstree -p | head -20
echo ""
echo "(-p muestra PIDs)"

echo ""
echo "=== pstree de PID 1 ==="
pstree -p 1 | head -15
echo "(arbol desde el proceso raiz)"

echo ""
echo "=== pstree con argumentos ==="
pstree -pa 1 | head -15
echo "(-a muestra argumentos de los comandos)"
'
```

`pstree` muestra la relacion padre-hijo de forma visual.
Es equivalente a reconstruir la jerarquia usando PPID de `ps -ef`.

### Paso 3.6: Diferencias entre Debian y AlmaLinux

```bash
docker compose exec debian-dev bash -c '
echo "=== Debian: procesos de PID 1 ==="
ps -p 1 -o pid,user,cmd
echo ""
echo "=== Debian: version de ps ==="
ps --version 2>&1 | head -1
'
```

```bash
docker compose exec alma-dev bash -c '
echo "=== AlmaLinux: procesos de PID 1 ==="
ps -p 1 -o pid,user,cmd
echo ""
echo "=== AlmaLinux: version de ps ==="
ps --version 2>&1 | head -1
'
```

El comportamiento de ps es identico en ambas distribuciones
(procps-ng). Las diferencias estan en que procesos estan corriendo,
no en la herramienta.

---

## Limpieza final

No hay recursos que limpiar.

---

## Conceptos reforzados

1. `ps aux` (BSD) muestra USER, %CPU, %MEM pero no PPID.
   `ps -ef` (System V) muestra PPID. Se complementan.

2. La columna **STAT** codifica el estado del proceso: S (sleeping)
   es lo normal, R (running) es raro de ver, Z (zombie) y D (disk
   sleep) indican problemas potenciales.

3. Los **modificadores** de STAT (`s`, `l`, `+`, `<`) aportan
   informacion adicional: lider de sesion, multi-threaded,
   foreground, alta prioridad.

4. `ps -eo campo1,campo2 --sort=-campo` permite seleccionar
   exactamente las columnas y el orden. Es la forma mas util
   para diagnostico.

5. **VSZ** es memoria virtual solicitada, **RSS** es memoria
   fisica usada. RSS es lo relevante para monitoreo.

6. `pstree -p` muestra la jerarquia padre-hijo de forma visual,
   equivalente a seguir los PPID de `ps -ef`.
