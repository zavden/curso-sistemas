# T03 — Direcciones Privadas y Loopback

> **Objetivo:** Entender los rangos de IPs privadas (RFC 1918), la interfaz de loopback, otras direcciones especiales, y cómo estas se aplican en virtualización — especialmente para evitar el solapamiento de rangos entre redes virtuales.

> Sin erratas detectadas en el material original.

---

## 1. El problema: no hay suficientes IPs para todos

IPv4 tiene 2^32 = ~4.300 millones de direcciones, pero hay más de 15.000 millones de dispositivos conectados. El espacio de IPv4 se agotó oficialmente en 2011.

La solución: **reservar rangos de IPs que se pueden reutilizar infinitamente** en redes internas, sin conflicto, porque nunca salen a internet.

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

El RFC 1918 (1996) define tres rangos reservados exclusivamente para uso privado. Los routers de internet están configurados para **no reenviar** paquetes con estas direcciones:

| Rango | CIDR | Direcciones | Máscara |
|-------|------|-------------|---------|
| 10.0.0.0 – 10.255.255.255 | 10.0.0.0/8 | ~16M | 255.0.0.0 |
| 172.16.0.0 – 172.31.255.255 | 172.16.0.0/12 | ~1M | 255.240.0.0 |
| 192.168.0.0 – 192.168.255.255 | 192.168.0.0/16 | ~65K | 255.255.0.0 |

### Propiedades fundamentales

1. **No ruteables en internet**: un router de internet descarta paquetes con IP origen privada.
2. **Reutilizables**: cada red privada del mundo puede usar los mismos rangos sin conflicto.
3. **Requieren NAT para salir**: el gateway traduce la IP privada a una pública.
4. **Libres de uso**: no necesitas registro ni permiso (a diferencia de las IPs públicas).

---

## 3. 10.0.0.0/8: la red privada grande

Con ~16 millones de direcciones, es el rango más grande. Se subdivide para organizaciones con muchos dispositivos.

| Contexto | Ejemplo de uso |
|----------|---------------|
| Redes corporativas | 10.0.0.0/8 subdividido por departamento |
| AWS VPC | 10.0.0.0/16 |
| Kubernetes | 10.244.0.0/16 (pods), 10.96.0.0/12 (servicios) |
| QEMU user-mode networking | 10.0.2.0/24 (sin libvirt) |
| VPNs corporativas | 10.8.0.0/24 (OpenVPN default) |

### Subdivisión típica empresarial

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

---

## 4. 172.16.0.0/12: la red privada media

Este rango va de 172.16.0.0 a 172.31.255.255. El `/12` es el menos intuitivo porque la frontera no cae en un octeto completo.

### Por qué 172.16 hasta 172.31 y no hasta 172.255

```
172.16.0.0  en binario:  10101100.00010000.00000000.00000000
Máscara /12:             11111111.11110000.00000000.00000000
                                  ────
                                  estos 4 bits varían

Segundo octeto: 0001xxxx = 16 a 31 (decimal)
                00010000 = 16
                00011111 = 31
```

**172.32+ NO es privada.** Solo 172.16 a 172.31 mantienen los primeros 12 bits iguales.

| Contexto | Ejemplo |
|----------|---------|
| Docker | 172.17.0.0/16 (bridge `docker0` por defecto) |
| Docker Compose | 172.18.0.0/16, 172.19.0.0/16 (redes adicionales) |
| AWS VPC | 172.31.0.0/16 (VPC default en muchas regiones) |

---

## 5. 192.168.0.0/16: la red privada doméstica

El rango más familiar: 192.168.0.0 a 192.168.255.255. Es el que usan prácticamente todos los routers domésticos.

| Red | Quién la usa |
|-----|-------------|
| 192.168.0.0/24 | Routers domésticos (Movistar, Vodafone) |
| 192.168.1.0/24 | Routers domésticos (TP-Link, Netgear, muchos ISPs) |
| 192.168.8.0/24 | Routers Huawei |
| 192.168.50.0/24 | Routers ASUS |
| 192.168.122.0/24 | **libvirt** (red NAT default de QEMU/KVM) |

### ¿Por qué libvirt eligió 192.168.122.x?

Para **evitar conflictos** con las redes domésticas más comunes (192.168.0.x y 192.168.1.x):

