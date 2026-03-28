# Lab — NTP / Chrony

## Objetivo

Entender la sincronizacion de tiempo en Linux: los dos relojes
(RTC hardware y system clock), protocolo NTP (stratum, pools),
las tres implementaciones (chrony, ntpd, systemd-timesyncd),
configuracion de chrony (chrony.conf, directivas clave), chronyc
(sources, tracking), systemd-timesyncd, drift y correccion (slew
vs step), NTS (Network Time Security), y diferencias Debian vs RHEL.

## Prerequisitos

- Entorno del curso levantado (`docker compose up -d`)

---

## Parte 1 — Relojes y NTP

### Objetivo

Entender los dos relojes del sistema y el protocolo NTP.

### Paso 1.1: Dos relojes

```bash
docker compose exec debian-dev bash -c '
echo "=== Dos relojes del sistema ==="
echo ""
echo "1. RTC (Real Time Clock) — reloj de hardware"
echo "   Chip en la placa base, funciona con bateria"
echo "   Mantiene la hora entre reboots"
echo "   Puede estar en UTC o en hora local"
echo ""
echo "2. System clock — reloj del kernel"
echo "   Se inicializa desde el RTC al arrancar"
echo "   Es el que usan las aplicaciones"
echo "   Se sincroniza via NTP"
echo ""
echo "--- Hora del sistema ---"
date
echo ""
echo "--- Timestamp epoch ---"
echo "El kernel almacena el tiempo como segundos desde epoch:"
date +%s
echo "(1 enero 1970 00:00:00 UTC)"
'
```

### Paso 1.2: NTP y stratum

```bash
docker compose exec debian-dev bash -c '
echo "=== NTP (Network Time Protocol) ==="
echo ""
echo "Sincroniza el reloj con servidores de tiempo via red."
echo "Compensa la latencia de red automaticamente."
echo ""
echo "--- Jerarquia de stratum ---"
echo "  Stratum 0 — relojes atomicos, GPS (fuente real)"
echo "  Stratum 1 — conectados directamente a stratum 0"
echo "  Stratum 2 — sincronizados con stratum 1"
echo "  Stratum 3 — sincronizados con stratum 2"
echo "  ..."
echo "  Stratum 15 — maximo valido"
echo "  Stratum 16 — no sincronizado"
echo ""
echo "--- Servidores NTP publicos ---"
echo "  pool.ntp.org          pool global"
echo "  time.google.com       Google"
echo "  time.cloudflare.com   Cloudflare"
echo "  time.nist.gov         NIST (US)"
'
```

### Paso 1.3: Tres implementaciones

```bash
docker compose exec debian-dev bash -c '
echo "=== Tres implementaciones de NTP ==="
echo ""
echo "1. chrony (chronyd) — RECOMENDADO"
echo "   Moderno, rapido, preciso"
echo "   Bueno para VMs y conexiones intermitentes"
echo "   Default en RHEL 8+, Fedora"
echo ""
echo "2. ntpd (ntp) — Legacy"
echo "   Implementacion original de referencia"
echo "   Mas lento para sincronizar"
echo "   Siendo reemplazado por chrony"
echo ""
echo "3. systemd-timesyncd — Basico"
echo "   Incluido en systemd (siempre disponible)"
echo "   Solo cliente SNTP (sin servidor)"
echo "   Default en Debian/Ubuntu si no hay chrony/ntpd"
echo ""
echo "--- Que hay instalado en este sistema ---"
echo ""
echo "chrony:"
dpkg -l chrony 2>/dev/null | grep -q "^ii" && echo "  instalado" || echo "  no instalado"
echo ""
echo "ntpd:"
dpkg -l ntp 2>/dev/null | grep -q "^ii" && echo "  instalado" || echo "  no instalado"
echo ""
echo "systemd-timesyncd:"
[[ -f /lib/systemd/systemd-timesyncd ]] && echo "  disponible" || echo "  no disponible"
'
```

### Paso 1.4: Por que importa la hora exacta

