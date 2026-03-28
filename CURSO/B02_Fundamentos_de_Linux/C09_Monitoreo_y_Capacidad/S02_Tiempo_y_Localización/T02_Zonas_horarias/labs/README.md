# Lab — Zonas horarias

## Objetivo

Entender zonas horarias en Linux: /etc/localtime como symlink a
/usr/share/zoneinfo/, ver la zona actual, explorar zonas disponibles,
variable TZ para cambio por proceso, horario de verano (DST) con
zdump, por que usar UTC en servidores, RTC en UTC vs hora local,
y conversion entre zonas en scripts.

## Prerequisitos

- Entorno del curso levantado (`docker compose up -d`)

---

## Parte 1 — Zona actual y archivos

### Objetivo

Entender como Linux implementa las zonas horarias.

### Paso 1.1: Zona actual

```bash
docker compose exec debian-dev bash -c '
echo "=== Zona horaria actual ==="
echo ""
echo "--- Con date ---"
echo "  Abreviacion: $(date +%Z)"
echo "  Offset:      $(date +%z)"
echo "  Fecha:       $(date)"
echo ""
echo "--- /etc/localtime ---"
echo "Es el archivo que define la zona del sistema:"
ls -la /etc/localtime
echo ""
echo "--- /etc/timezone (Debian) ---"
if [[ -f /etc/timezone ]]; then
    echo "  $(cat /etc/timezone)"
else
    echo "  (no existe)"
fi
echo ""
echo "El kernel trabaja internamente en UTC."
echo "La zona horaria es una capa de presentacion."
'
```

### Paso 1.2: /usr/share/zoneinfo

```bash
docker compose exec debian-dev bash -c '
echo "=== /usr/share/zoneinfo/ ==="
echo ""
echo "Directorio con todas las zonas horarias:"
echo ""
echo "--- Regiones ---"
ls /usr/share/zoneinfo/ | grep -E "^[A-Z]" | head -15
echo ""
echo "--- Ejemplo: America ---"
ls /usr/share/zoneinfo/America/ | head -15
echo "..."
echo "Total en America: $(ls /usr/share/zoneinfo/America/ | wc -l)"
echo ""
echo "--- Total de zonas ---"
find /usr/share/zoneinfo/ -type f | wc -l
echo "archivos de zona"
echo ""
echo "Los archivos contienen reglas de DST en formato binario (TZif)."
echo "/etc/localtime es un symlink a uno de estos archivos."
'
```

### Paso 1.3: Ver zonas de Latinoamerica

```bash
docker compose exec debian-dev bash -c '
echo "=== Zonas de Latinoamerica ==="
echo ""
echo "--- Mexico ---"
ls /usr/share/zoneinfo/America/ | grep -i mexico
echo ""
echo "--- Argentina ---"
ls /usr/share/zoneinfo/America/Argentina/ 2>/dev/null || \
    ls /usr/share/zoneinfo/America/ | grep -i argentina
echo ""
echo "--- Colombia, Chile, Peru ---"
for city in Bogota Santiago Lima; do
    [[ -f /usr/share/zoneinfo/America/$city ]] && echo "  America/$city"
done
echo ""
echo "--- Hora actual en varias zonas ---"
for tz in America/Mexico_City America/Bogota America/Santiago America/Buenos_Aires; do
    printf "  %-25s %s\n" "$tz" "$(TZ=$tz date +%H:%M)"
done
'
```

---

## Parte 2 — Variable TZ y DST

### Objetivo

Usar la variable TZ y entender el horario de verano.

### Paso 2.1: Variable TZ

```bash
docker compose exec debian-dev bash -c '
echo "=== Variable TZ ==="
echo ""
echo "TZ cambia la zona horaria POR PROCESO, sin afectar al sistema."
echo ""
echo "--- Zona del sistema ---"
echo "  $(date)"
echo ""
echo "--- Con TZ ---"
echo "  UTC:            $(TZ=UTC date)"
echo "  New York:       $(TZ=America/New_York date)"
echo "  Tokyo:          $(TZ=Asia/Tokyo date)"
echo "  Madrid:         $(TZ=Europe/Madrid date)"
echo "  Mexico City:    $(TZ=America/Mexico_City date)"
echo ""
echo "El mismo instante (epoch), diferente representacion:"
echo "  Epoch: $(date +%s)"
echo ""
echo "TZ tiene prioridad sobre /etc/localtime."
echo "Solo afecta al proceso actual, no a otros usuarios."
'
```

