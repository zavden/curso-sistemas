# T02 — top/htop

## Errata y notas sobre el material original

1. **labs F7/F8 invertidos**: El lab dice "F7 — Bajar prioridad (nice +1)"
   y "F8 — Subir prioridad (nice -1)". Es al revés: F7 **disminuye** nice
   (sube prioridad), F8 **aumenta** nice (baja prioridad). Los README.md
   y README.max.md lo tienen correcto.

2. **labs paso 3.2 `htop --batch-mode`**: Esta flag no existe en htop.
   htop no tiene un modo batch robusto como `top -bn1`. Para captura
   automática, usar `top -bn1`.

3. **max.md tabla comparativa top↔htop**: Mapea `u` (filtrar por
   usuario en top) a `F4` (filtrar por texto en htop). No son
   equivalentes — `F4` filtra por cadena de texto, no por usuario.
   La equivalencia real: en htop se filtra por usuario con `u`.

4. **max.md ejercicio 6**: `htop -d 0 -C --tree` — la flag `--tree`
   no existe como opción CLI de htop. Tree view solo se activa
   interactivamente con F5 o `t`.

5. **max.md ejercicio 9**: Dice "Observa %CPU en top" pero nunca
   ejecuta top. El proceso se crea y se mata en 2 segundos sin
   forma de observar nada.

6. **max.md ejercicio 8**: `tail -n +7` incluye la línea de cabecera
   de columnas. Para obtener solo datos, usar `tail -n +8`
   (7 líneas de cabecera: 5 resumen + 1 vacía + 1 columnas).

---

## top — Monitor de procesos en tiempo real

`top` es la herramienta estándar para monitorear procesos en tiempo real.
Viene preinstalada en todas las distribuciones Linux.

### La pantalla de top

```
top - 14:23:01 up 2 days,  5:12,  2 users,  load average: 0.52, 0.38, 0.29
Tasks: 231 total,   1 running, 229 sleeping,   0 stopped,   1 zombie
%Cpu(s):  3.2 us,  1.1 sy,  0.0 ni, 95.4 id,  0.2 wa,  0.0 hi,  0.1 si,  0.0 st
MiB Mem :  15921.4 total,   8234.2 free,   4521.8 used,   3165.4 buff/cache
MiB Swap:   8192.0 total,   8192.0 free,      0.0 used.  10876.3 avail Mem

    PID USER      PR  NI    VIRT    RES    SHR S  %CPU  %MEM     TIME+ COMMAND
   1842 dev       20   0  437284  42816  31024 S   2.3   0.3   0:15.42 gnome-shell
    502 root      20   0  112456  21344  15680 S   0.7   0.1   0:08.23 systemd-journal
```

La cabecera tiene 5 líneas, cada una revela un aspecto diferente del sistema.

### Línea 1 — Resumen del sistema

```
top - 14:23:01 up 2 days,  5:12,  2 users,  load average: 0.52, 0.38, 0.29
       ^hora    ^uptime            ^sesiones  ^carga: 1min, 5min, 15min
```

**Load average**: número promedio de procesos en estado R (running) o D
(uninterruptible sleep/disk wait) en promedio. La interpretación depende
del número de CPUs:

```
4 CPUs → load average 4.0 = 100% utilización
4 CPUs → load average 8.0 = sobrecargado, cola de espera
4 CPUs → load average 1.0 = 25% utilización

Regla general: load average / nº CPUs
  < 0.7 → sistema holgado
  ~ 1.0 → utilización óptima
  > 1.0 → hay cola de espera (puede ser normal si es breve)

# Ver número de CPUs
nproc
grep -c ^processor /proc/cpuinfo
```

> **Dato clave**: Load average alto con CPU% bajo indica I/O wait
> (procesos en estado D esperando disco). Load average alto con CPU%
> alto indica saturación real de CPU.

La tendencia importa: comparar los 3 valores (1, 5, 15 minutos).
Si 1min > 5min > 15min, la carga está subiendo. Si 1min < 15min,
está bajando.

### Línea 2 — Tareas

```
Tasks: 231 total,   1 running, 229 sleeping,   0 stopped,   1 zombie
```

Si hay zombies persistentes (> 0 por mucho tiempo), hay un problema:
algún proceso padre no está haciendo `wait()` por sus hijos.

### Línea 3 — CPU

```
%Cpu(s):  3.2 us,  1.1 sy,  0.0 ni, 95.4 id,  0.2 wa,  0.0 hi,  0.1 si,  0.0 st
```

| Campo | Significado |
|---|---|
| us | User space — programas del usuario |
| sy | System — código del kernel (syscalls, interrupciones) |
| ni | Nice — procesos con nice modificado |
| id | Idle — CPU sin uso |
| wa | I/O Wait — CPU esperando a que termine I/O de disco |
| hi | Hardware interrupts |
| si | Software interrupts |
| st | Steal — tiempo robado por el hypervisor (en VMs) |