```
Escenario sin conflicto (diseño real):
  LAN doméstica:   192.168.1.0/24    (router, host, otros)
  Red de VMs:      192.168.122.0/24  (bridge virbr0, VMs)
  → rangos diferentes → sin ambigüedad

Escenario con conflicto (hipotético):
  LAN doméstica:   192.168.1.0/24
  Red de VMs:      192.168.1.0/24    ← MISMO RANGO
  → el host no sabe si 192.168.1.50 es un dispositivo LAN o una VM
```

---

## 6. Cómo una IP privada accede a internet

Una IP privada **no puede** comunicarse directamente con internet. La solución es **NAT** (Network Address Translation), cubierto en profundidad en el siguiente tópico.

### Doble NAT en virtualización

Una VM detrás de libvirt pasa por **dos niveles de NAT**:

```
1. VM (192.168.122.100) envía paquete a google.com
   Origen: 192.168.122.100  →  Destino: 142.250.184.14

2. Host hace NAT nivel 1 (libvirt/iptables):
   Origen: 192.168.1.50  →  Destino: 142.250.184.14

3. Router doméstico hace NAT nivel 2:
   Origen: 85.60.3.22 (IP pública)  →  Destino: 142.250.184.14

4. Google responde a 85.60.3.22
   Router deshace NAT → 192.168.1.50
   Host deshace NAT → 192.168.122.100
   La VM recibe la respuesta
```

El doble NAT funciona bien para tráfico **saliente**. El tráfico **entrante** requiere port forwarding en ambos niveles.

---

## 7. Loopback: 127.0.0.0/8

### No es solo 127.0.0.1

Todo el rango 127.0.0.0/8 (~16 millones de direcciones) está reservado para loopback. En la práctica solo se usa `127.0.0.1`, pero cualquier dirección del rango funciona:

```bash
ping -c 1 127.0.0.1       # funciona
ping -c 1 127.42.42.42    # también funciona
ping -c 1 127.255.255.254 # también funciona
```

### La interfaz lo

Siempre existe, incluso sin tarjetas de red:

```bash
ip addr show lo
# 1: lo: <LOOPBACK,UP,LOWER_UP> mtu 65536 qdisc noqueue state UNKNOWN
#     link/loopback 00:00:00:00:00:00 brd 00:00:00:00:00:00
#     inet 127.0.0.1/8 scope host lo
#        valid_lft forever preferred_lft forever
```

Observaciones:
- **MTU 65536**: mucho más alto que Ethernet (1500) porque no hay red física.
- **MAC 00:00:00:00:00:00**: no es una interfaz real.
- **scope host**: solo válida para comunicación consigo mismo.

### Usos del loopback

**1. Probar servicios localmente:**

```bash
curl http://127.0.0.1:80           # ¿Nginx funciona?
psql -h 127.0.0.1 -U postgres     # ¿PostgreSQL acepta conexiones?
redis-cli -h 127.0.0.1 ping       # ¿Redis responde?
```

**2. Restringir servicios al acceso local (seguridad):**

```bash
# PostgreSQL escuchando solo en loopback:
ss -tlnp | grep 5432
# LISTEN  0  128  127.0.0.1:5432  0.0.0.0:*
# → Solo procesos locales pueden conectarse
```

**3. Desarrollo:**

```bash
python3 -m http.server 8000 --bind 127.0.0.1   # solo local
python3 -m http.server 8000 --bind 0.0.0.0     # expuesto a la red
```

### Loopback en VMs: cada VM tiene el suyo

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

Para acceder desde la VM a un servicio del host, usar la IP de virbr0 (192.168.122.1), no 127.0.0.1.

### /etc/hosts y localhost

```bash
cat /etc/hosts
# 127.0.0.1   localhost localhost.localdomain
# ::1         localhost localhost.localdomain
```

**Cuidado**: en sistemas con IPv6, `localhost` puede resolver a `::1`. Si un servicio solo escucha en IPv4, `curl http://localhost` falla pero `curl http://127.0.0.1` funciona. Diagnosticar con `getent hosts localhost`.

---

## 8. Otras direcciones especiales

