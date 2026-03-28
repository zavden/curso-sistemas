# T02 — Zonas horarias

## Cómo maneja Linux las zonas horarias

El kernel de Linux trabaja internamente en **UTC**. La zona horaria es
una capa de presentación que convierte UTC a hora local:

```bash
# El kernel almacena el tiempo en UTC (segundos desde epoch):
date +%s
# 1710689400

# La zona horaria convierte ese timestamp a hora local:
TZ=UTC date
# Tue Mar 17 15:30:00 UTC 2026

TZ=America/New_York date
# Tue Mar 17 11:30:00 EDT 2026

TZ=Europe/Madrid date
# Tue Mar 17 16:30:00 CET 2026

# El mismo instante (epoch), diferente representación
```

## Ver la zona horaria actual

```bash
# Con timedatectl:
timedatectl
# Time zone: America/Mexico_City (CST, -0600)

# Con date:
date +%Z    # abreviación: CST, UTC, CET
date +%z    # offset: -0600, +0000, +0100

# El archivo que define la zona:
ls -la /etc/localtime
# /etc/localtime -> /usr/share/zoneinfo/America/Mexico_City

# También se puede consultar:
cat /etc/timezone    # Debian/Ubuntu (archivo de texto)
# America/Mexico_City

# RHEL puede no tener /etc/timezone — usa solo /etc/localtime
```

## /etc/localtime

```bash
# /etc/localtime es un symlink o copia de un archivo de zona:
ls -la /etc/localtime
# lrwxrwxrwx 1 root root 39 Mar 17 /etc/localtime -> /usr/share/zoneinfo/UTC

# Los archivos de zona están en /usr/share/zoneinfo/:
ls /usr/share/zoneinfo/
# Africa/  America/  Antarctica/  Asia/  Atlantic/  Australia/
# Europe/  Indian/   Pacific/     UTC    US/        Etc/

# Ejemplo de contenido:
ls /usr/share/zoneinfo/America/ | head -10
# Adak
# Anchorage
# Argentina/
# Bogota
# Chicago
# Denver
# Los_Angeles
# Mexico_City
# New_York
# Sao_Paulo

# Los archivos contienen las reglas de DST (horario de verano)
# en formato binario (TZif)
```

## Cambiar la zona horaria

### Con timedatectl (recomendado)

```bash
# Listar zonas disponibles:
timedatectl list-timezones
# Africa/Abidjan
# Africa/Algiers
# America/Bogota
# America/Mexico_City
# Europe/Madrid
# ...

# Filtrar:
timedatectl list-timezones | grep America
timedatectl list-timezones | grep -i mexico

# Cambiar la zona:
sudo timedatectl set-timezone America/Mexico_City

# Verificar:
timedatectl
# Time zone: America/Mexico_City (CST, -0600)
date
# Tue Mar 17 09:30:00 CST 2026
```

### Manualmente (alternativa)

```bash
# Crear el symlink directamente:
sudo ln -sf /usr/share/zoneinfo/America/Mexico_City /etc/localtime

# En Debian, también actualizar /etc/timezone:
echo "America/Mexico_City" | sudo tee /etc/timezone

# En Debian, la herramienta interactiva:
sudo dpkg-reconfigure tzdata
# Menú gráfico para seleccionar región y ciudad
```

## La variable TZ

La variable de entorno `TZ` permite cambiar la zona horaria **por
proceso**, sin afectar al sistema:

```bash
# Cambiar solo para un comando:
TZ=UTC date
# Tue Mar 17 15:30:00 UTC 2026

TZ=Asia/Tokyo date
# Wed Mar 18 00:30:00 JST 2026

TZ=America/New_York date
# Tue Mar 17 11:30:00 EDT 2026

# Cambiar para toda la sesión:
export TZ=America/Bogota
date
# Tue Mar 17 10:30:00 -05 2026

# TZ tiene prioridad sobre /etc/localtime
# Esto es POR PROCESO — no afecta a otros usuarios ni servicios
```

```bash
# Formatos de TZ:
TZ=UTC                       # zona por nombre
TZ=America/Mexico_City       # zona con región/ciudad
TZ=CST6CDT                   # abreviación con offset
TZ=EST5EDT,M3.2.0,M11.1.0   # con reglas DST explícitas

# El formato recomendado es siempre Región/Ciudad:
TZ=America/Mexico_City       # ✓ preciso, incluye reglas DST
TZ=CST                       # ✗ ambiguo (CST = Central Standard, China Standard...)
```

### TZ en servicios

