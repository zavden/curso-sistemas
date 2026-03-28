# NAT con iptables

## Índice

1. [Qué es NAT y por qué existe](#1-qué-es-nat-y-por-qué-existe)
2. [La tabla nat en Netfilter](#2-la-tabla-nat-en-netfilter)
3. [SNAT — Source NAT](#3-snat--source-nat)
4. [MASQUERADE — SNAT dinámico](#4-masquerade--snat-dinámico)
5. [DNAT — Destination NAT](#5-dnat--destination-nat)
6. [Port forwarding completo](#6-port-forwarding-completo)
7. [REDIRECT — redirigir al propio host](#7-redirect--redirigir-al-propio-host)
8. [NAT y connection tracking](#8-nat-y-connection-tracking)
9. [Hairpin NAT (NAT loopback)](#9-hairpin-nat-nat-loopback)
10. [Escenarios completos](#10-escenarios-completos)
11. [Diagnóstico y depuración](#11-diagnóstico-y-depuración)
12. [Errores comunes](#12-errores-comunes)
13. [Cheatsheet](#13-cheatsheet)
14. [Ejercicios](#14-ejercicios)

---

## 1. Qué es NAT y por qué existe

**Network Address Translation** modifica direcciones IP (y opcionalmente puertos) en
los paquetes mientras atraviesan un dispositivo de red. Surgió por una razón pragmática:
el agotamiento del espacio IPv4. Sin NAT, los ~4.300 millones de direcciones IPv4 no
alcanzarían para todos los dispositivos conectados.

```
Red privada                    Router con NAT                Internet
┌──────────────┐              ┌──────────────┐
│ 192.168.1.10 │──────┐       │              │
│ 192.168.1.11 │──────┼──────▶│ eth0 (LAN)   │      ┌──────────────┐
│ 192.168.1.12 │──────┘       │ 192.168.1.1  │      │              │
└──────────────┘              │              │      │  Servidor    │
   Direcciones               │ eth1 (WAN)   │─────▶│  203.0.113.5 │
   RFC 1918                   │ 198.51.100.1 │      │              │
                              └──────────────┘      └──────────────┘

Paquete original:    src=192.168.1.10:45832  dst=203.0.113.5:443
Paquete post-NAT:    src=198.51.100.1:45832  dst=203.0.113.5:443
                     ▲
                     └── El router reescribió la IP origen
```

### Tipos fundamentales de NAT

| Tipo | Qué modifica | Cadena Netfilter | Dirección típica |
|------|-------------|------------------|------------------|
| **SNAT** | IP origen | POSTROUTING | LAN → Internet |
| **DNAT** | IP destino | PREROUTING | Internet → servicio interno |
| **MASQUERADE** | IP origen (dinámica) | POSTROUTING | LAN → Internet (IP dinámica) |
| **REDIRECT** | IP destino → localhost | PREROUTING / OUTPUT | Proxy transparente |

### Requisito: ip_forward

NAT sin reenvío de paquetes no tiene sentido. El kernel debe permitir que los paquetes
fluyan entre interfaces:

```bash
# Verificar estado actual
cat /proc/sys/net/ipv4/ip_forward

# Activar temporalmente
echo 1 > /proc/sys/net/ipv4/ip_forward
# o bien:
sysctl -w net.ipv4.ip_forward=1

# Activar permanentemente
echo "net.ipv4.ip_forward = 1" >> /etc/sysctl.d/99-forwarding.conf
sysctl -p /etc/sysctl.d/99-forwarding.conf
```

**Predicción**: sin `ip_forward=1`, puedes añadir reglas NAT sin error, pero los
paquetes nunca llegarán a la cadena FORWARD y serán descartados silenciosamente.

---

## 2. La tabla nat en Netfilter

La tabla `nat` contiene tres cadenas integradas, cada una actúa en un punto específico
del recorrido del paquete:

```
                        ┌─────────────────────────────────┐
                        │         DECISIÓN DE              │
  Paquete ──▶ PREROUTING ──▶  ENRUTAMIENTO  ──▶ FORWARD ──▶ POSTROUTING ──▶ Sale
  entra       (DNAT)         │                               (SNAT/MASQ)
                             │
                             ▼
                           INPUT              OUTPUT ──▶ POSTROUTING
                        (proceso local)     (DNAT local)   (SNAT/MASQ)
```

### Regla crítica: la primera coincidencia gana

En la tabla `nat`, solo se evalúa **el primer paquete** de cada conexión. Una vez que
Netfilter decide cómo traducir la conexión, almacena la traducción en la tabla conntrack
y aplica la misma transformación a todos los paquetes subsiguientes de esa conexión.

```bash
# Ver las traducciones NAT activas
conntrack -L -n
# o filtrando solo NAT
conntrack -L | grep DNAT
conntrack -L | grep SNAT
```

Esto significa que las reglas NAT no necesitan ser ultra-eficientes en cuanto a orden
(a diferencia de las reglas de filtrado), porque solo se evalúan una vez por conexión.

---

## 3. SNAT — Source NAT

SNAT reescribe la dirección IP de origen de los paquetes salientes. Se usa cuando
el router tiene una **IP pública estática** y necesita enmascarar toda la red interna.

### Sintaxis

```bash
iptables -t nat -A POSTROUTING -s <red_origen> -o <interfaz_salida> \
    -j SNAT --to-source <ip_publica>[:<puerto_inicio>-<puerto_fin>]
```

### Ejemplo básico: una red sale por una IP fija

```bash
# Toda la red 192.168.1.0/24 sale con IP 198.51.100.1
iptables -t nat -A POSTROUTING \
    -s 192.168.1.0/24 \
    -o eth1 \
    -j SNAT --to-source 198.51.100.1
```

**Qué ocurre paquete a paquete**:

```
1. PC 192.168.1.10 envía paquete a 203.0.113.5:443
   src=192.168.1.10:45832  dst=203.0.113.5:443

2. El paquete llega a POSTROUTING (ya pasó FORWARD)
   SNAT reescribe: src=198.51.100.1:45832

3. Conntrack registra: 192.168.1.10:45832 ↔ 198.51.100.1:45832

4. La respuesta llega: dst=198.51.100.1:45832
   Conntrack invierte: dst=192.168.1.10:45832
   → el paquete llega al PC original
```

### SNAT con rango de puertos

Cuando muchos clientes internos comparten una IP pública, el kernel puede quedarse
sin puertos efímeros. Se puede especificar un rango:

```bash
iptables -t nat -A POSTROUTING \
    -s 10.0.0.0/8 \
    -o eth0 \
    -j SNAT --to-source 198.51.100.1:1024-65535
```

### SNAT con múltiples IPs

Si tienes varias IPs públicas, el kernel distribuye las conexiones entre ellas:

```bash
iptables -t nat -A POSTROUTING \
    -s 192.168.1.0/24 \
    -o eth1 \
    -j SNAT --to-source 198.51.100.1-198.51.100.4
```

El kernel usa round-robin para asignar la IP de origen a cada nueva conexión.

### Cuándo usar SNAT

- Tienes una **IP pública estática** (contrato fijo con ISP)
- Quieres especificar exactamente qué IP usar
- Necesitas distribuir entre varias IPs públicas
- Rendimiento ligeramente mejor que MASQUERADE (no consulta la IP de la interfaz
  en cada paquete)

---

## 4. MASQUERADE — SNAT dinámico

MASQUERADE es funcionalmente equivalente a SNAT, pero **determina automáticamente
la IP de origen** consultando la dirección actual de la interfaz de salida. Es el
target ideal cuando la IP pública cambia (DHCP del ISP, PPPoE).

### Sintaxis

```bash
iptables -t nat -A POSTROUTING -s <red_origen> -o <interfaz_salida> -j MASQUERADE
```

### Ejemplo típico: compartir Internet

```bash
# La red interna 192.168.1.0/24 comparte la conexión de eth0
iptables -t nat -A POSTROUTING \
    -s 192.168.1.0/24 \
    -o eth0 \
    -j MASQUERADE
```

### MASQUERADE con restricción de puertos

```bash
iptables -t nat -A POSTROUTING \
    -s 192.168.1.0/24 \
    -o eth0 \
    -j MASQUERADE --to-ports 10000-65535
```

### Diferencia real: SNAT vs MASQUERADE

```
                    SNAT                          MASQUERADE
                    ─────                         ──────────
IP configurada:     Fija en la regla              Consultada de la interfaz
Cuando cambia IP:   Reglas quedan rotas           Se adapta automáticamente
Rendimiento:        Ligeramente mejor             Una consulta extra por conexión
Si la interfaz      Las conexiones NAT            Todas las conexiones NAT
cae y sube:         sobreviven                    se INVALIDAN (flush automático)
Caso de uso:        Servidores, IPs fijas         Hogares, DHCP, PPPoE
```

**Predicción**: si usas MASQUERADE con una IP fija funciona perfectamente, solo pagas
un coste mínimo extra. Si usas SNAT con IP dinámica, cuando la IP cambie, el NAT
dejará de funcionar hasta que actualices la regla manualmente.

### Por qué Docker usa MASQUERADE

Cuando Docker crea la red `bridge` por defecto, genera reglas como:

```bash
# Docker genera automáticamente esto:
iptables -t nat -A POSTROUTING \
    -s 172.17.0.0/16 ! -o docker0 \
    -j MASQUERADE
```

Usa MASQUERADE porque no puede asumir que la IP del host sea estática. El `! -o docker0`
evita aplicar NAT al tráfico entre contenedores.

---

## 5. DNAT — Destination NAT

DNAT reescribe la dirección IP de destino del paquete entrante. Se usa para redirigir
tráfico externo hacia servidores internos — es la base del **port forwarding**.

### Sintaxis

```bash
iptables -t nat -A PREROUTING -i <interfaz_entrada> -p <proto> \
    --dport <puerto_destino> \
    -j DNAT --to-destination <ip_interna>[:<puerto>]
```

### Ejemplo: redirigir HTTP a un servidor interno

```bash
# El tráfico que llega al puerto 80 del router se redirige a 192.168.1.100:80
iptables -t nat -A PREROUTING \
    -i eth1 \
    -p tcp --dport 80 \
    -j DNAT --to-destination 192.168.1.100:80
```

**Flujo del paquete**:

```
1. Cliente externo envía:
   src=203.0.113.50:52341  dst=198.51.100.1:80

2. PREROUTING aplica DNAT:
   src=203.0.113.50:52341  dst=192.168.1.100:80
                                ▲
                                └── Reescrito ANTES del enrutamiento

3. El kernel enruta hacia la LAN (decisión basada en el nuevo destino)

4. El paquete pasa por FORWARD (debe estar permitido)

5. POSTROUTING (no necesita SNAT — la respuesta vuelve por el router)

6. El servidor responde:
   src=192.168.1.100:80  dst=203.0.113.50:52341
   Conntrack invierte: src=198.51.100.1:80 (respuesta parece venir del router)
```

### DNAT con cambio de puerto

Puedes redirigir a un puerto diferente del original:

```bash
# Puerto público 8080 → servidor interno puerto 80
iptables -t nat -A PREROUTING \
    -i eth1 \
    -p tcp --dport 8080 \
    -j DNAT --to-destination 192.168.1.100:80

# Rango de puertos
iptables -t nat -A PREROUTING \
    -i eth1 \
    -p tcp --dport 8000:8999 \
    -j DNAT --to-destination 192.168.1.100:8000-8999
```

### DNAT a múltiples servidores (balanceo rudimentario)

```bash
# Distribución round-robin entre dos backends
iptables -t nat -A PREROUTING \
    -i eth1 \
    -p tcp --dport 80 \
    -j DNAT --to-destination 192.168.1.100-192.168.1.102
```

Esto distribuye conexiones nuevas entre .100, .101 y .102. No es un balanceador
real (no verifica salud de backends, no soporta pesos), pero funciona para casos
simples.

### El requisito olvidado: regla FORWARD

DNAT modifica el destino, pero el paquete aún debe pasar por la cadena FORWARD
de la tabla filter. Si la política de FORWARD es DROP (como debería ser en un
firewall), necesitas una regla explícita:

```bash
# Permitir el tráfico redirigido
iptables -A FORWARD \
    -i eth1 \
    -d 192.168.1.100 \
    -p tcp --dport 80 \
    -m conntrack --ctstate NEW,ESTABLISHED,RELATED \
    -j ACCEPT

# Y permitir las respuestas de vuelta
iptables -A FORWARD \
    -o eth1 \
    -s 192.168.1.100 \
    -p tcp --sport 80 \
    -m conntrack --ctstate ESTABLISHED,RELATED \
    -j ACCEPT
```

**Predicción**: si configuras DNAT pero olvidas la regla FORWARD, `conntrack -L`
mostrará las conexiones como `[UNREPLIED]` — el paquete llega al servidor pero
la respuesta es descartada (o el paquete nunca llega si FORWARD la bloquea primero).

---

## 6. Port forwarding completo

Port forwarding es simplemente DNAT + reglas FORWARD + SNAT/MASQUERADE para la
salida. Veamos un escenario completo paso a paso.

### Escenario: servidor web interno accesible desde Internet

```
Internet                    Router                      LAN
                     eth1: 198.51.100.1          eth0: 192.168.1.1
                            │                           │
  Clientes ─────────────────┤                           ├── 192.168.1.100 (web)
  externos                  │                           ├── 192.168.1.101 (ssh)
                            │                           └── 192.168.1.102 (mail)
```

```bash
# 1. Habilitar forwarding
sysctl -w net.ipv4.ip_forward=1

# 2. SNAT para salida a Internet (IP fija)
iptables -t nat -A POSTROUTING \
    -s 192.168.1.0/24 \
    -o eth1 \
    -j SNAT --to-source 198.51.100.1

# 3. Port forwarding: HTTP y HTTPS al servidor web
iptables -t nat -A PREROUTING -i eth1 -p tcp --dport 80 \
    -j DNAT --to-destination 192.168.1.100:80
iptables -t nat -A PREROUTING -i eth1 -p tcp --dport 443 \
    -j DNAT --to-destination 192.168.1.100:443

# 4. Port forwarding: SSH en puerto no estándar
iptables -t nat -A PREROUTING -i eth1 -p tcp --dport 2222 \
    -j DNAT --to-destination 192.168.1.101:22

# 5. Port forwarding: SMTP al servidor de correo
iptables -t nat -A PREROUTING -i eth1 -p tcp --dport 25 \
    -j DNAT --to-destination 192.168.1.102:25

# 6. Reglas FORWARD para permitir el tráfico redirigido
iptables -A FORWARD -i eth1 -d 192.168.1.100 \
    -p tcp -m multiport --dports 80,443 \
    -m conntrack --ctstate NEW -j ACCEPT

iptables -A FORWARD -i eth1 -d 192.168.1.101 \
    -p tcp --dport 22 \
    -m conntrack --ctstate NEW -j ACCEPT

iptables -A FORWARD -i eth1 -d 192.168.1.102 \
    -p tcp --dport 25 \
    -m conntrack --ctstate NEW -j ACCEPT

# 7. Permitir tráfico de conexiones ya establecidas (bidireccional)
iptables -A FORWARD \
    -m conntrack --ctstate ESTABLISHED,RELATED -j ACCEPT

# 8. Denegar todo lo demás en FORWARD
iptables -P FORWARD DROP
```

### Verificación del port forwarding

```bash
# Desde un cliente externo:
curl -v http://198.51.100.1/
ssh -p 2222 user@198.51.100.1

# En el router, verificar traducciones activas:
conntrack -L -p tcp --dport 80
# Ejemplo de salida:
# tcp  6 117 TIME_WAIT src=203.0.113.50 dst=198.51.100.1 sport=52341 dport=80
#   src=192.168.1.100 dst=203.0.113.50 sport=80 dport=52341 [ASSURED] mark=0
#   ▲ par original                       ▲ par esperado para respuesta
```

---

## 7. REDIRECT — redirigir al propio host

REDIRECT es un caso especial de DNAT donde el destino siempre es la máquina local
(127.0.0.1 o la IP de la interfaz de entrada). Se usa comúnmente para proxies
transparentes.

### Sintaxis

```bash
iptables -t nat -A PREROUTING -i <interfaz> -p <proto> \
    --dport <puerto_original> \
    -j REDIRECT --to-ports <puerto_local>
```

### Ejemplo: proxy HTTP transparente

```bash
# Redirigir todo el tráfico HTTP al proxy Squid en puerto 3128
iptables -t nat -A PREROUTING \
    -i eth0 \
    -p tcp --dport 80 \
    -j REDIRECT --to-ports 3128
```

Los clientes creen que hablan con el servidor destino, pero el paquete nunca sale
de la máquina — Squid lo intercepta, lo procesa y hace la petición real.

### Ejemplo: redirigir DNS a un resolver local

```bash
# Todo el DNS de la LAN se resuelve localmente (útil con Pi-hole)
iptables -t nat -A PREROUTING \
    -i eth0 \
    -p udp --dport 53 \
    -j REDIRECT --to-ports 5353

iptables -t nat -A PREROUTING \
    -i eth0 \
    -p tcp --dport 53 \
    -j REDIRECT --to-ports 5353
```

### REDIRECT en OUTPUT (tráfico local)

Para redirigir tráfico generado por el propio host:

```bash
# Redirigir el puerto 80 local al 8080 (app no-root)
iptables -t nat -A OUTPUT \
    -p tcp --dport 80 \
    -j REDIRECT --to-ports 8080
```

**Nota**: REDIRECT solo funciona en PREROUTING y OUTPUT, nunca en POSTROUTING.

---

## 8. NAT y connection tracking

NAT depende completamente del sistema de connection tracking (conntrack) de Netfilter.
Entender esta relación es fundamental para diagnosticar problemas.

### Cómo conntrack almacena las traducciones

```bash
# Formato de una entrada conntrack con NAT
conntrack -L -n

# Ejemplo de salida con DNAT:
tcp  6 117 TIME_WAIT
    src=203.0.113.50 dst=198.51.100.1 sport=52341 dport=80
    src=192.168.1.100 dst=203.0.113.50 sport=80 dport=52341
    [ASSURED] mark=0 use=1

# Línea 1: paquete original (antes de NAT)
#   El cliente 203.0.113.50 quería llegar a 198.51.100.1:80
#
# Línea 2: respuesta esperada (después de invertir NAT)
#   Se espera respuesta de 192.168.1.100 (el servidor real)
#   → conntrack la reescribirá como si viniera de 198.51.100.1
```

### El problema del conntrack lleno

Si la tabla conntrack se llena, se pierden paquetes **incluyendo los NAT**:

```bash
# Ver capacidad y uso actual
sysctl net.netfilter.nf_conntrack_max
sysctl net.netfilter.nf_conntrack_count

# Ver en dmesg si hay drops
dmesg | grep "nf_conntrack: table full"

# Aumentar si es necesario
sysctl -w net.netfilter.nf_conntrack_max=262144
```

### Limpiar traducciones NAT específicas

A veces necesitas forzar la reconexión después de cambiar reglas NAT:

```bash
# Eliminar todas las entradas conntrack
conntrack -F

# Eliminar solo las de un destino específico
conntrack -D -d 192.168.1.100

# Eliminar solo las de un protocolo/puerto
conntrack -D -p tcp --dport 80
```

**Predicción**: si cambias una regla DNAT pero no limpias conntrack, las conexiones
existentes seguirán usando la traducción antigua. Las nuevas conexiones usarán la
nueva regla.

### Timeouts por protocolo

```bash
# TCP establecido: 5 días por defecto
sysctl net.netfilter.nf_conntrack_tcp_timeout_established

# UDP: 30 segundos (stream: 120s)
sysctl net.netfilter.nf_conntrack_udp_timeout
sysctl net.netfilter.nf_conntrack_udp_timeout_stream

# Para servidores con alto tráfico NAT, reducir:
sysctl -w net.netfilter.nf_conntrack_tcp_timeout_established=3600
sysctl -w net.netfilter.nf_conntrack_tcp_timeout_time_wait=30
```

---

## 9. Hairpin NAT (NAT loopback)

### El problema

Un servidor web está en `192.168.1.100:80`. Has configurado DNAT para que
`198.51.100.1:80` lo alcance desde Internet. Pero, ¿qué pasa cuando un cliente
**interno** (ej. `192.168.1.50`) intenta acceder a `198.51.100.1:80`?

```
                    Sin hairpin NAT:
                    ─────────────────
1. PC 192.168.1.50 envía:
   src=192.168.1.50:39221  dst=198.51.100.1:80

2. Router aplica DNAT (PREROUTING):
   src=192.168.1.50:39221  dst=192.168.1.100:80

3. El paquete llega al servidor 192.168.1.100
   El servidor ve que el origen es LOCAL (192.168.1.50)
   → Responde DIRECTAMENTE a 192.168.1.50 (sin pasar por el router)

4. La respuesta llega con src=192.168.1.100:80
   Pero el cliente esperaba respuesta de 198.51.100.1:80
   → El cliente DESCARTA el paquete (no coincide con la conexión)
```

### La solución: SNAT en la red interna

```bash
# Regla DNAT normal (ya existente)
iptables -t nat -A PREROUTING \
    -i eth1 -p tcp --dport 80 \
    -j DNAT --to-destination 192.168.1.100:80

# Hairpin NAT: cuando el tráfico viene de la LAN hacia el servidor
# interno, también reescribir el origen
iptables -t nat -A POSTROUTING \
    -s 192.168.1.0/24 \
    -d 192.168.1.100 \
    -p tcp --dport 80 \
    -j SNAT --to-source 192.168.1.1
```

```
                    Con hairpin NAT:
                    ─────────────────
1. PC 192.168.1.50 envía:
   src=192.168.1.50:39221  dst=198.51.100.1:80

2. PREROUTING (DNAT):
   src=192.168.1.50:39221  dst=192.168.1.100:80

3. POSTROUTING (SNAT — hairpin):
   src=192.168.1.1:39221   dst=192.168.1.100:80
       ▲
       └── Ahora el origen es el router

4. Servidor responde al router:
   src=192.168.1.100:80  dst=192.168.1.1:39221

5. Conntrack invierte ambas traducciones:
   src=198.51.100.1:80  dst=192.168.1.50:39221
   → El cliente recibe la respuesta esperada ✓
```

### Alternativa: DNS interno (split-horizon)

En lugar de hairpin NAT, muchos administradores prefieren que el DNS interno
resuelva el nombre directamente a la IP interna:

```
# DNS externo (público):
www.example.com → 198.51.100.1

# DNS interno (solo para la LAN):
www.example.com → 192.168.1.100
```

Esto elimina la necesidad de hairpin NAT y es generalmente más limpio.

---

## 10. Escenarios completos

### Escenario 1: router doméstico básico

```
Internet ── ppp0 (IP dinámica) ── Router ── eth0 (192.168.1.1/24) ── LAN
```

```bash
#!/bin/bash
# router_hogar.sh — Firewall/NAT doméstico completo

# Limpiar reglas existentes
iptables -F
iptables -t nat -F
iptables -X

# Políticas por defecto
iptables -P INPUT DROP
iptables -P FORWARD DROP
iptables -P OUTPUT ACCEPT

# Loopback
iptables -A INPUT -i lo -j ACCEPT

# Conexiones establecidas
iptables -A INPUT -m conntrack --ctstate ESTABLISHED,RELATED -j ACCEPT
iptables -A FORWARD -m conntrack --ctstate ESTABLISHED,RELATED -j ACCEPT

# DHCP (el router es servidor DHCP)
iptables -A INPUT -i eth0 -p udp --dport 67 -j ACCEPT

# DNS (el router es resolver)
iptables -A INPUT -i eth0 -p udp --dport 53 -j ACCEPT
iptables -A INPUT -i eth0 -p tcp --dport 53 -j ACCEPT

# Administración SSH desde LAN
iptables -A INPUT -i eth0 -p tcp --dport 22 -j ACCEPT

# NAT: la LAN sale a Internet
iptables -t nat -A POSTROUTING -s 192.168.1.0/24 -o ppp0 -j MASQUERADE

# Forwarding: LAN puede salir
iptables -A FORWARD -i eth0 -o ppp0 -j ACCEPT

# Forwarding activado
sysctl -w net.ipv4.ip_forward=1
```

### Escenario 2: red de oficina con DMZ

```
Internet ── eth0 (198.51.100.1) ── Router ── eth1 (192.168.1.1/24) ── LAN
                                     │
                                     └──── eth2 (10.0.0.1/24) ── DMZ
                                                                  ├── 10.0.0.10 (web)
                                                                  └── 10.0.0.11 (mail)
```

```bash
#!/bin/bash
# router_oficina.sh — Firewall con DMZ

# Limpiar
iptables -F
iptables -t nat -F
iptables -X

# Políticas restrictivas
iptables -P INPUT DROP
iptables -P FORWARD DROP
iptables -P OUTPUT ACCEPT

# Loopback y establecidas
iptables -A INPUT -i lo -j ACCEPT
iptables -A INPUT -m conntrack --ctstate ESTABLISHED,RELATED -j ACCEPT
iptables -A FORWARD -m conntrack --ctstate ESTABLISHED,RELATED -j ACCEPT

# === NAT SALIENTE ===
# LAN sale con IP fija
iptables -t nat -A POSTROUTING -s 192.168.1.0/24 -o eth0 \
    -j SNAT --to-source 198.51.100.1
# DMZ sale con IP fija
iptables -t nat -A POSTROUTING -s 10.0.0.0/24 -o eth0 \
    -j SNAT --to-source 198.51.100.1

# === DNAT (port forwarding a DMZ) ===
# Web
iptables -t nat -A PREROUTING -i eth0 -p tcp \
    -m multiport --dports 80,443 \
    -j DNAT --to-destination 10.0.0.10
# Mail
iptables -t nat -A PREROUTING -i eth0 -p tcp \
    -m multiport --dports 25,143,993 \
    -j DNAT --to-destination 10.0.0.11

# === FORWARD ===
# LAN → Internet: permitido
iptables -A FORWARD -i eth1 -o eth0 -j ACCEPT

# DMZ → Internet: permitido (para actualizaciones, envío de correo)
iptables -A FORWARD -i eth2 -o eth0 -j ACCEPT

# Internet → DMZ: solo puertos publicados
iptables -A FORWARD -i eth0 -d 10.0.0.10 -p tcp \
    -m multiport --dports 80,443 \
    -m conntrack --ctstate NEW -j ACCEPT
iptables -A FORWARD -i eth0 -d 10.0.0.11 -p tcp \
    -m multiport --dports 25,143,993 \
    -m conntrack --ctstate NEW -j ACCEPT

# LAN → DMZ: permitido (administración)
iptables -A FORWARD -i eth1 -o eth2 -j ACCEPT

# DMZ → LAN: DENEGADO (si la DMZ se compromete, no accede a la LAN)
# (implícito por FORWARD DROP, pero lo hacemos explícito)
iptables -A FORWARD -i eth2 -o eth1 -j DROP

# Forwarding activado
sysctl -w net.ipv4.ip_forward=1
```

**Principio de seguridad de la DMZ**: la DMZ puede comunicarse con Internet (para
servir contenido) pero **nunca** con la LAN interna. Si un atacante compromete un
servidor de la DMZ, la red interna sigue protegida.

### Escenario 3: doble NAT (carrier-grade NAT)

```
ISP asigna IP privada ── Router ISP (CGNAT) ── Router local ── LAN
100.64.0.0/10            IP pública real        192.168.1.0/24
(RFC 6598)               compartida
```

En esta situación:
- El DNAT/port forwarding **no funciona** desde Internet porque no controlas
  el router del ISP
- Necesitas alternativas: VPN reversa, Cloudflare Tunnel, o solicitar IP pública
  al ISP

```bash
# Detectar si estás detrás de CGNAT:
# Compara tu IP WAN con tu IP pública
ip addr show ppp0   # ¿Muestra 100.64.x.x o 10.x.x.x?
curl ifconfig.me    # ¿Muestra una IP diferente?
# Si son diferentes → estás detrás de CGNAT
```

---

## 11. Diagnóstico y depuración

### Verificar reglas NAT activas

```bash
# Listar reglas de la tabla nat con contadores
iptables -t nat -L -n -v --line-numbers

# Salida típica:
# Chain PREROUTING (policy ACCEPT 0 packets, 0 bytes)
# num  pkts bytes target     prot opt in  out  source      destination
# 1     847 50820 DNAT       tcp  --  eth1 *   0.0.0.0/0   0.0.0.0/0
#                                                tcp dpt:80 to:192.168.1.100:80
#
# Chain POSTROUTING (policy ACCEPT 0 packets, 0 bytes)
# num  pkts bytes target     prot opt in  out  source         destination
# 1    2341 140K  MASQUERADE all  --  *   eth1  192.168.1.0/24 0.0.0.0/0
```

### Trazar el recorrido de un paquete

```bash
# Activar TRACE para un flujo específico
iptables -t raw -A PREROUTING -p tcp --dport 80 -j TRACE
iptables -t raw -A OUTPUT -p tcp --dport 80 -j TRACE

# Ver los logs del trace
dmesg | grep TRACE
# o en sistemas con rsyslog:
tail -f /var/log/kern.log | grep TRACE

# Ejemplo de salida TRACE:
# TRACE: raw:PREROUTING:policy:2 IN=eth1 ... SRC=203.0.113.50 DST=198.51.100.1
# TRACE: nat:PREROUTING:rule:1 IN=eth1 ... SRC=203.0.113.50 DST=198.51.100.1
# TRACE: filter:FORWARD:rule:3 IN=eth1 ... SRC=203.0.113.50 DST=192.168.1.100
#                                                              ▲ ya con DNAT aplicado

# IMPORTANTE: desactivar TRACE después de depurar
iptables -t raw -F
```

### Verificar conntrack

```bash
# Listar traducciones NAT activas
conntrack -L -n

# Buscar una conexión específica
conntrack -L -p tcp --dst 192.168.1.100 --dport 80

# Contar conexiones NAT
conntrack -C

# Monitorizar en tiempo real
conntrack -E
# conntrack -E muestra eventos: NEW, UPDATE, DESTROY
```

### Verificar que ip_forward está activo

```bash
# Si todo parece bien pero no funciona, verifica PRIMERO esto:
cat /proc/sys/net/ipv4/ip_forward
# Debe ser 1

# También verificar por interfaz (puede estar deshabilitado por interfaz)
cat /proc/sys/net/ipv4/conf/eth0/forwarding
cat /proc/sys/net/ipv4/conf/eth1/forwarding
```

### Capturar paquetes para verificar NAT

```bash
# En la interfaz externa: ver si el SNAT funciona
tcpdump -i eth1 -nn host 198.51.100.1 and port 80

# En la interfaz interna: ver si el DNAT llega
tcpdump -i eth0 -nn host 192.168.1.100 and port 80

# Comparar: el paquete en eth1 debe tener la IP pública,
# el mismo paquete en eth0 debe tener la IP privada
```

---

## 12. Errores comunes

### Error 1: DNAT sin regla FORWARD

```bash
# ✗ Solo DNAT — el paquete llega a FORWARD y es descartado
iptables -t nat -A PREROUTING -i eth1 -p tcp --dport 80 \
    -j DNAT --to-destination 192.168.1.100:80
# ¡La política de FORWARD es DROP!

# ✓ DNAT + FORWARD
iptables -t nat -A PREROUTING -i eth1 -p tcp --dport 80 \
    -j DNAT --to-destination 192.168.1.100:80
iptables -A FORWARD -i eth1 -d 192.168.1.100 -p tcp --dport 80 \
    -m conntrack --ctstate NEW -j ACCEPT
iptables -A FORWARD -m conntrack --ctstate ESTABLISHED,RELATED -j ACCEPT
```

### Error 2: olvidar ip_forward

```bash
# ✗ Reglas NAT perfectas, pero el kernel descarta entre interfaces
iptables -t nat -A POSTROUTING -s 192.168.1.0/24 -o eth1 -j MASQUERADE
# Los paquetes nunca llegan a POSTROUTING porque se descartan antes

# ✓ Activar PRIMERO
sysctl -w net.ipv4.ip_forward=1
```

### Error 3: SNAT con IP que cambia

```bash
# ✗ IP fija en SNAT pero la interfaz es DHCP
iptables -t nat -A POSTROUTING -s 192.168.1.0/24 -o eth0 \
    -j SNAT --to-source 198.51.100.1
# Cuando el ISP asigne otra IP, el NAT dejará de funcionar

# ✓ Usar MASQUERADE para IPs dinámicas
iptables -t nat -A POSTROUTING -s 192.168.1.0/24 -o eth0 \
    -j MASQUERADE
```

### Error 4: no limpiar conntrack al cambiar reglas

```bash
# ✗ Cambiar la regla DNAT sin limpiar
iptables -t nat -D PREROUTING 1
iptables -t nat -A PREROUTING -i eth1 -p tcp --dport 80 \
    -j DNAT --to-destination 192.168.1.200:80   # nuevo servidor
# Las conexiones existentes siguen yendo al servidor antiguo

# ✓ Limpiar conntrack después de cambiar reglas NAT
iptables -t nat -D PREROUTING 1
iptables -t nat -A PREROUTING -i eth1 -p tcp --dport 80 \
    -j DNAT --to-destination 192.168.1.200:80
conntrack -D -p tcp --dport 80
```

### Error 5: MASQUERADE en interfaz interna

```bash
# ✗ MASQUERADE aplicado a la interfaz equivocada
iptables -t nat -A POSTROUTING -s 192.168.1.0/24 -o eth0 -j MASQUERADE
#                                                    ▲
#                                        eth0 es la LAN, no la WAN

# ✓ Asegurarse de que -o apunta a la interfaz de salida (WAN)
iptables -t nat -A POSTROUTING -s 192.168.1.0/24 -o eth1 -j MASQUERADE
#                                                    ▲
#                                        eth1 es la WAN
```

---

## 13. Cheatsheet

```bash
# ╔══════════════════════════════════════════════════════════════════╗
# ║                  NAT CON IPTABLES — CHEATSHEET                 ║
# ╠══════════════════════════════════════════════════════════════════╣
# ║                                                                ║
# ║  PREREQUISITO:                                                 ║
# ║  sysctl -w net.ipv4.ip_forward=1                               ║
# ║                                                                ║
# ║  SNAT (IP estática):                                           ║
# ║  iptables -t nat -A POSTROUTING \                               ║
# ║    -s 192.168.1.0/24 -o eth_wan \                               ║
# ║    -j SNAT --to-source 198.51.100.1                             ║
# ║                                                                ║
# ║  MASQUERADE (IP dinámica):                                     ║
# ║  iptables -t nat -A POSTROUTING \                               ║
# ║    -s 192.168.1.0/24 -o eth_wan -j MASQUERADE                  ║
# ║                                                                ║
# ║  DNAT (port forwarding):                                       ║
# ║  iptables -t nat -A PREROUTING -i eth_wan \                     ║
# ║    -p tcp --dport 80 \                                          ║
# ║    -j DNAT --to-destination 192.168.1.100:80                    ║
# ║  + regla FORWARD correspondiente                               ║
# ║                                                                ║
# ║  REDIRECT (proxy transparente):                                ║
# ║  iptables -t nat -A PREROUTING -i eth_lan \                     ║
# ║    -p tcp --dport 80 -j REDIRECT --to-ports 3128               ║
# ║                                                                ║
# ║  Hairpin NAT:                                                  ║
# ║  iptables -t nat -A POSTROUTING \                               ║
# ║    -s 192.168.1.0/24 -d 192.168.1.100 \                        ║
# ║    -p tcp --dport 80 -j SNAT --to-source 192.168.1.1           ║
# ║                                                                ║
# ║  DIAGNÓSTICO:                                                  ║
# ║  iptables -t nat -L -n -v           # ver reglas               ║
# ║  conntrack -L -n                     # ver traducciones         ║
# ║  conntrack -D -p tcp --dport 80      # limpiar entradas         ║
# ║  conntrack -E                        # monitorizar en vivo      ║
# ║                                                                ║
# ╚══════════════════════════════════════════════════════════════════╝
```

---

## 14. Ejercicios

### Ejercicio 1: NAT saliente básico

**Contexto**: tienes un router Linux con dos interfaces:
- `eth0` — LAN: `10.0.0.1/24`
- `eth1` — WAN: IP asignada por DHCP del ISP

**Tareas**:

1. Activa el reenvío de paquetes de forma persistente
2. Configura NAT para que toda la LAN acceda a Internet
3. Permite que la LAN salga libremente pero bloquea tráfico entrante no solicitado
4. Verifica que el NAT funciona con `conntrack -L`

**Pistas**:
- La IP de eth1 es dinámica → ¿SNAT o MASQUERADE?
- No olvides las reglas FORWARD además del NAT
- La persistencia de ip_forward va en `/etc/sysctl.d/`

> **Pregunta de reflexión**: si tienes 200 dispositivos en la LAN haciendo NAT por
> una sola IP pública, ¿cuántas conexiones TCP simultáneas puede soportar el NAT
> teóricamente? ¿Qué limita primero: los puertos efímeros (~64.000) o la tabla
> conntrack?

---

### Ejercicio 2: port forwarding con DMZ

**Contexto**: misma topología del ejercicio anterior, pero ahora tienes un servidor
en `10.0.0.50` corriendo:
- Nginx en puerto 80 y 443
- SSH en puerto 22

**Tareas**:

1. Publica HTTP/HTTPS directamente (puertos 80 y 443)
2. Publica SSH en un puerto no estándar (puerto externo 2222 → interno 22)
3. Asegúrate de que las reglas FORWARD permitan solo estos servicios
4. Configura hairpin NAT para que los clientes de la LAN puedan acceder al
   servidor por la IP pública
5. Verifica con `conntrack -L` que las traducciones DNAT aparecen correctamente

**Pistas**:
- El hairpin NAT necesita SNAT adicional en POSTROUTING
- Puedes usar `curl` desde otro PC de la LAN para verificar hairpin
- `conntrack -L -d 10.0.0.50` filtra por destino

> **Pregunta de reflexión**: ¿por qué la solución de DNS interno (split-horizon)
> se considera generalmente superior al hairpin NAT? ¿En qué caso hairpin NAT
> es la única opción viable?

---

### Ejercicio 3: auditoría de reglas NAT

**Contexto**: heredas un servidor con estas reglas NAT y debes identificar los
problemas:

```bash
iptables -t nat -A POSTROUTING -j MASQUERADE
iptables -t nat -A PREROUTING -p tcp --dport 3306 \
    -j DNAT --to-destination 10.0.0.20:3306
iptables -t nat -A PREROUTING -p tcp --dport 80 \
    -j DNAT --to-destination 10.0.0.30:80
iptables -P FORWARD ACCEPT
```

**Tareas**:

1. Identifica al menos 4 problemas de seguridad o configuración
2. Reescribe las reglas corrigiendo cada problema
3. Añade las reglas FORWARD mínimas necesarias
4. Añade logging para conexiones nuevas a los servicios publicados

**Pistas**:
- ¿Qué pasa con el MASQUERADE sin `-s` ni `-o`?
- ¿Debería MySQL (3306) estar expuesto a Internet?
- ¿Es seguro `FORWARD ACCEPT`?
- ¿Falta especificar la interfaz de entrada en las reglas DNAT?

> **Pregunta de reflexión**: ¿cómo usarías `iptables -t raw -j TRACE` para verificar
> que tus reglas corregidas funcionan? ¿En qué punto del trace verías la traducción
> DNAT aplicarse?

---

> **Siguiente tema**: T04 — nftables (sucesor de iptables, sintaxis unificada, migración)
