# T01 — NTP / Chrony

> **Objetivo:** Entender cómo Linux maneja la hora del sistema, configurar Chrony o systemd-timesyncd, y diagnosticar problemas de sincronización.

## Erratas detectadas en el material original

Sin erratas detectadas.

---

## Por qué importa la hora exacta

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
│  Es el que usan las aplicaciones                                           │
│  Se sincroniza vía NTP                                                     │
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

## NTP — Network Time Protocol

NTP sincroniza el reloj del sistema con servidores de tiempo en Internet, compensando automáticamente la latencia de red.

### Jerarquía de stratum

```
Stratum 0 — Relojes atómicos, GPS (la fuente real del tiempo)
    │
Stratum 1 — Conectados directamente a stratum 0
    │
Stratum 2 — Sincronizados con stratum 1
    │
Stratum 3...15 — Cada nivel sincroniza con el anterior
    │
Stratum 16 — NO sincronizado
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
| **chrony** | Rápido, preciso, maneja VMs y conexiones intermitentes | — | RHEL 8+, Fedora, Arch |
| **ntpd** | Referencia original, mejor para stratum 1 | Lento para sync inicial | Legacy |
| **systemd-timesyncd** | Siempre disponible, básico pero suficiente | Solo cliente SNTP, sin servidor | Debian/Ubuntu |

```bash
# Ver cuál está activo:
systemctl is-active chronyd 2>/dev/null && echo "chrony"
systemctl is-active ntpd 2>/dev/null && echo "ntpd"
systemctl is-active systemd-timesyncd 2>/dev/null && echo "timesyncd"
```

### Cuándo usar cada uno

```
chrony     → VMs, containers, laptops, conexiones intermitentes
             Cuando necesitas sync rápido al arrancar
             Cuando la fuente es pool NTP público

ntpd       → Servidores que son stratum 1 o 2
             Cuando necesitas máxima precisión en la referencia
             Cuando necesitas servir tiempo a muchos clientes

timesyncd  → Escritorios/servidores simples
             Cuando chrony no está disponible
             Cuando no necesitas ser servidor NTP
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

# Al habilitar chrony, systemd-timesyncd se deshabilita automáticamente
```

### /etc/chrony.conf (RHEL) / /etc/chrony/chrony.conf (Debian)

```bash
# Servidores NTP:
pool pool.ntp.org iburst
# pool = usar múltiples servidores del pool (rotación DNS)
# iburst = enviar 4 requests rápidos al arrancar (sync más rápido)

# O servidores individuales:
server time.google.com iburst
server time.cloudflare.com iburst

# Archivo de drift (corrección del reloj local):
driftfile /var/lib/chrony/drift
# chrony mide cuánto se desvía tu reloj y lo compensa automáticamente

# Permitir ajuste grande al arrancar:
makestep 1.0 3
# Si la diferencia es > 1 segundo, ajustar de golpe (step)
# Pero solo en los primeros 3 intentos de sync
# Después, solo ajustes graduales (slew)

# RTC sincronizado por chrony:
rtcsync
# chrony actualiza el RTC periódicamente desde el system clock

# Directorio de claves:
keyfile /etc/chrony/chrony.keys

# Log:
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
# ^  = servidor
# *  = seleccionado actualmente (sincronizando con este)
# +  = candidato (bueno, podría ser seleccionado)
# -  = descartado (outlier)
# Reach = máscara octal de los últimos 8 intentos (377 = 11111111 = todos OK)

# Estado detallado de fuentes:
chronyc sourcestats
# Name/IP Address     NP  NR  Span  Frequency  Freq Skew  Offset  Std Dev
# time.google.com     10   5   540     +0.012      0.150   +234us    89us

# Estado de sincronización:
chronyc tracking
# Reference ID    : D8EF230A (time.google.com)
# Stratum         : 2
# System time     : 0.000000234 seconds fast of NTP time
# Last offset     : +0.000000234 seconds
# RMS offset      : 0.000000345 seconds
# Frequency       : 1.234 ppm slow
# Root delay      : 0.015000000 seconds