### Paso 2.2: TZ en sesion y servicios

```bash
docker compose exec debian-dev bash -c '
echo "=== TZ en sesion ==="
echo ""
echo "--- Cambiar para toda la sesion ---"
echo "  export TZ=America/Bogota"
echo "  (todos los comandos usaran esa zona)"
echo ""
echo "--- En servicios de systemd ---"
echo "  [Service]"
echo "  Environment=TZ=UTC"
echo "  (el servicio ve UTC aunque el sistema no)"
echo ""
echo "--- Ejemplo: base de datos siempre en UTC ---"
echo "  sudo systemctl edit postgresql.service"
echo "  [Service]"
echo "  Environment=TZ=UTC"
echo ""
echo "--- Formatos de TZ ---"
echo "  TZ=UTC                    nombre simple"
echo "  TZ=America/Mexico_City    region/ciudad (RECOMENDADO)"
echo "  TZ=CST6CDT                abreviacion con offset"
echo ""
echo "  Usar siempre Region/Ciudad:"
echo "  America/Mexico_City  ← preciso, incluye reglas DST"
echo "  CST                  ← ambiguo (Central Standard, China Standard...)"
'
```

### Paso 2.3: Horario de verano (DST)

```bash
docker compose exec debian-dev bash -c '
echo "=== Horario de verano (DST) ==="
echo ""
echo "DST = Daylight Saving Time"
echo "Cambio de hora estacional en algunas zonas."
echo ""
echo "--- Ver transiciones con zdump ---"
echo ""
echo "New York (tiene DST):"
zdump -v /usr/share/zoneinfo/America/New_York 2>/dev/null | grep 2026 | head -4
echo ""
echo "Bogota (no tiene DST):"
zdump -v /usr/share/zoneinfo/America/Bogota 2>/dev/null | grep 2026 | head -2
[[ $(zdump -v /usr/share/zoneinfo/America/Bogota 2>/dev/null | grep 2026 | wc -l) -eq 0 ]] && \
    echo "  (sin transiciones — no usa DST)"
echo ""
echo "--- Implicaciones ---"
echo "  Al cambiar a DST: una hora desaparece (01:59 → 03:00)"
echo "  Al volver: una hora se repite (01:59 → 01:00)"
echo ""
echo "  Problemas:"
echo "    - Logs con timestamps ambiguos"
echo "    - Cron jobs pueden ejecutarse 0 o 2 veces"
echo ""
echo "  Por eso los SERVIDORES deben usar UTC:"
echo "    - No hay DST en UTC"
echo "    - Los logs siempre son monotonicos"
'
```

### Paso 2.4: Actualizacion de reglas DST

```bash
docker compose exec debian-dev bash -c '
echo "=== Actualizacion de reglas DST ==="
echo ""
echo "Los gobiernos cambian las reglas DST con frecuencia."
echo "Las actualizaciones llegan via el paquete tzdata."
echo ""
echo "--- Version de tzdata ---"
dpkg -s tzdata 2>/dev/null | grep -E "^(Package|Version):" || \
    echo "  (dpkg no disponible)"
echo ""
echo "--- Actualizar ---"
echo "  apt install tzdata       (Debian)"
echo "  dnf install tzdata       (RHEL)"
echo ""
echo "Es importante mantener tzdata actualizado en servidores"
echo "que trabajan con zonas que cambian sus reglas."
'
```

---

## Parte 3 — Servidores y scripts

### Objetivo

Entender la configuracion recomendada para servidores.

### Paso 3.1: UTC en servidores

```bash
docker compose exec debian-dev bash -c '
echo "=== UTC en servidores ==="
echo ""
echo "RECOMENDACION: usar UTC en servidores."
echo ""
echo "--- Ventajas de UTC ---"
echo "  1. Sin DST — no hay saltos de hora"
echo "  2. Logs consistentes — facil correlacionar"
echo "  3. Sin ambiguedad — 01:30 UTC siempre es 01:30 UTC"
echo "  4. Bases de datos — almacenar en UTC, convertir en la app"
echo "  5. Cron — sin comportamiento inesperado"
echo ""
echo "--- Cuando usar zona local ---"
echo "  - Estaciones de trabajo (comodidad)"
echo "  - Servidores locales sin interaccion global"
echo "  - Requisitos legales de hora local"
echo ""
echo "--- Este sistema ---"
echo "  Zona: $(cat /etc/timezone 2>/dev/null || readlink /etc/localtime | sed 's|.*/zoneinfo/||')"
'
```

