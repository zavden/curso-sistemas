# IPv4: Formato de Dirección, Clases y CIDR

## Índice

1. [Qué es IPv4](#1-qué-es-ipv4)
2. [Formato de una dirección IPv4](#2-formato-de-una-dirección-ipv4)
3. [Clases de direcciones (histórico)](#3-clases-de-direcciones-histórico)
4. [CIDR (Classless Inter-Domain Routing)](#4-cidr-classless-inter-domain-routing)
5. [Máscaras de red](#5-máscaras-de-red)
6. [Direcciones especiales](#6-direcciones-especiales)
7. [Direcciones privadas vs públicas](#7-direcciones-privadas-vs-públicas)
8. [IPv4 en Linux](#8-ipv4-en-linux)
9. [Agotamiento de IPv4](#9-agotamiento-de-ipv4)
10. [Errores comunes](#10-errores-comunes)
11. [Cheatsheet](#11-cheatsheet)
12. [Ejercicios](#12-ejercicios)

---

## 1. Qué es IPv4

IPv4 (Internet Protocol version 4) es el protocolo de capa de red que asigna una
dirección lógica a cada dispositivo conectado a una red IP. Es el protocolo que
permite que un paquete encuentre su camino desde el origen hasta el destino a través
de múltiples redes.

```
Sin IP:  Los dispositivos solo pueden hablar con vecinos directos (capa de enlace)
Con IP:  Los dispositivos pueden hablar con cualquier host en Internet
```

IPv4 lleva funcionando desde 1983 y sigue siendo el protocolo dominante, aunque
IPv6 está creciendo. Todo lo que aprendas de IPv4 aplica conceptualmente a IPv6
con diferencias de formato.

---

## 2. Formato de una dirección IPv4

### Representación

Una dirección IPv4 tiene **32 bits** (4 bytes), representados como 4 números
decimales separados por puntos (notación dotted-decimal):

```
Binario:    11000000 10101000 00000001 00001010
Decimal:       192  .   168  .    1   .   10
Hexadecimal:  C0   .   A8   .   01   .   0A

Cada octeto va de 0 a 255 (8 bits = 2^8 = 256 valores)
```

### Conversión binario ↔ decimal

```
Posiciones de bits en un octeto (8 bits):

 128   64   32   16    8    4    2    1
  │     │    │    │    │    │    │    │
  ▼     ▼    ▼    ▼    ▼    ▼    ▼    ▼

Ejemplo: 192 en binario
  128 + 64 = 192
    1    1    0    0    0    0    0    0  = 11000000

Ejemplo: 168 en binario
  128 + 32 + 8 = 168
    1    0    1    0    1    0    0    0  = 10101000

Ejemplo: 10 en binario
  8 + 2 = 10
    0    0    0    0    1    0    1    0  = 00001010
```

Esta conversión es fundamental para entender subnetting (siguiente tema).

### Partes de una dirección IP

Toda dirección IPv4 se divide en dos partes:

```
        192.168.1.10 / 24
        ─────────── ────
             │        │
             │        └── Prefijo: cuántos bits son la parte de RED
             └─────────── Dirección IP completa

        11000000.10101000.00000001 . 00001010
        ─────────────────────────   ────────
                Red (24 bits)        Host (8 bits)

Red:   identifica LA RED a la que pertenece el host
Host:  identifica EL HOST dentro de esa red
```

La máscara de red define dónde termina la parte de red y empieza la parte de host.

---

## 3. Clases de direcciones (histórico)

En los inicios de Internet (1981-1993), las direcciones se asignaban por **clases**.
Este sistema ya no se usa para asignación, pero aparece en exámenes y en la
terminología cotidiana.

### Las 5 clases

```
Clase  Primer octeto   Máscara default   Redes      Hosts/red
────── ─────────────── ──────────────── ────────── ──────────
  A       1 - 126      255.0.0.0 (/8)       126    16,777,214
  B     128 - 191      255.255.0.0 (/16)  16,384       65,534
  C     192 - 223      255.255.255.0 (/24) 2,097,152      254
  D     224 - 239      (multicast, sin máscara)
  E     240 - 255      (experimental/reservado)
```

### Cómo identificar la clase

```
Primer bit(s) del primer octeto:

Clase A:  0xxxxxxx    →  0-127     (0 = red 0 reservada, 127 = loopback)
Clase B:  10xxxxxx    →  128-191
Clase C:  110xxxxx    →  192-223
Clase D:  1110xxxx    →  224-239   (multicast)
Clase E:  1111xxxx    →  240-255   (reservado)
```

### Por qué las clases son un problema

```
Ejemplo: Una empresa con 300 hosts

  Clase C (/24): máximo 254 hosts → no alcanza
  Clase B (/16): máximo 65,534 hosts → desperdicia 65,234 direcciones

  No hay clase intermedia.

Resultado: se asignaban redes Clase B a organizaciones que solo necesitaban
unos cientos de hosts → desperdicio masivo de direcciones IPv4
```

Este desperdicio aceleró el agotamiento de IPv4 y motivó la creación de CIDR.

---

## 4. CIDR (Classless Inter-Domain Routing)

### Qué es

CIDR (pronunciado "cider") reemplaza el sistema de clases con una máscara de longitud
variable. En lugar de estar limitado a /8, /16 o /24, puedes usar **cualquier
longitud de prefijo** de /0 a /32.

```
Clases (rígido):          CIDR (flexible):
  /8  → 16M hosts          /20 → 4094 hosts
  /16 → 65K hosts          /22 → 1022 hosts
  /24 → 254 hosts          /26 → 62 hosts
                            /28 → 14 hosts
  Solo 3 opciones           Cualquier tamaño
```

### Notación CIDR

```
192.168.1.0/24
             ──
              │
              └── Número de bits que forman la parte de RED

Significa:
  Los primeros 24 bits (3 octetos) = red
  Los últimos 8 bits (1 octeto)   = host

  Red:    192.168.1
  Hosts:  .0 a .255 (256 direcciones, 254 usables)
```

### Ejemplos de CIDR

```
10.0.0.0/8       → Red enorme: 10.x.x.x (16M hosts)
172.16.0.0/12    → Red grande: 172.16.0.0 - 172.31.255.255
192.168.1.0/24   → Red estándar: 192.168.1.x (254 hosts)
192.168.1.0/25   → Media red: 192.168.1.0 - 192.168.1.127 (126 hosts)
192.168.1.128/26 → Cuarto de red: 192.168.1.128 - 192.168.1.191 (62 hosts)
10.0.0.4/32      → Un solo host (host route)
0.0.0.0/0        → Todas las redes (default route)
```

---

## 5. Máscaras de red

### Qué es una máscara

La máscara de red es un valor de 32 bits donde los **1s contiguos** marcan la parte
de red y los **0s contiguos** marcan la parte de host.

```
Máscara /24:
  Binario:  11111111.11111111.11111111.00000000
  Decimal:  255     .255     .255     .0
            ────────────────────────── ─────────
            Red (24 bits)              Host (8 bits)

Máscara /20:
  Binario:  11111111.11111111.11110000.00000000
  Decimal:  255     .255     .240     .0
            ────────────────────── ─────────────
            Red (20 bits)          Host (12 bits)
```

### Tabla de máscaras comunes

```
CIDR    Máscara             Hosts usables   Uso típico
─────── ─────────────────── ─────────────── ──────────────────
/8      255.0.0.0           16,777,214      Redes privadas grandes
/12     255.240.0.0         1,048,574       Rango 172.16-31
/16     255.255.0.0         65,534          Redes corporativas
/20     255.255.240.0       4,094           Subredes medianas
/21     255.255.248.0       2,046           Subredes medianas
/22     255.255.252.0       1,022           Subredes medianas
/23     255.255.254.0       510             Subredes medianas
/24     255.255.255.0       254             LAN típica
/25     255.255.255.128     126             Mitad de /24
/26     255.255.255.192     62              Cuarto de /24
/27     255.255.255.224     30              Redes pequeñas
/28     255.255.255.240     14              Redes muy pequeñas
/29     255.255.255.248     6               Point-to-point
/30     255.255.255.252     2               Enlaces entre routers
/31     255.255.255.254     2*              Enlaces punto a punto (RFC 3021)
/32     255.255.255.255     1               Host route
```

*`/31` es un caso especial: sin broadcast ni network address, solo 2 hosts.

### Calcular con la máscara

Para determinar a qué red pertenece una IP, se aplica un AND lógico entre la IP y
la máscara:

```
IP:       192.168.1.130     = 11000000.10101000.00000001.10000010
Máscara:  255.255.255.192   = 11111111.11111111.11111111.11000000
                              ────────────────────────────────────
Resultado (AND):              11000000.10101000.00000001.10000000
                            = 192.168.1.128   ← Dirección de RED

Host:     130 AND 192 = 128
          10000010
          11000000
          ────────
          10000000 = 128

Red:      192.168.1.128/26
Rango:    192.168.1.128 - 192.168.1.191
Broadcast: 192.168.1.191
Hosts:    192.168.1.129 - 192.168.1.190 (62 hosts usables)
```

---

## 6. Direcciones especiales

### Dirección de red y broadcast

En cada subred, la **primera** y la **última** dirección están reservadas:

```
Red 192.168.1.0/24:

  192.168.1.0     ← Dirección de RED (todos los bits de host = 0)
  192.168.1.1     ← Primer host usable
  ...
  192.168.1.254   ← Último host usable
  192.168.1.255   ← Dirección de BROADCAST (todos los bits de host = 1)

  Total: 256 direcciones
  Usables: 254 (256 - 2)
  Fórmula: 2^(32-prefijo) - 2 hosts usables
```

### Loopback

```
127.0.0.0/8 — todo el rango 127.x.x.x

127.0.0.1 es la dirección de loopback estándar.
Tráfico a 127.0.0.1 nunca sale de la máquina — es local.
Se usa para que los procesos se comuniquen entre sí dentro del mismo host.

Nombre DNS asociado: localhost
```

### Link-local

```
169.254.0.0/16 — direcciones APIPA (Automatic Private IP Addressing)

Si un host no obtiene IP por DHCP, se asigna una IP de este rango
automáticamente. Solo funciona dentro de la LAN, no es enrutable.

Si ves 169.254.x.x en tu interfaz → DHCP falló.
```

### Dirección nula

```
0.0.0.0 — significado contextual:

  En routing:    0.0.0.0/0 = ruta por defecto (todas las redes)
  En un socket:  0.0.0.0 = "todas las interfaces" (INADDR_ANY)
  En DHCP:       fuente 0.0.0.0 = "aún no tengo IP"
```

---

## 7. Direcciones privadas vs públicas

### Rangos privados (RFC 1918)

Tres rangos de IPv4 están reservados para uso interno — nunca se enrutan en Internet:

```
┌─────────────────────────────────────────────────────────────┐
│  Rango                   CIDR           Hosts               │
├─────────────────────────────────────────────────────────────┤
│  10.0.0.0 - 10.255.255.255     /8      16,777,214          │
│  172.16.0.0 - 172.31.255.255   /12      1,048,574          │
│  192.168.0.0 - 192.168.255.255 /16         65,534          │
└─────────────────────────────────────────────────────────────┘
```

### Cómo funcionan con Internet

Las IPs privadas no pueden comunicarse directamente con Internet. Necesitan **NAT**
(Network Address Translation) en el router para traducirlas a una IP pública:

```
Red interna (privada)          Router (NAT)           Internet (pública)

[PC: 192.168.1.10] ─┐
                      ├── [Router] ──── [Servidor web]
[PC: 192.168.1.20] ─┘  IP pública:      93.184.216.34
                        203.0.113.5

El router traduce:
  192.168.1.10:54321 ──NAT──► 203.0.113.5:54321 → Internet
  192.168.1.20:8080  ──NAT──► 203.0.113.5:8080  → Internet

Desde Internet, todo el tráfico viene de 203.0.113.5 (la IP pública).
Las IPs privadas son invisibles desde fuera.
```

### Otros rangos especiales

```
100.64.0.0/10      Carrier-Grade NAT (CGN) — NAT del proveedor (ISP)
192.0.2.0/24       TEST-NET-1 — documentación y ejemplos
198.51.100.0/24    TEST-NET-2 — documentación y ejemplos
203.0.113.0/24     TEST-NET-3 — documentación y ejemplos
198.18.0.0/15      Benchmark testing
```

Los rangos TEST-NET se usan en documentación técnica (como estos READMEs) para
ejemplos sin riesgo de conflicto con IPs reales.

---

## 8. IPv4 en Linux

### Ver configuración IP

```bash
# Todas las interfaces
ip addr show

# Interfaz específica
ip addr show enp0s3

# Solo IPv4
ip -4 addr show
```

Salida típica:

```
2: enp0s3: <BROADCAST,MULTICAST,UP,LOWER_UP> mtu 1500
    inet 192.168.1.10/24 brd 192.168.1.255 scope global dynamic enp0s3
         ──────────────      ─────────────         ───────
         IP/máscara           broadcast             asignada por DHCP
       valid_lft 86400sec preferred_lft 86400sec
```

### Asignar IP manualmente

```bash
# Añadir una IP
sudo ip addr add 192.168.1.100/24 dev enp0s3

# Una interfaz puede tener MÚLTIPLES IPs
sudo ip addr add 10.0.0.1/8 dev enp0s3

# Eliminar una IP
sudo ip addr del 10.0.0.1/8 dev enp0s3
```

> **Nota**: los cambios con `ip addr` son temporales — se pierden al reiniciar.
> Para persistencia, usa NetworkManager (`nmcli`) o archivos de configuración.

### Ver tabla de routing

```bash
ip route show
# default via 192.168.1.1 dev enp0s3 proto dhcp metric 100
# 192.168.1.0/24 dev enp0s3 proto kernel scope link src 192.168.1.10
```

Lectura:
- **default via 192.168.1.1**: paquetes sin destino específico → enviar al gateway
- **192.168.1.0/24 dev enp0s3**: paquetes para esta red → enviar directo (sin router)

### Verificar conectividad

```bash
# Ping a otro host
ping -c 3 192.168.1.1

# Ping sin resolución DNS (-n evita DNS inverso)
ping -n -c 3 8.8.8.8
```

---

## 9. Agotamiento de IPv4

### El problema

IPv4 tiene 2^32 = **4,294,967,296** direcciones. Parece mucho, pero:

```
4.3 mil millones de direcciones IPv4
- Reservadas (loopback, privadas, multicast, etc.)
≈ 3.7 mil millones públicas disponibles
- Desperdicio por clases (antes de CIDR)
- Asignación ineficiente en los primeros años de Internet

Resultado: los registros regionales (RIRs) agotaron su pool:
  APNIC (Asia):    2011
  RIPE (Europa):   2012
  LACNIC (LatAm):  2014
  ARIN (NA):       2015
  AFRINIC (África): 2017
```

### Cómo sobrevivimos sin IPs suficientes

```
1. NAT (Network Address Translation):
   Múltiples hosts comparten una IP pública
   → La mayoría de redes domésticas usan esto

2. CIDR:
   Asignación eficiente sin desperdicio de clases
   → Adoptado en 1993

3. Carrier-Grade NAT (CGN):
   El ISP también hace NAT → doble NAT
   → 100.64.0.0/10

4. IPv6:
   Solución definitiva con 2^128 direcciones
   → Adopción lenta pero creciente
```

---

## 10. Errores comunes

### Error 1: Confundir máscara con gateway

```
IP:      192.168.1.10
Máscara: 255.255.255.0   ← Define el TAMAÑO de la red
Gateway: 192.168.1.1     ← Define la SALIDA de la red

La máscara dice "quién está en mi red local".
El gateway dice "a dónde enviar lo que NO está en mi red local".
Son conceptos completamente diferentes.
```

### Error 2: Asignar la dirección de red o broadcast a un host

```bash
# Red 192.168.1.0/24
sudo ip addr add 192.168.1.0/24 dev enp0s3    # ✗ Dirección de RED
sudo ip addr add 192.168.1.255/24 dev enp0s3  # ✗ Dirección de BROADCAST
sudo ip addr add 192.168.1.1/24 dev enp0s3    # ✔ Host válido
```

Linux permitirá asignarlas, pero causarán problemas de comunicación.

### Error 3: Pensar que 192.168.x.x siempre es /24

```
192.168.1.10/24 → red 192.168.1.0, hosts .1-.254
192.168.1.10/16 → red 192.168.0.0, hosts 192.168.0.1-192.168.255.254
192.168.1.10/25 → red 192.168.1.0, hosts .1-.126

La máscara define el tamaño, no el rango de IPs.
No asumas /24 solo porque ves 192.168.
```

### Error 4: Olvidar que NAT cambia las IPs

```
En el tema anterior dijimos que las IPs se preservan de extremo a extremo.
Con NAT eso NO es cierto:

  PC (192.168.1.10) → Router NAT → Internet (203.0.113.5)

  El servidor remoto ve 203.0.113.5, no 192.168.1.10.
  La IP de origen fue reemplazada por el router.

  "Las IPs se preservan extremo a extremo" es cierto sin NAT.
  Con NAT, la IP de origen cambia en el router.
```

### Error 5: No distinguir IP pública de privada al diagnosticar

```bash
# "Mi IP es 192.168.1.10, ¿por qué no puedo acceder desde Internet?"
# Porque 192.168.1.10 es PRIVADA — no existe en Internet.

# Tu IP pública la ves con:
curl -s ifconfig.me
# 203.0.113.5   ← esta es la que ve Internet

# La diferencia importa para:
# - Configurar servidores accesibles desde Internet
# - Port forwarding en el router
# - Diagnóstico de conectividad
```

---

## 11. Cheatsheet

```
╔══════════════════════════════════════════════════════════════════════╗
║                     IPv4 — REFERENCIA RÁPIDA                         ║
╠══════════════════════════════════════════════════════════════════════╣
║                                                                      ║
║  FORMATO                                                             ║
║  32 bits = 4 octetos separados por puntos                            ║
║  Cada octeto: 0-255                                                  ║
║  Ejemplo: 192.168.1.10                                               ║
║                                                                      ║
║  NOTACIÓN CIDR                                                       ║
║  IP/prefijo → 192.168.1.0/24                                        ║
║  Prefijo = bits de red                                               ║
║  32 - prefijo = bits de host                                         ║
║  Hosts usables = 2^(32-prefijo) - 2                                  ║
║                                                                      ║
║  MÁSCARAS COMUNES                                                    ║
║  /8  = 255.0.0.0       16M hosts                                    ║
║  /16 = 255.255.0.0     65K hosts                                    ║
║  /24 = 255.255.255.0   254 hosts                                    ║
║  /25 = 255.255.255.128 126 hosts                                    ║
║  /26 = 255.255.255.192  62 hosts                                    ║
║  /27 = 255.255.255.224  30 hosts                                    ║
║  /28 = 255.255.255.240  14 hosts                                    ║
║  /30 = 255.255.255.252   2 hosts                                    ║
║  /32 = 255.255.255.255   1 host (host route)                        ║
║                                                                      ║
║  RANGOS PRIVADOS (RFC 1918)                                          ║
║  10.0.0.0/8          Clase A privada                                 ║
║  172.16.0.0/12       Clase B privada                                 ║
║  192.168.0.0/16      Clase C privada                                 ║
║                                                                      ║
║  DIRECCIONES ESPECIALES                                              ║
║  0.0.0.0/0           Default route / todas las redes                 ║
║  127.0.0.0/8         Loopback (localhost)                            ║
║  169.254.0.0/16      Link-local (DHCP falló)                         ║
║  255.255.255.255     Broadcast global                                ║
║                                                                      ║
║  COMANDOS LINUX                                                      ║
║  ip addr show                    Ver IPs                             ║
║  ip -4 addr show                 Solo IPv4                           ║
║  ip addr add IP/MASK dev DEV     Asignar IP (temporal)               ║
║  ip addr del IP/MASK dev DEV     Eliminar IP                         ║
║  ip route show                   Ver tabla de rutas                  ║
║  curl ifconfig.me                Ver IP pública                      ║
║                                                                      ║
║  OPERACIÓN AND (para calcular red)                                   ║
║  IP AND Máscara = Dirección de red                                   ║
║                                                                      ║
╚══════════════════════════════════════════════════════════════════════╝
```

---

## 12. Ejercicios

### Ejercicio 1: Conversión binario-decimal

Convierte las siguientes direcciones IP a binario (sin calculadora). Usa la tabla
de posiciones: 128, 64, 32, 16, 8, 4, 2, 1.

1. `10.0.0.1`
2. `172.16.254.1`
3. `192.168.100.50`
4. `255.255.240.0` (esta es una máscara — ¿qué prefijo CIDR es?)

Ahora convierte de binario a decimal:

5. `00001010.11111111.00000000.00000001`
6. `11000000.10101000.00001010.10000000`

**Pregunta de predicción**: para la máscara del punto 4, cuenta los 1s contiguos
desde la izquierda. ¿Cuántos hosts usables permite esa máscara?

### Ejercicio 2: Identificar red, broadcast y rango

Para cada dirección IP con su máscara, calcula:
- La dirección de red
- La dirección de broadcast
- El rango de hosts usables
- El número de hosts usables

1. `192.168.1.130/24`
2. `10.10.10.50/16`
3. `172.16.5.100/20`
4. `192.168.1.200/26`
5. `10.0.0.1/30`

Para el punto 3 (`/20`), muestra el trabajo de AND bit a bit:

```
IP:      172.16.5.100
Máscara: 255.255.???.0   ← ¿qué valor tiene el tercer octeto?

Tercer octeto de la IP en binario:      00000101 (5)
Tercer octeto de la máscara en binario: ????????
AND: ????????
```

**Pregunta de reflexión**: ¿por qué la dirección de red y la de broadcast no son
asignables a hosts? ¿Qué pasaría si un host tuviera la dirección de broadcast?

### Ejercicio 3: Tu configuración real

1. Examina tu configuración de red:
   ```bash
   ip -4 addr show
   ip route show
   ```
2. Identifica:
   - Tu IP y máscara
   - La dirección de red de tu subred
   - Tu gateway
   - La dirección de broadcast
3. Determina si tu IP es pública o privada
4. Si es privada, descubre tu IP pública:
   ```bash
   curl -s ifconfig.me
   ```
5. Calcula cuántos hosts pueden existir en tu red local

**Pregunta de reflexión**: tu IP local es privada (probablemente 192.168.x.x o
10.x.x.x). Si tu vecino tiene la misma IP privada en su red, ¿hay conflicto? ¿Por
qué las direcciones privadas pueden repetirse en redes diferentes pero las públicas
no?
