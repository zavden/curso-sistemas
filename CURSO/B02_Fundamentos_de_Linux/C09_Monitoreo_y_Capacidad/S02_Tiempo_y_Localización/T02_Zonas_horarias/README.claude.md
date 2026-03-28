# T02 — Zonas horarias

> **Objetivo:** Entender cómo Linux maneja las zonas horarias, configurar la zona correcta, y usar la variable TZ para cambiar zonas por proceso.

## Erratas detectadas en el material original

Sin erratas detectadas.

---

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
# El kernel almacena el tiempo como segundos desde epoch:
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

---

## Ver la zona horaria actual

```bash
# Con timedatectl:
timedatectl
# Time zone: America/Mexico_City (CST, -0600)

# Con date:
date +%Z    # abreviación: CST, UTC, CET
date +%z    # offset numérico: -0600, +0000, +0100

# El archivo que define la zona:
ls -la /etc/localtime
# /etc/localtime -> /usr/share/zoneinfo/America/Mexico_City

# En Debian, también existe:
cat /etc/timezone
# America/Mexico_City

# En RHEL puede no existir /etc/timezone — usa solo /etc/localtime
```

---

## Archivos de zona horaria

```bash
# /usr/share/zoneinfo/ contiene todas las definiciones de zonas:
ls /usr/share/zoneinfo/
# Africa/  America/  Antarctica/  Asia/  Atlantic/  Australia/
# Europe/  Indian/   Pacific/     UTC    US/        Etc/

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
# en formato binario (TZif):
# - Offset de UTC
# - Reglas históricas y futuras de DST
# - Abreviaciones de la zona (CST, CDT, etc.)
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
# Crear el symlink directamente:
sudo ln -sf /usr/share/zoneinfo/America/Mexico_City /etc/localtime

# En Debian, también actualizar /etc/timezone:
echo "America/Mexico_City" | sudo tee /etc/timezone

# Herramienta interactiva de Debian:
sudo dpkg-reconfigure tzdata
# Menú gráfico para seleccionar región y ciudad
```

---

## La variable TZ

La variable de entorno `TZ` cambia la zona horaria **por proceso**, sin afectar al sistema:

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
# Es POR PROCESO — otros usuarios y servicios no se afectan
```

### Formatos de TZ

```
FORMATO                  EJEMPLO                 DESCRIPCIÓN
────────────────────────────────────────────────────────────────
Zona completa            TZ=America/Mexico_City   Recomendado
Zona UTC                 TZ=UTC                   UTC puro
Abreviación              TZ=CST6CDT               Ambiguo (evitar)
Con offset               TZ=GMT+5                 Offset fijo
Con DST explícito        TZ=EST5EDT,M3.2.0,M11    Reglas manuales

RECOMENDACIÓN:
  ✓ TZ=America/Mexico_City   → preciso, incluye reglas DST
  ✗ TZ=CST                   → ambiguo (CST = Central Standard, China Standard...)
```

### TZ en servicios de systemd

```bash
# Los servicios heredan la zona del sistema (/etc/localtime)
# Para un servicio con zona diferente:

sudo systemctl edit postgresql.service
# [Service]
# Environment=TZ=UTC
# El servicio ve UTC aunque el sistema esté en otra zona

# Verificar:
systemctl show postgresql.service -p Environment
```

---

## Horario de verano (DST)

```bash
# DST = Daylight Saving Time (horario de verano)
# Los archivos de zona contienen las reglas históricas y futuras

