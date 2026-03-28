# ss — Socket Statistics

## Índice

1. [De netstat a ss](#1-de-netstat-a-ss)
2. [Sintaxis básica y opciones principales](#2-sintaxis-básica-y-opciones-principales)
3. [Filtrar por tipo de socket](#3-filtrar-por-tipo-de-socket)
4. [Estados de conexión TCP](#4-estados-de-conexión-tcp)
5. [Filtros de estado](#5-filtros-de-estado)
6. [Filtros de dirección y puerto](#6-filtros-de-dirección-y-puerto)
7. [Información extendida](#7-información-extendida)
8. [Información de procesos](#8-información-de-procesos)
9. [Información de timers](#9-información-de-timers)
10. [Información de memoria](#10-información-de-memoria)
11. [Casos de uso en diagnóstico](#11-casos-de-uso-en-diagnóstico)
12. [Errores comunes](#12-errores-comunes)
13. [Cheatsheet](#13-cheatsheet)
14. [Ejercicios](#14-ejercicios)

---

## 1. De netstat a ss

`ss` (Socket Statistics) es el reemplazo moderno de `netstat`. Forma parte del
paquete `iproute2`, el mismo que proporciona `ip`. Mientras que netstat leía
archivos en `/proc/net/` (lento con miles de conexiones), ss usa la interfaz
netlink del kernel — mucho más rápido y con más información disponible.

### Comparación

| Característica | netstat | ss |
|---------------|---------|-----|
| Paquete | net-tools (obsoleto) | iproute2 (moderno) |
| Fuente de datos | /proc/net/* (texto) | Netlink (binario, kernel directo) |
| Rendimiento | Lento con muchas conexiones | Rápido incluso con millones |
| Filtros | No nativos (requiere grep) | Filtros integrados y potentes |
| Info TCP interna | No | Sí (ventana, RTT, congestion) |
| Info memoria socket | No | Sí (-m) |
| Info timers | Básica | Detallada (-o) |

### Equivalencias rápidas

```bash
netstat -tlnp     →     ss -tlnp       # puertos TCP en escucha
netstat -ulnp     →     ss -ulnp       # puertos UDP en escucha
netstat -an       →     ss -an          # todas las conexiones
netstat -s        →     ss -s           # estadísticas por protocolo
netstat -r        →     ip route        # tabla de rutas (no ss)
```

**Nota**: en muchas distribuciones modernas `netstat` ya no está instalado por
defecto. `ss` está siempre disponible.

---

## 2. Sintaxis básica y opciones principales

### Formato

```
ss [opciones] [filtro]
```

### Opciones fundamentales

| Opción | Significado |
|--------|------------|
| `-t` | Sockets TCP |
| `-u` | Sockets UDP |
| `-l` | Solo sockets en estado LISTEN |
| `-a` | Todos los estados (incluyendo LISTEN) |
| `-n` | No resolver nombres (mostrar números) |
| `-p` | Mostrar proceso propietario |
| `-e` | Información extendida (uid, ino, sk) |
| `-i` | Información TCP interna (RTT, ventana, congestion) |
| `-m` | Información de memoria del socket |
| `-o` | Información de timers |
| `-s` | Resumen estadístico |
| `-4` | Solo IPv4 |
| `-6` | Solo IPv6 |
| `-x` | Sockets Unix |
| `-w` | Sockets raw |

### Los comandos más usados

```bash
# Puertos TCP en escucha con proceso (el más común)
ss -tlnp

# Puertos UDP en escucha con proceso
ss -ulnp

# Todas las conexiones TCP (establecidas + escucha + todo)
ss -tan

# Todas las conexiones con proceso (requiere root para ver todos)
sudo ss -tanp

# Resumen estadístico
ss -s
```

### Salida típica de ss -tlnp

```bash
$ ss -tlnp
State    Recv-Q  Send-Q   Local Address:Port    Peer Address:Port  Process
LISTEN   0       128      0.0.0.0:22             0.0.0.0:*          users:(("sshd",pid=1234,fd=3))
LISTEN   0       511      0.0.0.0:80             0.0.0.0:*          users:(("nginx",pid=5678,fd=6))
LISTEN   0       128      127.0.0.1:5432         0.0.0.0:*          users:(("postgres",pid=9012,fd=5))
LISTEN   0       128      [::]:22                [::]:*             users:(("sshd",pid=1234,fd=4))
```

Columnas:
- **State**: estado del socket (LISTEN, ESTAB, TIME-WAIT…)
- **Recv-Q**: para LISTEN = conexiones pendientes de accept(); para ESTAB = bytes no leídos
- **Send-Q**: para LISTEN = backlog máximo; para ESTAB = bytes no enviados
- **Local Address:Port**: dirección y puerto local
- **Peer Address:Port**: dirección y puerto remoto (`*` si es LISTEN)
- **Process**: nombre del proceso, PID y file descriptor

---

## 3. Filtrar por tipo de socket

### TCP

```bash
# TCP en escucha
ss -tl

# TCP establecidos (sin LISTEN)
ss -t

# TCP todos los estados
ss -ta
```

### UDP

```bash
# UDP en escucha (UDP no tiene "establecido" estrictamente)
ss -ul

# UDP todos
ss -ua
```

UDP no tiene estados como TCP. Un socket UDP en "UNCONN" (Unconnected) es
simplemente un socket que no ha llamado a `connect()` — lo normal para
servidores UDP.

```bash
$ ss -ulnp
State    Recv-Q  Send-Q   Local Address:Port    Peer Address:Port  Process
UNCONN   0       0        0.0.0.0:68             0.0.0.0:*          users:(("dhclient",pid=890,fd=7))
UNCONN   0       0        127.0.0.53%lo:53       0.0.0.0:*          users:(("systemd-resolve",pid=650,fd=13))
```

### Unix sockets

```bash
# Sockets Unix en escucha
ss -xl

# Sockets Unix con proceso
ss -xlp

# Ejemplo de salida:
# u_str LISTEN 0 128 /run/dbus/system_bus_socket 15482 * 0 users:(("dbus-daemon",pid=645,fd=4))
# u_str LISTEN 0 4096 /var/run/docker.sock 20183 * 0 users:(("dockerd",pid=1200,fd=4))
```

### Raw sockets

```bash
# Sockets raw (ICMP, protocolos no TCP/UDP)
ss -w

# Ejemplo: ping abierto
# raw UNCONN 0 0 *:icmp *:* users:(("ping",pid=12345,fd=3))
```

### Combinar tipos

```bash
# TCP y UDP juntos
ss -tua

# TCP, UDP y Unix
ss -tuxa
```

---

## 4. Estados de conexión TCP

Comprender los estados TCP es fundamental para diagnosticar problemas de red.

### Diagrama de estados TCP

```
Cliente                                          Servidor
───────                                          ────────

                                                 LISTEN
                                                   │
  CLOSED ──── SYN ──────────────────────────────▶  │
    │                                              │
  SYN-SENT ◀──── SYN+ACK ─────────────────────── SYN-RECV
    │                                              │
    │ ──── ACK ──────────────────────────────────▶ │
    │                                              │
  ESTABLISHED ◀──────────────────────────────── ESTABLISHED
    │                                              │
    │ ════ transferencia de datos ═══════════════  │
    │                                              │
  (cierre activo)                          (cierre pasivo)
    │                                              │
    │ ──── FIN ──────────────────────────────────▶ │
  FIN-WAIT-1                                    CLOSE-WAIT
    │                                              │
    │ ◀──── ACK ──────────────────────────────── │
  FIN-WAIT-2                                    (app cierra)
    │                                              │
    │ ◀──── FIN ──────────────────────────────── LAST-ACK
    │                                              │
    │ ──── ACK ──────────────────────────────────▶ │
  TIME-WAIT                                     CLOSED
    │  (espera 2×MSL)
  CLOSED
```

### Qué indica cada estado al diagnosticar

| Estado | Qué significa | ¿Problema? |
|--------|--------------|------------|
| **LISTEN** | Servidor esperando conexiones | Normal para servicios |
| **SYN-SENT** | Cliente envió SYN, espera respuesta | Muchos = servidor no responde o firewall bloquea |
| **SYN-RECV** | Servidor recibió SYN, espera ACK | Muchos = posible SYN flood |
| **ESTABLISHED** | Conexión activa y funcional | Normal |
| **FIN-WAIT-1** | Lado activo envió FIN, espera ACK | Se acumula = peer lento cerrando |
| **FIN-WAIT-2** | ACK recibido, espera FIN del peer | Se acumula = app remota no cierra su lado |
| **CLOSE-WAIT** | Recibió FIN, app local no ha cerrado | **Problema**: app no cierra sockets → leak |
| **LAST-ACK** | Envió FIN, espera ACK final | Transitorio, raro verlo |
| **TIME-WAIT** | Conexión cerrada, espera 2×MSL | Normal, miles en servidores con mucho tráfico |
| **CLOSING** | Ambos lados enviaron FIN simultáneamente | Raro, transitorio |

### Los estados problemáticos

```bash
# CLOSE-WAIT acumulándose = bug en la aplicación
ss -tan state close-wait | wc -l
# Si crece con el tiempo → la app no cierra sockets correctamente

# SYN-RECV acumulándose = posible ataque SYN flood
ss -tan state syn-recv | wc -l
# Cientos/miles sostenidos → posible ataque

# TIME-WAIT en grandes cantidades = normal pero puede agotar puertos
ss -tan state time-wait | wc -l
# Miles en un servidor web con mucho tráfico es normal
# Si agota puertos efímeros → habilitar tw_reuse
```

---

## 5. Filtros de estado

ss permite filtrar directamente por estado TCP sin recurrir a grep.

### Sintaxis

```bash
ss [opciones] state ESTADO
ss [opciones] exclude ESTADO
```

### Filtrar por estado individual

```bash
# Solo conexiones establecidas
ss -tan state established

# Solo sockets en escucha
ss -tln state listening

# Solo TIME-WAIT
ss -tan state time-wait

# Solo CLOSE-WAIT (detectar leaks)
ss -tan state close-wait

# Solo SYN-RECV (detectar SYN floods)
ss -tan state syn-recv
```

### Excluir estados

```bash
# Todo excepto LISTEN y TIME-WAIT
ss -tan exclude listening exclude time-wait
```

### Grupos de estados

ss define grupos predefinidos para filtrar múltiples estados:

```bash
# connected = todo excepto LISTEN y CLOSED
ss -tan state connected

# synchronized = ESTABLISHED + cierre (FIN-WAIT-*, CLOSE-WAIT, LAST-ACK, CLOSING, TIME-WAIT)
ss -tan state synchronized

# bucket = SYN-RECV + TIME-WAIT (estados manejados como mini-sockets en el kernel)
ss -tan state bucket

# big = todo excepto bucket (sockets completos)
ss -tan state big
```

### Contar conexiones por estado

```bash
# Resumen rápido de todos los estados
ss -s
# Total: 352
# TCP:   287 (estab 145, closed 12, orphaned 0, timewait 130)
# UDP:   8
# RAW:   0

# Contar establecidas
ss -tan state established | tail -n +2 | wc -l

# Contar por estado (script útil)
for state in established syn-sent syn-recv fin-wait-1 fin-wait-2 \
    time-wait close-wait last-ack closing; do
    count=$(ss -tan state $state | tail -n +2 | wc -l)
    [ "$count" -gt 0 ] && echo "$state: $count"
done
```

---

## 6. Filtros de dirección y puerto

ss tiene un sistema de filtros potente que evita depender de `grep`.

### Sintaxis de filtros

```bash
ss [opciones] [ EXPRESIÓN ]
```

Los filtros se aplican después de las opciones, usando operadores:

| Operador | Significado |
|----------|------------|
| `sport` | Puerto de origen (source port) |
| `dport` | Puerto de destino (destination port) |
| `src` | Dirección IP de origen |
| `dst` | Dirección IP de destino |
| `==` o `eq` | Igual a |
| `!=` o `ne` | Diferente de |
| `>` o `gt` | Mayor que |
| `<` o `lt` | Menor que |
| `>=` o `ge` | Mayor o igual |
| `<=` o `le` | Menor o igual |
| `and` | Y lógico |
| `or` | O lógico |
| `not` | Negación |

### Filtrar por puerto

```bash
# Conexiones al puerto 80
ss -tan 'dport == 80'

# Conexiones al puerto 443 o 80
ss -tan '( dport == 80 or dport == 443 )'

# Conexiones desde un puerto efímero alto
ss -tan 'sport > 1024'

# Conexiones a puertos conocidos (< 1024)
ss -tan 'dport < 1024'

# Usar nombre de servicio en vez de número
ss -tan 'dport == :ssh'
ss -tan 'dport == :https'
```

### Filtrar por dirección IP

```bash
# Conexiones desde una IP específica
ss -tan 'src 192.168.1.100'

# Conexiones hacia una IP específica
ss -tan 'dst 203.0.113.5'

# Conexiones desde una subred
ss -tan 'src 192.168.1.0/24'

# Combinar IP y puerto
ss -tan 'dst 203.0.113.5 and dport == 443'
```

### Filtros compuestos

```bash
# Conexiones SSH desde la red interna
ss -tan 'src 192.168.1.0/24 and dport == 22'

# Conexiones establecidas al puerto 80 desde una IP
ss -tan state established 'src 10.0.0.50 and dport == 80'

# Todo excepto localhost
ss -tan 'not src 127.0.0.0/8'

# Conexiones a PostgreSQL o MySQL desde fuera de la LAN
ss -tan '( dport == 5432 or dport == 3306 ) and not src 192.168.1.0/24'
```

### Filtrar por dirección local vs remota

```bash
# Quién está conectado a MI puerto 80 (soy el servidor)
ss -tan 'sport == 80'

# A qué servidores estoy conectado por HTTPS (soy el cliente)
ss -tan 'dport == 443'
```

**Predicción**: `sport` y `dport` se refieren a la perspectiva local. `sport` es
el puerto de TU máquina, `dport` es el puerto de la máquina remota. Cuando eres
el servidor web, tus clientes se ven con `sport == 80`.

---

## 7. Información extendida

### -e: información extendida del socket

```bash
ss -tane
# ESTAB 0 0 192.168.1.10:22 192.168.1.50:52341
#   uid:0 ino:45678 sk:ffff8a0012345678 cgroup:/system.slice/sshd.service <->
```

Campos adicionales:
- **uid**: UID del propietario del socket
- **ino**: número de inodo del socket
- **sk**: dirección del socket en memoria kernel
- **cgroup**: cgroup del proceso (útil con systemd y contenedores)

### -i: información TCP interna

```bash
ss -ti
# ESTAB 0 0 192.168.1.10:22 192.168.1.50:52341
#   cubic wscale:7,7 rto:204 rtt:1.5/0.75 ato:40 mss:1448 pmtu:1500
#   rcvmss:1448 advmss:1448 cwnd:10 bytes_sent:123456 bytes_received:78901
#   bytes_acked:123457 segs_out:890 segs_in:456 data_segs_out:445
#   data_segs_in:228 send 77.3Mbps lastsnd:120 lastrcv:80 lastack:80
#   pacing_rate 154.5Mbps delivery_rate 65.2Mbps delivered:446
#   busy:5600ms rcv_space:29200 rcv_ssthresh:29200 minrtt:0.5
```

Campos clave para diagnóstico:

| Campo | Significado | Qué indica |
|-------|------------|------------|
| `rtt` | Round-trip time (ms) | Latencia de la conexión |
| `rto` | Retransmission timeout (ms) | Cuánto espera antes de retransmitir |
| `cwnd` | Congestion window (segmentos) | Tamaño de ventana de congestión |
| `mss` | Maximum segment size | Tamaño máximo de segmento |
| `pmtu` | Path MTU | MTU del camino |
| `send` | Tasa de envío estimada | Ancho de banda disponible |
| `retrans` | Retransmisiones | Pérdida de paquetes (valor alto = problema) |
| `bytes_sent/received` | Volumen transferido | Total de la conexión |
| `cubic/bbr/reno` | Algoritmo de congestión | Control de congestión activo |
| `wscale` | Window scaling factor | Factor de escalado de ventana |
| `lastsnd/lastrcv` | Último envío/recepción (ms) | Actividad reciente |

```bash
# Diagnóstico: conexiones con retransmisiones (pérdida de paquetes)
ss -ti state established | grep -B1 retrans

# Diagnóstico: conexiones con RTT alto (latencia)
ss -ti state established | grep -E "rtt:[0-9]{3,}"

# Diagnóstico: conexiones con ventana de congestión pequeña
ss -ti state established | grep -E "cwnd:[12] "
```

---

## 8. Información de procesos

### -p: mostrar proceso propietario

```bash
# Requiere root para ver procesos de otros usuarios
sudo ss -tlnp

# Salida:
# LISTEN 0 128 0.0.0.0:22 0.0.0.0:*
#   users:(("sshd",pid=1234,fd=3))
# LISTEN 0 511 0.0.0.0:80 0.0.0.0:*
#   users:(("nginx",pid=5678,fd=6),("nginx",pid=5679,fd=6))
```

El formato de `users` muestra:
- **Nombre del proceso** entre comillas
- **pid**: PID del proceso
- **fd**: file descriptor del socket

### Usos comunes con -p

```bash
# ¿Qué proceso está usando el puerto 8080?
sudo ss -tlnp 'sport == 8080'

# ¿Qué conexiones tiene el PID 1234?
ss -tanp | grep "pid=1234"

# ¿Qué proceso está conectado a la base de datos?
sudo ss -tanp 'dport == 5432'

# ¿nginx tiene conexiones establecidas?
sudo ss -tanp | grep nginx
```

### Sin root: limitaciones

```bash
# Sin root: solo ves tus propios procesos
ss -tlnp
# LISTEN 0 128 0.0.0.0:22 0.0.0.0:*
#   (sin información de proceso — el socket pertenece a root)

# Con root: ves todo
sudo ss -tlnp
# LISTEN 0 128 0.0.0.0:22 0.0.0.0:*
#   users:(("sshd",pid=1234,fd=3))
```

---

## 9. Información de timers

### -o: mostrar timers de socket

```bash
ss -tano

# ESTAB 0 0 192.168.1.10:22 192.168.1.50:52341 timer:(keepalive,25sec,0)
# TIME-WAIT 0 0 192.168.1.10:80 10.0.0.5:39221 timer:(timewait,45sec,0)
# SYN-RECV 0 0 192.168.1.10:80 203.0.113.50:12345 timer:(on,980ms,0)
```

### Tipos de timer

| Timer | Significado |
|-------|------------|
| `on` | Retransmisión activa (esperando ACK) |
| `keepalive` | Timer de keepalive TCP |
| `timewait` | Socket en TIME-WAIT (countdown para cerrar) |
| `persist` | Timer de persist (ventana 0, sondeo) |
| `off` | Sin timer activo |

El formato es: `timer:(tipo,tiempo_restante,reintentos)`

### Diagnóstico con timers

```bash
# Conexiones con retransmisiones activas (posible pérdida de paquetes)
ss -tano | grep "timer:(on,"

# Conexiones en TIME-WAIT con su countdown
ss -tano state time-wait

# Conexiones keepalive (inactivas pero mantenidas abiertas)
ss -tano | grep "timer:(keepalive,"

# Conexiones con muchos reintentos (último campo alto)
ss -tano | grep -E "timer:\(on,[^,]+,[3-9]\)"
```

---

## 10. Información de memoria

### -m: memoria del socket

```bash
ss -tanm

# ESTAB 0 0 192.168.1.10:80 10.0.0.5:39221
#   skmem:(r0,rb131072,t0,tb46080,f0,w0,o0,bl0,d0)
```

| Campo | Significado |
|-------|------------|
| `r` | Memoria de recepción usada |
| `rb` | Tamaño del buffer de recepción |
| `t` | Memoria de transmisión usada |
| `tb` | Tamaño del buffer de transmisión |
| `f` | Memoria en forward alloc |
| `w` | Memoria en write queue |
| `o` | Memoria en opt (opciones) |
| `bl` | Backlog |
| `d` | Drops |

### Diagnóstico con memoria

```bash
# Sockets con buffer de recepción muy lleno (app no lee rápido)
ss -tanm | grep -E "r[0-9]{5,}"

# Sockets con drops (d > 0)
ss -tanm | grep -v "d0)"
```

---

## 11. Casos de uso en diagnóstico

### Caso 1: "No puedo conectar a un servicio"

```bash
# Paso 1: ¿El servicio está escuchando?
ss -tlnp 'sport == 80'
# Si vacío → el servicio no está corriendo o escucha en otro puerto

# Paso 2: ¿En qué dirección escucha?
ss -tlnp 'sport == 80'
# 127.0.0.1:80  → solo acepta conexiones locales
# 0.0.0.0:80    → acepta de cualquier interfaz ✓
# 10.0.0.1:80   → solo de esa IP específica

# Paso 3: ¿Hay conexiones llegando?
ss -tan 'sport == 80'
# ESTAB → hay conexiones activas
# SYN-RECV → llegan pero no completan (firewall, timeout)
# Vacío → el tráfico no llega (firewall, routing)
```

### Caso 2: "El servidor está lento"

```bash
# ¿Cuántas conexiones simultáneas?
ss -tan state established 'sport == 80' | tail -n +2 | wc -l

# ¿Hay muchas en estados de cierre?
ss -s

# ¿Hay conexiones con retransmisiones altas?
ss -ti state established 'sport == 80' | grep retrans

# ¿El backlog de LISTEN está lleno?
ss -tln 'sport == 80'
# LISTEN  128  128  ...
# Recv-Q = Send-Q → backlog lleno → no acepta nuevas conexiones
```

### Caso 3: "Se agotan los puertos"

```bash
# Contar TIME-WAIT (cada una ocupa un puerto efímero)
ss -tan state time-wait | tail -n +2 | wc -l

# Ver el rango de puertos efímeros
cat /proc/sys/net/ipv4/ip_local_port_range
# 32768   60999  (28.232 puertos disponibles)

# Si TIME-WAIT se acerca al rango → problema
# Soluciones:
sysctl -w net.ipv4.tcp_tw_reuse=1    # reutilizar TIME-WAIT
# O aumentar el rango:
sysctl -w net.ipv4.ip_local_port_range="1024 65535"
```

### Caso 4: "¿Quién está consumiendo ancho de banda?"

```bash
# Conexiones con más datos transferidos
ss -ti state established | grep -E "bytes_(sent|received):[0-9]{7,}" -B1

# Conexiones con mayor tasa de envío
ss -ti state established | grep -E "send [0-9]+\.[0-9]+[MG]bps" -B1

# Top IPs conectadas al servidor web
sudo ss -tan 'sport == 80' | awk '{print $5}' | cut -d: -f1 | \
    sort | uniq -c | sort -rn | head -10
```

### Caso 5: "Posible leak de sockets"

```bash
# CLOSE-WAIT creciente indica que la app no cierra sockets
watch -n 5 'ss -tan state close-wait | wc -l'

# ¿Qué proceso tiene los sockets en CLOSE-WAIT?
sudo ss -tanp state close-wait

# Si un proceso acumula miles de CLOSE-WAIT:
# → Bug en la aplicación (no llama a close() en el socket)
# → Solución: arreglar la app, no el kernel
```

### Caso 6: "¿Docker/contenedor está escuchando?"

```bash
# Ver qué puertos están publicados en el host
sudo ss -tlnp | grep docker-proxy
# LISTEN 0 128 0.0.0.0:8080 0.0.0.0:*
#   users:(("docker-proxy",pid=12345,fd=4))

# Ver conexiones del contenedor (por cgroup con -e)
sudo ss -tane | grep "cgroup:.*docker"
```

---

## 12. Errores comunes

### Error 1: olvidar -n y esperar respuesta lenta

```bash
# ✗ Sin -n: ss intenta resolver cada IP con DNS reverso
ss -tap
# Puede tardar minutos si tienes cientos de conexiones

# ✓ Siempre usar -n para velocidad
ss -tanp
```

### Error 2: confundir sport y dport

```bash
# ✗ "¿Quién está conectado a mi servidor web?"
ss -tan 'dport == 80'
# Esto muestra conexiones donde TU MÁQUINA se conecta AL puerto 80 de otros

# ✓ Si eres el servidor, tu puerto local es sport
ss -tan 'sport == 80'
# Esto muestra conexiones donde otros se conectan a TU puerto 80
```

### Error 3: no usar sudo y no ver procesos

```bash
# ✗ Sin root: la columna Process está vacía para sockets de otros usuarios
ss -tlnp
# LISTEN 0 128 0.0.0.0:22 0.0.0.0:*
#   (sin proceso visible)

# ✓ Con root
sudo ss -tlnp
# LISTEN 0 128 0.0.0.0:22 0.0.0.0:*
#   users:(("sshd",pid=1234,fd=3))
```

### Error 4: confundir Recv-Q según el estado

```bash
# En LISTEN:
#   Recv-Q = conexiones pendientes de accept()  (cola SYN)
#   Send-Q = backlog máximo configurado
# LISTEN  45  128  0.0.0.0:80  → 45 pendientes de 128 posibles

# En ESTABLISHED:
#   Recv-Q = bytes recibidos pero no leídos por la app
#   Send-Q = bytes enviados pero no confirmados por el peer
# ESTAB  0  1380  192.168.1.10:80 10.0.0.5:39221
#   → 1380 bytes esperando ACK del otro lado

# No confundir: Recv-Q alto en LISTEN = app lenta aceptando
#               Recv-Q alto en ESTAB = app lenta leyendo datos
```

### Error 5: filtros sin comillas

```bash
# ✗ El shell interpreta los paréntesis y operadores
ss -tan ( dport == 80 or dport == 443 )
# bash: syntax error

# ✓ Comillas simples para proteger la expresión
ss -tan '( dport == 80 or dport == 443 )'
```

---

## 13. Cheatsheet

```bash
# ╔══════════════════════════════════════════════════════════════════╗
# ║                    SS — CHEATSHEET                             ║
# ╠══════════════════════════════════════════════════════════════════╣
# ║                                                                ║
# ║  BÁSICOS:                                                     ║
# ║  ss -tlnp                   # TCP LISTEN + proceso            ║
# ║  ss -ulnp                   # UDP LISTEN + proceso            ║
# ║  ss -tanp                   # todas TCP + proceso             ║
# ║  ss -s                      # resumen estadístico             ║
# ║                                                                ║
# ║  POR ESTADO:                                                  ║
# ║  ss -tan state established  # solo establecidas               ║
# ║  ss -tan state time-wait    # solo TIME-WAIT                  ║
# ║  ss -tan state close-wait   # solo CLOSE-WAIT (leak!)         ║
# ║  ss -tan state syn-recv     # solo SYN-RECV (SYN flood?)      ║
# ║  ss -tan state listening    # solo LISTEN                     ║
# ║                                                                ║
# ║  POR PUERTO:                                                  ║
# ║  ss -tan 'sport == 80'      # mi puerto 80 (soy servidor)    ║
# ║  ss -tan 'dport == 443'     # conecto a puerto 443 (cliente) ║
# ║  ss -tan 'dport == :ssh'    # por nombre de servicio          ║
# ║                                                                ║
# ║  POR IP:                                                      ║
# ║  ss -tan 'src 192.168.1.0/24'    # desde esa red             ║
# ║  ss -tan 'dst 10.0.0.1'          # hacia esa IP              ║
# ║                                                                ║
# ║  COMBINADOS:                                                  ║
# ║  ss -tan 'sport == 80 and src 10.0.0.0/8'                    ║
# ║  ss -tan state established 'dport == 443'                     ║
# ║  ss -tan '( dport == 80 or dport == 443 )'                   ║
# ║                                                                ║
# ║  DIAGNÓSTICO:                                                 ║
# ║  ss -ti                     # info TCP (RTT, cwnd, retrans)   ║
# ║  ss -tano                   # timers (keepalive, retrans)     ║
# ║  ss -tanm                   # memoria de sockets              ║
# ║  ss -tane                   # extendida (uid, cgroup)         ║
# ║                                                                ║
# ║  CONTAR:                                                      ║
# ║  ss -tan state X | tail -n +2 | wc -l  # contar estado X     ║
# ║  sudo ss -tan 'sport == 80' | awk '{print $5}' | \            ║
# ║    cut -d: -f1 | sort | uniq -c | sort -rn | head  # top IPs ║
# ║                                                                ║
# ╚══════════════════════════════════════════════════════════════════╝
```

---

## 14. Ejercicios

### Ejercicio 1: auditoría de servicios

**Contexto**: acabas de recibir acceso a un servidor Linux y necesitas
inventariar qué servicios están expuestos.

**Tareas**:

1. Lista todos los puertos TCP en escucha con su proceso propietario
2. Lista todos los puertos UDP en escucha con su proceso propietario
3. Identifica servicios que escuchan en `0.0.0.0` (todas las interfaces) vs
   `127.0.0.1` (solo local)
4. Verifica si hay sockets Unix de Docker o de bases de datos
5. Para cada servicio expuesto a todas las interfaces, determina si debería
   estar restringido a localhost

**Pistas**:
- Necesitas `sudo` para ver los procesos
- `0.0.0.0` y `[::]` significan "todas las interfaces"
- `127.0.0.1` y `[::1]` significan "solo local"
- Un servicio que solo necesita comunicación local (ej. PostgreSQL para una
  app en el mismo servidor) debería escuchar en 127.0.0.1

> **Pregunta de reflexión**: si un servicio escucha en `0.0.0.0:3306` pero
> el firewall bloquea el puerto 3306, ¿el servicio está realmente protegido?
> ¿Qué pasa si alguien desactiva el firewall temporalmente? ¿Es defensa en
> profundidad configurar tanto el bind address como el firewall?

---

### Ejercicio 2: diagnóstico de conexiones

**Contexto**: un servidor web Nginx reporta lentitud. Necesitas diagnosticar
usando `ss`.

**Tareas**:

1. Cuenta las conexiones establecidas al puerto 80 y 443
2. Cuenta las conexiones en cada estado TCP
3. Verifica si el backlog de LISTEN está lleno (Recv-Q cercano a Send-Q)
4. Busca conexiones con retransmisiones (indicador de pérdida de paquetes)
5. Identifica las 10 IPs con más conexiones simultáneas
6. Verifica si hay sockets CLOSE-WAIT (indicador de leak en la app)
7. Cuenta los TIME-WAIT y compara con el rango de puertos efímeros

**Pistas**:
- `ss -ti` para retransmisiones
- `ss -tln 'sport == 80'` para backlog
- El rango de puertos está en `/proc/sys/net/ipv4/ip_local_port_range`
- Si TIME-WAIT > 50% del rango de puertos → potencial problema

> **Pregunta de reflexión**: TIME-WAIT existe para evitar que paquetes retrasados
> de una conexión anterior se confundan con una nueva conexión en los mismos
> puertos. ¿Por qué entonces `tcp_tw_reuse` es seguro de activar pero
> `tcp_tw_recycle` fue eliminado del kernel (4.12+)?

---

### Ejercicio 3: monitorización continua

**Contexto**: necesitas crear un script que monitorice las conexiones de un
servidor y alerte sobre anomalías.

**Tareas**:

1. Escribe un script que cada 10 segundos muestre:
   - Total de conexiones establecidas
   - Total de TIME-WAIT
   - Total de CLOSE-WAIT
   - Total de SYN-RECV
   - Top 5 IPs por número de conexiones
2. Añade alertas (un simple `echo` con mensaje) cuando:
   - CLOSE-WAIT > 50
   - SYN-RECV > 100
   - Una sola IP tiene > 200 conexiones
3. Guarda las métricas en un archivo CSV con timestamp

**Pistas**:
- Usa `ss -tan state X | tail -n +2 | wc -l` para contar
- `date +%Y-%m-%dT%H:%M:%S` para timestamp
- Para el top de IPs: `awk` + `sort` + `uniq -c`
- No necesitas herramientas externas, solo `ss`, `awk`, `sort`, `wc`

> **Pregunta de reflexión**: este tipo de monitorización manual con scripts
> es frágil y limitada. ¿Qué herramientas de monitorización real (Prometheus
> + node_exporter, Grafana, Zabbix) expondrían estas mismas métricas de forma
> más robusta? ¿Qué métricas de `ss` serían las más valiosas en un dashboard?

---

> **Siguiente tema**: T02 — tcpdump (captura de paquetes, filtros BPF, análisis básico)
