# IPv6: Formato, Tipos de Dirección y Autoconfiguración

## Índice

1. [Por qué IPv6](#1-por-qué-ipv6)
2. [Formato de una dirección IPv6](#2-formato-de-una-dirección-ipv6)
3. [Reglas de abreviación](#3-reglas-de-abreviación)
4. [Prefijos y subnetting en IPv6](#4-prefijos-y-subnetting-en-ipv6)
5. [Tipos de dirección](#5-tipos-de-dirección)
6. [Link-local (fe80::)](#6-link-local-fe80)
7. [Global Unicast Address (GUA)](#7-global-unicast-address-gua)
8. [Autoconfiguración (SLAAC)](#8-autoconfiguración-slaac)
9. [IPv6 en Linux](#9-ipv6-en-linux)
10. [Dual stack: IPv4 e IPv6 simultáneo](#10-dual-stack-ipv4-e-ipv6-simultáneo)
11. [Errores comunes](#11-errores-comunes)
12. [Cheatsheet](#12-cheatsheet)
13. [Ejercicios](#13-ejercicios)

---

## 1. Por qué IPv6

IPv4 tiene 2^32 = ~4.3 mil millones de direcciones. IPv6 tiene 2^128 direcciones:

```
IPv4:  4,294,967,296 direcciones
IPv6:  340,282,366,920,938,463,463,374,607,431,768,211,456 direcciones
       (340 sextillones)

Para ponerlo en perspectiva:
  IPv6 puede asignar ~6.7 × 10^23 direcciones por cada metro cuadrado
  de la superficie terrestre.
```

Pero IPv6 no es solo "más direcciones". También simplifica aspectos del protocolo:

```
IPv4                              IPv6
────────────────────              ────────────────────
Header variable (20-60 bytes)     Header fijo (40 bytes)
Checksum en header                Sin checksum (lo hace la capa inferior)
Fragmentación por routers         Solo el emisor fragmenta
NAT necesario                     NAT innecesario (IPs suficientes)
Broadcast                         No existe (multicast lo reemplaza)
ARP (broadcast)                   NDP (multicast, más eficiente)
DHCP obligatorio o manual         Autoconfiguración (SLAAC) nativa
```

---

## 2. Formato de una dirección IPv6

### Representación

128 bits divididos en **8 grupos de 16 bits** (4 dígitos hex), separados por `:`

```
Completa:  2001:0db8:0000:0000:0000:0000:0000:0001

  2001 : 0db8 : 0000 : 0000 : 0000 : 0000 : 0000 : 0001
  ────   ────   ────   ────   ────   ────   ────   ────
  16b    16b    16b    16b    16b    16b    16b    16b
  └────────────────── 128 bits total ──────────────────┘
```

### Comparación visual con IPv4

```
IPv4:  192.168.1.10                          (32 bits, decimal)
IPv6:  2001:0db8:85a3:0000:0000:8a2e:0370:7334  (128 bits, hexadecimal)

IPv4 es fácil de memorizar.
IPv6 no está diseñado para memorizarse — se gestiona con DNS y SLAAC.
```

---

## 3. Reglas de abreviación

Dado que las direcciones IPv6 son largas, hay dos reglas para acortarlas:

### Regla 1: Eliminar ceros iniciales en cada grupo

```
2001:0db8:0000:0000:0000:0000:0000:0001
     ────  ───  ───  ───  ───  ───  ───
2001: db8:   0:   0:   0:   0:   0:   1

Cada grupo: 0db8 → db8, 0000 → 0, 0001 → 1
```

### Regla 2: Reemplazar una secuencia contigua de grupos all-zero con ::

```
2001:db8:0:0:0:0:0:1
         ─────────
2001:db8::1

Los seis grupos de ceros se comprimen a ::
```

### Restricción: `::` solo puede aparecer UNA vez

```
2001:0000:0000:0000:0000:0000:0000:0001
  ✔ 2001::1                              (todos los ceros en una secuencia)

2001:0000:0000:1234:0000:0000:0000:0001
  ✔ 2001::1234:0:0:0:1                   (:: en la primera secuencia)
  ✔ 2001:0:0:1234::1                     (:: en la segunda secuencia)
  ✗ 2001::1234::1                        (dos :: → ambiguo)
```

Si hay dos secuencias de ceros, `::` reemplaza la más larga. Si son iguales, se
reemplaza la primera (por convención).

### Ejemplos de abreviación

```
Completa                                  Abreviada
──────────────────────────────────────    ──────────────
2001:0db8:0000:0000:0000:0000:0000:0001  2001:db8::1
fe80:0000:0000:0000:0000:0000:0000:0001  fe80::1
0000:0000:0000:0000:0000:0000:0000:0001  ::1 (loopback)
0000:0000:0000:0000:0000:0000:0000:0000  :: (all zeros)
2001:0db8:85a3:0000:0000:8a2e:0370:7334  2001:db8:85a3::8a2e:370:7334
ff02:0000:0000:0000:0000:0000:0000:0001  ff02::1
```

### Expandir una dirección abreviada

Para expandir `2001:db8::1`, cuenta los grupos presentes y completa con ceros:

```
2001:db8::1
  Grupos presentes: 2001, db8, 1 → 3 grupos
  Grupos totales: 8
  Ceros a insertar: 8 - 3 = 5 grupos

Expandido: 2001:0db8:0000:0000:0000:0000:0000:0001
```

---

## 4. Prefijos y subnetting en IPv6

### Notación

Igual que CIDR en IPv4: dirección/longitud_de_prefijo

```
2001:db8::/32        ← prefijo asignado por el RIR a un ISP
2001:db8:abcd::/48   ← prefijo asignado por el ISP a un cliente
2001:db8:abcd:1::/64 ← subred específica dentro del sitio
```

### Estructura estándar de una dirección GUA

```
│◄──── 48 bits ────►│◄ 16 ►│◄──────── 64 bits ────────►│
┌────────────────────┬──────┬───────────────────────────┐
│ Global Routing     │Subnet│     Interface ID           │
│ Prefix             │  ID  │     (identifica el host)   │
└────────────────────┴──────┴───────────────────────────┘
│◄─── Asignado por ──►│◄ tú ►│◄── generado auto ────────►│
│     ISP/RIR          │      │    (EUI-64 o aleatorio)   │

Prefijo /48: lo que te da tu ISP
Subnet ID: 16 bits para crear 65,536 subredes
Interface ID: 64 bits para identificar hosts (siempre /64 en LANs)
```

### /64 es la norma para subredes LAN

A diferencia de IPv4, donde usas /24, /26, /28 según necesidad, en IPv6 la subred
estándar es **siempre /64**. Esto no es desperdicio — es un requisito para que
SLAAC funcione.

```
IPv4: ajustas la máscara al número de hosts
      /24 para 254, /26 para 62, /30 para 2

IPv6: siempre /64 para LANs
      2^64 = 18,446,744,073,709,551,616 hosts por subred
      No tiene sentido optimizar — hay direcciones de sobra
```

### Prefijos especiales

```
/32   Asignación típica a un ISP (por el RIR)
/48   Asignación típica a un cliente (por el ISP)
/56   Asignación residencial (algunos ISPs)
/64   Subred individual (la que configuras en una LAN)
/128  Host route (una sola dirección, como /32 en IPv4)
```

---

## 5. Tipos de dirección

IPv6 no tiene broadcast. En su lugar, usa tres tipos de comunicación:

```
┌─────────────────────────────────────────────────────────────┐
│                    Tipos de dirección IPv6                    │
├──────────────┬──────────────────────────────────────────────┤
│  Unicast      │  Un emisor → un receptor                    │
│               │  (como IPv4 unicast)                        │
├──────────────┼──────────────────────────────────────────────┤
│  Multicast    │  Un emisor → grupo de receptores            │
│               │  (reemplaza broadcast de IPv4)              │
├──────────────┼──────────────────────────────────────────────┤
│  Anycast      │  Un emisor → el receptor MÁS CERCANO       │
│               │  del grupo (routing decide cuál)            │
└──────────────┴──────────────────────────────────────────────┘

No existe broadcast en IPv6.
Lo que en IPv4 era broadcast (ff:ff:ff:ff:ff:ff / 255.255.255.255),
en IPv6 se hace con multicast a grupos específicos.
```

### Rangos de direcciones

```
Rango                  Tipo                 Equivalente IPv4
──────────────────     ────────────────     ──────────────────
::1/128                Loopback             127.0.0.1
::/128                 Dirección no espec.  0.0.0.0
fe80::/10              Link-local           169.254.0.0/16 (APIPA)
fc00::/7 (fd00::/8)    Unique Local (ULA)   10.0.0.0/8, 172.16.0.0/12,
                                            192.168.0.0/16
2000::/3               Global Unicast       IPs públicas
ff00::/8               Multicast            224.0.0.0/4
::ffff:0:0/96          IPv4-mapped          (para dual-stack)
```

---

## 6. Link-local (fe80::)

### Qué es

Toda interfaz IPv6 activa tiene **automáticamente** una dirección link-local. No
necesita configuración manual ni DHCP. Se genera al activar la interfaz.

```
fe80::xxxx:xxxx:xxxx:xxxx%enp0s3
─────                    ────────
Prefijo                  Zone ID (interfaz)
link-local
(/10 pero en práctica /64)
```

### Características

- **Siempre empieza con `fe80::`**
- **No es enrutable**: solo funciona dentro del mismo segmento de red (enlace)
- **Siempre existe**: incluso si no tienes IP global, tienes link-local
- **Necesita zone ID** (`%enp0s3`) para distinguir interfaces si tienes varias

### Para qué sirve

```
1. Comunicación entre vecinos de red
   → NDP (Neighbor Discovery Protocol) usa link-local
   → Equivale a lo que ARP hace en IPv4

2. Routing
   → Los routers anuncian rutas usando su dirección link-local
   → La ruta por defecto en IPv6 apunta a una link-local del router

3. Funcionamiento sin infraestructura
   → Dos máquinas conectadas directamente pueden comunicarse
     con link-local sin ningún servidor DHCP ni configuración
```

### Generación de la dirección link-local

```
Método 1: EUI-64 (basado en MAC)

  MAC: 08:00:27:a1:b2:c3

  1. Insertar ff:fe en el medio:   08:00:27:ff:fe:a1:b2:c3
  2. Invertir bit 7 del 1er byte:  0a:00:27:ff:fe:a1:b2:c3
                                    ──
                                    08 = 00001000
                                    bit7 invertido → 00001010 = 0a

  Link-local: fe80::a00:27ff:fea1:b2c3

Método 2: Aleatorio (privacy extensions, default en la mayoría de distros)

  Link-local: fe80::random:random:random:random
  → No expone la MAC del dispositivo
```

---

## 7. Global Unicast Address (GUA)

### Qué es

La GUA es la equivalente IPv6 de una IP pública en IPv4. Es globalmente enrutable
— funciona en Internet.

```
Rango: 2000::/3 (empieza con 2 o 3 en el primer dígito hex)
       En la práctica actual: 2xxx:xxxx:...

Ejemplo: 2001:db8:85a3::8a2e:370:7334
```

### Cómo se obtiene

```
IANA asigna bloques a RIRs (registros regionales):
  RIPE (Europa), ARIN (Norteamérica), LACNIC (Latam), etc.
       │
       ▼
RIR asigna /32 a ISPs
       │
       ▼
ISP asigna /48 o /56 a clientes
       │
       ▼
Cliente crea subredes /64 internamente
       │
       ▼
Hosts generan su Interface ID (64 bits) automáticamente
```

### Diferencia fundamental con IPv4

```
IPv4: tu ISP te da UNA IP pública → NAT para compartirla
IPv6: tu ISP te da un PREFIJO (/48 o /56) → miles de IPs públicas
      Cada dispositivo tiene su propia IP global
      NAT es innecesario
```

---

## 8. Autoconfiguración (SLAAC)

### Qué es

SLAAC (Stateless Address Autoconfiguration) permite que un host se configure
automáticamente su dirección IPv6 global **sin DHCP**. Es una de las características
más importantes de IPv6.

### Cómo funciona

```
1. El host activa la interfaz
   → Se genera automáticamente una dirección link-local

2. El host envía un Router Solicitation (RS)
   → Multicast a ff02::2 (todos los routers en el enlace)
   → "¿Hay algún router aquí?"

3. El router responde con un Router Advertisement (RA)
   → Contiene: prefijo de red (/64), lifetime, flags
   → "El prefijo de esta red es 2001:db8:1::/64"

4. El host combina el prefijo con su Interface ID
   → Prefijo (64 bits) + Interface ID (64 bits) = dirección GUA completa
   → 2001:db8:1::a00:27ff:fea1:b2c3/64

5. El host hace DAD (Duplicate Address Detection)
   → Verifica que nadie más usa esa dirección
   → Si está duplicada, no la usa (muy improbable con 2^64 posibilidades)
```

```
                Router                          Host
                  │                               │
                  │    ◄── RS (ff02::2) ──────── │ "¿Hay router?"
                  │                               │
                  │ ── RA (ff02::1) ──────────►  │ "Prefijo: 2001:db8:1::/64"
                  │                               │
                  │                               │ Host genera:
                  │                               │ 2001:db8:1:: + Interface ID
                  │                               │
                  │    ◄── NS (DAD) ──────────── │ "¿Alguien usa esta IP?"
                  │                               │
                  │                               │ (silencio = OK, la usa)
```

### SLAAC vs DHCPv6

```
SLAAC (stateless):
  ✔ Automático, sin servidor
  ✔ Configura IP y gateway
  ✗ No da DNS por sí solo (necesita RDNSS option o DHCPv6)
  → Usado en la mayoría de redes

DHCPv6 (stateful):
  ✔ Control centralizado de asignaciones
  ✔ Da DNS, dominio, y opciones adicionales
  ✗ Necesita servidor DHCPv6
  → Usado en redes enterprise

Combinación habitual:
  SLAAC para IP + gateway
  DHCPv6 stateless solo para DNS y opciones extra
  (flag M=0, O=1 en el Router Advertisement)
```

---

## 9. IPv6 en Linux

### Ver direcciones IPv6

```bash
ip -6 addr show
```

Salida típica:

```
1: lo: <LOOPBACK,UP,LOWER_UP>
    inet6 ::1/128 scope host
2: enp0s3: <BROADCAST,MULTICAST,UP,LOWER_UP>
    inet6 2001:db8:1::a00:27ff:fea1:b2c3/64 scope global dynamic
    inet6 fe80::a00:27ff:fea1:b2c3/64 scope link
```

Campos:
- **scope global**: dirección GUA (enrutable en Internet)
- **scope link**: dirección link-local (solo enlace local)
- **scope host**: loopback (solo esta máquina)
- **dynamic**: asignada por SLAAC o DHCPv6

### Asignar IPv6 manualmente

```bash
# Añadir dirección
sudo ip -6 addr add 2001:db8:1::100/64 dev enp0s3

# Eliminar
sudo ip -6 addr del 2001:db8:1::100/64 dev enp0s3
```

### Routing IPv6

```bash
# Ver rutas
ip -6 route show
# 2001:db8:1::/64 dev enp0s3 proto kernel metric 256
# fe80::/64 dev enp0s3 proto kernel metric 256
# default via fe80::1 dev enp0s3 proto ra metric 1024
#             ───────
#             Gateway es una dirección LINK-LOCAL del router

# Añadir ruta
sudo ip -6 route add 2001:db8:2::/64 via fe80::1 dev enp0s3
```

> **Nota**: en IPv6, el gateway por defecto es la dirección **link-local** del router,
> no una dirección global. Esto es diferente de IPv4.

### Ping IPv6

```bash
# Ping a loopback
ping -6 ::1

# Ping a una dirección global
ping -6 2001:db8:1::1

# Ping a link-local (requiere %interfaz)
ping -6 fe80::1%enp0s3
```

### Neighbor Discovery (equivalente a ARP)

```bash
# Ver vecinos IPv6 (equivalente a ip neigh show para IPv4)
ip -6 neigh show
# fe80::1 dev enp0s3 lladdr aa:bb:cc:11:22:33 router REACHABLE
# 2001:db8:1::5 dev enp0s3 lladdr dd:ee:ff:44:55:66 STALE
```

### Deshabilitar IPv6 (si es necesario)

```bash
# Temporal
sudo sysctl -w net.ipv6.conf.all.disable_ipv6=1

# Permanente
echo "net.ipv6.conf.all.disable_ipv6 = 1" | sudo tee /etc/sysctl.d/99-disable-ipv6.conf
sudo sysctl --system
```

> Deshabilitar IPv6 puede romper cosas que lo esperan (systemd, Docker). Solo hazlo
> si tienes un motivo concreto.

### Privacy extensions

Por defecto en la mayoría de distribuciones modernas, la parte de Interface ID se
genera aleatoriamente en lugar de usar EUI-64 (que revela la MAC):

```bash
# Ver si están activas
sysctl net.ipv6.conf.enp0s3.use_tempaddr
# 2 = prefer temporary (privacy) addresses

# Resultado: verás varias direcciones en la interfaz
ip -6 addr show enp0s3
# inet6 2001:db8:1::a00:27ff:fea1:b2c3/64 scope global        ← EUI-64 (estable)
# inet6 2001:db8:1::7d3f:8c12:abcd:ef01/64 scope global temporary  ← Privacy
```

La dirección temporary es la que se usa para conexiones salientes. La estable se
mantiene para que otros hosts puedan encontrarte (servidores).

---

## 10. Dual stack: IPv4 e IPv6 simultáneo

### Cómo funciona

La mayoría de redes actuales usan dual stack: IPv4 e IPv6 funcionan en paralelo
en la misma interfaz.

```
enp0s3:
  IPv4:  192.168.1.10/24                     ← para servicios IPv4
  IPv6:  2001:db8:1::a00:27ff:fea1:b2c3/64  ← para servicios IPv6
  IPv6:  fe80::a00:27ff:fea1:b2c3/64        ← link-local (siempre)
```

### Happy Eyeballs (RFC 8305)

Cuando un nombre DNS tiene tanto registro A (IPv4) como AAAA (IPv6), los navegadores
y aplicaciones modernas implementan "Happy Eyeballs": intentan IPv6 primero pero
si tarda más de ~250ms, prueban IPv4 en paralelo y usan lo que conecte más rápido.

```
curl example.com
  1. DNS devuelve: A = 93.184.216.34, AAAA = 2606:2800:220:1::
  2. curl intenta IPv6 (2606:2800:220:1::)
  3. Si IPv6 tarda > 250ms, también intenta IPv4 (93.184.216.34)
  4. Usa la primera conexión que se establezca
```

### Forzar un protocolo específico

```bash
# Forzar IPv4
curl -4 http://example.com
ping -4 example.com
ssh -4 user@host

# Forzar IPv6
curl -6 http://example.com
ping -6 example.com
ssh -6 user@host
```

---

## 11. Errores comunes

### Error 1: Usar :: dos veces en una dirección

```
✗ 2001:db8::1::5
  Ambiguo: ¿cuántos grupos de ceros representa cada ::?

✔ 2001:db8::1:0:0:5
  O: 2001:db8:0:0:1::5
```

`::` solo puede aparecer una vez. Si hay dos secuencias de ceros, la más larga usa
`::` y la otra se escribe con ceros explícitos.

### Error 2: Olvidar el zone ID con link-local

```bash
# FALLA:
ping -6 fe80::1
# connect: Invalid argument

# CORRECTO:
ping -6 fe80::1%enp0s3
#              ────────
#              Zone ID: qué interfaz usar
```

Como link-local es la misma en todas las interfaces (`fe80::/10`), el sistema no sabe
por cuál enviar el paquete sin el `%interfaz`.

### Error 3: Intentar usar subredes menores a /64 para LANs

```
✗ "Solo necesito 10 hosts, uso /120"
  SLAAC no funciona con prefijos menores a /64
  NDP puede tener problemas
  No hay beneficio: IPv6 tiene direcciones de sobra

✔ Siempre /64 para subredes LAN
  /126 o /127 solo para enlaces punto a punto (router-router)
```

### Error 4: Pensar que IPv6 elimina la necesidad de firewalls

```
IPv6 elimina la necesidad de NAT.
NAT NO es un firewall (aunque provee cierto aislamiento accidental).

Con IPv6, cada dispositivo tiene una IP pública accesible globalmente.
→ Un firewall es MÁS importante que con IPv4/NAT, no menos.
```

### Error 5: Deshabilitar IPv6 "porque no lo uso"

```
Problemas de deshabilitar IPv6:
  - systemd asume que IPv6 está disponible en muchas configuraciones
  - Docker puede fallar o comportarse diferente
  - Algunos servicios (como localhost resolution) pueden romperse
  - Pierdes la posibilidad de comunicarte con hosts solo-IPv6

Solo deshabilita IPv6 si tienes un motivo técnico concreto,
no "por si acaso" o "para simplificar".
```

---

## 12. Cheatsheet

```
╔══════════════════════════════════════════════════════════════════════╗
║                      IPv6 — REFERENCIA RÁPIDA                        ║
╠══════════════════════════════════════════════════════════════════════╣
║                                                                      ║
║  FORMATO                                                             ║
║  128 bits = 8 grupos de 4 hex separados por :                        ║
║  Abreviar: quitar 0s iniciales, reemplazar grupo(s) 0 con ::         ║
║  :: solo puede aparecer UNA VEZ                                      ║
║                                                                      ║
║  TIPOS DE DIRECCIÓN                                                  ║
║  ::1              Loopback                                           ║
║  fe80::/10        Link-local (auto, no enrutable)                    ║
║  fd00::/8         Unique Local Address (como privada IPv4)           ║
║  2000::/3         Global Unicast (como pública IPv4)                 ║
║  ff00::/8         Multicast (reemplaza broadcast)                    ║
║  ff02::1          Multicast todos los nodos del enlace               ║
║  ff02::2          Multicast todos los routers del enlace             ║
║                                                                      ║
║  PREFIJOS COMUNES                                                    ║
║  /32  Asignación a ISP                                               ║
║  /48  Asignación a cliente                                           ║
║  /56  Asignación residencial                                         ║
║  /64  Subred LAN (SIEMPRE /64 para LANs)                            ║
║  /128 Host route                                                     ║
║                                                                      ║
║  AUTOCONFIGURACIÓN (SLAAC)                                           ║
║  1. Interfaz UP → link-local auto                                    ║
║  2. RS a ff02::2 → "¿hay router?"                                   ║
║  3. RA con prefijo → host genera GUA                                 ║
║  4. DAD → verificar unicidad                                         ║
║                                                                      ║
║  COMANDOS LINUX                                                      ║
║  ip -6 addr show                  Ver direcciones IPv6               ║
║  ip -6 addr add IP/PFX dev DEV   Añadir IPv6                        ║
║  ip -6 route show                 Ver rutas IPv6                     ║
║  ip -6 neigh show                 Ver vecinos (NDP)                  ║
║  ping -6 IP                       Ping IPv6                          ║
║  ping -6 fe80::1%enp0s3          Ping link-local (con zone ID)      ║
║  curl -6 http://[IPv6]:port      curl con IPv6                      ║
║  ss -6 -tlnp                     Sockets IPv6 escuchando            ║
║                                                                      ║
║  IPv6 EN URLs                                                        ║
║  http://[2001:db8::1]:8080/path  Corchetes alrededor de la IP       ║
║  ssh user@2001:db8::1            Sin corchetes (no es URL)           ║
║                                                                      ║
║  DUAL STACK                                                          ║
║  curl -4 / curl -6               Forzar IPv4 o IPv6                 ║
║  Happy Eyeballs: intenta IPv6, fallback a IPv4 si tarda             ║
║                                                                      ║
╚══════════════════════════════════════════════════════════════════════╝
```

---

## 13. Ejercicios

### Ejercicio 1: Abreviación y expansión

Abrevia las siguientes direcciones IPv6 al máximo:

1. `2001:0db8:0000:0000:0000:0000:0000:0001`
2. `fe80:0000:0000:0000:0a00:27ff:fea1:b2c3`
3. `2001:0db8:0000:0042:0000:0000:0000:0000`
4. `0000:0000:0000:0000:0000:0000:0000:0001`
5. `ff02:0000:0000:0000:0000:0000:0000:0002`

Ahora expande estas a su forma completa de 8 grupos:

6. `2001:db8::1`
7. `fe80::1`
8. `::ffff:192.168.1.1` (IPv4-mapped — ¿cuántos grupos de ceros?)
9. `2001:db8:abcd::ef01:2345`

**Pregunta de predicción**: la dirección `::` (all zeros abreviada) ¿cuántos bits
tiene? ¿Qué significa en el contexto de una dirección de origen vs una ruta?

### Ejercicio 2: Explorar IPv6 en tu máquina

1. Lista todas tus direcciones IPv6:
   ```bash
   ip -6 addr show
   ```
2. Identifica:
   - ¿Tienes dirección link-local? (debe empezar con `fe80::`)
   - ¿Tienes dirección global? (empieza con `2` o `3`)
   - Si tienes global, ¿fue asignada por SLAAC (flag `dynamic`)?
   - ¿Ves direcciones con `temporary`? (privacy extensions)

3. Haz ping al loopback IPv6:
   ```bash
   ping -6 -c 3 ::1
   ```

4. Haz ping a tu link-local (necesitas el zone ID):
   ```bash
   ping -6 -c 3 fe80::TU_SUFIJO%enp0s3
   ```

5. Revisa la tabla de vecinos IPv6:
   ```bash
   ip -6 neigh show
   ```

6. ¿Tienes gateway IPv6?
   ```bash
   ip -6 route show default
   ```

**Pregunta de reflexión**: ¿por qué la ruta por defecto IPv6 apunta a una dirección
link-local del router (`fe80::...`) en vez de a su dirección global? ¿Qué ventaja
tiene esto?

### Ejercicio 3: Dual stack en acción

1. Verifica que tienes tanto IPv4 como IPv6:
   ```bash
   ip addr show enp0s3 | grep inet
   ```
2. Intenta acceder a un sitio con ambos protocolos:
   ```bash
   # Forzar IPv4
   curl -4 -sI http://example.com | head -3

   # Forzar IPv6
   curl -6 -sI http://example.com | head -3
   ```
3. Si ambos funcionan, verifica cuál usa por defecto:
   ```bash
   curl -v http://example.com 2>&1 | grep "Trying"
   # Verás si intenta IPv6 o IPv4 primero
   ```
4. Consulta los registros DNS de un dominio:
   ```bash
   dig example.com A      # IPv4
   dig example.com AAAA   # IPv6
   ```

**Pregunta de reflexión**: si un sitio web tiene tanto registro A como AAAA, y tu
conexión IPv6 es lenta pero funciona, ¿qué pasará con tu experiencia de navegación?
¿Cómo soluciona esto el algoritmo Happy Eyeballs?
