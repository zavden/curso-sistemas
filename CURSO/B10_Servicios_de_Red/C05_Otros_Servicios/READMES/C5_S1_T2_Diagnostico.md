# Diagnóstico DHCP

## Índice
1. [Logs del servidor dhcpd](#1-logs-del-servidor-dhcpd)
2. [Interpretar mensajes de log](#2-interpretar-mensajes-de-log)
3. [tcpdump — capturar DORA en la red](#3-tcpdump--capturar-dora-en-la-red)
4. [Herramientas del cliente](#4-herramientas-del-cliente)
5. [Escenarios de diagnóstico](#5-escenarios-de-diagnóstico)
6. [dhcpd.leases como herramienta de diagnóstico](#6-dhcpdleases-como-herramienta-de-diagnóstico)
7. [Errores comunes](#7-errores-comunes)
8. [Cheatsheet](#8-cheatsheet)
9. [Ejercicios](#9-ejercicios)

---

## 1. Logs del servidor dhcpd

dhcpd registra cada transacción DHCP en syslog, por defecto con facility `daemon` o `local7`.

### Ver logs en tiempo real

```bash
# systemd journal (distribuciones modernas)
sudo journalctl -u isc-dhcp-server -f       # Debian
sudo journalctl -u dhcpd -f                  # RHEL

# Syslog tradicional
sudo tail -f /var/log/syslog | grep dhcpd    # Debian
sudo tail -f /var/log/messages | grep dhcpd  # RHEL
```

### Redirigir logs a archivo dedicado

```bash
# /etc/rsyslog.d/dhcpd.conf
local7.*    /var/log/dhcpd.log
```

En dhcpd.conf, indicar el facility:

```bash
# /etc/dhcp/dhcpd.conf
log-facility local7;
```

```bash
# Aplicar
sudo systemctl restart rsyslog
sudo systemctl restart isc-dhcp-server

# Ahora los logs van a /var/log/dhcpd.log
sudo tail -f /var/log/dhcpd.log
```

---

## 2. Interpretar mensajes de log

### Transacción DORA completa en logs

```
Mar 21 10:00:01 server dhcpd: DHCPDISCOVER from 00:11:22:33:44:55 via eth0
Mar 21 10:00:01 server dhcpd: DHCPOFFER on 192.168.1.101 to 00:11:22:33:44:55 via eth0
Mar 21 10:00:01 server dhcpd: DHCPREQUEST for 192.168.1.101 from 00:11:22:33:44:55 via eth0
Mar 21 10:00:01 server dhcpd: DHCPACK on 192.168.1.101 to 00:11:22:33:44:55 (laptop-juan) via eth0
```

```
Línea 1: DHCPDISCOVER from MAC via INTERFAZ
         → El cliente con esa MAC pide una IP

Línea 2: DHCPOFFER on IP to MAC via INTERFAZ
         → El servidor ofrece esa IP

Línea 3: DHCPREQUEST for IP from MAC via INTERFAZ
         → El cliente acepta la oferta

Línea 4: DHCPACK on IP to MAC (hostname) via INTERFAZ
         → El servidor confirma la asignación
         → hostname entre paréntesis si el cliente lo envió
```

### Renovación (sin DISCOVER)

```
Mar 21 14:00:01 server dhcpd: DHCPREQUEST for 192.168.1.101 from 00:11:22:33:44:55 via eth0
Mar 21 14:00:01 server dhcpd: DHCPACK on 192.168.1.101 to 00:11:22:33:44:55 (laptop-juan) via eth0
```

Una renovación solo tiene REQUEST + ACK — no hay DISCOVER ni OFFER porque el cliente ya conoce su IP y el servidor.

### Mensajes de error y eventos

```bash
# Cliente pide IP que no le corresponde
DHCPNAK on 192.168.1.101 to 00:11:22:33:44:55 via eth0
# → El cliente tenía esa IP antes pero ya no es válida
#   (cambió de subred, lease expiró y se reasignó)

# Liberación voluntaria (shutdown limpio del cliente)
DHCPRELEASE of 192.168.1.101 from 00:11:22:33:44:55 via eth0 (found)
# → El cliente libera la IP al apagarse

# Conflicto de IP detectado
DHCPDECLINE of 192.168.1.101 from 00:11:22:33:44:55 via eth0: abandoned
# → El cliente detectó (ARP) que otro equipo ya usa esa IP
#   dhcpd marca la IP como "abandoned" y ofrece otra

# Sin IPs disponibles
DHCPDISCOVER from 00:11:22:33:44:55 via eth0: no free leases
# → El pool está agotado — todos los IPs asignados

# Cliente desconocido (si deny unknown-clients está activo)
DHCPDISCOVER from 00:11:22:33:44:55 via eth0: unknown client
```

### Mensajes via relay

```
DHCPDISCOVER from 00:11:22:33:44:55 via 192.168.2.1
                                          ↑
                                    IP del relay agent
                                    (no es una interfaz local)
```

Cuando `via` muestra una IP en lugar de una interfaz (`eth0`), el paquete llegó a través de un relay agent.

---

## 3. tcpdump — capturar DORA en la red

tcpdump permite ver los paquetes DHCP exactos que circulan por la red, incluyendo su contenido.

### Captura básica de tráfico DHCP

```bash
# DHCP usa puertos 67 (servidor) y 68 (cliente)
sudo tcpdump -i eth0 port 67 or port 68 -n -vv
```

Flags importantes:

| Flag | Efecto |
|------|--------|
| `-i eth0` | Interfaz a capturar |
| `-n` | No resolver DNS (más rápido) |
| `-vv` | Verbose — muestra opciones DHCP |
| `-e` | Muestra direcciones MAC |
| `-w file.pcap` | Guardar en archivo para Wireshark |
| `-c N` | Capturar solo N paquetes |

### Captura completa de un DORA

```bash
sudo tcpdump -i eth0 port 67 or port 68 -n -vve
```

```
10:00:01.001 00:11:22:33:44:55 > ff:ff:ff:ff:ff:ff, IPv4,
  0.0.0.0.68 > 255.255.255.255.67: BOOTP/DHCP, Request,
  DHCP-Message (53), Value: DHCP Discover
  Client-ID (61), Value: 00:11:22:33:44:55
  Hostname (12), Value: "laptop-juan"
  Parameter-Request (55), Value: Subnet, Router, DNS, Domain

10:00:01.003 aa:bb:cc:dd:ee:ff > 00:11:22:33:44:55, IPv4,
  192.168.1.10.67 > 192.168.1.101.68: BOOTP/DHCP, Reply,
  DHCP-Message (53), Value: DHCP Offer
  Server-ID (54), Value: 192.168.1.10
  Lease-Time (51), Value: 3600
  Subnet (1), Value: 255.255.255.0
  Router (3), Value: 192.168.1.1
  DNS (6), Value: 8.8.8.8

10:00:01.005 00:11:22:33:44:55 > ff:ff:ff:ff:ff:ff, IPv4,
  0.0.0.0.68 > 255.255.255.255.67: BOOTP/DHCP, Request,
  DHCP-Message (53), Value: DHCP Request
  Requested-IP (50), Value: 192.168.1.101
  Server-ID (54), Value: 192.168.1.10

10:00:01.006 aa:bb:cc:dd:ee:ff > 00:11:22:33:44:55, IPv4,
  192.168.1.10.67 > 192.168.1.101.68: BOOTP/DHCP, Reply,
  DHCP-Message (53), Value: DHCP ACK
  Lease-Time (51), Value: 3600
  Subnet (1), Value: 255.255.255.0
  Router (3), Value: 192.168.1.1
  DNS (6), Value: 8.8.8.8
```

### Anatomía del paquete DHCP

```
┌──────────────────────────────────────────────────────────┐
│              Paquete DHCP (dentro de UDP)                  │
│                                                          │
│  ┌─ Ethernet ─────────────────────────────────┐          │
│  │ src MAC: 00:11:22:33:44:55                 │          │
│  │ dst MAC: ff:ff:ff:ff:ff:ff (broadcast)     │          │
│  │                                            │          │
│  │ ┌─ IP ──────────────────────────────────┐  │          │
│  │ │ src: 0.0.0.0 (no tiene IP aún)       │  │          │
│  │ │ dst: 255.255.255.255 (broadcast)     │  │          │
│  │ │                                      │  │          │
│  │ │ ┌─ UDP ───────────────────────────┐  │  │          │
│  │ │ │ src port: 68 (bootpc)          │  │  │          │
│  │ │ │ dst port: 67 (bootps)          │  │  │          │
│  │ │ │                                │  │  │          │
│  │ │ │ ┌─ BOOTP/DHCP ──────────────┐  │  │  │          │
│  │ │ │ │ op: 1 (request)          │  │  │  │          │
│  │ │ │ │ xid: 0x3903F326 (trans.) │  │  │  │          │
│  │ │ │ │ chaddr: 00:11:22:33:44:55│  │  │  │          │
│  │ │ │ │                          │  │  │  │          │
│  │ │ │ │ Options:                 │  │  │  │          │
│  │ │ │ │  53: DHCP Discover       │  │  │  │          │
│  │ │ │ │  55: Subnet,Router,DNS   │  │  │  │          │
│  │ │ │ │  12: "laptop-juan"       │  │  │  │          │
│  │ │ │ └──────────────────────────┘  │  │  │          │
│  │ │ └────────────────────────────────┘  │  │          │
│  │ └────────────────────────────────────────┘  │          │
│  └────────────────────────────────────────────┘          │
└──────────────────────────────────────────────────────────┘
```

### Guardar y analizar con Wireshark

```bash
# Capturar a archivo
sudo tcpdump -i eth0 port 67 or port 68 -w /tmp/dhcp.pcap

# Abrir con Wireshark (GUI)
wireshark /tmp/dhcp.pcap

# O analizar con tshark (CLI)
tshark -r /tmp/dhcp.pcap -V -Y "bootp"
```

### Filtros útiles para tcpdump

```bash
# Solo DISCOVER (encontrar clientes que buscan DHCP)
sudo tcpdump -i eth0 port 67 or port 68 -n -v 2>&1 | grep -i discover

# Solo desde un MAC específico
sudo tcpdump -i eth0 ether src 00:11:22:33:44:55 and port 67 -n -vv

# Solo broadcast (descartar unicast de renovaciones)
sudo tcpdump -i eth0 'port 67 or port 68' and broadcast -n -vv

# Capturar solo 4 paquetes (un DORA completo)
sudo tcpdump -i eth0 port 67 or port 68 -c 4 -n -vv
```

---

## 4. Herramientas del cliente

### dhclient — cliente DHCP clásico

```bash
# Solicitar IP en una interfaz (ejecuta DORA)
sudo dhclient eth0

# Liberar la IP actual (envía DHCPRELEASE)
sudo dhclient -r eth0

# Renovar lease (envía DHCPREQUEST)
sudo dhclient eth0

# Modo verbose (ver el intercambio)
sudo dhclient -v eth0
```

Salida de `dhclient -v`:

```
Internet Systems Consortium DHCP Client 4.4.3
Listening on LPF/eth0/00:11:22:33:44:55
Sending on   LPF/eth0/00:11:22:33:44:55
DHCPDISCOVER on eth0 to 255.255.255.255 port 67 interval 3
DHCPOFFER of 192.168.1.101 from 192.168.1.10
DHCPREQUEST for 192.168.1.101 on eth0 to 255.255.255.255 port 67
DHCPACK of 192.168.1.101 from 192.168.1.10
bound to 192.168.1.101 -- renewal in 1800 seconds.
```

### NetworkManager (nmcli)

En sistemas con NetworkManager, `dhclient` no se usa directamente:

```bash
# Ver la configuración DHCP obtenida
nmcli device show eth0 | grep -i dhcp

# Forzar nueva solicitud DHCP
sudo nmcli connection down eth0 && sudo nmcli connection up eth0

# Ver el lease actual
nmcli -f DHCP4 device show eth0
```

```
DHCP4.OPTION[1]:   dhcp-lease-time = 3600
DHCP4.OPTION[2]:   domain-name = example.com
DHCP4.OPTION[3]:   domain-name-servers = 8.8.8.8
DHCP4.OPTION[4]:   ip-address = 192.168.1.101
DHCP4.OPTION[5]:   routers = 192.168.1.1
DHCP4.OPTION[6]:   subnet-mask = 255.255.255.0
```

### systemd-networkd

```bash
# Ver leases de networkd
networkctl status eth0

# Leases almacenados
ls /run/systemd/netif/leases/
cat /run/systemd/netif/leases/2    # Número de interfaz
```

### nmap — descubrir servidores DHCP

```bash
# Enviar DHCPDISCOVER y ver quién responde
# Útil para detectar DHCP rogues (servidores no autorizados)
sudo nmap --script broadcast-dhcp-discover -e eth0
```

```
Starting Nmap
Pre-scan script results:
| broadcast-dhcp-discover:
|   Response 1 of 1:
|     IP Offered: 192.168.1.105
|     DHCP Message Type: DHCPOFFER
|     Server Identifier: 192.168.1.10
|     Subnet Mask: 255.255.255.0
|     Router: 192.168.1.1
|     Domain Name Server: 8.8.8.8
|     Domain Name: example.com
|_    IP Address Lease Time: 1h
```

Si aparecen múltiples respuestas de servidores diferentes, hay un **DHCP rogue** en la red.

---

## 5. Escenarios de diagnóstico

### Escenario 1 — El cliente no obtiene IP

```
┌──────────────────────────────────────────────────────────┐
│  Síntoma: cliente obtiene IP 169.254.x.x (APIPA)         │
│  o no obtiene IP en absoluto                             │
│                                                          │
│  Diagnóstico:                                            │
│                                                          │
│  ┌── En el CLIENTE ──────────────────────────────────┐   │
│  │ 1. ¿El cable/WiFi está conectado?                 │   │
│  │    ip link show eth0  → state UP?                 │   │
│  │                                                   │   │
│  │ 2. ¿Sale el DISCOVER?                             │   │
│  │    sudo tcpdump -i eth0 port 67 or 68 -n          │   │
│  │    sudo dhclient -v eth0                          │   │
│  │    → Si no hay tráfico: problema de capa 2        │   │
│  │                                                   │   │
│  │ 3. ¿Llega el OFFER?                               │   │
│  │    → Si DISCOVER sale pero no llega OFFER:        │   │
│  │      firewall, servidor caído, o VLAN incorrecta  │   │
│  └───────────────────────────────────────────────────┘   │
│                                                          │
│  ┌── En el SERVIDOR ────────────────────────────────┐    │
│  │ 4. ¿El servicio está corriendo?                   │   │
│  │    systemctl status isc-dhcp-server               │   │
│  │                                                   │   │
│  │ 5. ¿Ve el DISCOVER en los logs?                   │   │
│  │    journalctl -u isc-dhcp-server | grep DISCOVER  │   │
│  │    → Si no: el broadcast no llega al servidor     │   │
│  │      (VLAN, firewall, relay no configurado)       │   │
│  │                                                   │   │
│  │ 6. ¿Hay IPs disponibles?                         │   │
│  │    grep "no free leases" logs                     │   │
│  │    → Pool agotado: ampliar range o reducir lease  │   │
│  │                                                   │   │
│  │ 7. ¿El cliente está permitido?                    │   │
│  │    deny unknown-clients? allow/deny en pools?     │   │
│  └───────────────────────────────────────────────────┘   │
└──────────────────────────────────────────────────────────┘
```

### Escenario 2 — El cliente obtiene IP incorrecta

```bash
# Síntoma: el cliente obtiene IP pero de un rango/subred incorrecto

# Causa 1: DHCP rogue (servidor no autorizado en la red)
# Diagnóstico: capturar quién responde al DISCOVER
sudo tcpdump -i eth0 port 67 -n -e 2>&1 | grep OFFER
# Ver la MAC e IP del servidor que responde
# Si no coincide con el servidor legítimo → rogue

# Causa 2: relay envía a servidor con configuración incorrecta
# Verificar la IP del relay (campo giaddr) en los logs

# Causa 3: el cliente tiene lease cacheado de otra red
# Solución:
sudo dhclient -r eth0    # Liberar
sudo dhclient -v eth0    # Solicitar nueva
```

### Escenario 3 — Conflicto de IP

```bash
# Síntoma: "address already in use" en dos equipos

# Diagnóstico: ¿quién tiene realmente esa IP?
arping -D -I eth0 192.168.1.101
# Si alguien responde → conflicto confirmado

# Ver en los logs del servidor
grep "DHCPDECLINE\|abandoned" /var/log/dhcpd.log

# Causas comunes:
# 1. IP estática configurada manualmente dentro del rango DHCP
# 2. Dos servidores DHCP con rangos solapados
# 3. Lease no expiró pero el equipo se reconfiguró

# Solución:
# - Identificar quién tiene IP estática y moverla fuera del rango
# - O crear una reservación para ese equipo
```

### Escenario 4 — Pool agotado

```bash
# Síntoma: "no free leases" en los logs

# ¿Cuántos leases activos hay?
grep -c "binding state active" /var/lib/dhcp/dhcpd.leases

# ¿Cuántas IPs tiene el rango?
# range 192.168.1.100 192.168.1.200 = 101 IPs

# Soluciones:
# 1. Reducir lease time (reciclar IPs más rápido)
#    default-lease-time 1800;    # 30 min
#
# 2. Ampliar el rango
#    range 192.168.1.50 192.168.1.250;
#
# 3. Limpiar leases huérfanos (equipos desconectados que no liberaron)
#    → Reiniciar dhcpd limpia leases expirados del archivo
```

---

## 6. dhcpd.leases como herramienta de diagnóstico

### Consultas útiles

```bash
# Todos los leases activos
grep -B1 -A7 "binding state active" /var/lib/dhcp/dhcpd.leases

# Buscar por MAC
grep -B2 -A6 "00:11:22:33:44:55" /var/lib/dhcp/dhcpd.leases

# Buscar por hostname
grep -B4 -A4 "laptop-juan" /var/lib/dhcp/dhcpd.leases

# Buscar por IP
grep -A8 "lease 192.168.1.101" /var/lib/dhcp/dhcpd.leases

# IPs abandonadas (conflictos previos)
grep -B1 -A5 "abandoned" /var/lib/dhcp/dhcpd.leases

# Contar leases por estado
grep "binding state" /var/lib/dhcp/dhcpd.leases | sort | uniq -c
#   45 binding state active;
#   12 binding state free;
#    3 binding state abandoned;
```

### Script de reporte de leases

```bash
#!/bin/bash
# dhcp-report.sh — resumen de leases activos

LEASES="/var/lib/dhcp/dhcpd.leases"

echo "=== DHCP Lease Report ==="
echo "Active leases: $(grep -c 'binding state active' "$LEASES")"
echo "Free leases:   $(grep -c 'binding state free' "$LEASES")"
echo "Abandoned:      $(grep -c 'abandoned' "$LEASES")"
echo ""
echo "=== Active Leases ==="
echo "IP              MAC                Hostname           Expires"
echo "─────────────── ────────────────── ────────────────── ──────────────────"

awk '/^lease/{ip=$2} /hardware ethernet/{mac=$3; gsub(/;/,"",mac)}
     /client-hostname/{host=$2; gsub(/[";]/,"",host)}
     /ends/{end=$3" "$4; gsub(/;/,"",end)}
     /binding state active/{printf "%-15s %-18s %-18s %s\n", ip, mac, host, end; host="-"}' \
     "$LEASES"
```

---

## 7. Errores comunes

### Error 1 — tcpdump no muestra opciones DHCP

```bash
# MAL — sin verbose, solo muestra resumen
sudo tcpdump -i eth0 port 67 or port 68
# Solo: "BOOTP/DHCP, Request from 00:11:22:33:44:55, length 300"

# BIEN — con -vv muestra opciones DHCP completas
sudo tcpdump -i eth0 port 67 or port 68 -n -vv

# AÚN MEJOR — con -e muestra MACs de capa 2
sudo tcpdump -i eth0 port 67 or port 68 -n -vve
```

### Error 2 — dhclient no funciona porque NetworkManager lo controla

```bash
# En sistemas con NetworkManager, ejecutar dhclient manualmente
# puede entrar en conflicto:
sudo dhclient eth0
# → NetworkManager detecta el cambio y lo revierte

# Solución: usar nmcli en lugar de dhclient
sudo nmcli connection down eth0
sudo nmcli connection up eth0

# O si necesitas dhclient para debug, desactivar NM temporalmente:
sudo nmcli device set eth0 managed no
sudo dhclient -v eth0
# ... hacer pruebas ...
sudo dhclient -r eth0
sudo nmcli device set eth0 managed yes
```

### Error 3 — No ver tráfico DHCP en la interfaz correcta

```bash
# El servidor tiene múltiples interfaces
# tcpdump en la interfaz equivocada no captura nada

# Verificar en qué interfaz escucha dhcpd:
grep INTERFACES /etc/default/isc-dhcp-server
# INTERFACESv4="eth1"   ← dhcpd escucha en eth1

# Capturar en la interfaz correcta:
sudo tcpdump -i eth1 port 67 or port 68 -n -vv

# O capturar en todas las interfaces:
sudo tcpdump -i any port 67 or port 68 -n -vv
```

### Error 4 — Confundir DHCPNAK con fallo del servidor

```bash
# Log: DHCPNAK on 192.168.1.101 to 00:11:22:33:44:55
# No es un error del servidor — es comportamiento correcto

# El servidor envía NAK cuando:
# - El cliente pide una IP de otra subred (cambió de red)
# - El lease ya expiró y la IP se reasignó a otro
# - La configuración del servidor cambió

# Tras recibir NAK, el cliente reinicia DORA normalmente
# y obtiene una nueva IP válida
```

### Error 5 — APIPA (169.254.x.x) interpretado como problema DHCP

```bash
# 169.254.x.x (APIPA) significa que el cliente NO recibió respuesta DHCP
# El cliente se auto-asigna una IP link-local

# Puede NO ser culpa del servidor DHCP:
# 1. Cable desconectado
# 2. Switch port en VLAN incorrecta
# 3. Firewall bloqueando broadcast entre segmentos
# 4. Servidor DHCP caído o no configurado para esa subred

# Diagnóstico: tcpdump en el SERVIDOR
# Si el DISCOVER no llega al servidor → problema de red/VLAN
# Si el DISCOVER llega pero no hay OFFER → config del servidor
```

---

## 8. Cheatsheet

```bash
# ── Logs del servidor ─────────────────────────────────────
journalctl -u isc-dhcp-server -f     # Logs en vivo (Debian)
journalctl -u dhcpd -f               # Logs en vivo (RHEL)
grep DHCPDISCOVER /var/log/dhcpd.log # Nuevos clientes
grep "no free leases" /var/log/...   # Pool agotado
grep DHCPNAK /var/log/...            # Leases rechazados
grep abandoned /var/log/...          # Conflictos de IP

# ── tcpdump — capturar DORA ──────────────────────────────
tcpdump -i eth0 port 67 or 68 -n -vv     # Captura verbose
tcpdump -i eth0 port 67 or 68 -n -vve    # + MACs
tcpdump -i eth0 port 67 or 68 -w f.pcap  # Guardar para Wireshark
tcpdump -i eth0 port 67 or 68 -c 4 -n -vv # Solo 4 paquetes (1 DORA)
tcpdump -i any port 67 or 68 -n -vv       # Todas las interfaces

# ── Cliente ───────────────────────────────────────────────
dhclient -v eth0                     # Solicitar IP (verbose)
dhclient -r eth0                     # Liberar IP
nmcli connection down ETH && nmcli connection up ETH  # NM
nmcli -f DHCP4 device show eth0      # Ver lease actual
nmap --script broadcast-dhcp-discover -e eth0  # Descubrir servers

# ── Leases ────────────────────────────────────────────────
grep -c "binding state active" /var/lib/dhcp/dhcpd.leases  # Contar
grep -B1 -A7 "active" /var/lib/dhcp/dhcpd.leases          # Ver activos
grep "00:11:22:33:44:55" /var/lib/dhcp/dhcpd.leases        # Por MAC
grep "binding state" ... | sort | uniq -c                  # Resumen

# ── Patrones de log DORA ──────────────────────────────────
# DHCPDISCOVER from MAC via IFACE       → Cliente busca
# DHCPOFFER on IP to MAC via IFACE      → Servidor ofrece
# DHCPREQUEST for IP from MAC via IFACE → Cliente acepta
# DHCPACK on IP to MAC (host) via IFACE → Confirmado ✓
# Renovación: solo REQUEST + ACK (sin DISCOVER/OFFER)

# ── Diagnóstico rápido ───────────────────────────────────
# ¿Servicio corriendo?   systemctl status isc-dhcp-server
# ¿Config válida?         dhcpd -t -cf /etc/dhcp/dhcpd.conf
# ¿Pool agotado?          grep "no free leases" logs
# ¿Conflicto de IP?       arping -D -I eth0 IP
# ¿DHCP rogue?            nmap --script broadcast-dhcp-discover
# ¿Broadcast llega?       tcpdump -i eth0 port 67 (en servidor)
```

---

## 9. Ejercicios

### Ejercicio 1 — Interpretar una captura tcpdump

Analiza la siguiente captura y responde las preguntas.

```
10:00:01.001 0.0.0.0.68 > 255.255.255.255.67: BOOTP/DHCP, Request
  DHCP-Message: Discover, Client-ID: 00:aa:bb:cc:dd:ee
  Hostname: "pc-nuevo"

10:00:01.003 192.168.1.10.67 > 192.168.1.150.68: BOOTP/DHCP, Reply
  DHCP-Message: Offer, Server-ID: 192.168.1.10
  Offered-IP: 192.168.1.150, Lease-Time: 3600
  Subnet: 255.255.255.0, Router: 192.168.1.1, DNS: 8.8.8.8

10:00:01.004 192.168.1.20.67 > 192.168.1.160.68: BOOTP/DHCP, Reply
  DHCP-Message: Offer, Server-ID: 192.168.1.20
  Offered-IP: 192.168.1.160, Lease-Time: 7200
  Subnet: 255.255.255.0, Router: 192.168.1.1, DNS: 10.0.0.53

10:00:01.005 0.0.0.0.68 > 255.255.255.255.67: BOOTP/DHCP, Request
  DHCP-Message: Request, Requested-IP: 192.168.1.150
  Server-ID: 192.168.1.10

10:00:01.006 192.168.1.10.67 > 192.168.1.150.68: BOOTP/DHCP, Reply
  DHCP-Message: ACK, Lease-Time: 3600
```

**Pregunta de predicción:** ¿Cuántos servidores DHCP respondieron? ¿Cuál eligió el cliente y cómo lo sabemos? ¿El segundo servidor (192.168.1.20) es un problema?

> **Respuesta:** Respondieron **dos** servidores: `192.168.1.10` (ofreció .150) y `192.168.1.20` (ofreció .160). El cliente eligió `192.168.1.10` — lo sabemos porque el DHCPREQUEST incluye `Server-ID: 192.168.1.10` y `Requested-IP: 192.168.1.150`. El segundo servidor **es potencialmente un problema**: podría ser un DHCP rogue (servidor no autorizado que ofrece DNS malicioso `10.0.0.53`), o un servidor legítimo mal configurado con rangos solapados. Debe investigarse quién controla `192.168.1.20`.

---

### Ejercicio 2 — Diagnosticar "no free leases"

El log muestra: `DHCPDISCOVER from 00:11:22:33:44:55 via eth0: no free leases`. Investiga y resuelve.

```bash
# 1. ¿Cuántos leases activos hay?
grep -c "binding state active" /var/lib/dhcp/dhcpd.leases
# 101

# 2. ¿Cuántas IPs tiene el rango?
grep "range" /etc/dhcp/dhcpd.conf
# range 192.168.1.100 192.168.1.200;
# → 101 IPs — el pool está lleno al 100%

# 3. ¿Hay leases de equipos que ya no están en la red?
# Buscar leases con ends en el pasado pero aún marcados active
# (esto no debería pasar, pero a veces el archivo necesita limpieza)

# 4. Soluciones (elegir según contexto):
# a) Reducir lease time para reciclar IPs más rápido
#    default-lease-time 1800;
#
# b) Ampliar el rango
#    range 192.168.1.50 192.168.1.250;
#
# c) Mover equipos con IP fija a reservaciones fuera del rango
#
# d) Reiniciar dhcpd para limpiar leases expirados del archivo
sudo systemctl restart isc-dhcp-server
```

**Pregunta de predicción:** Si reduces `default-lease-time` de 24 horas a 30 minutos, ¿los 101 clientes actuales liberan sus IPs inmediatamente?

> **Respuesta:** No. Los leases actuales conservan su tiempo original (24 horas) hasta que expiren o se renueven. Cuando cada cliente renueve (al 50% de su lease actual), recibirá el nuevo lease de 30 minutos. El efecto es gradual: a lo largo de las próximas ~12 horas, todos los clientes migrarán al lease corto. Mientras tanto, el pool sigue lleno. Para liberar IPs inmediatamente, habría que reiniciar el servicio (pero eso no invalida los leases existentes — los clientes seguirán usando sus IPs hasta que intenten renovar).

---

### Ejercicio 3 — Workflow completo de diagnóstico

Un usuario reporta que su equipo no obtiene IP. Sigue el procedimiento de diagnóstico completo usando todos los niveles.

```bash
# ── Nivel 1: En el CLIENTE ────────────────────────────────

# ¿Tiene conexión física?
ip link show eth0
# state UP → capa 2 OK

# ¿Qué IP tiene actualmente?
ip addr show eth0
# 169.254.x.x → APIPA, no recibió DHCP

# ¿Sale tráfico DHCP?
sudo tcpdump -i eth0 port 67 or port 68 -c 4 -n -vv &
sudo dhclient -v eth0
# Si vemos DISCOVER pero no OFFER → problema en servidor o red

# ── Nivel 2: En la RED ───────────────────────────────────

# ¿Hay un relay configurado? (si el servidor está en otra subred)
# Verificar en el router/switch

# ── Nivel 3: En el SERVIDOR ──────────────────────────────

# ¿El servicio está activo?
sudo systemctl status isc-dhcp-server

# ¿La configuración es válida?
sudo dhcpd -t -cf /etc/dhcp/dhcpd.conf

# ¿Ve los DISCOVER del cliente?
sudo journalctl -u isc-dhcp-server --since "5 minutes ago" | grep DISCOVER

# ¿Hay IPs disponibles?
sudo journalctl -u isc-dhcp-server --since "5 minutes ago" | grep "no free"

# ¿El firewall permite DHCP?
sudo ss -ulnp | grep :67
sudo firewall-cmd --list-services | grep dhcp
```

**Pregunta de predicción:** El DISCOVER aparece en los logs del servidor pero no se ve ningún OFFER. La configuración pasa `dhcpd -t`. ¿Cuáles son las dos causas más probables?

> **Respuesta:** 1) **Pool agotado** — todas las IPs del rango están asignadas, así que el servidor no puede ofrecer ninguna (buscar "no free leases" en los logs). 2) **El cliente está bloqueado por política** — si hay `deny unknown-clients` en la configuración o un `pool` con `allow` restrictivo, el servidor descarta el DISCOVER silenciosamente sin enviar OFFER. Verificar con `grep -E "deny|allow" /etc/dhcp/dhcpd.conf`.

---

> **Fin de la Sección 1 — DHCP Server.** Siguiente sección: S02 — Email (T01 — Conceptos: MTA, MDA, MUA, SMTP, POP3, IMAP).
