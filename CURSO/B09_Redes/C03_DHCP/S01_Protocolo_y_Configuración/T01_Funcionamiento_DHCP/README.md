# Funcionamiento DHCP

## Índice
1. [Qué es DHCP](#qué-es-dhcp)
2. [El problema que resuelve](#el-problema-que-resuelve)
3. [DORA: las cuatro fases](#dora-las-cuatro-fases)
4. [Anatomía de los mensajes DHCP](#anatomía-de-los-mensajes-dhcp)
5. [El servidor DHCP](#el-servidor-dhcp)
6. [DHCP relay](#dhcp-relay)
7. [Opciones DHCP](#opciones-dhcp)
8. [DHCPv6 vs SLAAC](#dhcpv6-vs-slaac)
9. [Seguridad DHCP](#seguridad-dhcp)
10. [Errores comunes](#errores-comunes)
11. [Cheatsheet](#cheatsheet)
12. [Ejercicios](#ejercicios)

---

## Qué es DHCP

DHCP (Dynamic Host Configuration Protocol, RFC 2131) asigna automáticamente configuración de red a los dispositivos que se conectan: IP, máscara, gateway, servidores DNS, y más. Opera sobre UDP usando los puertos **67** (servidor) y **68** (cliente):

```
┌──────────────────────────────────────────────────────────────┐
│                    ¿Qué asigna DHCP?                         │
│                                                              │
│  Mínimo (siempre):                                           │
│    • Dirección IP                                            │
│    • Máscara de subred                                       │
│    • Tiempo de lease (duración de la asignación)             │
│                                                              │
│  Típicamente también:                                        │
│    • Default gateway (puerta de enlace)                      │
│    • Servidores DNS                                          │
│    • Nombre de dominio (search domain)                       │
│                                                              │
│  Opcionalmente:                                              │
│    • Servidores NTP                                          │
│    • Servidor WINS                                           │
│    • Rutas estáticas adicionales                             │
│    • Nombre de host                                          │
│    • Servidores TFTP (para PXE boot)                         │
│    • MTU de la interfaz                                      │
└──────────────────────────────────────────────────────────────┘
```

---

## El problema que resuelve

Sin DHCP, cada dispositivo necesitaría configuración manual:

```
┌──────────────────────────────────────────────────────────────┐
│  Sin DHCP (configuración estática):                          │
│                                                              │
│  Administrador configura CADA máquina manualmente:           │
│    Laptop Ana:   IP 192.168.1.10, GW 192.168.1.1, DNS ...   │
│    PC Carlos:    IP 192.168.1.11, GW 192.168.1.1, DNS ...   │
│    Móvil Pedro:  IP 192.168.1.12, GW 192.168.1.1, DNS ...   │
│    Impresora:    IP 192.168.1.13, GW 192.168.1.1, DNS ...   │
│    ...                                                       │
│                                                              │
│  Problemas:                                                  │
│  • No escala (100+ dispositivos = inviable)                  │
│  • Errores humanos (IPs duplicadas, gateways incorrectos)    │
│  • Dispositivos visitantes (¿qué IP darles?)                 │
│  • Cambios de red requieren reconfigurar TODO                │
│                                                              │
│  Con DHCP:                                                   │
│                                                              │
│  Administrador configura UN servidor DHCP:                   │
│    "Rango: 192.168.1.10 - 192.168.1.200"                    │
│    "Gateway: 192.168.1.1"                                    │
│    "DNS: 8.8.8.8, 1.1.1.1"                                  │
│                                                              │
│  Cada dispositivo que se conecta recibe configuración        │
│  automáticamente. Cero intervención manual.                  │
└──────────────────────────────────────────────────────────────┘
```

---

## DORA: las cuatro fases

El proceso DHCP tiene cuatro pasos conocidos como **DORA**: Discover, Offer, Request, Acknowledge.

```
┌──────────────────────────────────────────────────────────────┐
│                   Proceso DORA                               │
│                                                              │
│  Cliente                                     Servidor DHCP   │
│  (sin IP)                                    (192.168.1.1)   │
│     │                                             │          │
│     │  1. DHCPDISCOVER (broadcast)                │          │
│     │  src: 0.0.0.0:68 → dst: 255.255.255.255:67 │          │
│     │─────────────────────────────────────────────►│          │
│     │  "¡Necesito una IP! ¿Hay alguien?"          │          │
│     │                                             │          │
│     │  2. DHCPOFFER (unicast o broadcast)         │          │
│     │  src: 192.168.1.1:67 → dst: 255.255.255.255:68        │
│     │◄─────────────────────────────────────────────│          │
│     │  "Te ofrezco 192.168.1.50, lease 24h"       │          │
│     │                                             │          │
│     │  3. DHCPREQUEST (broadcast)                 │          │
│     │  src: 0.0.0.0:68 → dst: 255.255.255.255:67 │          │
│     │─────────────────────────────────────────────►│          │
│     │  "Acepto 192.168.1.50 del servidor .1"      │          │
│     │                                             │          │
│     │  4. DHCPACK (unicast o broadcast)           │          │
│     │  src: 192.168.1.1:67 → dst: 255.255.255.255:68        │
│     │◄─────────────────────────────────────────────│          │
│     │  "Confirmado. IP 192.168.1.50, lease 24h,   │          │
│     │   GW .1, DNS 8.8.8.8"                       │          │
│     │                                             │          │
│     │  ── Cliente configura su interfaz ──         │          │
│     │  IP: 192.168.1.50/24                        │          │
│     │  GW: 192.168.1.1                            │          │
│     │  DNS: 8.8.8.8                               │          │
└──────────────────────────────────────────────────────────────┘
```

### ¿Por qué broadcast?

```
┌──────────────────────────────────────────────────────────────┐
│  El cliente NO tiene IP todavía.                             │
│  No puede enviar un paquete unicast porque:                  │
│  • No tiene dirección de origen                              │
│  • No sabe dónde está el servidor DHCP                       │
│                                                              │
│  Por eso usa:                                                │
│  • IP origen: 0.0.0.0 (sin dirección)                       │
│  • IP destino: 255.255.255.255 (broadcast a toda la red)     │
│  • MAC origen: su propia MAC                                 │
│  • MAC destino: ff:ff:ff:ff:ff:ff (broadcast L2)             │
│                                                              │
│  El DHCPREQUEST también es broadcast aunque el cliente       │
│  ya sabe qué servidor aceptar. ¿Por qué?                    │
│  → Para que OTROS servidores DHCP sepan que su oferta        │
│    fue rechazada y liberen la IP reservada.                  │
└──────────────────────────────────────────────────────────────┘
```

### ¿Qué pasa si hay múltiples servidores DHCP?

```
┌──────────────────────────────────────────────────────────────┐
│  DHCPDISCOVER (broadcast) → llega a TODOS los servidores     │
│                                                              │
│  Servidor A: DHCPOFFER → IP 192.168.1.50                    │
│  Servidor B: DHCPOFFER → IP 192.168.1.100                   │
│                                                              │
│  El cliente recibe ambas ofertas.                            │
│  Elige una (típicamente la primera que llega).               │
│                                                              │
│  DHCPREQUEST (broadcast):                                    │
│  "Acepto la IP 192.168.1.50 del servidor A"                  │
│                                                              │
│  Servidor A: ve que fue elegido → DHCPACK                    │
│  Servidor B: ve que NO fue elegido → libera 192.168.1.100    │
│                                                              │
│  ⚠ Dos servidores DHCP en la misma red con rangos           │
│  solapados causan conflictos. Asegurar que los pools         │
│  no se solapan o usar DHCP failover.                         │
└──────────────────────────────────────────────────────────────┘
```

### DHCPNAK y DHCPDECLINE

```
┌──────────────────────────────────────────────────────────────┐
│  DHCPNAK (del servidor):                                     │
│  "La IP que solicitas ya no es válida"                        │
│  → El cliente debe reiniciar DORA desde cero                │
│  Causas: el cliente cambió de subred, el servidor se         │
│  reconfiguró, el lease expiró completamente.                 │
│                                                              │
│  DHCPDECLINE (del cliente):                                  │
│  "Detecté que la IP que me asignaste ya está en uso"         │
│  → El cliente hizo ARP probe y alguien respondió             │
│  → El servidor marca esa IP como conflictiva                 │
│  → El cliente reinicia DORA                                  │
│                                                              │
│  DHCPRELEASE (del cliente):                                  │
│  "Ya no necesito esta IP, devuélvela al pool"                │
│  → El cliente libera voluntariamente su lease                │
│  → No espera a que expire                                    │
│                                                              │
│  DHCPINFORM (del cliente):                                   │
│  "Ya tengo IP (estática), pero necesito otros datos"         │
│  → Solicitar DNS, NTP, etc. sin pedir IP                     │
│  → El servidor responde con DHCPACK sin asignar IP           │
└──────────────────────────────────────────────────────────────┘
```

---

## Anatomía de los mensajes DHCP

DHCP se transporta sobre UDP. La estructura del mensaje está basada en BOOTP (su predecesor):

```
┌──────────────────────────────────────────────────────────────┐
│                 Mensaje DHCP                                 │
│                                                              │
│  ┌─────────────────────────────────────────────┐             │
│  │ op (1B)       │ Operación: 1=request 2=reply│             │
│  │ htype (1B)    │ Tipo de HW: 1=Ethernet      │             │
│  │ hlen (1B)     │ Longitud MAC: 6             │             │
│  │ hops (1B)     │ Relay hops (0 si directo)    │             │
│  ├─────────────────────────────────────────────┤             │
│  │ xid (4B)      │ Transaction ID (aleatorio)   │             │
│  │               │ Vincula request↔reply         │             │
│  ├─────────────────────────────────────────────┤             │
│  │ secs (2B)     │ Segundos desde inicio DORA   │             │
│  │ flags (2B)    │ Bit 0: broadcast flag         │             │
│  ├─────────────────────────────────────────────┤             │
│  │ ciaddr (4B)   │ Client IP (si ya tiene una)   │             │
│  │ yiaddr (4B)   │ "Your" IP (la IP asignada)    │             │
│  │ siaddr (4B)   │ Server IP (next-server, TFTP) │             │
│  │ giaddr (4B)   │ Gateway/Relay agent IP         │             │
│  ├─────────────────────────────────────────────┤             │
│  │ chaddr (16B)  │ Client MAC address             │             │
│  │ sname (64B)   │ Server hostname (opcional)     │             │
│  │ file (128B)   │ Boot filename (PXE)            │             │
│  ├─────────────────────────────────────────────┤             │
│  │ Magic cookie  │ 99.130.83.99 (identifica DHCP)│             │
│  │ Options       │ Opciones TLV (tipo/long/valor) │             │
│  └─────────────────────────────────────────────┘             │
│                                                              │
│  El Transaction ID (xid) es clave: permite al cliente       │
│  asociar la respuesta con su petición en una red donde      │
│  múltiples clientes hacen DORA simultáneamente.              │
└──────────────────────────────────────────────────────────────┘
```

### Puertos UDP

```
┌──────────────────────────────────────────────────────────────┐
│  Cliente → Servidor:   src port 68 → dst port 67            │
│  Servidor → Cliente:   src port 67 → dst port 68            │
│                                                              │
│  ¿Por qué puertos fijos (no efímeros)?                       │
│  • El cliente no tiene IP → no puede recibir en un           │
│    puerto efímero convencional                               │
│  • El SO escucha en el puerto 68 a nivel de interfaz raw     │
│  • Esto permite recibir respuestas incluso sin IP            │
│                                                              │
│  Firewall: DHCP no funciona si bloqueas UDP 67/68            │
└──────────────────────────────────────────────────────────────┘
```

---

## El servidor DHCP

El servidor DHCP gestiona un **pool de direcciones** y mantiene una tabla de leases activos:

```
┌──────────────────────────────────────────────────────────────┐
│                 Servidor DHCP                                │
│                                                              │
│  Pool: 192.168.1.10 - 192.168.1.200                         │
│  Máscara: 255.255.255.0 (/24)                                │
│  Gateway: 192.168.1.1                                        │
│  DNS: 8.8.8.8, 1.1.1.1                                      │
│  Lease time: 24 horas                                        │
│  Domain: home.local                                          │
│                                                              │
│  Tabla de leases:                                            │
│  ┌──────────────────┬───────────────┬──────────────────────┐ │
│  │ MAC              │ IP asignada   │ Expira               │ │
│  ├──────────────────┼───────────────┼──────────────────────┤ │
│  │ aa:bb:cc:11:22:33│ 192.168.1.10  │ 2026-03-22 10:30:00  │ │
│  │ dd:ee:ff:44:55:66│ 192.168.1.11  │ 2026-03-22 14:15:00  │ │
│  │ 11:22:33:aa:bb:cc│ 192.168.1.50  │ 2026-03-21 08:00:00  │ │
│  └──────────────────┴───────────────┴──────────────────────┘ │
│                                                              │
│  Reservaciones (IP fija por MAC):                            │
│  MAC dd:ee:ff:44:55:66 → siempre 192.168.1.100              │
│  (la impresora siempre recibe la misma IP)                   │
└──────────────────────────────────────────────────────────────┘
```

### ¿Quién actúa como servidor DHCP?

```
┌──────────────────────────────────────────────────────────────┐
│  En redes domésticas:                                        │
│    El router (Fritz!Box, TP-Link, etc.) es el servidor DHCP  │
│    Viene configurado de fábrica con un pool predeterminado   │
│                                                              │
│  En redes empresariales:                                     │
│    • ISC DHCP (dhcpd) — el clásico (obsoleto desde 2022)     │
│    • Kea DHCP — sucesor de ISC DHCP (ISC)                    │
│    • dnsmasq — DHCP + DNS ligero (ideal para redes pequeñas) │
│    • Windows DHCP Server — en entornos Active Directory      │
│                                                              │
│  En contenedores/virtualización:                             │
│    • libvirt crea redes NAT con dnsmasq como DHCP            │
│    • Docker usa su propio DHCP interno para bridge networks  │
│    • VirtualBox/VMware tienen DHCP integrado                 │
└──────────────────────────────────────────────────────────────┘
```

### Reservaciones (IP estática vía DHCP)

```
┌──────────────────────────────────────────────────────────────┐
│  Reservación = vincular una MAC a una IP fija                │
│                                                              │
│  En vez de configurar IP estática en el dispositivo:         │
│  • La impresora siempre recibe 192.168.1.100                 │
│  • El NAS siempre recibe 192.168.1.200                       │
│  • La impresora sigue usando DHCP (no config manual)         │
│                                                              │
│  Ventajas sobre IP estática configurada en el dispositivo:   │
│  • Control centralizado en el servidor DHCP                  │
│  • El dispositivo no necesita configuración especial         │
│  • Si cambias el DNS o gateway, el dispositivo lo recibe     │
│    automáticamente al renovar el lease                       │
│  • Funciona incluso para dispositivos sin interfaz de config │
│    (ej. impresoras de red, cámaras IP)                       │
└──────────────────────────────────────────────────────────────┘
```

---

## DHCP relay

DHCP usa broadcast, que no cruza routers. ¿Cómo funciona DHCP cuando el servidor está en otra subred?

```
┌──────────────────────────────────────────────────────────────┐
│                    DHCP Relay                                │
│                                                              │
│  Sin relay:                                                  │
│  ┌────────────────┐         ┌─────────────────┐              │
│  │ Subred A       │ Router  │ Subred B        │              │
│  │ 192.168.1.0/24 │────────│ 192.168.2.0/24  │              │
│  │                │         │                 │              │
│  │ [Cliente DHCP] │         │ [Servidor DHCP] │              │
│  └────────────────┘         └─────────────────┘              │
│                                                              │
│  Cliente envía DHCPDISCOVER (broadcast 255.255.255.255)      │
│  El broadcast NO cruza el router → el servidor nunca lo ve   │
│  → El cliente no obtiene IP                                  │
│                                                              │
│  Con relay agent en el router:                               │
│  ┌────────────────┐         ┌─────────────────┐              │
│  │ Subred A       │ Router  │ Subred B        │              │
│  │ 192.168.1.0/24 │────────│ 192.168.2.0/24  │              │
│  │                │ (relay) │                 │              │
│  │ [Cliente DHCP] │         │ [Servidor DHCP] │              │
│  └────────────────┘         └─────────────────┘              │
│                                                              │
│  1. Cliente envía DHCPDISCOVER (broadcast)                   │
│  2. Relay en el router captura el broadcast                  │
│  3. Relay reenvía como UNICAST al servidor DHCP              │
│     (añade giaddr = IP del relay en la subred del cliente)   │
│  4. Servidor ve giaddr = 192.168.1.1                         │
│     → Sabe que el cliente está en la subred 192.168.1.0/24   │
│     → Asigna IP de ese rango                                 │
│  5. Servidor envía DHCPOFFER al relay (unicast)              │
│  6. Relay reenvía al cliente (broadcast en subred A)         │
└──────────────────────────────────────────────────────────────┘
```

El campo `giaddr` (Gateway IP Address) del mensaje DHCP es clave: le indica al servidor desde qué subred viene la petición, para que asigne una IP del pool correcto.

---

## Opciones DHCP

Las opciones DHCP extienden el protocolo con datos adicionales. Cada opción tiene un **código numérico**, una longitud y un valor:

### Opciones más importantes

| Código | Nombre | Descripción |
|--------|--------|-------------|
| 1 | Subnet Mask | Máscara de subred (ej. 255.255.255.0) |
| 3 | Router | Default gateway |
| 6 | DNS Servers | Servidores DNS (pueden ser varios) |
| 12 | Hostname | Nombre del host |
| 15 | Domain Name | Search domain (ej. home.local) |
| 28 | Broadcast Address | Dirección de broadcast |
| 42 | NTP Servers | Servidores de hora |
| 51 | Lease Time | Duración del lease en segundos |
| 53 | DHCP Message Type | Tipo: Discover(1)/Offer(2)/Request(3)/ACK(5)/NAK(6) |
| 54 | Server Identifier | IP del servidor DHCP |
| 55 | Parameter Request List | Opciones que el cliente solicita |
| 58 | Renewal Time (T1) | Tiempo antes de intentar renovar (50% del lease) |
| 59 | Rebinding Time (T2) | Tiempo antes de intentar rebind (87.5% del lease) |
| 61 | Client Identifier | Identificador del cliente (puede ser distinto a MAC) |
| 119 | Domain Search List | Lista de search domains (RFC 3397) |
| 121 | Classless Static Routes | Rutas estáticas (RFC 3442) |

### Option 55: qué pide el cliente

```
┌──────────────────────────────────────────────────────────────┐
│  Cuando el cliente envía DHCPDISCOVER, incluye la opción 55  │
│  con una lista de las opciones que necesita:                 │
│                                                              │
│  Option 55: Parameter Request List                           │
│    1   (Subnet Mask)                                         │
│    3   (Router)                                              │
│    6   (DNS)                                                 │
│    15  (Domain Name)                                         │
│    42  (NTP)                                                 │
│    119 (Domain Search List)                                  │
│                                                              │
│  El servidor responde con las opciones que tiene configuradas│
│  de la lista solicitada. No está obligado a enviar todas.    │
└──────────────────────────────────────────────────────────────┘
```

### Vendor-specific options

```
# Los fabricantes y sistemas usan opciones personalizadas:
# • PXE boot: opciones 66 (TFTP server) y 67 (boot filename)
# • WPAD (Web Proxy Auto-Discovery): opción 252
# • Opciones 224-254 están reservadas para uso privado

# En redes empresariales, el servidor DHCP puede enviar
# rutas adicionales (opción 121) para enrutar tráfico
# específico por una interfaz diferente.
```

---

## DHCPv6 vs SLAAC

IPv6 tiene dos mecanismos de configuración automática, y las redes pueden usar uno, otro, o ambos:

```
┌──────────────────────────────────────────────────────────────┐
│               IPv6: SLAAC vs DHCPv6                          │
│                                                              │
│  SLAAC (Stateless Address Autoconfiguration):                │
│  ────────────────────────────────────────────                │
│  • El router envía RA (Router Advertisement) con el prefijo  │
│  • El cliente genera su propia IP: prefijo + EUI-64/random   │
│  • El router NO sabe qué IP eligió el cliente                │
│  • NO asigna DNS (originalmente — ahora RDNSS lo permite)   │
│  • Simple, sin servidor, sin estado                          │
│                                                              │
│  DHCPv6 Stateful:                                            │
│  ────────────────                                            │
│  • Servidor DHCPv6 asigna IPs (como DHCPv4)                 │
│  • El servidor mantiene una tabla de leases                  │
│  • Asigna DNS, NTP, y otras opciones                         │
│  • Más control administrativo                                │
│                                                              │
│  DHCPv6 Stateless (solo información):                        │
│  ─────────────────────────────────────                       │
│  • SLAAC asigna la IP                                        │
│  • DHCPv6 solo proporciona DNS, NTP, search domains          │
│  • Combinación de ambos: lo mejor de cada uno                │
│                                                              │
│  El RA del router indica qué método usar:                    │
│    M flag (Managed) = 1  → usar DHCPv6 para IPs             │
│    O flag (Other)   = 1  → usar DHCPv6 para info (DNS, etc.) │
│    M=0, O=0 → solo SLAAC, sin DHCPv6                        │
│    M=0, O=1 → SLAAC para IP, DHCPv6 para DNS                │
│    M=1, O=1 → DHCPv6 para todo                              │
└──────────────────────────────────────────────────────────────┘
```

### Diferencias clave DHCPv4 vs DHCPv6

```
┌────────────────────────┬──────────────────┬─────────────────┐
│                        │ DHCPv4           │ DHCPv6          │
├────────────────────────┼──────────────────┼─────────────────┤
│ Puertos                │ 67/68 UDP        │ 546/547 UDP     │
│ Broadcast              │ Sí               │ No (multicast)  │
│ Identificación cliente │ MAC (chaddr)     │ DUID + IAID     │
│ Discover               │ DHCPDISCOVER     │ Solicit         │
│ Offer                  │ DHCPOFFER        │ Advertise       │
│ Request                │ DHCPREQUEST      │ Request         │
│ Acknowledge            │ DHCPACK          │ Reply           │
│ Gateway info           │ Opción 3 (Router)│ Viene del RA    │
│ Relay                  │ giaddr           │ Relay-forward   │
│ Coexiste con           │ —                │ SLAAC           │
└────────────────────────┴──────────────────┴─────────────────┘
```

> **Nota**: DHCPv6 **no** asigna default gateway — eso siempre viene del Router Advertisement (RA) de NDP. Esto es una diferencia fundamental con DHCPv4.

---

## Seguridad DHCP

### DHCP Starvation

```
┌──────────────────────────────────────────────────────────────┐
│  Ataque: DHCP Starvation (agotamiento del pool)              │
│                                                              │
│  1. Atacante envía miles de DHCPDISCOVER con MACs falsas     │
│  2. El servidor asigna una IP a cada "dispositivo" falso     │
│  3. El pool se agota                                         │
│  4. Los clientes legítimos no obtienen IP                    │
│                                                              │
│  Mitigación:                                                 │
│  • Port security en el switch (limitar MACs por puerto)      │
│  • DHCP rate limiting                                        │
│  • Monitoreo de leases activos                               │
└──────────────────────────────────────────────────────────────┘
```

### Rogue DHCP Server

```
┌──────────────────────────────────────────────────────────────┐
│  Ataque: Rogue DHCP Server (servidor DHCP malicioso)         │
│                                                              │
│  1. Atacante conecta un servidor DHCP falso a la red         │
│  2. Los clientes reciben ofertas del servidor malicioso      │
│  3. El atacante asigna:                                      │
│     • Gateway = IP del atacante → intercepta todo el tráfico │
│     • DNS = DNS del atacante → redirige a sitios falsos      │
│                                                              │
│  Resultado: Man-in-the-Middle completo                       │
│                                                              │
│  Mitigación:                                                 │
│  • DHCP Snooping en switches gestionados                     │
│    → Solo puertos "trusted" pueden enviar DHCPOFFER          │
│    → Los puertos de usuarios son "untrusted"                 │
│  • 802.1X (autenticación de puerto)                          │
│  • Monitoreo de red para detectar DHCPOFFERs inesperados     │
└──────────────────────────────────────────────────────────────┘
```

### DHCP Snooping

```
┌──────────────────────────────────────────────────────────────┐
│  DHCP Snooping (en switches gestionados):                    │
│                                                              │
│  Puertos TRUSTED (hacia el servidor DHCP / uplink):          │
│  • Pueden enviar DHCPOFFER y DHCPACK                         │
│  • El servidor DHCP legítimo está en un puerto trusted       │
│                                                              │
│  Puertos UNTRUSTED (hacia los clientes):                     │
│  • Solo pueden enviar DHCPDISCOVER y DHCPREQUEST             │
│  • Si un puerto untrusted envía un DHCPOFFER → descartado   │
│  • El switch construye una tabla de MAC↔IP↔puerto            │
│    (binding table) que usa para DAI (Dynamic ARP Inspection) │
│                                                              │
│  El snooping también protege contra DHCP starvation:         │
│  • Limita el rate de DHCPDISCOVER por puerto                 │
│  • Detecta flujos anómalos de mensajes DHCP                 │
└──────────────────────────────────────────────────────────────┘
```

---

## Errores comunes

### 1. Dos servidores DHCP en la misma red con pools solapados

```
# Servidor A: pool 192.168.1.10 - 192.168.1.200
# Servidor B: pool 192.168.1.100 - 192.168.1.250
#                   ↑ SOLAPAMIENTO ↑

# Resultado: ambos servidores ofrecen la misma IP.
# Un cliente recibe .150 de A, otro recibe .150 de B.
# → Conflicto de IP. Ambas máquinas pierden conectividad.

# Prevención:
# • Solo UN servidor DHCP por subred
# • Si hay dos (redundancia): pools NO solapados
#   Servidor A: 192.168.1.10 - 192.168.1.100
#   Servidor B: 192.168.1.101 - 192.168.1.200
```

### 2. Rango DHCP que incluye IPs usadas estáticamente

```
# Pool DHCP: 192.168.1.10 - 192.168.1.200
# Impresora con IP estática: 192.168.1.50

# DHCP asigna 192.168.1.50 a un laptop → conflicto de IP
# La impresora pierde conectividad intermitentemente

# Solución: excluir IPs estáticas del pool
# O mejor: usar reservaciones DHCP en vez de IPs estáticas
```

### 3. Lease time demasiado corto o demasiado largo

```
# Lease de 5 minutos:
# • Demasiadas renovaciones (tráfico innecesario)
# • Si el servidor DHCP cae, los clientes pierden IP en 5 min

# Lease de 30 días:
# • IPs quedan "reservadas" para dispositivos que ya no están
# • En una cafetería, el pool se agota con clientes que ya se fueron

# Recomendaciones:
# Red de oficina: 8-24 horas (dispositivos estables)
# WiFi público: 1-4 horas (dispositivos transitorios)
# Red de servidores: usar reservaciones (IP fija por MAC)
```

### 4. Olvidar que DHCP broadcast no cruza routers

```
# "Puse el servidor DHCP en la subred del datacenter pero
#  los clientes en la subred de oficinas no obtienen IP"

# Broadcast 255.255.255.255 NO cruza routers.
# Solución: configurar DHCP relay en el router que conecta
# ambas subredes.

# En Linux (como relay):
# dhcrelay -i eth0 192.168.2.10    # IP del servidor DHCP

# En un router Cisco:
# interface GigabitEthernet0/0
#   ip helper-address 192.168.2.10
```

### 5. No verificar conflictos de IP antes de asignar

```
# El servidor DHCP debería verificar que la IP está libre
# antes de asignarla (ICMP echo / ARP probe).
# No todos los servidores DHCP lo hacen por defecto.

# El cliente también debería verificar (ARP probe tras DHCPACK).
# Si detecta conflicto → envía DHCPDECLINE.
# Clientes modernos (NetworkManager) hacen esta verificación.
```

---

## Cheatsheet

```
┌──────────────────────────────────────────────────────────────┐
│               Funcionamiento DHCP Cheatsheet                 │
├──────────────────────────────────────────────────────────────┤
│                                                              │
│  DORA:                                                       │
│    Discover → broadcast, "necesito IP" (0.0.0.0 → 255...255)│
│    Offer    → "te ofrezco esta IP, lease N horas"            │
│    Request  → broadcast, "acepto IP de servidor X"           │
│    Acknowledge → "confirmado, aquí tienes toda la config"    │
│                                                              │
│  OTROS MENSAJES:                                             │
│    NAK      → "esa IP ya no es válida, empieza de nuevo"     │
│    Decline  → "detecté conflicto ARP, dame otra"             │
│    Release  → "devuelvo la IP voluntariamente"               │
│    Inform   → "ya tengo IP, solo quiero DNS/NTP/etc."        │
│                                                              │
│  PUERTOS:                                                    │
│    Cliente: UDP 68       Servidor: UDP 67                    │
│    DHCPv6 — Cliente: UDP 546, Servidor: UDP 547              │
│                                                              │
│  OPCIONES CLAVE:                                             │
│    1 = subnet mask         3 = gateway (router)              │
│    6 = DNS servers         12 = hostname                     │
│    15 = domain name        42 = NTP                          │
│    51 = lease time         53 = message type                 │
│    55 = parameter request list                               │
│    121 = classless static routes                             │
│                                                              │
│  LEASE:                                                      │
│    T1 (50%) = intentar renovar con el mismo servidor         │
│    T2 (87.5%) = intentar rebind con cualquier servidor       │
│    100% = lease expira, perder IP, reiniciar DORA            │
│                                                              │
│  RESERVACIÓN:                                                │
│    MAC → IP fija en el servidor DHCP                         │
│    El dispositivo sigue usando DHCP (no config manual)       │
│    Recibe actualizaciones de DNS/gateway automáticamente     │
│                                                              │
│  RELAY:                                                      │
│    Broadcast no cruza routers                                │
│    Relay reenvía DHCP como unicast al servidor               │
│    giaddr indica la subred del cliente                       │
│                                                              │
│  SEGURIDAD:                                                  │
│    DHCP Starvation: agotar pool con MACs falsas              │
│    Rogue DHCP: servidor falso → MITM                         │
│    DHCP Snooping: trusted/untrusted ports en switch          │
│                                                              │
│  DHCPv6 vs SLAAC:                                            │
│    SLAAC: autoconfiguración sin servidor (RA + EUI-64)       │
│    DHCPv6 stateful: servidor asigna IP (como DHCPv4)         │
│    DHCPv6 stateless: SLAAC para IP, DHCPv6 para DNS/NTP     │
│    RA flags: M=managed(DHCPv6), O=other(info)               │
│    DHCPv6 NO asigna gateway (siempre viene del RA)           │
│                                                              │
│  SERVIDORES DHCP:                                            │
│    Router doméstico (built-in)                               │
│    dnsmasq (ligero, DHCP + DNS)                              │
│    Kea DHCP (sucesor de ISC dhcpd)                           │
│    Windows DHCP Server                                       │
│                                                              │
└──────────────────────────────────────────────────────────────┘
```

---

## Ejercicios

### Ejercicio 1: Trazar el proceso DORA

Un laptop nuevo se conecta por cable a una red de oficina. La red tiene:
- Servidor DHCP en 192.168.10.1
- Pool: 192.168.10.50 - 192.168.10.150
- Gateway: 192.168.10.1
- DNS: 10.0.0.53
- Lease time: 8 horas
- Domain: office.local

**Traza cada paso DORA**, indicando para cada mensaje:
1. IP origen y destino
2. Puerto origen y destino
3. MAC destino (unicast o broadcast)
4. Contenido relevante del mensaje (opciones DHCP clave)

Después responde:
- ¿Por qué el DHCPREQUEST del paso 3 es broadcast si el cliente ya sabe la IP del servidor?
- Si hay un segundo servidor DHCP en la red que ofrece 192.168.10.200, ¿qué pasa cuando el cliente envía DHCPREQUEST aceptando la oferta del primer servidor?
- ¿Qué opciones DHCP aparecerían en la opción 55 (Parameter Request List) del DHCPDISCOVER?

**Pregunta de reflexión**: ¿Por qué DHCP usa UDP en vez de TCP? ¿Qué problemas tendría intentar establecer una conexión TCP cuando el cliente no tiene IP?

---

### Ejercicio 2: Diseñar el esquema DHCP de una oficina

Una oficina tiene:
- 80 empleados con laptops (conexión WiFi)
- 10 impresoras de red (necesitan IP fija)
- 5 servidores (IP estática configurada manualmente)
- Una sala de reuniones con WiFi para visitantes (hasta 30 dispositivos)
- Subred: 192.168.1.0/24

**Diseña**:
1. ¿Qué rango de IPs asignarías al pool DHCP para empleados?
2. ¿Cómo manejarías las impresoras? (¿IP estática o reservación DHCP?)
3. ¿Qué rango excluirías para los servidores?
4. ¿Qué lease time pondrías para la red de empleados vs la de visitantes?
5. Si el pool de visitantes se agota frecuentemente, ¿qué ajustarías?

**Pregunta de reflexión**: ¿Por qué es mejor usar reservaciones DHCP para las impresoras en vez de configurar IP estática en cada impresora? ¿En qué caso preferirías IP estática real sobre reservación?

---

### Ejercicio 3: Diagnosticar problemas DHCP

Para cada escenario, identifica la causa más probable y cómo lo solucionarías:

**Escenario A**: Un nuevo empleado conecta su laptop al WiFi. Obtiene IP `169.254.x.x` (APIPA) en vez de una IP del rango `192.168.1.x`.

**Escenario B**: Dos usuarios reportan que su conexión se cae intermitentemente. Al investigar, ambos tienen la misma IP `192.168.1.75`.

**Escenario C**: Los dispositivos en el piso 3 no obtienen IP, pero los del piso 1 (donde está el servidor DHCP) funcionan correctamente.

**Escenario D**: Un usuario reporta que puede hacer ping a IPs pero no resolver nombres de dominio. Su IP fue obtenida por DHCP.

**Escenario E**: Varios usuarios reportan que su navegador los redirige a páginas extrañas. Al investigar, sus máquinas tienen como gateway una IP que no es el router de la empresa.

**Pregunta de reflexión**: En el escenario E, ¿cómo podrías detectar un rogue DHCP server desde la línea de comandos de Linux? ¿Qué herramienta usarías? (Pista: `nmap --script broadcast-dhcp-discover` o captura con `tcpdump port 67 or port 68`.)

---

> **Siguiente tema**: T02 — Leases (tiempo de alquiler, renovación, rebinding)
