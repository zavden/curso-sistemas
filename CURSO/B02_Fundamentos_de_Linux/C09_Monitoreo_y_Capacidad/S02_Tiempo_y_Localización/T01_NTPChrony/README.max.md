# T01 — NTP / Chrony

> **Objetivo:** Entender cómo Linux maneja la hora del sistema, configurar Chrony o systemd-timesyncd, y diagnosticar problemas de sincronización.

## Por qué importa la hora exacta

Un reloj desincronizado causa problemas reales en producción:

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                    PROBLEMAS DE UN RELOJ DESINCRONIZADO                    │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                              │
│  TLS      → "certificate not yet valid" o "expired"                         │
│  Kerberos → Falla de autenticación (tolerancia: ~5 minutos)               │
│  Logs     → Timestamps incorrectos, imposible correlacionar incidentes      │
│  Cron     → Tareas ejecutan a la hora equivocada                            │
│  Replicación DB → Conflictos de replicación                                │
│  make     → Confusión con timestamps, compilación rota                     │
│                                                                              │
└─────────────────────────────────────────────────────────────────────────────┘
```

---

## Dos relojes en el sistema

```
┌─────────────────────────────────────────────────────────────────────────────┐
│  RTC (Real Time Clock) — Chip en la placa base                             │
│  ─────────────────────────────────────────────────                         │
│  Batería en la placa base                                                  │
│  Mantiene la hora ENTRE reboots                                            │
│  Puede estar en UTC o hora local                                           │
│  Se sincroniza con chrony vía rtcsync                                      │
├─────────────────────────────────────────────────────────────────────────────┤
│  System Clock — Reloj del kernel                                           │
│  ───────────────────────────────                                           │
│  Se inicializa desde el RTC al arrancar                                    │
│  Es el que usan las aplicaciones                                          │
│  Se sincroniza vía NTP                                                    │
└─────────────────────────────────────────────────────────────────────────────┘
```

```bash
# Ver estado completo:
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

# Sincronizar RTC desde system clock:
sudo hwclock --systohc

# Sincronizar system clock desde RTC:
sudo hwclock --hctosys
```

---

## NTP — Jerarquía de servidores (Stratum)

```
┌─────────────────────────────────────────────────────────────────────────────┐
│  STRATUM 0 — Relojes de referencia                                         │
│  ──────────────────────────────────────                                   │
│  Relojes atómicos, GPS                                                     │
│  LA FUENTE REAL DEL TIEMPO                                                │
├─────────────────────────────────────────────────────────────────────────────┤
│  STRATUM 1 — Conectados directamente a Stratum 0                          │
│  ────────────────────────────────────────────                            │
│  Servidores de stratum 1                                                  │
├─────────────────────────────────────────────────────────────────────────────┤
│  STRATUM 2 — Sincronizan con Stratum 1                                    │
│  ───────────────────────────────────                                     │
│  Stratum 3, 4, 5...                                                      │
│  Cada nivel sincroniza con el nivel anterior                               │
│  Stratum 15 = máximo                                                      │
│  Stratum 16 = NO sincronizado                                              │
└─────────────────────────────────────────────────────────────────────────────┘
```

### Servidores NTP públicos

| Servidor | Descripción |
|----------|-------------|
| `pool.ntp.org` | Pool global (rotación DNS automática) |
| `0.pool.ntp.org` | Subpool 0 |
| `time.google.com` | Google |
| `time.cloudflare.com` | Cloudflare |
| `time.nist.gov` | NIST (US) |

---

## Tres implementaciones de NTP en Linux

| Implementación | Ventajas | Desventajas | Default en |
|----------------|----------|-------------|------------|
| **chrony** | Rápido, preciso, maneja VMs y conexiones intermitentes | | RHEL 8+, Fedora, Arch |
| **ntpd** | Referencia original, mejor para stratum 1 | Lento para sync inicial | Legacy |
| **systemd-timesyncd** | Siempre disponible, básico pero suficiente | Solo cliente SNTP | Debian/Ubuntu |

```bash
# Ver cuál está activo:
systemctl is-active chronyd 2>/dev/null && echo "chrony"
systemctl is-active ntpd 2>/dev/null && echo "ntpd"
systemctl is-active systemd-timesyncd 2>/dev/null && echo "timesyncd"
```

### Cuándo usar cada uno

```
chrony → VMs, containers, laptops, conexiones intermitentes
         → Cuando necesitas sync rápido al arrancar
         → Cuando la fuente es pool NTP público

ntpd    → Servidores que son stratum 1 o 2
         → Cuando necesitas máxima precisión
         → Cuando necesitas servir tiempo a otros

timesyncd → Escritorios/servidores simples
           → Cuando chrony no está disponible
           → Cuando no necesitas stratum 1
```

---

## timedatectl — Herramienta unificada

```bash
# Ver estado:
timedatectl
# System clock synchronized: yes  ← ¿está sincronizado?
# NTP service: active             ← ¿NTP está activo?