# Ver transiciones DST de una zona:
zdump -v /usr/share/zoneinfo/America/New_York | grep 2026
# Sun Mar  8 06:59:59 2026 UT = Sun Mar  8 01:59:59 2026 EST
# Sun Mar  8 07:00:00 2026 UT = Sun Mar  8 03:00:00 2026 EDT  ← salto de 2 a 3
# Sun Nov  1 05:59:59 2026 UT = Sun Nov  1 01:59:59 2026 EDT
# Sun Nov  1 06:00:00 2026 UT = Sun Nov  1 01:00:00 2026 EST  ← retroceso de 2 a 1
```

### Implicaciones del DST

```
┌─────────────────────────────────────────────────────────────────────────────┐
│  PROBLEMAS DURANTE EL CAMBIO DE DST                                        │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                              │
│  AL AVANZAR EL RELOJ (Marzo):                                             │
│  01:59 → 03:00 (una hora DESAPARECE)                                      │
│  Un evento a las 02:30 nunca ocurre                                         │
│                                                                              │
│  AL RETROCEDER (Noviembre):                                                │
│  01:59 → 01:00 (una hora se REPITE)                                        │
│  Un evento a las 01:30 ocurre DOS veces                                    │
│                                                                              │
│  IMPLICACIONES:                                                             │
│  - Timestamps en logs pueden ser ambiguos                                   │
│  - Cron jobs en esa hora pueden ejecutarse 0 o 2 veces                    │
│  - Certificados TLS con validez exacta pueden fallar                        │
└─────────────────────────────────────────────────────────────────────────────┘
```

### Actualizar reglas de zona

```bash
# Los gobiernos cambian las reglas DST con frecuencia
# Las actualizaciones llegan vía paquete tzdata:
sudo apt install tzdata       # Debian/Ubuntu
sudo dnf install tzdata       # RHEL/Fedora

# Verificar la versión:
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
|--------------|----------|-------------|
| **UTC** (recomendado) | Sin problemas de DST, fácil sync entre OS | Dual-boot con Windows puede desfasar |
| **Hora local** | Compatible con Windows legacy | DST causa problemas al arrancar |

```bash
# Cambiar a UTC (recomendado):
sudo timedatectl set-local-rtc false

# Cambiar a hora local (NO recomendado):
sudo timedatectl set-local-rtc true

# ¿Por qué UTC en el RTC?
# - En dual-boot con Windows, Windows espera hora local
#   Linux puede adaptarse a ambos con set-local-rtc
# - Si el RTC está en hora local y hay DST,
#   pueden ocurrir problemas al arrancar durante una transición
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
│  ZONA LOCAL CUANDO:                                                        │
│  - Requisitos legales o de auditoría que exigen hora local                  │
│  - Servidor dedicado a una sola zona con apps legacy                       │
│  - Estaciones de trabajo / laptops (comodidad del usuario)                 │
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

# Convertir epoch a formato legible con zona:
date -d @1710689400
date -d @1710689400 -u
TZ=America/New_York date -d @1710689400

# En logs, usar formato ISO 8601:
date -u +"%Y-%m-%dT%H:%M:%S%z"
# 2026-03-17T15:30:00+0000

date --iso-8601=seconds
# 2026-03-17T15:30:00+00:00
```

---

## Debian vs RHEL

| Aspecto | Debian/Ubuntu | RHEL/Fedora |
|---------|---------------|-------------|
| `/etc/timezone` | Sí (archivo de texto con nombre de zona) | No existe |
| `/etc/localtime` | Symlink a zoneinfo | Symlink a zoneinfo |
| Cambiar zona | `timedatectl set-timezone` o `dpkg-reconfigure tzdata` | `timedatectl set-timezone` |
| Herramienta interactiva | `dpkg-reconfigure tzdata` | — |

---

## Quick reference

```
VER ZONA:
  timedatectl | grep "Time zone"
  date +%Z                          # abreviación (CST, UTC)
  date +%z                          # offset (-0600, +0000)
  cat /etc/timezone                 # nombre (solo Debian)
  ls -la /etc/localtime             # symlink al archivo de zona

CAMBIAR ZONA:
  sudo timedatectl set-timezone America/Mexico_City
  sudo timedatectl set-timezone UTC
  timedatectl list-timezones | grep -i BUSCAR

TZ POR PROCESO:
  TZ=UTC date                       # un comando
  export TZ=America/Mexico_City     # sesión completa

RTC:
  timedatectl | grep "RTC in local"
  sudo timedatectl set-local-rtc false    # UTC (recomendado)

DST:
  zdump -v /usr/share/zoneinfo/America/New_York | grep 2026

FORMATOS PARA LOGS:
  date -u +"%Y-%m-%dT%H:%M:%S%z"   # ISO 8601 en UTC
  date --iso-8601=seconds           # ISO 8601 con offset

ARCHIVOS:
  /etc/localtime                    # zona activa (symlink)
  /etc/timezone                     # nombre de zona (Debian)
  /usr/share/zoneinfo/              # todas las zonas disponibles
```