Los más relevantes en la práctica:
- **us** alto → la aplicación consume CPU (optimizar código)
- **sy** alto → muchas syscalls o context switches (revisar I/O patterns)
- **wa** alto → el disco es el cuello de botella (I/O lento, disco saturado)
- **st** alto → la VM no recibe suficiente CPU del host (redimensionar o migrar)

Todos los campos suman 100%. La relación práctica:
- `us + sy` = uso real de CPU
- `id` alto = CPU libre
- Si `id` es bajo pero `wa` es alto, el problema no es CPU sino disco

```bash
# Presionar 1 dentro de top para ver cada CPU individual
# Útil para detectar procesos single-threaded saturando un solo core
```

### Línea 4-5 — Memoria

```
MiB Mem :  15921.4 total,   8234.2 free,   4521.8 used,   3165.4 buff/cache
MiB Swap:   8192.0 total,   8192.0 free,      0.0 used.  10876.3 avail Mem
```

- **buff/cache**: memoria usada por el kernel para cachear disco. Se puede
  recuperar si una aplicación la necesita — no es memoria "usada"
- **avail Mem**: estimación de cuánta memoria está realmente disponible
  para nuevas aplicaciones (free + buff/cache recuperable). **Este es el
  número que importa**, no `free`
- **Swap used > 0**: no siempre es problema. El kernel puede mover páginas
  inactivas a swap preventivamente. Swap used **y creciendo** sí indica
  presión de memoria

### Columnas de procesos

| Columna | Significado |
|---|---|
| PID | Process ID |
| USER | Usuario |
| PR | Prioridad del kernel (generalmente 20 + NI) |
| NI | Nice value (-20 a 19). Menor = más prioridad |
| VIRT | Memoria virtual total (≈ VSZ de ps) |
| RES | Memoria residente en RAM (≈ RSS de ps) |
| SHR | Memoria compartida (libs, shared memory) |
| S | Estado: R=running, S=sleeping, D=disk sleep, Z=zombie, T=stopped |
| %CPU | Uso de CPU (ver nota sobre primera iteración) |
| %MEM | Porcentaje de RAM física |
| TIME+ | Tiempo acumulado de CPU (con centésimas) |
| COMMAND | Nombre del comando |

**PR y NI**: La prioridad del kernel (PR) generalmente es `20 + NI`.
Un proceso con `NI = 0` tiene `PR = 20`. Pero si PR muestra `rt`, el
proceso tiene prioridad **real-time** (gestionada con `chrt`, no con
`nice`/`renice`).

**%CPU — primera iteración vs siguientes**: En `top -bn1`, el %CPU de
la **primera iteración** es el promedio acumulado desde que arrancó el
proceso (igual que `ps aux`). Las iteraciones siguientes muestran el
uso real durante el intervalo de muestreo. Por eso para scripting se
suelen usar al menos 2 iteraciones: `top -bn2 -d1` y descartar la primera.

### Comandos interactivos de top

Dentro de top, estas teclas son los controles más usados:

**Ordenamiento**:

| Tecla | Ordena por |
|---|---|
| P | %CPU (default) |
| M | %MEM |
| T | TIME+ |
| N | PID |
| R | Invertir el orden |

**Visualización**:

| Tecla | Acción |
|---|---|
| 1 | Mostrar cada CPU individual (toggle) |
| H | Mostrar threads individuales (toggle) |
| c | Mostrar comando completo con argumentos (toggle) |
| V | Tree view — vista de árbol (toggle) |
| f | Seleccionar qué columnas mostrar |
| x | Resaltar columna de ordenamiento |
| z | Colores (toggle) |
| E | Cambiar unidades de memoria en cabecera (KiB→MiB→GiB) |
| e | Cambiar unidades de memoria en lista de procesos |

**Acciones sobre procesos**:

| Tecla | Acción |
|---|---|
| k | Kill — enviar señal a un proceso (pide PID y señal) |
| r | Renice — cambiar prioridad de un proceso |

**Filtrado**:

| Tecla | Acción |
|---|---|
| u / U | Filtrar por usuario (u = nombre, U = UID también) |
| o | Agregar filtro (ej: `%CPU>10.0`, `COMMAND=nginx`) |
| = | Quitar filtros |
| L | Localizar cadena de texto en la pantalla |

**Otros**:

| Tecla | Acción |
|---|---|
| d / s | Cambiar intervalo de refresco (default 3s) |
| q | Salir |
| W | Guardar configuración actual en `~/.toprc` |

### top en modo batch

Para scripting o capturar la salida:

```bash
# Una iteración, no interactivo
top -bn1

# Top 5 procesos por CPU (saltar 7 líneas de cabecera)
top -bn1 -o %CPU | tail -n +8 | head -5

# Descartar primera iteración (acumulada) y usar la segunda (intervalo real)
top -bn2 -d1 | grep -A 999 '^top -' | tail -n +2 | head -12

# Capturar 5 iteraciones con intervalo de 2 segundos
top -bn5 -d2 > top-capture.txt

# Procesos de un usuario específico
top -bn1 -u dev
```

### Configuración persistente

```bash
# Dentro de top, configurar como se desee, luego presionar W
# Guarda en ~/.toprc (o ~/.config/procps/toprc en versiones nuevas)

# Iniciar top con opciones específicas
top -d 1                    # refresco cada 1 segundo
top -p 1842,502             # solo estos PIDs
top -H -p 1842              # threads de un proceso específico
```

---

## htop — top mejorado

`htop` es un reemplazo de `top` con mejor interfaz, soporte de mouse, y
más funcionalidades. No viene preinstalado:

```bash
# Instalar
sudo apt install htop      # Debian/Ubuntu
sudo dnf install htop      # Fedora/RHEL
```

### Ventajas sobre top

| Característica | top | htop |
|---|---|---|
| Scroll horizontal | No | Sí |
| Scroll vertical | Limitado | Completo |
| Soporte de mouse | No | Sí (click en cabeceras para ordenar) |
| Tree view | V (básico) | F5 (completo, con plegado) |
| Buscar proceso | L (localizar texto) | F3 (buscar por nombre) |
| Filtrar por texto | o (sintaxis especial) | F4 (texto libre) |
| Filtrar por usuario | u | u |
| Matar proceso | k (pide PID) | F9 (menú visual de señales) |
| Barras de CPU/MEM | Texto porcentual | Gráficas con colores |
| Selección múltiple | No | Barra espaciadora |
| Configuración | ~/.toprc + W | F2 (UI interactiva) |
| Batch mode para scripts | `-bn1` (robusto) | No tiene batch mode real |

### Pantalla de htop

```
  0[||||||                   12.5%]   Tasks: 83, 1 thr; 1 running
  1[||||                      8.3%]   Load average: 0.52 0.38 0.29
  2[||                        4.1%]   Uptime: 2 days, 05:12:34
  3[|                         2.0%]
  Mem[||||||||||||       3.42G/15.5G]
  Swp[                    0K/8.00G]

    PID USER     PRI  NI  VIRT   RES   SHR S CPU%  MEM%   TIME+  Command
   1842 dev       20   0  437M 42.8M 31.0M S  2.3   0.3  0:15.42 /usr/bin/gnome-shell
    502 root      20   0  112M 21.3M 15.6M S  0.7   0.1  0:08.23 systemd-journald
```

### Colores de las barras

Las barras de **CPU** usan colores para distinguir tipos de uso:

| Color | Significado |
|---|---|
| Azul | Baja prioridad (nice > 0) |
| Verde | Usuario normal (us) |
| Rojo | Kernel (sy) |
| Cyan/Celeste | Virtualización (steal) |

Las barras de **memoria** usan:

| Color | Significado |
|---|---|
| Verde | Memoria usada por procesos |
| Azul | Buffers |
| Amarillo/Naranja | Cache |

> Saber leer los colores es clave: si la barra de CPU es
> mayoritariamente roja, el kernel está trabajando mucho
> (posible I/O intensivo). Si es verde, los procesos de
> usuario consumen la CPU.

### Teclas de htop

| Tecla | Acción |
|---|---|
| F1 / ? | Ayuda |
| F2 | Setup — configurar columnas, colores, barras |
| F3 / / | Buscar proceso por nombre (resalta, navega con F3) |
| F4 / \\ | Filtrar — mostrar solo los que coincidan |
| F5 | Tree view (toggle) |
| F6 | Elegir columna de ordenamiento |
| F7 | Disminuir nice → subir prioridad |
| F8 | Aumentar nice → bajar prioridad |
| F9 | Kill — menú visual de señales |
| F10 / q | Salir |

| Tecla | Acción |
|---|---|
| Space | Marcar/desmarcar proceso (para acciones en lote) |
| U | Desmarcar todos |
| u | Filtrar por usuario |
| t | Tree view (alternativa a F5) |
| H | Mostrar/ocultar threads de usuario |
| K | Mostrar/ocultar threads del kernel |
| p | Mostrar ruta completa del ejecutable |
| c | Tag del proceso y sus hijos |
| e | Mostrar variables de entorno del proceso |
| l | Mostrar archivos abiertos (lsof) |
| s | Mostrar syscalls (strace) |
| I | Invertir orden |

### Configuración de htop

```bash
# La configuración se guarda en:
~/.config/htop/htoprc

# O usar F2 dentro de htop para:
# - Elegir qué columnas mostrar
# - Configurar qué barras aparecen arriba (CPU, memoria, swap, etc.)
# - Cambiar esquema de colores
# - Cambiar comportamiento del tree view
```

