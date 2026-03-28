# T01 — NAT (Network Address Translation)

> **Objetivo:** Entender cómo NAT permite que dispositivos con IPs privadas accedan a internet, los tipos de NAT (SNAT, DNAT, MASQUERADE), la tabla conntrack, cómo libvirt implementa NAT automáticamente, y cuándo preferir NAT vs bridge vs red aislada.

## Erratas detectadas en el material original

| Archivo | Línea | Error | Corrección |
|---------|-------|-------|------------|
| `README.max.md` | 56–58 | Ejemplo de DNAT muestra `src: 203.0.113.50:80 → dst: 203.0.113.50` — la IP origen y destino son iguales, y el puerto está en el origen en lugar del destino | En DNAT, un **cliente** envía a la IP pública del servidor. Correcto: `src: <IP_cliente>:<puerto_cliente> → dst: 203.0.113.50:80`, y después de DNAT: `src: <IP_cliente>:<puerto_cliente> → dst: 192.168.122.100:80` |

---

## 1. El problema: las IPs privadas no llegan a internet

Las IPs privadas (10.x, 172.16-31.x, 192.168.x) no son ruteables en internet. Un paquete con origen 192.168.122.100 será descartado por los routers de internet.

Pero tus VMs necesitan internet: descargar paquetes, actualizar, consultar APIs. ¿Cómo resolver esta contradicción?

```
VM (192.168.122.100) quiere hablar con google.com (142.250.184.14)

Paquete:  src=192.168.122.100  dst=142.250.184.14
          ─────────────────
          IP privada → los routers de internet la descartan

¿Solución?  →  NAT
```

---

## 2. Qué es NAT

NAT (Network Address Translation) es un mecanismo por el cual el gateway **modifica las direcciones IP de los paquetes en tránsito**. Reemplaza la IP privada del origen por una IP pública antes de enviar a internet, y hace la traducción inversa en la respuesta.

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
                    │  NAT     │                   192.168.122.100
                    │  inverso │
dst=85.60.3.22:45678│         │  dst=192.168.122.100:45678
                    └─────────┘
```

### NAT opera en capa 3 y 4

NAT modifica cabeceras IP (capa 3) y puertos TCP/UDP (capa 4). La combinación IP+puerto permite que múltiples dispositivos privados compartan una sola IP pública:

```
VM1: 192.168.122.100:45678  →  85.60.3.22:45678  →  google.com
VM2: 192.168.122.101:52340  →  85.60.3.22:52340  →  github.com
VM3: 192.168.122.102:38921  →  85.60.3.22:38921  →  debian.org
                                ────────────────
                                misma IP pública,
                                puertos diferentes
```

Este mecanismo se llama **PAT** (Port Address Translation) o **NAT overload**, aunque coloquialmente se dice simplemente "NAT".

---

## 3. SNAT: Source NAT

SNAT modifica la dirección de **origen** del paquete saliente. Es el tipo más común — lo que hacen tu router doméstico y el gateway de libvirt.

### Flujo paso a paso

```
Paso 1: VM envía paquete
  src=192.168.122.100:45678  dst=142.250.184.14:443

Paso 2: Paquete llega al gateway (virbr0)
  Destino NO está en 192.168.122.0/24 → reenviar a internet → aplica SNAT

Paso 3: SNAT
  src=192.168.122.100:45678  →  src=192.168.1.50:45678
                                     ──────────────
                                     IP del host en la LAN

Paso 4: Sale por enp0s3 hacia el router (que hará otro SNAT con IP pública)
```

### Configuración manual con iptables

```bash
# SNAT con IP fija (IP estática del host)
sudo iptables -t nat -A POSTROUTING \
    -s 192.168.122.0/24 \
    -o enp0s3 \
    -j SNAT --to-source 192.168.1.50
```

| Parte | Significado |
|-------|------------|
| `-t nat` | Tabla de NAT del kernel |
| `-A POSTROUTING` | Aplicar **después** de la decisión de routing |
| `-s 192.168.122.0/24` | Solo paquetes con origen en la red de VMs |
| `-o enp0s3` | Solo paquetes que salen por la interfaz física |
| `-j SNAT --to-source` | Reemplazar IP origen por esta |

En la práctica, libvirt crea esta regla automáticamente.

---

## 4. La tabla de traducciones (conntrack)

El kernel mantiene una tabla de seguimiento de conexiones (**conntrack**) que registra cada traducción NAT activa. Sin esta tabla, no podría saber a quién devolver las respuestas.

### Ver la tabla conntrack

```bash
# Instalar
sudo dnf install conntrack-tools    # Fedora/RHEL
sudo apt install conntrack          # Debian/Ubuntu

