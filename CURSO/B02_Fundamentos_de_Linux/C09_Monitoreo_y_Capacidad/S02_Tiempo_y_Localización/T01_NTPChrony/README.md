# T01 — NTP / Chrony

## Por qué importa la hora exacta

Un reloj incorrecto causa problemas reales en producción:

```bash
# Problemas de un reloj desincronizado:
# - Certificados TLS rechazados ("certificate not yet valid" o "expired")
# - Autenticación Kerberos falla (tolerancia típica: 5 minutos)
# - Logs con timestamps incorrectos → imposible correlacionar incidentes
# - Cron/timers ejecutan a la hora equivocada
# - Base de datos con conflictos de replicación
# - make se confunde con timestamps de archivos (compilación rota)
```

## Dos relojes en el sistema

```bash
# 1. RTC (Real Time Clock) — reloj de hardware
#    Chip en la placa base, funciona con batería cuando el equipo está apagado
#    Mantiene la hora entre reboots
#    Puede estar en UTC o en hora local

# 2. System clock — reloj del kernel
#    Se inicializa desde el RTC al arrancar
#    Es el que usan las aplicaciones
#    Se sincroniza vía NTP

# Ver ambos:
timedatectl
#                Local time: Tue 2026-03-17 15:30:00 UTC
#            Universal time: Tue 2026-03-17 15:30:00 UTC
#                  RTC time: Tue 2026-03-17 15:30:00
#                 Time zone: UTC (UTC, +0000)
# System clock synchronized: yes
#               NTP service: active
#           RTC in local TZ: no

# Ver solo el RTC:
sudo hwclock --show
# 2026-03-17 15:30:00.123456+00:00

# Sincronizar RTC desde el system clock:
sudo hwclock --systohc

# Sincronizar system clock desde el RTC:
sudo hwclock --hctosys
```

## NTP — Network Time Protocol

NTP sincroniza el reloj del sistema con servidores de tiempo en Internet
mediante un protocolo que compensa la latencia de red:

```bash
# Jerarquía de NTP (stratum):
# Stratum 0 — relojes atómicos, GPS (la fuente real)
# Stratum 1 — servidores conectados directamente a stratum 0
# Stratum 2 — servidores sincronizados con stratum 1
# Stratum 3 — servidores sincronizados con stratum 2
# ...
# Stratum 15 — máximo (stratum 16 = no sincronizado)

# Servidores NTP públicos:
# pool.ntp.org          — pool global (rotación DNS)
# 0.pool.ntp.org        — subpool
# time.google.com       — Google
# time.cloudflare.com   — Cloudflare
# time.nist.gov         — NIST (US)
```

## Chrony vs ntpd vs systemd-timesyncd

Linux tiene tres implementaciones de NTP:

```bash
# 1. chrony (chronyd) — RECOMENDADO
#    - Moderno, rápido, preciso
#    - Funciona bien con conexiones intermitentes
#    - Bueno para VMs (maneja saltos de tiempo)
#    - Default en RHEL 8+, Fedora, Arch

# 2. ntpd (ntp) — Legacy
#    - Implementación original de referencia
#    - Más lento para sincronizar
#    - Mejor para stratum 1 servers
#    - Siendo reemplazado por chrony en todas las distros

# 3. systemd-timesyncd — Básico
#    - Incluido en systemd (siempre disponible)
#    - Solo cliente SNTP (simplificado)
#    - Sin capacidad de servidor NTP
#    - Default en Debian/Ubuntu (si no hay chrony/ntpd)
#    - Suficiente para la mayoría de casos

# Ver cuál está activo:
systemctl is-active chronyd 2>/dev/null && echo "chrony"
systemctl is-active ntpd 2>/dev/null && echo "ntpd"
systemctl is-active systemd-timesyncd 2>/dev/null && echo "timesyncd"
```

## timedatectl — La herramienta unificada

```bash
# timedatectl funciona con cualquier implementación de NTP:

# Ver estado:
timedatectl
# System clock synchronized: yes  ← ¿está sincronizado?
# NTP service: active             ← ¿NTP está activo?

# Habilitar NTP:
sudo timedatectl set-ntp true

# Deshabilitar NTP (ajuste manual):
sudo timedatectl set-ntp false

# Ajustar la hora manualmente (solo si NTP está deshabilitado):
sudo timedatectl set-time "2026-03-17 15:30:00"
sudo timedatectl set-time "15:30:00"     # solo hora
sudo timedatectl set-time "2026-03-17"   # solo fecha

# timedatectl show (formato parseable):
timedatectl show
# Timezone=UTC
# NTP=yes
# NTPSynchronized=yes
```

## Chrony — Configuración

### Instalar y habilitar

```bash
# Debian/Ubuntu:
sudo apt install chrony
sudo systemctl enable --now chronyd

# RHEL/Fedora (ya instalado):
sudo systemctl enable --now chronyd

# Al habilitar chrony, systemd-timesyncd se deshabilita automáticamente
```

### /etc/chrony.conf (RHEL) / /etc/chrony/chrony.conf (Debian)

```bash
# Servidores NTP:
pool pool.ntp.org iburst
# pool = usar múltiples servidores del pool
# iburst = enviar 4 requests rápidos al arrancar (sync más rápido)

# O servidores individuales:
server time.google.com iburst
server time.cloudflare.com iburst

# Archivo de drift (corrección del reloj local):
driftfile /var/lib/chrony/drift
# chrony mide cuánto se desvía tu reloj y lo compensa

# Permitir ajuste grande al arrancar:
makestep 1.0 3
# Si la diferencia es > 1 segundo, ajustar de golpe
# Pero solo en los primeros 3 intentos de sync
# Después, solo ajustes graduales (slew)

# RTC sincronizado por chrony:
rtcsync
# chrony actualiza el RTC periódicamente

# Directorio de claves:
keyfile /etc/chrony/chrony.keys

# Log:
logdir /var/log/chrony
```