---

## Cuándo usar cada uno

| Situación | Herramienta | Por qué |
|---|---|---|
| Servidor sin paquetes extra | `top` | Siempre disponible |
| Debugging interactivo | `htop` | Mejor UX, mouse, scroll |
| Capturar datos para análisis | `top -bn1` | Modo batch robusto |
| Ver jerarquía de procesos | `htop` F5 o `pstree` | Mejor visualización |
| Scripts de monitoreo | `top -bn1` + awk | O leer `/proc` directamente |
| Dashboard en producción | `top` | No requiere instalar nada |

## Alternativas modernas

| Herramienta | Nota |
|---|---|
| `btop` | TUI avanzada con gráficos, CPU/MEM/NET/DISK |
| `glances` | Monitor todo-en-uno, exporta a JSON/CSV/InfluxDB |
| `atop` | Logging de recursos con historial reproducible |

Estas son útiles pero no reemplazan el conocimiento de `top`/`htop`,
que están disponibles en prácticamente cualquier servidor.

---

## Labs

### Parte 1 — Cabecera de top

#### 1.1 Capturar una instantánea

```bash
echo "=== Instantánea de top (modo batch, 1 iteración) ==="
top -bn1 | head -12
```

Las primeras 5 líneas son la cabecera. Cada una muestra un aspecto
diferente del sistema.

#### 1.2 Línea 1 — uptime y load average

```bash
echo "=== Línea 1: uptime ==="
top -bn1 | head -1
echo ""
echo "Campos:"
echo "  top - HH:MM:SS  hora actual"
echo "  up X days/min   tiempo encendido"
echo "  N users          sesiones activas"
echo "  load average: X, Y, Z (1min, 5min, 15min)"

echo ""
echo "=== Comparar con uptime ==="
uptime

echo ""
echo "=== Load average desde /proc ==="
cat /proc/loadavg
echo "Campos: 1min 5min 15min running/total last_pid"
```

#### 1.3 Línea 2 — tareas

```bash
echo "=== Línea 2: tareas ==="
top -bn1 | sed -n '2p'
echo ""
echo "=== Verificar con ps ==="
echo "Total:    $(ps -e --no-headers | wc -l)"
echo "Running:  $(ps -eo stat --no-headers | grep -c '^R')"
echo "Sleeping: $(ps -eo stat --no-headers | grep -c '^S')"
echo "Stopped:  $(ps -eo stat --no-headers | grep -c '^T')"
echo "Zombie:   $(ps -eo stat --no-headers | grep -c '^Z')"
```

#### 1.4 Línea 3 — CPU breakdown

```bash
echo "=== Línea 3: CPU ==="
top -bn1 | sed -n '3p'
echo ""
echo "us + sy = uso real de CPU"
echo "id alto = CPU libre"
echo "wa alto = cuello de botella en disco"
echo "st alto = el hypervisor está sobrecargado"
```

#### 1.5 Líneas 4-5 — memoria

```bash
echo "=== Líneas 4-5: memoria ==="
top -bn1 | sed -n '4,5p'
echo ""
echo "=== Comparar con free ==="
free -h
echo ""
echo "El campo 'available' es lo que importa, no 'free'"
echo "(el kernel usa RAM libre para cache y la devuelve cuando se necesita)"
```

---

### Parte 2 — top interactivo y batch

#### 2.1 Modo batch para scripting

```bash
echo "=== Top 5 procesos por CPU ==="
top -bn1 -o %CPU | tail -n +8 | head -5

echo ""
echo "=== Top 5 procesos por memoria ==="
top -bn1 -o %MEM | tail -n +8 | head -5
```

`-b` activa modo batch. `-n1` limita a 1 iteración. `-o %CPU` ordena.
`tail -n +8` salta las 7 líneas de cabecera (5 resumen + 1 vacía +
1 columnas).

#### 2.2 Capturar múltiples iteraciones

```bash
echo "=== 3 iteraciones con intervalo de 1 segundo ==="
top -bn3 -d1 | grep "^%Cpu"
echo ""
echo "La primera línea refleja el promedio desde el arranque."
echo "Las siguientes son intervalos reales de 1 segundo."
```

#### 2.3 Extraer información específica

```bash
echo "=== Load average ==="
cat /proc/loadavg

echo ""
echo "=== Uso de memoria ==="
top -bn1 | grep "MiB Mem"

echo ""
echo "=== Procesos de root, top 5 por CPU ==="
top -bn1 -u root | tail -n +8 | head -5
```

#### 2.4 Filtros con -o en modo interactivo

Para probar los comandos interactivos, ejecutar `top` directamente
(no en batch) y:

```
P         → ordenar por %CPU
M         → ordenar por %MEM
1         → CPUs individuales
u + root  → solo procesos de root
o + %CPU>5.0 → solo procesos con >5% CPU
=         → quitar filtros
c         → comando completo
V         → tree view
q         → salir
```