# Ver conexiones de la red de VMs
sudo conntrack -L | grep 192.168.122

# Ejemplo de salida:
# tcp  6 117 TIME_WAIT src=192.168.122.100 dst=142.250.184.14 sport=45678 dport=443
#                      src=142.250.184.14 dst=192.168.1.50 sport=443 dport=45678 [ASSURED]
```

### Anatomía de una entrada

```
tcp  6  117  TIME_WAIT
 │   │   │      └── estado de la conexión TCP
 │   │   └── TTL en segundos
 │   └── número de protocolo (6=TCP, 17=UDP)
 └── protocolo

src=192.168.122.100  dst=142.250.184.14  sport=45678  dport=443
─────── Paquete ORIGINAL: VM → internet (IP privada) ──────────

src=142.250.184.14  dst=192.168.1.50  sport=443  dport=45678  [ASSURED]
─────── Paquete de RESPUESTA esperado: internet → host (IP traducida) ──
```

### Estadísticas

```bash
sudo conntrack -C                                    # conexiones activas
cat /proc/sys/net/netfilter/nf_conntrack_max          # máximo (default: 262144)
# Si llegas al máximo, paquetes nuevos se descartan
```

---

## 5. MASQUERADE: SNAT dinámico

MASQUERADE es una variante de SNAT que usa automáticamente la IP de la interfaz de salida, sin especificarla. Ideal cuando la IP del host es dinámica (DHCP):

```bash
# SNAT (IP fija):
iptables -t nat -A POSTROUTING -s 192.168.122.0/24 -o enp0s3 \
    -j SNAT --to-source 192.168.1.50

# MASQUERADE (IP dinámica):
iptables -t nat -A POSTROUTING -s 192.168.122.0/24 -o enp0s3 \
    -j MASQUERADE
```

| Característica | SNAT | MASQUERADE |
|---------------|------|-----------|
| IP de traducción | Fija, en la regla | Automática (IP actual de la interfaz) |
| Rendimiento | Ligeramente mejor | Ligeramente peor (consulta IP cada vez) |
| Cuándo usar | IP estática del host | IP dinámica (DHCP) |
| Qué usa libvirt | — | **MASQUERADE** (por defecto) |

Libvirt usa MASQUERADE porque no puede asumir que la IP del host sea estática.

---

## 6. DNAT: Destination NAT (port forwarding)

DNAT modifica la dirección de **destino** del paquete entrante. Se usa para redirigir tráfico desde el host hacia una VM.

### El problema

```
Quieres acceder al servidor web de una VM (192.168.122.100:80)
desde otra máquina en tu LAN (192.168.1.0/24).
Pero 192.168.122.100 no existe en la LAN — está detrás de NAT.
```

### La solución

```bash
# Redirigir puerto 8080 del host al puerto 80 de la VM
sudo iptables -t nat -A PREROUTING \
    -p tcp --dport 8080 \
    -j DNAT --to-destination 192.168.122.100:80
```

### Flujo completo

```
1. Cliente envía:  src=192.168.1.200:50000  dst=192.168.1.50:8080
                                                 IP del host

2. Host aplica DNAT (PREROUTING):
                   src=192.168.1.200:50000  dst=192.168.122.100:80
                                                 IP de la VM

3. VM responde:    src=192.168.122.100:80   dst=192.168.1.200:50000

4. Host deshace DNAT (conntrack):
                   src=192.168.1.50:8080    dst=192.168.1.200:50000
                   El cliente ve la respuesta desde el host, no la VM.
```

### Hairpin NAT: PREROUTING no aplica a tráfico local

```bash
# Desde otra máquina en la LAN:
curl http://192.168.1.50:8080     # SÍ pasa por PREROUTING → DNAT funciona

# Desde el propio host:
curl http://localhost:8080        # NO pasa por PREROUTING → Connection refused
curl http://192.168.1.50:8080     # NO pasa por PREROUTING (tráfico local)

