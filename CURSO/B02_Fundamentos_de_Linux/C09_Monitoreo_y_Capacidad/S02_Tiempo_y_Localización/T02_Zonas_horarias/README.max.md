# T02 — Zonas horarias

> **Objetivo:** Entender cómo Linux maneja las zonas horarias, configurar la zona correcta, y usar la variable TZ para cambiar zonas por proceso.

## Cómo maneja Linux las zonas horarias

El kernel trabaja internamente en **UTC** (segundos desde epoch). La zona horaria es solo una capa de presentación:

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                        EL KERNEL SIEMPRE USA UTC                           │
│                                                                              │
│  timestamp = 1710689400 (segundos desde epoch)                             │
│       │                                                                     │
│       ├──► /etc/localtime (zona horaria) ──► hora local                   │
│       │                                                                     │
│       └──► Sin conversión ──► UTC pura                                      │
└─────────────────────────────────────────────────────────────────────────────┘
```

```bash
# UTC (epoch):
date +%s
# 1710689400

# Conversión a diferentes zonas:
TZ=UTC date
# Tue Mar 17 15:30:00 UTC 2026

TZ=America/New_York date
# Tue Mar 17 11:30:00 EDT 2026

TZ=Europe/Madrid date
# Tue Mar 17 16:30:00 CET 2026

# El mismo instante (epoch), diferente representación
```

---

## Ver la zona horaria actual

```bash
# Con timedatectl:
timedatectl
# Time zone: America/Mexico_City (CST, -0600)

# Con date:
date +%Z    # abreviación: CST, UTC, CET
date +%z    # offset: -0600, +0000, +0100

# Archivo de zona:
ls -la /etc/localtime
# /etc/localtime -> /usr/share/zoneinfo/America/Mexico_City

# En Debian:
cat /etc/timezone
# America/Mexico_City

# En RHEL puede no existir /etc/timezone (solo /etc/localtime)
```

---

## Archivos de zona horaria

```bash
# /usr/share/zoneinfo/ contiene las definiciones:
ls /usr/share/zoneinfo/
# Africa/  America/  Antarctica/  Asia/  Atlantic/  Australia/
# Europe/  Indian/   Pacific/  UTC  US/

ls /usr/share/zoneinfo/America/
# Adak          Bogota         Guan            New_York      Scoresbysund
# Anchorage     Buenos_Aires   Halifax         Nome           Shipegan
# Argentina/    Caracas        Havana          Noronha        St_Johns
# Asuncion     Cayenne        Indiana/        Phoenix        St_Kitts
# ...

# El formato es binario (TZif) y contiene:
# - Offset de UTC
# - Reglas DST (horario de verano)
# - Histórico de cambios de zona
```

---

## Cambiar la zona horaria

### Con timedatectl (método recomendado)

```bash
# Listar zonas disponibles:
timedatectl list-timezones

# Filtrar:
timedatectl list-timezones | grep America
timedatectl list-timezones | grep -i mexico
timedatectl list-timezones | grep -i madrid

# Cambiar:
sudo timedatectl set-timezone America/Mexico_City
sudo timedatectl set-timezone UTC
sudo timedatectl set-timezone Europe/Madrid

# Verificar:
timedatectl
date
```

### Manualmente (alternativa)

```bash
# Crear symlink:
sudo ln -sf /usr/share/zoneinfo/America/Mexico_City /etc/localtime

# En Debian, también actualizar /etc/timezone:
echo "America/Mexico_City" | sudo tee /etc/timezone

# Herramienta interactiva:
sudo dpkg-reconfigure tzdata
# Menú para seleccionar región y ciudad
```

---

## La variable TZ

La variable `TZ` cambia la zona horaria **por proceso**, sin afectar al sistema:

```bash
# Cambiar solo para un comando:
TZ=UTC date
TZ=Asia/Tokyo date
TZ=America/New_York date

# Cambiar para toda la sesión:
export TZ=America/Bogota
date

# TZ tiene prioridad sobre /etc/localtime
# Es POR PROCESO — otros usuarios y servicios no se afectan
```

### Formatos de TZ

```
FORMATO                  EJEMPLO                 DESCRIPCIÓN
────────────────────────────────────────────────────────────────
Zona completa           TZ=America/Mexico_City   Recomendado
Zona UTC               TZ=UTC                  UTC puro
Abreviación            TZ=CST6CDT             Ambiguo (evitar)
Con offset             TZ=GMT+5               Offset fijo
Con DST explícito      TZ=EST5EDT,M3.2.0,M11  Reglas manuales

RECOMENDACIÓN:
  ✓ TZ=America/Mexico_City   → preciso, incluye DST
  ✗ TZ=CST                 → ambiguo (CST = Central Standard, China Standard...)