| Rango | Nombre | Propósito |
|-------|--------|-----------|
| 169.254.0.0/16 | Link-local (APIPA) | Auto-asignación cuando DHCP falla |
| 100.64.0.0/10 | CGNAT (RFC 6598) | NAT a nivel de ISP |
| 0.0.0.0/8 | "Esta red" | Bind en todas las interfaces / ruta default |
| 224.0.0.0/4 | Multicast | Comunicación uno-a-muchos |
| 240.0.0.0/4 | Reservado (Clase E) | Experimental, no asignable |
| 255.255.255.255 | Broadcast limitado | Broadcast a toda la red local |

### CGNAT: triple NAT con VMs

Si tu ISP usa CGNAT (IP "pública" en 100.64–100.127):

```
VM (192.168.122.x) → Host NAT → Router NAT → ISP CGNAT → Internet
```

Verificar: `curl ifconfig.me` y comparar con la IP del router. Si son diferentes, estás detrás de CGNAT.

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

```
10.x.x.x                  → PRIVADA
127.x.x.x                 → LOOPBACK
169.254.x.x               → LINK-LOCAL (DHCP falló)
172.16.x.x – 172.31.x.x   → PRIVADA
192.168.x.x               → PRIVADA
Cualquier otra cosa        → probablemente PÚBLICA
```

### Ver tu IP privada vs pública

```bash
# IP privada (tu interfaz de red)
ip -brief addr show up
# enp0s3    UP    192.168.1.50/24        ← privada

# IP pública (la que ve internet)
curl -s ifconfig.me
# 85.60.3.22                              ← pública

# Alternativas:
curl -s icanhazip.com
curl -s api.ipify.org
```

### Script para clasificar

```bash
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
        echo "LINK-LOCAL (DHCP falló)"; return
    fi
    echo "PÚBLICA"
}

is_private 192.168.122.100   # PRIVADA (192.168.x.x)
is_private 172.20.0.1        # PRIVADA (172.16-31.x.x)
is_private 172.32.0.1        # PÚBLICA (172.32 fuera del rango)
is_private 8.8.8.8           # PÚBLICA
```

---

## 10. Direcciones privadas en virtualización

En un host con libvirt y Docker, coexisten múltiples redes privadas:

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
│  NAT del host (iptables/nftables):                 │
│  192.168.122.0/24 → MASQUERADE via enp0s3          │
│  172.17.0.0/16    → MASQUERADE via enp0s3          │
│  10.0.1.0/28      → (sin NAT, red aislada)        │
└─────────────────────────────────────────────────────┘
```

### Qué rango usar para cada red virtual

| Escenario | Rango recomendado | Razón |
|-----------|-------------------|-------|
| Red NAT general (default) | 192.168.122.0/24 | Libvirt default, no conflicta con LAN |
| Lab aislado pequeño | 10.0.1.0/28 o 10.0.2.0/28 | Poco desperdicio |
| Red con muchas VMs | 10.10.0.0/16 | Espacio amplio para subnetting |
| Testing Docker + VMs | 172.18.0.0/16 | No conflicta con docker0 (172.17.x) |

### Comunicación entre redes privadas del host

Una VM en 192.168.122.100 **no puede** hablar directamente con un contenedor en 172.17.0.2 — están en redes diferentes con bridges diferentes:

```bash
# ¿El host hace forwarding entre interfaces?
cat /proc/sys/net/ipv4/ip_forward
# 1 = activado (libvirt lo activa)

# Pero además se necesitan reglas de firewall que lo permitan.
# La comunicación cruzada no está permitida por defecto.
```

---

## 11. El problema del solapamiento de rangos

### Escenario: VPN que conflicta con libvirt

```
Tu LAN doméstica:     192.168.1.0/24
Red de VMs (libvirt): 192.168.122.0/24
VPN del trabajo:      192.168.122.0/24    ← CONFLICTO
```

El kernel tiene dos rutas para 192.168.122.0/24 y elige una. Resultado: pierdes acceso a las VMs o a la VPN.

### Cómo evitarlo

```bash
# Antes de crear una red virtual, verificar qué rangos ya están en uso:
ip route
# default via 192.168.1.1 dev enp0s3
# 192.168.1.0/24 dev enp0s3 proto kernel scope link
# 192.168.122.0/24 dev virbr0 proto kernel scope link
# 172.17.0.0/16 dev docker0 proto kernel scope link