---

## Ejercicios

### Ejercicio 1 — Zona actual y representaciones

```bash
# 1. ¿Qué zona horaria tiene tu sistema?
timedatectl | grep "Time zone"

# 2. Muestra la hora en 3 formatos:
date
date +%Z
date +%z

# 3. ¿Adónde apunta /etc/localtime?
ls -la /etc/localtime

# Predicción: Si tu sistema está en UTC, ¿qué mostrará date +%Z?
# ¿Y date +%z? ¿Y si estuviera en America/Mexico_City?
```

<details><summary>Predicción</summary>

En UTC:
- `date +%Z` → `UTC`
- `date +%z` → `+0000`

En America/Mexico_City (sin DST, horario estándar):
- `date +%Z` → `CST` (Central Standard Time)
- `date +%z` → `-0600`

En America/Mexico_City (con DST, horario de verano):
- `date +%Z` → `CDT` (Central Daylight Time)
- `date +%z` → `-0500`

`%Z` da la abreviación textual de la zona, `%z` da el offset numérico respecto a UTC. El offset cambia con DST.

</details>

### Ejercicio 2 — /etc/localtime y zoneinfo

```bash
# 1. ¿Cuántas zonas horarias hay disponibles?
timedatectl list-timezones | wc -l

# 2. Lista las zonas de América:
timedatectl list-timezones | grep America | head -10

# 3. ¿Cuántos archivos hay en /usr/share/zoneinfo/?
find /usr/share/zoneinfo/ -type f | wc -l

# Predicción: ¿Por qué /etc/localtime es un symlink y no una copia?
# ¿Qué ventaja tiene el symlink sobre copiar el archivo?
```

<details><summary>Predicción</summary>

Ventajas del symlink sobre una copia:

1. **Actualizaciones automáticas**: cuando se actualiza el paquete `tzdata`, los archivos en `/usr/share/zoneinfo/` se actualizan (nuevas reglas DST, correcciones). Si `/etc/localtime` es un symlink, apunta al archivo actualizado automáticamente. Si fuera una copia, habría que volver a copiarla manualmente después de cada actualización.

2. **Identificación de la zona**: con un symlink puedes ver qué zona está configurada con `readlink /etc/localtime`. Con una copia, el archivo binario no revela fácilmente qué zona es (por eso Debian tiene `/etc/timezone` como respaldo textual).

3. **Espacio**: menor (un symlink es solo unos bytes, una copia duplica el archivo).

Nota: `timedatectl set-timezone` crea un symlink. Si usas `cp` en vez de `ln -sf`, funciona pero pierdes la ventaja de actualizaciones automáticas de tzdata.

</details>

### Ejercicio 3 — Variable TZ

```bash
# 1. Muestra la hora actual en 5 zonas:
for tz in UTC America/New_York America/Mexico_City Europe/Madrid Asia/Tokyo; do
    echo "$tz: $(TZ=$tz date '+%H:%M %Z')"
done

# 2. Cambia TZ para la sesión y verifica:
export TZ=Asia/Tokyo
date
unset TZ    # restaurar

# Predicción: Si haces export TZ=Asia/Tokyo y luego otro usuario
# ejecuta date en otra terminal, ¿verá la hora de Tokyo?
```

<details><summary>Predicción</summary>