# Desde el host, acceder directamente a la VM (más simple):
curl http://192.168.122.100:80
```

---

## 7. NAT en libvirt: la red default

```bash
virsh net-dumpxml default
```

```xml
<network>
  <name>default</name>
  <forward mode='nat'>
    <nat>
      <port start='1024' end='65535'/>
    </nat>
  </forward>
  <bridge name='virbr0' stp='on' delay='0'/>
  <ip address='192.168.122.1' netmask='255.255.255.0'>
    <dhcp>
      <range start='192.168.122.2' end='192.168.122.254'/>
    </dhcp>
  </ip>
</network>
```

| Elemento | Significado |
|----------|------------|
| `<forward mode='nat'>` | Habilitar NAT para las VMs |
| `<port start='1024' end='65535'/>` | Rango de puertos en la traducción |
| `<bridge name='virbr0'>` | Bridge virtual para las VMs |
| `stp='on'` | Spanning Tree Protocol (evita loops) |
| `<ip address='192.168.122.1'>` | Gateway (host en la red de VMs) |
| `<dhcp>` + `<range>` | DHCP integrado (dnsmasq), IPs .2–.254 |

### Qué pasa internamente al arrancar una VM

1. Libvirt crea interfaz TAP (`vnetN`) y la conecta a `virbr0`.
2. La VM arranca y ve su interfaz de red (`eth0` o `enp1s0`).
3. La VM envía DHCP Discover (broadcast).
4. dnsmasq (servidor DHCP de libvirt) responde con IP del rango .2–.254.
5. La VM configura IP, gateway (.1), y DNS (.1).
6. Tráfico a internet → host aplica MASQUERADE vía iptables/nftables.

### Verificar estado

```bash
virsh net-list --all
# Si inactive: virsh net-start default
# Auto-inicio: virsh net-autostart default
```

---

## 8. Cómo libvirt implementa NAT con iptables/nftables

### Reglas NAT

```bash
# iptables (legacy)
sudo iptables -t nat -L -n -v
# MASQUERADE  all  192.168.122.0/24  !192.168.122.0/24

# nftables (Fedora 39+, RHEL 9+, Debian 11+)
sudo nft list ruleset | grep -A5 "192.168.122"
# ip saddr 192.168.122.0/24 ip daddr != 192.168.122.0/24 masquerade
```

La condición `!192.168.122.0/24` como destino es crucial: sin ella, el tráfico VM→VM también pasaría por MASQUERADE, rompiendo la comunicación directa entre VMs.

### Reglas FORWARD

```bash
sudo iptables -L FORWARD -n -v | grep virbr0
# ACCEPT  virbr0→*   192.168.122.0/24  0.0.0.0/0  state RELATED,ESTABLISHED
# ACCEPT  *→virbr0   0.0.0.0/0  192.168.122.0/24
# REJECT  *→virbr0   0.0.0.0/0  0.0.0.0/0
```

Permite tráfico saliente de VMs y respuestas, rechaza tráfico entrante no solicitado.

### ip_forward: el prerequisito

```bash
cat /proc/sys/net/ipv4/ip_forward
# 1 = activado (libvirt lo pone a 1)
# 0 = NAT no funciona

# Activar manualmente:
sudo sysctl -w net.ipv4.ip_forward=1

# Permanente:
echo "net.ipv4.ip_forward = 1" | sudo tee /etc/sysctl.d/99-forward.conf
sudo sysctl --system
```

---

## 9. Doble NAT: VM → host → router

En un entorno doméstico, los paquetes pasan por **dos niveles de NAT**:

```
┌── VM ──────────────────────────────────────────┐
│ src=192.168.122.100:45678  dst=142.250.x.x:443 │
└───────────────────┬────────────────────────────┘
               NAT nivel 1 (libvirt/host)
┌── HOST ───────────┴────────────────────────────┐
│ src=192.168.1.50:45678     dst=142.250.x.x:443 │
└───────────────────┬────────────────────────────┘
               NAT nivel 2 (router doméstico)
┌── ROUTER ─────────┴────────────────────────────┐
│ src=85.60.3.22:45678       dst=142.250.x.x:443 │
└───────────────────┬────────────────────────────┘
               Internet → Google responde a 85.60.3.22:45678
               Router deshace NAT → 192.168.1.50:45678
               Host deshace NAT → 192.168.122.100:45678