# Elegir un rango que NO aparezca aquí.
# Buenos candidatos: 10.0.0.0/24, 10.0.1.0/24, 10.10.0.0/16
```

Cada red virtual debe tener un rango **único** en el host.

---

## 12. Errores comunes

### Error 1: Creer que una IP privada es accesible desde internet

```bash
# "Voy a compartir mi servidor: http://192.168.1.50:8080"
# → Tu amigo NO puede acceder. 192.168.1.50 no existe fuera de tu red.
# Necesitas IP pública + port forwarding en el router.
```

### Error 2: Confundir 172.16.x con el rango completo

```
172.16.0.0 – 172.31.255.255    → PRIVADA
172.32.0.0 – 172.255.255.255   → PÚBLICA

172.20.5.1  → privada ✓
172.35.5.1  → ¡PÚBLICA! ✗
```

### Error 3: Asumir que localhost siempre es 127.0.0.1

```bash
# Con IPv6, localhost puede resolver a ::1
getent hosts localhost
# 127.0.0.1       localhost
# ::1             localhost

# Solución: usar 127.0.0.1 explícitamente donde importa
```

### Error 4: Crear red virtual en el mismo rango que la LAN

```bash
# Tu LAN es 192.168.1.0/24
# Creas red libvirt con 192.168.1.0/24 ← ROUTING AMBIGUO
# Solución: ip route para verificar antes de crear
```

### Error 5: Pensar que 169.254.x.x es funcional

```bash
# inet 169.254.45.67/16 scope link
# → IP de emergencia. Sin gateway, sin DNS. No funciona para nada útil.
# → Arreglar DHCP.
```

---

## Cheatsheet

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
║  REGLAS CLAVE                                                      ║
║  ✓ IPs privadas requieren NAT para salir a internet                ║
║  ✓ Cada red virtual necesita un rango único en el host             ║
║  ✓ Loopback es per-máquina (VM y host: 127.0.0.1 separados)       ║
║  ✓ Verificar ip route ANTES de crear redes virtuales               ║
║  ✗ 172.32+ NO es privada (solo 172.16–172.31)                      ║
║  ✗ 169.254.x.x NO es funcional (arreglar DHCP)                    ║
║                                                                    ║
╚══════════════════════════════════════════════════════════════════════╝
```

---

## Ejercicios

### Ejercicio 1 — Inventario de redes en tu host

```bash
ip -brief addr show up
ip route
```

Anota todas las interfaces, sus IPs y CIDRs. Después:

```bash
curl -s ifconfig.me
```

Preguntas:
1. ¿Cuántas redes privadas diferentes coexisten en tu host?
2. ¿Tu IP pública coincide con alguna IP privada? ¿Por qué?
3. ¿En qué rango RFC 1918 cae cada IP privada?

<details><summary>Predicción</summary>

- `ip -brief addr show up` muestra al menos `lo` (127.0.0.1/8) y una interfaz Ethernet (probablemente 192.168.x.x/24). Si tienes libvirt, aparece `virbr0` (192.168.122.1/24). Si tienes Docker, `docker0` (172.17.0.1/16).
- `ip route` muestra las rutas de cada red. La ruta `default` apunta al gateway de tu LAN.
- `curl ifconfig.me` muestra tu IP pública, que será completamente diferente a tus IPs privadas. La razón: tu router hace NAT, traduciendo tu IP privada a la pública antes de enviar paquetes a internet.
- Típicamente tienes 2-4 redes privadas: LAN doméstica (192.168.x), libvirt (192.168.122.x), Docker (172.17.x), loopback (127.x).

</details>

### Ejercicio 2 — Clasificar IPs

Sin herramientas, clasifica cada IP como privada, pública, loopback, link-local, o CGNAT:

```
a) 192.168.1.50
b) 8.8.8.8
c) 10.0.2.15
d) 172.20.0.1
e) 172.32.0.1
f) 127.0.0.1
g) 169.254.45.67
h) 100.64.1.1
i) 192.169.1.1
j) 10.255.255.254
```

<details><summary>Predicción</summary>