**No.** `export TZ=Asia/Tokyo` solo afecta al **proceso actual y sus hijos** (subshells, comandos lanzados desde esa sesión). No afecta a:
- Otros usuarios en otras terminales
- Otros servicios del sistema
- Otras sesiones del mismo usuario

Cada proceso tiene su propio entorno. `TZ` modifica la zona para ese proceso. Si no tiene `TZ` definida, usa `/etc/localtime`.

Para cambiar la zona de **todo el sistema**, necesitas `timedatectl set-timezone` (que modifica `/etc/localtime`). Para cambiar solo la de un servicio, usas `Environment=TZ=...` en su unit de systemd.

</details>

### Ejercicio 4 — Horario de verano (DST)

```bash
# 1. ¿Tu zona actual tiene DST?
zdump -v /etc/localtime 2>/dev/null | grep 2026 | head -4

# 2. Ver las transiciones de New York en 2026:
zdump -v /usr/share/zoneinfo/America/New_York | grep 2026

# 3. ¿Bogotá tiene DST?
zdump -v /usr/share/zoneinfo/America/Bogota | grep 2026

# Predicción: ¿Por qué Bogotá no tiene transiciones DST?
# ¿Qué zonas típicamente NO tienen DST?
```

<details><summary>Predicción</summary>

Bogotá (Colombia) no tiene DST porque **los países cercanos al ecuador generalmente no lo usan**. Cerca del ecuador, la diferencia de horas de luz entre verano e invierno es mínima, así que no hay beneficio en adelantar el reloj.

Zonas que típicamente **NO** tienen DST:
- Países tropicales/ecuatoriales: Colombia, Ecuador, Perú, Venezuela, Panamá, Costa Rica
- La mayoría de Asia: Japón, China, India, Corea del Sur, Singapur
- Algunos estados de EE.UU.: Arizona (excepto Navajo Nation), Hawaii
- Algunas zonas de Australia: Queensland, Northern Territory, Western Australia
- Rusia (eliminó DST en 2014)

Zonas que **SÍ** tienen DST:
- EE.UU. (la mayoría), Canadá
- Europa (la UE debate eliminarlo)
- Chile, Paraguay
- Partes de Australia (New South Wales, Victoria)
- Nueva Zelanda

</details>

### Ejercicio 5 — Conversión entre zonas en scripts

```bash
# 1. Convierte "2026-03-17 15:30:00 UTC" a 3 zonas:
TS="2026-03-17 15:30:00 UTC"
for tz in America/New_York Europe/Madrid Asia/Tokyo; do
    echo "$tz: $(TZ=$tz date -d "$TS" '+%Y-%m-%d %H:%M:%S %Z')"
done

# 2. Convierte un epoch a hora local y UTC:
date -d @1710689400
date -d @1710689400 -u

# Predicción: ¿Cuál será la hora en Tokyo para "15:30 UTC"?
# Tokyo es UTC+9. ¿Cambiará de día?
```

<details><summary>Predicción</summary>

Tokyo es UTC+9, así que:
- 15:30 UTC + 9 horas = **00:30 del día siguiente** en Tokyo (JST)
- Sí, cambia de día: 2026-03-17 15:30 UTC = 2026-03-**18** 00:30 JST

Esto es importante para scripts que trabajan con zonas globales:
```bash
TZ=Asia/Tokyo date -d "2026-03-17 15:30:00 UTC"
# Wed Mar 18 00:30:00 JST 2026
```

La fecha cambia cuando el offset cruza la medianoche. Es una razón más para almacenar timestamps en UTC y convertir solo al mostrar al usuario.

</details>

### Ejercicio 6 — UTC en servidores

```bash
# 1. Ver la zona actual:
timedatectl | grep "Time zone"

# 2. ¿Está el RTC en UTC?
timedatectl | grep "RTC in local"

# Predicción: Un cron job está configurado para ejecutarse a las
# 02:30 en un servidor con zona America/New_York.
# ¿Qué pasa el día que comienza DST (marzo)?
# ¿Y el día que termina (noviembre)?
```

<details><summary>Predicción</summary>