```

- **Tráfico saliente**: funciona sin problemas, transparente para la VM.
- **Tráfico entrante**: necesita port forwarding en **ambos** niveles (engorroso). Si necesitas acceso entrante, bridge es mejor solución.

---

## 10. Port forwarding hacia una VM

### Método 1: iptables manual

```bash
# Redirigir puerto 2222 del host al SSH (22) de la VM
sudo iptables -t nat -A PREROUTING \
    -p tcp -d 192.168.1.50 --dport 2222 \
    -j DNAT --to-destination 192.168.122.100:22

# Permitir el tráfico en FORWARD (necesario)
sudo iptables -A FORWARD \
    -p tcp -d 192.168.122.100 --dport 22 \
    -m state --state NEW,ESTABLISHED,RELATED \
    -j ACCEPT

# Desde la LAN:
ssh -p 2222 user@192.168.1.50    # se conecta a la VM
```

### Método 2: acceso directo (más simple)

Si solo necesitas acceder desde el host:

```bash
ssh user@192.168.122.100       # sin port forwarding
curl http://192.168.122.100:80
```

### Reserva DHCP para IP estática

Para que la VM siempre tenga la misma IP (necesario para port forwarding confiable):

```bash
virsh domiflist mi-vm
# Interface  Type    Source   Model    MAC
# vnet0      network default virtio   52:54:00:ab:cd:ef

virsh net-update default add ip-dhcp-host \
    '<host mac="52:54:00:ab:cd:ef" ip="192.168.122.100"/>' \
    --live --config
```

---

## 11. NAT vs bridge vs red aislada

```
                    NAT              Bridge           Aislada
                    ───              ──────           ───────
VM → Internet       ✅ (via NAT)     ✅ (directo)     ❌
VM → Host           ✅               ✅               ❌
Host → VM           ✅ (IP directa)  ✅               ❌
VM → VM (misma red) ✅               ✅               ✅
LAN → VM            ❌ (port fwd)    ✅ (IP en LAN)   ❌
Internet → VM       ❌ (doble fwd)   ✅ (si hay ruta)  ❌
Config adicional    Ninguna (default) Crear bridge     Ninguna
Aislamiento         Medio            Bajo              Total
```

```
NAT:      VM (.122.100) ── virbr0 ── [NAT] ── enp0s3 ── router ── internet
          VM no visible desde LAN

Bridge:   VM (.1.150) ── br0 ── enp0s3 ── router ── internet
          VM visible como dispositivo más en la LAN

Aislada:  VM (.50.10) ── virbr1 ── VM (.50.20)
          Sin salida a ningún lado
```

**NAT**: labs, VMs que necesitan internet sin acceso entrante. Out-of-the-box.
**Bridge**: VMs que ofrecen servicios accesibles desde la LAN. Requiere configurar bridge.
**Aislada**: labs de seguridad, simulación sin internet.

---

## 12. Limitaciones de NAT

### 1. VMs no directamente accesibles

Sin port forwarding, nadie puede iniciar conexiones hacia la VM desde fuera.

### 2. Protocolos con IP embebida en payload

| Protocolo | Problema | Solución |
|-----------|----------|----------|
| FTP activo | Servidor intenta conectar a IP privada | FTP pasivo, o módulo `nf_nat_ftp` |
| SIP (VoIP) | Endpoints negocian IPs privadas | ALG o STUN/TURN |
| IPsec (AH) | AH firma cabecera IP, NAT la invalida | ESP con NAT-T |

### 3. Rendimiento

Cada paquete: buscar conntrack + modificar cabecera + recalcular checksums. Despreciable en labs, potencialmente notable con docenas de VMs.

### 4. Depuración más compleja

```bash
# Pre-NAT (IP de la VM visible):
sudo tcpdump -i virbr0 host 142.250.184.14

# Post-NAT (IP traducida):
sudo tcpdump -i enp0s3 host 142.250.184.14
```

### 5. NAT no es un firewall

NAT oculta IPs privadas pero no inspecciona contenido, no bloquea malware. Las VMs siguen siendo vulnerables en conexiones que ellas inician.

---

## 13. Errores comunes

### Error 1: VM tiene IP pero no internet

```bash
# Diagnóstico en el host:
cat /proc/sys/net/ipv4/ip_forward              # ¿es 1?
sudo iptables -t nat -L POSTROUTING -n | grep 192.168.122   # ¿hay MASQUERADE?
virsh net-list --all                           # ¿red activa?
```

### Error 2: Reiniciar iptables borra reglas de libvirt

```bash
sudo systemctl restart iptables   # o sudo iptables -F
# → Reglas NAT de libvirt DESAPARECEN

