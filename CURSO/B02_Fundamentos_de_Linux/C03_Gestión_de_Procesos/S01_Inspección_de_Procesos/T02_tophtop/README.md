# T02 — top/htop

## top — Monitor de procesos en tiempo real

`top` es la herramienta estándar para monitorear procesos en tiempo real. Viene
preinstalada en todas las distribuciones Linux.

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
   ...
```

### Línea 1 — Resumen del sistema

```
top - 14:23:01 up 2 days,  5:12,  2 users,  load average: 0.52, 0.38, 0.29
       ^hora    ^uptime            ^sesiones  ^carga: 1min, 5min, 15min
```

**Load average**: número promedio de procesos que están ejecutándose o esperando
CPU. La interpretación depende del número de CPUs:

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

### Línea 2 — Tareas

```
Tasks: 231 total,   1 running, 229 sleeping,   0 stopped,   1 zombie
```

Si hay zombies persistentes (> 0 por mucho tiempo), hay un problema: algún
proceso padre no está haciendo `wait()` por sus hijos.

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
- **avail Mem**: estimación de cuánta memoria está realmente disponible para
  nuevas aplicaciones (free + buff/cache recuperable). Este es el número que
  importa, no `free`

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
| %CPU | Uso de CPU **instantáneo** (a diferencia de ps que muestra acumulado) |
| %MEM | Porcentaje de RAM física |
| TIME+ | Tiempo acumulado de CPU (con centésimas) |
| COMMAND | Nombre del comando |

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

**Acciones sobre procesos**:

| Tecla | Acción |
|---|---|
| k | Kill — enviar señal a un proceso (pide PID y señal) |
| r | Renice — cambiar prioridad de un proceso |

**Filtrado**:

| Tecla | Acción |
|---|---|
| u | Filtrar por usuario |
| o | Agregar filtro (ej: `%CPU>10.0`, `COMMAND=nginx`) |
| = | Quitar filtros |

**Otros**:

| Tecla | Acción |
|---|---|
| d | Cambiar intervalo de refresco (default 3s) |
| q | Salir |
| W | Guardar configuración actual en `~/.toprc` |

### top en modo batch

Para scripting o capturar la salida:

```bash
# Una iteración, no interactivo
top -bn1

# Solo los 10 procesos con más CPU
top -bn1 -o %CPU | head -17

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

## htop — top mejorado

`htop` es un reemplazo de `top` con mejor interfaz, soporte de mouse, y más
funcionalidades. No viene preinstalado:

```bash
# Instalar
sudo apt install htop      # Debian
sudo dnf install htop      # RHEL (requiere EPEL)
```

### Ventajas sobre top

| Característica | top | htop |
|---|---|---|
| Scroll horizontal | No | Sí |
| Scroll vertical | Limitado | Completo |
| Soporte de mouse | No | Sí |
| Tree view | Básico (V) | Completo (F5) |
| Buscar proceso | No directamente | F3 |
| Filtrar por texto | Limitado (o) | F4 (texto libre) |
| Matar proceso | k (pide PID) | F9 (seleccionar señal visualmente) |
| Barras de CPU/MEM | Texto | Gráficas con colores |
| Selección múltiple | No | Barra espaciadora |
| Configuración | ~/.toprc | UI interactiva (F2) |

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

Las barras de CPU usan colores para distinguir los tipos de uso:
- **Azul**: baja prioridad (nice)
- **Verde**: usuario normal
- **Rojo**: kernel
- **Cyan**: virtualización (steal)

Las barras de memoria usan colores para:
- **Verde**: memoria usada
- **Azul**: buffers
- **Amarillo/Naranja**: cache

### Teclas de htop

| Tecla | Acción |
|---|---|
| F1 / ? | Ayuda |
| F2 | Setup — configurar columnas, colores, barras |
| F3 / / | Buscar proceso por nombre |
| F4 / \\ | Filtrar — mostrar solo los que coincidan |
| F5 | Tree view (toggle) |
| F6 | Elegir columna de ordenamiento |
| F7 | Disminuir nice (subir prioridad) |
| F8 | Aumentar nice (bajar prioridad) |
| F9 | Kill — menú visual de señales |
| F10 / q | Salir |
| Space | Marcar/desmarcar proceso (para acciones en lote) |
| U | Desmarcar todos |
| t | Tree view (toggle, alternativa a F5) |
| H | Mostrar/ocultar threads de usuario |
| K | Mostrar/ocultar threads del kernel |
| p | Mostrar ruta completa del ejecutable |
| e | Mostrar variables de entorno del proceso seleccionado |
| l | Mostrar archivos abiertos del proceso (lsof) |
| s | Mostrar syscalls del proceso (strace) |
| I | Invertir orden |

### Configuración de htop

```bash
# La configuración se guarda en:
~/.config/htop/htoprc

# O usar F2 dentro de htop para:
# - Elegir qué columnas mostrar
# - Configurar qué barras aparecen arriba
# - Cambiar esquema de colores
# - Cambiar comportamiento del tree view
```

## Cuándo usar cada uno

| Situación | Herramienta |
|---|---|
| Servidor sin paquetes extra | `top` (siempre disponible) |
| Debugging interactivo | `htop` (mejor UX) |
| Capturar datos para análisis | `top -bn1` (modo batch) |
| Ver jerarquía de procesos | `htop` F5 o `pstree` |
| Dashboard en producción | `top` (no requiere instalar nada) |
| Scripts de monitoreo | `top -bn1` + parsing, o mejor `/proc` directamente |

## Alternativas modernas

Existen herramientas más recientes que cubren el mismo espacio:

| Herramienta | Nota |
|---|---|
| `btop` | TUI avanzada con gráficos, CPU/MEM/NET/DISK |
| `glances` | Monitor todo-en-uno, exporta a JSON/CSV |
| `atop` | Logging de recursos, puede reproducir historial |

Estas son útiles pero no reemplazan el conocimiento de `top`/`htop`, que están
disponibles en prácticamente cualquier servidor.

---

## Ejercicios

### Ejercicio 1 — Explorar top

```bash
# Abrir top
top

# Dentro de top:
# 1. Presionar 1 → ¿cuántas CPUs tienes?
# 2. Presionar M → ordenar por memoria
# 3. Presionar c → mostrar comandos completos
# 4. Presionar V → vista de árbol
# 5. Presionar u → filtrar por tu usuario
# 6. Presionar = → quitar filtro
# 7. Presionar q → salir
```

### Ejercicio 2 — top en modo batch

```bash
# Los 5 procesos con más CPU (una sola captura)
top -bn1 -o %CPU | head -12

# Los 5 procesos con más memoria
top -bn1 -o %MEM | head -12

# Interpretar las líneas de resumen:
# ¿Cuál es tu load average?
# ¿Cuánta RAM disponible tienes realmente? (avail Mem, no free)
# ¿Tienes procesos zombie?
```

### Ejercicio 3 — htop interactivo

```bash
# Instalar si no está
which htop || sudo apt install htop  # o sudo dnf install htop

htop

# Dentro de htop:
# 1. F5 → activar tree view
# 2. F3 → buscar "bash"
# 3. F4 → filtrar por "systemd"
# 4. F6 → ordenar por %MEM
# 5. Seleccionar un proceso → presionar e → ver su entorno
# 6. Seleccionar un proceso → presionar l → ver archivos abiertos
# 7. q → salir
```
