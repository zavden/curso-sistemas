# Mejora 1 — Exportar Reporte como JSON o HTML

## Índice

1. [¿Por qué exportar en múltiples formatos?](#1-por-qué-exportar-en-múltiples-formatos)
2. [Arquitectura: separar datos de presentación](#2-arquitectura-separar-datos-de-presentación)
3. [Generar JSON desde Bash](#3-generar-json-desde-bash)
4. [Generar HTML desde Bash](#4-generar-html-desde-bash)
5. [Flag --format para seleccionar salida](#5-flag---format-para-seleccionar-salida)
6. [Implementación completa](#6-implementación-completa)
7. [Validar la salida](#7-validar-la-salida)
8. [Errores comunes](#8-errores-comunes)
9. [Cheatsheet](#9-cheatsheet)
10. [Ejercicios](#10-ejercicios)

---

## 1. ¿Por qué exportar en múltiples formatos?

El reporte coloreado de T02 funciona para un sysadmin en la terminal. Pero en producción real necesitas otros formatos:

```
Terminal (T02):
  └── Diagnóstico rápido en el servidor

JSON:
  ├── Consumir desde scripts (jq, Python, etc.)
  ├── Alimentar dashboards (Grafana, custom)
  ├── Almacenar histórico en base de datos
  ├── Enviar a APIs de monitorización
  └── Comparar estados entre servidores

HTML:
  ├── Enviar por email como reporte visual
  ├── Publicar en intranet
  ├── Abrir en un navegador sin herramientas especiales
  └── Generar PDFs (con wkhtmltopdf o el navegador)
```

```
                    ┌──────────────┐
                    │ Recolección  │
                    │ (datos)      │
                    └──────┬───────┘
                           │
              ┌────────────┼────────────┐
              │            │            │
        ┌─────▼─────┐ ┌───▼───┐  ┌────▼────┐
        │ Terminal   │ │ JSON  │  │  HTML   │
        │ (colores)  │ │       │  │         │
        └───────────┘ └───────┘  └─────────┘
          T02            T03         T03
```

---

## 2. Arquitectura: separar datos de presentación

### El patrón: recolectar una vez, formatear muchas veces

```bash
# Estructura del script refactorizado:

# 1. Recolectar datos en variables/arrays (una sola vez)
collect_all_data() {
    # Llena variables globales con datos crudos
    # No imprime nada
}

# 2. Evaluar umbrales (una sola vez)
evaluate_thresholds() {
    # Compara datos contra umbrales
    # Llena array de issues y severidades
}

# 3. Formatear según el formato elegido
case "$FORMAT" in
    terminal) output_terminal ;;
    json)     output_json ;;
    html)     output_html ;;
esac
```

### Estructuras de datos intermedias

```bash
# Almacenar datos de disco en arrays paralelos
declare -a DISK_MOUNT=()
declare -a DISK_SIZE=()
declare -a DISK_USED=()
declare -a DISK_AVAIL=()
declare -a DISK_PCT=()
declare -a DISK_SEV=()

# Memoria como variables simples
MEM_TOTAL=0
MEM_USED=0
MEM_AVAIL=0
MEM_PCT=0
MEM_SEV="ok"

SWAP_TOTAL=0
SWAP_USED=0
SWAP_FREE=0
SWAP_PCT=0
SWAP_SEV="ok"

# Load
LOAD_1="" LOAD_5="" LOAD_15=""
LOAD_CPUS=0
LOAD_SEV="ok"

# Services
SVC_FAILED_COUNT=0
declare -a SVC_FAILED_NAMES=()

# Global
declare -a ISSUES=()
GLOBAL_STATUS="ok"
```

---

## 3. Generar JSON desde Bash

### Fundamentos de JSON

```json
{
  "string": "valor",
  "number": 42,
  "float": 3.14,
  "boolean": true,
  "null_value": null,
  "array": [1, 2, 3],
  "object": {
    "nested": "value"
  }
}
```

Reglas críticas:
- Las strings van entre **comillas dobles** (no simples)
- Los números **no** van entre comillas
- Los booleanos son `true`/`false` (minúsculas, sin comillas)
- No hay coma después del último elemento de un array/objeto

### Escapar strings para JSON

```bash
# Los valores pueden contener caracteres especiales que rompen JSON
json_escape() {
    local str="$1"
    # Escapar: \ → \\, " → \", newline → \n, tab → \t
    str="${str//\\/\\\\}"      # backslash primero
    str="${str//\"/\\\"}"      # comillas dobles
    str="${str//$'\n'/\\n}"    # newlines
    str="${str//$'\t'/\\t}"    # tabs
    echo "$str"
}

# Uso:
# json_escape 'He said "hello"'  → He said \"hello\"
```

### Construir JSON manualmente

```bash
output_json_disk() {
    echo '    "disk": ['
    local i
    for i in "${!DISK_MOUNT[@]}"; do
        [ "$i" -gt 0 ] && echo ','
        printf '      {'
        printf '"mount": "%s", ' "${DISK_MOUNT[$i]}"
        printf '"size_kb": %s, ' "${DISK_SIZE[$i]}"
        printf '"used_kb": %s, ' "${DISK_USED[$i]}"
        printf '"avail_kb": %s, ' "${DISK_AVAIL[$i]}"
        printf '"percent": %s, ' "${DISK_PCT[$i]}"
        printf '"status": "%s"' "${DISK_SEV[$i]}"
        printf '}'
    done
    echo ''
    echo '    ]'
}
```

### Usar jq para generar JSON (si está disponible)

```bash
# jq puede construir JSON correcto garantizando el escapado
output_json_with_jq() {
    jq -n \
        --arg host "$HOSTNAME" \
        --arg kernel "$KERNEL" \
        --arg date "$DATE_STR" \
        --argjson uptime "$UPTIME_SEC" \
        --argjson mem_total "$MEM_TOTAL" \
        --argjson mem_used "$MEM_USED" \
        --argjson mem_pct "$MEM_PCT" \
        --arg mem_sev "$MEM_SEV" \
        '{
            system: {
                hostname: $host,
                kernel: $kernel,
                date: $date,
                uptime_seconds: $uptime
            },
            memory: {
                ram: {
                    total_kb: $mem_total,
                    used_kb: $mem_used,
                    percent: $mem_pct,
                    status: $mem_sev
                }
            }
        }'
}
```

### JSON completo: estructura objetivo

```json
{
  "system": {
    "hostname": "web-prod-01",
    "kernel": "6.19.8-200.fc43.x86_64",
    "date": "2026-03-21T14:30:00",
    "uptime_seconds": 1213920
  },
  "status": "warn",
  "issues": [
    {"severity": "warn", "message": "Disk / at 84%"}
  ],
  "disk": [
    {
      "mount": "/",
      "size_kb": 52428800,
      "used_kb": 44040192,
      "avail_kb": 5872025,
      "percent": 84,
      "status": "warn"
    },
    {
      "mount": "/home",
      "size_kb": 470548480,
      "used_kb": 214748364,
      "avail_kb": 243056345,
      "percent": 44,
      "status": "ok"
    }
  ],
  "memory": {
    "ram": {
      "total_kb": 16384000,
      "used_kb": 6144000,
      "available_kb": 10240000,
      "percent": 37,
      "status": "ok"
    },
    "swap": {
      "total_kb": 4194304,
      "used_kb": 98304,
      "free_kb": 4096000,
      "percent": 2,
      "status": "ok"
    }
  },
  "load": {
    "avg_1": 0.45,
    "avg_5": 0.62,
    "avg_15": 0.38,
    "cpus": 8,
    "status": "ok"
  },
  "services": {
    "failed_count": 0,
    "failed": []
  }
}
```

---

## 4. Generar HTML desde Bash

### Estructura HTML básica

```bash
output_html() {
    cat << 'HTMLHEAD'
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>System Health Report</title>
<style>
HTMLHEAD

    output_html_css
    echo '</style>'
    echo '</head>'
    echo '<body>'

    output_html_body

    echo '</body>'
    echo '</html>'
}
```

### CSS para el reporte

```bash
output_html_css() {
    cat << 'CSS'
    * { margin: 0; padding: 0; box-sizing: border-box; }
    body {
        font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, monospace;
        background: #1a1a2e;
        color: #eee;
        padding: 20px;
        max-width: 800px;
        margin: 0 auto;
    }
    h1 { font-size: 1.4em; margin-bottom: 5px; }
    .subtitle { color: #888; font-size: 0.9em; margin-bottom: 20px; }
    .section { background: #16213e; border-radius: 8px; padding: 15px; margin-bottom: 15px; }
    .section h2 { font-size: 1.1em; margin-bottom: 10px; color: #a8d8ea; }
    table { width: 100%; border-collapse: collapse; }
    th, td { text-align: left; padding: 6px 10px; }
    th { color: #888; font-weight: normal; font-size: 0.85em; border-bottom: 1px solid #333; }
    .bar-bg { background: #333; border-radius: 4px; height: 18px; position: relative; }
    .bar-fill { border-radius: 4px; height: 100%; }
    .bar-text { position: absolute; right: 6px; top: 0; font-size: 0.8em; line-height: 18px; }
    .ok   { color: #4ecca3; }
    .warn { color: #f0c040; }
    .crit { color: #e84545; }
    .bar-ok   .bar-fill { background: #4ecca3; }
    .bar-warn .bar-fill { background: #f0c040; }
    .bar-crit .bar-fill { background: #e84545; }
    .status-banner {
        padding: 12px 15px; border-radius: 8px; margin-bottom: 15px;
        font-weight: bold; font-size: 1.1em;
    }
    .status-ok   { background: #1b4332; color: #4ecca3; }
    .status-warn { background: #3d2e00; color: #f0c040; }
    .status-crit { background: #3d0000; color: #e84545; }
    .issue-list { list-style: none; padding-left: 10px; font-size: 0.9em; margin-top: 5px; }
    .issue-list li::before { content: "● "; }
    .issue-list li.crit::before { color: #e84545; }
    .issue-list li.warn::before { color: #f0c040; }
    .services-ok { color: #4ecca3; }
    .timestamp { color: #666; font-size: 0.8em; text-align: center; margin-top: 15px; }
CSS
}
```

### Generar barras de progreso en HTML

```bash
html_bar() {
    local pct=$1
    local sev=$2

    cat << HTMLBAR
<div class="bar-bg bar-${sev}">
  <div class="bar-fill" style="width: ${pct}%;"></div>
  <span class="bar-text">${pct}%</span>
</div>
HTMLBAR
}
```

### Generar la tabla de discos en HTML

```bash
output_html_disk() {
    echo '<div class="section">'
    echo '<h2>Disk Usage</h2>'
    echo '<table>'
    echo '<tr><th>Mount</th><th>Used / Size</th><th>Usage</th><th>Status</th></tr>'

    local i
    for i in "${!DISK_MOUNT[@]}"; do
        local used_h=$(human_kb "${DISK_USED[$i]}")
        local size_h=$(human_kb "${DISK_SIZE[$i]}")
        local sev="${DISK_SEV[$i]}"

        echo "<tr>"
        echo "  <td>${DISK_MOUNT[$i]}</td>"
        echo "  <td>${used_h} / ${size_h}</td>"
        echo "  <td>$(html_bar "${DISK_PCT[$i]}" "$sev")</td>"
        echo "  <td class=\"${sev}\">${sev^^}</td>"
        echo "</tr>"
    done

    echo '</table>'
    echo '</div>'
}
```

### Banner de estado global

```bash
output_html_summary() {
    echo "<div class=\"status-banner status-${GLOBAL_STATUS}\">"

    case "$GLOBAL_STATUS" in
        ok)   echo "  ALL SYSTEMS OK" ;;
        warn) echo "  WARNINGS DETECTED" ;;
        crit) echo "  CRITICAL ISSUES" ;;
    esac

    if [ ${#ISSUES[@]} -gt 0 ]; then
        echo '  <ul class="issue-list">'
        for issue in "${ISSUES[@]}"; do
            local sev="${issue%%:*}"
            local msg="${issue#*:}"
            echo "    <li class=\"${sev}\">${msg}</li>"
        done
        echo '  </ul>'
    fi

    echo '</div>'
}
```

---

## 5. Flag --format para seleccionar salida

### Parsear argumentos

```bash
# Variables de configuración
FORMAT="terminal"    # Default
OUTPUT_FILE=""       # Vacío = stdout

# Parsear argumentos
usage() {
    echo "Usage: $(basename "$0") [OPTIONS]"
    echo ""
    echo "Options:"
    echo "  --format FORMAT   Output format: terminal, json, html (default: terminal)"
    echo "  --output FILE     Write to file instead of stdout"
    echo "  --no-color        Disable colors in terminal output"
    echo "  --help            Show this help"
}

while [ $# -gt 0 ]; do
    case "$1" in
        --format)
            FORMAT="${2:-}"
            [ -z "$FORMAT" ] && { echo "Error: --format requires a value" >&2; exit 1; }
            shift
            ;;
        --output|-o)
            OUTPUT_FILE="${2:-}"
            [ -z "$OUTPUT_FILE" ] && { echo "Error: --output requires a file path" >&2; exit 1; }
            shift
            ;;
        --no-color)
            NO_COLOR=1
            ;;
        --help|-h)
            usage
            exit 0
            ;;
        *)
            echo "Unknown option: $1" >&2
            usage >&2
            exit 1
            ;;
    esac
    shift
done

# Validar formato
case "$FORMAT" in
    terminal|json|html) ;;
    *) echo "Error: unknown format '$FORMAT'" >&2; exit 1 ;;
esac
```

### Redirigir a archivo

```bash
# En el main, después de parsear argumentos:
main() {
    collect_all_data
    evaluate_thresholds

    if [ -n "$OUTPUT_FILE" ]; then
        # Forzar no-color si escribimos a archivo
        NO_COLOR=1
        setup_colors

        case "$FORMAT" in
            terminal) output_terminal > "$OUTPUT_FILE" ;;
            json)     output_json > "$OUTPUT_FILE" ;;
            html)     output_html > "$OUTPUT_FILE" ;;
        esac
        echo "Report written to: $OUTPUT_FILE" >&2
    else
        case "$FORMAT" in
            terminal) output_terminal ;;
            json)     output_json ;;
            html)     output_html ;;
        esac
    fi
}
```

### Uso

```bash
# Terminal (default)
./health_report.sh

# JSON a stdout
./health_report.sh --format json

# JSON a archivo
./health_report.sh --format json --output /tmp/health.json

# HTML a archivo
./health_report.sh --format html --output /tmp/health.html

# Pipe JSON a jq
./health_report.sh --format json | jq '.disk[] | select(.percent > 80)'

# Enviar HTML por email
./health_report.sh --format html | mail -a "Content-Type: text/html" \
    -s "Health Report" admin@empresa.com
```

---

## 6. Implementación completa

### Script refactorizado con múltiples formatos

```bash
#!/bin/bash
# health_report.sh — System Health Check con múltiples formatos de salida
# Uso: ./health_report.sh [--format terminal|json|html] [--output FILE]

set -uo pipefail

# ── Configuración ──────────────────────────────────

DISK_WARN=80;  DISK_CRIT=90
MEM_WARN=80;   MEM_CRIT=95
SWAP_WARN=50;  SWAP_CRIT=80
LOAD_WARN_FACTOR=1; LOAD_CRIT_FACTOR=2
BAR_WIDTH=20
FORMAT="terminal"
OUTPUT_FILE=""
NO_COLOR="${NO_COLOR:-}"

# ── Parsear argumentos ─────────────────────────────

while [ $# -gt 0 ]; do
    case "$1" in
        --format)  FORMAT="${2:?--format requires a value}"; shift ;;
        --output|-o) OUTPUT_FILE="${2:?--output requires a path}"; shift ;;
        --no-color) NO_COLOR=1 ;;
        --help|-h) echo "Usage: $0 [--format terminal|json|html] [--output FILE] [--no-color]"; exit 0 ;;
        *) echo "Unknown option: $1" >&2; exit 1 ;;
    esac
    shift
done

case "$FORMAT" in
    terminal|json|html) ;;
    *) echo "Error: unknown format '$FORMAT'" >&2; exit 1 ;;
esac

# ── Colores ────────────────────────────────────────

setup_colors() {
    if [ -z "$NO_COLOR" ] && [ -t 1 ] && [ "$FORMAT" = "terminal" ]; then
        RED='\033[0;31m'; GREEN='\033[0;32m'; YELLOW='\033[1;33m'
        BOLD='\033[1m'; DIM='\033[2m'; NC='\033[0m'
    else
        RED=''; GREEN=''; YELLOW=''; BOLD=''; DIM=''; NC=''
    fi
}
setup_colors

# ── Datos globales ─────────────────────────────────

HOSTNAME_STR=""
KERNEL_STR=""
DATE_STR=""
UPTIME_STR=""
UPTIME_SEC=0

declare -a DISK_MOUNT=() DISK_SIZE=() DISK_USED=() DISK_AVAIL=() DISK_PCT=() DISK_SEV=()
MEM_TOTAL=0; MEM_USED=0; MEM_AVAIL=0; MEM_PCT=0; MEM_SEV="ok"
SWAP_TOTAL=0; SWAP_USED=0; SWAP_FREE=0; SWAP_PCT=0; SWAP_SEV="ok"
LOAD_1=""; LOAD_5=""; LOAD_15=""; LOAD_CPUS=0; LOAD_SEV="ok"
SVC_FAILED_COUNT=0; declare -a SVC_FAILED_NAMES=()
declare -a ISSUES=()
GLOBAL_STATUS="ok"

# ── Utilidades ─────────────────────────────────────

human_kb() {
    awk -v k="$1" 'BEGIN {
        if (k>=1048576) printf "%.1fG", k/1048576
        else if (k>=1024) printf "%.0fM", k/1024
        else printf "%dK", k
    }'
}

add_issue() {
    local sev=$1 msg=$2
    ISSUES+=("${sev}:${msg}")
    case "$sev" in
        crit) GLOBAL_STATUS="crit" ;;
        warn) [ "$GLOBAL_STATUS" != "crit" ] && GLOBAL_STATUS="warn" ;;
    esac
}

# ── Recolección ────────────────────────────────────

collect_all() {
    HOSTNAME_STR=$(cat /proc/sys/kernel/hostname 2>/dev/null || hostname)
    KERNEL_STR=$(cut -d' ' -f3 /proc/version 2>/dev/null || uname -r)
    DATE_STR=$(date '+%Y-%m-%dT%H:%M:%S')
    UPTIME_SEC=$(awk '{print int($1)}' /proc/uptime)
    UPTIME_STR=$(awk '{d=int($1/86400);h=int(($1%86400)/3600);m=int(($1%3600)/60);printf "%dd %dh %dm",d,h,m}' /proc/uptime)

    # Disk
    while read -r mount size used avail pct; do
        pct=${pct%%%}
        DISK_MOUNT+=("$mount")
        DISK_SIZE+=("$size")
        DISK_USED+=("$used")
        DISK_AVAIL+=("$avail")
        DISK_PCT+=("$pct")
    done < <(df -P -k --output=target,size,used,avail,pcent \
        -x tmpfs -x devtmpfs -x squashfs -x overlay -x efivarfs 2>/dev/null | tail -n +2)

    # Memory
    while IFS=': ' read -r key value _; do
        case "$key" in
            MemTotal)     MEM_TOTAL=$value ;;
            MemAvailable) MEM_AVAIL=$value ;;
            SwapTotal)    SWAP_TOTAL=$value ;;
            SwapFree)     SWAP_FREE=$value ;;
        esac
    done < /proc/meminfo
    MEM_USED=$(( MEM_TOTAL - MEM_AVAIL ))
    [ "$MEM_TOTAL" -gt 0 ] && MEM_PCT=$(( MEM_USED * 100 / MEM_TOTAL ))
    SWAP_USED=$(( SWAP_TOTAL - SWAP_FREE ))
    [ "$SWAP_TOTAL" -gt 0 ] && SWAP_PCT=$(( SWAP_USED * 100 / SWAP_TOTAL ))

    # Load
    read -r LOAD_1 LOAD_5 LOAD_15 _ _ < /proc/loadavg
    LOAD_CPUS=$(nproc)

    # Services
    local failed
    failed=$(systemctl --failed --no-legend --no-pager --plain 2>/dev/null || true)
    if [ -n "$failed" ]; then
        SVC_FAILED_COUNT=$(echo "$failed" | wc -l)
        while read -r unit _rest; do
            SVC_FAILED_NAMES+=("$unit")
        done <<< "$failed"
    fi
}

# ── Evaluación ─────────────────────────────────────

evaluate() {
    # Disk
    for i in "${!DISK_PCT[@]}"; do
        local p=${DISK_PCT[$i]}
        if [ "$p" -ge "$DISK_CRIT" ]; then DISK_SEV+=("crit"); add_issue crit "Disk ${DISK_MOUNT[$i]} at ${p}%"
        elif [ "$p" -ge "$DISK_WARN" ]; then DISK_SEV+=("warn"); add_issue warn "Disk ${DISK_MOUNT[$i]} at ${p}%"
        else DISK_SEV+=("ok")
        fi
    done

    # Memory
    if [ "$MEM_PCT" -ge "$MEM_CRIT" ]; then MEM_SEV="crit"; add_issue crit "RAM at ${MEM_PCT}%"
    elif [ "$MEM_PCT" -ge "$MEM_WARN" ]; then MEM_SEV="warn"; add_issue warn "RAM at ${MEM_PCT}%"
    fi

    if [ "$SWAP_TOTAL" -gt 0 ]; then
        if [ "$SWAP_PCT" -ge "$SWAP_CRIT" ]; then SWAP_SEV="crit"; add_issue crit "Swap at ${SWAP_PCT}%"
        elif [ "$SWAP_PCT" -ge "$SWAP_WARN" ]; then SWAP_SEV="warn"; add_issue warn "Swap at ${SWAP_PCT}%"
        fi
    fi

    # Load
    LOAD_SEV=$(awk -v l="$LOAD_1" -v c="$LOAD_CPUS" -v wf="$LOAD_WARN_FACTOR" -v cf="$LOAD_CRIT_FACTOR" \
        'BEGIN { if (l>=c*cf) print "crit"; else if (l>=c*wf) print "warn"; else print "ok" }')
    [ "$LOAD_SEV" != "ok" ] && add_issue "$LOAD_SEV" "Load ${LOAD_1} (CPUs: ${LOAD_CPUS})"

    # Services
    [ "$SVC_FAILED_COUNT" -gt 0 ] && add_issue crit "${SVC_FAILED_COUNT} failed service(s)"
}

# ── Output: Terminal ───────────────────────────────

progress_bar() {
    local pct=$1 sev=$2
    local filled=$(( pct * BAR_WIDTH / 100 )) empty=$(( BAR_WIDTH - filled ))
    local color=""
    case "$sev" in ok) color="$GREEN";; warn) color="$YELLOW";; crit) color="$RED";; esac
    local f=$(printf '%*s' "$filled" '' | tr ' ' '█')
    local e=$(printf '%*s' "$empty" '' | tr ' ' '░')
    printf "%b[%s%s]%b" "$color" "$f" "$e" "$NC"
}

sev_icon() {
    case "$1" in
        ok) echo -e "${GREEN}✓${NC}";; warn) echo -e "${YELLOW}!${NC}";; crit) echo -e "${RED}✗${NC}";;
    esac
}

output_terminal() {
    echo ""
    echo -e "${BOLD}  System Health Check${NC}"
    echo -e "  ${DIM}${HOSTNAME_STR} — ${DATE_STR}${NC}"
    echo -e "  ${DIM}Kernel: ${KERNEL_STR} — Up: ${UPTIME_STR}${NC}"
    printf "  ${DIM}"; printf '%*s' 56 '' | tr ' ' '─'; printf "${NC}\n"

    echo -e "${BOLD}  DISK${NC}"
    for i in "${!DISK_MOUNT[@]}"; do
        printf "  %-12s %s %3d%%  %6s / %6s  %b\n" \
            "${DISK_MOUNT[$i]}" "$(progress_bar "${DISK_PCT[$i]}" "${DISK_SEV[$i]}")" \
            "${DISK_PCT[$i]}" "$(human_kb "${DISK_USED[$i]}")" "$(human_kb "${DISK_SIZE[$i]}")" \
            "$(sev_icon "${DISK_SEV[$i]}")"
    done
    printf "  ${DIM}"; printf '%*s' 56 '' | tr ' ' '─'; printf "${NC}\n"

    echo -e "${BOLD}  MEMORY${NC}"
    printf "  %-12s %s %3d%%  %6s / %6s  %b\n" \
        "RAM" "$(progress_bar "$MEM_PCT" "$MEM_SEV")" "$MEM_PCT" \
        "$(human_kb "$MEM_USED")" "$(human_kb "$MEM_TOTAL")" "$(sev_icon "$MEM_SEV")"
    if [ "$SWAP_TOTAL" -gt 0 ]; then
        printf "  %-12s %s %3d%%  %6s / %6s  %b\n" \
            "Swap" "$(progress_bar "$SWAP_PCT" "$SWAP_SEV")" "$SWAP_PCT" \
            "$(human_kb "$SWAP_USED")" "$(human_kb "$SWAP_TOTAL")" "$(sev_icon "$SWAP_SEV")"
    fi
    printf "  ${DIM}"; printf '%*s' 56 '' | tr ' ' '─'; printf "${NC}\n"

    echo -e "${BOLD}  LOAD${NC}"
    local lc=""; case "$LOAD_SEV" in ok) lc="$GREEN";; warn) lc="$YELLOW";; crit) lc="$RED";; esac
    printf "  Load avg:    %b%-5s%b  %-5s  %-5s  %b(CPUs: %d)%b  %b\n" \
        "$lc" "$LOAD_1" "$NC" "$LOAD_5" "$LOAD_15" "$DIM" "$LOAD_CPUS" "$NC" "$(sev_icon "$LOAD_SEV")"
    printf "  ${DIM}"; printf '%*s' 56 '' | tr ' ' '─'; printf "${NC}\n"

    echo -e "${BOLD}  SERVICES${NC}"
    if [ "$SVC_FAILED_COUNT" -eq 0 ]; then
        echo -e "  ${GREEN}✓${NC} All services running"
    else
        echo -e "  ${RED}✗${NC} ${SVC_FAILED_COUNT} failed:"
        for name in "${SVC_FAILED_NAMES[@]}"; do
            echo -e "    ${RED}${name}${NC}"
        done
    fi
    printf "  ${DIM}"; printf '%*s' 56 '' | tr ' ' '─'; printf "${NC}\n"

    # Summary
    local sc=""; case "$GLOBAL_STATUS" in ok) sc="$GREEN";; warn) sc="$YELLOW";; crit) sc="$RED";; esac
    local st=""; case "$GLOBAL_STATUS" in ok) st="ALL SYSTEMS OK";; warn) st="WARNINGS DETECTED";; crit) st="CRITICAL ISSUES";; esac
    echo -e "  ${BOLD}STATUS: ${sc}${st}${NC}"
    for issue in "${ISSUES[@]}"; do
        local s="${issue%%:*}" m="${issue#*:}"
        case "$s" in crit) echo -e "    ${RED}●${NC} ${m}";; warn) echo -e "    ${YELLOW}●${NC} ${m}";; esac
    done
    echo ""
}

# ── Output: JSON ───────────────────────────────────

output_json() {
    echo '{'

    # System
    printf '  "system": {"hostname": "%s", "kernel": "%s", "date": "%s", "uptime_seconds": %s},\n' \
        "$HOSTNAME_STR" "$KERNEL_STR" "$DATE_STR" "$UPTIME_SEC"

    # Status
    printf '  "status": "%s",\n' "$GLOBAL_STATUS"

    # Issues
    echo '  "issues": ['
    local first=true
    for issue in "${ISSUES[@]}"; do
        local s="${issue%%:*}" m="${issue#*:}"
        $first || echo ','
        printf '    {"severity": "%s", "message": "%s"}' "$s" "$m"
        first=false
    done
    echo ''
    echo '  ],'

    # Disk
    echo '  "disk": ['
    for i in "${!DISK_MOUNT[@]}"; do
        [ "$i" -gt 0 ] && echo ','
        printf '    {"mount": "%s", "size_kb": %s, "used_kb": %s, "avail_kb": %s, "percent": %s, "status": "%s"}' \
            "${DISK_MOUNT[$i]}" "${DISK_SIZE[$i]}" "${DISK_USED[$i]}" "${DISK_AVAIL[$i]}" "${DISK_PCT[$i]}" "${DISK_SEV[$i]}"
    done
    echo ''
    echo '  ],'

    # Memory
    printf '  "memory": {\n'
    printf '    "ram": {"total_kb": %s, "used_kb": %s, "available_kb": %s, "percent": %s, "status": "%s"},\n' \
        "$MEM_TOTAL" "$MEM_USED" "$MEM_AVAIL" "$MEM_PCT" "$MEM_SEV"
    printf '    "swap": {"total_kb": %s, "used_kb": %s, "free_kb": %s, "percent": %s, "status": "%s"}\n' \
        "$SWAP_TOTAL" "$SWAP_USED" "$SWAP_FREE" "$SWAP_PCT" "$SWAP_SEV"
    echo '  },'

    # Load
    printf '  "load": {"avg_1": %s, "avg_5": %s, "avg_15": %s, "cpus": %s, "status": "%s"},\n' \
        "$LOAD_1" "$LOAD_5" "$LOAD_15" "$LOAD_CPUS" "$LOAD_SEV"

    # Services
    echo '  "services": {'
    printf '    "failed_count": %s,\n' "$SVC_FAILED_COUNT"
    echo -n '    "failed": ['
    local first=true
    for name in "${SVC_FAILED_NAMES[@]}"; do
        $first || echo -n ', '
        printf '"%s"' "$name"
        first=false
    done
    echo ']'
    echo '  }'

    echo '}'
}

# ── Output: HTML ───────────────────────────────────

output_html() {
    cat << 'EOF'
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>System Health Report</title>
<style>
*{margin:0;padding:0;box-sizing:border-box}
body{font-family:system-ui,-apple-system,monospace;background:#1a1a2e;color:#eee;padding:20px;max-width:800px;margin:0 auto}
h1{font-size:1.4em;margin-bottom:4px} .sub{color:#888;font-size:.9em;margin-bottom:16px}
.sec{background:#16213e;border-radius:8px;padding:15px;margin-bottom:12px}
.sec h2{font-size:1.05em;margin-bottom:8px;color:#a8d8ea}
table{width:100%;border-collapse:collapse} th,td{text-align:left;padding:5px 8px}
th{color:#888;font-weight:normal;font-size:.85em;border-bottom:1px solid #333}
.bar{background:#333;border-radius:4px;height:16px;position:relative;min-width:120px}
.fill{border-radius:4px;height:100%} .btxt{position:absolute;right:5px;top:0;font-size:.75em;line-height:16px}
.ok{color:#4ecca3} .warn{color:#f0c040} .crit{color:#e84545}
.f-ok .fill{background:#4ecca3} .f-warn .fill{background:#f0c040} .f-crit .fill{background:#e84545}
.banner{padding:12px;border-radius:8px;margin-bottom:12px;font-weight:bold;font-size:1.1em}
.b-ok{background:#1b4332;color:#4ecca3} .b-warn{background:#3d2e00;color:#f0c040} .b-crit{background:#3d0000;color:#e84545}
.issues{list-style:none;padding-left:8px;font-size:.9em;margin-top:5px;font-weight:normal}
.issues li::before{content:"● "} .issues li.crit::before{color:#e84545} .issues li.warn::before{color:#f0c040}
.ts{color:#555;font-size:.8em;text-align:center;margin-top:12px}
</style>
</head>
<body>
EOF

    printf '<h1>System Health Report</h1>\n'
    printf '<p class="sub">%s &mdash; %s &mdash; Kernel: %s &mdash; Up: %s</p>\n' \
        "$HOSTNAME_STR" "$DATE_STR" "$KERNEL_STR" "$UPTIME_STR"

    # Banner
    local bc="" bt=""
    case "$GLOBAL_STATUS" in ok) bc="b-ok"; bt="ALL SYSTEMS OK";; warn) bc="b-warn"; bt="WARNINGS DETECTED";; crit) bc="b-crit"; bt="CRITICAL ISSUES";; esac
    printf '<div class="banner %s">%s' "$bc" "$bt"
    if [ ${#ISSUES[@]} -gt 0 ]; then
        echo '<ul class="issues">'
        for issue in "${ISSUES[@]}"; do
            local s="${issue%%:*}" m="${issue#*:}"
            printf '  <li class="%s">%s</li>\n' "$s" "$m"
        done
        echo '</ul>'
    fi
    echo '</div>'

    # Disk
    echo '<div class="sec"><h2>Disk Usage</h2><table>'
    echo '<tr><th>Mount</th><th>Used / Size</th><th>Usage</th><th>Status</th></tr>'
    for i in "${!DISK_MOUNT[@]}"; do
        printf '<tr><td>%s</td><td>%s / %s</td>' \
            "${DISK_MOUNT[$i]}" "$(human_kb "${DISK_USED[$i]}")" "$(human_kb "${DISK_SIZE[$i]}")"
        printf '<td><div class="bar f-%s"><div class="fill" style="width:%d%%"></div><span class="btxt">%d%%</span></div></td>' \
            "${DISK_SEV[$i]}" "${DISK_PCT[$i]}" "${DISK_PCT[$i]}"
        printf '<td class="%s">%s</td></tr>\n' "${DISK_SEV[$i]}" "${DISK_SEV[$i]^^}"
    done
    echo '</table></div>'

    # Memory
    echo '<div class="sec"><h2>Memory</h2><table>'
    echo '<tr><th>Type</th><th>Used / Total</th><th>Usage</th><th>Status</th></tr>'

    printf '<tr><td>RAM</td><td>%s / %s</td>' "$(human_kb "$MEM_USED")" "$(human_kb "$MEM_TOTAL")"
    printf '<td><div class="bar f-%s"><div class="fill" style="width:%d%%"></div><span class="btxt">%d%%</span></div></td>' \
        "$MEM_SEV" "$MEM_PCT" "$MEM_PCT"
    printf '<td class="%s">%s</td></tr>\n' "$MEM_SEV" "${MEM_SEV^^}"

    if [ "$SWAP_TOTAL" -gt 0 ]; then
        printf '<tr><td>Swap</td><td>%s / %s</td>' "$(human_kb "$SWAP_USED")" "$(human_kb "$SWAP_TOTAL")"
        printf '<td><div class="bar f-%s"><div class="fill" style="width:%d%%"></div><span class="btxt">%d%%</span></div></td>' \
            "$SWAP_SEV" "$SWAP_PCT" "$SWAP_PCT"
        printf '<td class="%s">%s</td></tr>\n' "$SWAP_SEV" "${SWAP_SEV^^}"
    fi
    echo '</table></div>'

    # Load
    echo '<div class="sec"><h2>Load Average</h2>'
    printf '<p class="%s">%s &nbsp; %s &nbsp; %s &nbsp; <span style="color:#888">(CPUs: %d)</span> &mdash; %s</p>' \
        "$LOAD_SEV" "$LOAD_1" "$LOAD_5" "$LOAD_15" "$LOAD_CPUS" "${LOAD_SEV^^}"
    echo '</div>'

    # Services
    echo '<div class="sec"><h2>Services</h2>'
    if [ "$SVC_FAILED_COUNT" -eq 0 ]; then
        echo '<p class="ok">✓ All services running</p>'
    else
        printf '<p class="crit">✗ %d failed service(s):</p><ul class="issues">' "$SVC_FAILED_COUNT"
        for name in "${SVC_FAILED_NAMES[@]}"; do
            printf '<li class="crit">%s</li>' "$name"
        done
        echo '</ul>'
    fi
    echo '</div>'

    printf '<p class="ts">Generated by health_report.sh at %s</p>\n' "$DATE_STR"
    echo '</body></html>'
}

# ── Main ───────────────────────────────────────────

collect_all
evaluate

if [ -n "$OUTPUT_FILE" ]; then
    case "$FORMAT" in
        terminal) NO_COLOR=1; setup_colors; output_terminal > "$OUTPUT_FILE" ;;
        json)     output_json > "$OUTPUT_FILE" ;;
        html)     output_html > "$OUTPUT_FILE" ;;
    esac
    echo "Report written to: $OUTPUT_FILE" >&2
else
    case "$FORMAT" in
        terminal) output_terminal ;;
        json)     output_json ;;
        html)     output_html ;;
    esac
fi
```

---

## 7. Validar la salida

### Validar JSON

```bash
# Con jq (método más común)
./health_report.sh --format json | jq . > /dev/null && echo "JSON válido"

# Verificar campos específicos
./health_report.sh --format json | jq '.status'
./health_report.sh --format json | jq '.disk[] | .mount + ": " + (.percent|tostring) + "%"'
./health_report.sh --format json | jq '.issues[] | .message'

# Con Python
./health_report.sh --format json | python3 -m json.tool > /dev/null && echo "JSON válido"
```

### Validar HTML

```bash
# Verificar que se puede parsear
./health_report.sh --format html | python3 -c "
import sys
from html.parser import HTMLParser
parser = HTMLParser()
parser.feed(sys.stdin.read())
print('HTML parseable')
"

# Abrir en el navegador
./health_report.sh --format html --output /tmp/health.html
xdg-open /tmp/health.html     # Linux
# open /tmp/health.html        # macOS
```

---

## 8. Errores comunes

### Error 1: JSON con coma trailing

```bash
# MAL — coma después del último elemento
echo '{"a": 1, "b": 2,}'      # JSON inválido

# BIEN — controlar la coma con flag
local first=true
for item in "${array[@]}"; do
    $first || echo ','
    echo -n "  \"$item\""
    first=false
done
```

### Error 2: Valores numéricos como strings en JSON

```bash
# MAL — porcentaje entre comillas
printf '"percent": "%s"' "$pct"
# Produce: "percent": "84"  ← string, no número

# BIEN — sin comillas para números
printf '"percent": %s' "$pct"
# Produce: "percent": 84    ← número
```

### Error 3: No escapar caracteres especiales en JSON/HTML

```bash
# MAL — si el hostname contiene comillas o <>, el output se rompe
printf '"hostname": "%s"' "$HOSTNAME"
# Si HOSTNAME='server "A"' → JSON roto

# BIEN — escapar
printf '"hostname": "%s"' "$(json_escape "$HOSTNAME")"

# En HTML, escapar <, >, &, "
html_escape() {
    local str="$1"
    str="${str//&/&amp;}"
    str="${str//</&lt;}"
    str="${str//>/&gt;}"
    str="${str//\"/&quot;}"
    echo "$str"
}
```

### Error 4: Generar HTML sin Content-Type al enviar por email

```bash
# MAL — el email muestra el HTML como texto plano
./health_report.sh --format html | mail -s "Report" admin@e.com

# BIEN — especificar Content-Type
./health_report.sh --format html | \
    mail -a "Content-Type: text/html; charset=UTF-8" \
    -s "Health Report" admin@e.com
```

### Error 5: Mezclar lógica de recolección con formato

```bash
# MAL — formateo dentro de la recolección
collect_disk() {
    df -h | while read line; do
        echo -e "${RED}$line${NC}"    # ¡Colores en recolección!
    done
}

# BIEN — recolección pura, formateo separado
collect_disk() {
    # Solo datos, sin formato
    df -P -k ... | while read mount size used avail pct; do
        DISK_MOUNT+=("$mount")
        # ...
    done
}
```

---

## 9. Cheatsheet

```
EJECUCIÓN
  ./health_report.sh                         Terminal (default)
  ./health_report.sh --format json           JSON a stdout
  ./health_report.sh --format html -o f.html HTML a archivo
  ./health_report.sh --format json | jq .    Validar y formatear JSON

JSON EN BASH
  printf '"key": "%s"' "$val"                String
  printf '"key": %s' "$num"                  Número (sin comillas)
  printf '"key": %s' "true"                  Boolean
  $first || echo ','                         Evitar coma trailing
  json_escape "$str"                         Escapar " \ \n

HTML EN BASH
  cat << 'EOF' ... EOF                       Heredoc para HTML estático
  html_escape "$str"                         Escapar < > & "
  printf '<td>%s</td>' "$val"               Celda de tabla

VALIDACIÓN
  jq . < file.json                           Validar JSON
  python3 -m json.tool < file.json           Validar JSON (alt)
  xdg-open file.html                         Abrir HTML en navegador

ARQUITECTURA
  collect_all() → datos en variables/arrays
  evaluate()    → severidades en variables
  output_X()    → formatear según modo
```

---

## 10. Ejercicios

### Ejercicio 1: Generar JSON manualmente y validar

**Objetivo**: Practicar la generación de JSON correcto desde Bash.

```bash
# 1. Crear un script que genere JSON con datos del sistema
cat > /tmp/json_test.sh << 'SCRIPT'
#!/bin/bash
set -uo pipefail

# Recolectar datos
hostname=$(cat /proc/sys/kernel/hostname)
kernel=$(cut -d' ' -f3 /proc/version)
date_str=$(date '+%Y-%m-%dT%H:%M:%S')
uptime_sec=$(awk '{print int($1)}' /proc/uptime)
cpus=$(nproc)

# Generar JSON
echo '{'
printf '  "hostname": "%s",\n' "$hostname"
printf '  "kernel": "%s",\n' "$kernel"
printf '  "date": "%s",\n' "$date_str"
printf '  "uptime_seconds": %d,\n' "$uptime_sec"
printf '  "cpus": %d,\n' "$cpus"

# Array de filesystems
echo '  "filesystems": ['
first=true
df -P -k --output=target,pcent -x tmpfs -x devtmpfs -x squashfs -x overlay -x efivarfs 2>/dev/null | \
    tail -n +2 | while read -r mount pct; do
        pct=${pct%%%}
        $first || echo ','
        printf '    {"mount": "%s", "percent": %s}' "$mount" "$pct"
        first=false
    done
echo ''
echo '  ]'
echo '}'
SCRIPT
chmod +x /tmp/json_test.sh

# 2. Ejecutar y ver la salida
echo "=== Raw output ==="
/tmp/json_test.sh

# 3. Validar con jq (si está instalado)
echo ""
echo "=== Validation ==="
if command -v jq &>/dev/null; then
    /tmp/json_test.sh | jq . && echo "JSON: VALID" || echo "JSON: INVALID"
else
    /tmp/json_test.sh | python3 -m json.tool > /dev/null 2>&1 \
        && echo "JSON: VALID" || echo "JSON: INVALID"
fi

# 4. Extraer datos con jq
echo ""
echo "=== Queries ==="
if command -v jq &>/dev/null; then
    /tmp/json_test.sh | jq -r '.hostname'
    /tmp/json_test.sh | jq '.filesystems[] | select(.percent > 50)'
    /tmp/json_test.sh | jq '[.filesystems[].percent] | max'
fi

# 5. Limpiar
rm /tmp/json_test.sh
```

**Pregunta de reflexión**: El bucle `while read` dentro del pipe para generar el array de filesystems tiene un problema con la variable `first`. ¿La coma se gestiona correctamente? ¿Por qué podría fallar y cómo lo solucionarías?

> El pipe (`df ... | while read`) crea un subshell. La variable `first` se modifica dentro del subshell pero esa modificación no es visible: `first` vuelve a `true` en cada iteración desde la perspectiva del shell padre. El resultado es que **ningún** filesystem tiene coma antes (falta la coma entre elementos). Solución: usar process substitution (`while read ...; done < <(df ...)`) para que el while corra en el shell principal, o pre-calcular el número de filesystems y usar un contador para la coma.

---

### Ejercicio 2: Generar un reporte HTML mínimo

**Objetivo**: Crear un HTML con datos del sistema que se pueda abrir en un navegador.

```bash
# 1. Crear el generador HTML
cat > /tmp/html_test.sh << 'SCRIPT'
#!/bin/bash
set -uo pipefail

hostname=$(cat /proc/sys/kernel/hostname)
date_str=$(date '+%Y-%m-%d %H:%M:%S')

cat << EOF
<!DOCTYPE html>
<html>
<head>
<meta charset="UTF-8">
<title>Health Check - $hostname</title>
<style>
  body { font-family: monospace; background: #1a1a2e; color: #eee; padding: 20px; max-width: 700px; margin: auto; }
  h1 { color: #a8d8ea; }
  table { width: 100%; border-collapse: collapse; margin: 15px 0; }
  th, td { padding: 8px; text-align: left; border-bottom: 1px solid #333; }
  th { color: #888; }
  .ok { color: #4ecca3; } .warn { color: #f0c040; } .crit { color: #e84545; }
  .bar { background: #333; border-radius: 4px; height: 14px; }
  .fill { border-radius: 4px; height: 100%; }
  .f-ok { background: #4ecca3; } .f-warn { background: #f0c040; } .f-crit { background: #e84545; }
</style>
</head>
<body>
<h1>System Health: $hostname</h1>
<p style="color:#888">$date_str</p>

<h2>Disk Usage</h2>
<table>
<tr><th>Mount</th><th>Usage</th><th>Percent</th></tr>
EOF

# Generar filas de disco
df -P -k --output=target,pcent -x tmpfs -x devtmpfs -x squashfs -x overlay -x efivarfs 2>/dev/null | \
    tail -n +2 | while read -r mount pct; do
        pct=${pct%%%}
        sev="ok"
        [ "$pct" -ge 90 ] && sev="crit"
        [ "$pct" -ge 80 ] && [ "$pct" -lt 90 ] && sev="warn"
        fc="f-$sev"

        echo "<tr>"
        echo "  <td>$mount</td>"
        echo "  <td><div class=\"bar\"><div class=\"fill $fc\" style=\"width:${pct}%\"></div></div></td>"
        echo "  <td class=\"$sev\">${pct}%</td>"
        echo "</tr>"
    done

cat << 'EOF'
</table>

<h2>Memory</h2>
<table>
<tr><th>Type</th><th>Used</th><th>Total</th><th>Percent</th></tr>
EOF

# Memoria
awk '
/^MemTotal:/     {mt=$2}
/^MemAvailable:/ {ma=$2}
/^SwapTotal:/    {st=$2}
/^SwapFree:/     {sf=$2}
END {
    mu=mt-ma; mp=(mt>0)?int(mu*100/mt):0
    su=st-sf; sp=(st>0)?int(su*100/st):0

    ms="ok"; if(mp>=95) ms="crit"; else if(mp>=80) ms="warn"
    ss="ok"; if(sp>=80) ss="crit"; else if(sp>=50) ss="warn"

    printf "<tr><td>RAM</td><td>%.1fG</td><td>%.1fG</td><td class=\"%s\">%d%%</td></tr>\n", \
        mu/1048576, mt/1048576, ms, mp
    if(st>0)
        printf "<tr><td>Swap</td><td>%.1fG</td><td>%.1fG</td><td class=\"%s\">%d%%</td></tr>\n", \
            su/1048576, st/1048576, ss, sp
}' /proc/meminfo

cat << 'EOF'
</table>
<p style="color:#555;font-size:.8em;text-align:center;margin-top:20px">Generated by health_report.sh</p>
</body>
</html>
EOF
SCRIPT
chmod +x /tmp/html_test.sh

# 2. Generar HTML
/tmp/html_test.sh > /tmp/health-test.html

# 3. Verificar que se generó
ls -la /tmp/health-test.html
echo "Open with: xdg-open /tmp/health-test.html"

# 4. Verificar HTML parseable
python3 -c "
from html.parser import HTMLParser
p = HTMLParser()
p.feed(open('/tmp/health-test.html').read())
print('HTML: parseable')
" 2>/dev/null || echo "Python3 not available for validation"

# 5. Limpiar
rm /tmp/html_test.sh /tmp/health-test.html
```

**Pregunta de reflexión**: El CSS del reporte usa un fondo oscuro (`#1a1a2e`). Si este HTML se envía por email, ¿se verá igual en todos los clientes de correo? ¿Qué limitaciones tiene el HTML en emails?

> No se verá igual. Los clientes de email (Gmail, Outlook, Apple Mail) tienen soporte CSS muy limitado y variable: muchos ignoran `<style>` en `<head>` y solo respetan estilos inline (`style="..."`). Gmail elimina colores de fondo del `<body>`, Outlook usa el motor de Word para renderizar HTML. Para emails robustos, hay que usar tablas para layout (no flexbox/grid), estilos inline en cada elemento, y colores seguros que funcionen tanto en fondo claro como oscuro. El HTML de nuestro reporte necesitaría adaptarse significativamente para email.

---

### Ejercicio 3: Implementar --format con los tres formatos

**Objetivo**: Integrar el flag --format en un script funcional.

```bash
# TAREA: Guardar el script completo de la sección 6 como:
#   ~/health_report.sh
#   chmod +x ~/health_report.sh

# Luego ejecutar los siguientes comandos y verificar cada salida:

# 1. Terminal (default)
#   ~/health_report.sh

# 2. JSON a stdout (validar con jq)
#   ~/health_report.sh --format json | jq .

# 3. JSON a archivo
#   ~/health_report.sh --format json --output /tmp/health.json
#   cat /tmp/health.json | jq '.status'

# 4. HTML a archivo (abrir en navegador)
#   ~/health_report.sh --format html --output /tmp/health.html
#   xdg-open /tmp/health.html

# 5. Consultas sobre el JSON
#   ~/health_report.sh --format json | jq '.disk[] | select(.percent > 50) | .mount'
#   ~/health_report.sh --format json | jq '.memory.ram.percent'
#   ~/health_report.sh --format json | jq '.issues'

# 6. Comparar dos ejecuciones
#   ~/health_report.sh --format json > /tmp/h1.json
#   sleep 5
#   ~/health_report.sh --format json > /tmp/h2.json
#   diff <(jq -S . /tmp/h1.json) <(jq -S . /tmp/h2.json)

# VERIFICAR:
#   a) ¿El JSON es válido? (jq no da error)
#   b) ¿Los números son números (sin comillas)?
#   c) ¿El HTML se abre correctamente en el navegador?
#   d) ¿Las barras de progreso en HTML reflejan los porcentajes reales?
#   e) ¿--format terminal sin -o produce colores, pero con -o no?
```

**Pregunta de reflexión**: Si quisieras almacenar estos reportes JSON históricamente para detectar tendencias (ej: disco creciendo), ¿dónde y cómo los guardarías? ¿Qué herramienta usarías para consultar "¿cuál fue el uso de disco de / en los últimos 30 días?"?

> Opciones de almacenamiento: (1) archivos JSON individuales con nombre de fecha (`health-2026-03-21.json`) en un directorio — simple, usar `jq` en bucle para consultar. (2) Append a un archivo JSONL (JSON Lines, un JSON por línea) — eficiente para series temporales, consultar con `jq -s`. (3) Insertar en SQLite — `jq` para extraer valores y `sqlite3` para insertar; permite queries SQL sobre el histórico. Para la consulta de 30 días: con JSONL sería `cat health.jsonl | jq -r 'select(.system.date > "2026-02-19") | "\(.system.date) \(.disk[] | select(.mount == "/") | .percent)"'`. Con SQLite: `SELECT date, disk_root_pct FROM health WHERE date > date('now', '-30 days')`. SQLite es la mejor opción si necesitas queries frecuentes.

---

> **Siguiente tema**: T04 — Mejora 2: interfaz TUI con ratatui (paneles de disco, memoria, servicios, logs recientes).