---

### Parte 3 — htop

#### 3.1 Verificar disponibilidad

```bash
echo "=== htop disponible? ==="
if command -v htop &>/dev/null; then
    htop --version | head -1
else
    echo "htop no instalado"
    echo "Instalar: apt install htop (Debian) o dnf install htop (RHEL)"
fi
```

#### 3.2 Colores de las barras

Abrir `htop` interactivamente y observar:

```
CPU: Azul=nice, Verde=user, Rojo=kernel, Cyan=steal
MEM: Verde=usada, Azul=buffers, Amarillo=cache
```

Estas barras dan un diagnóstico visual instantáneo.

#### 3.3 Teclas de función

```
F1  — Ayuda
F2  — Setup (configurar columnas, colores, metros)
F3  — Buscar proceso (resalta y navega)
F4  — Filtrar (muestra solo los que coinciden)
F5  — Vista de árbol
F6  — Ordenar por columna
F7  — Disminuir nice (subir prioridad)
F8  — Aumentar nice (bajar prioridad)
F9  — Matar proceso (menú de señales)
F10 — Salir
```

#### 3.4 Exploración interactiva

Abrir `htop` y practicar:

```
1. F5 → activar tree view
2. F3 → buscar "bash"
3. F4 → filtrar por "sleep"
4. F6 → ordenar por %MEM
5. Seleccionar un proceso → e → ver su entorno
6. Seleccionar un proceso → l → ver archivos abiertos
7. u → filtrar por un usuario específico
8. q → salir
```

---

## Ejercicios

### Ejercicio 1 — Interpretar la cabecera de top

```bash
top -bn1 | head -5
nproc
```

Responder:
1. ¿Cuántas CPUs tiene el sistema?
2. ¿Cuál es el load average de 1 minuto? ¿Está el sistema sobrecargado?
3. ¿Hay procesos zombie?
4. ¿Cuánto CPU está idle (`id`)?
5. ¿Cuánta memoria está realmente disponible? (pista: no es `free`)

<details><summary>Predicción</summary>

**¿Qué esperas ver?**

1. El número de CPUs lo da `nproc`. Si el load average de 1 min es
   menor que ese número, el sistema no está sobrecargado.

2. Zombies aparecen en la línea 2 (Tasks). En un contenedor simple,
   normalmente son 0.

3. `id` en la línea 3 muestra el % de CPU libre. En un sistema
   inactivo suele estar por encima del 90%.

4. La memoria disponible está en la línea 5: `avail Mem`. Es mayor
   que `free` porque incluye buff/cache recuperable. Si `free -h`
   muestra `free: 200M` pero `available: 12G`, hay 12G realmente
   disponibles.

**Verificación:**
```bash
echo "CPUs: $(nproc)"
echo "Load 1min: $(awk '{print $1}' /proc/loadavg)"
echo "Available: $(awk '/MemAvailable/ {printf "%.0f MiB\n", $2/1024}' /proc/meminfo)"
```

</details>

---

### Ejercicio 2 — Primera iteración vs siguientes

```bash
# Crear un proceso CPU-bound
(while true; do :; done) &
PID_BURN=$!
sleep 1

echo "=== Primera iteración (acumulada desde que arrancó) ==="
top -bn1 -p $PID_BURN | tail -n +8

echo ""
echo "=== Segunda iteración (intervalo real de 1 segundo) ==="
top -bn2 -d1 -p $PID_BURN | tail -1

kill $PID_BURN 2>/dev/null
```

<details><summary>Predicción</summary>

**¿Qué esperas ver?**

La primera iteración de `top -bn1` calcula %CPU como el tiempo de CPU
acumulado dividido por el tiempo total que el proceso lleva corriendo.
Si el proceso lleva solo 1 segundo, será cercano al 100%. Pero si
llevara mucho tiempo, sería un promedio de toda su vida.

La segunda iteración (con `-bn2 -d1`) mide %CPU solo durante el último
intervalo de 1 segundo. Para un `while true; do :; done`, debería
mostrar ~100% (o ~99.x%) porque consume un core entero.

**Punto clave**: Para scripting, si quieres el uso de CPU "ahora",
usa `top -bn2 -d1` y descarta la primera iteración. `top -bn1` te da
el promedio histórico (como `ps aux`).

</details>

---

### Ejercicio 3 — CPU breakdown: generar carga

```bash
echo "=== Estado base (sin carga) ==="
top -bn1 | sed -n '3p'
echo ""

echo "=== Generando carga de usuario (us) ==="
(while true; do :; done) &
PID_CPU=$!
sleep 2
top -bn2 -d1 | grep "^%Cpu" | tail -1
kill $PID_CPU 2>/dev/null

echo ""
echo "=== Generando carga I/O (wa) ==="
dd if=/dev/zero of=/tmp/test_io bs=1M count=100 oflag=sync 2>/dev/null &
PID_IO=$!
sleep 2
top -bn2 -d1 | grep "^%Cpu" | tail -1
wait $PID_IO 2>/dev/null
rm -f /tmp/test_io
```