# Solución: regenerar reglas reiniciando la red
virsh net-destroy default && virsh net-start default
```

### Error 3: PREROUTING no aplica a tráfico local

```bash
# Port forwarding funciona desde otra máquina pero NO desde el host.
# PREROUTING solo procesa tráfico que llega desde fuera.
# Solución: usar IP de la VM directamente desde el host.
```

### Error 4: DNAT sin regla FORWARD

```bash
# Solo DNAT no basta — el paquete se redirige pero se descarta en FORWARD.
# Necesitas ambas reglas:
# 1. DNAT en PREROUTING
# 2. ACCEPT en FORWARD para el destino
```

### Error 5: Pensar que NAT = seguridad

NAT oculta IPs pero no filtra contenido. Para seguridad real, necesitas reglas de firewall.

---

## Cheatsheet

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
║  XML:     <forward mode='nat'/>                                    ║
║  NAT:     MASQUERADE via iptables/nftables                         ║
║  Red:     192.168.122.0/24, gateway .1, DHCP .2–.254               ║
║                                                                    ║
╠══════════════════════════════════════════════════════════════════════╣
║                                                                    ║
║  INSPECCIÓN                                                        ║
║  virsh net-dumpxml default         XML de la red                   ║
║  iptables -t nat -L -n -v          Reglas NAT                     ║
║  conntrack -L                      Traducciones activas            ║
║  cat /proc/sys/net/ipv4/ip_forward ¿Forwarding activo?             ║
║  tcpdump -i virbr0                 Tráfico pre-NAT                 ║
║  tcpdump -i enp0s3                 Tráfico post-NAT                ║
║                                                                    ║
╠══════════════════════════════════════════════════════════════════════╣
║                                                                    ║
║  PORT FORWARDING                                                   ║
║  iptables -t nat -A PREROUTING -p tcp --dport 2222 \               ║
║    -j DNAT --to-destination 192.168.122.100:22                     ║
║  (+ regla FORWARD)                                                 ║
║  Desde el host: usar IP de la VM directamente (más simple)         ║
║                                                                    ║
╠══════════════════════════════════════════════════════════════════════╣
║                                                                    ║
║  TROUBLESHOOTING                                                   ║
║  VM sin internet → ip_forward=1? reglas NAT? red activa?           ║
║  Port fwd falla → ¿falta FORWARD? ¿tráfico local?                 ║
║  Reglas desaparecen → net-destroy + net-start para regenerar       ║
║                                                                    ║
╚══════════════════════════════════════════════════════════════════════╝
```

---

## Ejercicios

### Ejercicio 1 — Inspección de NAT en tu host

```bash
virsh net-dumpxml default 2>/dev/null || echo "libvirt no disponible"
sudo iptables -t nat -L -n -v 2>/dev/null
cat /proc/sys/net/ipv4/ip_forward
ip addr show virbr0 2>/dev/null
```

Preguntas:
1. ¿El `<forward mode>` es `nat`, `route`, u otro?
2. ¿Ves MASQUERADE para 192.168.122.0/24? ¿En qué cadena (PREROUTING, POSTROUTING, OUTPUT)?
3. ¿`ip_forward` está en 1?

<details><summary>Predicción</summary>

1. El modo es **`nat`** — es el default de libvirt. `route` solo hace routing sin traducción de IP. `bridge` no existe como forward mode (es un tipo de red diferente).
2. Sí, la regla MASQUERADE está en **POSTROUTING** — se aplica después de la decisión de routing, justo antes de que el paquete salga por la interfaz física. PREROUTING es para DNAT (tráfico entrante), OUTPUT para tráfico generado por el host.
3. Debe estar en **1**. Si es 0, las VMs no podrían acceder a internet — el kernel descartaría paquetes destinados a salir por otra interfaz. Libvirt lo activa automáticamente al arrancar una red NAT.

</details>

### Ejercicio 2 — Rastrear un paquete a través de doble NAT

```
VM:          192.168.122.100
Host:        192.168.1.50 (LAN) / 192.168.122.1 (virbr0)
Router:      192.168.1.1 (LAN) / 85.60.3.22 (IP pública)
Destino:     142.250.184.14 (google.com)
```

La VM ejecuta `curl https://google.com`. Describe src/dst en cada punto:

1. Sale de la VM
2. Sale del host (después de MASQUERADE)
3. Sale del router (después de NAT del router)
4. Google responde
5. El paquete llega de vuelta a la VM