**Inicio de DST (marzo) — avance del reloj:**
- A las 01:59:59 el reloj salta a 03:00:00
- Las 02:30 **nunca existen**
- El cron job **no se ejecuta** ese día

**Fin de DST (noviembre) — retroceso del reloj:**
- A las 01:59:59 el reloj retrocede a 01:00:00
- Las 02:30 ocurren **una sola vez** (ya en EST, no EDT)
- El cron job se ejecuta normalmente

Nota: el comportamiento exacto depende de la implementación de cron. Algunas versiones de cron detectan el salto y ejecutan los jobs que se "perdieron". Otras no.

**Solución:** si el servidor estuviera en UTC, las 02:30 UTC siempre existen sin ambigüedad. Por eso los servidores deben usar UTC: `sudo timedatectl set-timezone UTC`.

Si necesitas que un cron se ejecute a las 02:30 hora local de New York:
```bash
# En crontab (servidor en UTC):
30 7 * * * /path/to/script    # 07:30 UTC = 02:30 EST (invierno)
# Pero en verano sería 03:30 EDT...
# Mejor: usar systemd timer con OnCalendar y timezone support
```

</details>

### Ejercicio 7 — /etc/timezone vs /etc/localtime

```bash
# 1. Compara ambos archivos:
ls -la /etc/localtime
cat /etc/timezone 2>/dev/null || echo "No existe"

# 2. ¿Qué tipo de archivo es /etc/localtime?
file /etc/localtime

# Predicción: Si alguien cambia /etc/localtime con ln -sf pero
# olvida actualizar /etc/timezone en Debian, ¿qué problemas puede haber?
```

<details><summary>Predicción</summary>

Si `/etc/localtime` apunta a una zona pero `/etc/timezone` dice otra:

1. **`date` y las aplicaciones usan `/etc/localtime`** — la hora se muestra correctamente según la zona nueva.

2. **Herramientas que leen `/etc/timezone`** pueden confundirse:
   - `dpkg-reconfigure tzdata` mostrará la zona vieja
   - Algunas aplicaciones Java leen `/etc/timezone` para determinar la zona
   - `timedatectl` puede mostrar información inconsistente

3. **`timedatectl set-timezone`** actualiza **ambos** archivos automáticamente, por eso es el método recomendado.

Para corregir manualmente:
```bash
# Actualizar /etc/timezone para que coincida:
readlink /etc/localtime | sed 's|.*/zoneinfo/||' | sudo tee /etc/timezone
```

En RHEL esto no es un problema porque no usa `/etc/timezone`.

</details>

### Ejercicio 8 — Formatos de tiempo para logs

```bash
# 1. Genera un timestamp en diferentes formatos:
echo "Epoch:    $(date +%s)"
echo "UTC:      $(date -u)"
echo "ISO 8601: $(date --iso-8601=seconds)"
echo "RFC 3339: $(date -u +%Y-%m-%dT%H:%M:%SZ)"
echo "Custom:   $(date -u +"%Y-%m-%d %H:%M:%S UTC")"

# Predicción: ¿Por qué es mejor usar ISO 8601 con offset
# en logs en vez de solo la hora local sin offset?
```

<details><summary>Predicción</summary>

Hora local sin offset: `2026-03-17 10:30:00 CST`
ISO 8601 con offset: `2026-03-17T10:30:00-0600`

Problemas de solo hora local:
1. **"CST" es ambiguo**: Central Standard Time (US, UTC-6), China Standard Time (UTC+8), Cuba Standard Time (UTC-5). Sin offset numérico, no sabes cuál es.
2. **Correlación entre servidores**: si tienes logs de un servidor en New York y otro en Tokyo, sin offset no puedes determinar el orden real de los eventos.
3. **DST**: durante la transición, "01:30 EST" y "01:30 EDT" son horas diferentes. Sin indicar cuál es cuál, el log es ambiguo.

