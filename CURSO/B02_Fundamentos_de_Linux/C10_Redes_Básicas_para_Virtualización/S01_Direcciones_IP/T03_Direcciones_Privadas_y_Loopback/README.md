# Direcciones Privadas y Loopback

## Índice

1. [El problema: no hay suficientes IPs para todos](#1-el-problema-no-hay-suficientes-ips-para-todos)
2. [Direcciones privadas: RFC 1918](#2-direcciones-privadas-rfc-1918)
3. [10.0.0.0/8: la red privada grande](#3-100008-la-red-privada-grande)
4. [172.16.0.0/12: la red privada media](#4-17216.0.012-la-red-privada-media)
5. [192.168.0.0/16: la red privada doméstica](#5-19216800016-la-red-privada-doméstica)
6. [Cómo una IP privada accede a internet](#6-cómo-una-ip-privada-accede-a-internet)
7. [Loopback: 127.0.0.0/8](#7-loopback-127.0.008)
8. [Otras direcciones especiales](#8-otras-direcciones-especiales)
9. [¿Pública o privada? Cómo identificar una IP](#9-pública-o-privada-cómo-identificar-una-ip)
10. [Direcciones privadas en virtualización](#10-direcciones-privadas-en-virtualización)
11. [El problema del solapamiento de rangos](#11-el-problema-del-solapamiento-de-rangos)
12. [Errores comunes](#12-errores-comunes)
13. [Cheatsheet](#13-cheatsheet)
14. [Ejercicios](#14-ejercicios)

---

## 1. El problema: no hay suficientes IPs para todos

IPv4 tiene un espacio de 2^32 = ~4.300 millones de direcciones. Parece mucho, pero hay más de 15.000 millones de dispositivos conectados a internet (contando IoT, teléfonos, servidores, etc.). El espacio de IPv4 se agotó oficialmente en 2011.

La solución que permitió que IPv4 sobreviviera hasta hoy es una idea elegante: **reservar rangos de IPs que se pueden reusar infinitamente** en redes internas, sin conflicto, porque nunca salen a internet.

Tu laptop tiene una IP privada (ej. 192.168.1.50). La laptop de tu vecino tiene la misma IP privada (192.168.1.50) en su red. No hay conflicto porque ambas redes son privadas e independientes — solo se comunican con internet a través de NAT, usando una IP pública única por red.

```
Red de tu casa:              Red de tu vecino:
192.168.1.0/24               192.168.1.0/24
├── laptop:  192.168.1.50    ├── laptop:  192.168.1.50   ← misma IP, sin conflicto
├── teléfono: 192.168.1.51   ├── tablet:  192.168.1.51
└── router:  192.168.1.1     └── router:  192.168.1.1
    │ IP pública: 85.60.3.22     │ IP pública: 91.42.7.15
    └──── internet ──────────────┘
```

---

## 2. Direcciones privadas: RFC 1918

El RFC 1918 (1996) define tres rangos reservados exclusivamente para uso privado. Los routers de internet están configurados para **no reenviar** paquetes con estas direcciones como origen o destino:

| Rango | Notación CIDR | Direcciones | Máscara |
|-------|---------------|-------------|---------|
| 10.0.0.0 – 10.255.255.255 | 10.0.0.0/8 | 16,777,216 (~16M) | 255.0.0.0 |
| 172.16.0.0 – 172.31.255.255 | 172.16.0.0/12 | 1,048,576 (~1M) | 255.240.0.0 |
| 192.168.0.0 – 192.168.255.255 | 192.168.0.0/16 | 65,536 (~65K) | 255.255.0.0 |

### Propiedades fundamentales

1. **No ruteables en internet**: si un paquete con IP origen 192.168.x.x llega a un router de internet, lo descarta.
2. **Reutilizables**: cada red privada del mundo puede usar los mismos rangos sin conflicto.
3. **Requieren NAT para salir**: el gateway traduce la IP privada a una IP pública antes de enviar el paquete a internet.
4. **Libres de uso**: no necesitas permiso ni registro para usarlas (a diferencia de las IPs públicas, asignadas por los RIR).

---

## 3. 10.0.0.0/8: la red privada grande

Con ~16 millones de direcciones, este es el rango más grande. Se subdivide en redes más pequeñas para organizaciones con muchos dispositivos o servicios.

### Dónde lo encontrarás

| Contexto | Ejemplo de uso |
|----------|---------------|
| Redes corporativas | 10.0.0.0/8 para toda la empresa, subdividida por departamento |
| AWS VPC | 10.0.0.0/16 (VPC por defecto en algunas regiones) |
| Kubernetes | 10.244.0.0/16 (pods en Flannel), 10.96.0.0/12 (servicios) |
| QEMU user-mode networking | 10.0.2.0/24 (red por defecto sin libvirt) |
| VPNs corporativas | 10.8.0.0/24 (OpenVPN por defecto) |

### Ejemplo con QEMU sin libvirt

Cuando usas QEMU directamente (sin libvirt) con `-net user`, la VM obtiene:

```
IP de la VM:   10.0.2.15
Gateway:       10.0.2.2
DNS:           10.0.2.3
```

Este es el networking "user-mode" de QEMU: sencillo pero limitado (no permite tráfico entrante a la VM sin port forwarding explícito).

### Subdivisión típica en un entorno empresarial

```
10.0.0.0/8  (empresa completa)
├── 10.1.0.0/16    Oficina Madrid
│   ├── 10.1.1.0/24    Planta 1
│   ├── 10.1.2.0/24    Planta 2
│   └── 10.1.10.0/24   Servidores
├── 10.2.0.0/16    Oficina Barcelona
│   ├── 10.2.1.0/24    Empleados
│   └── 10.2.10.0/24   Servidores
└── 10.100.0.0/16  Data center
    ├── 10.100.1.0/24  Producción
    └── 10.100.2.0/24  Staging
```

El rango 10.x.x.x tiene espacio para este tipo de jerarquías sin preocuparse por quedarse sin direcciones.

---

## 4. 172.16.0.0/12: la red privada media

Este rango va de 172.16.0.0 a 172.31.255.255 (~1 millón de direcciones). El `/12` es el menos intuitivo porque la frontera no cae en un octeto completo.

### Por qué 172.16 hasta 172.31 y no hasta 172.255

La máscara /12 significa que los primeros 12 bits son de red:

```
172.16.0.0  en binario:  10101100.00010000.00000000.00000000
Máscara /12:             11111111.11110000.00000000.00000000
                                  ────
                                  estos 4 bits varían

Segundo octeto puede ser: 0001xxxx = 16 a 31 (decimal)
                          00010000 = 16
                          00011111 = 31
```

Los valores del segundo octeto de 16 a 31 son los que mantienen los primeros 12 bits iguales. De ahí el rango 172.16.x.x a 172.31.x.x.

### Dónde lo encontrarás

| Contexto | Ejemplo |
|----------|---------|
| Docker | 172.17.0.0/16 (bridge `docker0` por defecto) |
| AWS VPC | 172.31.0.0/16 (VPC default en muchas regiones) |
| Docker Compose | 172.18.0.0/16, 172.19.0.0/16 (redes adicionales) |

### Docker y 172.17.0.0/16

Docker usa este rango por defecto para su bridge interno:

```bash
ip addr show docker0
# inet 172.17.0.1/16 brd 172.17.255.255 scope global docker0
```

Los contenedores obtienen IPs como 172.17.0.2, 172.17.0.3, etc. Si también usas libvirt (192.168.122.x) y Docker (172.17.x.x), tendrás dos redes privadas diferentes en el mismo host — cada una con su propio bridge y NAT.

---

## 5. 192.168.0.0/16: la red privada doméstica

El rango más familiar: 192.168.0.0 a 192.168.255.255 (~65K direcciones). Es el que usan prácticamente todos los routers domésticos.

### Subredes comunes

| Red | Quién la usa |
|-----|-------------|
| 192.168.0.0/24 | Routers domésticos (Movistar, Vodafone) |
| 192.168.1.0/24 | Routers domésticos (TP-Link, Netgear, muchos ISPs) |
| 192.168.8.0/24 | Routers Huawei |
| 192.168.50.0/24 | Routers ASUS |
| 192.168.122.0/24 | libvirt (red NAT default de QEMU/KVM) |
| 192.168.100.0/24 | Algunos routers como gateway secundario |

### ¿Por qué libvirt eligió 192.168.122.x?

Libvirt usa 192.168.122.0/24 intencionalmente para **evitar conflictos** con las redes domésticas más comunes (192.168.0.x y 192.168.1.x). Si usara 192.168.1.0/24, habría un conflicto en la mayoría de hogares: el host tendría dos redes con el mismo rango y el routing sería ambiguo.

```
Escenario sin conflicto (diseño real de libvirt):
  LAN doméstica:   192.168.1.0/24    (router, host, otros dispositivos)
  Red de VMs:      192.168.122.0/24  (bridge virbr0, VMs)
  → rangos diferentes → sin ambigüedad

Escenario con conflicto (hipotético):
  LAN doméstica:   192.168.1.0/24
  Red de VMs:      192.168.1.0/24    ← mismo rango
  → el host no sabe si 192.168.1.50 es un dispositivo en la LAN o una VM
```

---

## 6. Cómo una IP privada accede a internet

Una IP privada **no puede** comunicarse directamente con internet. Los routers de internet descartan paquetes con IPs privadas como origen. La solución es **NAT** (Network Address Translation), que el próximo tópico cubre en profundidad. Aquí va el resumen conceptual.

### El flujo simplificado

```
1. La VM (192.168.122.100) envía un paquete a google.com (142.250.184.14)
   Origen: 192.168.122.100  →  Destino: 142.250.184.14

2. El paquete llega a virbr0 (192.168.122.1) en el host

3. El host hace NAT:
   Reemplaza el origen 192.168.122.100 por la IP del host (192.168.1.50)
   Origen: 192.168.1.50  →  Destino: 142.250.184.14

4. El router doméstico hace NAT otra vez:
   Reemplaza el origen 192.168.1.50 por la IP pública (85.60.3.22)
   Origen: 85.60.3.22  →  Destino: 142.250.184.14

5. Google responde a 85.60.3.22
   El router deshace NAT → 192.168.1.50
   El host deshace NAT → 192.168.122.100
   La VM recibe la respuesta
```

### Doble NAT en virtualización

Una VM detrás de libvirt NAT pasa por **dos niveles de NAT**:

```
VM (192.168.122.100)
  ↓  NAT nivel 1 (libvirt/iptables en el host)
Host (192.168.1.50)
  ↓  NAT nivel 2 (router doméstico)
Internet (IP pública 85.60.3.22)
```

Este doble NAT funciona bien para tráfico saliente (la VM navega por internet sin problemas). Pero el tráfico **entrante** — que alguien desde internet alcance la VM — requiere port forwarding en ambos niveles, lo que complica las cosas. Por eso las redes tipo bridge (sin NAT) existen como alternativa.

---

## 7. Loopback: 127.0.0.0/8

### No es solo 127.0.0.1

Todo el rango 127.0.0.0/8 (127.0.0.0 a 127.255.255.255, ~16 millones de direcciones) está reservado para loopback. En la práctica, solo se usa `127.0.0.1`, pero cualquier dirección en ese rango funciona:

```bash
ping -c 1 127.0.0.1
# 64 bytes from 127.0.0.1: icmp_seq=1 ttl=64 time=0.032 ms

ping -c 1 127.42.42.42
# 64 bytes from 127.42.42.42: icmp_seq=1 ttl=64 time=0.038 ms

ping -c 1 127.255.255.254
# 64 bytes from 127.255.255.254: icmp_seq=1 ttl=64 time=0.035 ms
```

Todas funcionan y todas apuntan a la propia máquina.

### La interfaz lo

La interfaz de loopback `lo` siempre existe, incluso en un sistema sin ninguna tarjeta de red:

```bash
ip addr show lo
# 1: lo: <LOOPBACK,UP,LOWER_UP> mtu 65536 qdisc noqueue state UNKNOWN
#     link/loopback 00:00:00:00:00:00 brd 00:00:00:00:00:00
#     inet 127.0.0.1/8 scope host lo
#        valid_lft forever preferred_lft forever
#     inet6 ::1/128 scope host noprefixroute
#        valid_lft forever preferred_lft forever
```

Observa:
- **MTU 65536**: mucho más alto que Ethernet (1500), porque no hay red física — los paquetes no salen de la máquina.
- **MAC 00:00:00:00:00:00**: no es una interfaz real, no tiene MAC.
- **scope host**: la dirección solo es válida para comunicación consigo mismo.
- **valid_lft forever**: nunca expira (no viene de DHCP).

### Para qué sirve loopback

**1. Probar servicios localmente:**

```bash
# ¿Nginx está corriendo?
curl http://127.0.0.1:80

# ¿PostgreSQL acepta conexiones?
psql -h 127.0.0.1 -U postgres

# ¿Redis responde?
redis-cli -h 127.0.0.1 ping
```

**2. Restringir servicios al acceso local:**

Muchos servicios se configuran para escuchar solo en loopback por seguridad:

```bash
# PostgreSQL por defecto escucha solo en loopback:
ss -tlnp | grep 5432
# LISTEN  0  128  127.0.0.1:5432  0.0.0.0:*

# Esto significa que solo procesos locales pueden conectarse.
# Si necesitas acceso remoto, cambias bind-address a 0.0.0.0 o a la IP específica.
```

**3. Desarrollo y testing:**

```bash
# Servidor de desarrollo (solo accesible desde tu máquina):
python3 -m http.server 8000 --bind 127.0.0.1

# vs expuesto a la red (accesible desde otros dispositivos):
python3 -m http.server 8000 --bind 0.0.0.0
```

### Loopback en VMs: cada VM tiene el suyo

El loopback es **per-máquina**. Si haces `ping 127.0.0.1` dentro de una VM, es el loopback de la VM, no del host:

```
┌──────── HOST ────────┐     ┌──────── VM ─────────┐
│ lo: 127.0.0.1        │     │ lo: 127.0.0.1       │
│ (loopback del host)  │     │ (loopback de la VM)  │
│                      │     │                      │
│ enp0s3: 192.168.1.50 │     │ eth0: 192.168.122.100│
│ virbr0: 192.168.122.1│←───→│                      │
└──────────────────────┘     └──────────────────────┘

Desde el host:  curl 127.0.0.1:80  →  nginx del HOST
Desde la VM:    curl 127.0.0.1:80  →  nginx de la VM (si tiene)
```

Si necesitas acceder desde la VM a un servicio del host, usa la IP de virbr0 (192.168.122.1), no 127.0.0.1.

### /etc/hosts y localhost

El archivo `/etc/hosts` mapea nombres a IPs localmente, sin consultar DNS:

```bash
cat /etc/hosts
# 127.0.0.1   localhost localhost.localdomain
# ::1         localhost localhost.localdomain
```

Esto permite usar `localhost` como alias de `127.0.0.1`:

```bash
ping localhost
# PING localhost (127.0.0.1) 56(84) bytes of data.
# 64 bytes from localhost (127.0.0.1): icmp_seq=1 ttl=64 time=0.031 ms

curl http://localhost:8080
# equivalente a curl http://127.0.0.1:8080
```

**Cuidado**: en algunos sistemas, `localhost` puede resolver a `::1` (IPv6) en lugar de `127.0.0.1`. Si un servicio solo escucha en IPv4, `curl http://localhost` fallará pero `curl http://127.0.0.1` funcionará. Esto es una fuente frecuente de confusión.

---

## 8. Otras direcciones especiales

Además de las privadas y loopback, hay otros rangos reservados que encontrarás en la práctica:

### 169.254.0.0/16 — Link-local (APIPA)

Cuando un dispositivo tiene DHCP habilitado pero no recibe respuesta, se auto-asigna una IP en el rango 169.254.x.x. Esta IP solo permite comunicación en el mismo segmento de red local (capa 2).

```bash
# Dentro de una VM cuyo DHCP falló:
ip addr show eth0
# inet 169.254.45.67/16 brd 169.254.255.255 scope link eth0
#                                             ──────────
#                                             scope link = solo local
```

**Si ves 169.254.x.x, algo falló.** No es una IP funcional para comunicación general. Diagnóstico:

```bash
# ¿La red virtual está activa?
virsh net-list --all

# ¿libvirtd está corriendo?
systemctl status libvirtd

# Intentar renovar DHCP desde dentro de la VM:
sudo dhclient -v eth0
```

### 100.64.0.0/10 — Carrier-Grade NAT (CGNAT)

Rango reservado para ISPs que hacen NAT a nivel de operador. Si tu IP "pública" empieza con 100.64–100.127, tu ISP te está dando una IP privada y haciendo NAT por ti. Esto significa que estás detrás de **triple NAT** con VMs:

```
VM (192.168.122.x)  →  Host NAT  →  Router NAT  →  ISP CGNAT  →  Internet
```

Esto no afecta la navegación normal, pero hace casi imposible recibir conexiones entrantes (ej. hosting de servidores).

```bash
# Verificar si estás detrás de CGNAT:
# 1. Ver tu IP "pública" según tu router
# 2. Compararla con lo que ve internet
curl ifconfig.me
# Si son diferentes, estás detrás de CGNAT
```

### 0.0.0.0/8 — "Esta red"

Según el contexto:

| Uso | Significado |
|-----|------------|
| `bind 0.0.0.0:80` | Escuchar en todas las interfaces |
| Ruta `0.0.0.0/0` | Ruta por defecto (cualquier destino) |
| Origen en DHCP Discover | "Aún no tengo IP" |

### 224.0.0.0/4 — Multicast

Rango 224.0.0.0 a 239.255.255.255. Usado para comunicación uno-a-muchos (grupos). Rara vez lo configuras directamente.

### 240.0.0.0/4 — Reservado (Clase E)

Reservado para uso experimental. No asignable. En la práctica, nunca lo verás.

### 255.255.255.255 — Broadcast limitado

Broadcast a todos los dispositivos en el segmento de red local. Usado internamente por protocolos como DHCP.

### Mapa visual del espacio IPv4

```
0.0.0.0/8         "Esta red"
─────────────────────────────
1.0.0.0 – 9.255.255.255       IPs públicas
─────────────────────────────
10.0.0.0/8         PRIVADA (RFC 1918) ★
─────────────────────────────
11.0.0.0 – 100.63.255.255     IPs públicas
─────────────────────────────
100.64.0.0/10      CGNAT (operadores)
─────────────────────────────
100.128.0.0 – 126.255.255.255 IPs públicas
─────────────────────────────
127.0.0.0/8        LOOPBACK ★
─────────────────────────────
128.0.0.0 – 172.15.255.255    IPs públicas
─────────────────────────────
172.16.0.0/12      PRIVADA (RFC 1918) ★
─────────────────────────────
172.32.0.0 – 192.167.255.255  IPs públicas
─────────────────────────────
192.168.0.0/16     PRIVADA (RFC 1918) ★
─────────────────────────────
192.169.0.0 – 223.255.255.255 IPs públicas
─────────────────────────────
224.0.0.0/4        Multicast
─────────────────────────────
240.0.0.0/4        Reservado (Clase E)
─────────────────────────────
255.255.255.255    Broadcast limitado
```

---

## 9. ¿Pública o privada? Cómo identificar una IP

### Regla rápida

Si el primer octeto es:

```
10.x.x.x          → PRIVADA
127.x.x.x         → LOOPBACK
169.254.x.x       → LINK-LOCAL (problema)
172.16.x.x – 172.31.x.x → PRIVADA
192.168.x.x       → PRIVADA
Cualquier otra cosa → probablemente PÚBLICA
```

### Ver tu IP privada vs tu IP pública

```bash
# IP privada (la de tu interfaz de red)
ip -brief addr show up
# enp0s3    UP    192.168.1.50/24        ← privada

# IP pública (la que ve internet)
curl -s ifconfig.me
# 85.60.3.22                              ← pública

# Alternativas para IP pública:
curl -s icanhazip.com
curl -s api.ipify.org
curl -s checkip.amazonaws.com
```

La IP privada y la pública son diferentes porque tu router hace NAT: traduce tu IP privada a la IP pública del router antes de enviar paquetes a internet.

### Script rápido para clasificar

```bash
# Verificar si una IP es privada
is_private() {
    local ip="$1"
    local first=$(echo "$ip" | cut -d. -f1)
    local second=$(echo "$ip" | cut -d. -f2)

    if [ "$first" -eq 10 ]; then echo "PRIVADA (10.x.x.x)"; return; fi
    if [ "$first" -eq 172 ] && [ "$second" -ge 16 ] && [ "$second" -le 31 ]; then
        echo "PRIVADA (172.16-31.x.x)"; return
    fi
    if [ "$first" -eq 192 ] && [ "$second" -eq 168 ]; then
        echo "PRIVADA (192.168.x.x)"; return
    fi
    if [ "$first" -eq 127 ]; then echo "LOOPBACK"; return; fi
    if [ "$first" -eq 169 ] && [ "$second" -eq 254 ]; then
        echo "LINK-LOCAL (error DHCP)"; return
    fi
    echo "PÚBLICA"
}

is_private 192.168.122.100   # PRIVADA (192.168.x.x)
is_private 10.0.2.15         # PRIVADA (10.x.x.x)
is_private 8.8.8.8           # PÚBLICA
is_private 172.20.0.1        # PRIVADA (172.16-31.x.x)
is_private 172.32.0.1        # PÚBLICA (172.32 está fuera del rango)
```

---

## 10. Direcciones privadas en virtualización

En un host con libvirt y Docker, coexisten múltiples redes privadas. Cada una tiene su propio bridge, su propio rango, y su propio NAT:

```
┌─────────────────────── HOST ───────────────────────┐
│                                                     │
│  enp0s3:    192.168.1.50/24    (LAN doméstica)     │
│  virbr0:    192.168.122.1/24   (VMs libvirt)       │
│  docker0:   172.17.0.1/16     (contenedores)       │
│  virbr1:    10.0.1.1/28       (lab aislado)        │
│                                                     │
│  ┌── VM1 ──┐  ┌── VM2 ──┐  ┌── Container ──┐      │
│  │ .122.100│  │ .122.101│  │ 172.17.0.2    │      │
│  └─────────┘  └─────────┘  └──────────────┘      │
│                                                     │
│  Tablas de NAT del host (iptables/nftables):       │
│  192.168.122.0/24 → MASQUERADE via enp0s3          │
│  172.17.0.0/16    → MASQUERADE via enp0s3          │
│  10.0.1.0/28      → (sin NAT, red aislada)        │
└─────────────────────────────────────────────────────┘
```

### Qué rango usar para cada red virtual

| Escenario | Rango recomendado | Razón |
|-----------|-------------------|-------|
| Red NAT general (default) | 192.168.122.0/24 | Libvirt default, no conflicta con LAN |
| Lab aislado pequeño | 10.0.1.0/28 o 10.0.2.0/28 | Poco desperdicio, fácil de numerar |
| Red con muchas VMs | 10.10.0.0/16 | Espacio amplio para subnetting |
| Red que simule un entorno corporativo | 10.x.x.x/8 subdividido | Realista |
| Red para testing de Docker + VMs | 172.18.0.0/16 | No conflicta con docker0 (172.17.x) |

### Comunicación entre redes privadas del mismo host

Una VM en 192.168.122.100 **no puede** hablar directamente con un contenedor Docker en 172.17.0.2 — están en redes diferentes con bridges diferentes. El tráfico tendría que pasar por el host (routing entre interfaces):

```bash
# Verificar si el host hace forwarding entre interfaces:
cat /proc/sys/net/ipv4/ip_forward
# 1 = activado (libvirt lo activa automáticamente)
# 0 = desactivado

# Pero además necesitas reglas de firewall que lo permitan.
# libvirt configura sus propias reglas para virbr0;
# Docker configura las suyas para docker0.
# La comunicación cruzada no está permitida por defecto.
```

---

## 11. El problema del solapamiento de rangos

Uno de los problemas más sutiles y frustrantes en virtualización ocurre cuando dos redes privadas usan el mismo rango.

### Escenario: VPN que conflicta con libvirt

```
Tu LAN doméstica:     192.168.1.0/24
Red de VMs (libvirt): 192.168.122.0/24
VPN del trabajo:      192.168.122.0/24    ← CONFLICTO con libvirt
```

Cuando te conectas a la VPN:
- Tu host tiene dos rutas para 192.168.122.0/24: una via virbr0 (VMs) y otra via la VPN.
- El kernel elige una (la más específica o la más reciente) y la otra deja de funcionar.
- Resultado: o pierdes acceso a las VMs o a la VPN.

### Cómo evitarlo

```bash
# Antes de elegir un rango para una red virtual, verifica qué rangos ya están en uso:
ip route
# default via 192.168.1.1 dev enp0s3
# 192.168.1.0/24 dev enp0s3 proto kernel scope link
# 192.168.122.0/24 dev virbr0 proto kernel scope link
# 172.17.0.0/16 dev docker0 proto kernel scope link

# Elige un rango que NO aparezca aquí.
# Buenos candidatos para labs:
#   10.0.0.0/24, 10.0.1.0/24, 10.10.0.0/16
#   172.20.0.0/16 (si Docker no lo usa)
```

### Escenario: dos redes libvirt con el mismo rango

```bash
# Si creas dos redes con el mismo rango:
virsh net-dumpxml net-a
# <ip address='10.0.1.1' netmask='255.255.255.0'>

virsh net-dumpxml net-b
# <ip address='10.0.1.1' netmask='255.255.255.0'>  ← MISMO RANGO

# Resultado: la segunda red no arrancará, o si arranca,
# el routing será ambiguo y las VMs de ambas redes no funcionarán correctamente.
```

Cada red virtual debe tener un rango único en el host.

---

## 12. Errores comunes

### Error 1: Creer que una IP privada es accesible desde internet

```bash
# Desde tu laptop con IP 192.168.1.50:
# "Voy a compartir mi servidor web con un amigo: http://192.168.1.50:8080"
# → Tu amigo NO puede acceder. 192.168.1.50 no existe fuera de tu red.

# Lo que necesitas es tu IP pública + port forwarding en el router:
curl ifconfig.me
# 85.60.3.22
# Tu amigo usa: http://85.60.3.22:8080
# Y tu router debe redirigir el puerto 8080 a 192.168.1.50:8080
```

### Error 2: Confundir 172.16.x con el rango completo hasta 172.255.x

```
172.16.0.0 – 172.31.255.255    → PRIVADA
172.32.0.0 – 172.255.255.255   → PÚBLICA

172.20.5.1  → privada ✓
172.35.5.1  → ¡PÚBLICA! no es privada ✗
```

El rango privado 172.x es solo de 172.16 a 172.31 (el /12 restringe los 4 bits del segundo octeto a 0001xxxx).

### Error 3: Asumir que localhost siempre es 127.0.0.1

```bash
# En /etc/hosts de un sistema con IPv6:
# 127.0.0.1   localhost
# ::1         localhost

# Si un programa resuelve "localhost" puede obtener ::1 en lugar de 127.0.0.1
# Si el servicio solo escucha en IPv4, la conexión falla.

# Diagnóstico:
getent hosts localhost
# 127.0.0.1       localhost
# ::1             localhost

# Solución: usar 127.0.0.1 explícitamente en configuraciones donde importa.
```

### Error 4: Crear una red virtual en el mismo rango que la LAN del host

```bash
# Tu LAN es 192.168.1.0/24
# Creas una red libvirt con 192.168.1.0/24 ← ERROR

# El host no sabe si 192.168.1.50 es tu laptop o una VM.
# Resultado: routing ambiguo, pérdida de conectividad.

# Solución: verificar con ip route antes de crear la red.
```

### Error 5: Pensar que una IP link-local (169.254.x.x) es funcional

```bash
# Dentro de la VM:
ip addr show eth0
# inet 169.254.45.67/16 scope link

# "Ya tiene IP, debería funcionar"
# → NO. Es una IP de emergencia. No tiene gateway, no tiene DNS.
# → No puede acceder a internet ni al host.
# → Necesitas arreglar DHCP.
```

---

## 13. Cheatsheet

```text
╔══════════════════════════════════════════════════════════════════════╗
║              DIRECCIONES PRIVADAS Y LOOPBACK CHEATSHEET             ║
╠══════════════════════════════════════════════════════════════════════╣
║                                                                    ║
║  RANGOS PRIVADOS (RFC 1918)                                        ║
║  10.0.0.0/8         ~16M IPs    Empresas, clouds, QEMU user-mode  ║
║  172.16.0.0/12      ~1M IPs     Docker, AWS VPC                   ║
║  192.168.0.0/16     ~65K IPs    Doméstico, oficinas, libvirt ★    ║
║                                                                    ║
╠══════════════════════════════════════════════════════════════════════╣
║                                                                    ║
║  DIRECCIONES ESPECIALES                                            ║
║  127.0.0.0/8        Loopback (solo la propia máquina)              ║
║  169.254.0.0/16     Link-local (DHCP falló)                        ║
║  100.64.0.0/10      CGNAT (NAT del ISP)                            ║
║  0.0.0.0            "Cualquier interfaz" / "sin IP"                ║
║  255.255.255.255    Broadcast limitado                              ║
║                                                                    ║
╠══════════════════════════════════════════════════════════════════════╣
║                                                                    ║
║  EN VIRTUALIZACIÓN                                                 ║
║  192.168.122.0/24   libvirt default (NAT)                          ║
║  10.0.2.0/24        QEMU user-mode (sin libvirt)                   ║
║  172.17.0.0/16      Docker bridge (docker0)                        ║
║  Cada red virtual necesita un rango único en el host               ║
║                                                                    ║
╠══════════════════════════════════════════════════════════════════════╣
║                                                                    ║
║  DIAGNÓSTICO                                                       ║
║  ip -brief addr             Ver IPs de todas las interfaces        ║
║  curl ifconfig.me           Ver tu IP pública                      ║
║  ip route                   Ver rangos en uso (evitar solapamiento)║
║  cat /etc/hosts             Mapeo localhost → IP                   ║
║  getent hosts localhost     Resolver localhost (IPv4 vs IPv6)      ║
║                                                                    ║
╠══════════════════════════════════════════════════════════════════════╣
║                                                                    ║
║  REGLAS CLAVE                                                      ║
║  ✓ IPs privadas no son ruteables en internet (requieren NAT)       ║
║  ✓ Cada red virtual debe tener un rango único en el host           ║
║  ✓ Loopback es per-máquina (VM y host tienen 127.0.0.1 separados) ║
║  ✓ 169.254.x.x = DHCP falló, no es una IP funcional               ║
║  ✗ No uses el mismo rango que tu LAN para redes virtuales          ║
║  ✗ 172.32+ NO es privada (solo 172.16–172.31)                      ║
║                                                                    ║
╚══════════════════════════════════════════════════════════════════════╝
```

---

## 14. Ejercicios

### Ejercicio 1: Inventario de redes en tu host

Ejecuta los siguientes comandos y construye una tabla con todas las redes privadas activas en tu máquina:

```bash
ip -brief addr show up
ip route
```

Para cada interfaz, anota:

| Interfaz | IP | Rango/CIDR | Tipo (LAN/libvirt/Docker/otro) | ¿Privada? |
|----------|----|-----------|---------------------------------|-----------|
| ¿? | ¿? | ¿? | ¿? | ¿? |

Después ejecuta:
```bash
curl -s ifconfig.me
```

**Preguntas:**
1. ¿Cuántas redes privadas diferentes coexisten en tu host?
2. ¿Tu IP pública coincide con alguna de tus IPs privadas? ¿Por qué no?
3. Si tuvieras que crear una nueva red virtual para un lab, ¿qué rango elegirías para evitar conflictos con las redes que ya tienes? Justifica con base en la salida de `ip route`.
4. Si estás detrás de CGNAT (tu IP "pública" empieza con 100.64–100.127), ¿cuántos niveles de NAT tendría que atravesar un paquete de una VM para llegar a internet?

### Ejercicio 2: Loopback y servicios

Ejecuta los siguientes comandos:

```bash
# 1. Verificar loopback
ping -c 2 127.0.0.1

# 2. Ver si hay servicios escuchando en loopback
ss -tlnp | grep 127.0.0.1

# 3. Verificar que 127.42.42.42 también funciona
ping -c 1 127.42.42.42

# 4. Ver cómo resuelve localhost
getent hosts localhost
```

**Preguntas:**
1. ¿Encontraste algún servicio escuchando solo en 127.0.0.1? ¿Cuál? ¿Por qué crees que eligieron restringirlo a loopback?
2. ¿`localhost` resolvió a 127.0.0.1, a ::1, o a ambos? Si es ambos, ¿cuál usaría `curl http://localhost` por defecto?
3. Si inicias un servidor web con `python3 -m http.server 8080 --bind 127.0.0.1`, ¿podrías acceder desde otra máquina en tu red? ¿Y si cambias a `--bind 0.0.0.0`?
4. Imagina que una VM tiene nginx escuchando en `127.0.0.1:80`. Si desde el host haces `curl http://192.168.122.100:80`, ¿recibirás respuesta? ¿Por qué?

### Ejercicio 3: Planificación de rangos para un laboratorio

Vas a diseñar un laboratorio con las siguientes redes virtuales en libvirt. Tu LAN doméstica es 192.168.1.0/24 y ya tienes la red default de libvirt (192.168.122.0/24).

Necesitas crear:
1. **Red "dmz"**: simula una zona desmilitarizada, necesita 10 VMs.
2. **Red "internal"**: red interna corporativa, necesita 50 VMs.
3. **Red "management"**: red de gestión, solo 3 VMs.

Para cada red:
- Elige un rango privado (no debe solapar con la LAN ni con la default).
- Elige la máscara CIDR más ajustada.
- Indica la IP del gateway, el rango DHCP, y la dirección de broadcast.

**Preguntas reflexivas:**
1. ¿Usaste el mismo bloque privado (10.x, 172.16.x, 192.168.x) para todas o mezclaste? ¿Hay ventaja en usar uno solo?
2. Si más adelante instalas Docker (que usa 172.17.0.0/16), ¿alguno de tus rangos conflictaría?
3. Si un compañero se conecta por VPN y su VPN asigna 10.8.0.0/24, ¿conflicta con alguna de tus redes? ¿Cómo lo verificarías antes de que haya problemas?

---

> **Nota**: las direcciones privadas son la razón por la que IPv4 sigue funcionando a pesar de haberse quedado sin IPs públicas hace más de una década. En virtualización, cada red virtual usa un rango privado — y el principal error del día a día no es conceptual sino práctico: solapamiento de rangos. Antes de crear una red virtual, ejecuta `ip route` y verifica que tu rango elegido no conflicte con nada. Son 5 segundos que te ahorran horas de debugging.