<details><summary>Predicción</summary>

1. **Sale de la VM:**
   `src=192.168.122.100:45678  dst=142.250.184.14:443`

2. **Sale del host (MASQUERADE):**
   `src=192.168.1.50:45678  dst=142.250.184.14:443`
   (IP privada de la VM reemplazada por IP del host en la LAN)

3. **Sale del router (NAT nivel 2):**
   `src=85.60.3.22:45678  dst=142.250.184.14:443`
   (IP privada del host reemplazada por IP pública del router)

4. **Google responde:**
   `src=142.250.184.14:443  dst=85.60.3.22:45678`

5. **Vuelta:**
   - Router deshace NAT: `dst=192.168.1.50:45678`
   - Host deshace MASQUERADE: `dst=192.168.122.100:45678`
   - VM recibe la respuesta

**tcpdump**: en `virbr0` verías la IP de la VM (192.168.122.100) como origen. En `enp0s3` verías la IP del host (192.168.1.50) — la VM ya es invisible.

</details>

### Ejercicio 3 — SNAT vs MASQUERADE

¿Cuál usarías en cada caso? Justifica:

1. Host con IP estática 192.168.1.50 en un servidor empresarial
2. Host con IP dinámica (DHCP) en casa
3. Red de libvirt que debe funcionar en cualquier host

<details><summary>Predicción</summary>

1. **SNAT** — la IP es estática y conocida. SNAT es ligeramente más eficiente porque no necesita consultar la IP de la interfaz en cada paquete. `iptables -t nat -A POSTROUTING -s 192.168.122.0/24 -o eth0 -j SNAT --to-source 192.168.1.50`.

2. **MASQUERADE** — la IP puede cambiar con cada lease DHCP. Si usaras SNAT con la IP actual y esta cambia, la regla dejaría de funcionar hasta que la actualices manualmente.

3. **MASQUERADE** — libvirt no puede saber de antemano la IP del host ni si es estática o dinámica. MASQUERADE se adapta automáticamente a cualquier IP. Por eso libvirt siempre usa MASQUERADE por defecto.

</details>

### Ejercicio 4 — Conntrack: interpretar la tabla

Dada esta entrada de conntrack, responde las preguntas:

```
tcp  6  300  ESTABLISHED src=192.168.122.101 dst=93.184.216.34 sport=52340 dport=80
                          src=93.184.216.34 dst=192.168.1.50 sport=80 dport=52340 [ASSURED]
```

1. ¿Qué VM inició la conexión?
2. ¿A qué servidor se conectó?
3. ¿La IP del host en la LAN es...?
4. ¿El servidor web ve la IP de la VM o la del host?
5. ¿Cuánto tiempo queda antes de que esta entrada expire?

<details><summary>Predicción</summary>

1. La VM con IP **192.168.122.101** (primera línea, campo `src`).
2. El servidor en **93.184.216.34** puerto 80 (HTTP). Esta IP es de example.com.
3. La IP del host es **192.168.1.50** — aparece como `dst` en la segunda línea (es la IP traducida que el servidor ve como origen).
4. El servidor ve **192.168.1.50** (la IP del host, post-MASQUERADE). La IP de la VM (192.168.122.101) nunca sale a internet.
5. **300 segundos** (5 minutos). Es el TTL restante para esta entrada. Después de 300s sin actividad, la entrada se elimina y la conexión se considera expirada.

</details>

### Ejercicio 5 — Port forwarding: escribir las reglas

Tienes una VM con IP 192.168.122.100 ejecutando nginx en el puerto 80. Quieres que otras máquinas de tu LAN (192.168.1.0/24) puedan acceder vía el puerto 8080 del host (192.168.1.50).

Escribe las reglas iptables necesarias.

<details><summary>Predicción</summary>

Se necesitan **dos reglas**:

```bash
# 1. DNAT: redirigir puerto 8080 del host al 80 de la VM
sudo iptables -t nat -A PREROUTING \
    -p tcp -d 192.168.1.50 --dport 8080 \
    -j DNAT --to-destination 192.168.122.100:80

# 2. FORWARD: permitir el tráfico al destino
sudo iptables -A FORWARD \
    -p tcp -d 192.168.122.100 --dport 80 \
    -m state --state NEW,ESTABLISHED,RELATED \
    -j ACCEPT
```

