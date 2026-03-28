# ISC DHCP Server

## Índice
1. [Qué es DHCP](#1-qué-es-dhcp)
2. [El proceso DORA](#2-el-proceso-dora)
3. [Instalación](#3-instalación)
4. [dhcpd.conf — estructura](#4-dhcpdconf--estructura)
5. [Rangos y subredes](#5-rangos-y-subredes)
6. [Reservaciones (IP fija por MAC)](#6-reservaciones-ip-fija-por-mac)
7. [Opciones DHCP](#7-opciones-dhcp)
8. [Lease time y base de datos de leases](#8-lease-time-y-base-de-datos-de-leases)
9. [Configuraciones avanzadas](#9-configuraciones-avanzadas)
10. [Errores comunes](#10-errores-comunes)
11. [Cheatsheet](#11-cheatsheet)
12. [Ejercicios](#12-ejercicios)

---

## 1. Qué es DHCP

**DHCP (Dynamic Host Configuration Protocol)** asigna automáticamente direcciones IP y parámetros de red a los equipos de una LAN. Sin DHCP, cada equipo necesitaría configuración manual de IP, máscara, gateway y DNS.

```
┌──────────────────────────────────────────────────────────┐
│                    Red con DHCP                           │
│                                                          │
│  ┌──────────┐                                            │
│  │  DHCP     │  Pool: 192.168.1.100 - 192.168.1.200     │
│  │  Server   │  Gateway: 192.168.1.1                     │
│  │  .10      │  DNS: 192.168.1.10, 8.8.8.8              │
│  └────┬─────┘                                            │
│       │ broadcast                                        │
│  ─────┼──────────────────────────────────────            │
│       │           │            │                         │
│  ┌────┴───┐  ┌────┴───┐  ┌────┴───┐                     │
│  │ PC-1   │  │ PC-2   │  │ PC-3   │                     │
│  │ .101   │  │ .102   │  │ .103   │                     │
│  │ (auto) │  │ (auto) │  │ (auto) │                     │
│  └────────┘  └────────┘  └────────┘                     │
│                                                          │
│  Cada equipo recibe IP, máscara, gateway y DNS           │
│  automáticamente al conectarse a la red                  │
└──────────────────────────────────────────────────────────┘
```

### ISC DHCP vs alternativas

| Servidor | Estado | Notas |
|----------|--------|-------|
| **ISC DHCP (dhcpd)** | Mantenimiento (EOL 2022) | Estándar histórico, aún en uso masivo |
| **Kea** | Activo | Sucesor de ISC DHCP, diseño modular, API REST |
| **dnsmasq** | Activo | Ligero, combina DHCP + DNS, ideal para redes pequeñas |

ISC DHCP se estudia porque sigue siendo el más desplegado y el que aparece en certificaciones. Su configuración es directamente transferible a Kea.

---

## 2. El proceso DORA

DHCP usa un intercambio de 4 paquetes llamado **DORA** (Discover, Offer, Request, Acknowledge). Todos los paquetes iniciales son **broadcast** porque el cliente aún no tiene IP.

```
┌──────────────────────────────────────────────────────────┐
│                   Proceso DORA                            │
│                                                          │
│  Cliente                              Servidor DHCP      │
│  (sin IP)                             192.168.1.10       │
│                                                          │
│  1. DHCPDISCOVER ──── broadcast ────►                    │
│     src: 0.0.0.0                                         │
│     dst: 255.255.255.255                                 │
│     "¿Hay algún servidor DHCP?"                          │
│                                                          │
│  2.                  ◄──── DHCPOFFER                     │
│                      "Te ofrezco 192.168.1.101           │
│                       mask 255.255.255.0                 │
│                       gw 192.168.1.1                     │
│                       dns 8.8.8.8                        │
│                       lease 86400 seg"                   │
│                                                          │
│  3. DHCPREQUEST ──── broadcast ────►                     │
│     "Acepto la oferta de 192.168.1.10                    │
│      para 192.168.1.101"                                 │
│     (broadcast para que otros DHCP                       │
│      sepan que no fueron elegidos)                       │
│                                                          │
│  4.                  ◄──── DHCPACK                       │
│                      "Confirmado.                        │
│                       192.168.1.101 es tuya              │
│                       por 86400 segundos"                │
│                                                          │
│  Cliente configura la interfaz con los datos recibidos   │
└──────────────────────────────────────────────────────────┘
```

### Renovación del lease

El cliente no espera a que el lease expire para renovar:

```
  Lease: 86400 seg (24 horas)
  ├──────────────────────┤
  0%          50%        100%
  │           │          │
  Asignado    T1         T2      Expiración
              │          │
              │          └── 87.5% → broadcast DHCPREQUEST
              │              (intenta cualquier servidor)
              └── 50% → unicast DHCPREQUEST al mismo servidor
                  (intenta renovar silenciosamente)
```

- **T1 (50%):** el cliente envía DHCPREQUEST unicast al servidor original
- **T2 (87.5%):** si T1 falló, envía DHCPREQUEST broadcast a cualquier servidor
- **100%:** si no renovó, pierde la IP y reinicia DORA desde cero

### Otros mensajes DHCP

| Mensaje | Dirección | Propósito |
|---------|-----------|-----------|
| DHCPDECLINE | Cliente → Servidor | "Esa IP ya está en uso" (conflicto ARP) |
| DHCPNAK | Servidor → Cliente | "Tu lease ya no es válido" |
| DHCPRELEASE | Cliente → Servidor | "Ya no necesito esta IP" (apagado limpio) |
| DHCPINFORM | Cliente → Servidor | "Ya tengo IP, dame solo las opciones" |

---

## 3. Instalación

### Debian/Ubuntu

```bash
# Instalar
sudo apt install isc-dhcp-server

# El servicio NO arranca hasta configurarlo
sudo systemctl status isc-dhcp-server

# Configurar la interfaz de escucha
sudo vim /etc/default/isc-dhcp-server
```

```bash
# /etc/default/isc-dhcp-server
INTERFACESv4="eth0"
INTERFACESv6=""
```

### RHEL/Fedora

```bash
# Instalar
sudo dnf install dhcp-server

# Configurar
sudo vim /etc/dhcp/dhcpd.conf

# Habilitar e iniciar
sudo systemctl enable --now dhcpd
```

### Archivos principales

| Archivo | Propósito |
|---------|-----------|
| `/etc/dhcp/dhcpd.conf` | Configuración principal |
| `/etc/default/isc-dhcp-server` | Interfaz de escucha (Debian) |
| `/var/lib/dhcp/dhcpd.leases` | Base de datos de leases activos |
| `/var/lib/dhcp/dhcpd.leases~` | Backup de leases |

### Verificar sintaxis antes de iniciar

```bash
# Validar configuración (SIEMPRE antes de reiniciar)
sudo dhcpd -t -cf /etc/dhcp/dhcpd.conf

# Si la sintaxis es correcta:
# Internet Systems Consortium DHCP Server 4.4.3
# Config file: /etc/dhcp/dhcpd.conf
# ...exiting.

# Si hay error, muestra la línea exacta:
# /etc/dhcp/dhcpd.conf line 15: expecting a parameter or declaration
```

---

## 4. dhcpd.conf — estructura

### Sintaxis general

```bash
# Comentarios
# Esto es un comentario

# Parámetros globales (aplican a todo)
option domain-name "example.com";
option domain-name-servers 192.168.1.10, 8.8.8.8;
default-lease-time 600;
max-lease-time 7200;

# Declaración de subred (obligatoria)
subnet 192.168.1.0 netmask 255.255.255.0 {
    range 192.168.1.100 192.168.1.200;
    option routers 192.168.1.1;
}

# Reservación por MAC
host impresora {
    hardware ethernet 00:11:22:33:44:55;
    fixed-address 192.168.1.50;
}
```

Reglas:
- Cada sentencia termina en **`;`**
- Los bloques usan **`{ }`**
- Los parámetros se heredan: global → subnet → pool → host
- Las opciones más específicas sobreescriben las más generales

### Jerarquía de declaraciones

```
┌──────────────────────────────────────────────────────────┐
│              Jerarquía dhcpd.conf                         │
│                                                          │
│  global                                                  │
│  ├── option domain-name-servers ...                      │
│  ├── default-lease-time 600                              │
│  │                                                       │
│  ├── shared-network "oficina" {                          │
│  │   ├── subnet 192.168.1.0/24 {                        │
│  │   │   ├── range 192.168.1.100 .200                   │
│  │   │   ├── option routers 192.168.1.1                 │
│  │   │   │                                              │
│  │   │   ├── pool {                                     │
│  │   │   │   ├── range 192.168.1.150 .180               │
│  │   │   │   └── allow members of "vip"                 │
│  │   │   │                                              │
│  │   │   └── host impresora {                           │
│  │   │       ├── hardware ethernet ...                  │
│  │   │       └── fixed-address 192.168.1.50             │
│  │   │                                                  │
│  │   └── subnet 192.168.2.0/24 {                        │
│  │       └── ...                                        │
│  │                                                      │
│  └── group {                                             │
│      ├── option domain-name "lab.local"                  │
│      ├── host lab1 { ... }                               │
│      └── host lab2 { ... }                               │
│                                                          │
│  Herencia: host < pool < subnet < shared-network < global│
└──────────────────────────────────────────────────────────┘
```

---

## 5. Rangos y subredes

### Subnet básica

Cada subred conectada a una interfaz del servidor **debe** tener una declaración `subnet`, incluso si no se asignan IPs dinámicas en ella.

```bash
# /etc/dhcp/dhcpd.conf

# Parámetros globales
option domain-name "example.com";
option domain-name-servers 192.168.1.10, 8.8.8.8;
default-lease-time 3600;       # 1 hora
max-lease-time 86400;          # 24 horas

# Subred principal — con rango dinámico
subnet 192.168.1.0 netmask 255.255.255.0 {
    range 192.168.1.100 192.168.1.200;
    option routers 192.168.1.1;
    option broadcast-address 192.168.1.255;
    option subnet-mask 255.255.255.0;
}
```

### Múltiples rangos en una subred

```bash
subnet 192.168.1.0 netmask 255.255.255.0 {
    # Rango para estaciones de trabajo
    range 192.168.1.100 192.168.1.150;

    # Rango separado para dispositivos IoT
    range 192.168.1.200 192.168.1.250;

    option routers 192.168.1.1;
}
```

### Múltiples subredes

```bash
# Subred de oficina
subnet 192.168.1.0 netmask 255.255.255.0 {
    range 192.168.1.100 192.168.1.200;
    option routers 192.168.1.1;
    option domain-name-servers 192.168.1.10;
}

# Subred de laboratorio (en otra interfaz o VLAN)
subnet 192.168.2.0 netmask 255.255.255.0 {
    range 192.168.2.50 192.168.2.150;
    option routers 192.168.2.1;
    option domain-name-servers 192.168.2.10;
    default-lease-time 1800;    # 30 min (override global)
}

# Subred del servidor (sin rango — solo declarar que existe)
subnet 10.0.0.0 netmask 255.255.255.0 {
    # Sin range — no asigna IPs aquí
    # Necesario si dhcpd escucha en una interfaz de esta subred
}
```

### shared-network (múltiples subredes en un segmento físico)

```bash
# Dos subredes en la misma interfaz física (VLANs, etc.)
shared-network "edificio-a" {
    subnet 192.168.1.0 netmask 255.255.255.0 {
        range 192.168.1.100 192.168.1.200;
        option routers 192.168.1.1;
    }
    subnet 192.168.2.0 netmask 255.255.255.0 {
        range 192.168.2.100 192.168.2.200;
        option routers 192.168.2.1;
    }
}
```

---

## 6. Reservaciones (IP fija por MAC)

Las reservaciones asignan siempre la **misma IP** a un dispositivo identificado por su dirección MAC. La IP reservada **no debe** estar dentro del rango dinámico.

### Sintaxis

```bash
# Dentro o fuera de una subnet
host impresora-rrhh {
    hardware ethernet 00:11:22:33:44:55;
    fixed-address 192.168.1.50;
}

host servidor-backup {
    hardware ethernet AA:BB:CC:DD:EE:FF;
    fixed-address 192.168.1.51;
    option host-name "backup";
}
```

### Reservación dentro de subnet

```bash
subnet 192.168.1.0 netmask 255.255.255.0 {
    range 192.168.1.100 192.168.1.200;    # Dinámico: .100-.200
    option routers 192.168.1.1;

    # Reservaciones fuera del rango dinámico
    host impresora {
        hardware ethernet 00:11:22:33:44:55;
        fixed-address 192.168.1.50;        # .50 está FUERA del range
    }

    host nas {
        hardware ethernet AA:BB:CC:DD:EE:FF;
        fixed-address 192.168.1.51;
    }
}
```

### Reservación con opciones específicas

```bash
host telefono-voip {
    hardware ethernet 00:AA:BB:CC:DD:EE;
    fixed-address 192.168.1.60;
    option tftp-server-name "192.168.1.10";    # Servidor TFTP para firmware
    option bootfile-name "phone-config.xml";    # Archivo de config
    default-lease-time 604800;                  # 7 días
}
```

### group — agrupar reservaciones con opciones comunes

```bash
# Todas las impresoras comparten las mismas opciones
group {
    option domain-name "print.example.com";
    default-lease-time 604800;    # 7 días para impresoras

    host hp-piso1 {
        hardware ethernet 00:11:22:33:44:01;
        fixed-address 192.168.1.50;
    }
    host hp-piso2 {
        hardware ethernet 00:11:22:33:44:02;
        fixed-address 192.168.1.51;
    }
    host hp-piso3 {
        hardware ethernet 00:11:22:33:44:03;
        fixed-address 192.168.1.52;
    }
}
```

---

## 7. Opciones DHCP

Las **opciones** son los parámetros de red que el servidor envía al cliente junto con la IP.

### Opciones más comunes

| Opción | Ejemplo | Propósito |
|--------|---------|-----------|
| `option routers` | `192.168.1.1` | Default gateway |
| `option subnet-mask` | `255.255.255.0` | Máscara de subred |
| `option broadcast-address` | `192.168.1.255` | Dirección broadcast |
| `option domain-name-servers` | `8.8.8.8, 8.8.4.4` | Servidores DNS |
| `option domain-name` | `"example.com"` | Dominio de búsqueda |
| `option domain-search` | `"example.com", "lab.local"` | Lista de dominios de búsqueda |
| `option ntp-servers` | `192.168.1.10` | Servidor NTP |
| `option host-name` | `"pc-juan"` | Hostname del cliente |
| `option netbios-name-servers` | `192.168.1.10` | WINS server |
| `option tftp-server-name` | `"192.168.1.10"` | Servidor TFTP (PXE boot) |
| `option bootfile-name` | `"pxelinux.0"` | Archivo de boot (PXE) |

### Opciones numéricas (por código)

```bash
# Si una opción no tiene nombre predefinido, usar su código
option option-150 "192.168.1.10";    # Opción 150 (Cisco IP phones)

# Definir opciones personalizadas
option space custom;
option custom.proxy-url code 252 = text;
option custom.proxy-url "http://proxy.example.com/wpad.dat";
```

### Herencia de opciones

```bash
# Global — aplica a todas las subredes
option domain-name-servers 8.8.8.8, 8.8.4.4;

subnet 192.168.1.0 netmask 255.255.255.0 {
    range 192.168.1.100 192.168.1.200;
    option routers 192.168.1.1;
    # Hereda domain-name-servers de global

    host especial {
        hardware ethernet 00:11:22:33:44:55;
        fixed-address 192.168.1.50;
        option domain-name-servers 192.168.1.10;  # Override para este host
    }
}
```

---

## 8. Lease time y base de datos de leases

### Parámetros de tiempo

```bash
# Tiempos en segundos
default-lease-time 3600;     # 1 hora — si el cliente no pide tiempo específico
max-lease-time 86400;        # 24 horas — máximo que el cliente puede pedir
min-lease-time 300;          # 5 min — mínimo (opcional)
```

| Escenario | Lease recomendado |
|-----------|------------------|
| Red de oficina estable | 8-24 horas |
| WiFi de cafetería/evento | 1-2 horas |
| Red de laboratorio | 15-30 minutos |
| Dispositivos fijos (impresoras, NAS) | 7+ días (o reservación) |
| Red con pocas IPs disponibles | Corto (recicla IPs rápido) |

### La base de datos de leases

`/var/lib/dhcp/dhcpd.leases` registra cada IP asignada:

```bash
cat /var/lib/dhcp/dhcpd.leases
```

```
lease 192.168.1.101 {
  starts 6 2026/03/21 10:30:00;
  ends 6 2026/03/21 11:30:00;
  cltt 6 2026/03/21 10:30:00;
  binding state active;
  next binding state free;
  rewind binding state free;
  hardware ethernet 00:11:22:33:44:55;
  uid "\001\000\021\"3DU";
  client-hostname "desktop-juan";
}
```

| Campo | Significado |
|-------|------------|
| `starts` | Inicio del lease |
| `ends` | Expiración del lease |
| `cltt` | Client Last Transaction Time |
| `binding state` | `active`, `free`, `backup`, `expired` |
| `hardware ethernet` | MAC del cliente |
| `client-hostname` | Hostname que reporta el cliente |

### Limpieza de leases

El archivo crece con el tiempo porque dhcpd añade registros sin borrar los antiguos. Se limpia automáticamente al reiniciar el servicio:

```bash
# dhcpd reconstruye el archivo al iniciar
# Fusiona dhcpd.leases con dhcpd.leases~
# Resultado: solo leases activos

# Si el archivo es muy grande y quieres forzar limpieza:
sudo systemctl stop isc-dhcp-server
sudo rm /var/lib/dhcp/dhcpd.leases~
sudo systemctl start isc-dhcp-server
```

### Consultar leases activos

```bash
# Ver todos los leases activos
grep -A 8 "binding state active" /var/lib/dhcp/dhcpd.leases

# Herramientas de terceros
sudo dhcp-lease-list    # Si está disponible
```

---

## 9. Configuraciones avanzadas

### DHCP Relay (ip helper-address)

Cuando el servidor DHCP está en otra subred, los broadcast DHCP no cruzan el router. Se necesita un **relay agent** en el router o en un equipo de la subred:

```
┌──────────────────────────────────────────────────────────┐
│                  DHCP Relay                                │
│                                                          │
│  Subred A                      Subred B                  │
│  192.168.1.0/24                192.168.2.0/24            │
│                                                          │
│  ┌────────┐    ┌──────────┐    ┌──────────┐             │
│  │ Cliente │    │  Router   │    │ DHCP Srv │             │
│  │ (sin IP)│    │  + Relay  │    │ .2.10    │             │
│  └────┬───┘    └────┬─────┘    └────┬─────┘             │
│       │             │               │                    │
│  1. DISCOVER ──►    │               │                    │
│  (broadcast)        │               │                    │
│       │        2. Relay reenvía     │                    │
│       │           como unicast ──► │                    │
│       │             │          3. OFFER                  │
│       │        4. Relay reenvía ◄──                      │
│  5. ◄─────────      │               │                    │
│                                                          │
│  En el router (Cisco):                                   │
│    ip helper-address 192.168.2.10                        │
│                                                          │
│  En Linux (dhcrelay):                                    │
│    dhcrelay -i eth0 192.168.2.10                         │
└──────────────────────────────────────────────────────────┘
```

```bash
# Instalar relay en Linux
sudo apt install isc-dhcp-relay    # Debian
sudo dnf install dhcp-relay        # RHEL

# Configurar
sudo vim /etc/default/isc-dhcp-relay
# SERVERS="192.168.2.10"
# INTERFACES="eth0 eth1"

sudo systemctl enable --now isc-dhcp-relay
```

### Pool con restricciones (allow/deny)

```bash
# Clases de clientes
class "voip" {
    match if substring(hardware, 1, 3) = 00:0a:95;    # OUI del fabricante
}

subnet 192.168.1.0 netmask 255.255.255.0 {
    # Pool para teléfonos VoIP
    pool {
        range 192.168.1.200 192.168.1.250;
        allow members of "voip";
        option tftp-server-name "192.168.1.10";
    }

    # Pool general (excluye VoIP)
    pool {
        range 192.168.1.100 192.168.1.199;
        deny members of "voip";
    }

    option routers 192.168.1.1;
}
```

### Failover (alta disponibilidad)

Dos servidores DHCP pueden compartir un pool para alta disponibilidad:

```bash
# Servidor primario
failover peer "dhcp-failover" {
    primary;
    address 192.168.1.10;
    port 647;
    peer address 192.168.1.11;
    peer port 647;
    max-response-delay 60;
    max-unacked-updates 10;
    mclt 3600;
    split 128;             # 50/50 del pool
    load balance max seconds 3;
}

subnet 192.168.1.0 netmask 255.255.255.0 {
    pool {
        failover peer "dhcp-failover";
        range 192.168.1.100 192.168.1.200;
    }
    option routers 192.168.1.1;
}
```

### DDNS (Dynamic DNS updates)

DHCP puede actualizar registros DNS automáticamente cuando asigna una IP:

```bash
# Actualizar DNS cuando se asigna un lease
ddns-update-style interim;
ddns-updates on;
ddns-domainname "example.com.";
ddns-rev-domainname "in-addr.arpa.";

# Clave TSIG para autenticación con BIND
key "dhcp-dns-key" {
    algorithm hmac-sha256;
    secret "clave_base64_aqui";
};

zone example.com. {
    primary 192.168.1.10;
    key "dhcp-dns-key";
}

zone 1.168.192.in-addr.arpa. {
    primary 192.168.1.10;
    key "dhcp-dns-key";
}
```

---

## 10. Errores comunes

### Error 1 — No declarar la subred de la interfaz del servidor

```bash
# El servidor tiene IP 10.0.0.5 en eth0 y sirve 192.168.1.0/24 via relay
# Error al iniciar:
# No subnet declaration for eth0 (10.0.0.5)

# Solución: declarar TODAS las subredes de interfaces activas
subnet 10.0.0.0 netmask 255.255.255.0 {
    # Sin range — no asigna IPs aquí
    # Solo necesario para que dhcpd arranque
}

subnet 192.168.1.0 netmask 255.255.255.0 {
    range 192.168.1.100 192.168.1.200;
    option routers 192.168.1.1;
}
```

### Error 2 — IP reservada dentro del rango dinámico

```bash
# MAL — la IP .50 podría asignarse dinámicamente a otro equipo
# antes de que la impresora pida su lease
subnet 192.168.1.0 netmask 255.255.255.0 {
    range 192.168.1.50 192.168.1.200;    # ← incluye .50

    host impresora {
        hardware ethernet 00:11:22:33:44:55;
        fixed-address 192.168.1.50;       # ← conflicto potencial
    }
}

# BIEN — separar el rango dinámico de las reservaciones
subnet 192.168.1.0 netmask 255.255.255.0 {
    range 192.168.1.100 192.168.1.200;    # Dinámico: .100-.200

    host impresora {
        hardware ethernet 00:11:22:33:44:55;
        fixed-address 192.168.1.50;       # Reservado: .50 (fuera del range)
    }
}
```

> En la práctica ISC DHCP es lo suficientemente inteligente para no dar una fixed-address a otro cliente, pero separar los rangos es mejor práctica y evita confusión.

### Error 3 — Falta el punto y coma

```bash
# dhcpd -t reporta un error críptico:
# /etc/dhcp/dhcpd.conf line 12: expecting semicolon.

# La causa suele ser una línea sin ; al final:
option routers 192.168.1.1     # ← falta ;

# Corrección:
option routers 192.168.1.1;
```

### Error 4 — Dos servidores DHCP sin failover

```bash
# Si hay dos servidores DHCP en la misma red SIN failover:
# - Ambos responden a DISCOVER
# - El cliente toma la primera OFFER que llegue
# - Pueden asignar la MISMA IP a dos clientes → conflicto

# Solución 1: un solo servidor DHCP
# Solución 2: configurar failover (sección 9)
# Solución 3: rangos no solapados en cada servidor
# Servidor A: range 192.168.1.100 192.168.1.150
# Servidor B: range 192.168.1.151 192.168.1.200
```

### Error 5 — No configurar la interfaz de escucha

```bash
# Debian: el servicio falla al iniciar si no se configura INTERFACESv4
sudo systemctl status isc-dhcp-server
# "Not configured to listen on any interfaces!"

# Solución:
sudo vim /etc/default/isc-dhcp-server
# INTERFACESv4="eth0"

sudo systemctl restart isc-dhcp-server
```

---

## 11. Cheatsheet

```bash
# ── Instalación ──────────────────────────────────────────
apt install isc-dhcp-server          # Debian/Ubuntu
dnf install dhcp-server              # RHEL/Fedora
# Configurar interfaz: /etc/default/isc-dhcp-server (Debian)
# INTERFACESv4="eth0"

# ── Archivos ─────────────────────────────────────────────
/etc/dhcp/dhcpd.conf                 # Configuración
/var/lib/dhcp/dhcpd.leases           # Leases activos
/etc/default/isc-dhcp-server         # Interfaz (Debian)

# ── dhcpd.conf mínimo ────────────────────────────────────
# option domain-name-servers 8.8.8.8;
# default-lease-time 3600;
# max-lease-time 86400;
#
# subnet 192.168.1.0 netmask 255.255.255.0 {
#     range 192.168.1.100 192.168.1.200;
#     option routers 192.168.1.1;
# }

# ── Reservación ───────────────────────────────────────────
# host nombre {
#     hardware ethernet AA:BB:CC:DD:EE:FF;
#     fixed-address 192.168.1.50;
# }

# ── Opciones comunes ─────────────────────────────────────
# option routers IP;                 # Gateway
# option domain-name-servers IP;     # DNS
# option domain-name "dom.com";      # Dominio
# option ntp-servers IP;             # NTP
# option subnet-mask MASK;           # Máscara
# option broadcast-address IP;       # Broadcast

# ── Validación y servicio ─────────────────────────────────
dhcpd -t -cf /etc/dhcp/dhcpd.conf   # Validar sintaxis
systemctl restart isc-dhcp-server    # Debian
systemctl restart dhcpd              # RHEL

# ── Leases ────────────────────────────────────────────────
cat /var/lib/dhcp/dhcpd.leases       # Ver leases
grep "binding state active" ...      # Solo activos
dhcp-lease-list                      # Herramienta (si disponible)

# ── DORA (recordar) ──────────────────────────────────────
# D = Discover  (client → broadcast)
# O = Offer     (server → client)
# R = Request   (client → broadcast)
# A = Acknowledge (server → client)
# Renovación: T1=50% unicast, T2=87.5% broadcast

# ── Relay ─────────────────────────────────────────────────
dhcrelay -i eth0 SERVER_IP           # Relay agent Linux
# Router Cisco: ip helper-address SERVER_IP
```

---

## 12. Ejercicios

### Ejercicio 1 — Configurar un servidor DHCP básico

Configura un servidor que asigne IPs en el rango `192.168.1.100-200` con gateway, DNS y lease de 4 horas.

```bash
# /etc/dhcp/dhcpd.conf
option domain-name "lab.local";
option domain-name-servers 192.168.1.10, 8.8.8.8;
default-lease-time 14400;       # 4 horas
max-lease-time 86400;           # 24 horas

subnet 192.168.1.0 netmask 255.255.255.0 {
    range 192.168.1.100 192.168.1.200;
    option routers 192.168.1.1;
    option broadcast-address 192.168.1.255;
}
```

```bash
# Configurar interfaz (Debian)
echo 'INTERFACESv4="eth0"' | sudo tee /etc/default/isc-dhcp-server

# Validar y arrancar
sudo dhcpd -t -cf /etc/dhcp/dhcpd.conf
sudo systemctl restart isc-dhcp-server
```

**Pregunta de predicción:** Si un cliente solicita un lease de 48 horas pero `max-lease-time` es 86400 (24h), ¿qué lease recibe? ¿Y si solicita 2 horas pero `default-lease-time` es 14400 (4h)?

> **Respuesta:** Si pide 48h (172800s) y el máximo es 86400s, recibe 86400s (24h) — el servidor nunca otorga más que `max-lease-time`. Si pide 2h (7200s), recibe 7200s — el cliente puede pedir menos que el default. `default-lease-time` solo se aplica cuando el cliente **no especifica** un tiempo concreto en su DHCPREQUEST.

---

### Ejercicio 2 — Reservaciones para infraestructura

Agrega reservaciones para una impresora, un NAS y un punto de acceso WiFi, con opciones comunes por grupo.

```bash
# Agregar a dhcpd.conf

# Grupo de infraestructura — lease largo, DNS específico
group {
    default-lease-time 604800;    # 7 días
    option domain-name-servers 192.168.1.10;

    host impresora-hp {
        hardware ethernet 00:11:22:33:44:01;
        fixed-address 192.168.1.30;
        option host-name "printer-1f";
    }

    host nas-synology {
        hardware ethernet 00:11:22:33:44:02;
        fixed-address 192.168.1.31;
        option host-name "nas";
    }

    host ap-wifi {
        hardware ethernet 00:11:22:33:44:03;
        fixed-address 192.168.1.32;
        option host-name "ap-lobby";
    }
}
```

```bash
# Validar
sudo dhcpd -t -cf /etc/dhcp/dhcpd.conf
sudo systemctl restart isc-dhcp-server
```

**Pregunta de predicción:** Si el NAS tiene su IP configurada estáticamente como `192.168.1.31` (no usa DHCP), ¿tiene sentido la reservación DHCP? ¿Qué pasa si alguien cambia el NAS a DHCP sin saberlo?

> **Respuesta:** La reservación es una **red de seguridad**. Si el NAS está configurado estáticamente, la reservación DHCP no se usa — pero si alguien lo cambia a DHCP (intencionalmente o al resetear), recibirá la misma IP `192.168.1.31` automáticamente en lugar de una IP aleatoria del rango dinámico. Es una buena práctica tener reservaciones incluso para equipos estáticos.

---

### Ejercicio 3 — Analizar el archivo de leases

Dado el siguiente extracto de `dhcpd.leases`, responde las preguntas:

```
lease 192.168.1.105 {
  starts 6 2026/03/21 08:00:00;
  ends 6 2026/03/21 12:00:00;
  binding state active;
  hardware ethernet AA:BB:CC:DD:EE:11;
  client-hostname "laptop-juan";
}

lease 192.168.1.110 {
  starts 5 2026/03/20 14:00:00;
  ends 6 2026/03/21 14:00:00;
  binding state active;
  hardware ethernet AA:BB:CC:DD:EE:22;
  client-hostname "desktop-maria";
}

lease 192.168.1.115 {
  starts 4 2026/03/19 10:00:00;
  ends 4 2026/03/19 14:00:00;
  binding state free;
  hardware ethernet AA:BB:CC:DD:EE:33;
}
```

**Pregunta de predicción:** Son las 10:00 UTC del 21 de marzo de 2026. ¿Cuáles de estos leases están realmente activos? ¿Cuál intentará renovar primero y a qué hora?

> **Respuesta:** `.105` (laptop-juan) está activo (expira a las 12:00 de hoy). `.110` (desktop-maria) está activo (expira a las 14:00 de hoy). `.115` ya expiró el 19 de marzo y su estado es `free`. El que renueva primero es `.105`: su lease dura 4 horas (08:00-12:00), así que T1 (50%) es a las **10:00** — exactamente ahora. Enviará un DHCPREQUEST unicast al servidor para renovar. `.110` tiene lease de 24 horas (14:00 ayer a 14:00 hoy), su T1 es a las 02:00 de hoy, probablemente ya renovó.

---

> **Siguiente:** T02 — Diagnóstico DHCP (dhcpd logs, tcpdump para ver DORA).
