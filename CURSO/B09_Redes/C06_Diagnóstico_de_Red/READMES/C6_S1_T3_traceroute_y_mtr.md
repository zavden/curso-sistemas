# traceroute y mtr — Diagnóstico de rutas

## Índice

1. [El problema que resuelven](#1-el-problema-que-resuelven)
2. [Cómo funciona traceroute](#2-cómo-funciona-traceroute)
3. [traceroute — uso básico](#3-traceroute--uso-básico)
4. [Modos de sondeo](#4-modos-de-sondeo)
5. [Interpretar la salida de traceroute](#5-interpretar-la-salida-de-traceroute)
6. [Opciones avanzadas de traceroute](#6-opciones-avanzadas-de-traceroute)
7. [mtr — traceroute continuo](#7-mtr--traceroute-continuo)
8. [Interpretar la salida de mtr](#8-interpretar-la-salida-de-mtr)
9. [mtr — opciones y modos](#9-mtr--opciones-y-modos)
10. [Patrones de problemas](#10-patrones-de-problemas)
11. [Escenarios de diagnóstico](#11-escenarios-de-diagnóstico)
12. [Errores comunes](#12-errores-comunes)
13. [Cheatsheet](#13-cheatsheet)
14. [Ejercicios](#14-ejercicios)

---

## 1. El problema que resuelven

Cuando un destino es inalcanzable o lento, `ping` te dice que hay un problema
pero no **dónde**. traceroute y mtr revelan cada salto (router) entre tú y el
destino, mostrando dónde se pierde la conectividad o dónde aumenta la latencia.

```
Tu máquina ──► Router 1 ──► Router 2 ──► Router 3 ──► ... ──► Destino
                  │              │             │
                 OK           OK          ¿Timeout?
                1ms           5ms          ¿200ms?
                                          ¿Pérdida?

ping solo dice: "destino inalcanzable" o "latencia 206ms"
traceroute dice: "el salto 3 es donde empieza el problema"
mtr dice: "el salto 3 tiene 15% de pérdida sostenida"
```

### Cuándo usar cada herramienta

| Herramienta | Cuándo |
|-------------|--------|
| `ping` | Verificar conectividad básica, medir latencia promedio |
| `traceroute` | Identificar EN QUÉ salto falla o se degrada la ruta |
| `mtr` | Diagnosticar problemas intermitentes con estadísticas continuadas |

---

## 2. Cómo funciona traceroute

traceroute explota el campo **TTL (Time To Live)** de IP. Cada router que
procesa un paquete decrementa el TTL en 1. Cuando llega a 0, el router
descarta el paquete y envía un mensaje ICMP "Time Exceeded" de vuelta al
origen.

```
Paso 1: enviar paquete con TTL=1
─────────────────────────────────
[Tu PC] ──TTL=1──▶ [Router 1] ──✗ TTL=0
                    Router 1 responde: ICMP Time Exceeded
                    → Ahora sabes la IP y latencia del salto 1

Paso 2: enviar paquete con TTL=2
─────────────────────────────────
[Tu PC] ──TTL=2──▶ [Router 1] ──TTL=1──▶ [Router 2] ──✗ TTL=0
                                          Router 2 responde: ICMP Time Exceeded
                                          → Salto 2 identificado

Paso 3: enviar paquete con TTL=3
─────────────────────────────────
[Tu PC] ──TTL=3──▶ [Router 1] ──▶ [Router 2] ──TTL=1──▶ [Router 3] ──✗
                                                          → Salto 3

...continúa hasta que el paquete llega al destino...

Paso N: enviar paquete con TTL=N (llega al destino)
────────────────────────────────────────────────────
[Tu PC] ──TTL=N──▶ ... ──▶ [Destino]
                             Destino responde:
                             - ICMP Echo Reply (si se envió ICMP)
                             - ICMP Port Unreachable (si se envió UDP)
                             - TCP RST o SYN-ACK (si se envió TCP)
                             → Traceroute completo
```

### Por qué 3 sondas por salto

traceroute envía **3 paquetes** para cada TTL (por defecto). Esto permite
detectar variaciones de latencia y rutas asimétricas:

```
Salto 3:  5.2ms  5.1ms  5.3ms     ← consistente
Salto 4:  10ms   45ms   10ms      ← el segundo fue anómalo
Salto 5:  *      15ms   15ms      ← el primero se perdió
```

---

## 3. traceroute — uso básico

### Instalación

```bash
# Debian/Ubuntu
apt install traceroute

# RHEL/Fedora
dnf install traceroute
```

### Uso básico

```bash
# traceroute estándar (UDP por defecto en Linux)
traceroute 8.8.8.8

# Con nombre de host
traceroute www.example.com

# Salida típica:
traceroute to 8.8.8.8 (8.8.8.8), 30 hops max, 60 byte packets
 1  gateway (192.168.1.1)  0.543 ms  0.621 ms  0.712 ms
 2  isp-router.example.net (198.51.100.1)  3.215 ms  3.108 ms  3.342 ms
 3  core1.isp.net (203.0.113.10)  8.112 ms  7.998 ms  8.231 ms
 4  peering.google.com (72.14.236.1)  9.543 ms  9.321 ms  9.678 ms
 5  108.170.244.1 (108.170.244.1)  10.112 ms  9.998 ms  10.231 ms
 6  dns.google (8.8.8.8)  10.543 ms  10.121 ms  10.478 ms
```

Cada línea muestra:
- **Número de salto**: posición en la ruta
- **Nombre/IP del router**: el router que respondió con ICMP Time Exceeded
- **3 tiempos**: latencia de cada una de las 3 sondas (ida+vuelta)

---

## 4. Modos de sondeo

traceroute puede enviar diferentes tipos de paquetes. Esto importa porque
los firewalls en el camino pueden bloquear un tipo pero no otro.

### UDP (default en Linux)

```bash
traceroute 8.8.8.8
# Envía paquetes UDP a puertos altos (33434+)
# El destino responde con ICMP Port Unreachable
```

Ventaja: es el default, funciona en la mayoría de redes.
Problema: muchos firewalls bloquean UDP a puertos altos.

### ICMP (-I)

```bash
sudo traceroute -I 8.8.8.8
# Envía ICMP Echo Request (como ping)
# El destino responde con ICMP Echo Reply
```

Requiere root. Funciona bien cuando el destino responde a ping pero los
sondeos UDP están bloqueados.

### TCP (-T)

```bash
sudo traceroute -T -p 80 8.8.8.8
# Envía paquetes TCP SYN al puerto especificado
# El destino responde con SYN-ACK o RST
```

Requiere root. La opción más fiable para atravesar firewalls, ya que la
mayoría permite tráfico TCP al puerto 80 o 443.

```bash
# TCP al puerto 443 (HTTPS) — traversa casi cualquier firewall
sudo traceroute -T -p 443 www.example.com
```

### Comparación: cuándo usar cada modo

```
Situación                              Modo recomendado
─────────────────────────────────     ─────────────────
Diagnóstico general                    UDP (default)
Destino responde a ping                ICMP (-I)
Firewall bloquea UDP e ICMP            TCP (-T -p 443)
Verificar ruta hacia un servicio web   TCP (-T -p 80)
Todo bloqueado                         Probar los tres
```

```bash
# Ejemplo: probar los tres modos cuando algo falla
traceroute 203.0.113.5
sudo traceroute -I 203.0.113.5
sudo traceroute -T -p 443 203.0.113.5
```

---

## 5. Interpretar la salida de traceroute

### Línea normal

```
 3  core1.isp.net (203.0.113.10)  8.112 ms  7.998 ms  8.231 ms
```

Los 3 tiempos son similares → salto estable, latencia consistente.

### Asteriscos (*)

```
 4  * * *
```

Los tres sondeos se perdieron. Causas posibles:
- El router NO responde ICMP Time Exceeded (configurado así por política)
- Un firewall bloquea los sondeos
- **No necesariamente significa que el tráfico no pasa** — el router puede
  reenviar paquetes normalmente pero ignorar los que tienen TTL expirado

```
 3  core1.isp.net (203.0.113.10)  8.112 ms  7.998 ms  8.231 ms
 4  * * *
 5  * * *
 6  dns.google (8.8.8.8)  10.543 ms  10.121 ms  10.478 ms
```

Saltos 4 y 5 no responden ICMP pero el tráfico llega al destino → **no hay
problema**, los routers intermedios simplemente no responden a traceroute.

### Latencia que salta

```
 1  gateway (192.168.1.1)           0.5 ms   0.6 ms   0.7 ms
 2  isp-router (198.51.100.1)      3.2 ms   3.1 ms   3.3 ms
 3  core1.isp.net (203.0.113.10)   8.1 ms   8.0 ms   8.2 ms
 4  transatlantic (154.54.0.1)    85.3 ms  85.1 ms  85.5 ms    ← salto grande
 5  eu-router (195.66.224.1)      86.2 ms  86.0 ms  86.4 ms
 6  destino.eu (93.184.216.34)    87.1 ms  86.8 ms  87.3 ms
```

La latencia salta de ~8ms a ~85ms entre los saltos 3 y 4. Esto indica un
**enlace de larga distancia** (probablemente un cable submarino o enlace
intercontinental). Es normal — no es un problema.

### Latencia inconsistente

```
 3  router-a (10.0.0.1)    5.2 ms  45.3 ms  5.1 ms
```

Una de las tres sondas tardó mucho más. Causas:
- El router estaba ocupado procesando una ráfaga de tráfico
- Enrutamiento ECMP envió la sonda por un camino diferente
- Priorización de tráfico: ICMP se procesa con baja prioridad

Si es solo un salto intermedio con variaciones puntuales → normalmente no
es un problema real.

### IPs múltiples en un salto

```
 3  10.0.0.1 (10.0.0.1)  5.2 ms  10.0.0.2 (10.0.0.2)  5.1 ms  10.0.0.1 (10.0.0.1)  5.3 ms
```

Las tres sondas respondidas por IPs diferentes → **ECMP (Equal-Cost
Multi-Path)**. Hay múltiples rutas y el tráfico se balancea entre ellas.
Es normal en redes de gran tamaño.

### !H, !N, !P, !X

```
 5  router.example.net (10.0.0.5)  10.2 ms !H  10.1 ms !H  10.3 ms !H
```

Códigos de error ICMP:

| Código | Significado |
|--------|------------|
| `!H` | Host unreachable |
| `!N` | Network unreachable |
| `!P` | Protocol unreachable |
| `!X` | Communication administratively prohibited (firewall) |
| `!S` | Source route failed |
| `!F` | Fragmentation needed (MTU problem) |

`!X` es especialmente informativo: un firewall explícitamente rechaza el
tráfico en ese punto.

---

## 6. Opciones avanzadas de traceroute

### Opciones de control

```bash
# Cambiar número de sondas por salto (default 3)
traceroute -q 1 8.8.8.8      # solo 1 sonda (más rápido)
traceroute -q 5 8.8.8.8      # 5 sondas (más datos)

# Cambiar máximo de saltos (default 30)
traceroute -m 15 8.8.8.8     # máximo 15 saltos

# Cambiar timeout por sonda (default 5 segundos)
traceroute -w 2 8.8.8.8      # 2 segundos de espera

# No resolver nombres (más rápido)
traceroute -n 8.8.8.8

# Especificar interfaz de salida
traceroute -i eth0 8.8.8.8

# Especificar dirección de origen
traceroute -s 192.168.1.10 8.8.8.8

# Primer TTL (saltar saltos conocidos)
traceroute -f 5 8.8.8.8      # empezar en TTL=5
```

### Tamaño de paquete

```bash
# Especificar tamaño de paquete (útil para diagnosticar MTU)
traceroute 8.8.8.8 1500       # paquetes de 1500 bytes

# Si un salto responde !F → ese enlace tiene MTU menor
traceroute 8.8.8.8 1500
#  4  router.isp.net (10.0.0.4)  10.2 ms !F  10.1 ms !F  10.3 ms !F
# → El enlace después del salto 4 no soporta paquetes de 1500 bytes
```

### tracepath — alternativa sin root

```bash
# tracepath no requiere root y descubre MTU automáticamente
tracepath 8.8.8.8

# Salida incluye MTU de cada salto:
#  1?: [LOCALHOST]                  pmtu 1500
#  1:  gateway                     0.543ms
#  2:  isp-router                  3.215ms asymm  3
#  2:  isp-router                  3.108ms pmtu 1480    ← MTU reducido
```

---

## 7. mtr — traceroute continuo

mtr (My Traceroute) combina `ping` y `traceroute` en una sola herramienta.
Envía sondas continuamente y acumula estadísticas, lo que permite detectar
problemas **intermitentes** que un traceroute puntual no revelaría.

### Instalación

```bash
# Debian/Ubuntu
apt install mtr-tiny       # solo CLI
apt install mtr            # CLI + GUI (ncurses)

# RHEL/Fedora
dnf install mtr
```

### Uso básico

```bash
# Modo interactivo (ncurses, se actualiza en tiempo real)
mtr 8.8.8.8

# Modo report (ejecutar N ciclos y mostrar resumen)
mtr -r -c 100 8.8.8.8

# Sin resolver DNS (más rápido)
mtr -n 8.8.8.8

# Report sin DNS
mtr -rn -c 100 8.8.8.8
```

### Salida interactiva

```
                              My traceroute  [v0.95]
server1 (192.168.1.10) → dns.google (8.8.8.8)          2026-03-21T14:23:01+0000
Keys:  Help   Display mode   Restart statistics   Order of fields   quit
                                        Packets               Pings
 Host                                 Loss%   Snt   Last   Avg  Best  Wrst StDev
 1. gateway (192.168.1.1)              0.0%    50    0.5   0.6   0.3   1.2   0.2
 2. isp-router (198.51.100.1)          0.0%    50    3.2   3.3   2.8   5.1   0.5
 3. core1.isp.net (203.0.113.10)       0.0%    50    8.1   8.2   7.5  10.3   0.6
 4. ???                                       100.0    50    0.0   0.0   0.0   0.0   0.0
 5. peering.google (72.14.236.1)       0.0%    50    9.5   9.6   9.0  12.1   0.7
 6. dns.google (8.8.8.8)              0.0%    50   10.1  10.2   9.5  13.4   0.8
```

---

## 8. Interpretar la salida de mtr

### Columnas

| Columna | Significado |
|---------|------------|
| **Loss%** | Porcentaje de paquetes perdidos |
| **Snt** | Paquetes enviados |
| **Last** | Latencia del último sondeo (ms) |
| **Avg** | Latencia promedio (ms) |
| **Best** | Mejor latencia (ms) |
| **Wrst** | Peor latencia (ms) |
| **StDev** | Desviación estándar (variabilidad) |

### Cómo leer las métricas

**Loss% (pérdida)**:
```
0.0%    → salto perfecto
0-2%    → pérdida mínima, aceptable en Internet
3-10%   → posible problema, investigar
>10%    → problema significativo
100%    → el router no responde ICMP (no necesariamente un problema)
```

**Avg vs Best/Wrst**:
```
Avg=10ms, Best=9ms, Wrst=12ms, StDev=0.8
→ Latencia estable y consistente ✓

Avg=10ms, Best=5ms, Wrst=200ms, StDev=45
→ Latencia muy variable → problema intermitente (congestión, bufferbloat)
```

### La regla más importante: mirar el patrón, no el salto individual

```
 Host                    Loss%  Avg
 1. gateway               0.0%  0.5
 2. isp-router             0.0%  3.2
 3. core1.isp.net         15.0%  8.1    ← ¿Problema aquí?
 4. peering.google         0.0%  9.5
 5. dns.google             0.0% 10.1    ← destino: 0% pérdida
```

El salto 3 muestra 15% de pérdida, pero los saltos siguientes (4 y 5) tienen
0%. Esto significa que **el salto 3 descarta sus propias respuestas ICMP**
(rate limiting ICMP), no que pierda tráfico real. El tráfico fluye
perfectamente.

**Regla**: si la pérdida de un salto no se propaga a los saltos siguientes
→ es ICMP rate limiting, no pérdida real.

```
 Host                    Loss%  Avg
 1. gateway               0.0%  0.5
 2. isp-router             0.0%  3.2
 3. core1.isp.net         15.0%  8.1    ← Empieza la pérdida
 4. peering.google        15.0%  9.5    ← Se mantiene
 5. dns.google            14.8% 10.1    ← Se mantiene hasta el destino
```

Aquí la pérdida empieza en el salto 3 y **se propaga** al destino → el
problema real está **en o cerca del salto 3**.

### Latencia que aumenta vs que salta

```
                         Avg
 1. gateway              0.5    ← base
 2. isp-router           3.2    ← +2.7ms (normal, hop local)
 3. core1                8.1    ← +4.9ms (normal, distancia)
 4. transatlantic       85.3    ← +77ms (enlace transcontinental)
 5. eu-router           86.2    ← +0.9ms (normal dentro de EU)
 6. destino             87.1    ← +0.9ms (normal)
```

El salto de 8→85ms es un enlace de larga distancia — **no es un problema**.
La latencia aumenta una vez y se mantiene estable después.

```
                         Avg
 1. gateway              0.5
 2. isp-router           3.2
 3. core1                8.1
 4. problem-router     108.3    ← +100ms (anómalo)
 5. next-router        110.2    ← solo +2ms después
 6. destino            112.1    ← solo +2ms después
```

El salto 4 añade 100ms de latencia pero los siguientes solo añaden ~2ms
cada uno. El problema está en el enlace entre el salto 3 y el salto 4
(congestión, saturación, o enlace degradado).

---

## 9. mtr — opciones y modos

### Modos de salida

```bash
# Interactivo (default, ncurses)
mtr 8.8.8.8

# Report (ejecuta y muestra resumen al terminar)
mtr -r 8.8.8.8            # default 10 ciclos
mtr -r -c 100 8.8.8.8     # 100 ciclos (más datos)

# Report wide (incluye IPs y nombres)
mtr -rw -c 100 8.8.8.8

# CSV (para procesar con scripts)
mtr -rw -c 100 --csv 8.8.8.8

# JSON (para APIs y herramientas)
mtr -rw -c 100 --json 8.8.8.8

# Raw (para parsear)
mtr -rw -c 100 --raw 8.8.8.8
```

### Protocolo de sondeo

```bash
# ICMP (default en mtr)
mtr 8.8.8.8

# UDP
mtr -u 8.8.8.8

# TCP
mtr -T -P 443 8.8.8.8     # TCP al puerto 443

# SCTP
mtr -S 8.8.8.8
```

**Nota**: a diferencia de traceroute (que usa UDP por defecto), mtr usa
**ICMP** por defecto.

### Opciones de control

```bash
# Sin resolver DNS
mtr -n 8.8.8.8

# Intervalo entre sondas (default 1 segundo)
mtr -i 0.5 8.8.8.8        # cada 0.5 segundos

# Tamaño de paquete
mtr -s 1400 8.8.8.8        # paquetes de 1400 bytes

# Máximo de saltos
mtr -m 20 8.8.8.8

# Primer TTL
mtr -f 5 8.8.8.8           # empezar en TTL=5

# Especificar interfaz
mtr -I eth0 8.8.8.8

# Mostrar IPs y nombres
mtr -b 8.8.8.8             # both (nombre + IP)

# Mostrar ASN (Autonomous System Number)
mtr -z 8.8.8.8
```

### Teclas en modo interactivo

| Tecla | Acción |
|-------|--------|
| `d` | Cambiar modo de display |
| `n` | Toggle resolución DNS |
| `r` | Resetear estadísticas |
| `o` | Cambiar orden de campos |
| `j` | Toggle latencia/jitter |
| `q` | Salir |
| `p` | Pausar |
| ` ` | Continuar (después de pausa) |

---

## 10. Patrones de problemas

### Patrón 1: pérdida que se propaga

```
 Host                    Loss%   Avg
 1. gateway               0.0%   0.5
 2. isp-router             0.0%   3.2
 3. problem-link          25.0%  15.3    ← empieza aquí
 4. next-router           24.8%  16.1    ← se mantiene
 5. destino               25.2%  17.0    ← se mantiene
```

**Diagnóstico**: pérdida real de paquetes en el enlace entre salto 2 y salto 3.
**Acción**: reportar al ISP o al proveedor del salto 3.

### Patrón 2: pérdida que no se propaga (ICMP rate limiting)

```
 Host                    Loss%   Avg
 1. gateway               0.0%   0.5
 2. isp-router             0.0%   3.2
 3. core-router           30.0%   8.1    ← "pérdida" alta
 4. next-router            0.0%   9.5    ← pero 0% aquí
 5. destino                0.0%  10.1    ← y 0% al final
```

**Diagnóstico**: falso positivo. El router del salto 3 limita sus respuestas
ICMP. El tráfico real fluye sin problemas.
**Acción**: ninguna, no hay problema.

### Patrón 3: latencia alta en un punto

```
 Host                    Loss%   Avg   StDev
 1. gateway               0.0%   0.5    0.1
 2. isp-router             0.0%   3.2    0.3
 3. congested-link         0.0% 150.3   45.2    ← latencia alta + variable
 4. next-router            0.0% 152.1   44.8    ← se mantiene
 5. destino                0.0% 153.5   45.0    ← se mantiene
```

**Diagnóstico**: congestión o enlace saturado entre saltos 2 y 3. El StDev
alto indica variabilidad (bufferbloat o congestión intermitente).
**Acción**: si el salto pertenece a tu ISP → reportar. Si es un peering
→ poco control.

### Patrón 4: asimetría de rutas

```
 Host                    Loss%   Avg
 1. gateway               0.0%   0.5
 2. isp-router             0.0%   3.2
 3. core1                  0.0%  15.3    ← ida por esta ruta
 4. core2                  0.0%   8.1    ← ¡latencia MENOR que el salto 3!
 5. destino                0.0%  10.1
```

La latencia del salto 4 es **menor** que la del salto 3. Esto no tiene
sentido si la ruta es simétrica. Causa: la **respuesta ICMP del salto 3
vuelve por una ruta diferente** (más larga). El tiempo que muestra
traceroute/mtr es ida+vuelta, y la vuelta puede ser por otro camino.

**Acción**: generalmente no es un problema. Solo importa la latencia y
pérdida al destino final.

### Patrón 5: destino inalcanzable

```
 Host                    Loss%   Avg
 1. gateway               0.0%   0.5
 2. isp-router             0.0%   3.2
 3. core1                  0.0%   8.1
 4. firewall              !X     ---     ← administratively prohibited
```

o:

```
 3. core1                  0.0%   8.1
 4. ???                  100.0%   ---
 5. ???                  100.0%   ---
 ...
30. ???                  100.0%   ---
```

**Diagnóstico**: firewall bloqueando el tráfico (con !X) o destino
inalcanzable (100% pérdida sin respuesta).
**Acción**: verificar firewalls, ACLs, y que la IP destino sea correcta.

### Patrón 6: último salto con pérdida, intermedios bien

```
 Host                    Loss%   Avg
 1. gateway               0.0%   0.5
 2. isp-router             0.0%   3.2
 3. core1                  0.0%   8.1
 4. edge-router            0.0%   9.5
 5. destino               15.0%  50.3    ← solo el destino
```

**Diagnóstico**: el servidor destino está sobrecargado o su firewall limita
ICMP. Si la aplicación funciona bien (web, SSH), el problema puede ser
solo ICMP y no tráfico real.
**Acción**: verificar la aplicación directamente (curl, ssh).

---

## 11. Escenarios de diagnóstico

### Escenario 1: "Internet está lento"

```bash
# Paso 1: mtr al DNS de Google (referencia estable)
mtr -rn -c 50 8.8.8.8

# Paso 2: mtr al servicio que reportan lento
mtr -rn -c 50 www.servicio-lento.com

# Paso 3: comparar resultados
# - ¿La latencia es alta desde el mismo salto? → problema en ese tramo
# - ¿La latencia es alta solo al destino? → problema del servidor destino
# - ¿La latencia es alta desde el salto 1? → problema en tu red local
```

### Escenario 2: "Funciona a ratos"

```bash
# Problemas intermitentes requieren captura extendida
mtr -rn -c 500 8.8.8.8
# 500 ciclos ≈ ~8 minutos de datos

# Buscar en la salida:
# - Loss% entre 1-10% → pérdida intermitente
# - StDev alto → latencia variable
# - Wrst muy superior a Avg → picos ocasionales

# Comparar con mtr TCP (¿el firewall bloquea ICMP intermitentemente?)
mtr -rn -c 500 -T -P 443 www.example.com
```

### Escenario 3: "No llego a un servidor específico"

```bash
# Paso 1: traceroute estándar
traceroute -n www.example.com

# Si se detiene en un salto con *** :
# Paso 2: probar con ICMP
sudo traceroute -I -n www.example.com

# Paso 3: probar con TCP al puerto del servicio
sudo traceroute -T -p 443 -n www.example.com

# Si TCP llega pero UDP e ICMP no → firewall en el camino

# Paso 4: verificar con mtr TCP
mtr -n -T -P 443 www.example.com
```

### Escenario 4: diagnosticar problemas de MTU

```bash
# Enviar paquetes grandes con traceroute
traceroute -n 8.8.8.8 1500

# Si un salto devuelve !F (fragmentation needed):
# → El enlace tiene MTU menor que 1500
# → Revisar si hay túneles (VPN, GRE) que reducen MTU

# tracepath descubre MTU automáticamente
tracepath 8.8.8.8
```

### Generar reporte para soporte técnico

```bash
# Reporte completo para enviar al ISP o soporte
echo "=== Date ===" > /tmp/net_report.txt
date >> /tmp/net_report.txt
echo "" >> /tmp/net_report.txt

echo "=== mtr report ===" >> /tmp/net_report.txt
mtr -rwn -c 100 8.8.8.8 >> /tmp/net_report.txt
echo "" >> /tmp/net_report.txt

echo "=== mtr to problem host ===" >> /tmp/net_report.txt
mtr -rwn -c 100 www.example.com >> /tmp/net_report.txt

cat /tmp/net_report.txt
# Adjuntar este archivo al ticket de soporte
```

---

## 12. Errores comunes

### Error 1: asumir que * * * significa un problema

```bash
# ✗ "Hay un problema en los saltos 4 y 5, no responden"
#  3  core1.isp.net  8.1 ms  8.0 ms  8.2 ms
#  4  * * *
#  5  * * *
#  6  dns.google     10.1 ms 10.0 ms 10.2 ms

# ✓ Los saltos 4 y 5 simplemente no responden ICMP
# El tráfico llega al destino perfectamente (salto 6 OK)
# Esto es configuración normal de muchos routers de backbone
```

### Error 2: diagnosticar por un solo salto intermedio

```bash
# ✗ "El salto 3 tiene 20% de pérdida, es el problema"
#  3  core-router   20.0%   8.1 ms
#  4  next-router    0.0%   9.5 ms     ← 0% aquí
#  5  destino        0.0%  10.1 ms     ← 0% aquí

# ✓ La pérdida no se propaga → es ICMP rate limiting
# Solo es problema si la pérdida se mantiene hasta el destino
```

### Error 3: ejecutar traceroute sin probar diferentes protocolos

```bash
# ✗ "El destino es inalcanzable" (solo probó UDP)
traceroute www.example.com
# Llega al salto 5 y luego * * * hasta el 30

# ✓ Probar con TCP al puerto del servicio
sudo traceroute -T -p 443 www.example.com
# Llega perfectamente — era un firewall bloqueando UDP
```

### Error 4: no usar suficientes ciclos en mtr

```bash
# ✗ Solo 10 ciclos (default): estadísticas poco fiables
mtr -r 8.8.8.8
# 10 paquetes → un solo paquete perdido = 10% pérdida

# ✓ Al menos 100 ciclos para estadísticas significativas
mtr -r -c 100 8.8.8.8
# 100 paquetes → un paquete perdido = 1% pérdida (más realista)
```

### Error 5: confundir latencia del salto con latencia añadida

```bash
# ✗ "El salto 4 tiene 85ms de latencia, es lento"
#  3  core1          8.1 ms
#  4  transatlantic 85.3 ms   ← 85ms total, pero la contribución es 77ms
#  5  eu-router     86.2 ms   ← solo +0.9ms sobre el salto 4

# ✓ Lo que importa es el INCREMENTO entre saltos, no el valor absoluto
# El salto 4 añade 77ms (enlace transcontinental, normal)
# El salto 5 añade 0.9ms (normal)
```

---

## 13. Cheatsheet

```bash
# ╔══════════════════════════════════════════════════════════════════╗
# ║            TRACEROUTE Y MTR — CHEATSHEET                       ║
# ╠══════════════════════════════════════════════════════════════════╣
# ║                                                                ║
# ║  TRACEROUTE:                                                  ║
# ║  traceroute -n 8.8.8.8             # UDP, sin DNS             ║
# ║  sudo traceroute -I -n 8.8.8.8     # ICMP                    ║
# ║  sudo traceroute -T -p 443 -n X    # TCP al puerto 443       ║
# ║  traceroute -n -q 1 8.8.8.8        # 1 sonda (rápido)        ║
# ║  traceroute -n -m 15 8.8.8.8       # máx 15 saltos           ║
# ║  traceroute -n 8.8.8.8 1500        # paquetes grandes (MTU)  ║
# ║                                                                ║
# ║  MTR (recomendado sobre traceroute):                          ║
# ║  mtr -n 8.8.8.8                    # interactivo sin DNS     ║
# ║  mtr -rn -c 100 8.8.8.8            # report 100 ciclos       ║
# ║  mtr -rwn -c 100 8.8.8.8           # report wide             ║
# ║  mtr -n -T -P 443 X               # TCP al puerto 443       ║
# ║  mtr -n -u 8.8.8.8                 # UDP                     ║
# ║  mtr -b 8.8.8.8                    # nombre + IP             ║
# ║  mtr -z 8.8.8.8                    # mostrar ASN             ║
# ║                                                                ║
# ║  INTERPRETAR:                                                 ║
# ║  * * *           → router no responde ICMP (puede ser normal) ║
# ║  Loss% en medio  → si no se propaga = ICMP rate limiting     ║
# ║                  → si se propaga = pérdida real               ║
# ║  Salto latencia  → incremento importa, no valor absoluto     ║
# ║  StDev alto      → latencia variable (congestión/jitter)     ║
# ║  !X              → firewall bloqueando                        ║
# ║  !F              → problema de MTU                            ║
# ║                                                                ║
# ║  REGLA DE ORO:                                                ║
# ║  Solo es problema si la pérdida/latencia se mantiene          ║
# ║  hasta el destino final                                        ║
# ║                                                                ║
# ╚══════════════════════════════════════════════════════════════════╝
```

---

## 14. Ejercicios

### Ejercicio 1: comparar modos de traceroute

**Contexto**: necesitas diagnosticar la ruta hacia un servidor web externo.

**Tareas**:

1. Ejecuta traceroute con los tres modos (UDP, ICMP, TCP) al mismo destino
2. Compara las salidas:
   - ¿Llegan los tres al destino?
   - ¿Muestran los mismos saltos intermedios?
   - ¿Algún salto aparece como `* * *` en un modo pero no en otro?
3. Si algún modo no llega, ¿qué tipo de filtrado se aplica en el camino?
4. Ejecuta `mtr -rn -c 50` con ICMP y TCP y compara las estadísticas

**Pistas**:
- Usa destinos reales como 8.8.8.8 o 1.1.1.1
- Los tres modos deberían mostrar los mismos primeros saltos (tu red local)
- Las diferencias suelen aparecer en saltos de peering o cerca del destino
- TCP al puerto 443 es el más fiable para atravesar firewalls

> **Pregunta de reflexión**: ¿por qué traceroute en Linux usa UDP por defecto
> mientras que tracert en Windows usa ICMP? ¿Cuál de los dos es más
> propenso a ser bloqueado por firewalls intermedios?

---

### Ejercicio 2: interpretar salida de mtr

**Contexto**: recibes este reporte de mtr de un usuario que dice que "Internet
está lento":

```
Host                    Loss%   Snt   Avg  Best  Wrst  StDev
1. gateway               0.0%   100   0.5   0.3   1.2    0.2
2. isp-gw                0.0%   100   3.2   2.8   5.1    0.5
3. isp-core             25.0%   100  45.3   8.0 200.5   55.2
4. peering               0.0%   100  12.5  11.0  15.1    1.1
5. cdn-edge              0.0%   100  13.2  12.0  16.3    1.2
6. destino               5.0%   100  55.8  13.0 350.2   70.5
```

**Tareas**:

1. Analiza cada salto y determina si hay un problema real
2. El salto 3 tiene 25% de pérdida — ¿es un problema real o ICMP rate limiting?
3. El salto 6 tiene 5% de pérdida y StDev=70.5 — ¿qué indica?
4. ¿Dónde está el problema real y qué lo causa probablemente?
5. ¿Qué recomendarías al usuario?

**Pistas**:
- Compara Loss% del salto 3 con el salto 4 (¿se propaga?)
- StDev alto + diferencia grande Best/Wrst = variabilidad
- El salto 6 es el destino — ¿puede ser el servidor?
- Un servidor con pérdida + latencia variable puede estar sobrecargado

> **Pregunta de reflexión**: si el reporte muestra pérdida solo en el
> destino final pero todos los intermedios están bien, ¿es un problema de
> red o un problema del servidor? ¿Cómo lo verificarías?

---

### Ejercicio 3: diagnóstico de MTU

**Contexto**: una VPN funciona para tráfico web normal pero las descargas
de archivos grandes se "cuelgan" a mitad de transferencia.

**Tareas**:

1. Ejecuta traceroute con paquetes de diferentes tamaños (576, 1000, 1400,
   1500) a través de la VPN
2. Identifica a partir de qué tamaño empiezan los problemas
3. Usa `tracepath` para descubrir el MTU del path automáticamente
4. Usa `ping` con flag DF (Don't Fragment) para confirmar el MTU máximo:
   ```bash
   ping -M do -s 1472 destino    # 1472 + 28 (cabecera) = 1500
   ping -M do -s 1400 destino    # probar menor
   ```
5. Una vez encontrado el MTU, ¿cómo lo configurarías en la interfaz VPN?

**Pistas**:
- VPNs encapsulan paquetes, reduciendo el MTU efectivo
- WireGuard típicamente usa MTU=1420 (1500 - 80 de overhead)
- `!F` en traceroute = fragmentation needed = MTU excedido
- `ping -M do -s SIZE` envía con Don't Fragment flag

> **Pregunta de reflexión**: ¿por qué la descarga de archivos grandes falla
> pero la navegación web funciona? La respuesta tiene que ver con cómo TCP
> negocia el MSS (Maximum Segment Size) y qué pasa cuando Path MTU
> Discovery falla porque algún router bloquea ICMP "need to frag".

---

> **Siguiente tema**: T04 — nmap (escaneo de puertos, detección de servicios)
