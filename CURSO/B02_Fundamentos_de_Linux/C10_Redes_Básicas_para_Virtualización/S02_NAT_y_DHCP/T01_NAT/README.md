# NAT (Network Address Translation)

## Índice

1. [El problema: las IPs privadas no llegan a internet](#1-el-problema-las-ips-privadas-no-llegan-a-internet)
2. [Qué es NAT](#2-qué-es-nat)
3. [SNAT: Source NAT](#3-snat-source-nat)
4. [La tabla de traducciones (conntrack)](#4-la-tabla-de-traducciones-conntrack)
5. [MASQUERADE: SNAT dinámico](#5-masquerade-snat-dinámico)
6. [DNAT: Destination NAT (port forwarding)](#6-dnat-destination-nat-port-forwarding)
7. [NAT en libvirt: la red default](#7-nat-en-libvirt-la-red-default)
8. [Cómo libvirt implementa NAT con iptables/nftables](#8-cómo-libvirt-implementa-nat-con-iptablesnftables)
9. [Doble NAT: VM → host → router](#9-doble-nat-vm--host--router)
10. [Port forwarding hacia una VM](#10-port-forwarding-hacia-una-vm)
11. [NAT vs bridge vs red aislada](#11-nat-vs-bridge-vs-red-aislada)
12. [Limitaciones de NAT](#12-limitaciones-de-nat)
13. [Errores comunes](#13-errores-comunes)
14. [Cheatsheet](#14-cheatsheet)
15. [Ejercicios](#15-ejercicios)

---

## 1. El problema: las IPs privadas no llegan a internet

En el tópico anterior vimos que las IPs privadas (10.x, 172.16-31.x, 192.168.x) no son ruteables en internet. Un paquete con origen 192.168.122.100 que llegue a un router de internet será descartado — el router no sabe a qué red privada pertenece esa IP.

Pero tus VMs necesitan acceder a internet: descargar paquetes, actualizar el sistema, consultar APIs. Y tienen IPs privadas. ¿Cómo resuelves esta contradicción?

```
VM (192.168.122.100) quiere hablar con google.com (142.250.184.14)

Paquete:  src=192.168.122.100  dst=142.250.184.14
          ─────────────────
          IP privada → los routers de internet la descartan

¿Solución?  →  NAT
```

---

## 2. Qué es NAT

NAT (Network Address Translation) es un mecanismo por el cual un dispositivo intermedio (el gateway) **modifica las direcciones IP de los paquetes en tránsito**. Reemplaza la IP privada del origen por una IP pública antes de enviar el paquete a internet, y hace la traducción inversa cuando llega la respuesta.

### El concepto en un diagrama

```
                    GATEWAY
                    (hace NAT)
                    ┌─────────┐
VM ──────────────→  │  tabla   │  ──────────────→  Internet
192.168.122.100     │  NAT     │                   google.com
                    │         │
src=.122.100:45678  │ traduce  │  src=85.60.3.22:45678
dst=142.250.x.x     │         │  dst=142.250.x.x
                    └─────────┘

Google responde a 85.60.3.22:45678
                    ┌─────────┐
Internet ────────→  │  tabla   │  ──────────────→  VM
google.com          │  NAT     │                   192.168.122.100
                    │         │
src=142.250.x.x     │ traduce  │  src=142.250.x.x
dst=85.60.3.22:45678│  inverso │  dst=192.168.122.100:45678
                    └─────────┘
```

El gateway mantiene una **tabla de traducciones** que recuerda qué IP privada y puerto corresponden a cada conexión saliente. Cuando la respuesta llega, consulta la tabla para saber a quién enviarla.

### NAT opera en capa 3 y 4

NAT modifica las cabeceras IP (capa 3) y, en la mayoría de los casos, también los puertos TCP/UDP (capa 4). Esta combinación IP+puerto es lo que permite que múltiples dispositivos privados compartan una sola IP pública:

```
VM1: 192.168.122.100:45678  →  85.60.3.22:45678  →  google.com
VM2: 192.168.122.101:52340  →  85.60.3.22:52340  →  github.com
VM3: 192.168.122.102:38921  →  85.60.3.22:38921  →  debian.org
                                ────────────────
                                misma IP pública,
                                puertos diferentes
```

Cuando la respuesta llega a 85.60.3.22:45678, el gateway sabe que es para VM1. Si llega a :52340, es para VM2. Este mecanismo se llama **PAT** (Port Address Translation) o **NAT overload**, aunque coloquialmente se dice simplemente "NAT".

---

## 3. SNAT: Source NAT

SNAT (Source NAT) modifica la dirección de **origen** del paquete saliente. Es el tipo de NAT más común — lo que hacen tu router doméstico y el gateway de libvirt.

### Flujo paso a paso

```
Paso 1: La VM envía el paquete
  src = 192.168.122.100:45678
  dst = 142.250.184.14:443 (google.com HTTPS)

Paso 2: El paquete llega al gateway (virbr0 → host)
  El host ve que el destino NO está en 192.168.122.0/24
  → necesita reenviar el paquete a internet
  → aplica SNAT: reemplaza el origen

Paso 3: SNAT
  src = 192.168.122.100:45678  →  src = 192.168.1.50:45678
                                       ──────────────
                                       IP del host en la LAN

Paso 4: El paquete sale por enp0s3 hacia el router doméstico
  (el router hará otro SNAT con la IP pública)
```

### Configuración manual con iptables

```bash
# SNAT con IP fija (cuando la IP pública es estática)
sudo iptables -t nat -A POSTROUTING \
    -s 192.168.122.0/24 \
    -o enp0s3 \
    -j SNAT --to-source 192.168.1.50
```

Desglose:
- `-t nat`: tabla de NAT del kernel.
- `-A POSTROUTING`: aplicar **después** de la decisión de routing (el paquete ya sabe por dónde sale).
- `-s 192.168.122.0/24`: solo paquetes con origen en la red de VMs.
- `-o enp0s3`: solo paquetes que salen por la interfaz física.
- `-j SNAT --to-source 192.168.1.50`: reemplazar la IP origen por esta.

En la práctica, no necesitas escribir esta regla manualmente — libvirt la crea automáticamente.

---

## 4. La tabla de traducciones (conntrack)

El kernel de Linux mantiene una tabla de seguimiento de conexiones (**conntrack**) que registra cada traducción NAT activa. Sin esta tabla, no podría saber a quién devolver las respuestas.

### Ver la tabla conntrack

```bash
# Instalar herramienta (si no está)
sudo dnf install conntrack-tools    # Fedora/RHEL
sudo apt install conntrack          # Debian/Ubuntu

# Ver todas las conexiones rastreadas
sudo conntrack -L

# Filtrar solo las de la red de VMs
sudo conntrack -L | grep 192.168.122

# Ejemplo de salida:
# tcp  6 117 TIME_WAIT src=192.168.122.100 dst=142.250.184.14 sport=45678 dport=443
#                      src=142.250.184.14 dst=192.168.1.50 sport=443 dport=45678 [ASSURED]
```

### Anatomía de una entrada conntrack

```
tcp  6  117  TIME_WAIT
 │   │   │      │
 │   │   │      └── estado de la conexión TCP
 │   │   └── TTL en segundos (cuánto queda antes de expirar)
 │   └── número de protocolo (6=TCP, 17=UDP)
 └── protocolo

src=192.168.122.100  dst=142.250.184.14  sport=45678  dport=443
─────────────────────────────────────────────────────────────────
Paquete ORIGINAL: VM → internet (IP privada del origen)

src=142.250.184.14  dst=192.168.1.50  sport=443  dport=45678  [ASSURED]
─────────────────────────────────────────────────────────────────────────
Paquete de RESPUESTA esperado: internet → host (IP traducida)
```

La tabla tiene las dos direcciones: la original y la traducida. Cuando llega la respuesta, el kernel busca la segunda línea, la empareja, y deshace la traducción para entregar el paquete a la VM.

### Estadísticas de conntrack

```bash
# Cuántas conexiones se están rastreando
sudo conntrack -C
# 156

# Máximo de conexiones rastreables
cat /proc/sys/net/netfilter/nf_conntrack_max
# 262144

# Si llegas al máximo, los paquetes nuevos se descartan.
# Raramente un problema en labs, pero sí en producción con muchas VMs.
```

---

## 5. MASQUERADE: SNAT dinámico

MASQUERADE es una variante de SNAT que **no requiere especificar la IP de traducción** — usa automáticamente la IP de la interfaz de salida. Es ideal cuando la IP del host es dinámica (asignada por DHCP):

```bash
# SNAT (IP fija):
iptables -t nat -A POSTROUTING -s 192.168.122.0/24 -o enp0s3 \
    -j SNAT --to-source 192.168.1.50

# MASQUERADE (IP dinámica):
iptables -t nat -A POSTROUTING -s 192.168.122.0/24 -o enp0s3 \
    -j MASQUERADE
```

### Diferencias

| Característica | SNAT | MASQUERADE |
|---------------|------|-----------|
| IP de traducción | Fija, especificada en la regla | Automática (IP actual de la interfaz) |
| Rendimiento | Ligeramente mejor (no consulta la IP cada vez) | Ligeramente peor (consulta la IP en cada paquete) |
| Cuándo usar | IP estática del host | IP dinámica del host (DHCP) |
| Qué usa libvirt | — | MASQUERADE (por defecto) |

### ¿Por qué libvirt usa MASQUERADE?

Porque no puede asumir que la IP del host sea estática. Si tu ISP cambia tu IP (o si tu host obtiene IP por DHCP de la red doméstica), MASQUERADE se adapta automáticamente. SNAT con IP fija dejaría de funcionar si la IP cambia.

---

## 6. DNAT: Destination NAT (port forwarding)

DNAT modifica la dirección de **destino** del paquete entrante. Se usa para **redirigir tráfico desde el host hacia una VM** — lo que se conoce como port forwarding.

### El problema

```
Quieres acceder al servidor web de una VM (192.168.122.100:80)
desde otra máquina en tu LAN (192.168.1.0/24).

Pero 192.168.122.100 no existe en la LAN — está detrás de NAT.
```

### La solución con DNAT

```bash
# Redirigir el puerto 8080 del host al puerto 80 de la VM
sudo iptables -t nat -A PREROUTING \
    -p tcp --dport 8080 \
    -j DNAT --to-destination 192.168.122.100:80
```

Ahora, cuando alguien accede a `http://192.168.1.50:8080` (IP del host), el paquete se redirige a la VM:

```
Cliente (192.168.1.200) → Host:8080 → DNAT → VM:80

Antes DNAT:   src=192.168.1.200  dst=192.168.1.50:8080
Después DNAT: src=192.168.1.200  dst=192.168.122.100:80
```

### Flujo completo de port forwarding

```
1. Cliente envía:  src=192.168.1.200:50000  dst=192.168.1.50:8080
                                                 ─────────────────
                                                 IP y puerto del host

2. Host aplica DNAT (PREROUTING):
   dst cambia:     src=192.168.1.200:50000  dst=192.168.122.100:80
                                                 ──────────────────
                                                 IP y puerto de la VM

3. VM responde:    src=192.168.122.100:80   dst=192.168.1.200:50000

4. Host deshace DNAT (conntrack):
   src cambia:     src=192.168.1.50:8080    dst=192.168.1.200:50000
                        ────────────────
                        el cliente ve la respuesta desde el host, no la VM

5. Cliente recibe respuesta — no sabe que una VM la procesó.
```

### DNAT desde el propio host (hairpin NAT)

Un caso sutil: si intentas acceder a `localhost:8080` desde el propio host esperando que DNAT lo redirija a la VM, **no funciona** por defecto. PREROUTING no se aplica a tráfico generado localmente — solo a tráfico que llega desde fuera.

```bash
# Desde el host:
curl http://localhost:8080        # NO pasa por PREROUTING
curl http://192.168.1.50:8080     # NO pasa por PREROUTING (es tráfico local)

# Desde otra máquina:
curl http://192.168.1.50:8080     # SÍ pasa por PREROUTING → DNAT funciona
```

Para acceder desde el host a la VM directamente:

```bash
# Opción más simple: usar la IP de la VM directamente
curl http://192.168.122.100:80

# O agregar regla OUTPUT para tráfico local (más complejo)
sudo iptables -t nat -A OUTPUT \
    -p tcp --dport 8080 \
    -j DNAT --to-destination 192.168.122.100:80
```

---

## 7. NAT en libvirt: la red default

Cuando instalas libvirt, crea automáticamente la red `default` con NAT:

```bash
virsh net-dumpxml default
```

```xml
<network>
  <name>default</name>
  <uuid>a1b2c3d4-...</uuid>
  <forward mode='nat'>
    <nat>
      <port start='1024' end='65535'/>
    </nat>
  </forward>
  <bridge name='virbr0' stp='on' delay='0'/>
  <mac address='52:54:00:xx:xx:xx'/>
  <ip address='192.168.122.1' netmask='255.255.255.0'>
    <dhcp>
      <range start='192.168.122.2' end='192.168.122.254'/>
    </dhcp>
  </ip>
</network>
```

### Desglose del XML

| Elemento | Significado |
|----------|------------|
| `<forward mode='nat'>` | Habilitar NAT para las VMs |
| `<port start='1024' end='65535'/>` | Rango de puertos usados en la traducción |
| `<bridge name='virbr0'>` | Bridge virtual al que se conectan las VMs |
| `stp='on'` | Spanning Tree Protocol activo (evita loops) |
| `<ip address='192.168.122.1'>` | IP del gateway (host en la red de VMs) |
| `netmask='255.255.255.0'` | Máscara /24 |
| `<dhcp>` | Servidor DHCP integrado en libvirt |
| `<range start='.2' end='.254'>` | IPs asignables a VMs por DHCP |

### Verificar que la red está activa

```bash
virsh net-list --all
# Name      State    Autostart   Persistent
# default   active   yes         yes

# Si no está activa:
virsh net-start default

# Para que inicie con el sistema:
virsh net-autostart default
```

### Qué pasa internamente cuando arrancas una VM en la red default

1. Libvirt crea una interfaz TAP (`vnetN`) y la conecta al bridge `virbr0`.
2. La VM arranca y ve su interfaz de red (`eth0` o `enp1s0`).
3. La VM envía un DHCP Discover (broadcast).
4. El servidor DHCP de libvirt (dnsmasq) responde con una IP del rango .2–.254.
5. La VM configura su IP, gateway (.1), y DNS (.1).
6. Cuando la VM envía tráfico a internet, el host aplica MASQUERADE via iptables/nftables.

---

## 8. Cómo libvirt implementa NAT con iptables/nftables

Libvirt no inventa su propio NAT — utiliza las herramientas del kernel de Linux. Dependiendo de la distribución y versión, usa iptables (legacy) o nftables (moderno).

### Ver las reglas NAT generadas por libvirt

```bash
# Con iptables (legacy)
sudo iptables -t nat -L -n -v

# Salida típica (simplificada):
# Chain POSTROUTING
# target     prot  source            destination
# MASQUERADE  all  192.168.122.0/24  !192.168.122.0/24
#                  ────────────────  ────────────────────
#                  origen: red VMs   destino: fuera de la red
```

```bash
# Con nftables (Fedora 39+, RHEL 9+, Debian 11+)
sudo nft list ruleset | grep -A5 "192.168.122"

# Salida típica:
# chain LIBVIRT_PRT {
#     ip saddr 192.168.122.0/24 ip daddr != 192.168.122.0/24 masquerade
# }
```

### Desglose de la regla MASQUERADE

```
MASQUERADE  all  192.168.122.0/24  !192.168.122.0/24
│           │    │                 │
│           │    │                 └── destino: cualquier IP EXCEPTO la red de VMs
│           │    └── origen: la red de VMs
│           └── todos los protocolos (TCP, UDP, ICMP)
└── acción: reemplazar IP origen por la IP de la interfaz de salida
```

La condición `!192.168.122.0/24` como destino es crucial: sin ella, el tráfico VM→VM también pasaría por MASQUERADE, lo que rompería la comunicación directa entre VMs.

### Reglas de firewall (FORWARD)

Además de NAT, libvirt configura reglas para permitir el reenvío de paquetes:

```bash
sudo iptables -L FORWARD -n -v | grep virbr0

# ACCEPT  all  --  virbr0  *   192.168.122.0/24  0.0.0.0/0  state RELATED,ESTABLISHED
# ACCEPT  all  --  *       virbr0  0.0.0.0/0  192.168.122.0/24
# REJECT  all  --  *       virbr0  0.0.0.0/0  0.0.0.0/0
```

Esto permite:
1. Tráfico saliente de las VMs (192.168.122.0/24 → cualquiera).
2. Respuestas a conexiones iniciadas por las VMs (RELATED,ESTABLISHED).
3. Rechazar tráfico entrante no solicitado hacia las VMs.

### ip_forward: el prerequisito

NAT no funciona si el kernel no permite reenviar paquetes entre interfaces. Libvirt habilita esto automáticamente:

```bash
cat /proc/sys/net/ipv4/ip_forward
# 1 = activado (libvirt lo pone a 1)
# 0 = desactivado (NAT no funciona)

# Si necesitas activarlo manualmente:
sudo sysctl -w net.ipv4.ip_forward=1

# Para hacerlo permanente:
echo "net.ipv4.ip_forward = 1" | sudo tee /etc/sysctl.d/99-forward.conf
sudo sysctl --system
```

---

## 9. Doble NAT: VM → host → router

En un entorno doméstico típico con VMs, los paquetes pasan por **dos niveles de NAT**:

```
┌── VM ───────────────────────────────────────────────────┐
│ IP: 192.168.122.100                                     │
│ Gateway: 192.168.122.1                                  │
│                                                         │
│ Paquete original:                                       │
│   src=192.168.122.100:45678  dst=142.250.184.14:443    │
└────────────────────────┬────────────────────────────────┘
                         │
                    NAT nivel 1
                    (libvirt/host)
                         │
┌── HOST ────────────────┴────────────────────────────────┐
│ virbr0: 192.168.122.1                                   │
│ enp0s3: 192.168.1.50                                    │
│                                                         │
│ Después de MASQUERADE:                                  │
│   src=192.168.1.50:45678  dst=142.250.184.14:443       │
└────────────────────────┬────────────────────────────────┘
                         │
                    NAT nivel 2
                    (router doméstico)
                         │
┌── ROUTER ──────────────┴────────────────────────────────┐
│ LAN: 192.168.1.1                                        │
│ WAN: 85.60.3.22 (IP pública)                            │
│                                                         │
│ Después de NAT:                                         │
│   src=85.60.3.22:45678  dst=142.250.184.14:443         │
└────────────────────────┬────────────────────────────────┘
                         │
                    Internet
                         │
                    Google responde a 85.60.3.22:45678
                    Router deshace NAT → 192.168.1.50:45678
                    Host deshace NAT → 192.168.122.100:45678
                    VM recibe la respuesta
```

### ¿Funciona bien?

Para tráfico **saliente** (VM navega internet): sí, sin problemas. Doble NAT es transparente para la VM.

Para tráfico **entrante** (alguien desde internet quiere llegar a la VM): necesitas port forwarding en **ambos** niveles:

```
Internet → puerto 8080 del router (DNAT nivel 2)
         → puerto 8080 del host   (DNAT nivel 1)
         → puerto 80 de la VM
```

Esto es engorroso. Si necesitas que una VM sea accesible desde fuera, la red tipo **bridge** es mejor solución (la VM obtiene una IP directamente en la LAN).

---

## 10. Port forwarding hacia una VM

Aunque libvirt no configura port forwarding automáticamente, puedes hacerlo con iptables o con hooks de libvirt.

### Método 1: iptables manual

```bash
# Redirigir puerto 2222 del host al SSH (22) de la VM
sudo iptables -t nat -A PREROUTING \
    -p tcp -d 192.168.1.50 --dport 2222 \
    -j DNAT --to-destination 192.168.122.100:22

# Permitir el tráfico en FORWARD
sudo iptables -A FORWARD \
    -p tcp -d 192.168.122.100 --dport 22 \
    -m state --state NEW,ESTABLISHED,RELATED \
    -j ACCEPT
```

Ahora, desde cualquier máquina en la LAN:
```bash
ssh -p 2222 user@192.168.1.50
# Se conecta a la VM 192.168.122.100 por SSH
```

### Método 2: acceso directo (más simple)

Si solo necesitas acceder desde el host a la VM, no necesitas port forwarding — usa la IP de la VM directamente:

```bash
# Desde el host:
ssh user@192.168.122.100
curl http://192.168.122.100:80
```

Esto funciona porque el host tiene una interfaz (virbr0) en la red 192.168.122.0/24.

### Método 3: reserva DHCP con IP estática

Para que la VM siempre tenga la misma IP (necesario para port forwarding confiable):

```bash
# Obtener la MAC de la VM
virsh domiflist mi-vm
# Interface  Type    Source   Model    MAC
# vnet0      network default virtio   52:54:00:ab:cd:ef

# Agregar reserva DHCP
virsh net-update default add ip-dhcp-host \
    '<host mac="52:54:00:ab:cd:ef" ip="192.168.122.100"/>' \
    --live --config
```

Ahora la VM siempre recibirá 192.168.122.100 por DHCP.

---

## 11. NAT vs bridge vs red aislada

Libvirt ofrece tres modos principales de red. La elección depende de qué necesita la VM:

### Comparación detallada

```
                    NAT              Bridge           Aislada
                    ───              ──────           ───────
VM → Internet       ✅ (via NAT)     ✅ (directo)     ❌
VM → Host           ✅               ✅               ❌
Host → VM           ✅ (IP directa)  ✅               ❌
VM → VM (misma red) ✅               ✅               ✅
LAN → VM            ❌ (port fwd)    ✅ (IP en LAN)   ❌
Internet → VM       ❌ (doble fwd)   ✅ (si hay ruta)  ❌
Requiere config     Ninguna          Crear bridge      Ninguna
  adicional         (default)        en el host
Aislamiento         Medio            Bajo              Total
```

### Cuándo usar cada una

**NAT** (la opción por defecto):
- Labs de desarrollo donde las VMs necesitan internet.
- Entornos donde no necesitas acceso entrante a las VMs.
- La opción más simple — funciona out-of-the-box.

**Bridge**:
- VMs que ofrecen servicios accesibles desde la LAN (servidores web, bases de datos).
- Entornos que simulan redes "reales" (la VM aparece como una máquina más en la LAN).
- Requiere configurar un bridge en el host (se cubre en S03).

**Aislada**:
- Labs de seguridad o firewall donde las VMs no deben tocar la red real.
- Simulación de redes internas (sin internet por diseño).
- Labs de almacenamiento/RAID donde la red no es relevante.

### Diagrama comparativo

```
NAT:
  VM (.122.100) ─── virbr0 ─── [NAT] ─── enp0s3 ─── router ─── internet
                                  │
                    VM no visible desde LAN

Bridge:
  VM (.1.150) ─── br0 ─── enp0s3 ─── router ─── internet
     │                       │
     └── misma red ──────────┘
     VM visible como un dispositivo más en la LAN

Aislada:
  VM (.50.10) ─── virbr1 ─── VM (.50.20)
                    │
              sin salida a ningún lado
```

---

## 12. Limitaciones de NAT

### 1. Las VMs no son directamente accesibles

Desde fuera de la red NAT, no puedes iniciar una conexión hacia la VM sin port forwarding explícito. La VM puede iniciar conexiones salientes (y recibirá las respuestas gracias a conntrack), pero nadie puede "encontrarla".

### 2. Protocolos con IP embebida en el payload

Algunos protocolos incluyen la dirección IP dentro de los datos del paquete (no solo en la cabecera). NAT solo traduce la cabecera, así que estos protocolos se rompen:

| Protocolo | Problema | Solución |
|-----------|----------|----------|
| FTP modo activo | El servidor intenta conectar de vuelta a la IP privada del cliente | Usar FTP pasivo, o cargar el módulo `nf_nat_ftp` |
| SIP (VoIP) | Los endpoints negocian IPs privadas para el flujo de audio | ALG (Application Layer Gateway) o STUN/TURN |
| IPsec (AH) | AH firma la cabecera IP — NAT la modifica → firma inválida | Usar IPsec en modo ESP con NAT-T |

Para labs de virtualización esto rara vez es un problema. Si lo es, la solución suele ser bridge en lugar de NAT.

### 3. Rendimiento

Cada paquete que pasa por NAT requiere:
1. Buscar en la tabla conntrack.
2. Modificar la cabecera IP.
3. Recalcular checksums de IP y TCP/UDP.

En un host doméstico con pocas VMs, el impacto es despreciable. En un hipervisor con docenas de VMs y tráfico intenso, puede notarse — otro motivo para usar bridge en producción.

### 4. Depuración más compleja

Cuando hay un problema de red con NAT, el paquete pasa por más transformaciones, lo que dificulta el diagnóstico con `tcpdump`:

```bash
# En el host, ves la IP traducida:
sudo tcpdump -i enp0s3 host 142.250.184.14
# src 192.168.1.50  →  ya no ves la IP de la VM

# Para ver la IP original de la VM:
sudo tcpdump -i virbr0 host 142.250.184.14
# src 192.168.122.100  →  IP real de la VM (antes de NAT)
```

Hay que capturar en la interfaz correcta (virbr0 para tráfico pre-NAT, enp0s3 para post-NAT).

---

## 13. Errores comunes

### Error 1: La VM no tiene internet pero sí tiene IP

```bash
# Dentro de la VM:
ip addr show eth0
# inet 192.168.122.100/24   ← tiene IP, bien

ping 192.168.122.1
# PONG ← llega al gateway, bien

ping 8.8.8.8
# No response ← no llega a internet
```

**Diagnóstico en el host:**
```bash
# ¿ip_forward está activo?
cat /proc/sys/net/ipv4/ip_forward
# Si es 0: sudo sysctl -w net.ipv4.ip_forward=1

# ¿Hay reglas NAT?
sudo iptables -t nat -L POSTROUTING -n | grep 192.168.122
# Si no hay regla MASQUERADE: la red de libvirt puede no estar activa

# ¿La red está activa?
virsh net-list --all
# Si está inactive: virsh net-start default
```

### Error 2: Reiniciar iptables borra las reglas de libvirt

```bash
# Si ejecutas esto:
sudo systemctl restart iptables
# o
sudo iptables -F    # flush de TODAS las reglas

# Las reglas NAT de libvirt DESAPARECEN.
# Las VMs pierden acceso a internet.

# Solución: reiniciar la red de libvirt para que regenere las reglas
virsh net-destroy default && virsh net-start default
```

### Error 3: Confundir PREROUTING con OUTPUT para tráfico local

```bash
# Quieres hacer port forwarding desde el host a una VM:
sudo iptables -t nat -A PREROUTING -p tcp --dport 8080 \
    -j DNAT --to-destination 192.168.122.100:80

# Funciona desde otra máquina:
# (desde 192.168.1.200) curl http://192.168.1.50:8080  → OK

# NO funciona desde el propio host:
# (desde el host) curl http://localhost:8080  → Connection refused
# PREROUTING no aplica a tráfico generado localmente.
```

### Error 4: Crear port forwarding sin permitir FORWARD

```bash
# Solo DNAT no es suficiente:
sudo iptables -t nat -A PREROUTING -p tcp --dport 8080 \
    -j DNAT --to-destination 192.168.122.100:80
# El paquete se redirige... pero se descarta en la cadena FORWARD.

# También necesitas:
sudo iptables -A FORWARD -p tcp -d 192.168.122.100 --dport 80 \
    -m state --state NEW,ESTABLISHED,RELATED -j ACCEPT
```

### Error 5: Pensar que NAT proporciona seguridad

NAT oculta las IPs privadas, pero **no es un firewall**. NAT no inspecciona contenido, no bloquea malware, no filtra por protocolo. Las VMs detrás de NAT siguen siendo vulnerables a tráfico malicioso en las conexiones que ellas mismas inician (ej. descargar un archivo infectado).

Para seguridad real, necesitas reglas de firewall (iptables/nftables) además de NAT.

---

## 14. Cheatsheet

```text
╔══════════════════════════════════════════════════════════════════════╗
║                        NAT CHEATSHEET                               ║
╠══════════════════════════════════════════════════════════════════════╣
║                                                                    ║
║  TIPOS DE NAT                                                      ║
║  SNAT         Cambia IP origen (salida a internet)                  ║
║  DNAT         Cambia IP destino (port forwarding entrante)          ║
║  MASQUERADE   SNAT automático (IP dinámica del gateway)             ║
║  PAT          NAT con traducción de puertos (múltiples hosts)       ║
║                                                                    ║
╠══════════════════════════════════════════════════════════════════════╣
║                                                                    ║
║  LIBVIRT NAT (RED DEFAULT)                                         ║
║  Red:      192.168.122.0/24                                        ║
║  Gateway:  192.168.122.1 (virbr0)                                  ║
║  DHCP:     192.168.122.2 – 192.168.122.254                        ║
║  Modo:     <forward mode='nat'/>                                   ║
║  NAT:      MASQUERADE via iptables/nftables                        ║
║                                                                    ║
╠══════════════════════════════════════════════════════════════════════╣
║                                                                    ║
║  COMANDOS DE INSPECCIÓN                                            ║
║  virsh net-dumpxml default         XML de la red                   ║
║  virsh net-list --all              Estado de redes virtuales       ║
║  iptables -t nat -L -n -v          Reglas NAT (legacy)             ║
║  nft list ruleset                  Reglas NAT (nftables)           ║
║  conntrack -L                      Tabla de traducciones activas   ║
║  conntrack -C                      Conteo de conexiones            ║
║  cat /proc/sys/net/ipv4/ip_forward Forwarding habilitado?          ║
║                                                                    ║
╠══════════════════════════════════════════════════════════════════════╣
║                                                                    ║
║  PORT FORWARDING A UNA VM                                          ║
║  iptables -t nat -A PREROUTING \                                   ║
║    -p tcp --dport 2222 \                                           ║
║    -j DNAT --to-destination 192.168.122.100:22                     ║
║  (+ regla FORWARD correspondiente)                                 ║
║                                                                    ║
║  Acceso directo desde el host (sin port forwarding):               ║
║  ssh user@192.168.122.100                                          ║
║                                                                    ║
╠══════════════════════════════════════════════════════════════════════╣
║                                                                    ║
║  NAT vs BRIDGE vs AISLADA                                          ║
║  NAT:     VM sale a internet, no es visible desde LAN              ║
║  Bridge:  VM visible en la LAN como dispositivo físico             ║
║  Aislada: VM solo habla con otras VMs en la misma red              ║
║                                                                    ║
╠══════════════════════════════════════════════════════════════════════╣
║                                                                    ║
║  TROUBLESHOOTING                                                   ║
║  VM sin internet → verificar ip_forward, reglas NAT, red activa    ║
║  Port fwd no funciona → ¿falta regla FORWARD? ¿tráfico local?     ║
║  Reglas NAT desaparecen → ¿reiniciaste iptables? → net-destroy/    ║
║                           net-start para regenerar                  ║
║  tcpdump pre-NAT: capturar en virbr0                               ║
║  tcpdump post-NAT: capturar en enp0s3                              ║
║                                                                    ║
╚══════════════════════════════════════════════════════════════════════╝
```

---

## 15. Ejercicios

### Ejercicio 1: Inspección de NAT en tu host

Si tienes libvirt instalado, ejecuta estos comandos y responde:

```bash
# 1. Ver la red default
virsh net-dumpxml default

# 2. Ver las reglas NAT del kernel
sudo iptables -t nat -L -n -v
# (o sudo nft list ruleset si usas nftables)

# 3. Verificar ip_forward
cat /proc/sys/net/ipv4/ip_forward

# 4. Ver virbr0
ip addr show virbr0
```

**Preguntas:**
1. ¿El `<forward mode>` de la red default es `nat`, `route`, o algo más?
2. ¿Ves una regla MASQUERADE para 192.168.122.0/24 en la tabla NAT? ¿En qué cadena está (PREROUTING, POSTROUTING, OUTPUT)?
3. ¿`ip_forward` está en 1? Si no, ¿qué pasaría con las VMs?
4. Si tienes Docker instalado, ¿ves reglas NAT adicionales para 172.17.0.0/16? ¿Quién las creó?

### Ejercicio 2: Rastreo de un paquete NAT

Imagina este escenario (no necesitas ejecutarlo, pero si tienes VMs puedes verificar):

```
VM:          192.168.122.100
Host:        192.168.1.50 (LAN) / 192.168.122.1 (virbr0)
Router:      192.168.1.1 (LAN) / 85.60.3.22 (IP pública)
Destino:     142.250.184.14 (google.com)
```

La VM ejecuta `curl https://google.com`. Describe el paquete en cada punto:

1. **Sale de la VM**: ¿cuáles son las IPs origen y destino?
2. **Llega a virbr0 en el host**: ¿cambia algo?
3. **Sale por enp0s3 del host** (después de MASQUERADE): ¿cuáles son ahora las IPs?
4. **Sale por el router** (después de NAT del router): ¿cuáles son las IPs finales?
5. **Google responde**: ¿a qué IP responde? Describe el camino de vuelta.

**Pregunta reflexiva:** Si en el paso 3 usaras `tcpdump -i virbr0`, ¿verías la IP de la VM o la del host como origen? ¿Y si capturas en `enp0s3`?

### Ejercicio 3: Diseño de acceso a servicios de VMs

Tienes tres VMs conectadas a la red NAT default de libvirt:

| VM | IP | Servicio | Puerto |
|----|----|---------| ------|
| web-server | 192.168.122.100 | nginx | 80 |
| db-server | 192.168.122.101 | PostgreSQL | 5432 |
| dev-tools | 192.168.122.102 | Gitea | 3000 |

Necesitas:
1. Acceder al nginx desde otra máquina de tu LAN (192.168.1.200). Escribe la(s) regla(s) iptables necesarias. ¿Qué puerto del host elegirías?
2. Acceder a PostgreSQL **solo desde el host** (no desde la LAN). ¿Necesitas port forwarding? ¿O hay una forma más simple?
3. Acceder a Gitea tanto desde el host como desde la LAN. Escribe la configuración completa.

**Preguntas reflexivas:**
1. ¿Por qué no sería buena idea exponer PostgreSQL (puerto 5432) a toda la LAN vía DNAT?
2. Si más adelante necesitas que el nginx sea accesible desde internet, ¿cuántos niveles de port forwarding necesitarías configurar?
3. ¿En qué punto dirías "esto es demasiado complejo, mejor uso bridge"?

---

> **Nota**: NAT es el mecanismo que hace posible que miles de millones de dispositivos con IPs privadas accedan a internet a través de unas pocas IPs públicas. En virtualización, libvirt configura NAT automáticamente para la red default, lo que significa que tus VMs tienen internet sin configurar nada. La complejidad aparece cuando necesitas tráfico entrante (port forwarding) o cuando necesitas diagnosticar problemas — en ambos casos, entender conntrack, MASQUERADE, y la diferencia entre PREROUTING y OUTPUT te ahorra horas.