# Forzar sincronización inmediata:
sudo chronyc makestep

# Ver actividad de fuentes:
chronyc activity

# Modo interactivo:
chronyc
# chronyc> sources
# chronyc> tracking
# chronyc> quit
```

---

## systemd-timesyncd — Configuración

```bash
# Archivo de configuración:
cat /etc/systemd/timesyncd.conf
# [Time]
# NTP=
# FallbackNTP=ntp.ubuntu.com 0.pool.ntp.org 1.pool.ntp.org

# Configurar servidores (via drop-in):
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
# Todo reloj de hardware tiene una desviación (drift)
# Puede ser de microsegundos a milisegundos por segundo
# Sin NTP, un reloj puede desviarse minutos por semana

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
│  Acelerar o desacelerar el reloj gradualmente                              │
│  No causa saltos de tiempo                                                 │
│  Las aplicaciones no se enteran                                            │
│  chrony lo usa por defecto después del makestep inicial                    │
├─────────────────────────────────────────────────────────────────────────────┤
│  STEP — Ajuste de golpe                                                    │
│  ───────────────────────────                                               │
│  Saltar el reloj directamente                                              │
│  Solo cuando la diferencia es grande (> makestep threshold)                │
│  Puede confundir a aplicaciones que usan monotonic clocks                  │
│  makestep 1.0 3 → permitir step si > 1s, solo primeros 3 syncs           │
└─────────────────────────────────────────────────────────────────────────────┘
```

---

## NTS — Network Time Security

```bash
# NTP tradicional NO tiene autenticación
# Un atacante en la red puede falsificar respuestas NTP

# NTS (RFC 8915) agrega autenticación criptográfica
# chrony soporta NTS desde la versión 4.0

# En chrony.conf:
server time.cloudflare.com iburst nts
# nts = usar Network Time Security para este servidor

# Verificar que NTS funciona:
chronyc -N authdata
# Name/IP address       Mode KeyID Type KLen Last Atmp  NAK Cook CLen
# time.cloudflare.com    NTS     1   15  256  33s    0    0    8  100
```

---

## Diagnóstico de problemas

```bash
# PROBLEMA: "System clock synchronized: no"

# 1. ¿Qué servicio NTP está activo?
systemctl is-active chronyd ntpd systemd-timesyncd

# 2. Si chrony — ver fuentes:
chronyc sources
chronyc activity

# 3. Ver logs:
journalctl -u chronyd -n 20 --no-pager

# 4. Verificar conectividad a servidores NTP:
ping -c 3 pool.ntp.org

# 5. Forzar sincronización:
sudo chronyc makestep

# 6. Verificar resultado:
timedatectl | grep "System clock synchronized"
```

```bash
# PROBLEMA: La hora cambia abruptamente

# Ver la config de makestep:
grep makestep /etc/chrony.conf
# makestep 1.0 3
# chrony debería usar slew después de los primeros 3 syncs
# Si hay steps frecuentes → revisar config o fuentes de tiempo
```

---

## Debian vs RHEL

| Aspecto | Debian/Ubuntu | RHEL/Fedora |
|---------|---------------|-------------|
| Default NTP | systemd-timesyncd | chrony |
| Config chrony | `/etc/chrony/chrony.conf` | `/etc/chrony.conf` |
| Config timesyncd | `/etc/systemd/timesyncd.conf` | (no usado) |
| Pool por defecto | `ntp.ubuntu.com` | `2.rhel.pool.ntp.org` |
| RTC en UTC | Sí (default) | Sí (default) |
| NTS soportado | Sí (chrony) | Sí (chrony) |

---

## Quick reference

```
VER ESTADO:
  timedatectl                    → estado completo
  timedatectl show               → formato parseable
  timedatectl timesync-status    → estado de timesyncd

