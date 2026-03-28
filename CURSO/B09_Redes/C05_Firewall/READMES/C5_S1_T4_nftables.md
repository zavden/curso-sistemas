# nftables

## Índice

1. [Por qué nftables reemplaza a iptables](#1-por-qué-nftables-reemplaza-a-iptables)
2. [Arquitectura de nftables](#2-arquitectura-de-nftables)
3. [Sintaxis fundamental](#3-sintaxis-fundamental)
4. [Familias, tablas y cadenas](#4-familias-tablas-y-cadenas)
5. [Reglas y expresiones](#5-reglas-y-expresiones)
6. [Sets y maps](#6-sets-y-maps)
7. [NAT con nftables](#7-nat-con-nftables)
8. [Connection tracking en nftables](#8-connection-tracking-en-nftables)
9. [Scripting y archivos de configuración](#9-scripting-y-archivos-de-configuración)
10. [Migración desde iptables](#10-migración-desde-iptables)
11. [nftables en distribuciones actuales](#11-nftables-en-distribuciones-actuales)
12. [Diagnóstico y depuración](#12-diagnóstico-y-depuración)
13. [Errores comunes](#13-errores-comunes)
14. [Cheatsheet](#14-cheatsheet)
15. [Ejercicios](#15-ejercicios)

---

## 1. Por qué nftables reemplaza a iptables

iptables cumplió su función durante dos décadas, pero acumuló limitaciones
estructurales que no podían resolverse sin rediseñar el sistema:

### Problemas de iptables

| Problema | Detalle |
|----------|---------|
| **Código duplicado** | iptables, ip6tables, arptables, ebtables — cuatro herramientas separadas con bases de código casi idénticas |
| **Rendimiento lineal** | Cada paquete recorre la lista de reglas secuencialmente; con miles de reglas (ej. listas de bloqueo), el rendimiento degrada |
| **Actualizaciones atómicas** | `iptables-restore` reemplaza *todas* las reglas; no hay forma de actualizar una sola regla sin tocar las demás |
| **Extensiones en kernel** | Cada módulo de match (conntrack, multiport, limit…) requiere código kernel; añadir funcionalidad exige un módulo nuevo |
| **Sintaxis inconsistente** | Cada extensión tiene sus propias flags (`-m conntrack --ctstate` vs `-m limit --limit`) |

### Qué resuelve nftables

```
iptables (2001)                          nftables (2014, kernel 3.13+)
────────────────                         ─────────────────────────────
4 herramientas separadas          →      1 herramienta: nft
Tablas/cadenas fijas              →      Tablas/cadenas definidas por el usuario
Matches como módulos kernel       →      Máquina virtual en kernel (nf_tables)
Evaluación lineal                 →      Sets con lookup O(1) (hash/rbtree)
iptables-save/restore             →      Formato de archivo nativo + atómico
Solo IPv4 o solo IPv6             →      Familia inet = IPv4 + IPv6 juntos
```

### Línea temporal de adopción

```
2014    nftables entra en kernel 3.13
2017    Debian 10 (Buster) lo incluye como alternativa
2019    RHEL 8 / CentOS 8: nftables es el backend por defecto
        (firewalld usa nftables, iptables es wrapper de compatibilidad)
2021    Debian 11 (Bullseye): nftables por defecto
2022+   La mayoría de distribuciones modernas usan nftables
        iptables legacy sigue disponible pero desaconsejado
```

**Situación actual**: en distribuciones modernas, cuando ejecutas `iptables`, en
realidad estás ejecutando `iptables-nft` — un wrapper que traduce la sintaxis de
iptables a reglas nftables en el kernel. El binario legacy (`iptables-legacy`)
existe pero está en vías de desaparecer.

```bash
# Verificar qué versión de iptables tienes
iptables -V
# iptables v1.8.9 (nf_tables)   ← iptables-nft (wrapper)
# iptables v1.8.9 (legacy)      ← iptables legacy

# Ver la alternativa activa
update-alternatives --query iptables   # Debian/Ubuntu
alternatives --display iptables        # RHEL/Fedora
```

---

## 2. Arquitectura de nftables

### Componentes

```
┌─────────────────────────────────────────────────────────┐
│                     Espacio de usuario                  │
│                                                         │
│  ┌─────────┐   ┌───────────────┐   ┌────────────────┐  │
│  │  nft     │   │ iptables-nft  │   │  firewalld     │  │
│  │ (native) │   │  (compat)     │   │  (frontend)    │  │
│  └────┬─────┘   └──────┬────────┘   └──────┬─────────┘  │
│       │                │                    │            │
│       └────────────────┼────────────────────┘            │
│                        │                                 │
│                   libnftnl                               │
│                   (librería)                             │
│                        │                                 │
│                    netlink                               │
│                   (socket)                               │
└────────────────────────┼────────────────────────────────┘
                         │
┌────────────────────────┼────────────────────────────────┐
│                        ▼              Espacio de kernel  │
│                                                         │
│              ┌──────────────────┐                        │
│              │    nf_tables     │                        │
│              │  (máquina virtual│                        │
│              │   + bytecode)    │                        │
│              └──────────────────┘                        │
│                        │                                 │
│              ┌──────────────────┐                        │
│              │    Netfilter     │                        │
│              │    (hooks)       │                        │
│              └──────────────────┘                        │
└─────────────────────────────────────────────────────────┘
```

La diferencia clave: en iptables, cada match y target es un módulo kernel separado.
En nftables, el kernel contiene una **máquina virtual** que ejecuta bytecode. Cuando
escribes una regla `nft`, la herramienta la compila a bytecode y lo envía al kernel
vía netlink. Esto significa:

- Nuevas funcionalidades no requieren módulos kernel nuevos
- Las reglas se pueden optimizar antes de enviarlas al kernel
- Una sola infraestructura sirve para IPv4, IPv6, ARP y bridging

### Jerarquía de objetos

```
Familia (ip, ip6, inet, arp, bridge, netdev)
  └── Tabla (nombre libre, creada por el usuario)
        ├── Cadena (nombre libre, tipo + hook + prioridad)
        │     ├── Regla 1
        │     ├── Regla 2
        │     └── Regla N
        ├── Set (conjunto de elementos para lookup rápido)
        ├── Map (mapeo clave → valor)
        └── Flowtable (aceleración de paquetes)
```

A diferencia de iptables (tablas fijas: filter, nat, mangle, raw), en nftables
**tú defines las tablas y cadenas** con los nombres que quieras. El comportamiento
se determina por el `type` y `hook` de la cadena.

---

## 3. Sintaxis fundamental

### Formato general de un comando nft

```
nft  [opciones]  operación  familia  objeto  [especificador]  [definición]
```

```bash
# Ejemplos de estructura:
nft  add        table   inet    my_filter
nft  add        chain   inet    my_filter    input    { type filter hook input priority 0 \; }
nft  add        rule    inet    my_filter    input    tcp dport 22 accept
nft  list       ruleset
nft  delete     rule    inet    my_filter    input    handle 7
```

### Operaciones principales

| Operación | Descripción |
|-----------|-------------|
| `add` | Añadir (tabla, cadena, regla, set…) |
| `create` | Crear (falla si ya existe, a diferencia de `add`) |
| `insert` | Insertar regla al principio de la cadena |
| `delete` | Eliminar por handle o especificador |
| `replace` | Reemplazar una regla por handle |
| `list` | Listar (tabla, cadena, ruleset, sets…) |
| `flush` | Vaciar (tabla, cadena, ruleset) |

### Diferencia clave: add vs insert

```bash
# add: añade al FINAL de la cadena
nft add rule inet my_filter input tcp dport 80 accept

# insert: añade al PRINCIPIO de la cadena
nft insert rule inet my_filter input tcp dport 443 accept

# add con posición: después de un handle específico
nft add rule inet my_filter input handle 5 tcp dport 8080 accept

# insert con posición: antes de un handle específico
nft insert rule inet my_filter input handle 5 tcp dport 8443 accept
```

### Formato inline vs bloque

nft acepta comandos individuales o bloques delimitados por llaves:

```bash
# Comando individual
nft add table inet my_filter
nft add chain inet my_filter input '{ type filter hook input priority 0; }'

# Bloque (más legible, se usa en archivos)
nft -f - <<'EOF'
table inet my_filter {
    chain input {
        type filter hook input priority 0; policy drop;

        iif lo accept
        ct state established,related accept
        tcp dport 22 accept
    }
}
EOF
```

**Predicción**: el punto y coma dentro de llaves en la shell debe escaparse
(`\;` o con comillas) para evitar que el shell lo interprete como separador
de comandos.

---

## 4. Familias, tablas y cadenas

### Familias

| Familia | Equivalente iptables | Descripción |
|---------|---------------------|-------------|
| `ip` | iptables | Solo IPv4 |
| `ip6` | ip6tables | Solo IPv6 |
| `inet` | *(no existe)* | IPv4 + IPv6 en las mismas reglas |
| `arp` | arptables | Protocolo ARP |
| `bridge` | ebtables | Frames de capa 2 |
| `netdev` | *(no existe)* | Ingress hook (antes de cualquier procesamiento) |

La familia `inet` es la gran novedad: **una sola regla aplica a IPv4 e IPv6**
simultáneamente. Esto elimina la necesidad de mantener reglas duplicadas.

```bash
# Con iptables necesitabas:
iptables -A INPUT -p tcp --dport 22 -j ACCEPT
ip6tables -A INPUT -p tcp --dport 22 -j ACCEPT

# Con nftables (familia inet):
nft add rule inet my_filter input tcp dport 22 accept
# ¡Una sola regla para ambos protocolos!
```

### Tablas

Las tablas son contenedores con nombre libre. Pertenecen a una familia:

```bash
# Crear tablas
nft add table inet firewall
nft add table inet my_nat
nft add table netdev early_drop

# Listar tablas
nft list tables

# Vaciar una tabla (elimina todas las reglas pero mantiene cadenas)
nft flush table inet firewall

# Eliminar una tabla (y todo su contenido)
nft delete table inet firewall
```

### Cadenas

Las cadenas tienen dos categorías:

**Cadenas base** (enganchadas a Netfilter):

```bash
nft add chain inet firewall input {
    type filter hook input priority 0 \; policy drop \;
}
```

Parámetros obligatorios:
- **type**: `filter`, `nat`, o `route` (equivalente a mangle)
- **hook**: `prerouting`, `input`, `forward`, `output`, `postrouting` (más `ingress` para netdev)
- **priority**: número que determina el orden de evaluación entre cadenas del mismo hook

**Cadenas regulares** (llamadas explícitamente, como las user-defined de iptables):

```bash
nft add chain inet firewall tcp_checks
# No tiene type/hook/priority → solo se invoca con "jump" o "goto"
```

### Prioridades

La prioridad determina qué cadena se evalúa primero en el mismo hook. Valores
más bajos = antes:

```
Prioridad    Nombre simbólico    Equivalencia iptables
─────────    ────────────────    ──────────────────────
-400         raw                 tabla raw
-300         mangle              tabla mangle (PREROUTING)
-200         dstnat              tabla nat (PREROUTING/DNAT)
-150         ─                   ─
-100         ─                   ─
  0          filter              tabla filter
 100         security            tabla security
 200         ─                   ─
 300         srcnat              tabla nat (POSTROUTING/SNAT)
```

```bash
# Cadena tipo NAT con DNAT (prerouting, prioridad dstnat = -200)
nft add chain inet my_nat prerouting {
    type nat hook prerouting priority dstnat \; policy accept \;
}

# Cadena tipo NAT con SNAT (postrouting, prioridad srcnat = 300)
nft add chain inet my_nat postrouting {
    type nat hook postrouting priority srcnat \; policy accept \;
}
```

---

## 5. Reglas y expresiones

### Anatomía de una regla nft

```
nft add rule [familia] [tabla] [cadena]   [expresiones de match]   [veredicto]
                                          ─────────────────────    ──────────
                                          Qué paquetes             Qué hacer
```

### Matches comunes

```bash
# Protocolo
tcp dport 22                    # puerto destino TCP
udp sport 53                    # puerto origen UDP
tcp dport { 80, 443, 8080 }    # múltiples puertos (set anónimo)
tcp dport 1024-65535            # rango de puertos

# Direcciones IP
ip saddr 192.168.1.0/24        # red origen IPv4
ip daddr 10.0.0.100            # destino IPv4 específico
ip6 saddr fd00::/64            # red origen IPv6

# Interfaces
iif eth0                       # interfaz de entrada (por índice, inmutable)
iifname "eth0"                 # interfaz de entrada (por nombre, hotplug)
oif eth1                       # interfaz de salida
oifname "wlan*"                # patrón con wildcard

# Meta
meta l4proto tcp               # protocolo capa 4
meta iiftype loopback          # tipo de interfaz
meta nfproto ipv4              # familia del paquete (útil en inet)
meta mark 0x1                  # fwmark del paquete
```

**Diferencia importante**: `iif`/`oif` resuelven el nombre a un índice al cargar
la regla. Si la interfaz no existe en ese momento, la regla falla. Usa `iifname`/
`oifname` para interfaces que pueden no existir al arranque (VPN, USB).

### Veredictos (targets)

```bash
accept                         # aceptar el paquete
drop                           # descartar silenciosamente
reject                         # rechazar con ICMP/TCP-RST
reject with icmp type host-unreachable
reject with tcp reset
queue                          # enviar a espacio de usuario (NFQUEUE)
jump other_chain               # saltar a otra cadena (retorna)
goto other_chain               # saltar a otra cadena (no retorna)
return                         # volver a la cadena anterior
log prefix "DROPPED: "         # registrar y continuar evaluando
counter                        # contar y continuar evaluando
```

### Composición de reglas

En nftables, múltiples expresiones en una regla actúan como AND implícito:

```bash
# Paquetes TCP, desde 192.168.1.0/24, al puerto 22 → aceptar
# (equivale a: -s 192.168.1.0/24 -p tcp --dport 22 -j ACCEPT)
nft add rule inet firewall input \
    ip saddr 192.168.1.0/24 tcp dport 22 accept
```

### Contadores

A diferencia de iptables, nftables **no cuenta paquetes por defecto** (menos
overhead). Añade `counter` explícitamente donde lo necesites:

```bash
# Con contador
nft add rule inet firewall input tcp dport 22 counter accept

# Listar con contadores
nft list chain inet firewall input
# tcp dport 22 counter packets 847 bytes 52114 accept
```

---

## 6. Sets y maps

Los sets son una de las mejores mejoras de nftables sobre iptables. Permiten
agrupar elementos (IPs, puertos, interfaces) en estructuras de datos con
búsqueda O(1) en lugar de evaluar regla por regla.

### Sets anónimos (inline)

```bash
# Múltiples puertos (equivalente a -m multiport)
nft add rule inet firewall input tcp dport { 22, 80, 443 } accept

# Múltiples IPs
nft add rule inet firewall input ip saddr { 10.0.0.1, 10.0.0.2, 10.0.0.3 } accept

# Rangos
nft add rule inet firewall input tcp dport { 1024-65535 } accept
```

### Sets con nombre (dinámicos)

```bash
# Crear un set de IPs
nft add set inet firewall blocked_ips {
    type ipv4_addr \;
    comment \"IPs bloqueadas\" \;
}

# Añadir elementos al set
nft add element inet firewall blocked_ips { 203.0.113.50, 198.51.100.99 }

# Usar el set en una regla (@ referencia un set con nombre)
nft add rule inet firewall input ip saddr @blocked_ips drop

# Eliminar un elemento
nft delete element inet firewall blocked_ips { 203.0.113.50 }

# Listar el set
nft list set inet firewall blocked_ips
```

### Sets con flags

```bash
# Set con timeout (elementos expiran automáticamente)
nft add set inet firewall recent_ssh {
    type ipv4_addr \;
    flags timeout \;
    timeout 5m \;
}

# Set dinámico (las reglas pueden añadir elementos)
nft add set inet firewall rate_limited {
    type ipv4_addr \;
    flags dynamic,timeout \;
    timeout 1m \;
}

# Regla que añade IPs al set dinámicamente
nft add rule inet firewall input tcp dport 22 \
    ct state new \
    add @rate_limited { ip saddr } \
    accept

# Set con intervalo (para rangos y subredes)
nft add set inet firewall internal_nets {
    type ipv4_addr \;
    flags interval \;
}
nft add element inet firewall internal_nets { 10.0.0.0/8, 172.16.0.0/12, 192.168.0.0/16 }
```

### Sets concatenados

Permiten crear tuplas (combinaciones de campos):

```bash
# Set de pares IP + puerto
nft add set inet firewall allowed_services {
    type ipv4_addr . inet_proto . inet_service \;
}
nft add element inet firewall allowed_services {
    192.168.1.100 . tcp . 80,
    192.168.1.100 . tcp . 443,
    192.168.1.101 . tcp . 22
}

# Uso: match con operador concatenación
nft add rule inet firewall forward \
    ip daddr . meta l4proto . tcp dport @allowed_services accept
```

### Maps (verdict maps)

Los maps asocian una clave con un veredicto o un valor:

```bash
# Verdict map: acción según el puerto
nft add rule inet firewall input tcp dport vmap {
    22 : accept,
    80 : accept,
    443 : accept,
    3306 : drop
}

# Map de redirección (DNAT por puerto)
nft add map inet my_nat port_redirect {
    type inet_service : ipv4_addr \;
}
nft add element inet my_nat port_redirect {
    80 : 192.168.1.100,
    443 : 192.168.1.100,
    25 : 192.168.1.101
}
nft add rule inet my_nat prerouting dnat to tcp dport map @port_redirect
```

### Comparación de rendimiento: sets vs reglas lineales

```
Escenario: bloquear 10.000 IPs

iptables (lineal):
  10.000 reglas -s <IP> -j DROP
  → Cada paquete se compara con las 10.000 reglas: O(n)

nftables con set:
  1 set con 10.000 elementos + 1 regla
  → Búsqueda hash: O(1)
  → ~1000x más rápido con 10.000 entradas
```

---

## 7. NAT con nftables

La sintaxis NAT en nftables es más limpia y consistente que en iptables.

### Configurar tabla NAT

```bash
# Crear tabla y cadenas NAT
nft add table inet my_nat

nft add chain inet my_nat prerouting {
    type nat hook prerouting priority dstnat \; policy accept \;
}

nft add chain inet my_nat postrouting {
    type nat hook postrouting priority srcnat \; policy accept \;
}
```

### SNAT / MASQUERADE

```bash
# SNAT con IP fija
nft add rule inet my_nat postrouting \
    oifname "eth1" ip saddr 192.168.1.0/24 \
    snat to 198.51.100.1

# MASQUERADE (IP dinámica)
nft add rule inet my_nat postrouting \
    oifname "eth1" ip saddr 192.168.1.0/24 \
    masquerade

# MASQUERADE con rango de puertos
nft add rule inet my_nat postrouting \
    oifname "eth1" ip saddr 192.168.1.0/24 \
    masquerade to :10000-65535
```

### DNAT

```bash
# Port forwarding: puerto 80 a servidor interno
nft add rule inet my_nat prerouting \
    iifname "eth1" tcp dport 80 \
    dnat to 192.168.1.100:80

# Con cambio de puerto
nft add rule inet my_nat prerouting \
    iifname "eth1" tcp dport 2222 \
    dnat to 192.168.1.101:22

# DNAT con map (múltiples puertos a distintos servidores)
nft add map inet my_nat dnat_map {
    type inet_service : ipv4_addr . inet_service \;
}
nft add element inet my_nat dnat_map {
    80 : 192.168.1.100 . 80,
    443 : 192.168.1.100 . 443,
    25 : 192.168.1.101 . 25
}
nft add rule inet my_nat prerouting \
    iifname "eth1" dnat to tcp dport map @dnat_map
```

### REDIRECT

```bash
# Proxy transparente
nft add rule inet my_nat prerouting \
    iifname "eth0" tcp dport 80 \
    redirect to :3128
```

### Comparación directa iptables → nftables NAT

```bash
# iptables:
iptables -t nat -A POSTROUTING -s 192.168.1.0/24 -o eth1 -j MASQUERADE
iptables -t nat -A PREROUTING -i eth1 -p tcp --dport 80 \
    -j DNAT --to-destination 192.168.1.100:80

# nftables equivalente:
nft add rule inet my_nat postrouting oifname "eth1" ip saddr 192.168.1.0/24 masquerade
nft add rule inet my_nat prerouting iifname "eth1" tcp dport 80 dnat to 192.168.1.100:80
```

---

## 8. Connection tracking en nftables

### Sintaxis ct (conntrack)

```bash
# Estados de conexión (reemplaza -m conntrack --ctstate)
ct state established,related accept
ct state new accept
ct state invalid drop

# Dirección original de conntrack
ct original ip saddr 192.168.1.0/24

# Marcar conexiones
ct mark set 0x1
ct mark 0x1 accept

# Helpers (FTP, SIP, etc.)
ct helper set "ftp"
```

### Ejemplo: firewall stateful completo

```bash
nft -f - <<'EOF'
table inet firewall {
    chain input {
        type filter hook input priority 0; policy drop;

        # Conexiones establecidas
        ct state established,related accept

        # Inválidas
        ct state invalid drop

        # Loopback
        iif lo accept

        # ICMP (ping)
        meta l4proto icmp accept
        meta l4proto icmpv6 accept

        # SSH
        tcp dport 22 ct state new accept

        # HTTP/HTTPS
        tcp dport { 80, 443 } ct state new accept

        # Logging de lo demás
        log prefix "INPUT_DROP: " counter drop
    }

    chain forward {
        type filter hook forward priority 0; policy drop;

        ct state established,related accept
        ct state invalid drop

        # LAN → WAN
        iifname "eth0" oifname "eth1" accept

        log prefix "FORWARD_DROP: " counter drop
    }

    chain output {
        type filter hook output priority 0; policy accept;
    }
}
EOF
```

---

## 9. Scripting y archivos de configuración

### Formato de archivo nftables

nftables tiene un formato de archivo nativo, legible y estructurado:

```bash
# /etc/nftables.conf — archivo de configuración principal
```

```
#!/usr/sbin/nft -f

# Limpiar todo al inicio
flush ruleset

# Variables (define)
define LAN_NET = 192.168.1.0/24
define WAN_IF  = "eth1"
define LAN_IF  = "eth0"
define DNS_SERVERS = { 1.1.1.1, 8.8.8.8 }
define WEB_PORTS = { 80, 443 }

table inet firewall {

    set blocked_ips {
        type ipv4_addr
        flags interval
        elements = { 203.0.113.0/24, 198.51.100.0/24 }
    }

    chain input {
        type filter hook input priority 0; policy drop;

        ct state established,related accept
        ct state invalid drop

        iif lo accept

        # Anti-spoofing
        iifname $WAN_IF ip saddr $LAN_NET drop

        # Bloquear IPs del set
        ip saddr @blocked_ips drop

        # Servicios permitidos
        iifname $LAN_IF tcp dport $WEB_PORTS accept
        iifname $LAN_IF tcp dport 22 accept

        # ICMP
        meta l4proto { icmp, icmpv6 } accept

        counter drop
    }

    chain forward {
        type filter hook forward priority 0; policy drop;

        ct state established,related accept
        ct state invalid drop

        # LAN → WAN
        iifname $LAN_IF oifname $WAN_IF accept

        counter drop
    }

    chain output {
        type filter hook output priority 0; policy accept;
    }
}

table inet my_nat {
    chain postrouting {
        type nat hook postrouting priority srcnat; policy accept;
        oifname $WAN_IF ip saddr $LAN_NET masquerade
    }
}
```

### Variables con define

```bash
define my_ip = 192.168.1.1
define my_nets = { 10.0.0.0/8, 172.16.0.0/12, 192.168.0.0/16 }
define ssh_port = 22

# Uso:
ip saddr $my_ip accept
ip saddr $my_nets accept
tcp dport $ssh_port accept
```

### Include

```bash
# En /etc/nftables.conf:
include "/etc/nftables.d/*.nft"

# Esto permite modularizar:
# /etc/nftables.d/sets.nft      — definiciones de sets
# /etc/nftables.d/input.nft     — reglas de input
# /etc/nftables.d/forward.nft   — reglas de forward
```

### Aplicar y persistir

```bash
# Aplicar archivo
nft -f /etc/nftables.conf

# Verificar sintaxis sin aplicar
nft -c -f /etc/nftables.conf

# Exportar ruleset actual a formato de archivo
nft list ruleset > /etc/nftables.conf

# Servicio systemd
systemctl enable nftables        # cargar al arranque
systemctl start nftables         # aplicar ahora
systemctl restart nftables       # recargar configuración
```

### Atomicidad

Una ventaja fundamental de nftables: al cargar un archivo con `nft -f`, **todo
se aplica atómicamente**. O se aplican todas las reglas o ninguna. No hay ventana
de tiempo donde el firewall esté "a medio configurar".

```bash
# Con iptables-restore: lectura línea a línea, si falla a mitad
# el firewall queda parcialmente configurado

# Con nft -f: el archivo se compila completo, se envía como una
# transacción netlink, y el kernel lo aplica de una vez
```

---

## 10. Migración desde iptables

### Herramienta automática: iptables-translate

```bash
# Traducir una regla individual
iptables-translate -A INPUT -p tcp --dport 22 -j ACCEPT
# nft add rule ip filter INPUT tcp dport 22 counter accept

iptables-translate -A INPUT -s 192.168.1.0/24 -p tcp \
    -m multiport --dports 80,443 -j ACCEPT
# nft add rule ip filter INPUT ip saddr 192.168.1.0/24 \
#     tcp dport { 80, 443 } counter accept

# Traducir un archivo iptables-save completo
iptables-restore-translate -f /etc/iptables/rules.v4
```

### Tabla de equivalencias

```
iptables                              nftables
──────────────────────────────────    ─────────────────────────────────
-A INPUT                              add rule inet t input
-I INPUT 1                            insert rule inet t input
-D INPUT 3                            delete rule inet t input handle N
-p tcp --dport 22                     tcp dport 22
-p tcp -m multiport --dports 80,443   tcp dport { 80, 443 }
-s 192.168.1.0/24                     ip saddr 192.168.1.0/24
-d 10.0.0.1                           ip daddr 10.0.0.1
-i eth0                               iifname "eth0"
-o eth1                               oifname "eth1"
-m conntrack --ctstate NEW            ct state new
-m limit --limit 3/min               limit rate 3/minute
-m recent --name ssh --set            add @ssh_recent { ip saddr }
-j ACCEPT                             accept
-j DROP                               drop
-j REJECT                             reject
-j LOG --log-prefix "X: "             log prefix "X: "
-j SNAT --to-source IP                snat to IP
-j DNAT --to-destination IP:PORT      dnat to IP:PORT
-j MASQUERADE                         masquerade
-j REDIRECT --to-ports PORT           redirect to :PORT
-N my_chain                           add chain inet t my_chain
-j my_chain                           jump my_chain
-P INPUT DROP                         ... policy drop;
```

### Ejemplo de migración completa

**iptables original**:
```bash
iptables -P INPUT DROP
iptables -P FORWARD DROP
iptables -P OUTPUT ACCEPT
iptables -A INPUT -i lo -j ACCEPT
iptables -A INPUT -m conntrack --ctstate ESTABLISHED,RELATED -j ACCEPT
iptables -A INPUT -p tcp --dport 22 -j ACCEPT
iptables -A INPUT -p tcp -m multiport --dports 80,443 -j ACCEPT
iptables -A INPUT -p icmp -j ACCEPT
iptables -A INPUT -j LOG --log-prefix "DROPPED: "
iptables -t nat -A POSTROUTING -s 192.168.1.0/24 -o eth1 -j MASQUERADE
```

**nftables equivalente**:
```
#!/usr/sbin/nft -f
flush ruleset

table inet firewall {
    chain input {
        type filter hook input priority 0; policy drop;

        iif lo accept
        ct state established,related accept
        tcp dport 22 accept
        tcp dport { 80, 443 } accept
        meta l4proto icmp accept
        log prefix "DROPPED: " drop
    }

    chain forward {
        type filter hook forward priority 0; policy drop;
    }

    chain output {
        type filter hook output priority 0; policy accept;
    }
}

table inet my_nat {
    chain postrouting {
        type nat hook postrouting priority srcnat; policy accept;
        oifname "eth1" ip saddr 192.168.1.0/24 masquerade
    }
}
```

### Coexistencia durante migración

**No mezcles reglas iptables-legacy y nft** en el mismo sistema. Usan APIs
kernel diferentes y no se ven entre sí. Sin embargo, `iptables-nft` (el wrapper)
sí coexiste con `nft` porque ambos usan la API nftables:

```bash
# Ver qué reglas existen en cada subsistema
iptables-legacy-save    # reglas legacy (si las hay)
iptables-nft-save       # reglas iptables-nft (traducidas a nftables)
nft list ruleset        # todas las reglas nftables (incluye las de iptables-nft)
```

**Predicción**: si ves reglas duplicadas o conflictivas, posiblemente tienes un
servicio usando iptables-nft y otro cargando reglas con nft directamente. Ambas
aparecen en `nft list ruleset`.

---

## 11. nftables en distribuciones actuales

### Debian / Ubuntu

```bash
# Instalación
apt install nftables

# Archivo de configuración
/etc/nftables.conf

# Servicio
systemctl enable nftables
systemctl start nftables

# Debian 11+: nftables es el backend por defecto
# iptables es un alias a iptables-nft
```

### RHEL / Fedora / AlmaLinux

```bash
# Ya instalado por defecto en RHEL 8+
# firewalld usa nftables como backend

# Si quieres nft directamente (sin firewalld):
systemctl stop firewalld
systemctl disable firewalld
systemctl enable nftables
systemctl start nftables

# Archivo de configuración
/etc/sysconfig/nftables.conf
# Este archivo incluye:
# include "/etc/nftables/main.nft"
```

### Interacción con firewalld

firewalld (tema siguiente) usa nftables como backend desde RHEL 8 / Fedora 18+.
Las reglas que firewalld crea aparecen en `nft list ruleset`:

```bash
# Ver las tablas de firewalld
nft list ruleset | head -5
# table inet firewalld { ...

# ADVERTENCIA: no modifiques directamente las reglas de firewalld con nft
# firewalld las sobreescribirá al recargar
```

### Relación entre las herramientas

```
┌──────────────────────────────────────────────────────┐
│                 Kernel: nf_tables                    │
│              (única infraestructura)                 │
└───────────┬──────────────┬──────────────┬────────────┘
            │              │              │
     ┌──────┴──────┐  ┌───┴────┐  ┌──────┴──────┐
     │iptables-nft │  │  nft   │  │  firewalld  │
     │  (compat)   │  │(native)│  │ (frontend)  │
     └─────────────┘  └────────┘  └─────────────┘
     Sintaxis vieja    Sintaxis    Zonas, servicios
     → traducida       nativa      → genera reglas nft

     Usar UNA de estas herramientas, no mezclar.
```

---

## 12. Diagnóstico y depuración

### Listar reglas

```bash
# Todo el ruleset
nft list ruleset

# Una tabla específica
nft list table inet firewall

# Una cadena específica
nft list chain inet firewall input

# Con handles (necesarios para delete/replace)
nft -a list chain inet firewall input
# tcp dport 22 accept  # handle 7
# tcp dport 80 accept  # handle 9

# En formato JSON (útil para scripts)
nft -j list ruleset
```

### Contadores y estadísticas

```bash
# Añadir contadores a reglas existentes
# (las reglas nft no cuentan por defecto)
nft add rule inet firewall input tcp dport 22 counter accept

# Resetear contadores de una tabla
nft reset counters table inet firewall

# Objetos counter con nombre (reutilizables)
nft add counter inet firewall ssh_counter
nft add rule inet firewall input tcp dport 22 counter name ssh_counter accept
nft list counter inet firewall ssh_counter
```

### Tracing (equivalente a -j TRACE)

```bash
# Activar trace para un flujo específico
nft add rule inet firewall prerouting \
    tcp dport 80 meta nftrace set 1

# Monitorizar el trace
nft monitor trace

# Salida típica:
# trace id abc123 inet firewall prerouting packet: iif "eth1" ...
# trace id abc123 inet firewall prerouting rule tcp dport 80 meta nftrace set 1 (verdict continue)
# trace id abc123 inet firewall input rule tcp dport { 80, 443 } accept (verdict accept)

# Desactivar: eliminar la regla de trace
nft -a list chain inet firewall prerouting
nft delete rule inet firewall prerouting handle <N>
```

`nft monitor trace` es mucho más legible que los TRACE de iptables en dmesg:
muestra la tabla, cadena y regla exacta que evaluó cada paquete.

### Monitorización de eventos

```bash
# Ver cambios en reglas en tiempo real
nft monitor

# Tipos de eventos que muestra:
# - add/delete de tablas, cadenas, reglas, sets
# - cambios en elementos de sets
# Útil para detectar qué servicio está modificando el firewall
```

---

## 13. Errores comunes

### Error 1: olvidar escapar el punto y coma en shell

```bash
# ✗ El shell interpreta ; como separador de comandos
nft add chain inet fw input { type filter hook input priority 0; policy drop; }
# bash: policy: command not found

# ✓ Escapar con backslash
nft add chain inet fw input { type filter hook input priority 0 \; policy drop \; }

# ✓ O usar comillas simples
nft add chain inet fw input '{ type filter hook input priority 0; policy drop; }'
```

### Error 2: mezclar iptables-legacy con nft

```bash
# ✗ Reglas en dos subsistemas que no se ven entre sí
iptables-legacy -A INPUT -p tcp --dport 22 -j ACCEPT   # API legacy
nft add rule inet fw input tcp dport 80 accept           # API nf_tables
# Las reglas legacy no aparecen en "nft list ruleset"
# → comportamiento impredecible

# ✓ Verificar qué backend usa iptables
iptables -V   # debe decir (nf_tables)
# ✓ Usar solo nft, o solo iptables-nft, o solo firewalld
```

### Error 3: tabla inet no filtra ARP ni bridge

```bash
# ✗ Creer que inet = todo
# inet = IPv4 + IPv6 solamente
# Para ARP: familia arp
# Para bridge/L2: familia bridge

# ✓ Usar la familia correcta para el tráfico
nft add table bridge my_bridge_filter    # tráfico L2
nft add table arp my_arp_filter          # tráfico ARP
```

### Error 4: confundir iif con iifname

```bash
# ✗ iif con interfaz que no existe al cargar las reglas
nft add rule inet fw input iif wg0 accept
# Error: interfaz wg0 no existe todavía (VPN no levantada)

# ✓ iifname para interfaces dinámicas
nft add rule inet fw input iifname "wg0" accept
# Funciona: el nombre se evalúa en runtime
```

### Error 5: policy accept en cadena nat

```bash
# ✗ Usar policy drop en cadenas nat
nft add chain inet my_nat prerouting {
    type nat hook prerouting priority dstnat \; policy drop \;
}
# Los paquetes sin regla DNAT serían descartados → nada funciona

# ✓ Las cadenas nat deben tener policy accept
# La tabla nat solo decide SI traducir, no si permitir
# El filtrado se hace en las cadenas filter
nft add chain inet my_nat prerouting {
    type nat hook prerouting priority dstnat \; policy accept \;
}
```

---

## 14. Cheatsheet

```bash
# ╔══════════════════════════════════════════════════════════════════╗
# ║                    NFTABLES — CHEATSHEET                       ║
# ╠══════════════════════════════════════════════════════════════════╣
# ║                                                                ║
# ║  GESTIÓN:                                                     ║
# ║  nft list ruleset                   # ver todo                 ║
# ║  nft -a list ruleset                # ver con handles          ║
# ║  nft flush ruleset                  # borrar todo              ║
# ║  nft -c -f /etc/nftables.conf       # verificar sintaxis       ║
# ║  nft -f /etc/nftables.conf          # aplicar (atómico)        ║
# ║  nft list ruleset > backup.nft      # exportar                 ║
# ║                                                                ║
# ║  TABLAS Y CADENAS:                                            ║
# ║  nft add table inet fw                                         ║
# ║  nft add chain inet fw input \                                 ║
# ║    '{ type filter hook input priority 0; policy drop; }'       ║
# ║  nft delete table inet fw                                      ║
# ║                                                                ║
# ║  REGLAS:                                                      ║
# ║  nft add rule inet fw input tcp dport 22 accept                ║
# ║  nft insert rule inet fw input tcp dport 443 accept            ║
# ║  nft delete rule inet fw input handle 7                        ║
# ║  nft replace rule inet fw input handle 7 tcp dport 22 drop     ║
# ║                                                                ║
# ║  MATCHES COMUNES:                                             ║
# ║  ip saddr 192.168.1.0/24           # red origen                ║
# ║  tcp dport { 80, 443 }             # puertos (set)             ║
# ║  iifname "eth0"                    # interfaz entrada          ║
# ║  ct state established,related      # conntrack                 ║
# ║  meta l4proto icmp                 # protocolo L4              ║
# ║                                                                ║
# ║  NAT:                                                         ║
# ║  masquerade                         # SNAT dinámico            ║
# ║  snat to 198.51.100.1              # SNAT fijo                ║
# ║  dnat to 192.168.1.100:80          # DNAT                     ║
# ║  redirect to :3128                 # redirigir a localhost     ║
# ║                                                                ║
# ║  SETS:                                                        ║
# ║  nft add set inet fw ips '{ type ipv4_addr; }'                ║
# ║  nft add element inet fw ips '{ 1.2.3.4, 5.6.7.8 }'          ║
# ║  ip saddr @ips drop                # usar set en regla         ║
# ║                                                                ║
# ║  DEPURACIÓN:                                                  ║
# ║  meta nftrace set 1                # activar trace            ║
# ║  nft monitor trace                 # ver trace en vivo        ║
# ║  nft monitor                       # ver cambios en reglas    ║
# ║  counter                           # contar paquetes          ║
# ║                                                                ║
# ╚══════════════════════════════════════════════════════════════════╝
```

---

## 15. Ejercicios

### Ejercicio 1: firewall básico con nftables

**Contexto**: un servidor Linux con una sola interfaz (`eth0`) necesita un
firewall que permita SSH, HTTP, HTTPS e ICMP.

**Tareas**:

1. Crea un archivo `/etc/nftables.conf` completo con:
   - Tabla `inet firewall`
   - Cadena `input` con política drop
   - Cadena `output` con política accept
   - Reglas para loopback, conexiones establecidas, SSH, HTTP/S, ICMP
   - Contadores en cada regla
   - Logging para paquetes descartados
2. Verifica la sintaxis con `nft -c -f`
3. Aplica y comprueba con `nft list ruleset`
4. Usa `nft monitor trace` para verificar que una petición SSH recorre las reglas correctamente

**Pistas**:
- Usa variables `define` para puertos y redes
- No olvides `flush ruleset` al inicio del archivo
- `meta l4proto { icmp, icmpv6 }` cubre ambas familias en `inet`

> **Pregunta de reflexión**: ¿por qué nftables no activa contadores por defecto
> mientras iptables siempre los tiene? ¿Qué impacto tiene esto en rendimiento
> cuando hay miles de reglas?

---

### Ejercicio 2: migración de iptables a nftables

**Contexto**: tienes estas reglas iptables que debes migrar:

```bash
iptables -P INPUT DROP
iptables -P FORWARD DROP
iptables -A INPUT -i lo -j ACCEPT
iptables -A INPUT -m conntrack --ctstate ESTABLISHED,RELATED -j ACCEPT
iptables -A INPUT -p tcp -m multiport --dports 22,80,443 -j ACCEPT
iptables -A INPUT -s 10.0.0.0/8 -p tcp --dport 3306 -j ACCEPT
iptables -A INPUT -m limit --limit 5/min -j LOG --log-prefix "DROPPED: "
iptables -A FORWARD -i eth0 -o eth1 -j ACCEPT
iptables -A FORWARD -m conntrack --ctstate ESTABLISHED,RELATED -j ACCEPT
iptables -t nat -A POSTROUTING -s 192.168.1.0/24 -o eth1 -j MASQUERADE
iptables -t nat -A PREROUTING -i eth1 -p tcp --dport 80 \
    -j DNAT --to-destination 192.168.1.100:80
```

**Tareas**:

1. Usa `iptables-translate` para obtener una traducción automática
2. Mejora la traducción automática:
   - Usa familia `inet` en lugar de `ip`
   - Agrupa puertos en sets
   - Añade variables `define`
   - Añade un set con nombre para las IPs que pueden acceder a MySQL
3. Escribe el archivo nftables.conf final

> **Pregunta de reflexión**: la traducción automática de `iptables-translate`
> genera reglas en la familia `ip` (solo IPv4). ¿Qué implicaciones tiene
> cambiar a `inet`? ¿Hay algún caso donde una regla IPv4 no debería
> aplicarse también a IPv6?

---

### Ejercicio 3: sets dinámicos para rate limiting

**Contexto**: necesitas implementar protección contra fuerza bruta SSH usando
sets dinámicos de nftables (sin fail2ban).

**Tareas**:

1. Crea un set dinámico con timeout que almacene IPs que intenten conectar
   por SSH
2. Implementa la lógica: si una IP hace más de 3 conexiones nuevas SSH en
   1 minuto, añadirla a un set de bloqueo con timeout de 10 minutos
3. Las IPs bloqueadas deben recibir un `drop` antes de cualquier otra evaluación
4. Añade contadores para monitorizar cuántas IPs se bloquean
5. Verifica el contenido del set con `nft list set`

**Pistas**:
- Necesitarás al menos dos sets: uno para contar intentos y otro para bloquear
- `limit rate over 3/minute` combina con `add @blocked { ip saddr }`
- Los flags `dynamic,timeout` permiten que las reglas modifiquen el set

> **Pregunta de reflexión**: ¿cuáles son las ventajas y desventajas de esta
> solución nft nativa frente a fail2ban? ¿En qué escenarios cada una es
> superior?

---

> **Siguiente tema**: S02 — firewalld: T01 — Zonas (modelo de confianza: public, internal, trusted, etc.)