```bash
docker compose exec debian-dev bash -c '
echo "=== Por que importa ==="
echo ""
echo "Un reloj desincronizado causa problemas reales:"
echo ""
echo "  - Certificados TLS rechazados"
echo "    (\"certificate not yet valid\" o \"expired\")"
echo ""
echo "  - Autenticacion Kerberos falla"
echo "    (tolerancia tipica: 5 minutos)"
echo ""
echo "  - Logs con timestamps incorrectos"
echo "    (imposible correlacionar incidentes)"
echo ""
echo "  - Cron/timers ejecutan a la hora equivocada"
echo ""
echo "  - Base de datos con conflictos de replicacion"
echo ""
echo "  - make se confunde con timestamps de archivos"
echo ""
echo "--- Verificar la hora actual ---"
echo "Hora del sistema:"
date
echo ""
echo "Hora UTC:"
date -u
'
```

---

## Parte 2 — Chrony y timesyncd

### Objetivo

Explorar la configuracion de chrony y systemd-timesyncd.

### Paso 2.1: Configuracion de chrony

```bash
docker compose exec debian-dev bash -c '
echo "=== Configuracion de chrony ==="
echo ""
echo "--- Archivo de configuracion ---"
CONF=""
[[ -f /etc/chrony/chrony.conf ]] && CONF="/etc/chrony/chrony.conf"
[[ -f /etc/chrony.conf ]] && CONF="/etc/chrony.conf"

if [[ -n "$CONF" ]]; then
    echo "Archivo: $CONF"
    echo ""
    grep -v "^#" "$CONF" | grep -v "^$" | head -20
else
    echo "(chrony no instalado — mostrando config de referencia)"
    echo ""
    cat << '\''EOF'\''
# Servidores NTP:
pool pool.ntp.org iburst
#   pool = usar multiples servidores del pool
#   iburst = 4 requests rapidos al arrancar

# Drift (correccion del reloj local):
driftfile /var/lib/chrony/drift

# Ajuste grande al arrancar:
makestep 1.0 3
#   Si diferencia > 1s, ajustar de golpe
#   Pero solo en los primeros 3 intentos

# Sincronizar RTC:
rtcsync

# Log:
logdir /var/log/chrony
EOF
fi
'
```

### Paso 2.2: Directivas de chrony

```bash
docker compose exec debian-dev bash -c '
echo "=== Directivas de chrony ==="
echo ""
echo "--- pool vs server ---"
echo "  pool pool.ntp.org iburst"
echo "    Usa multiples servidores del pool (rotacion DNS)"
echo ""
echo "  server time.google.com iburst"
echo "    Usa un servidor especifico"
echo ""
echo "--- iburst ---"
echo "  Envia 4 requests rapidos al arrancar"
echo "  Sincronizacion inicial mas rapida"
echo ""
echo "--- makestep ---"
echo "  makestep 1.0 3"
echo "  Si la diferencia > 1 segundo, ajustar de golpe (step)"
echo "  Solo en los primeros 3 intentos de sync"
echo "  Despues: solo ajustes graduales (slew)"
echo ""
echo "--- driftfile ---"
echo "  driftfile /var/lib/chrony/drift"
echo "  chrony mide cuanto se desvia tu reloj y lo compensa"
if [[ -f /var/lib/chrony/drift ]]; then
    echo "  Contenido: $(cat /var/lib/chrony/drift)"
    echo "  (frecuencia en ppm y error estimado)"
fi
echo ""
echo "--- rtcsync ---"
echo "  chrony actualiza el RTC periodicamente"
'
```

### Paso 2.3: chronyc — control de chrony

```bash
docker compose exec debian-dev bash -c '
echo "=== chronyc ==="
echo ""
if command -v chronyc &>/dev/null; then
    echo "--- sources (fuentes de tiempo) ---"
    chronyc sources 2>/dev/null || echo "  (chronyd no activo)"
    echo ""
    echo "--- tracking (estado de sincronizacion) ---"
    chronyc tracking 2>/dev/null || echo "  (chronyd no activo)"
else
    echo "(chronyc no instalado — mostrando salida de referencia)"
    echo ""
    echo "--- chronyc sources ---"
    echo "MS Name/IP address     Stratum Poll Reach LastRx Last sample"
    echo "^* time.google.com           1   6   377    34   +234us[+345us] +/- 15ms"
    echo "^+ time.cloudflare.com       3   6   377    35   -123us[-234us] +/- 20ms"
    echo ""
    echo "Indicadores:"
    echo "  ^  = servidor"
    echo "  *  = seleccionado actual"
    echo "  +  = candidato"
    echo "  -  = descartado"
    echo "  Reach = mascara de ultimos 8 intentos (377 = todos OK)"
    echo ""
    echo "--- chronyc tracking ---"
    echo "  Reference ID    : servidor NTP actual"
    echo "  Stratum         : nivel del servidor"
    echo "  System time     : diferencia con NTP"
    echo "  Last offset     : ultimo ajuste"
    echo "  RMS offset      : error promedio"
    echo "  Frequency       : desviacion del reloj (ppm)"
fi
'
```