CHRONY:
  chronyc sources                → fuentes de tiempo
  chronyc sourcestats            → estadísticas de fuentes
  chronyc tracking               → estado de sincronización
  chronyc makestep               → forzar sync inmediato
  chronyc activity               → fuentes activas/offline

TIMESYNCD:
  timedatectl set-ntp true/false → habilitar/deshabilitar NTP
  /etc/systemd/timesyncd.conf.d/*.conf → configuración

HARDWARE (RTC):
  sudo hwclock --show            → ver hora del RTC
  sudo hwclock --systohc         → system clock → RTC
  sudo hwclock --hctosys         → RTC → system clock

CHRONY.CONF:
  pool pool.ntp.org iburst       → pool con sync rápido al inicio
  server time.google.com iburst  → servidor individual
  makestep 1.0 3                 → step si > 1s, primeros 3 syncs
  driftfile /var/lib/chrony/drift → compensación de desviación
  rtcsync                        → actualizar RTC periódicamente

NTP POOLS:
  pool.ntp.org                   → global
  time.google.com                → Google
  time.cloudflare.com            → Cloudflare (soporta NTS)
```

---

## Ejercicios

### Ejercicio 1 — Estado de sincronización

```bash
# 1. Verifica el estado completo:
timedatectl

# 2. ¿Qué servicio NTP está activo?
for svc in chronyd ntpd systemd-timesyncd; do
    systemctl is-active "$svc" 2>/dev/null && echo "Activo: $svc"
done

# Predicción: ¿Qué significan las líneas "System clock synchronized"
# y "NTP service" de timedatectl? ¿Pueden ser diferentes?
```

<details><summary>Predicción</summary>

- `System clock synchronized: yes/no` — indica si el reloj del sistema está **actualmente** sincronizado con un servidor NTP. "yes" significa que el offset es pequeño y estable.
- `NTP service: active/inactive` — indica si hay un **servicio** NTP habilitado (chrony, ntpd, o timesyncd).

**Sí, pueden ser diferentes:**
- `NTP service: active` + `System clock synchronized: no` — el servicio NTP está corriendo pero **aún no ha logrado sincronizar** (ej. recién arrancó, no hay conectividad a internet, firewall bloquea UDP/123).
- `NTP service: inactive` + `System clock synchronized: no` — NTP deshabilitado, el reloj no se sincroniza.
- `NTP service: active` + `System clock synchronized: yes` — todo funciona correctamente.

</details>

### Ejercicio 2 — Dos relojes

```bash
# 1. Ver la hora del system clock:
date

# 2. Ver la hora del RTC:
sudo hwclock --show

# 3. Ver el timestamp epoch:
date +%s

# Predicción: ¿Por qué existen dos relojes separados?
# ¿Qué pasaría si solo existiera el system clock?
```

<details><summary>Predicción</summary>

Dos relojes separados porque cumplen funciones distintas:

- **RTC** (hardware): funciona con batería incluso cuando el equipo está apagado. Su único propósito es mantener la hora entre reboots. Es un chip simple con un cristal que oscila — no es muy preciso (puede desviarse segundos por día), pero funciona sin electricidad.

- **System clock** (kernel): es el reloj de alta resolución que usan las aplicaciones, con precisión de nanosegundos. Solo existe mientras el sistema está encendido.

Si solo existiera el system clock: al apagar y encender, el sistema **no sabría qué hora es**. Tendría que esperar a que NTP sincronizara, y si no hay red (ej. laptop sin wifi), la hora sería incorrecta. El RTC proporciona una hora "suficientemente buena" al arrancar para que el sistema funcione hasta que NTP tome el control.

El flujo es: boot → RTC inicializa system clock → NTP corrige system clock → `rtcsync` actualiza RTC periódicamente.

</details>

### Ejercicio 3 — Stratum y pools

```bash
# Si usas chrony:
chronyc sources 2>/dev/null

# Si usas timesyncd:
timedatectl timesync-status 2>/dev/null

# Predicción: Si tu servidor tiene stratum=2 y tu reloj se sincroniza
# con él, ¿qué stratum tendrás tú? ¿Importa en la práctica?
```

<details><summary>Predicción</summary>

Tendrás **stratum 3** — siempre es el stratum del servidor + 1.

En la práctica, **no importa mucho** para la mayoría de sistemas. La diferencia de precisión entre stratum 2 y stratum 4 es típicamente microsegundos, irrelevante para casi todas las aplicaciones. La red (latencia, jitter) domina más que el nivel de stratum.

Lo que importa en la práctica:
- Que `System clock synchronized: yes`
- Que el offset sea pequeño (microsegundos a milisegundos)
- Que haya múltiples fuentes configuradas (redundancia)

El stratum importa principalmente si estás configurando un servidor NTP para una organización — querrías ser stratum 2-3, no stratum 10.

Los pools públicos (`pool.ntp.org`) típicamente dan servidores stratum 2-3. `time.google.com` es stratum 1.

</details>

### Ejercicio 4 — Configuración de chrony

```bash
# 1. Lee el archivo de configuración:
cat /etc/chrony/chrony.conf 2>/dev/null || cat /etc/chrony.conf 2>/dev/null

# 2. Identifica las directivas principales:
# pool/server, iburst, makestep, driftfile, rtcsync

# Predicción: ¿Qué hace "makestep 1.0 3"?
# ¿Por qué no permite step siempre?
```

<details><summary>Predicción</summary>

`makestep 1.0 3` significa:
- **1.0** — si la diferencia entre el reloj local y NTP es mayor a **1 segundo**...
- **3** — ...permitir ajuste de golpe (step), pero **solo en los primeros 3 syncs** después de arrancar.

Después de esos 3 intentos, chrony solo usa **slew** (ajuste gradual).

**¿Por qué no step siempre?** Porque un step puede causar problemas:
- Aplicaciones que miden intervalos de tiempo se confunden (ej. un timer de 30 segundos de repente "dura" 0 segundos si el reloj salta hacia adelante)
- Bases de datos pueden generar entradas duplicadas o con timestamps inconsistentes
- Cron puede ejecutar un job dos veces o saltárselo
- Logs pierden coherencia temporal

Al arrancar, un step es aceptable porque las aplicaciones aún no están corriendo. Después, el slew es seguro: acelera o frena el reloj imperceptiblemente (ej. avanzar 100μs por segundo) hasta corregir la desviación sin saltos.

</details>

### Ejercicio 5 — chronyc tracking

```bash
# Examina esta salida de chronyc tracking:
# Reference ID    : D8EF230A (time.google.com)
# Stratum         : 2
# System time     : 0.000000234 seconds fast of NTP time
# Last offset     : +0.000000234 seconds
# RMS offset      : 0.000000345 seconds
# Frequency       : 1.234 ppm slow
# Root delay      : 0.015000000 seconds

# Predicción: ¿Qué significan "System time", "Frequency" y "Root delay"?
# ¿Este sistema está bien sincronizado?
```

<details><summary>Predicción</summary>

- **System time: 0.000000234 seconds fast** — el reloj local está 0.234μs **adelantado** respecto al tiempo NTP. Esto es una diferencia ínfima (submicrosegundo).

- **Frequency: 1.234 ppm slow** — el oscilador del reloj local es 1.234 partes por millón más lento que el tiempo real. Esto es el drift que chrony compensa automáticamente. 1.234 ppm ≈ 0.107 segundos/día de desviación sin corrección.

- **Root delay: 0.015 seconds** — la latencia de red total ida y vuelta hasta la fuente de stratum 0 (el reloj atómico original). 15ms es normal para un servidor público por internet.

**¿Bien sincronizado?** Sí, **excelentemente** sincronizado:
- Offset de 0.234μs — submicrosegundo
- RMS offset de 0.345μs — consistentemente preciso
- Stratum 2 — fuente de buena calidad
- Frequency drift bajo (1.234 ppm)

</details>

### Ejercicio 6 — chronyc sources y Reach

```bash
# Examina esta salida de chronyc sources:
# MS Name/IP address     Stratum Poll Reach LastRx Last sample
# ^* time.google.com           1   6   377    34   +234us[+345us] +/- 15ms
# ^- time.nist.gov             1   6   177    33   +500us[+500us] +/- 80ms
# ^+ time.cloudflare.com       3   6   377    35   -123us[-234us] +/- 20ms

# Predicción: ¿Qué significan los indicadores *, -, +?
# ¿Por qué time.nist.gov tiene Reach=177 y no 377?
```

<details><summary>Predicción</summary>

Indicadores:
- `^*` time.google.com — el servidor **actualmente seleccionado** para sincronización. Es el que chrony considera la mejor fuente.
- `^+` time.cloudflare.com — un **candidato aceptable**. Si la fuente actual fallara, chrony podría cambiar a esta.
- `^-` time.nist.gov — **descartado** como outlier. chrony determinó que sus respuestas son inconsistentes con las otras fuentes (offset +500us vs las otras que coinciden más entre sí, y tiene ±80ms de error vs ±15-20ms).

**Reach=177 vs 377:**
Reach es una máscara octal de los **últimos 8 intentos de poll**. Cada bit representa un intento (1=éxito, 0=fallo):

- `377` octal = `11111111` binario = 8 de 8 intentos exitosos (100%)
- `177` octal = `01111111` binario = 7 de 8 intentos exitosos — el intento más antiguo (bit 8) falló

time.nist.gov perdió un paquete en algún momento reciente. Esto puede ser por latencia de red, congestión, o porque el servidor NIST es más lejano geográficamente (está en EE.UU.). Es otra razón por la que chrony lo descartó como fuente.

</details>

### Ejercicio 7 — systemd-timesyncd

```bash
# 1. Ver la configuración actual:
cat /etc/systemd/timesyncd.conf

# 2. Ver estado (si timesyncd está activo):
timedatectl timesync-status 2>/dev/null

# Predicción: ¿Cuál es la diferencia entre NTP y FallbackNTP
# en timesyncd.conf? ¿Qué pasa si NTP está vacío?
```

<details><summary>Predicción</summary>

- **NTP=** — servidores **principales**. timesyncd intentará estos primero. Si están definidos, son los servidores primarios.
- **FallbackNTP=** — servidores de **respaldo**. Solo se usan si los servidores NTP principales no están disponibles o si NTP no está configurado.

Si `NTP=` está vacío (que es el default):
- timesyncd usa directamente los servidores de `FallbackNTP`
- En Ubuntu, `FallbackNTP` típicamente incluye `ntp.ubuntu.com` y servidores de `pool.ntp.org`

El patrón de configuración recomendado es usar drop-ins en `/etc/systemd/timesyncd.conf.d/`:
```ini
[Time]
NTP=time.google.com time.cloudflare.com
FallbackNTP=0.pool.ntp.org 1.pool.ntp.org
```

Esto configura Google y Cloudflare como principales, con pool.ntp.org como respaldo.

</details>

### Ejercicio 8 — Drift y corrección

```bash
# 1. Si chrony está activo, ver el drift file:
cat /var/lib/chrony/drift 2>/dev/null

# 2. Ver la frecuencia en tracking:
chronyc tracking 2>/dev/null | grep "Frequency"

# Predicción: Si el drift file dice "5.678  0.123",
# ¿cuánto se desviaría el reloj sin NTP en una semana?
```

<details><summary>Predicción</summary>

El drift file contiene `5.678 0.123`:
- **5.678** — frecuencia en partes por millón (ppm). El reloj es 5.678 ppm más rápido/lento que el tiempo real.
- **0.123** — error estimado de la medición de drift.

Cálculo de desviación en una semana sin NTP:
- 5.678 ppm = 5.678 microsegundos de error por segundo
- 1 día = 86,400 segundos
- 1 semana = 604,800 segundos
- Desviación = 5.678 × 604,800 = **3,433,670 μs ≈ 3.43 segundos por semana**

Con NTP habilitado, chrony compensa este drift continuamente con slew, manteniendo el offset en microsegundos. Sin NTP, el reloj se desviaría ~3.4 segundos cada semana, ~14 segundos al mes. Después de unos meses, podría causar problemas con Kerberos (tolerancia de 5 minutos).

</details>

### Ejercicio 9 — Diagnóstico de problema

```bash
# Escenario: timedatectl muestra "System clock synchronized: no"
# ¿Cómo diagnosticarías?

# Predicción: Lista los 5 pasos de diagnóstico en orden.
# ¿Qué causa más común tiene este problema en un servidor nuevo?
```

<details><summary>Predicción</summary>

5 pasos de diagnóstico:

1. **¿Qué servicio NTP está activo?**
   ```bash
   systemctl is-active chronyd ntpd systemd-timesyncd
   ```
   Si ninguno está activo → `sudo timedatectl set-ntp true`

2. **¿El servicio está corriendo sin errores?**
   ```bash
   journalctl -u chronyd --since "1 hour ago" --no-pager
   ```
   Buscar errores como "no suitable source", "can't resolve hostname"

3. **¿Hay conectividad a los servidores NTP?**
   ```bash
   ping -c 3 pool.ntp.org
   # NTP usa UDP/123 — verificar firewall:
   ss -ulnp | grep 123
   ```

4. **¿Las fuentes están respondiendo?**
   ```bash
   chronyc sources    # ¿Reach > 0?
   chronyc activity   # ¿Hay fuentes online?
   ```

5. **Forzar sincronización:**
   ```bash
   sudo chronyc makestep
   timedatectl | grep "System clock synchronized"
   ```

**Causa más común en un servidor nuevo:**
- En Debian: sysstat está habilitado pero `ENABLED="false"` — perdón, en NTP: **systemd-timesyncd** está activo pero no puede resolver `ntp.ubuntu.com` si el DNS no está configurado o el firewall bloquea UDP/123.
- Firewall bloqueando UDP/123 saliente
- En containers: no hay servicio NTP corriendo (los containers usan el reloj del host)

</details>

### Ejercicio 10 — Comparativa Debian vs RHEL

```bash
# En Debian:
# cat /etc/chrony/chrony.conf 2>/dev/null || echo "Usa timesyncd"
# cat /etc/systemd/timesyncd.conf

# En RHEL:
# cat /etc/chrony.conf
# chronyc sources

# Predicción: Si migras un servidor de Debian (con timesyncd) a RHEL,
# ¿qué cambios necesitas hacer respecto a NTP?
# ¿Necesitas instalar algo o ya funciona?
```

<details><summary>Predicción</summary>

Al migrar de Debian (timesyncd) a RHEL:

**No necesitas instalar nada** — RHEL ya incluye chrony instalado y habilitado por defecto. La sincronización funciona automáticamente al primer boot.

Cambios que podrías querer hacer:

1. **Servidores NTP personalizados**: si en Debian tenías servidores específicos en `/etc/systemd/timesyncd.conf.d/custom.conf`, necesitas migrar esa configuración a `/etc/chrony.conf` (nótese: sin subdirectorio `/chrony/`):
   - timesyncd: `NTP=time.google.com` → chrony: `server time.google.com iburst`

2. **El pool por defecto es diferente**: Debian usa `ntp.ubuntu.com`, RHEL usa `2.rhel.pool.ntp.org`. Esto normalmente no importa.

3. **Los comandos de verificación cambian**: en vez de `timedatectl timesync-status`, usarás `chronyc sources` y `chronyc tracking` para más detalle.

4. **NTS**: si quieres seguridad, chrony en RHEL soporta NTS — solo agregar `nts` a la línea del servidor en chrony.conf. timesyncd no soporta NTS.

Ventaja de la migración: chrony es más preciso y robusto que timesyncd.

</details>