a) `192.168.1.50` → **PRIVADA** (192.168.x.x, RFC 1918)
b) `8.8.8.8` → **PÚBLICA** (DNS de Google)
c) `10.0.2.15` → **PRIVADA** (10.x.x.x, RFC 1918 — es la IP default de QEMU user-mode)
d) `172.20.0.1` → **PRIVADA** (172.16-31.x.x, RFC 1918 — 20 está entre 16 y 31)
e) `172.32.0.1` → **PÚBLICA** (172.32 está **fuera** del rango privado 172.16-31)
f) `127.0.0.1` → **LOOPBACK** (127.x.x.x)
g) `169.254.45.67` → **LINK-LOCAL** (169.254.x.x — DHCP falló)
h) `100.64.1.1` → **CGNAT** (100.64-127.x.x, RFC 6598 — NAT del ISP)
i) `192.169.1.1` → **PÚBLICA** (192.169 NO es 192.168 — un solo dígito de diferencia)
j) `10.255.255.254` → **PRIVADA** (10.x.x.x, RFC 1918 — todo el rango /8 es privado)

Las trampas comunes: (e) 172.32 parece privada pero no lo es, (i) 192.169 no es 192.168.

</details>

### Ejercicio 3 — Loopback: probar y entender

```bash
# 1. Ping a loopback estándar
ping -c 2 127.0.0.1

# 2. Ping a otra dirección del rango loopback
ping -c 1 127.42.42.42

# 3. Ver servicios escuchando en loopback
ss -tlnp | grep 127.0.0.1

# 4. Cómo resuelve localhost
getent hosts localhost
```

Preguntas:
1. ¿Cuál es la latencia de loopback? ¿Por qué es tan baja?
2. ¿Encontraste servicios escuchando solo en 127.0.0.1? ¿Por qué estarían restringidos?
3. ¿`localhost` resolvió a 127.0.0.1, a ::1, o a ambos?

<details><summary>Predicción</summary>

1. La latencia es ~0.03ms o menor. Es extremadamente baja porque el paquete **nunca sale de la máquina** — el kernel lo procesa internamente sin tocar ninguna interfaz de red física.
2. Es probable encontrar servicios como PostgreSQL (5432), MySQL (3306), o Redis (6379) escuchando en 127.0.0.1. Están restringidos por **seguridad**: solo procesos locales pueden conectarse, impidiendo acceso remoto no autorizado.
3. En la mayoría de sistemas modernos, `localhost` resuelve a ambos (`127.0.0.1` y `::1`). `curl http://localhost` usará el primero que funcione (generalmente IPv6 ::1 primero). Si un servicio solo escucha en IPv4, `curl localhost` puede fallar pero `curl 127.0.0.1` funciona.

</details>

### Ejercicio 4 — Loopback per-máquina en VMs

Si una VM tiene nginx escuchando en `127.0.0.1:80`:

1. ¿Desde dentro de la VM, `curl http://127.0.0.1:80` funciona?
2. ¿Desde el host, `curl http://127.0.0.1:80` llega al nginx de la VM?
3. ¿Desde el host, `curl http://192.168.122.100:80` llega al nginx de la VM?
4. ¿Qué debe cambiar para que el host pueda acceder?

<details><summary>Predicción</summary>

1. **Sí** — 127.0.0.1 dentro de la VM es el loopback de la VM. Nginx escucha ahí → responde.
2. **No** — 127.0.0.1 en el host es el loopback **del host**, no de la VM. Cada máquina tiene su propio 127.0.0.1 independiente. Si el host tiene nginx, respondería el nginx del host; si no, conexión rechazada.
3. **No** — nginx escucha en `127.0.0.1:80` (solo loopback). Paquetes que llegan a la VM vía `eth0` (192.168.122.100) no son loopback, así que nginx no los acepta.
4. Cambiar la configuración de nginx para escuchar en `0.0.0.0:80` (todas las interfaces) o en `192.168.122.100:80` (la IP de la interfaz de red). Entonces `curl http://192.168.122.100:80` desde el host funcionará.

</details>

### Ejercicio 5 — IP privada vs IP pública

```bash
# Ver tu IP privada
ip -brief addr show up

# Ver tu IP pública
curl -s ifconfig.me
```

Preguntas:
1. ¿Son iguales? ¿Por qué?
2. Si haces `curl ifconfig.me` desde dentro de una VM con libvirt NAT, ¿verás la misma IP pública que desde el host?
3. ¿Cómo puedes saber si estás detrás de CGNAT?

<details><summary>Predicción</summary>

