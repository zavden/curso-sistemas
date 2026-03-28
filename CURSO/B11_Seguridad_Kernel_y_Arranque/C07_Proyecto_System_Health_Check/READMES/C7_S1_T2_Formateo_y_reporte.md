# Formateo y Reporte — Salida Coloreada, Umbrales de Alerta y Resumen Ejecutivo

## Índice

1. [Objetivo: de datos crudos a reporte visual](#1-objetivo-de-datos-crudos-a-reporte-visual)
2. [Códigos de escape ANSI para colores](#2-códigos-de-escape-ansi-para-colores)
3. [Sistema de umbrales](#3-sistema-de-umbrales)
4. [Barras de progreso en terminal](#4-barras-de-progreso-en-terminal)
5. [Formato de tabla alineada](#5-formato-de-tabla-alineada)
6. [Resumen ejecutivo](#6-resumen-ejecutivo)
7. [Implementación completa: health_report.sh](#7-implementación-completa-health_reportsh)
8. [Detección de terminal y fallback](#8-detección-de-terminal-y-fallback)
9. [Errores comunes](#9-errores-comunes)
10. [Cheatsheet](#10-cheatsheet)
11. [Ejercicios](#11-ejercicios)

---

## 1. Objetivo: de datos crudos a reporte visual

En T01 construimos la recolección de datos. Ahora transformamos esos datos crudos en un reporte que un sysadmin pueda leer de un vistazo y saber si hay problemas.

```
Datos crudos (T01):                    Reporte visual (T02):

ram|16384000|6144000|10240000|37       ┌─────────────────────────────────┐
swap|4194304|98304|4096000|2           │  MEMORY                        │
                                       │  RAM:  [████████░░░░░░░] 37%   │
                                       │        5.9G / 15.6G   OK      │
                                       │  Swap: [░░░░░░░░░░░░░░░]  2%  │
                                       │        96M / 4.0G      OK     │
                                       └─────────────────────────────────┘
```

### Principios de diseño del reporte

```
1. ESCANEABLE en 3 segundos
   Un vistazo = saber si hay problemas
   Colores: verde OK, amarillo WARN, rojo CRIT

2. PROGRESIVO en detalle
   Resumen arriba → detalles abajo
   Si todo está verde, no necesitas leer más

3. CONSISTENTE
   Mismo formato para disco, memoria, load
   Mismos umbrales, mismos indicadores

4. ROBUSTO
   Funciona sin colores si no hay terminal (pipe, cron)
   Anchos fijos para alineación predecible
```

---

## 2. Códigos de escape ANSI para colores

### Fundamentos

Los terminales modernos interpretan secuencias de escape ANSI para aplicar colores y estilos. La secuencia comienza con `\033[` (o `\e[`) y termina con `m`.

```
Formato: \033[<código>m

Reset:       \033[0m       Vuelve al color normal
Negrita:     \033[1m
Tenue:       \033[2m
Subrayado:   \033[4m
```

### Colores de texto (foreground)

```
Color       Normal    Bright/Bold
──────────────────────────────────
Negro       \033[30m  \033[90m
Rojo        \033[31m  \033[91m
Verde       \033[32m  \033[92m
Amarillo    \033[33m  \033[93m
Azul        \033[34m  \033[94m
Magenta     \033[35m  \033[95m
Cyan        \033[36m  \033[96m
Blanco      \033[37m  \033[97m
```

### Definir variables de color en un script

```bash
# Colores como variables (patrón estándar)
if [ -t 1 ]; then
    # stdout es un terminal → usar colores
    RED='\033[0;31m'
    GREEN='\033[0;32m'
    YELLOW='\033[1;33m'
    BLUE='\033[0;34m'
    CYAN='\033[0;36m'
    BOLD='\033[1m'
    DIM='\033[2m'
    NC='\033[0m'    # No Color (reset)
else
    # stdout no es un terminal (pipe, archivo) → sin colores
    RED=''
    GREEN=''
    YELLOW=''
    BLUE=''
    CYAN=''
    BOLD=''
    DIM=''
    NC=''
fi
```

### Uso

```bash
echo -e "${RED}ERROR${NC}: disk full"
echo -e "${GREEN}[OK]${NC}    All services running"
echo -e "${YELLOW}[WARN]${NC}  Disk at 85%"
echo -e "${BOLD}=== System Health ===${NC}"
```

> **Pregunta de predicción**: ¿Por qué verificamos `[ -t 1 ]` antes de usar colores? ¿Qué pasaría si enviamos códigos ANSI a un archivo o por un pipe?

Los códigos ANSI son caracteres de control que solo los terminales interpretan como colores. Si redirigimos a un archivo (`script.sh > log.txt`) o por un pipe (`script.sh | mail`), los códigos aparecen como basura literal: `[0;31mERROR[0m: disk full`. El test `[ -t 1 ]` ("¿el file descriptor 1 —stdout— es un terminal?") permite desactivar colores automáticamente cuando la salida no va a un terminal.

---

## 3. Sistema de umbrales

### Definir umbrales por métrica

```bash
# Umbrales configurables al inicio del script
DISK_WARN=80        # Porcentaje
DISK_CRIT=90
MEM_WARN=80
MEM_CRIT=95
SWAP_WARN=50
SWAP_CRIT=80
LOAD_WARN_FACTOR=1  # load > CPUs × factor
LOAD_CRIT_FACTOR=2
```

### Función de evaluación de severidad

```bash
# Retorna: ok, warn, crit
check_threshold() {
    local value=$1
    local warn=$2
    local crit=$3

    if [ "$value" -ge "$crit" ]; then
        echo "crit"
    elif [ "$value" -ge "$warn" ]; then
        echo "warn"
    else
        echo "ok"
    fi
}

# Uso:
# check_threshold 85 80 90   → warn
# check_threshold 92 80 90   → crit
# check_threshold 45 80 90   → ok
```

### Colorear según severidad

```bash
colorize() {
    local severity=$1
    local text=$2

    case "$severity" in
        ok)   echo -e "${GREEN}${text}${NC}" ;;
        warn) echo -e "${YELLOW}${text}${NC}" ;;
        crit) echo -e "${RED}${text}${NC}" ;;
        *)    echo "$text" ;;
    esac
}

# Indicadores con etiqueta
status_label() {
    local severity=$1

    case "$severity" in
        ok)   echo -e "${GREEN}  OK  ${NC}" ;;
        warn) echo -e "${YELLOW} WARN ${NC}" ;;
        crit) echo -e "${RED} CRIT ${NC}" ;;
    esac
}
```

### Evaluación de load average (valores decimales)

```bash
check_load() {
    local load=$1
    local cpus=$2

    awk -v l="$load" -v c="$cpus" -v wf="$LOAD_WARN_FACTOR" -v cf="$LOAD_CRIT_FACTOR" \
        'BEGIN {
            if (l >= c * cf) print "crit"
            else if (l >= c * wf) print "warn"
            else print "ok"
        }'
}

# Uso:
# check_load 2.5 4   → ok   (2.5 < 4×1)
# check_load 5.0 4   → warn (5.0 >= 4×1 pero < 4×2)
# check_load 9.0 4   → crit (9.0 >= 4×2)
```

---

## 4. Barras de progreso en terminal

### Barra de progreso básica

```bash
progress_bar() {
    local percent=$1
    local width=${2:-20}       # Ancho de la barra en caracteres
    local severity=${3:-ok}

    local filled=$(( percent * width / 100 ))
    local empty=$(( width - filled ))

    # Caracteres de la barra
    local fill_char="█"
    local empty_char="░"

    # Color según severidad
    local color=""
    case "$severity" in
        ok)   color="$GREEN" ;;
        warn) color="$YELLOW" ;;
        crit) color="$RED" ;;
    esac

    # Construir la barra
    local bar=""
    for ((i = 0; i < filled; i++)); do bar+="$fill_char"; done
    for ((i = 0; i < empty; i++)); do bar+="$empty_char"; done

    echo -e "${color}[${bar}]${NC} ${percent}%"
}

# Uso:
# progress_bar 37 20 ok     → [███████░░░░░░░░░░░░░] 37%   (verde)
# progress_bar 85 20 warn   → [█████████████████░░░] 85%   (amarillo)
# progress_bar 95 20 crit   → [███████████████████░] 95%   (rojo)
```

### Barra con printf (sin bucle)

```bash
progress_bar_fast() {
    local percent=$1
    local width=${2:-20}
    local severity=${3:-ok}

    local filled=$(( percent * width / 100 ))
    local empty=$(( width - filled ))

    local color=""
    case "$severity" in
        ok)   color="$GREEN" ;;
        warn) color="$YELLOW" ;;
        crit) color="$RED" ;;
    esac

    # printf con repetición: %Ns imprime N caracteres
    local fill_str=$(printf '%*s' "$filled" '' | tr ' ' '█')
    local empty_str=$(printf '%*s' "$empty" '' | tr ' ' '░')

    printf "${color}[%s%s]${NC} %3d%%" "$fill_str" "$empty_str" "$percent"
}
```

### Barra con ASCII simple (máxima compatibilidad)

```bash
# Para terminales que no soportan Unicode
progress_bar_ascii() {
    local percent=$1
    local width=${2:-20}

    local filled=$(( percent * width / 100 ))
    local empty=$(( width - filled ))

    local fill_str=$(printf '%*s' "$filled" '' | tr ' ' '#')
    local empty_str=$(printf '%*s' "$empty" '' | tr ' ' '-')

    printf "[%s%s] %3d%%" "$fill_str" "$empty_str" "$percent"
}

# [########------------] 40%
```

---

## 5. Formato de tabla alineada

### printf para alineación

```bash
# printf con anchos fijos garantiza columnas alineadas
printf "%-15s %8s %8s %8s %5s  %s\n" "Mount" "Size" "Used" "Avail" "Use%" "Status"
printf "%-15s %8s %8s %8s %5s  %s\n" "─────────────" "────────" "────────" "────────" "─────" "──────"
printf "%-15s %8s %8s %8s %4d%%  %s\n" "/" "50G" "42G" "5.4G" 84 "$(status_label warn)"
printf "%-15s %8s %8s %8s %4d%%  %s\n" "/home" "449G" "200G" "226G" 44 "$(status_label ok)"

# Resultado:
# Mount             Size     Used    Avail  Use%  Status
# ─────────────  ────────  ────────  ────────  ─────  ──────
# /                 50G      42G     5.4G    84%   WARN
# /home            449G     200G     226G    44%    OK
```

### Formato de especificación printf

```
%-15s     String alineado a la izquierda, 15 caracteres de ancho
%8s       String alineado a la derecha, 8 caracteres
%4d       Entero, 4 caracteres de ancho
%3d%%     Entero de 3 dígitos seguido de literal %
%.1f      Float con 1 decimal
```

### Box drawing (marcos)

```bash
# Caracteres Unicode para marcos
# ┌─┐  Esquinas: ┌ ┐ └ ┘
# │ │  Vertical: │
# └─┘  Horizontal: ─
# ├─┤  T-junctions: ├ ┤ ┬ ┴
# ┼    Cruz: ┼

print_box_header() {
    local title="$1"
    local width=${2:-50}
    local padding=$(( (width - ${#title} - 2) / 2 ))

    printf "┌"
    printf '%*s' "$width" '' | tr ' ' '─'
    printf "┐\n"

    printf "│"
    printf '%*s' "$padding" ''
    printf " %s " "$title"
    printf '%*s' $(( width - padding - ${#title} - 2 )) ''
    printf "│\n"

    printf "├"
    printf '%*s' "$width" '' | tr ' ' '─'
    printf "┤\n"
}

print_box_line() {
    local content="$1"
    local width=${2:-50}
    local visible_len
    # Calcular longitud sin códigos ANSI
    visible_len=$(echo -e "$content" | sed 's/\x1b\[[0-9;]*m//g' | wc -c)
    visible_len=$(( visible_len - 1 ))  # wc -c incluye newline
    local pad=$(( width - visible_len ))
    [ "$pad" -lt 0 ] && pad=0
    printf "│ %b%*s│\n" "$content" "$pad" ""
}

print_box_footer() {
    local width=${2:-50}
    printf "└"
    printf '%*s' "$width" '' | tr ' ' '─'
    printf "┘\n"
}
```

---

## 6. Resumen ejecutivo

El resumen ejecutivo aparece al inicio del reporte. En 2-3 líneas indica el estado global del sistema.

### Lógica del resumen

```
Recolectar la severidad de todas las métricas:
  disk_sev[]     = ok, ok, warn     (por cada mount)
  mem_sev        = ok
  swap_sev       = ok
  load_sev       = warn
  services_count = 0

Calcular estado global:
  Si alguna es CRIT  → estado global = CRIT
  Si alguna es WARN  → estado global = WARN
  Si todas son OK    → estado global = OK

Generar resumen:
  "SYSTEM STATUS: OK — All metrics within normal range"
  "SYSTEM STATUS: WARNING — 2 issues detected (disk /, load)"
  "SYSTEM STATUS: CRITICAL — 1 critical issue (disk /var at 96%)"
```

### Implementación

```bash
# Acumular severidades durante el formateo
declare -a ISSUES=()
GLOBAL_STATUS="ok"

add_issue() {
    local severity=$1
    local message=$2

    ISSUES+=("${severity}:${message}")

    # Elevar estado global si es más severo
    case "$severity" in
        crit) GLOBAL_STATUS="crit" ;;
        warn) [ "$GLOBAL_STATUS" != "crit" ] && GLOBAL_STATUS="warn" ;;
    esac
}

print_summary() {
    local status_text status_color

    case "$GLOBAL_STATUS" in
        ok)
            status_text="OK"
            status_color="$GREEN"
            ;;
        warn)
            status_text="WARNING"
            status_color="$YELLOW"
            ;;
        crit)
            status_text="CRITICAL"
            status_color="$RED"
            ;;
    esac

    echo -e "${BOLD}SYSTEM STATUS: ${status_color}${status_text}${NC}"

    if [ ${#ISSUES[@]} -eq 0 ]; then
        echo -e "${DIM}All metrics within normal range${NC}"
    else
        local warn_count=0 crit_count=0
        for issue in "${ISSUES[@]}"; do
            case "${issue%%:*}" in
                warn) warn_count=$(( warn_count + 1 )) ;;
                crit) crit_count=$(( crit_count + 1 )) ;;
            esac
        done

        local summary=""
        [ "$crit_count" -gt 0 ] && summary+="${crit_count} critical"
        [ "$warn_count" -gt 0 ] && {
            [ -n "$summary" ] && summary+=", "
            summary+="${warn_count} warning"
        }
        echo -e "${DIM}Issues: ${summary}${NC}"

        for issue in "${ISSUES[@]}"; do
            local sev="${issue%%:*}"
            local msg="${issue#*:}"
            case "$sev" in
                crit) echo -e "  ${RED}●${NC} $msg" ;;
                warn) echo -e "  ${YELLOW}●${NC} $msg" ;;
            esac
        done
    fi
    echo ""
}
```

---

## 7. Implementación completa: health_report.sh

### Script completo

```bash
#!/bin/bash
# health_report.sh — System Health Check con formato coloreado
# Combina recolección (T01) + formateo (T02)

set -uo pipefail

# ── Configuración ──────────────────────────────────

# Umbrales
DISK_WARN=80
DISK_CRIT=90
MEM_WARN=80
MEM_CRIT=95
SWAP_WARN=50
SWAP_CRIT=80
LOAD_WARN_FACTOR=1
LOAD_CRIT_FACTOR=2

# Ancho de la barra de progreso
BAR_WIDTH=20

# ── Colores ────────────────────────────────────────

if [ -t 1 ]; then
    RED='\033[0;31m'
    GREEN='\033[0;32m'
    YELLOW='\033[1;33m'
    BLUE='\033[0;34m'
    CYAN='\033[0;36m'
    BOLD='\033[1m'
    DIM='\033[2m'
    NC='\033[0m'
else
    RED='' GREEN='' YELLOW='' BLUE='' CYAN=''
    BOLD='' DIM='' NC=''
fi

# ── Estado global ──────────────────────────────────

declare -a ISSUES=()
GLOBAL_STATUS="ok"

add_issue() {
    local sev=$1 msg=$2
    ISSUES+=("${sev}:${msg}")
    case "$sev" in
        crit) GLOBAL_STATUS="crit" ;;
        warn) [ "$GLOBAL_STATUS" != "crit" ] && GLOBAL_STATUS="warn" ;;
    esac
}

# ── Funciones de utilidad ──────────────────────────

human_kb() {
    awk -v k="$1" 'BEGIN {
        if (k >= 1048576) printf "%.1fG", k/1048576
        else if (k >= 1024) printf "%.0fM", k/1024
        else printf "%dK", k
    }'
}

check_threshold() {
    local val=$1 warn=$2 crit=$3
    if [ "$val" -ge "$crit" ]; then echo "crit"
    elif [ "$val" -ge "$warn" ]; then echo "warn"
    else echo "ok"
    fi
}

status_icon() {
    case "$1" in
        ok)   echo -e "${GREEN}✓${NC}" ;;
        warn) echo -e "${YELLOW}!${NC}" ;;
        crit) echo -e "${RED}✗${NC}" ;;
    esac
}

progress_bar() {
    local pct=$1 width=${2:-$BAR_WIDTH} sev=${3:-ok}
    local filled=$(( pct * width / 100 ))
    local empty=$(( width - filled ))
    local color=""
    case "$sev" in
        ok)   color="$GREEN" ;;
        warn) color="$YELLOW" ;;
        crit) color="$RED" ;;
    esac
    local fill_str=$(printf '%*s' "$filled" '' | tr ' ' '█')
    local empty_str=$(printf '%*s' "$empty" '' | tr ' ' '░')
    printf "%b[%s%s]%b" "$color" "$fill_str" "$empty_str" "$NC"
}

separator() {
    printf "${DIM}"
    printf '%*s' 56 '' | tr ' ' '─'
    printf "${NC}\n"
}

# ── Secciones del reporte ─────────────────────────

report_header() {
    local hostname kernel date_str uptime_str

    hostname=$(cat /proc/sys/kernel/hostname 2>/dev/null || hostname)
    kernel=$(cut -d' ' -f3 /proc/version 2>/dev/null || uname -r)
    date_str=$(date '+%Y-%m-%d %H:%M:%S')
    uptime_str=$(awk '{d=int($1/86400); h=int(($1%86400)/3600); m=int(($1%3600)/60); printf "%dd %dh %dm", d, h, m}' /proc/uptime)

    echo ""
    echo -e "${BOLD}  System Health Check${NC}"
    echo -e "  ${DIM}${hostname} — ${date_str}${NC}"
    echo -e "  ${DIM}Kernel: ${kernel} — Up: ${uptime_str}${NC}"
    separator
}

report_disk() {
    echo -e "${BOLD}  DISK${NC}"

    df -P -k --output=target,size,used,avail,pcent \
        -x tmpfs -x devtmpfs -x squashfs -x overlay -x efivarfs 2>/dev/null | \
        tail -n +2 | while read -r mount size used avail pct; do
            pct=${pct%%%}
            local sev
            sev=$(check_threshold "$pct" "$DISK_WARN" "$DISK_CRIT")
            [ "$sev" != "ok" ] && add_issue "$sev" "Disk ${mount} at ${pct}%"

            local bar
            bar=$(progress_bar "$pct" "$BAR_WIDTH" "$sev")
            local icon
            icon=$(status_icon "$sev")

            printf "  %-12s %s %3d%%  %6s / %6s  %b\n" \
                "$mount" "$bar" "$pct" \
                "$(human_kb "$used")" "$(human_kb "$size")" "$icon"
        done

    separator
}

report_memory() {
    echo -e "${BOLD}  MEMORY${NC}"

    local mt=0 ma=0 st=0 sf=0
    while IFS=': ' read -r key value _; do
        case "$key" in
            MemTotal)     mt=$value ;;
            MemAvailable) ma=$value ;;
            SwapTotal)    st=$value ;;
            SwapFree)     sf=$value ;;
        esac
    done < /proc/meminfo

    # RAM
    local mu=$(( mt - ma ))
    local mp=0
    [ "$mt" -gt 0 ] && mp=$(( mu * 100 / mt ))
    local mem_sev
    mem_sev=$(check_threshold "$mp" "$MEM_WARN" "$MEM_CRIT")
    [ "$mem_sev" != "ok" ] && add_issue "$mem_sev" "RAM at ${mp}%"

    printf "  %-12s %s %3d%%  %6s / %6s  %b\n" \
        "RAM" "$(progress_bar "$mp" "$BAR_WIDTH" "$mem_sev")" "$mp" \
        "$(human_kb "$mu")" "$(human_kb "$mt")" "$(status_icon "$mem_sev")"

    # Swap
    if [ "$st" -gt 0 ]; then
        local su=$(( st - sf ))
        local sp=$(( su * 100 / st ))
        local swap_sev
        swap_sev=$(check_threshold "$sp" "$SWAP_WARN" "$SWAP_CRIT")
        [ "$swap_sev" != "ok" ] && add_issue "$swap_sev" "Swap at ${sp}%"

        printf "  %-12s %s %3d%%  %6s / %6s  %b\n" \
            "Swap" "$(progress_bar "$sp" "$BAR_WIDTH" "$swap_sev")" "$sp" \
            "$(human_kb "$su")" "$(human_kb "$st")" "$(status_icon "$swap_sev")"
    else
        echo -e "  Swap         ${DIM}not configured${NC}"
    fi

    separator
}

report_load() {
    echo -e "${BOLD}  LOAD${NC}"

    local l1 l5 l15 procs _
    read -r l1 l5 l15 procs _ < /proc/loadavg
    local cpus
    cpus=$(nproc)

    local load_sev
    load_sev=$(awk -v l="$l1" -v c="$cpus" -v wf="$LOAD_WARN_FACTOR" -v cf="$LOAD_CRIT_FACTOR" \
        'BEGIN {
            if (l >= c * cf) print "crit"
            else if (l >= c * wf) print "warn"
            else print "ok"
        }')

    [ "$load_sev" != "ok" ] && add_issue "$load_sev" "Load ${l1} (CPUs: ${cpus})"

    local load_color=""
    case "$load_sev" in
        ok)   load_color="$GREEN" ;;
        warn) load_color="$YELLOW" ;;
        crit) load_color="$RED" ;;
    esac

    printf "  Load avg:    %b%-5s%b  %-5s  %-5s  %b(CPUs: %d)%b  %b\n" \
        "$load_color" "$l1" "$NC" "$l5" "$l15" \
        "$DIM" "$cpus" "$NC" "$(status_icon "$load_sev")"
    printf "  Processes:   %b%s%b\n" "$DIM" "$procs" "$NC"

    separator
}

report_services() {
    echo -e "${BOLD}  SERVICES${NC}"

    local failed
    failed=$(systemctl --failed --no-legend --no-pager --plain 2>/dev/null || true)

    if [ -z "$failed" ]; then
        echo -e "  ${GREEN}✓${NC} All services running"
    else
        local count
        count=$(echo "$failed" | wc -l)
        add_issue "crit" "${count} failed service(s)"

        echo -e "  ${RED}✗${NC} ${count} failed service(s):"
        echo "$failed" | awk '{printf "    %s%-20s%s %s\n", "'"$RED"'", $1, "'"$NC"'", $4}'
    fi

    separator
}

report_summary() {
    local status_text status_color

    case "$GLOBAL_STATUS" in
        ok)   status_text="ALL SYSTEMS OK"    ; status_color="$GREEN" ;;
        warn) status_text="WARNINGS DETECTED" ; status_color="$YELLOW" ;;
        crit) status_text="CRITICAL ISSUES"   ; status_color="$RED" ;;
    esac

    echo -e "  ${BOLD}STATUS: ${status_color}${status_text}${NC}"

    if [ ${#ISSUES[@]} -gt 0 ]; then
        for issue in "${ISSUES[@]}"; do
            local sev="${issue%%:*}"
            local msg="${issue#*:}"
            case "$sev" in
                crit) echo -e "    ${RED}●${NC} ${msg}" ;;
                warn) echo -e "    ${YELLOW}●${NC} ${msg}" ;;
            esac
        done
    fi
    echo ""
}

# ── Main ───────────────────────────────────────────

report_header
report_disk
report_memory
report_load
report_services
report_summary
```

### Ejemplo de salida (representación sin colores reales)

```
  System Health Check
  web-prod-01 — 2026-03-21 14:30:00
  Kernel: 6.19.8-200.fc43.x86_64 — Up: 14d 3h 22m
  ────────────────────────────────────────────────────────
  DISK
  /            [████████████████░░░░] 84%   42.0G /  50.0G  !
  /home        [████████░░░░░░░░░░░░] 44%  200.0G / 449.0G  ✓
  /boot        [███░░░░░░░░░░░░░░░░░] 19%  200.0M /   1.0G  ✓
  ────────────────────────────────────────────────────────
  MEMORY
  RAM          [███████░░░░░░░░░░░░░] 37%    5.9G /  15.6G  ✓
  Swap         [░░░░░░░░░░░░░░░░░░░░]  2%   96.0M /   4.0G  ✓
  ────────────────────────────────────────────────────────
  LOAD
  Load avg:    0.45   0.62   0.38   (CPUs: 8)  ✓
  Processes:   2/384
  ────────────────────────────────────────────────────────
  SERVICES
  ✓ All services running
  ────────────────────────────────────────────────────────
  STATUS: WARNINGS DETECTED
    ● Disk / at 84%
```

---

## 8. Detección de terminal y fallback

### Detectar capacidades del terminal

```bash
# ¿Es un terminal?
[ -t 1 ]                    # stdout va a terminal

# ¿Soporta colores?
COLORS=$(tput colors 2>/dev/null || echo 0)
[ "$COLORS" -ge 8 ]         # Al menos 8 colores

# ¿Soporta Unicode?
# Verificar la variable LANG/LC_ALL
[[ "$LANG" == *UTF-8* ]] || [[ "$LC_ALL" == *UTF-8* ]]

# Combinación robusta
setup_formatting() {
    if [ -t 1 ] && [ "$(tput colors 2>/dev/null || echo 0)" -ge 8 ]; then
        RED='\033[0;31m'
        GREEN='\033[0;32m'
        YELLOW='\033[1;33m'
        BOLD='\033[1m'
        DIM='\033[2m'
        NC='\033[0m'
    else
        RED='' GREEN='' YELLOW='' BOLD='' DIM='' NC=''
    fi

    # Caracteres de barra: Unicode o ASCII
    if locale charmap 2>/dev/null | grep -qi utf; then
        FILL_CHAR="█"
        EMPTY_CHAR="░"
        CHECK="✓"
        CROSS="✗"
        BULLET="●"
    else
        FILL_CHAR="#"
        EMPTY_CHAR="-"
        CHECK="OK"
        CROSS="FAIL"
        BULLET="*"
    fi
}
```

### Forzar/deshabilitar colores

```bash
# Convenciones estándar:
# NO_COLOR=1     → deshabilitar colores (estándar no-color.org)
# FORCE_COLOR=1  → forzar colores incluso sin terminal

if [ "${NO_COLOR:-}" ]; then
    RED='' GREEN='' YELLOW='' BOLD='' DIM='' NC=''
elif [ "${FORCE_COLOR:-}" ] || [ -t 1 ]; then
    RED='\033[0;31m'
    # ...
fi
```

### Flag --no-color

```bash
# Parsear argumentos
while [ $# -gt 0 ]; do
    case "$1" in
        --no-color) NO_COLOR=1 ;;
        --help) show_help; exit 0 ;;
        *) echo "Unknown option: $1" >&2; exit 1 ;;
    esac
    shift
done
```

---

## 9. Errores comunes

### Error 1: Colores corrompen la alineación de printf

```bash
# MAL — los códigos ANSI ocupan caracteres pero son invisibles
printf "%-20s %s\n" "${RED}CRITICAL${NC}" "disk full"
# La columna se desalinea porque \033[0;31m cuenta como caracteres

# BIEN — aplicar color DESPUÉS del padding
printf "%-10s %s\n" "CRITICAL" "disk full" | sed "s/CRITICAL/${RED}CRITICAL${NC}/"

# MEJOR — calcular el padding manualmente
label="CRITICAL"
color_label="${RED}${label}${NC}"
pad=$(( 10 - ${#label} ))
printf "%b%*s %s\n" "$color_label" "$pad" "" "disk full"
```

### Error 2: echo -e no funciona en todos los shells

```bash
# MAL — echo -e no es portable (falla en algunos sh)
echo -e "${RED}error${NC}"

# BIEN — printf es POSIX y siempre interpreta escapes
printf "%b\n" "${RED}error${NC}"
# %b interpreta secuencias de escape en el argumento

# TAMBIÉN BIEN — $'...' en bash
echo $'\033[31merror\033[0m'
```

### Error 3: Subshell en pipes pierde variables

```bash
# MAL — el while dentro de un pipe se ejecuta en subshell
# Las variables modificadas dentro NO se ven fuera
echo "data" | while read -r line; do
    GLOBAL_STATUS="warn"  # Modificación perdida
done
echo "$GLOBAL_STATUS"  # Sigue siendo "ok"

# BIEN — usar process substitution
while read -r line; do
    GLOBAL_STATUS="warn"  # Modificación persiste
done < <(echo "data")
echo "$GLOBAL_STATUS"  # "warn"

# TAMBIÉN BIEN — usar here-string o redirección
while read -r line; do
    GLOBAL_STATUS="warn"
done <<< "data"
```

### Error 4: No manejar terminales estrechos

```bash
# MAL — asumir que el terminal tiene 80+ columnas
# En un terminal de 40 columnas, las líneas se rompen

# BIEN — detectar ancho y adaptar
TERM_WIDTH=$(tput cols 2>/dev/null || echo 80)
if [ "$TERM_WIDTH" -lt 60 ]; then
    BAR_WIDTH=10
else
    BAR_WIDTH=20
fi
```

### Error 5: No manejar valores faltantes

```bash
# MAL — asume que awk siempre encuentra el campo
mem_total=$(awk '/MemTotal/ {print $2}' /proc/meminfo)
pct=$(( used * 100 / mem_total ))  # División por 0 si awk no matcheó

# BIEN — valor por defecto
mem_total=$(awk '/^MemTotal:/ {print $2}' /proc/meminfo)
mem_total=${mem_total:-1}  # Default 1 para evitar división por 0
```

---

## 10. Cheatsheet

```
COLORES ANSI
  \033[0;31m   Rojo        \033[0;32m   Verde
  \033[1;33m   Amarillo    \033[0;34m   Azul
  \033[1m      Negrita     \033[2m      Tenue
  \033[0m      Reset

DETECCIÓN
  [ -t 1 ]                   ¿stdout es terminal?
  tput colors                Número de colores soportados
  locale charmap             ¿UTF-8?
  NO_COLOR=1                 Convención para deshabilitar

PRINTF
  %-15s       String, 15 chars, alineado izquierda
  %8s         String, 8 chars, alineado derecha
  %3d%%       Entero de 3 dígitos + literal %
  %b          Interpretar escapes (\033)

BARRAS
  █░           Unicode (UTF-8)
  #-           ASCII fallback
  progress_bar PERCENT WIDTH SEVERITY

UMBRALES
  DISK:   80% warn, 90% crit
  RAM:    80% warn, 95% crit
  SWAP:   50% warn, 80% crit
  LOAD:   >CPUs warn, >CPUs×2 crit

RESUMEN
  Acumular issues durante formateo
  GLOBAL_STATUS = max(all severities)
  Mostrar al final: OK / WARNING / CRITICAL + lista
```

---

## 11. Ejercicios

### Ejercicio 1: Barras de progreso con umbrales

**Objetivo**: Implementar y probar el sistema de barras coloreadas.

```bash
# 1. Crear el script
cat > /tmp/bar_test.sh << 'SCRIPT'
#!/bin/bash

# Colores
if [ -t 1 ]; then
    RED='\033[0;31m'; GREEN='\033[0;32m'
    YELLOW='\033[1;33m'; NC='\033[0m'
else
    RED=''; GREEN=''; YELLOW=''; NC=''
fi

progress_bar() {
    local pct=$1 width=${2:-25} sev=${3:-ok}
    local filled=$(( pct * width / 100 ))
    local empty=$(( width - filled ))
    local color=""
    case "$sev" in
        ok)   color="$GREEN" ;;
        warn) color="$YELLOW" ;;
        crit) color="$RED" ;;
    esac
    local f=$(printf '%*s' "$filled" '' | tr ' ' '█')
    local e=$(printf '%*s' "$empty" '' | tr ' ' '░')
    printf "%b[%s%s]%b %3d%%" "$color" "$f" "$e" "$NC" "$pct"
}

check_threshold() {
    local val=$1 warn=$2 crit=$3
    if [ "$val" -ge "$crit" ]; then echo "crit"
    elif [ "$val" -ge "$warn" ]; then echo "warn"
    else echo "ok"
    fi
}

# 2. Mostrar barras para distintos porcentajes
echo "=== Disk Thresholds (warn=80, crit=90) ==="
for pct in 10 25 50 75 80 85 90 95 100; do
    sev=$(check_threshold "$pct" 80 90)
    printf "  %3d%%  " "$pct"
    progress_bar "$pct" 25 "$sev"
    printf "  %s\n" "$sev"
done

echo ""
echo "=== Memory Thresholds (warn=80, crit=95) ==="
for pct in 20 40 60 80 90 95 99; do
    sev=$(check_threshold "$pct" 80 95)
    printf "  %3d%%  " "$pct"
    progress_bar "$pct" 25 "$sev"
    printf "  %s\n" "$sev"
done
SCRIPT
chmod +x /tmp/bar_test.sh

# 3. Ejecutar
/tmp/bar_test.sh

# 4. Probar sin colores (pipe a cat)
echo ""
echo "=== Without colors (piped) ==="
/tmp/bar_test.sh | cat

# 5. Limpiar
rm /tmp/bar_test.sh
```

**Pregunta de reflexión**: Al ejecutar `/tmp/bar_test.sh | cat`, ¿los colores desaparecen? ¿Por qué? ¿Qué cambia entre ejecutar el script directamente y a través de un pipe?

> Sí, los colores desaparecen. Cuando ejecutas el script directamente, stdout (fd 1) está conectado al terminal, por lo que `[ -t 1 ]` es verdadero y se asignan los códigos de color. Cuando pones un pipe (`| cat`), stdout del script va al pipe (no al terminal), `[ -t 1 ]` es falso y las variables de color quedan vacías. El resultado de `cat` sí va al terminal, pero el script ya decidió no incluir colores. Esto es el comportamiento correcto — protege contra basura ANSI en archivos y pipes.

---

### Ejercicio 2: Reporte de disco formateado

**Objetivo**: Crear un reporte de disco completo con barras, alineación y estado.

```bash
# 1. Crear el script de reporte de disco
cat > /tmp/disk_report.sh << 'SCRIPT'
#!/bin/bash
set -uo pipefail

DISK_WARN=80
DISK_CRIT=90

if [ -t 1 ]; then
    RED='\033[0;31m'; GREEN='\033[0;32m'
    YELLOW='\033[1;33m'; BOLD='\033[1m'
    DIM='\033[2m'; NC='\033[0m'
else
    RED=''; GREEN=''; YELLOW=''; BOLD=''; DIM=''; NC=''
fi

human_kb() {
    awk -v k="$1" 'BEGIN {
        if (k>=1048576) printf "%.1fG", k/1048576
        else if (k>=1024) printf "%.0fM", k/1024
        else printf "%dK", k
    }'
}

progress_bar() {
    local pct=$1 width=20 sev=$2
    local filled=$(( pct * width / 100 )) empty=$(( width - filled ))
    local color=""
    case "$sev" in ok) color="$GREEN";; warn) color="$YELLOW";; crit) color="$RED";; esac
    local f=$(printf '%*s' "$filled" '' | tr ' ' '█')
    local e=$(printf '%*s' "$empty" '' | tr ' ' '░')
    printf "%b[%s%s]%b" "$color" "$f" "$e" "$NC"
}

echo ""
echo -e "${BOLD}  Disk Usage Report — $(hostname) — $(date '+%Y-%m-%d %H:%M')${NC}"
printf "  ${DIM}"
printf '%*s' 56 '' | tr ' ' '─'
printf "${NC}\n"

ISSUES=0
df -P -k --output=target,size,used,avail,pcent \
    -x tmpfs -x devtmpfs -x squashfs -x overlay -x efivarfs 2>/dev/null | \
    tail -n +2 | while read -r mount size used avail pct; do
        pct=${pct%%%}
        if [ "$pct" -ge "$DISK_CRIT" ]; then sev="crit"
        elif [ "$pct" -ge "$DISK_WARN" ]; then sev="warn"
        else sev="ok"; fi

        bar=$(progress_bar "$pct" "$sev")
        icon=""
        case "$sev" in
            ok)   icon="${GREEN}✓${NC}" ;;
            warn) icon="${YELLOW}!${NC}" ;;
            crit) icon="${RED}✗${NC}" ;;
        esac

        printf "  %-12s %s %3d%%  %6s / %6s  %b\n" \
            "$mount" "$bar" "$pct" \
            "$(human_kb "$used")" "$(human_kb "$size")" "$icon"
    done

printf "  ${DIM}"
printf '%*s' 56 '' | tr ' ' '─'
printf "${NC}\n"
echo ""
SCRIPT
chmod +x /tmp/disk_report.sh

# 2. Ejecutar
/tmp/disk_report.sh

# 3. Limpiar
rm /tmp/disk_report.sh
```

**Pregunta de reflexión**: El bucle `while read` está dentro de un pipe (`df ... | tail | while`). Si quisiéramos contar el total de issues dentro del bucle, ¿la variable `ISSUES` sería visible fuera? ¿Cómo lo solucionarías?

> No, `ISSUES` no sería visible fuera del bucle porque el pipe crea un subshell. Soluciones: (1) usar process substitution: `while read ...; done < <(df ... | tail)` — así el while corre en el shell principal. (2) Escribir el resultado a un archivo temporal y leerlo después. (3) Usar un named pipe (FIFO). La opción (1) es la más limpia y es la que usamos en el script completo de la sección 7.

---

### Ejercicio 3: Reporte completo con resumen ejecutivo

**Objetivo**: Unir todos los componentes en un reporte completo ejecutable.

```bash
# 1. Copiar el script completo de la sección 7 a un archivo
# (El script está en la sección 7 — copiarlo tal cual)

# 2. Hacerlo ejecutable
# chmod +x health_report.sh

# 3. Ejecutar y observar la salida
# ./health_report.sh

# 4. Probar redirigiendo a archivo (sin colores)
# ./health_report.sh > /tmp/health-report.txt
# cat /tmp/health-report.txt

# 5. Probar con umbrales bajos para forzar warnings
# (editar DISK_WARN=10 DISK_CRIT=20 en el script)

# TAREA: copiar el script de la sección 7, guardarlo como
# ~/health_report.sh, ejecutarlo en tu sistema, y verificar:
#   a) ¿La salida muestra todos los filesystems reales?
#   b) ¿Los colores se desactivan al hacer: ./health_report.sh | cat ?
#   c) ¿El resumen ejecutivo al final refleja correctamente los issues?
#   d) ¿Qué pasa si cambias DISK_WARN a 10? ¿Todos los discos
#      aparecen en amarillo?
```

**Pregunta de reflexión**: El script actual genera el resumen al final del reporte. En un reporte largo (muchos discos, muchos servicios), ¿por qué podría ser mejor mover el resumen al inicio? ¿Qué desafío técnico presenta esto en Bash?

> Mover el resumen al inicio permitiría al sysadmin ver el estado global sin tener que scrollear hasta abajo — el reporte sigue el principio de "pirámide invertida" (lo más importante primero). El desafío técnico es que las issues se acumulan **durante** el formateo de cada sección (disco, memoria, load, servicios). Para mostrar el resumen antes, necesitas ejecutar la recolección y evaluación primero (sin imprimir), almacenar los resultados en variables/arrays, imprimir el resumen, y luego imprimir los detalles. Esto requiere separar la lógica de evaluación de la de impresión en dos pasadas, o almacenar toda la salida en un buffer (`output=$(report_all)`) y reordenarla.

---

> **Siguiente tema**: T03 — Mejora 1: exportar reporte como JSON o HTML.