<details><summary>Predicción</summary>

**¿Qué esperas ver?**

1. **Estado base**: `id` cercano a 100%, todo lo demás cercano a 0%.

2. **Carga user**: `us` sube significativamente (hasta ~25% en 4 CPUs,
   ~100%/N_CPUs). `id` baja proporcionalmente. `sy`, `wa` permanecen
   bajos.

3. **Carga I/O**: `wa` sube (CPU esperando a que el disco escriba).
   `sy` también puede subir (syscalls de write). `us` permanece bajo.
   En un contenedor, `wa` puede no ser muy alto si el almacenamiento
   es rápido (SSD/overlay).

**Diagnóstico práctico**: Si un servidor tiene `us` alto → optimizar
la aplicación. Si tiene `wa` alto → el problema es el disco, no la
CPU. Si tiene `st` alto → la VM necesita más recursos del host.

</details>

---

### Ejercicio 4 — Memoria: free vs available

```bash
echo "=== top muestra ==="
top -bn1 | sed -n '4,5p'
echo ""
echo "=== free muestra ==="
free -h
echo ""
echo "=== /proc/meminfo (campos clave) ==="
grep -E '^(MemTotal|MemFree|MemAvailable|Buffers|Cached|SwapTotal|SwapFree):' /proc/meminfo
```

Responder:
1. ¿Cuánta memoria aparece como `free` en top?
2. ¿Cuánta como `avail Mem`?
3. ¿Por qué `available` > `free`?
4. ¿Se está usando swap? ¿Es preocupante?

<details><summary>Predicción</summary>

**¿Qué esperas ver?**

`available` es siempre mayor o igual que `free`. La diferencia es la
memoria de buff/cache que el kernel puede recuperar si una aplicación
la necesita.

Ejemplo: si `free = 500 MiB` y `avail = 12000 MiB`, hay ~11.5 GiB
de cache recuperable. El sistema tiene memoria de sobra.

**Sobre swap**: swap used > 0 no siempre es problema. El kernel puede
mover páginas inactivas preventivamente. Es preocupante cuando:
- `swap used` está creciendo continuamente
- `avail Mem` es bajo (< 10% del total)
- El sistema está haciendo "swap thrashing" (swap in/out constante)

Los 3 valores (`free`, `avail`, y `buffers+cache`) vienen de
`/proc/meminfo`. `top` y `free` leen la misma fuente.

</details>

---

### Ejercicio 5 — top batch: parsing para scripts

```bash
# Extraer los 3 procesos con más RSS (memoria residente)
echo "=== Top 3 por memoria residente ==="
top -bn1 -o RES | tail -n +8 | head -3
echo ""

# Extraer solo el load average
echo "=== Load average ==="
awk '{printf "1min: %s  5min: %s  15min: %s\n", $1, $2, $3}' /proc/loadavg
echo ""

# Sumar el %CPU de todos los procesos
echo "=== %CPU total ==="
top -bn1 | tail -n +8 | awk '{sum += $9} END {printf "Total: %.1f%%\n", sum}'
echo ""

# Contar procesos por estado
echo "=== Procesos por estado ==="
top -bn1 | tail -n +8 | awk '{count[$8]++} END {for (s in count) print s": "count[s]}' | sort
```

<details><summary>Predicción</summary>

**¿Qué esperas ver?**

- **Top 3 por RES**: Los procesos que más RAM física ocupan. `-o RES`
  ordena por memoria residente (descendente por defecto).

- **Load average**: Los 3 números de `/proc/loadavg`. Más directo que
  parsear la salida de `top` o `uptime`.

- **%CPU total**: La columna $9 es %CPU (la 9ª columna en la salida de
  top). En un sistema con 4 CPUs, el máximo teórico es 400%.
  `tail -n +8` salta las 7 líneas de cabecera.

- **Procesos por estado**: La columna $8 es S (estado). Verás
  mayoritariamente `S` (sleeping), quizás algún `R` (running),
  y con suerte `I` (idle kernel thread).

</details>

---

### Ejercicio 6 — Comandos interactivos de top

Ejecutar `top` interactivamente y seguir esta secuencia:

```
1. Observar el orden por defecto (P = %CPU)
2. Presionar M → ¿cambia el proceso en primer lugar?
3. Presionar T → ¿quién ha acumulado más CPU?
4. Presionar 1 → ¿cuántas CPUs aparecen?
5. Presionar c → ¿qué cambia en la columna COMMAND?
6. Presionar V → ¿quién es padre de quién?
7. Presionar u → escribir "root" → ¿cuántos procesos tiene root?
8. Presionar = → volver a ver todos
9. Presionar o → escribir "%CPU>1.0" → ¿cuántos quedan?
10. Presionar = → quitar filtro
11. Presionar d → escribir 1 → ¿el refresco es más rápido?
12. Presionar q → salir
```