```bash
# Los servicios de systemd heredan la zona del sistema (/etc/localtime)
# Para un servicio con zona diferente:

# [Service]
# Environment=TZ=UTC
# El servicio ve UTC aunque el sistema esté en otra zona

# Ejemplo: base de datos siempre en UTC:
sudo systemctl edit postgresql.service
# [Service]
# Environment=TZ=UTC
```

## Horario de verano (DST)

```bash
# DST (Daylight Saving Time) es un cambio de hora estacional
# Los archivos de zona en /usr/share/zoneinfo/ contienen las reglas

# Ver las transiciones DST de una zona:
zdump -v /usr/share/zoneinfo/America/New_York | grep 2026
# America/New_York  Sun Mar  8 06:59:59 2026 UT = Sun Mar  8 01:59:59 2026 EST
# America/New_York  Sun Mar  8 07:00:00 2026 UT = Sun Mar  8 03:00:00 2026 EDT
# America/New_York  Sun Nov  1 05:59:59 2026 UT = Sun Nov  1 01:59:59 2026 EDT
# America/New_York  Sun Nov  1 06:00:00 2026 UT = Sun Nov  1 01:00:00 2026 EST

# Implicaciones:
# - Al cambiar a DST, una hora desaparece (01:59 → 03:00)
# - Al volver, una hora se repite (01:59 → 01:00)
# - Los timestamps en logs pueden ser ambiguos durante la transición
# - Los cron jobs en esa hora pueden ejecutarse 0 o 2 veces

# Por eso los servidores deben usar UTC:
# - No hay DST en UTC
# - Los logs siempre son monotónicos
# - No hay ambigüedad
```

```bash
# Actualizar las reglas de zonas horarias:
# Los gobiernos cambian las reglas DST con frecuencia
# Las actualizaciones llegan a través del paquete tzdata:
sudo apt install tzdata       # Debian
sudo dnf install tzdata       # RHEL

# Verificar la versión de tzdata:
dpkg -s tzdata 2>/dev/null | grep Version    # Debian
rpm -q tzdata 2>/dev/null                     # RHEL
```

## Servidores: ¿UTC o zona local?

```bash
# RECOMENDACIÓN: usar UTC en servidores

# Ventajas de UTC:
# 1. Sin DST — no hay saltos de hora
# 2. Logs consistentes — fácil correlacionar entre servidores globales
# 3. Sin ambigüedad — 01:30 UTC siempre es 01:30 UTC
# 4. Bases de datos — almacenar timestamps en UTC, convertir en la app
# 5. Cron — sin comportamiento inesperado en cambios DST

# Cuándo usar zona local:
# - Estaciones de trabajo / laptops (comodidad del usuario)
# - Servidores que solo operan en una zona
# - Requisitos legales o de auditoría que exigen hora local

# Configurar UTC:
sudo timedatectl set-timezone UTC
```

## RTC — UTC vs hora local

```bash
# El RTC puede estar en UTC o en hora local:
timedatectl | grep "RTC in local"
# RTC in local TZ: no    ← en UTC (recomendado)

# Cambiar a hora local (NO recomendado):
sudo timedatectl set-local-rtc true

# Cambiar a UTC (recomendado):
sudo timedatectl set-local-rtc false

# ¿Por qué UTC en el RTC?
# - En dual-boot con Windows, Windows espera hora local en el RTC
#   Linux puede adaptarse a ambos
# - Si el RTC está en hora local y hay DST,
#   pueden ocurrir problemas al arrancar durante una transición
```

## Trabajar con zonas horarias en scripts

```bash
# Convertir entre zonas:
# Hora actual en UTC:
date -u
# Tue Mar 17 15:30:00 UTC 2026

# Hora actual en otra zona:
TZ=America/New_York date
# Tue Mar 17 11:30:00 EDT 2026

# Convertir un timestamp específico:
TZ=America/New_York date -d "2026-03-17 15:30:00 UTC"
# Tue Mar 17 11:30:00 EDT 2026

# En logs, siempre registrar en UTC o con offset:
date -u +"%Y-%m-%dT%H:%M:%S%z"
# 2026-03-17T15:30:00+0000

date --iso-8601=seconds
# 2026-03-17T15:30:00+00:00
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
```

### Ejercicio 2 — Explorar zonas

```bash
# 1. ¿Cuántas zonas horarias hay disponibles?
timedatectl list-timezones | wc -l

# 2. Mostrar la hora actual en 3 zonas diferentes:
for tz in UTC America/New_York Asia/Tokyo; do
    echo "$tz: $(TZ=$tz date)"
done
```

### Ejercicio 3 — DST

```bash
# ¿Tu zona tiene horario de verano?
zdump -v /etc/localtime 2>/dev/null | grep 2026 || echo "Sin transiciones DST"
```