### chronyc — Control de chrony

```bash
# Ver las fuentes de tiempo:
chronyc sources
# MS Name/IP address     Stratum Poll Reach LastRx Last sample
# ^* time.google.com           1   6   377    34   +234us[ +345us] +/-  15ms
# ^+ time.cloudflare.com       3   6   377    35   -123us[ -234us] +/-  20ms

# Indicadores:
# ^ = servidor, * = seleccionado actual, + = candidato, - = descartado
# Reach = máscara de los últimos 8 intentos (377 = todos exitosos)

# Ver estado detallado de las fuentes:
chronyc sourcestats
# Name/IP Address     NP  NR  Span  Frequency  Freq Skew  Offset  Std Dev
# time.google.com     10   5   540     +0.012      0.150   +234us    89us

# Ver el estado de sincronización:
chronyc tracking
# Reference ID    : D8EF230A (time.google.com)
# Stratum         : 2
# Ref time (UTC)  : Tue Mar 17 15:30:00 2026
# System time     : 0.000000234 seconds fast of NTP time
# Last offset     : +0.000000234 seconds
# RMS offset      : 0.000000345 seconds
# Frequency       : 1.234 ppm slow
# Root delay      : 0.015000000 seconds

# Verificar si está sincronizado:
chronyc waitsync 1 0.1 0.0 1 2>/dev/null && echo "Sincronizado" || echo "No sincronizado"
```

## systemd-timesyncd — Configuración

```bash
# Archivo de configuración:
cat /etc/systemd/timesyncd.conf
# [Time]
# NTP=
# FallbackNTP=ntp.ubuntu.com 0.pool.ntp.org 1.pool.ntp.org

# Configurar servidores:
sudo tee /etc/systemd/timesyncd.conf.d/custom.conf << 'EOF'
[Time]
NTP=time.google.com time.cloudflare.com
FallbackNTP=0.pool.ntp.org 1.pool.ntp.org
EOF

sudo systemctl restart systemd-timesyncd

# Ver estado:
timedatectl timesync-status
# Server: time.google.com (216.239.35.0)
# Poll interval: 32s
# Leap: normal
# Offset: +0.234ms
# Delay: 15.000ms
# Jitter: 0.089ms
```

## Drift — Desviación del reloj

```bash
# Todo reloj de hardware tiene una desviación (drift)
# Puede ser de microsegundos a milisegundos por segundo
# Sin NTP, un reloj puede desviarse minutos por semana

# chrony mide y compensa el drift:
cat /var/lib/chrony/drift
# 1.234    0.567
# Frecuencia (ppm) y error estimado

# NTP usa dos técnicas para corregir:
# 1. Slew — ajuste gradual (acelerar/frenar el reloj)
#    Preferido porque no causa saltos de tiempo
#    Las aplicaciones no se enteran

# 2. Step — ajuste de golpe
#    Usado solo cuando la diferencia es grande
#    Puede confundir a aplicaciones que dependen del tiempo monótono
#    makestep en chrony controla cuándo se permite
```

## Seguridad — NTS (Network Time Security)

```bash
# NTP tradicional no tiene autenticación
# Un atacante en la red puede falsificar respuestas NTP
# NTS (RFC 8915) agrega autenticación criptográfica

# chrony soporta NTS desde la versión 4.0:
# En chrony.conf:
server time.cloudflare.com iburst nts
# nts = usar NTS para este servidor

# Verificar que NTS funciona:
chronyc -N authdata
# Name/IP address       Mode KeyID Type KLen Last Atmp  NAK Cook CLen
# time.cloudflare.com    NTS     1   15  256  33s    0    0    8  100
```

## Debian vs RHEL

| Aspecto | Debian/Ubuntu | RHEL/Fedora |
|---|---|---|
| Default NTP | systemd-timesyncd | chrony |
| Config chrony | /etc/chrony/chrony.conf | /etc/chrony.conf |
| Config timesyncd | /etc/systemd/timesyncd.conf | (no usado) |
| Pool por defecto | ntp.ubuntu.com | 2.rhel.pool.ntp.org |
| RTC en UTC | Sí (default) | Sí (default) |

---

## Ejercicios

### Ejercicio 1 — Estado de sincronización

```bash
# 1. ¿Está sincronizado tu reloj?
timedatectl | grep -i synch

# 2. ¿Qué servicio NTP está activo?
for svc in chronyd ntpd systemd-timesyncd; do
    systemctl is-active "$svc" 2>/dev/null && echo "Activo: $svc"
done

# 3. ¿Cuál es la diferencia con el servidor NTP?
chronyc tracking 2>/dev/null | grep "System time" || \
    timedatectl timesync-status 2>/dev/null | grep "Offset"
```

### Ejercicio 2 — Fuentes de tiempo

```bash
# Si usas chrony:
chronyc sources 2>/dev/null

# Si usas timesyncd:
timedatectl timesync-status 2>/dev/null

# ¿Qué stratum tiene tu servidor NTP?
```

### Ejercicio 3 — RTC

```bash
# ¿El RTC está en UTC o en hora local?
timedatectl | grep "RTC in local"

# ¿Cuál es la hora del RTC?
sudo hwclock --show 2>/dev/null || echo "Necesitas root para ver el RTC"
```
