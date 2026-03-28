# Bloque 9: Redes

**Objetivo**: Comprender redes TCP/IP y configurar networking en Linux.
Este es un bloque multi-temático que requiere conocimientos de CLI y Linux fundamentals.

## Capítulo 1: Modelo TCP/IP [A]

### Sección 1: Capas y Protocolos
- **T01 - Las 4 capas**: enlace, red, transporte, aplicación — comparación con OSI
- **T02 - Encapsulación**: headers, payload, MTU, fragmentación
- **T03 - Ethernet**: frames, MAC addresses, ARP, ARP cache

### Sección 2: Capa de Red
- **T01 - IPv4**: formato de dirección, clases (histórico), CIDR
- **T02 - Subnetting**: máscara de red, cálculo de subredes, broadcast, hosts disponibles
- **T03 - IPv6**: formato, tipos de dirección (link-local, global, loopback), autoconfiguración
- **T04 - ICMP**: ping, traceroute, mensajes de error, Path MTU Discovery

### Sección 3: Capa de Transporte
- **T01 - TCP**: three-way handshake, estados de conexión, ventana, retransmisión, FIN/RST
- **T02 - UDP**: sin conexión, sin garantía de entrega, cuándo preferir sobre TCP
- **T03 - Puertos**: well-known (0-1023), registered, ephemeral, /etc/services

### Sección 4: Capa de Aplicación (HTTP)
- **T01 - HTTP/1.1**: request/response structure, métodos (GET, POST, PUT, DELETE, PATCH), status codes (1xx-5xx)
- **T02 - Headers HTTP**: Content-Type, Content-Length, Host, User-Agent, Accept, Cache-Control, MIME types
- **T03 - CORS**: Same-Origin Policy, preflight requests, Access-Control-Allow-*, relevancia para WASM en navegador
- **T04 - HTTP/2 y HTTP/3**: multiplexing, server push, QUIC (UDP), binary framing, diferencias prácticas con HTTP/1.1
- **T05 - WebSocket**: upgrade de HTTP, comunicación bidireccional, frames, cuándo usar sobre HTTP

## Capítulo 2: DNS [M]

### Sección 1: Funcionamiento
- **T01 - Jerarquía DNS**: root servers, TLDs, dominios, FQDN
- **T02 - Tipos de registro**: A, AAAA, CNAME, MX, NS, PTR, SOA, TXT, SRV
- **T03 - Resolución**: recursiva vs iterativa, caching, TTL
- **T04 - /etc/resolv.conf y /etc/hosts**: configuración del cliente, nsswitch.conf

### Sección 2: Herramientas
- **T01 - dig y nslookup**: consultas DNS, interpretar la salida
- **T02 - host**: consultas simples
- **T03 - systemd-resolved**: configuración moderna, resolvectl

## Capítulo 3: DHCP [A]

### Sección 1: Protocolo y Configuración
- **T01 - Funcionamiento DHCP**: DORA (Discover, Offer, Request, Acknowledge)
- **T02 - Leases**: tiempo de alquiler, renovación, rebinding
- **T03 - Configuración de cliente**: dhclient, NetworkManager, systemd-networkd

## Capítulo 4: Configuración de Red en Linux [M]

### Sección 1: Herramientas Modernas
- **T01 - ip command**: ip addr, ip route, ip link, ip neigh — reemplazo de ifconfig/route
- **T02 - NetworkManager**: nmcli, nmtui, connection profiles, on Debian y RHEL
- **T03 - systemd-networkd**: .network files, cuándo usar sobre NetworkManager
- **T04 - /etc/network/interfaces**: configuración clásica Debian (aún válida)

### Sección 2: Configuración Avanzada
- **T01 - Hostname**: hostnamectl, /etc/hostname, /etc/hosts
- **T02 - Bonding y teaming**: alta disponibilidad de red, modos de bonding
- **T03 - VLANs**: 802.1Q, configuración con ip link, NetworkManager
- **T04 - Bridge networking**: crear bridges, uso en virtualización y Docker
- **T05 - VPN tunnels**: concepto de túnel, WireGuard (kernel-integrado, configuración mínima con wg/wg-quick), comparación breve con OpenVPN (user-space, TLS-based, más complejo)

## Capítulo 5: Firewall [M]

### Sección 1: Netfilter e iptables
- **T01 - Arquitectura Netfilter**: tablas (filter, nat, mangle), chains (INPUT, OUTPUT, FORWARD)
- **T02 - iptables**: reglas, targets (ACCEPT, DROP, REJECT, LOG), matching
- **T03 - NAT con iptables**: SNAT, DNAT, MASQUERADE, port forwarding
- **T04 - nftables**: sucesor de iptables, sintaxis, migración

### Sección 2: firewalld
- **T01 - Zonas**: public, internal, trusted, etc. — modelo de confianza
- **T02 - firewall-cmd**: añadir servicios, puertos, rich rules, permanent vs runtime
- **T03 - Direct rules**: integración con iptables subyacente, cuándo necesarias

## Capítulo 6: Diagnóstico de Red [M]

### Sección 1: Herramientas
- **T01 - ss**: reemplazo de netstat, filtros, estados de conexión
- **T02 - tcpdump**: captura de paquetes, filtros BPF, análisis básico
- **T03 - traceroute y mtr**: diagnóstico de rutas, interpretación de resultados
- **T04 - nmap**: escaneo de puertos, detección de servicios (uso legítimo en lab)
- **T05 - curl y wget**: pruebas HTTP, flags útiles para diagnóstico

## Capítulo 7: Redes en Docker [M]

### Sección 1: Networking de Contenedores
- **T01 - Bridge networking avanzado**: subnets custom, gateways, comunicación entre redes
- **T02 - Host networking**: cuándo usar, implicaciones de seguridad
- **T03 - Lab de red multi-contenedor**: simular topologías de red con Docker Compose