ISO 8601 con offset (`-0600`) elimina toda ambigüedad. Aún mejor es UTC puro (`2026-03-17T16:30:00Z`) porque no requiere conversión mental — todos los eventos están en la misma escala temporal.

</details>

### Ejercicio 9 — Diagnóstico de problema de zona

```bash
# Escenario: Un usuario reporta que los logs de una aplicación
# muestran timestamps 6 horas adelante respecto a la hora local.

# Predicción: ¿Cuáles son las 3 causas más probables?
# ¿Cómo diagnosticarías cada una?
```

<details><summary>Predicción</summary>

Las 3 causas más probables:

**1. El servidor está en UTC y el usuario espera hora local:**
```bash
timedatectl | grep "Time zone"
# Si muestra UTC y el usuario está en CST (UTC-6) → normal
```
Los logs en UTC están 6 horas "adelante" para alguien en UTC-6. No es un error, es la configuración correcta para servidores. Solución: ajustar la herramienta de visualización, no el servidor.

**2. La aplicación tiene TZ configurado diferente al sistema:**
```bash
# Verificar el entorno del proceso:
cat /proc/$(pgrep app_name)/environ | tr '\0' '\n' | grep TZ
# O en el unit de systemd:
systemctl show app.service -p Environment
```
Si el servicio tiene `Environment=TZ=UTC` pero el sistema está en CST → la app escribe en UTC.

**3. La aplicación usa UTC internamente (hardcodeado):**
Muchas aplicaciones (nginx, MySQL, PostgreSQL) pueden estar configuradas para usar UTC independientemente de la zona del sistema. Verificar la configuración de la aplicación:
- MySQL: `SELECT @@global.time_zone;`
- PostgreSQL: `SHOW timezone;`
- nginx: los logs de acceso usan la zona del sistema, pero la app backend puede usar otra.

Diagnóstico rápido:
```bash
date                    # hora del sistema
TZ=UTC date             # hora en UTC
# Si la diferencia es exactamente 6 horas → el sistema está en CST y los logs en UTC
```

</details>

### Ejercicio 10 — tzdata y cambios de gobierno

```bash
# 1. Ver la versión de tzdata:
dpkg -s tzdata 2>/dev/null | grep Version
rpm -q tzdata 2>/dev/null

# 2. ¿Cuándo fue la última actualización?
dpkg -s tzdata 2>/dev/null | grep -i "installed-size\|version"

# Predicción: Un gobierno anuncia que eliminará el DST
# en 2 semanas. ¿Qué necesitas hacer en tus servidores?
# ¿Qué pasa si no haces nada?
```

<details><summary>Predicción</summary>

**Lo que necesitas hacer:**

1. **Actualizar tzdata lo antes posible:**
   ```bash
   sudo apt update && sudo apt install tzdata    # Debian
   sudo dnf update tzdata                         # RHEL
   ```
   La comunidad IANA tz database publica actualizaciones cuando los gobiernos cambian reglas de DST. El paquete `tzdata` distribuye estas actualizaciones.

2. **Verificar que la actualización incluye el cambio:**
   ```bash
   zdump -v /usr/share/zoneinfo/TU/ZONA | grep $(date +%Y)
   # Debería mostrar las nuevas reglas (sin transición DST)
   ```

3. **Reiniciar aplicaciones** que cachean la información de zona (Java es conocido por esto — tiene su propia copia de tzdata).

**Si no haces nada:**
- Cuando llegue la fecha que antes era el cambio de DST, tus servidores **adelantarán o atrasarán el reloj incorrectamente**.
- Los logs tendrán timestamps incorrectos durante esa ventana.
- Los cron jobs se ejecutarán a la hora equivocada.
- Las conversiones de hora en la aplicación serán incorrectas.
- El problema se autocorrige cuando llegue la siguiente transición que sí esperaba el sistema, pero durante el período intermedio la hora está mal.

Esto ha pasado en la realidad: países como Turquía, Egipto, Marruecos han cambiado reglas de DST con poco aviso, causando problemas en servidores no actualizados.

</details>