<details><summary>Predicción</summary>

**¿Qué esperas ver?**

1. **P**: El proceso con más %CPU arriba. En un sistema inactivo,
   probablemente `top` mismo.

2. **M**: El proceso con más %MEM arriba. Suele ser diferente del de
   más CPU (a menos que haya un proceso que consuma ambos).

3. **T**: Procesos que llevan más tiempo ejecutándose y han acumulado
   más CPU. Suelen ser servicios de larga duración.

4. **1**: Muestra una línea `%Cpu` por cada core. El número de líneas
   = número de CPUs del sistema (o las asignadas al contenedor).

5. **c**: COMMAND pasa de mostrar solo el nombre (`bash`) a la línea
   completa con argumentos (`/bin/bash --login`).

6. **V**: Indentación que muestra padre → hijo. PID 1 (init/systemd)
   aparece como raíz.

7. **u + root**: Solo procesos de root. Típicamente son servicios del
   sistema.

8-12. Los filtros se aplican/quitan. `d 1` cambia el intervalo a 1
   segundo (refresco más frecuente).

</details>

---

### Ejercicio 7 — htop: exploración interactiva

```bash
# Verificar que htop está disponible
command -v htop && echo "htop disponible" || echo "htop no instalado — instalar con: apt/dnf install htop"
```

Si htop está disponible, ejecutar `htop` y:

```
1. Observar las barras de CPU:
   - ¿Cuántos cores se muestran?
   - ¿De qué color es la mayoría? (Verde=user, Rojo=kernel)

2. Observar la barra de memoria:
   - ¿Cuánto es verde (usado) vs amarillo (cache)?
   - ¿Hay swap en uso?

3. F5 → activar tree view
   - ¿Quién es el padre de tu shell actual?
   - ¿Hay procesos con muchos hijos?

4. F3 → buscar tu shell (escribir "bash" o "zsh")
   - ¿Cuántas instancias encuentras?

5. F4 → filtrar por "top"
   - ¿htop se muestra a sí mismo?
   - Presionar Esc para quitar el filtro

6. F6 → ordenar por PERCENT_MEM
   - ¿Cuál es el proceso que más RAM usa?

7. Seleccionar un proceso con las flechas → e
   - ¿Qué variables de entorno tiene? (PATH, HOME, USER...)

8. q → salir
```

<details><summary>Predicción</summary>

**¿Qué esperas ver?**

1. **Barras CPU**: Número de barras = `nproc`. En un sistema inactivo,
   las barras están casi vacías (mayoritariamente gris/vacío).

2. **Barra memoria**: Amarillo (cache) puede ser una parte
   significativa. Esto es normal — el kernel cachea datos de disco
   para acelerar acceso.

3. **Tree view**: La jerarquía muestra init/systemd → servicios → shells.
   Tu bash/zsh aparece como hijo de sshd, docker exec, o similar.

4. **Buscar**: F3 resalta la primera coincidencia y navega con F3
   sucesivos. Diferente de F4 que filtra (oculta no-coincidencias).

5. **Filtrar por "top"**: htop se muestra a sí mismo (su COMMAND
   contiene "htop" que incluye "top"). Esto demuestra que F4 hace
   substring matching.

6. **Ordenar por MEM**: Muestra qué consume más RAM. Los procesos
   de sistema suelen usar relativamente poca memoria.

7. **Variables de entorno (e)**: Cada proceso hereda su entorno del
   padre. Verás PATH, HOME, LANG, etc.

</details>

---

### Ejercicio 8 — Monitorear un proceso específico

```bash
# Crear un proceso para monitorear
sleep 300 &
PID_SLEEP=$!

echo "PID del sleep: $PID_SLEEP"
echo ""

echo "=== top: monitorear solo este PID ==="
top -bn1 -p $PID_SLEEP | tail -n +7

echo ""
echo "=== top: monitorear múltiples PIDs ==="
top -bn1 -p $PID_SLEEP,$$ | tail -n +7

kill $PID_SLEEP 2>/dev/null
```

Ahora abrir `htop` y buscar el proceso:
```
1. Crear: sleep 300 &
2. Abrir htop
3. F3 → buscar "sleep"
4. Observar PID, USER, %CPU, %MEM, STATE
5. F9 → ver menú de señales (no enviar ninguna, Esc para cancelar)
6. q → salir
7. kill %1  (matar el sleep desde el shell)
```

<details><summary>Predicción</summary>

**¿Qué esperas ver?**