1. **No son iguales.** Tu interfaz tiene una IP privada (e.g. 192.168.1.50). Tu IP pública (e.g. 85.60.3.22) es la del router, que hace NAT. Todos los dispositivos de tu red comparten la misma IP pública.
2. **Sí**, la misma IP pública. La VM hace NAT a través del host (192.168.122.x → 192.168.1.x), y el host sale vía el router (192.168.1.x → IP pública). Internet ve la misma IP pública del router.
3. Comparar la IP que muestra tu router como "IP WAN" con lo que dice `curl ifconfig.me`. Si son diferentes, tu ISP te da una IP privada (100.64.x.x) y hace CGNAT. También verificar si la IP del router empieza con 100.64-100.127.

</details>

### Ejercicio 6 — El rango 172.16.0.0/12

¿Cuáles de estas IPs son privadas?

```
a) 172.15.255.255
b) 172.16.0.1
c) 172.24.100.50
d) 172.31.255.254
e) 172.32.0.1
```

<details><summary>Predicción</summary>

El rango privado 172.x es solo de 172.**16** a 172.**31** (los 4 bits del segundo octeto deben ser 0001xxxx = 16 a 31).

a) `172.15.255.255` → **PÚBLICA** (15 < 16, está justo fuera del rango)
b) `172.16.0.1` → **PRIVADA** (16 es el inicio del rango)
c) `172.24.100.50` → **PRIVADA** (24 está entre 16 y 31)
d) `172.31.255.254` → **PRIVADA** (31 es el final del rango)
e) `172.32.0.1` → **PÚBLICA** (32 > 31, está justo fuera del rango)

La trampa: 172.15 y 172.32 están a un número de distancia del rango privado, pero son públicas.

</details>

### Ejercicio 7 — Solapamiento de rangos

Tienes esta configuración:

```bash
ip route
# default via 192.168.1.1 dev enp0s3
# 192.168.1.0/24 dev enp0s3
# 192.168.122.0/24 dev virbr0
# 172.17.0.0/16 dev docker0
```

¿Cuál de estos rangos para una nueva red virtual causaría conflicto?

```
a) 10.0.1.0/24
b) 192.168.122.0/24
c) 172.18.0.0/16
d) 192.168.1.0/24
e) 10.10.0.0/16
```

<details><summary>Predicción</summary>

a) `10.0.1.0/24` → **Sin conflicto** ✓ (no aparece en ip route)
b) `192.168.122.0/24` → **CONFLICTO** ✗ (ya existe como virbr0, misma red exacta)
c) `172.18.0.0/16` → **Potencial problema** ⚠ (Docker Compose puede crear redes en 172.18.x. No conflicta con docker0=172.17, pero si en el futuro Docker crea redes adicionales, podría solapar)
d) `192.168.1.0/24` → **CONFLICTO** ✗ (es la LAN doméstica del host, routing ambiguo)
e) `10.10.0.0/16` → **Sin conflicto** ✓ (no aparece en ip route, buena elección)

La regla: siempre ejecutar `ip route` antes de elegir un rango. Conflictos evidentes: (b) y (d). Riesgo futuro: (c).

</details>

### Ejercicio 8 — Planificación de rangos para un lab

Tu LAN es 192.168.1.0/24. Ya tienes libvirt default (192.168.122.0/24) y Docker (172.17.0.0/16).

Necesitas crear 3 redes virtuales:
- **"dmz"**: 10 VMs
- **"internal"**: 50 VMs
- **"mgmt"**: 3 VMs

Elige rango, CIDR, gateway, y rango DHCP para cada una.

<details><summary>Predicción</summary>

Usar el bloque 10.0.0.0/8 para evitar conflictos con todo lo existente:

**dmz (10 VMs):** necesita ≥12 IPs → **/28** (14 usables)
```
Red: 10.0.1.0/28
Gateway: 10.0.1.1
DHCP: 10.0.1.2 – 10.0.1.14
Broadcast: 10.0.1.15
```

**internal (50 VMs):** necesita ≥52 IPs → **/26** (62 usables)
```
Red: 10.0.2.0/26
Gateway: 10.0.2.1
DHCP: 10.0.2.2 – 10.0.2.62
Broadcast: 10.0.2.63
```

**mgmt (3 VMs):** necesita ≥5 IPs → **/29** (6 usables)
```
Red: 10.0.3.0/29
Gateway: 10.0.3.1
DHCP: 10.0.3.2 – 10.0.3.6
Broadcast: 10.0.3.7
```

Ventajas de usar 10.x para todo: no conflicta con la LAN (192.168.1.x), ni con libvirt default (192.168.122.x), ni con Docker (172.17.x). Y 10.0.0.0/8 tiene espacio de sobra para expansión futura.