### Paso 3.2: Conversion entre zonas en scripts

```bash
docker compose exec debian-dev bash -c '
echo "=== Conversion entre zonas ==="
echo ""
echo "--- Hora actual en UTC ---"
date -u
echo ""
echo "--- Convertir UTC a otras zonas ---"
UTC_NOW=$(date -u +"%Y-%m-%d %H:%M:%S")
echo "UTC: $UTC_NOW"
echo ""
for tz in America/New_York America/Mexico_City Europe/Madrid Asia/Tokyo; do
    LOCAL=$(TZ=$tz date -d "$UTC_NOW UTC" +"%Y-%m-%d %H:%M:%S %Z" 2>/dev/null)
    printf "  %-25s %s\n" "$tz:" "$LOCAL"
done
echo ""
echo "--- Formatos para logs ---"
echo "  UTC:       $(date -u +%Y-%m-%dT%H:%M:%S%z)"
echo "  ISO 8601:  $(date --iso-8601=seconds 2>/dev/null || date -u +%Y-%m-%dT%H:%M:%SZ)"
echo ""
echo "En logs, siempre registrar en UTC o con offset."
'
```

### Paso 3.3: Debian vs RHEL

```bash
docker compose exec debian-dev bash -c '
echo "=== Debian ==="
echo ""
echo "Zona:"
cat /etc/timezone 2>/dev/null || echo "(no /etc/timezone)"
echo ""
echo "/etc/localtime:"
ls -la /etc/localtime
echo ""
echo "Cambiar zona: timedatectl set-timezone, o:"
echo "  ln -sf /usr/share/zoneinfo/ZONA /etc/localtime"
echo "  echo ZONA > /etc/timezone"
echo "  dpkg-reconfigure tzdata (interactivo)"
'
echo ""
docker compose exec alma-dev bash -c '
echo "=== RHEL ==="
echo ""
echo "Zona:"
readlink /etc/localtime 2>/dev/null | sed "s|.*/zoneinfo/||" || echo "(no symlink)"
echo ""
echo "/etc/localtime:"
ls -la /etc/localtime
echo ""
echo "RHEL usa solo /etc/localtime (no /etc/timezone)."
echo "Cambiar: timedatectl set-timezone"
'
```

### Paso 3.4: Tabla resumen

```bash
docker compose exec debian-dev bash -c '
echo "=== Resumen ==="
echo ""
echo "| Concepto | Descripcion |"
echo "|----------|-------------|"
echo "| /etc/localtime | Symlink al archivo de zona activa |"
echo "| /usr/share/zoneinfo/ | Directorio con todas las zonas |"
echo "| /etc/timezone | Nombre de la zona (solo Debian) |"
echo "| TZ=zona | Cambia zona por proceso |"
echo "| zdump -v | Muestra transiciones DST |"
echo "| tzdata | Paquete con reglas de zonas |"
echo "| UTC | Recomendado para servidores |"
echo ""
echo "--- Diagnostico ---"
echo "  date              ver hora y zona actual"
echo "  date +%Z / +%z    abreviacion y offset"
echo "  TZ=zona date      hora en otra zona"
echo "  zdump -v zona     transiciones DST"
'
```

---

## Limpieza final

No hay recursos que limpiar.

---

## Conceptos reforzados

1. El kernel trabaja en UTC. La zona horaria es una capa
   de presentacion via /etc/localtime → /usr/share/zoneinfo/.

2. La variable TZ cambia la zona por proceso sin afectar
   al sistema. Formato recomendado: Region/Ciudad.

3. DST causa que una hora desaparezca o se repita.
   zdump -v muestra las transiciones. Problemas: logs
   ambiguos, cron ejecutando 0 o 2 veces.

4. Servidores deben usar UTC: sin DST, logs consistentes,
   sin ambiguedad. Convertir a hora local en la app.

5. tzdata contiene las reglas de zonas. Los gobiernos
   cambian DST frecuentemente, mantener actualizado.

6. Debian usa /etc/timezone ademas de /etc/localtime.
   RHEL usa solo /etc/localtime.