### Paso 2.4: systemd-timesyncd

```bash
docker compose exec debian-dev bash -c '
echo "=== systemd-timesyncd ==="
echo ""
echo "--- Configuracion ---"
echo "Archivo: /etc/systemd/timesyncd.conf"
echo ""
if [[ -f /etc/systemd/timesyncd.conf ]]; then
    cat /etc/systemd/timesyncd.conf | grep -v "^#" | grep -v "^$"
else
    echo "  (no encontrado)"
fi
echo ""
echo "--- Drop-ins ---"
ls /etc/systemd/timesyncd.conf.d/ 2>/dev/null || echo "  (sin drop-ins)"
echo ""
echo "--- Configuracion de referencia ---"
cat << '\''EOF'\''
[Time]
NTP=time.google.com time.cloudflare.com
FallbackNTP=0.pool.ntp.org 1.pool.ntp.org
EOF
echo ""
echo "NTP: servidores principales"
echo "FallbackNTP: servidores de respaldo"
echo ""
echo "timesyncd es suficiente para la mayoria de casos."
echo "Para servidor NTP propio, necesitas chrony."
'
```

### Paso 2.5: Drift — desviacion del reloj

```bash
docker compose exec debian-dev bash -c '
echo "=== Drift ==="
echo ""
echo "Todo reloj de hardware tiene una desviacion (drift)."
echo "Sin NTP, puede desviarse minutos por semana."
echo ""
echo "--- Dos tecnicas de correccion ---"
echo ""
echo "1. Slew — ajuste gradual"
echo "   Acelerar o frenar el reloj suavemente"
echo "   Las aplicaciones no se enteran"
echo "   Preferido porque no causa saltos"
echo ""
echo "2. Step — ajuste de golpe"
echo "   Usado cuando la diferencia es grande"
echo "   Puede confundir apps que dependen del tiempo"
echo "   makestep en chrony controla cuando se permite"
echo ""
echo "--- Drift file ---"
if [[ -f /var/lib/chrony/drift ]]; then
    echo "  /var/lib/chrony/drift: $(cat /var/lib/chrony/drift)"
elif [[ -f /var/lib/chrony/chrony.drift ]]; then
    echo "  $(cat /var/lib/chrony/chrony.drift)"
else
    echo "  (sin drift file — chrony no activo)"
fi
'
```

---

## Parte 3 — Comparacion

### Objetivo

Comparar implementaciones, ver diferencias entre distros.

### Paso 3.1: chrony vs timesyncd

```bash
docker compose exec debian-dev bash -c '
echo "=== chrony vs timesyncd ==="
echo ""
echo "| Aspecto | chrony | timesyncd |"
echo "|---------|--------|-----------|"
echo "| Precision | Alta (us) | Buena (ms) |"
echo "| Servidor NTP | Si | No (solo cliente) |"
echo "| VMs/contenedores | Excelente | Bueno |"
echo "| Conexion intermitente | Excelente | Basico |"
echo "| Configuracion | chrony.conf | timesyncd.conf |"
echo "| Control | chronyc | timedatectl |"
echo "| Default Debian | No | Si |"
echo "| Default RHEL | Si | No |"
echo "| NTS (seguridad) | Si (v4.0+) | No |"
echo ""
echo "--- Cuando usar cada uno ---"
echo "  timesyncd: estaciones de trabajo, uso basico"
echo "  chrony: servidores, VMs, precision critica"
'
```

### Paso 3.2: Debian vs RHEL