# Habilitar NTP:
sudo timedatectl set-ntp true

# Deshabilitar NTP (para ajuste manual):
sudo timedatectl set-ntp false

# Ajustar hora manualmente (solo si NTP está deshabilitado):
sudo timedatectl set-time "2026-03-17 15:30:00"
sudo timedatectl set-time "15:30:00"     # solo hora
sudo timedatectl set-time "2026-03-17"   # solo fecha

# Formato parseable:
timedatectl show
# Timezone=UTC
# NTP=yes
# NTPSynchronized=yes
```

---

## Chrony — Configuración

### Instalar y habilitar

```bash
# Debian/Ubuntu:
sudo apt install chrony
sudo systemctl enable --now chronyd

# RHEL/Fedora (ya instalado):
sudo systemctl enable --now chronyd
# Al habilitar chrony, timesyncd se deshabilita automáticamente
```

### /etc/chrony.conf

```bash
# Servidores NTP:
pool pool.ntp.org iburst
# pool = usar múltiples servidores del pool (rotación DNS)
# iburst = 4 requests rápidos al arrancar (sync más rápido)

# O servidores individuales:
server time.google.com iburst
server time.cloudflare.com iburst

# Archivo de drift:
driftfile /var/lib/chrony/drift
# chrony mide la desviación del reloj y la compensa automáticamente

# Ajuste grande al arrancar:
makestep 1.0 3
# Si la diferencia > 1 segundo → ajustar de golpe
# Pero solo en los primeros 3 sync attempts
# Después: ajustes graduales (slew)

# Sincronizar RTC periódicamente:
rtcsync
# chrony actualiza el RTC desde el system clock

# Logging:
logdir /var/log/chrony
```

### chronyc — Comandos de control

```bash
# Ver fuentes de tiempo:
chronyc sources
# MS Name/IP address     Stratum Poll Reach LastRx Last sample
# ^* time.google.com           1   6   377    34   +234us[ +345us] +/-  15ms
# ^+ time.cloudflare.com       3   6   377    35   -123us[ -234us] +/-  20ms

# Indicadores:
# ^ = servidor, * = sincronizado actualmente, + = candidato, - = descartado
# Reach = últimos 8 intentos (377 = todos exitosos, en octal)

# Estado detallado de fuentes:
chronyc sourcestats
# Name/IP Address     NP  NR  Span  Frequency  Freq Skew  Offset  Std Dev
# time.google.com     10   5   540     +0.012      0.150   +234us    89us

# Estado de sincronización:
chronyc tracking
# Reference ID    : D8EF230A (time.google.com)
# Stratum         : 2
# Ref time (UTC)  : Tue Mar 17 15:30:00 2026
# System time     : 0.000000234 seconds fast of NTP time
# Last offset     : +0.000000234 seconds
# RMS offset      : 0.000000345 seconds
# Frequency       : 1.234 ppm slow
# Root delay      : 0.015000000 seconds

# Forzar sincronización:
chronyc makestep

# Verificar sync:
timedatectl | grep "System clock synchronized"
```

### chronyc — Comandos interactivos

```bash
# Modo interactivo:
chronyc

# Comandos dentro de chronyc:
chronyc> activity      # ver qué fuentes están activas
chronyc> clients      # ver quién se sincroniza con este servidor
chronyc> sources      # ver fuentes
chronyc> tracking     # ver estado de sync
chronyc> makestep     # forzar sync inmediato
chronyc> offline      # marcar fuente como offline
chronyc> online      # marcar fuente como online
chronyc> quit
```

---

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

---

## Drift — Desviación del reloj

```bash
# Todo reloj de hardware tiene drift
# Puede ser μs a ms por segundo
# Sin NTP: minutos de desviación por semana

# chrony mide y compensa el drift:
cat /var/lib/chrony/drift
# 1.234    0.567
# Frecuencia (ppm) y error estimado
```

### Técnicas de corrección

```
┌─────────────────────────────────────────────────────────────────────────────┐
│  SLEW — Ajuste gradual (PREFERIDO)                                         │
│  ─────────────────────────────────────                                     │
│  Acelerar o desacelerar el reloj gradualmente                             │
│  No causa saltos de tiempo                                                 │
│  Las aplicaciones no se enteran                                             │
│  chrony lo usa por defecto después del primer sync                         │
├─────────────────────────────────────────────────────────────────────────────┤
│  STEP — Ajuste de golpe                                                   │
│  ───────────────────────────                                               │
│  Saltar el reloj directamente                                             │
│  Solo cuando la diferencia > 1 segundo                                    │
│  Puede confundir a aplicaciones que usan monotonic clocks                  │
│  makestep en chrony controla cuándo se permite                             │
└─────────────────────────────────────────────────────────────────────────────┘
```

---

## NTS — Network Time Security

```bash
# NTP tradicional NO tiene autenticación
# Un atacante puede falsificar respuestas NTP