</details>

### Ejercicio 9 — Doble NAT: seguir un paquete

Una VM con IP 192.168.122.100 (detrás de libvirt NAT) en un host con IP 192.168.1.50 (detrás de un router con IP pública 85.60.3.22) hace `curl google.com`.

Describe la IP de origen del paquete en cada salto:

```
VM → virbr0 (host) → router → internet → Google
```

<details><summary>Predicción</summary>

```
1. VM envía paquete:
   Origen: 192.168.122.100  →  Destino: 142.250.184.14 (google.com)

2. Paquete llega a virbr0 (192.168.122.1). Host aplica NAT nivel 1:
   Origen: 192.168.1.50     →  Destino: 142.250.184.14
   (192.168.122.100 reemplazada por la IP del host en la LAN)

3. Paquete llega al router (192.168.1.1). Router aplica NAT nivel 2:
   Origen: 85.60.3.22       →  Destino: 142.250.184.14
   (192.168.1.50 reemplazada por la IP pública del router)

4. Google recibe y responde a 85.60.3.22.

5. Vuelta:
   Router recibe respuesta para 85.60.3.22 → deshace NAT → 192.168.1.50
   Host recibe en 192.168.1.50 → deshace NAT → 192.168.122.100
   VM recibe la respuesta.
```

Cada nivel de NAT mantiene una tabla de traducciones para saber cómo "deshacer" el NAT en la respuesta. Google nunca ve las IPs privadas — solo ve 85.60.3.22.

</details>

### Ejercicio 10 — Escenario integrado: diagnosticar conectividad

Tienes:
```
Host:       enp0s3 = 192.168.1.50/24
            virbr0 = 192.168.122.1/24
            docker0 = 172.17.0.1/16
VM-web:     eth0 = 192.168.122.100/24 (nginx en 0.0.0.0:80)
VM-db:      eth0 = 192.168.122.101/24 (postgres en 127.0.0.1:5432)
Container:  eth0 = 172.17.0.2/16
```

¿Qué funciona y qué no?

| Test | ¿Funciona? |
|------|:----------:|
| Host → `curl 192.168.122.100:80` | ? |
| Host → `psql -h 192.168.122.101` | ? |
| VM-web → `curl 127.0.0.1:80` | ? |
| VM-web → `ping 192.168.122.101` | ? |
| VM-web → `ping 172.17.0.2` | ? |
| VM-web → `curl ifconfig.me` | ? |
| Container → `ping 192.168.122.100` | ? |

<details><summary>Predicción</summary>

| Test | ¿Funciona? | Razón |
|------|:----------:|-------|
| Host → `curl 192.168.122.100:80` | **Sí** ✓ | nginx escucha en 0.0.0.0 (todas las interfaces). El host alcanza la VM directamente vía virbr0. |
| Host → `psql -h 192.168.122.101` | **No** ✗ | PostgreSQL escucha en 127.0.0.1 (solo loopback). Paquetes que llegan vía eth0 no son loopback → conexión rechazada. |
| VM-web → `curl 127.0.0.1:80` | **Sí** ✓ | nginx escucha en 0.0.0.0 que incluye loopback. Desde la propia VM, 127.0.0.1 funciona. |
| VM-web → `ping 192.168.122.101` | **Sí** ✓ | Ambas VMs en la misma subred (192.168.122.0/24) conectadas al mismo bridge virbr0. |
| VM-web → `ping 172.17.0.2` | **No** ✗ | Redes diferentes (192.168.122.x vs 172.17.x), bridges diferentes. La VM envía al gateway (192.168.122.1), pero las reglas de firewall del host no permiten routing cruzado entre virbr0 y docker0 por defecto. |
| VM-web → `curl ifconfig.me` | **Sí** ✓ | La VM sale vía gateway 192.168.122.1 → host hace NAT → router hace NAT → internet. |
| Container → `ping 192.168.122.100` | **No** ✗ | Misma razón que VM→container pero al revés. Docker y libvirt configuran reglas de firewall independientes. El routing cruzado no está permitido por defecto. |

Clave: `0.0.0.0` acepta de todas partes, `127.0.0.1` solo de la propia máquina. Redes en bridges diferentes no se comunican sin routing explícito.

</details>