```bash
docker compose exec debian-dev bash -c '
echo "=== Debian ==="
echo ""
echo "NTP por defecto:"
echo "  systemd-timesyncd (si no hay chrony/ntpd)"
echo ""
echo "Config chrony: /etc/chrony/chrony.conf"
[[ -f /etc/chrony/chrony.conf ]] && \
    grep -E "^(pool|server)" /etc/chrony/chrony.conf 2>/dev/null | head -3 || \
    echo "  (chrony no instalado)"
echo ""
echo "Config timesyncd:"
[[ -f /etc/systemd/timesyncd.conf ]] && \
    grep -v "^#" /etc/systemd/timesyncd.conf | grep -v "^$" | head -5 || \
    echo "  (no encontrado)"
'
echo ""
docker compose exec alma-dev bash -c '
echo "=== RHEL ==="
echo ""
echo "NTP por defecto: chrony"
echo ""
echo "Config chrony: /etc/chrony.conf"
if [[ -f /etc/chrony.conf ]]; then
    grep -E "^(pool|server)" /etc/chrony.conf | head -3
else
    echo "  (no encontrado)"
fi
echo ""
echo "chronyd activo:"
pgrep -x chronyd &>/dev/null && echo "  si" || echo "  no (contenedor sin systemd)"
'
```

### Paso 3.3: NTS — Network Time Security

```bash
docker compose exec debian-dev bash -c '
echo "=== NTS (Network Time Security) ==="
echo ""
echo "NTP tradicional NO tiene autenticacion."
echo "Un atacante puede falsificar respuestas NTP."
echo ""
echo "NTS (RFC 8915) agrega autenticacion criptografica."
echo "chrony soporta NTS desde la version 4.0."
echo ""
echo "--- Configuracion ---"
echo "  server time.cloudflare.com iburst nts"
echo "  (agregar nts al final de la linea del servidor)"
echo ""
echo "--- Verificar ---"
echo "  chronyc -N authdata"
echo ""
echo "--- Version de chrony ---"
if command -v chronyd &>/dev/null; then
    chronyd --version 2>&1 | head -1
    echo "  (NTS requiere 4.0+)"
else
    echo "  (chrony no instalado)"
fi
'
```

### Paso 3.4: Tabla resumen

```bash
docker compose exec debian-dev bash -c '
echo "=== Resumen ==="
echo ""
echo "| Concepto | Descripcion |"
echo "|----------|-------------|"
echo "| RTC | Reloj hardware, mantiene hora con bateria |"
echo "| System clock | Reloj del kernel, usado por apps |"
echo "| NTP | Protocolo de sincronizacion via red |"
echo "| Stratum | Nivel en la jerarquia NTP (0-15) |"
echo "| chrony | Implementacion NTP moderna (recomendada) |"
echo "| timesyncd | Cliente SNTP basico de systemd |"
echo "| iburst | 4 requests rapidos al arrancar |"
echo "| makestep | Permitir ajuste de golpe al inicio |"
echo "| drift | Desviacion del reloj local |"
echo "| slew | Ajuste gradual del reloj |"
echo "| step | Ajuste de golpe del reloj |"
echo "| NTS | Autenticacion criptografica para NTP |"
echo ""
echo "--- Diagnostico rapido ---"
echo "  timedatectl                 ver estado de sincronizacion"
echo "  chronyc sources             ver fuentes de tiempo"
echo "  chronyc tracking            ver precision actual"
echo "  timedatectl timesync-status  estado de timesyncd"
'
```

---

## Limpieza final

No hay recursos que limpiar.

---

## Conceptos reforzados

1. Dos relojes: RTC (hardware, bateria) y system clock
   (kernel, apps). El system clock se sincroniza via NTP.

2. NTP usa jerarquia de stratum (0-15). Los servidores
   publicos (pool.ntp.org, time.google.com) son stratum 1-2.

3. Tres implementaciones: chrony (recomendado, preciso),
   ntpd (legacy), systemd-timesyncd (basico, incluido
   en systemd). Debian usa timesyncd, RHEL usa chrony.

4. chrony.conf: pool/server con iburst, makestep para
   ajuste inicial, driftfile para compensar desviacion.

5. Drift: todo reloj se desvia. NTP corrige con slew
   (gradual, preferido) o step (de golpe, solo al inicio).

6. NTS agrega autenticacion criptografica a NTP.
   chrony 4.0+ lo soporta con la directiva nts.