```

### TZ en servicios

```bash
# systemd services heredan TZ del sistema (/etc/localtime)
# Para un servicio con zona diferente:

# [Service]
# Environment=TZ=UTC
# El servicio ve UTC aunque el sistema esté en otra zona

# Ejemplo: base de datos siempre en UTC:
sudo systemctl edit postgresql.service
# [Service]
# Environment=TZ=UTC

# Verificar:
systemctl show postgresql.service -p Environment
```

---

## Horario de verano (DST)

```bash
# DST = Daylight Saving Time (horario de verano)
# Los archivos de zona contienen las reglas históricas

# Ver transiciones DST:
zdump -v /usr/share/zoneinfo/America/New_York | grep 2026
# America/New_York  Sun Mar  8 06:59:59 2026 UT = Sun Mar  8 01:59:59 2026 EST
# America/New_York  Sun Mar  8 07:00:00 2026 UT = Sun Mar  8 03:00:00 2026 EDT
# America/New_York  Sun Nov  1 05:59:59 2026 UT = Sun Nov  1 01:59:59 2026 EDT
# America/New_York  Sun Nov  1 06:00:00 2026 UT = Sun Nov  1 01:00:00 2026 EST
```

### Implicaciones del DST

```
┌─────────────────────────────────────────────────────────────────────────────┐
│  PROBLEMAS DURANTE EL CAMBIO DE DST                                        │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                              │
│  AL AVANZAR EL RELOJ (Marzo):                                             │
│  01:59 → 03:00 (una hora DESAPARECE)                                    │
│  Un evento a las 02:30 nunca ocurre                                         │
│                                                                              │
│  AL RETROCEDER (Noviembre):                                               │
│  01:59 → 01:00 (una hora se REPITE)                                      │
│  Un evento a las 02:30 ocurre DOS veces                                    │
│                                                                              │
│  IMPLICACIONES:                                                             │
│  - Timestamps en logs pueden ser ambiguos                                   │
│  - Cron jobs en esa hora pueden ejecutarse 0 o 2 veces                    │
│  - Certificados TLS con validez exacta pueden fallar                         │
└─────────────────────────────────────────────────────────────────────────────┘
```

### Solución: usar UTC en servidores

```
✓ UTC NO tiene DST
✓ Logs siempre monotónicos
✓ Sin ambigüedad
✓ Fácil correlacionar entre servidores globales
```

### Actualizar reglas de zona

```bash
# Las reglas DST cambian con frecuencia (gobiernos modifican)
# Se actualizan vía paquete tzdata:

sudo apt install tzdata       # Debian/Ubuntu
sudo dnf install tzdata       # RHEL/Fedora

# Ver versión:
dpkg -s tzdata | grep Version    # Debian
rpm -q tzdata                     # RHEL
```

---

## RTC — UTC vs hora local

```bash
# El RTC puede estar en UTC o en hora local:
timedatectl | grep "RTC in local"
# RTC in local TZ: no    ← en UTC (recomendado)
# RTC in local TZ: yes   ← en hora local (NO recomendado)
```

| Configuración | Ventajas | Desventajas |
|--------------|----------|--------------|
| **UTC** (recomendado) | Sin DST, fácil sync entre OS, sin problemas en cambios | Dual-boot con Windows puede desfasar |
| **Hora local** | Compatible con Windows legacy | DST causa problemas, más complejo |

```bash
# Cambiar a hora local (NO recomendado):
sudo timedatectl set-local-rtc true

# Cambiar a UTC (recomendado):
sudo timedatectl set-local-rtc false
```

---

## Servidores: UTC vs zona local

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                    RECOMENDACIÓN: UTC EN SERVIDORES                         │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                              │
│  VENTAJAS DE UTC:                                                          │
│  ✓ Sin DST — no hay saltos de hora                                        │
│  ✓ Logs consistentes — fácil correlacionar entre servidores globales       │
│  ✓ Sin ambigüedad — 01:30 UTC siempre es 01:30 UTC                       │
│  ✓ Bases de datos — almacenar en UTC, convertir en la app                  │
│  ✓ Cron — sin comportamiento inesperado en cambios DST                     │
│                                                                              │
│  ZONA LOCAL EN SERVIDORES CUANDO:                                          │
│  - Requisitos legales o de auditoría que exigen hora local                   │
│  - Servidor dedicado a una sola zona con aplicaciones legacy                │
│  - Estaciones de trabajo / laptops (comodidad del usuario)                │
└─────────────────────────────────────────────────────────────────────────────┘
```