# NTS (RFC 8915) agrega autenticación criptográfica:
# chrony soporta NTS desde versión 4.0

# En chrony.conf:
server time.cloudflare.com iburst nts

# Verificar NTS:
chronyc -N authdata
# Name/IP address       Mode KeyID Type KLen Last Atmp  NAK Cook CLen
# time.cloudflare.com    NTS     1   15  256  33s    0    0    8  100
```

---

## Diagnóstico de problemas

```bash
# PROBLEMA: "System clock synchronized: no"

# 1. Ver qué servicio está activo:
systemctl is-active chronyd ntpd systemd-timesyncd

# 2. Si chrony:
chronyc activity
# Si dice "2 sources active, 0 synced" → hay fuentes pero no sincronizado

# 3. Ver logs de chrony:
journalctl -u chronyd -n 20 --no-pager

# 4. Verificar conectividad:
ping -c 3 pool.ntp.org

# 5. Forzar sync:
sudo chronyc makestep
```

```bash
# PROBLEMA: La hora cambia abruptamente

# Ver el modo de ajuste:
grep makestep /etc/chrony.conf

#chrony debería usar slew la mayoría del tiempo
#Si hay steps frecuentes → revisar configuración
```

---

## Comparativa: Debian vs RHEL

| Aspecto | Debian/Ubuntu | RHEL/Fedora |
|---------|---------------|--------------|
| Default | systemd-timesyncd | chrony |
| Config chrony | `/etc/chrony/chrony.conf` | `/etc/chrony.conf` |
| Pool por defecto | `ntp.ubuntu.com` | `2.rhel.pool.ntp.org` |
| RTC en UTC | Sí | Sí |
| NTS soportado | Sí (chrony) | Sí (chrony) |

---

## Quick reference

```
VER ESTADO:
  timedatectl
  timedatectl show
  timedatectl timesync-status    (timesyncd)
  chronyc tracking               (chrony)

CHRONY:
  chronyc sources               → fuentes de tiempo
  chronyc sourcestats           → estadísticas
  chronyc tracking              → estado de sync
  chronyc makestep              → forzar sync

TIMESYNCD:
  timedatectl set-ntp true/false
  /etc/systemd/timesyncd.conf.d/*.conf

HARDWARE:
  sudo hwclock --show
  sudo hwclock --systohc
  sudo hwclock --hctosys

NTP POOL:
  pool.ntp.org
  time.google.com
  time.cloudflare.com
```

---

## Ejercicios

### Ejercicio 1 — Estado de sincronización

```bash
# 1. ¿Está sincronizado?
timedatectl | grep -i "synchronized"

# 2. ¿Qué servicio NTP está activo?
for svc in chronyd ntpd systemd-timesyncd; do
    systemctl is-active "$svc" 2>/dev/null && echo "Activo: $svc"
done

# 3. ¿Cuál es la diferencia con NTP?
chronyc tracking 2>/dev/null | grep "System time"
timedatectl timesync-status 2>/dev/null | grep "Offset"
```

### Ejercicio 2 — Fuentes de tiempo

```bash
# chrony:
chronyc sources 2>/dev/null
chronyc sourcestats 2>/dev/null

# timesyncd:
timedatectl timesync-status 2>/dev/null

# ¿Qué stratum tienen?
# Stratum 1 = conectado a reloj atómico/GPS
# Stratum 2 = conectado a stratum 1
```

### Ejercicio 3 — RTC y system clock

```bash
# ¿El RTC está en UTC o hora local?
timedatectl | grep "RTC in"

# ¿Hora del RTC?
sudo hwclock --show

# ¿Diferencia entre RTC y system clock?
date
sudo hwclock --show

# Sincronizar RTC → system clock:
sudo hwclock --hctosys
```

### Ejercicio 4 — Cambiar servidor NTP

```bash
# 1. Ver el estado actual:
timedatectl

# 2. Configurar chrony para usar time.google.com:
#    Editar /etc/chrony/chrony.conf o /etc/chrony.conf y añadir:
#    server time.google.com iburst

# 3. Reiniciar:
sudo systemctl restart chronyd

# 4. Verificar:
sleep 2
chronyc sources | grep google

# 5. Ver tracking:
chronyc tracking | grep "Reference ID"
```

### Ejercicio 5 — Diagnóstico de problema de sync

```bash
# Escenario: "El reloj dice que es ayer"

# 1. Ver estado actual:
timedatectl
date

# 2. ¿Hay conectividad a NTP?
ping -c 3 pool.ntp.org

# 3. ¿Qué servicio está activo?
systemctl is-active chronyd ntpd systemd-timesyncd

# 4. Si chrony, ver fuentes:
chronyc sources

# 5. Ver logs:
journalctl -u chronyd --since "1 hour ago" --no-pager

# 6. Forzar sync:
sudo chronyc makestep

# 7. Verificar después:
sleep 2
timedatectl | grep "System clock synchronized"
```