Sin la regla FORWARD, el paquete se redirige (DNAT funciona) pero se descarta al intentar pasar entre interfaces.

Desde la LAN: `curl http://192.168.1.50:8080` → funciona.
Desde el host: `curl http://localhost:8080` → **no funciona** (PREROUTING no procesa tráfico local). Desde el host, usar `curl http://192.168.122.100:80` directamente.

</details>

### Ejercicio 6 — Hairpin NAT

Explica por qué este escenario falla:

```bash
# En el host:
sudo iptables -t nat -A PREROUTING -p tcp --dport 8080 \
    -j DNAT --to-destination 192.168.122.100:80

# Desde el propio host:
curl http://localhost:8080
# Connection refused
```

¿Cómo accederías al nginx de la VM desde el host?

<details><summary>Predicción</summary>

**Por qué falla:** La cadena PREROUTING solo procesa paquetes que **llegan desde fuera** del host (tráfico recibido por una interfaz de red). El tráfico generado localmente (como `curl localhost`) pasa directamente a la cadena OUTPUT, no a PREROUTING. Como la regla DNAT está en PREROUTING, no se aplica al tráfico local.

**Soluciones:**

1. **La más simple** — usar la IP de la VM directamente:
   ```bash
   curl http://192.168.122.100:80
   ```
   El host tiene una interfaz (virbr0) en la red 192.168.122.0/24, así que puede alcanzar la VM sin NAT.

2. **Agregar regla OUTPUT** (más complejo):
   ```bash
   sudo iptables -t nat -A OUTPUT -p tcp --dport 8080 \
       -j DNAT --to-destination 192.168.122.100:80
   ```
   Esto aplica DNAT al tráfico local. Pero la opción 1 es preferible por su simplicidad.

</details>

### Ejercicio 7 — Diagnosticar VM sin internet

Una VM tiene IP 192.168.122.100, puede hacer ping al gateway (192.168.122.1), pero `ping 8.8.8.8` no responde.

Describe los pasos de diagnóstico en el host:

<details><summary>Predicción</summary>

El hecho de que la VM llega al gateway (192.168.122.1) confirma que la red interna funciona. El problema está en el reenvío o en NAT.

1. **¿ip_forward está activo?**
   ```bash
   cat /proc/sys/net/ipv4/ip_forward
   ```
   Si es `0`, el kernel descarta paquetes que deben pasar entre interfaces. Solución: `sudo sysctl -w net.ipv4.ip_forward=1`.

2. **¿Hay reglas MASQUERADE?**
   ```bash
   sudo iptables -t nat -L POSTROUTING -n | grep 192.168.122
   ```
   Si no hay regla, los paquetes salen con IP privada y son descartados en internet. Puede ocurrir si reiniciaste iptables o la red de libvirt.

3. **¿La red de libvirt está activa?**
   ```bash
   virsh net-list --all
   ```
   Si está "inactive", las reglas NAT no existen. `virsh net-start default` las regenera.

4. **¿El host tiene internet?**
   ```bash
   ping 8.8.8.8    # desde el host
   ```
   Si el host tampoco tiene internet, el problema no es NAT sino la red física.

5. **¿Hay reglas FORWARD que bloqueen?**
   ```bash
   sudo iptables -L FORWARD -n -v | grep virbr0
   ```
   Un firewall agresivo (como firewalld) podría bloquear el tráfico.

Orden de probabilidad: 1 > 3 > 2 > 5 > 4.

</details>

### Ejercicio 8 — MASQUERADE con condición !destino

¿Por qué la regla NAT de libvirt incluye `!192.168.122.0/24` como destino?

```
MASQUERADE  all  192.168.122.0/24  !192.168.122.0/24
```

¿Qué pasaría si la condición fuera solo `MASQUERADE all 192.168.122.0/24 0.0.0.0/0`?

<details><summary>Predicción</summary>

La condición `!192.168.122.0/24` significa: "aplica MASQUERADE solo cuando el destino NO está en la red de VMs".

**Sin esa condición:** el tráfico VM→VM también pasaría por MASQUERADE. Cuando VM1 (192.168.122.100) envía a VM2 (192.168.122.101), el host traduciría el origen a su propia IP (192.168.1.50). VM2 recibiría el paquete con origen 192.168.1.50 en lugar de 192.168.122.100. La comunicación directa entre VMs se rompería — cada VM vería al host como el origen de todo el tráfico.

