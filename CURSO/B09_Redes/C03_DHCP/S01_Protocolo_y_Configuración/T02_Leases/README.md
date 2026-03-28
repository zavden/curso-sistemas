# T02 — Leases: Tiempo de Alquiler, Renovación y Rebinding

## Índice

1. [¿Qué es un lease?](#1-qué-es-un-lease)
2. [Anatomía de un lease](#2-anatomía-de-un-lease)
3. [Ciclo de vida completo](#3-ciclo-de-vida-completo)
4. [Temporizadores T1 y T2](#4-temporizadores-t1-y-t2)
5. [Renovación (renew)](#5-renovación-renew)
6. [Rebinding](#6-rebinding)
7. [Expiración y comportamiento post-lease](#7-expiración-y-comportamiento-post-lease)
8. [Lease en el servidor: almacenamiento y gestión](#8-lease-en-el-servidor-almacenamiento-y-gestión)
9. [Lease en el cliente: archivos y estado](#9-lease-en-el-cliente-archivos-y-estado)
10. [Reservas estáticas vs leases dinámicos](#10-reservas-estáticas-vs-leases-dinámicos)
11. [Estrategias de duración de lease](#11-estrategias-de-duración-de-lease)
12. [Lease en DHCPv6](#12-lease-en-dhcpv6)
13. [Diagnóstico y troubleshooting](#13-diagnóstico-y-troubleshooting)
14. [Errores comunes](#14-errores-comunes)
15. [Cheatsheet](#15-cheatsheet)
16. [Ejercicios](#16-ejercicios)

---

## 1. ¿Qué es un lease?

Un **lease** (alquiler) es el mecanismo por el cual un servidor DHCP asigna una dirección IP a un cliente **por un tiempo limitado**. No es una cesión permanente: es un préstamo temporal con fecha de vencimiento.

Este modelo existe por una razón fundamental: **las direcciones IPv4 son un recurso escaso**. Si cada dispositivo que se conectara a una red obtuviera una IP permanente, los pools se agotarían rápidamente en redes con alta rotación de dispositivos (cafeterías, universidades, redes corporativas con portátiles).

```
Sin leases (asignación permanente):
┌──────────────┐
│  Pool: /24   │  256 IPs → 256 dispositivos → se acabó
│  254 usables │  Laptop que se fue hace 3 meses aún "ocupa" su IP
└──────────────┘

Con leases (asignación temporal):
┌──────────────┐
│  Pool: /24   │  254 usables, pero rotan
│  Lease: 8h   │  Laptop que se fue → IP liberada tras 8 horas
│              │  Mismas 254 IPs sirven a miles de dispositivos
└──────────────┘
```

El lease crea un **contrato temporal** entre cliente y servidor:
- El servidor promete no asignar esa IP a otro mientras dure el lease
- El cliente se compromete a renovar antes de que expire, o a dejar de usar la IP

---

## 2. Anatomía de un lease

Un lease contiene más información que solo la IP y la duración. El servidor almacena un registro completo:

```
Lease record (servidor ISC DHCP):
┌──────────────────────────────────────────────────┐
│ lease 192.168.1.100 {                            │
│   starts 2026/03/21 08:00:00;                    │
│   ends   2026/03/21 16:00:00;       ← 8 horas   │
│   tstp   2026/03/21 16:00:00;       ← timestamp  │
│   cltt   2026/03/21 08:00:00;       ← last txn   │
│   binding state active;                          │
│   next binding state free;                       │
│   hardware ethernet aa:bb:cc:dd:ee:ff;           │
│   uid "\001\000\273\314\335\356\377";            │
│   client-hostname "laptop-usuario";              │
│ }                                                │
└──────────────────────────────────────────────────┘
```

Campos clave:

| Campo | Significado |
|-------|-------------|
| `starts` | Momento exacto en que comenzó el lease |
| `ends` | Momento en que expira si no se renueva |
| `binding state` | Estado actual: `active`, `free`, `expired`, `backup` |
| `hardware ethernet` | MAC del cliente (identificador principal) |
| `uid` | Client identifier (option 61), si el cliente lo envió |
| `client-hostname` | Nombre que el cliente reportó (option 12) |
| `cltt` | Client Last Transaction Time — última comunicación |

El campo `uid` merece atención especial. Cuando un cliente envía la option 61 (Client Identifier), el servidor usa ese valor —no la MAC— como clave primaria para identificar al cliente. Esto permite que un mismo dispositivo con múltiples interfaces obtenga la misma IP independientemente de qué interfaz use.

---

## 3. Ciclo de vida completo

Un lease pasa por fases bien definidas desde que se crea hasta que se libera o expira:

```
         DORA completo
              │
              ▼
┌─────────────────────────┐
│       BOUND             │  Cliente usa la IP normalmente
│  IP asignada, lease     │  Temporizadores T1 y T2 corriendo
│  activo                 │
└────────────┬────────────┘
             │
             │ T1 expira (50% del lease)
             ▼
┌─────────────────────────┐     Éxito
│      RENEWING           │────────────► Vuelve a BOUND
│  Unicast al servidor    │              (lease extendido)
│  DHCPREQUEST → ACK      │
└────────────┬────────────┘
             │
             │ Falla (servidor no responde)
             │ T2 expira (87.5% del lease)
             ▼
┌─────────────────────────┐     Éxito
│      REBINDING          │────────────► Vuelve a BOUND
│  Broadcast a CUALQUIER  │
│  servidor DHCP          │
└────────────┬────────────┘
             │
             │ Nadie responde
             │ Lease expira (100%)
             ▼
┌─────────────────────────┐
│       EXPIRED           │  Cliente DEBE dejar de usar la IP
│  Sin dirección IP       │  Vuelve a INIT → nuevo DORA
└─────────────────────────┘
```

Existe también la transición **INIT-REBOOT**: cuando un cliente arranca y ya tiene un lease previo almacenado, no hace un Discover completo. En su lugar, envía directamente un DHCPREQUEST (broadcast) pidiendo la misma IP que tenía. Si el servidor confirma con ACK, el cliente pasa a BOUND sin necesidad de DORA completo:

```
INIT-REBOOT (cliente que reinicia):

Cliente                              Servidor
   │                                     │
   │──── DHCPREQUEST (broadcast) ───────►│  "¿Puedo seguir con 192.168.1.100?"
   │     (IP solicitada en option 50)    │
   │                                     │
   │◄─── DHCPACK ───────────────────────│  "Sí, confirmado"
   │                                     │
   │  → BOUND                            │
```

Pero si la IP ya no es válida (cambió de subred, el lease expiró en el servidor), el servidor envía DHCPNAK y el cliente debe comenzar DORA desde cero.

---

## 4. Temporizadores T1 y T2

Los temporizadores T1 y T2 son el corazón del sistema de renovación. Sus valores por defecto están definidos en los RFC, pero el servidor puede sobrescribirlos:

```
Lease time (ejemplo: 8 horas = 28800 segundos)
│
├── T1 = 50% = 4 horas ──► Intenta RENEW (unicast)
│
├── T2 = 87.5% = 7 horas ──► Intenta REBIND (broadcast)
│
└── 100% = 8 horas ──► Lease expira
```

```
Timeline visual de un lease de 8 horas:

08:00          12:00              15:00        16:00
  │              │                  │            │
  │    BOUND     │    RENEWING      │  REBINDING │
  │              │                  │            │
  ├──────────────┼──────────────────┼────────────┤
  │              │                  │            │
  start         T1               T2          expires
              (50%)           (87.5%)        (100%)

  ◄── Usa la IP ──────────────────────────────────►
                 ◄── Intenta renovar ─────────────►
                                    ◄── Urgente ──►
```

**¿Por qué estos porcentajes específicos?**

- **T1 al 50%**: deja tiempo suficiente para múltiples intentos de renovación antes de escalar. El cliente reintenta periódicamente (típicamente cada ~60 segundos o un porcentaje del tiempo restante).
- **T2 al 87.5%** (7/8 del lease): momento de "pánico controlado". Si el servidor original no respondió durante todo el período T1→T2, probablemente está caído. El broadcast busca cualquier otro servidor que pueda asumir.

El servidor puede enviar valores explícitos:

| DHCP Option | Función | Valor por defecto |
|-------------|---------|-------------------|
| Option 51 | Lease time total | Configurado en servidor |
| Option 58 | T1 (renewal time) | 50% de option 51 |
| Option 59 | T2 (rebinding time) | 87.5% de option 51 |

```
# ISC DHCP: modificar T1 y T2 explícitamente
subnet 192.168.1.0 netmask 255.255.255.0 {
    range 192.168.1.100 192.168.1.200;
    default-lease-time 28800;      # 8 horas
    option dhcp-renewal-time 7200; # T1 = 2 horas (25%)
    option dhcp-rebinding-time 21600; # T2 = 6 horas (75%)
}
```

> **Importante**: si T2 ≤ T1, el comportamiento es indefinido. Siempre debe cumplirse T1 < T2 < lease-time.

---

## 5. Renovación (renew)

Cuando T1 expira, el cliente entra en estado **RENEWING** e intenta extender su lease directamente con el servidor que se lo otorgó:

```
RENEWING — Comunicación unicast:

Cliente (192.168.1.100)          Servidor (192.168.1.1)
   │                                     │
   │──── DHCPREQUEST (unicast) ─────────►│
   │     ciaddr: 192.168.1.100           │
   │     (NO server-identifier)          │
   │     (NO requested-address)          │
   │                                     │
   │◄─── DHCPACK ──────────────────────-─│
   │     yiaddr: 192.168.1.100           │
   │     option 51: 28800 (nuevo lease)  │
   │                                     │
   │  → BOUND (temporizadores reiniciados)
```

Aspectos clave del RENEWING:

1. **Unicast, no broadcast**: el cliente ya conoce la IP del servidor, así que le habla directamente. Esto reduce el tráfico en la red.

2. **ciaddr lleno**: a diferencia del DORA inicial (donde ciaddr es 0.0.0.0), aquí el cliente pone su IP actual en ciaddr. Esto le dice al servidor "estoy renovando esta IP".

3. **Sin server-identifier**: no se incluye la option 54 porque el mensaje va directamente al servidor. La option 54 solo se usa en el DORA para indicar qué servidor se eligió entre varios.

4. **El servidor puede cambiar parámetros**: en el ACK de renovación, el servidor puede actualizar opciones como DNS, gateway o incluso el lease time. El cliente debe aceptar los nuevos valores.

5. **Reintentos**: si el servidor no responde, el cliente reintenta periódicamente. El RFC 2131 sugiere reintentar a intervalos que son la mitad del tiempo restante hasta T2.

```
Ejemplo de reintentos (lease 8h, T1=4h, T2=7h):

12:00  T1 expira → primer DHCPREQUEST
12:30  Sin respuesta → reintento (mitad de 3h restantes ≈ 90min?
       implementaciones varían, típicamente ~60s)
12:31  Reintento
13:00  Reintento
 ...   (sigue intentando)
15:00  T2 expira → cambia a REBINDING
```

**¿Puede el servidor rechazar la renovación?**

Sí, con un DHCPNAK. Esto ocurre cuando:
- La subred fue reconfigurada y esa IP ya no es válida
- El servidor fue reemplazado y no tiene registro de ese lease
- Una política administrativa cambió

Ante un NAK, el cliente debe **dejar de usar la IP inmediatamente** y volver a INIT (DORA completo).

---

## 6. Rebinding

Si el servidor original no responde durante todo el período RENEWING (T1 → T2), el cliente escala a **REBINDING** cuando T2 expira:

```
REBINDING — Comunicación broadcast:

Cliente (192.168.1.100)          CUALQUIER servidor DHCP
   │                                     │
   │════ DHCPREQUEST (broadcast) ═══════►│  255.255.255.255
   │     ciaddr: 192.168.1.100           │
   │     (NO server-identifier)          │
   │                                     │
   │◄═══ DHCPACK ═══════════════════════─│  (otro servidor responde)
   │     yiaddr: 192.168.1.100           │
   │                                     │
   │  → BOUND (nuevo servidor "adopta" el lease)
```

La diferencia fundamental con RENEWING:

| Aspecto | RENEWING | REBINDING |
|---------|----------|-----------|
| Destino | Unicast al servidor original | Broadcast a toda la red |
| Urgencia | Moderada (queda 37.5% del lease) | Alta (queda 12.5% del lease) |
| Propósito | Extender con el mismo servidor | Buscar CUALQUIER servidor |
| Escenario | Operación normal | Servidor original probablemente caído |

**¿Cómo puede otro servidor "adoptar" el lease?**

En entornos con múltiples servidores DHCP (alta disponibilidad), el segundo servidor puede:

1. **Failover configurado**: tiene réplica del estado de leases del primario. Sabe que 192.168.1.100 fue asignada a esa MAC y simplemente extiende el lease.

2. **Sin failover**: el servidor secundario ve el DHCPREQUEST con ciaddr=192.168.1.100. Si esa IP está dentro de su pool y no tiene conflicto, puede asumir el lease.

```
Failover entre dos servidores:

┌─────────────┐     Lease sync      ┌─────────────┐
│  Primario   │◄────────────────────►│ Secundario  │
│ 192.168.1.1 │    (peer state)      │ 192.168.1.2 │
│  ACTIVE     │                      │   STANDBY   │
└──────┬──────┘                      └──────┬──────┘
       │                                     │
       │  Primario cae                       │
       ✗                                     │
                                             │
Cliente ══════ REBIND (broadcast) ══════════►│
                                             │
       ◄═══════════ DHCPACK ════════════════─│
                (secundario asume)
```

---

## 7. Expiración y comportamiento post-lease

Si ningún servidor responde al REBINDING y el lease expira al 100%, el cliente **debe** dejar de usar la IP:

```
Expiración del lease:

16:00  Lease expira
  │
  ├── Cliente DESONFIGURA la interfaz
  │   (elimina la IP, rutas asociadas)
  │
  ├── Aplicaciones pierden conectividad
  │
  └── Cliente vuelve a INIT
      └── Envía DHCPDISCOVER (nuevo DORA)
          └── Si hay servidor → obtiene nueva IP (puede ser diferente)
          └── Si no hay servidor → sin red
```

**Comportamiento según la implementación**:

No todas las implementaciones manejan la expiración de forma idéntica:

| Implementación | Comportamiento al expirar |
|----------------|--------------------------|
| dhclient (ISC) | Elimina IP, ejecuta script de release, reinicia DORA |
| NetworkManager | Elimina IP, intenta reconectar automáticamente |
| systemd-networkd | Elimina IP, reintenta DORA según configuración |
| Windows DHCP | Usa APIPA (169.254.x.x) como fallback temporal |

**APIPA (Automatic Private IP Addressing)**:

Cuando un cliente no puede obtener un lease DHCP, algunos sistemas operativos asignan una IP del rango **169.254.0.0/16** (link-local). Esto permite comunicación básica en la red local sin servidor DHCP, pero sin acceso a Internet:

```
Sin DHCP → fallback APIPA:

┌─────────────────────────────────┐
│  Cliente sin lease DHCP         │
│  Se autoasigna: 169.254.x.y    │
│  (x.y elegidos pseudo-random)  │
│                                 │
│  ✓ Puede hablar con otros      │
│    169.254.x.x en el segmento  │
│  ✗ No tiene gateway → sin      │
│    acceso a otras redes         │
│  ✗ No tiene DNS                │
└─────────────────────────────────┘
```

En Linux, el paquete `avahi-autoipd` proporciona esta funcionalidad, aunque no suele estar habilitado por defecto en servidores.

---

## 8. Lease en el servidor: almacenamiento y gestión

### ISC DHCP (dhcpd)

El servidor almacena el estado de todos los leases en un archivo de texto:

```bash
# Ubicación del archivo de leases
# Debian/Ubuntu:
/var/lib/dhcp/dhcpd.leases

# RHEL/CentOS:
/var/lib/dhcpd/dhcpd.leases
```

```bash
# Ver leases activos
cat /var/lib/dhcp/dhcpd.leases
```

```
# Ejemplo de contenido:
lease 192.168.1.100 {
  starts 6 2026/03/21 08:00:00;
  ends 6 2026/03/21 16:00:00;
  cltt 6 2026/03/21 08:00:00;
  binding state active;
  next binding state free;
  rewind binding state free;
  hardware ethernet aa:bb:cc:11:22:33;
  uid "\001\000\273\314\021\"3";
  client-hostname "dev-laptop";
}

lease 192.168.1.101 {
  starts 6 2026/03/21 09:30:00;
  ends 6 2026/03/21 17:30:00;
  cltt 6 2026/03/21 09:30:00;
  binding state active;
  next binding state free;
  hardware ethernet dd:ee:ff:44:55:66;
  client-hostname "phone-usuario";
}
```

**El archivo de leases es append-only**: cada renovación o cambio añade un nuevo bloque al final. El servidor lee el archivo de arriba a abajo y el último registro para cada IP es el vigente. Periódicamente, el servidor hace **cleanup** reescribiendo el archivo solo con los leases activos (esto genera el archivo `dhcpd.leases~` como backup).

```bash
# Buscar leases por MAC
grep -A 8 "aa:bb:cc:11:22:33" /var/lib/dhcp/dhcpd.leases

# Contar leases activos
grep -c "binding state active" /var/lib/dhcp/dhcpd.leases
```

### Kea DHCP (sucesor de ISC DHCP)

Kea, el sucesor moderno de ISC DHCP, almacena leases en una base de datos (MySQL, PostgreSQL o CSV):

```bash
# Kea con backend CSV (por defecto)
/var/lib/kea/kea-leases4.csv

# Formato CSV:
# address,hwaddr,client_id,valid_lifetime,expire,subnet_id,fqdn_fwd,
# fqdn_rev,hostname,state,user_context
192.168.1.100,aa:bb:cc:11:22:33,,28800,1742637600,1,0,0,dev-laptop,0,
```

```bash
# Kea: consultar leases via API
curl -s -X POST http://localhost:8000/ \
  -H "Content-Type: application/json" \
  -d '{"command": "lease4-get-all", "service": ["dhcp4"]}'

# Kea: buscar lease por IP
curl -s -X POST http://localhost:8000/ \
  -d '{"command": "lease4-get", "service": ["dhcp4"],
       "arguments": {"ip-address": "192.168.1.100"}}'
```

### dnsmasq

dnsmasq almacena leases en un archivo simple:

```bash
# Ubicación por defecto
/var/lib/misc/dnsmasq.leases

# Formato: timestamp MAC IP hostname client-id
1742637600 aa:bb:cc:11:22:33 192.168.1.100 dev-laptop 01:aa:bb:cc:11:22:33
```

---

## 9. Lease en el cliente: archivos y estado

El cliente también almacena información sobre sus leases para poder usarla tras un reinicio (INIT-REBOOT):

### dhclient

```bash
# Archivo de leases del cliente
/var/lib/dhcp/dhclient.leases        # Debian/Ubuntu
/var/lib/dhclient/dhclient.leases    # RHEL/CentOS

# O específico por interfaz
/var/lib/dhcp/dhclient.eth0.leases
```

```
# Contenido del lease del cliente:
lease {
  interface "eth0";
  fixed-address 192.168.1.100;
  option subnet-mask 255.255.255.0;
  option routers 192.168.1.1;
  option domain-name-servers 192.168.1.1, 8.8.8.8;
  option domain-name "home.local";
  option dhcp-lease-time 28800;
  option dhcp-server-identifier 192.168.1.1;
  renew 6 2026/03/21 12:00:00;     ← T1
  rebind 6 2026/03/21 15:00:00;    ← T2
  expire 6 2026/03/21 16:00:00;    ← Fin del lease
}
```

### systemd-networkd

```bash
# systemd-networkd almacena leases como archivos binarios/JSON
/run/systemd/netif/leases/

# Listar leases
ls /run/systemd/netif/leases/

# El nombre del archivo es el índice de la interfaz
cat /run/systemd/netif/leases/2
```

```ini
# Contenido típico:
[DHCPv4]
ADDRESS=192.168.1.100
NETMASK=255.255.255.0
ROUTER=192.168.1.1
DNS=192.168.1.1 8.8.8.8
DOMAINNAME=home.local
LIFETIME=28800
T1=14400
T2=25200
SERVER_ADDRESS=192.168.1.1
NEXT_SERVER=0.0.0.0
```

### NetworkManager

```bash
# NetworkManager almacena leases internamente
# Se pueden consultar con nmcli
nmcli device show eth0 | grep DHCP

# O ver el lease directamente
cat /var/lib/NetworkManager/internal-*.lease

# Información detallada
nmcli -f DHCP4 connection show "Wired connection 1"
```

---

## 10. Reservas estáticas vs leases dinámicos

Las **reservas** (también llamadas asignaciones estáticas) garantizan que un dispositivo específico siempre reciba la misma IP. Técnicamente siguen usando DHCP (DORA), pero el servidor tiene la IP pre-asignada:

```
Lease dinámico vs Reserva estática:

Dinámico:
┌─────────────────┐     DORA     ┌──────────┐
│ MAC: aa:bb:cc:… │────────────►│ Servidor │──► Asigna del pool
│ "¿IP por favor?"│             │  DHCP    │    cualquier IP libre
└─────────────────┘             └──────────┘    (192.168.1.100 hoy,
                                                 .105 mañana)

Reserva:
┌─────────────────┐     DORA     ┌──────────┐
│ MAC: aa:bb:cc:… │────────────►│ Servidor │──► Siempre asigna
│ "¿IP por favor?"│             │  DHCP    │    192.168.1.50
└─────────────────┘             └──────────┘    (reservada para esta MAC)
```

```
# ISC DHCP: reserva estática
host printer-oficina {
    hardware ethernet 00:11:22:33:44:55;
    fixed-address 192.168.1.50;
}

# Kea DHCP: reserva
"reservations": [
    {
        "hw-address": "00:11:22:33:44:55",
        "ip-address": "192.168.1.50",
        "hostname": "printer-oficina"
    }
]

# dnsmasq: reserva
dhcp-host=00:11:22:33:44:55,192.168.1.50,printer-oficina
```

**¿Las reservas tienen lease time?** Sí. Aunque la IP siempre será la misma, el mecanismo DORA y la renovación siguen funcionando. El cliente aún renueva el lease periódicamente. La diferencia es que el servidor siempre responderá con la misma IP.

Cuándo usar reservas:

| Dispositivo | ¿Reserva? | Razón |
|-------------|-----------|-------|
| Impresoras | Sí | Otros dispositivos necesitan encontrarla siempre en la misma IP |
| Servidores internos | Sí (o IP estática) | Servicios que dependen de la IP |
| Access points | Sí | Gestión centralizada por IP |
| Portátiles | No | Rotan entre redes, no necesitan IP fija |
| Teléfonos | No | A menos que se use VoIP con configuración por IP |

---

## 11. Estrategias de duración de lease

La duración del lease es una decisión de diseño que equilibra varios factores:

```
Lease corto                              Lease largo
(minutos-1h)                             (días-semanas)
     │                                        │
     ├── ✓ IPs se reciclan rápido             ├── ✓ Menos tráfico DHCP
     ├── ✓ Cambios de config se propagan      ├── ✓ Funciona sin servidor
     │     rápido (DNS, gateway)              │     por más tiempo
     ├── ✗ Mucho tráfico DHCP                 ├── ✗ IPs "atrapadas" en
     ├── ✗ Servidor caído = red caída         │     dispositivos ausentes
     │     rápidamente                        ├── ✗ Cambios de config
     └── ✗ Más carga en el servidor           │     tardan en propagarse
                                              └── ✗ Pool se agota más fácil
```

Recomendaciones por escenario:

| Escenario | Lease recomendado | Razón |
|-----------|-------------------|-------|
| Cafetería / WiFi público | 1-2 horas | Alta rotación, pool limitado |
| Oficina corporativa | 8-12 horas | Jornada laboral, rotación diaria |
| Red doméstica | 24 horas | Pocos dispositivos, estabilidad |
| Data center / servidores | 7-30 días | Máquinas permanentes, o usar reservas |
| Red de conferencia / evento | 30-60 minutos | Dispositivos van y vienen rápidamente |
| IoT / sensores | 12-24 horas | Estabilidad, pero con renovación |

```
# ISC DHCP: configurar duración
subnet 192.168.1.0 netmask 255.255.255.0 {
    range 192.168.1.100 192.168.1.200;

    default-lease-time 28800;   # 8 horas (si el cliente no pide)
    max-lease-time 86400;       # 24 horas (máximo que puede pedir)
    min-lease-time 3600;        # 1 hora (mínimo aceptado)
}
```

El cliente puede **solicitar** una duración específica (option 51 en el DHCPDISCOVER), pero el servidor decide:

```
Negociación del lease time:

Cliente pide 48h ──► Servidor tiene max=24h ──► Otorga 24h
Cliente pide 30m ──► Servidor tiene min=1h  ──► Otorga 1h
Cliente no pide  ──► Servidor usa default   ──► Otorga 8h (default)
```

**Lease infinito**: es posible configurar un lease "infinito" (`infinite` en ISC DHCP), pero se desaconseja. Si un dispositivo se retira sin hacer DHCPRELEASE (lo cual es común), esa IP queda reservada indefinidamente.

---

## 12. Lease en DHCPv6

DHCPv6 también usa leases, pero introduce conceptos adicionales:

```
DHCPv6 Lease — Identity Associations (IA):

┌──────────────────────────────────────────────┐
│  IA_NA (Non-Temporary Address)               │
│  ├── IAID: identificador de la interfaz      │
│  ├── T1: renewal time                        │
│  ├── T2: rebinding time                      │
│  └── IAAddress                               │
│      ├── IPv6 address                        │
│      ├── preferred-lifetime                  │
│      └── valid-lifetime                      │
└──────────────────────────────────────────────┘
```

Diferencias clave con DHCPv4:

| Aspecto | DHCPv4 | DHCPv6 |
|---------|--------|--------|
| Identificador | MAC (o Client ID) | DUID (DHCP Unique Identifier) |
| Estructura | Un lease = una IP | IA puede contener múltiples IPs |
| Lifetimes | Solo lease time | preferred + valid lifetime |
| Temporizadores | T1/T2 por lease | T1/T2 por IA, lifetimes por dirección |
| Release | Opcional (muchos no lo hacen) | Más riguroso, el cliente debería liberar |

**Preferred vs Valid lifetime**:

```
Timeline de una dirección DHCPv6:

start              preferred expires            valid expires
  │                      │                           │
  │    PREFERRED         │      DEPRECATED           │   INVALID
  │  (uso normal)        │  (no para nuevas          │  (no se puede
  │                      │   conexiones,              │   usar)
  │                      │   existentes OK)           │
  ├──────────────────────┼───────────────────────────┤
```

Este modelo de doble lifetime permite transiciones suaves: cuando una dirección pasa a "deprecated", el host puede seguir usándola para conexiones existentes pero debe preferir otra dirección para conexiones nuevas. Esto es especialmente útil en renumeración de redes.

**DUID (DHCP Unique Identifier)**:

A diferencia de DHCPv4 que usa la MAC, DHCPv6 identifica clientes por DUID. Existen tres tipos:

```
DUID-LLT: Link-Layer + Time     ← Más común
  Tipo(2) + HWType(2) + Time(4) + LinkLayerAddr(6+)

DUID-EN: Enterprise Number
  Tipo(2) + EnterpriseNum(4) + Identifier(variable)

DUID-LL: Link-Layer only
  Tipo(2) + HWType(2) + LinkLayerAddr(6+)
```

El DUID persiste aunque cambie la interfaz de red, lo cual es una mejora sobre depender de la MAC.

---

## 13. Diagnóstico y troubleshooting

### Ver el lease actual del cliente

```bash
# Con dhclient
cat /var/lib/dhcp/dhclient.eth0.leases

# Con systemd-networkd
cat /run/systemd/netif/leases/*

# Con NetworkManager
nmcli -f DHCP4 device show eth0

# Lease time restante (todos los sistemas)
# No hay comando estándar, pero puedes calcular:
# 1. Ver cuándo expira
# 2. Comparar con la hora actual
```

### Forzar renovación

```bash
# dhclient: forzar renovación
sudo dhclient -r eth0    # Release (liberar lease actual)
sudo dhclient eth0       # Obtener nuevo lease

# systemd-networkd: renovar
sudo networkctl renew eth0

# NetworkManager: renovar
sudo nmcli device reapply eth0
# O desconectar/reconectar
sudo nmcli device disconnect eth0 && sudo nmcli device connect eth0
```

### Monitorear tráfico DHCP

```bash
# Capturar todo el tráfico DHCP (puertos 67-68)
sudo tcpdump -i eth0 -n port 67 or port 68 -vv

# Ejemplo de salida durante renovación:
# 12:00:01 IP 192.168.1.100.68 > 192.168.1.1.67: BOOTP/DHCP, Request
#   Client-Ethernet-Address aa:bb:cc:11:22:33
#   Requested-IP Option 50, length 4: 192.168.1.100
#   Lease-Time Option 51, length 4: 28800
#
# 12:00:01 IP 192.168.1.1.67 > 192.168.1.100.68: BOOTP/DHCP, ACK
#   Your-IP 192.168.1.100
#   Lease-Time Option 51, length 4: 28800
```

### Verificar estado en el servidor

```bash
# ISC DHCP: leases activos
grep "binding state active" /var/lib/dhcp/dhcpd.leases | wc -l

# ISC DHCP: buscar lease por IP
grep -A 10 "lease 192.168.1.100" /var/lib/dhcp/dhcpd.leases | tail -11

# ISC DHCP: buscar lease por MAC
grep -B 1 -A 9 "aa:bb:cc:11:22:33" /var/lib/dhcp/dhcpd.leases

# Kea: via API
curl -s -X POST http://localhost:8000/ \
  -d '{"command": "lease4-get-all", "service": ["dhcp4"]}' | jq '.arguments.leases | length'

# dnsmasq: leases activos
cat /var/lib/misc/dnsmasq.leases
```

### Pool agotado

```bash
# Verificar si el pool está lleno
# ISC DHCP: comparar leases activos con el rango
ACTIVE=$(grep -c "binding state active" /var/lib/dhcp/dhcpd.leases)
echo "Leases activos: $ACTIVE"

# Ver en los logs si hay "no free leases"
journalctl -u isc-dhcp-server | grep -i "no free leases"
# o
grep "no free leases" /var/log/syslog

# dnsmasq: en el log aparece
# "no address range available for DHCP request"
```

---

## 14. Errores comunes

### Error 1: Confundir lease time con "tiempo hasta que pierde la IP"

```
INCORRECTO:
"El lease es de 8 horas, así que el cliente tendrá IP durante 8 horas"

CORRECTO:
El cliente tendrá IP INDEFINIDAMENTE mientras pueda renovar.
El lease de 8 horas solo significa que, si no puede contactar
NINGÚN servidor DHCP durante 8 horas consecutivas, perderá la IP.

En operación normal, el cliente renueva al 50% (4 horas) y
obtiene un lease nuevo de 8 horas → nunca expira.
```

### Error 2: Asumir que DHCPRELEASE ocurre automáticamente al desconectar

```
INCORRECTO:
"Cuando desconecto el cable, el cliente envía DHCPRELEASE"

CORRECTO:
La mayoría de las implementaciones NO envían RELEASE al perder
conectividad física. El lease simplemente expira por timeout.

DHCPRELEASE se envía explícitamente solo cuando:
- Se ejecuta "dhclient -r"
- Se ejecuta "systemctl stop systemd-networkd" (en algunos casos)
- El sistema hace shutdown ordenado (no siempre)

Por eso los leases cortos son importantes en redes con alta
rotación: no puedes confiar en que los clientes liberen sus IPs.
```

### Error 3: Editar manualmente el archivo de leases del servidor

```
INCORRECTO:
# Editar el archivo mientras el servidor corre
sudo vim /var/lib/dhcp/dhcpd.leases
# "Voy a borrar este lease para liberar la IP"

CORRECTO:
El servidor mantiene el archivo en memoria. Editar el archivo
directamente puede causar corrupción o ser ignorado.

Para liberar un lease:
# Opción 1: esperar a que expire naturalmente
# Opción 2: reiniciar el servidor (relee el archivo)
sudo systemctl restart isc-dhcp-server
# Opción 3: en Kea, usar la API
curl -X POST http://localhost:8000/ \
  -d '{"command": "lease4-del", "service": ["dhcp4"],
       "arguments": {"ip-address": "192.168.1.100"}}'
```

### Error 4: Configurar T1 > T2 o no dejar margen

```
INCORRECTO:
option dhcp-renewal-time 25200;   # T1 = 7h
option dhcp-rebinding-time 14400; # T2 = 4h  ← ¡T2 < T1!
default-lease-time 28800;         # lease = 8h

CORRECTO:
# Siempre: T1 < T2 < lease-time
# Con margen suficiente en cada fase
option dhcp-renewal-time 14400;   # T1 = 4h (50%)
option dhcp-rebinding-time 25200; # T2 = 7h (87.5%)
default-lease-time 28800;         # lease = 8h

# Regla práctica: mantener los defaults (50% / 87.5%)
# a menos que haya una razón específica para cambiarlos.
```

### Error 5: Ignorar el impacto del lease en la propagación de cambios

```
PROBLEMA:
Cambié el servidor DNS de 192.168.1.1 a 192.168.1.5 en el DHCP.
Los clientes siguen usando el viejo DNS.

RAZÓN:
Los clientes solo reciben nuevas opciones al renovar el lease.
Con lease de 24 horas, pueden pasar hasta 12 horas (T1)
antes de que el primer cliente reciba el cambio.

SOLUCIÓN:
1. Cambiar la opción en el servidor DHCP
2. Reducir temporalmente el lease time ANTES del cambio
   (si el cambio es planificado)
3. O forzar renovación en los clientes:
   # En cada cliente:
   sudo dhclient -r eth0 && sudo dhclient eth0

   # O a nivel de red, reiniciar el servidor DHCP
   # (clientes renovarán cuando T1 expire)
```

---

## 15. Cheatsheet

```
TEMPORIZADORES
──────────────────────────────────────────────
T1 = 50% del lease      → RENEW (unicast)
T2 = 87.5% del lease    → REBIND (broadcast)
100%                     → EXPIRE (pierde IP)

ESTADOS DEL CLIENTE
──────────────────────────────────────────────
INIT        → Sin IP, inicia DORA
BOUND       → IP asignada, funcionando
RENEWING    → Renovando con servidor original
REBINDING   → Buscando cualquier servidor
INIT-REBOOT → Reinicio, pide misma IP

ARCHIVOS DE LEASE
──────────────────────────────────────────────
SERVIDOR:
  ISC DHCP   /var/lib/dhcp/dhcpd.leases
  Kea        /var/lib/kea/kea-leases4.csv
  dnsmasq    /var/lib/misc/dnsmasq.leases

CLIENTE:
  dhclient        /var/lib/dhcp/dhclient.*.leases
  systemd-networkd /run/systemd/netif/leases/*
  NetworkManager   nmcli -f DHCP4 device show IFACE

COMANDOS DE RENOVACIÓN
──────────────────────────────────────────────
dhclient -r eth0 && dhclient eth0    # Release + nuevo
networkctl renew eth0                # systemd-networkd
nmcli device reapply eth0            # NetworkManager

DIAGNÓSTICO
──────────────────────────────────────────────
tcpdump -i eth0 port 67 or port 68   # Capturar DHCP
journalctl -u isc-dhcp-server        # Logs del servidor
grep "no free leases" /var/log/syslog # Pool agotado

CONFIGURACIÓN ISC DHCP
──────────────────────────────────────────────
default-lease-time 28800;    # Default: 8h
max-lease-time 86400;        # Máximo: 24h
min-lease-time 3600;         # Mínimo: 1h

RESERVA ESTÁTICA
──────────────────────────────────────────────
host NAME {
    hardware ethernet MAC;
    fixed-address IP;
}

DHCPv6 LIFETIMES
──────────────────────────────────────────────
preferred-lifetime  → Uso normal
valid-lifetime      → Aún utilizable (deprecated)
T1, T2              → Renovación (igual que v4)
```

---

## 16. Ejercicios

### Ejercicio 1: Analizar un archivo de leases

Dado el siguiente archivo de leases de ISC DHCP:

```
lease 10.0.0.50 {
  starts 6 2026/03/21 06:00:00;
  ends 6 2026/03/21 14:00:00;
  binding state active;
  hardware ethernet aa:11:22:33:44:55;
  client-hostname "workstation-1";
}

lease 10.0.0.51 {
  starts 5 2026/03/20 22:00:00;
  ends 6 2026/03/21 06:00:00;
  binding state active;
  hardware ethernet bb:22:33:44:55:66;
  client-hostname "laptop-juan";
}

lease 10.0.0.51 {
  starts 6 2026/03/21 03:00:00;
  ends 6 2026/03/21 11:00:00;
  binding state active;
  hardware ethernet bb:22:33:44:55:66;
  client-hostname "laptop-juan";
}

lease 10.0.0.52 {
  starts 5 2026/03/20 10:00:00;
  ends 5 2026/03/20 18:00:00;
  binding state free;
  hardware ethernet cc:33:44:55:66:77;
}
```

Asumiendo que la hora actual es `2026/03/21 10:00:00`:

1. ¿Cuántos leases están realmente activos y en qué estado está cada IP?
2. ¿Por qué hay dos entradas para 10.0.0.51? ¿Cuál es la vigente?
3. La IP 10.0.0.50 tiene un lease de 8 horas. ¿En qué estado DHCP está el cliente a las 10:00?
4. ¿Está 10.0.0.52 disponible para asignar a un nuevo cliente?

### Ejercicio 2: Diseñar lease times para una organización

Una empresa tiene estas tres redes:

- **Red de oficinas** (192.168.1.0/24): 150 empleados con portátiles, horario 8:00-18:00, los viernes muchos trabajan remoto (solo 80 presenciales).
- **Red WiFi invitados** (192.168.2.0/24, solo 50 IPs disponibles): visitantes que están entre 1 y 4 horas, picos de 30 visitantes simultáneos.
- **Red de servidores** (10.0.0.0/24): 20 servidores que nunca se apagan.

Para cada red:
1. Define `default-lease-time`, `max-lease-time` y justifica tu elección.
2. ¿Usarías reservas estáticas? ¿Para qué dispositivos?
3. ¿Qué pasaría si configuraras lease de 7 días en la red de invitados?

### Ejercicio 3: Diagnosticar problema de renovación

Un administrador reporta: "Desde ayer cambiamos el servidor DNS en la configuración DHCP de 8.8.8.8 a 1.1.1.1, pero hoy algunos clientes siguen usando 8.8.8.8."

El servidor está configurado con `default-lease-time 86400` (24 horas).

1. Explica por qué los clientes no actualizaron todos al mismo tiempo.
2. ¿Cuánto tiempo máximo puede tardar un cliente en recibir el nuevo DNS?
3. El administrador quiere que el cambio se aplique en menos de 1 hora. ¿Qué pasos seguirías? Describe la secuencia, considerando que no puede forzar renovación en cada cliente individualmente (hay 500).
4. ¿Cómo podrías preparar mejor este tipo de cambios para el futuro?

**Pregunta de reflexión**: Si configuras un lease de 5 minutos y el servidor DHCP cae, ¿cuánto tiempo tienen los clientes antes de perder conectividad? ¿Y si el lease es de 24 horas? ¿Cuál es la paradoja fundamental entre estabilidad ante fallos y agilidad de cambios?

---

> **Siguiente tema**: T03 — Configuración de cliente (dhclient, NetworkManager, systemd-networkd)