```bash
# Configurar UTC:
sudo timedatectl set-timezone UTC
```

---

## Trabajar con zonas horarias en scripts

```bash
# Hora actual en UTC:
date -u
date -u +"%Y-%m-%d %H:%M:%S UTC"

# Hora actual en otra zona:
TZ=America/New_York date
TZ=Asia/Tokyo date

# Convertir un timestamp específico:
TZ=America/New_York date -d "2026-03-17 15:30:00 UTC"
# Muestra la hora en New York de ese timestamp UTC

# En logs, usar formato ISO 8601:
date -u +"%Y-%m-%dT%H:%M:%S%z"
# 2026-03-17T15:30:00+0000

date --iso-8601=seconds
# 2026-03-17T15:30:00+00:00

# Convertir epoch a formato legible con zona:
date -d @1710689400
date -d @1710689400 -u
TZ=America/New_York date -d @1710689400
```

---

## Quick reference

```
VER ZONA:
  timedatectl | grep "Time zone"
  date +%Z         # abreviación
  date +%z        # offset

CAMBIAR ZONA:
  sudo timedatectl set-timezone America/Mexico_City
  sudo ln -sf /usr/share/zoneinfo/Region/City /etc/localtime

TZ POR PROCESO:
  TZ=UTC date                    # un comando
  export TZ=America/Mexico_City  # sesión

UTC EN SERVIDORES:
  sudo timedatectl set-timezone UTC

RTC:
  timedatectl | grep "RTC in local"
  sudo timedatectl set-local-rtc false   # UTC

DST:
  zdump -v /usr/share/zoneinfo/America/New_York | grep 2026
```

---

## Ejercicios

### Ejercicio 1 — Zona actual

```bash
# 1. ¿Qué zona horaria tiene tu sistema?
timedatectl | grep "Time zone"

# 2. ¿El RTC está en UTC?
timedatectl | grep "RTC in local"

# 3. ¿Cuál es el offset actual?
date +%z
date +%Z

# 4. ¿Qué hora es en UTC?
date -u
```

### Ejercicio 2 — Explorar zonas

```bash
# 1. ¿Cuántas zonas hay?
timedatectl list-timezones | wc -l

# 2. Hora actual en 5 zonas diferentes:
for tz in UTC America/New_York Europe/London Asia/Tokyo Pacific/Auckland; do
    echo "$tz: $(TZ=$tz date '+%H:%M %Z')"
done

# 3. Ver la zona del sistema sin cambiar nada:
cat /etc/localtime | head -1 2>/dev/null || file /etc/localtime
```

### Ejercicio 3 — DST

```bash
# 1. ¿Tu zona tiene DST?
zdump -v /etc/localtime 2>/dev/null | grep 2026 | head -5

# 2. ¿Qué zonas tienen DST en 2026?
timedatectl list-timezones | while read tz; do
    if zdump -v /usr/share/zoneinfo/$tz 2>/dev/null | grep -q "2026"; then
        echo "$tz"
    fi
done | head -10

# 3. Ver transiciones DST de New York:
zdump -v /usr/share/zoneinfo/America/New_York | grep -E "2026|2027"
```

### Ejercicio 4 — TZ en scripts

```bash
# 1. Escribir un script que muestre la hora en múltiples zonas:

cat << 'EOF' > /usr/local/bin/timezones.sh
#!/bin/bash
echo "=== Hora en diferentes zonas horarias ==="
for tz in UTC America/New_York America/Los_Angeles \
          Europe/London Europe/Madrid Asia/Tokyo; do
    echo "$tz: $(TZ=$tz date '+%Y-%m-%d %H:%M:%S %Z')"
done
EOF
chmod +x /usr/local/bin/timezones.sh
/usr/local/bin/timezones.sh

# 2. Convertir un timestamp específico a diferentes zonas:
TS="2026-03-17 15:30:00 UTC"
for tz in UTC America/New_York Europe/Madrid Asia/Tokyo; do
    echo "$tz: $(TZ=$tz date -d "$TS" '+%Y-%m-%d %H:%M:%S %Z')"
done
```

### Ejercicio 5 — UTC en servidor

```bash
# Escenario: configurar un servidor web en UTC

# 1. Ver zona actual:
timedatectl | grep "Time zone"

# 2. Cambiar a UTC:
sudo timedatectl set-timezone UTC

# 3. Verificar:
timedatectl | grep "Time zone"
date

# 4. Configurar una aplicación para UTC:
#    En /etc/environment o en el unit de systemd:
#    Environment=TZ=UTC

# 5. En la base de datos:
#    ALTER SYSTEM SET timezone = 'UTC';
#    SHOW TIMEZONE;
```