**Con la condición:** el tráfico VM→VM pasa directamente por el bridge (virbr0) sin MASQUERADE. Solo el tráfico que sale de la red de VMs (hacia internet o la LAN) se traduce.

Esta es una regla de diseño fundamental: NAT solo para tráfico que cruza la frontera entre red privada y exterior.

</details>

### Ejercicio 9 — NAT vs bridge: elegir la red

Para cada escenario, ¿NAT, bridge, o aislada?

1. Lab de desarrollo: 3 VMs que necesitan `apt update` pero no necesitan ser accesibles desde la LAN.
2. Servidor web en VM que debe ser accesible desde toda la LAN.
3. Lab de seguridad: 5 VMs simulando una red interna, sin contacto con internet.
4. VM con base de datos accesible solo desde el host.
5. VM que debe tener IP en la LAN como si fuera una máquina física.

<details><summary>Predicción</summary>

1. **NAT** — las VMs necesitan internet (apt update) pero no necesitan ser accesibles. NAT funciona out-of-the-box sin configuración adicional.

2. **Bridge** — el servidor web necesita ser accesible desde toda la LAN. Con NAT, necesitarías port forwarding. Con bridge, la VM obtiene una IP directa en la LAN y es accesible como cualquier otra máquina.

3. **Aislada** — por diseño, las VMs no deben tocar internet ni la red real. Red aislada garantiza el aislamiento total. Las VMs solo pueden hablar entre ellas.

4. **NAT** — acceso solo desde el host no necesita port forwarding. El host puede alcanzar directamente 192.168.122.x vía virbr0. No hay razón para usar bridge ya que no necesita acceso desde la LAN.

5. **Bridge** — "como si fuera una máquina física" es exactamente lo que bridge hace: la VM obtiene una IP en el mismo rango que la LAN del host y aparece como un dispositivo más.

</details>

### Ejercicio 10 — Diseño de acceso a servicios de VMs

Tienes 3 VMs en la red NAT default:

| VM | IP | Servicio | Puerto |
|----|----|---------| ------|
| web | 192.168.122.100 | nginx | 80 |
| db | 192.168.122.101 | PostgreSQL | 5432 |
| git | 192.168.122.102 | Gitea | 3000 |

Requisitos:
1. nginx accesible desde la LAN (192.168.1.0/24) en el puerto 80 del host.
2. PostgreSQL accesible solo desde el host.
3. Gitea accesible desde la LAN en el puerto 3000 del host.

Escribe las reglas y explica.

<details><summary>Predicción</summary>

**1. nginx (LAN → host:80 → VM:80):**

```bash
sudo iptables -t nat -A PREROUTING \
    -p tcp -d 192.168.1.50 --dport 80 \
    -j DNAT --to-destination 192.168.122.100:80

sudo iptables -A FORWARD \
    -p tcp -d 192.168.122.100 --dport 80 \
    -m state --state NEW,ESTABLISHED,RELATED -j ACCEPT
```

Desde la LAN: `curl http://192.168.1.50:80` → llega al nginx de la VM.

**2. PostgreSQL (solo desde host):**

No necesita port forwarding. Desde el host:
```bash
psql -h 192.168.122.101 -U postgres
```
El host tiene interfaz virbr0 en la red 192.168.122.0/24, así que alcanza la VM directamente. No exponerlo a la LAN por seguridad — bases de datos no deben ser accesibles más allá de lo necesario.

**3. Gitea (LAN → host:3000 → VM:3000):**

```bash
sudo iptables -t nat -A PREROUTING \
    -p tcp -d 192.168.1.50 --dport 3000 \
    -j DNAT --to-destination 192.168.122.102:3000

sudo iptables -A FORWARD \
    -p tcp -d 192.168.122.102 --dport 3000 \
    -m state --state NEW,ESTABLISHED,RELATED -j ACCEPT
```

**Importante:** estas reglas iptables se pierden al reiniciar. Para persistirlas, usar `iptables-save`/`iptables-restore` o configurar hooks de libvirt. También conviene hacer reservas DHCP (`virsh net-update`) para que las VMs siempre tengan las mismas IPs.

**¿Cuándo pasar a bridge?** Si necesitas exponer muchos puertos o muchas VMs a la LAN, la gestión de reglas DNAT se vuelve compleja. Ese es el punto donde bridge simplifica: cada VM obtiene IP directa en la LAN, sin port forwarding.

</details>