**En top con `-p PID`**: Solo muestra los procesos especificados.
El `sleep` tendrá:
- %CPU: 0.0 (sleep no consume CPU)
- %MEM: ~0.0 (sleep es mínimo)
- S: S (sleeping — esperando que pase el tiempo)
- RES: muy bajo (unas decenas de KiB)

**Con `-p PID1,PID2`**: Muestra ambos. `$$` es el PID del shell
actual, que también estará en estado S (esperando input).

**En htop con F3**: Resalta "sleep" y puedes navegar entre
coincidencias. F9 muestra un menú visual de señales (SIGTERM,
SIGKILL, SIGSTOP, etc.) — más intuitivo que `k` de top que pide
escribir el número de señal.

</details>

---

### Ejercicio 9 — PR, NI y prioridad

```bash
echo "=== Procesos con nice != 0 ==="
top -bn1 | tail -n +8 | awk '$7 != 0 && $7 != "NI" {print $1, $2, $6, $7, $12}'
echo "(columnas: PID USER PR NI COMMAND)"
echo ""

echo "=== Crear proceso con nice alto (baja prioridad) ==="
nice -n 10 sleep 60 &
PID_NICE=$!

echo "=== Crear proceso con nice normal ==="
sleep 60 &
PID_NORMAL=$!

echo ""
echo "=== Comparar en top ==="
top -bn1 -p $PID_NICE,$PID_NORMAL | tail -n +7

echo ""
echo "PID $PID_NICE → nice 10 (baja prioridad, PR debería ser 30)"
echo "PID $PID_NORMAL → nice 0 (prioridad normal, PR debería ser 20)"

kill $PID_NICE $PID_NORMAL 2>/dev/null

echo ""
echo "=== Procesos real-time (PR = rt) ==="
ps -eo pid,cls,rtprio,ni,comm --sort=-rtprio | head -10
echo "(cls: TS=normal, FF=FIFO, RR=round-robin)"
```

<details><summary>Predicción</summary>

**¿Qué esperas ver?**

La relación entre NI y PR es: **PR = 20 + NI**

- `nice -n 10 sleep` → NI=10, PR=30 (menos prioridad que el default)
- `sleep` normal → NI=0, PR=20 (prioridad default)

Cuanto más alto el número de NI/PR, **menos** prioridad tiene el
proceso. Rango de NI: -20 (máxima prioridad) a +19 (mínima).

**Procesos real-time**: Algunos procesos del kernel tienen PR=`rt`.
Estos tienen prioridad absoluta sobre cualquier proceso normal.
Se identifican con `ps -eo cls` como FF (FIFO) o RR (Round Robin)
en vez de TS (Time Sharing).

Solo root puede bajar nice (subir prioridad): `nice -n -5 command`.
Cualquier usuario puede subir nice (bajar prioridad) de sus propios
procesos.

</details>

---

### Ejercicio 10 — top dentro vs fuera del contenedor

```bash
echo "=== Desde FUERA del contenedor ==="
echo "Procesos totales: $(ps -e --no-headers | wc -l)"
echo "CPUs:             $(nproc)"
echo "Memoria total:    $(awk '/MemTotal/ {printf "%.0f MiB", $2/1024}' /proc/meminfo)"
echo "Load average:     $(cat /proc/loadavg)"
echo ""

echo "=== Desde DENTRO del contenedor ==="
echo "(si estás en Docker, estos valores pueden diferir)"
echo ""
echo "top -bn1 muestra:"
top -bn1 | head -5
echo ""

echo "=== Detalle ==="
echo "- Load average: compartido con el host (mismo kernel)"
echo "- Memoria: en kernels < 4.14, muestra la del HOST, no la del contenedor"
echo "  (cgroup-aware en kernels recientes y versiones nuevas de procps)"
echo "- CPUs: puede mostrar solo las asignadas al cgroup"
echo "- Procesos: solo los del namespace PID del contenedor"
```

<details><summary>Predicción</summary>

**¿Qué esperas ver?**

El contenedor comparte el kernel con el host, así que:

| Recurso | ¿Ve el host o el contenedor? |
|---|---|
| Load average | Host (un solo kernel, un solo scheduler) |
| Memoria (top viejo) | Host (lee /proc/meminfo del host) |
| Memoria (top nuevo) | Contenedor (cgroup-aware, procps ≥ 3.3.15) |
| CPUs | Depende de la configuración de cgroup |
| Procesos | Solo los del PID namespace del contenedor |

**Implicación práctica**: Si `top` dentro de un contenedor muestra
16 GiB de RAM pero el contenedor tiene un límite de 512 MiB, no te
fíes. Verificar con:
```bash
cat /sys/fs/cgroup/memory/memory.limit_in_bytes  # cgroups v1
cat /sys/fs/cgroup/memory.max                     # cgroups v2
```

htop en versiones 3.x+ es cgroup-aware y muestra correctamente los
límites del contenedor.

</details>
