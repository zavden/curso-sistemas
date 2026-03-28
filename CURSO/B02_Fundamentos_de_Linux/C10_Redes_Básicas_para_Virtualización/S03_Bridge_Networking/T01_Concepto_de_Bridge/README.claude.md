# T01 — Concepto de Bridge (Puente de Red)

## Erratas detectadas en los archivos fuente

| Archivo | Linea | Error | Correccion |
|---------|-------|-------|------------|
| `README.max.md` | 105 | `Cuando la VM debe提供服务 (web server, SSH server)` — contiene caracteres chinos "提供服务" (proporcionar servicios) en medio del texto en espanol. | `Cuando la VM debe ofrecer servicios (web server, SSH server)` |

---

## 1. El problema: las VMs detras de NAT son invisibles

Con la red NAT de libvirt (192.168.122.0/24), las VMs pueden acceder a internet, pero nadie puede acceder a ellas desde fuera del host:

```
Laptop de un companero (192.168.1.200)
    |
    |  quiere acceder a nginx en la VM
    |  "curl http://192.168.122.100"  <- FALLA
    |  (192.168.122.100 no existe en la LAN)
    |
    v
Router (192.168.1.1) -> no sabe que es 192.168.122.x -> descarta
```

La VM tiene una IP en una red privada (192.168.122.0/24) que solo existe dentro del host. Para que la VM sea visible en la LAN del host, necesitamos conectarla directamente a esa LAN. Eso es lo que hace un bridge.

```
Con bridge:
Laptop del companero (192.168.1.200)
    |
    |  "curl http://192.168.1.150"  <- FUNCIONA
    |  (la VM tiene una IP en la LAN real)
    |
    v
Router (192.168.1.1) -> DHCP asigna 192.168.1.150 a la VM -> OK
```

---

## 2. Que es un bridge

Un bridge (puente) es un dispositivo que **conecta dos o mas segmentos de red y los hace funcionar como una sola red** a nivel de capa 2 (Ethernet). En Linux, un bridge es una implementacion por software que convierte al host en un switch virtual.

### Analogia

Imagina dos habitaciones con personas que quieren comunicarse:

- **Sin bridge**: cada habitacion es una red aislada. Las personas de una habitacion no pueden hablar con las de la otra.
- **Con bridge**: abrimos una puerta entre las dos habitaciones. Ahora todas las personas pueden comunicarse como si estuvieran en la misma habitacion.

En virtualizacion:
- **Habitacion 1**: la red fisica del host (tu LAN, con router, laptops, etc.).
- **Habitacion 2**: las VMs.
- **Bridge**: la "puerta" que conecta ambas. Las VMs aparecen en la LAN como si fueran maquinas fisicas.

### Definicion tecnica

Un bridge examina cada **trama Ethernet** (capa 2) que recibe, lee la **direccion MAC destino**, y decide si debe reenviarla por otro puerto o descartarla. Aprende que MACs estan en que puerto observando el trafico, construyendo una **tabla de MACs** (forwarding database / FDB).

---

## 3. Capa 2: Ethernet, MACs y tramas

Para entender el bridge, necesitas entender en que capa opera.

### El modelo OSI relevante

```
Capa 3 -- Red (IP)         Router decide por IP
                           -------------------
Capa 2 -- Enlace (Ethernet) Bridge/Switch decide por MAC  <- AQUI
                           -------------------
Capa 1 -- Fisica            Cable, senales electricas
```

Un bridge **no entiende de IPs**. Solo ve MACs. No sabe que 192.168.1.100 existe — solo sabe que la MAC `08:00:27:ab:cd:ef` esta en el puerto 1.

### Trama Ethernet

Cada paquete que viaja por una red Ethernet va envuelto en una **trama**:

```
+------------------------------------------------------------+
| MAC destino | MAC origen | Tipo | Datos (payload) | FCS    |
| (6 bytes)   | (6 bytes)  |(2 B) | (46-1500 bytes) | (4 B)  |
+------------------------------------------------------------+
  --------------------------
  Esto es lo que el bridge lee
  para tomar decisiones
```

| Campo | Significado |
|-------|------------|
| MAC destino | A quien va dirigida la trama? |
| MAC origen | Quien la envio? (el bridge aprende de esto) |
| Tipo | Que protocolo va dentro (0x0800 = IPv4, 0x0806 = ARP) |
| Datos | El paquete IP completo (con IPs, puertos, etc.) |
| FCS | Frame Check Sequence (verificacion de integridad) |

### Direcciones MAC

Las direcciones MAC (Media Access Control) son identificadores de 48 bits asignados a cada interfaz de red:

```
08:00:27:ab:cd:ef
-------- --------
OUI      NIC-specific
(fabricante) (dispositivo)
```

| OUI (primeros 3 bytes) | Fabricante |
|------------------------|-----------|
| 08:00:27 | VirtualBox |
| 52:54:00 | QEMU/KVM |
| 00:0c:29 | VMware |
| 02:42:xx | Docker |

Cuando ves una MAC que empieza con `52:54:00`, sabes que es una interfaz de QEMU/KVM.

---

## 4. Como funciona un bridge: la tabla MAC

El bridge toma decisiones de reenvio basandose en una tabla que construye observando el trafico. El proceso se llama **MAC learning**.

### Paso 1: estado inicial — tabla vacia

```
Bridge br0 (tabla vacia)
  Puerto 1: eth0 (interfaz fisica del host)
  Puerto 2: vnet0 (VM1)
  Puerto 3: vnet1 (VM2)

Tabla MAC:
  (vacia -- aun no ha visto trafico)
```

### Paso 2: VM1 envia una trama

```
VM1 (MAC: 52:54:00:aa:11:11) envia una trama a cualquier destino

Bridge ve:
  "La trama llego por puerto 2, origen MAC 52:54:00:aa:11:11"
  -> Aprender: 52:54:00:aa:11:11 esta en puerto 2

Tabla MAC:
  52:54:00:aa:11:11 -> Puerto 2

Como no conoce el destino, FLOOD:
  -> Reenvia la trama por TODOS los puertos (excepto el de origen)
  -> Puerto 1 (eth0) y Puerto 3 (vnet1) reciben la trama
```

### Paso 3: el destino responde

```
La maquina destino (MAC: 08:00:27:bb:22:22) responde por puerto 1 (eth0)

Bridge ve:
  "Trama desde puerto 1, origen 08:00:27:bb:22:22, destino 52:54:00:aa:11:11"
  -> Aprender: 08:00:27:bb:22:22 esta en puerto 1
  -> Destino 52:54:00:aa:11:11 esta en puerto 2 (ya lo sabe)
  -> Reenviar SOLO por puerto 2 (no hace flood)

Tabla MAC:
  52:54:00:aa:11:11  -> Puerto 2
  08:00:27:bb:22:22  -> Puerto 1
```

### Paso 4: estado estable

Despues de un poco de trafico, el bridge conoce todas las MACs:

```
Tabla MAC:
  52:54:00:aa:11:11  -> Puerto 2 (VM1)
  52:54:00:aa:22:22  -> Puerto 3 (VM2)
  08:00:27:bb:22:22  -> Puerto 1 (laptop en la LAN)
  d4:3b:04:cc:33:33  -> Puerto 1 (telefono en la LAN)

Ahora el bridge reenvia tramas de forma selectiva:
  VM1 -> VM2: solo por puerto 3 (no sale por eth0)
  VM1 -> laptop: solo por puerto 1 (no va a VM2)
  laptop -> VM2: solo por puerto 3
```

### Aging: las entradas expiran

Las entradas de la tabla MAC tienen un **aging timer** (tipicamente 300 segundos). Si una MAC no genera trafico en ese tiempo, se elimina de la tabla. Si vuelve a aparecer, se reaprende.

```bash
# Ver el aging time del bridge
cat /sys/class/net/br0/bridge/ageing_time
# 30000  (en centesimas de segundo = 300 segundos)
```

---

## 5. Bridge vs hub vs switch vs router

Estos cuatro dispositivos se confunden frecuentemente. La diferencia esta en que capa operan y que informacion usan para tomar decisiones:

### Comparacion

| Dispositivo | Capa OSI | Unidad de datos | Que mira | Decision |
|------------|---------|----------------|----------|----------|
| **Hub** | 1 (Fisica) | Bits | Nada | Reenvia TODO a TODOS los puertos |
| **Bridge** | 2 (Enlace) | Tramas | MAC destino | Reenvia selectivamente por MAC |
| **Switch** | 2 (Enlace) | Tramas | MAC destino | = Bridge con muchos puertos |
| **Router** | 3 (Red) | Paquetes | IP destino | Conecta redes DIFERENTES |

### Bridge = Switch

En redes modernas, bridge y switch son practicamente lo mismo. La diferencia es historica:

- **Bridge** (1980s): conecta 2 segmentos de red. Software. Pocos puertos.
- **Switch** (1990s+): bridge con muchos puertos (8, 24, 48). Hardware dedicado (ASICs).

En Linux, el componente del kernel que implementa un bridge funciona exactamente como un switch: multiples puertos, tabla MAC, forwarding selectivo. El nombre `bridge` persiste por convencion.

### Por que un bridge y no un router

Un router conecta redes **diferentes** (192.168.1.0/24 <-> 10.0.0.0/24). Un bridge conecta segmentos de la **misma red**.

En virtualizacion:
- **NAT**: el host actua como router entre la red de VMs (192.168.122.0/24) y la LAN (192.168.1.0/24). Son redes diferentes -> routing + NAT.
- **Bridge**: las VMs se conectan directamente a la LAN (192.168.1.0/24). Es la misma red -> bridge, sin NAT.

```
NAT (dos redes, router entre ellas):
  VM: 192.168.122.100 --[NAT/router]-- LAN: 192.168.1.0/24

Bridge (una sola red):
  VM: 192.168.1.150 --[bridge]-- LAN: 192.168.1.0/24
  (la VM tiene IP en la misma red que el host)
```

---

## 6. Bridge en virtualizacion: el switch virtual

En QEMU/KVM, el bridge conecta las interfaces virtuales de las VMs (TAP devices) con la interfaz fisica del host. El resultado es que las VMs aparecen como maquinas mas en la red fisica.

### Topologia con bridge

```
+--------------------------- HOST ----------------------------+
|                                                              |
|  +-- VM1 ---------+   +-- VM2 ---------+                    |
|  | eth0            |   | eth0            |                    |
|  | (52:54:00:..)   |   | (52:54:00:..)   |                    |
|  +------+----------+   +------+----------+                    |
|         | vnet0                | vnet1                        |
|         |                     |                               |
|  +------+---------------------+----------+                    |
|  |          Bridge: br0                   |                    |
|  |     (switch virtual de Linux)          |                    |
|  |                                        |                    |
|  |  Tabla MAC:                            |                    |
|  |  52:54:00:aa:11:11 -> vnet0            |                    |
|  |  52:54:00:aa:22:22 -> vnet1            |                    |
|  |  d4:3b:04:cc:33:33 -> enp0s3          |                    |
|  +-----------------+---------------------+                    |
|                    | enp0s3 (interfaz fisica)                 |
|                    | (ya NO tiene IP propia -- la IP          |
|                    |  esta en br0)                             |
+--------------------+------------------------------------------+
                     |
                     | cable Ethernet
                     |
            +--------+--------+
            |   Switch/Router  |
            |   192.168.1.1    |
            |                  |
            |  DHCP asigna:    |
            |  Host: .50       |  (IP de br0)
            |  VM1:  .150      |
            |  VM2:  .151      |
            +------------------+
```

### Que pasa con la IP del host

Cuando creas un bridge, la interfaz fisica (`enp0s3`) se convierte en un **puerto del bridge** y pierde su IP. La IP del host se mueve al bridge (`br0`):

```
ANTES del bridge:
  enp0s3: 192.168.1.50/24  (la interfaz tiene la IP)

DESPUES de crear el bridge:
  enp0s3: sin IP              (es un puerto del bridge, no un endpoint)
  br0:    192.168.1.50/24    (el bridge tiene la IP del host)
```

Esto es analogo a como funciona un switch fisico: el switch no tiene IP en sus puertos — tiene una IP de gestion asignada a una interfaz virtual.

### TAP devices: la conexion VM <-> bridge

Cada VM conectada al bridge tiene un TAP device (`vnetN`) que es su "cable virtual":

```
VM  <-->  vnet0  <-->  br0  <-->  enp0s3  <-->  red fisica
          ------        ---        ------
          "cable"       "switch"   "uplink"
          virtual       virtual    (cable real)
```

El TAP device se crea automaticamente cuando la VM arranca y se destruye cuando la VM se apaga:

```bash
# Con una VM corriendo:
ip link show type bridge
# br0: <BROADCAST,MULTICAST,UP,LOWER_UP>

bridge link show
# vnet0: <BROADCAST,MULTICAST,UP,LOWER_UP> master br0
# enp0s3: <BROADCAST,MULTICAST,UP,LOWER_UP> master br0
```

---

## 7. Bridge vs NAT: comparacion detallada

### Flujo de un paquete

**Con NAT** (la VM envia un paquete a internet):

```
VM (192.168.122.100) -> virbr0 -> iptables MASQUERADE -> enp0s3 -> router
                                 ------------------
                                 traduce IP privada a la IP del host
```

El paquete pasa por capa 3 (routing + NAT). La IP de la VM se reemplaza.

**Con bridge** (la VM envia un paquete a internet):

```
VM (192.168.1.150) -> vnet0 -> br0 -> enp0s3 -> router
                               ---
                               simple forwarding de capa 2
```

El paquete pasa solo por capa 2 (switching). La IP de la VM no se modifica.

### Tabla comparativa

| Aspecto | NAT | Bridge |
|---------|-----|--------|
| IP de la VM | Red privada (192.168.122.x) | Red del host (192.168.1.x) |
| Visible desde la LAN | No (necesita port forwarding) | Si (IP en la misma red) |
| Visible desde internet | No (doble port forwarding) | Depende del router |
| Configuracion en el host | Ninguna (libvirt default) | Crear bridge, mover IP |
| DHCP de la VM | dnsmasq de libvirt | DHCP del router real |
| DNS de la VM | dnsmasq de libvirt | DNS del router real |
| Rendimiento | Ligera penalizacion (NAT + conntrack) | Directo (sin traduccion) |
| Aislamiento | VM aislada de la LAN | VM expuesta a la LAN |
| IPs disponibles | 254 (pool privado) | Depende del DHCP del router |
| Riesgo de conflictos | Bajo (red privada separada) | Posible (compite con otros hosts) |
| Funciona con WiFi | Si | Complicado (ver nota) |

### Por que bridge es complicado con WiFi

El bridging de capa 2 requiere que la interfaz permita tramas con MACs de origen diferentes a la suya. Las tarjetas WiFi en modo cliente **no permiten esto** — el access point rechaza tramas con MAC desconocida.

Soluciones:
- Usar interfaz cableada (Ethernet) para el bridge.
- Usar NAT para VMs cuando el host solo tiene WiFi.
- Algunos drivers WiFi soportan modo 4-address (WDS) que permite bridge, pero es raro.

---

## 8. Spanning Tree Protocol (STP)

Si un bridge tiene una topologia con **loops** (caminos redundantes), las tramas circulan indefinidamente, saturando la red (broadcast storm). STP previene esto.

### El problema de los loops

```
Bridge A ---- Bridge B
    |              |
    +---- Bridge C -+
           |
         Loop: una trama broadcast puede circular A->B->C->A->B->C...
         indefinidamente, consumiendo todo el ancho de banda
```

### Como STP lo resuelve

STP desactiva puertos redundantes para eliminar loops, manteniendo un solo camino activo:

```
Bridge A ---- Bridge B
    |              |
    +---- Bridge C -+
                    X <- puerto bloqueado por STP
```

Si el camino activo falla, STP reactiva el puerto bloqueado como respaldo.

### STP en libvirt

Los bridges de libvirt tienen STP habilitado por defecto:

```xml
<bridge name='virbr0' stp='on' delay='0'/>
```

El `delay` controla cuanto espera STP antes de empezar a reenviar tramas (mientras aprende la topologia). Para labs simples, `delay='0'` es correcto.

### Importa STP para labs de VMs?

En un lab simple con un solo bridge y unas pocas VMs, los loops son imposibles y STP no hace nada util. Pero si creas topologias complejas con multiples bridges conectados entre si (ej. simular una red empresarial), STP evita que un error de configuracion tumbe toda la red virtual.

---

## 9. Tipos de bridge en Linux

### Linux bridge (kernel)

El bridge nativo del kernel de Linux. Es el mas usado en virtualizacion con libvirt:

```bash
# Crear un bridge
sudo ip link add br0 type bridge

# Anadir una interfaz al bridge
sudo ip link set enp0s3 master br0

# Activar el bridge
sudo ip link set br0 up
```

Ventajas: integrado en el kernel, estable, compatible con todo.

### Open vSwitch (OVS)

Un switch virtual mas avanzado, usado en entornos de cloud (OpenStack, oVirt):

```bash
# Crear un bridge OVS
sudo ovs-vsctl add-br ovs-br0

# Anadir una interfaz
sudo ovs-vsctl add-port ovs-br0 enp0s3
```

Ventajas: VLANs, QoS, SDN (Software-Defined Networking), OpenFlow, tuneles VXLAN/GRE.
Desventajas: mas complejo, requiere paquete adicional.

Para este curso usaremos Linux bridge — es mas que suficiente para labs de virtualizacion.

### macvtap

Una alternativa al bridge que conecta VMs directamente a la interfaz fisica sin crear un bridge explicito:

```xml
<!-- En libvirt, usar macvtap en lugar de bridge -->
<interface type='direct' trustGuestRxFilters='yes'>
  <source dev='enp0s3' mode='bridge'/>
  <model type='virtio'/>
</interface>
```

Ventajas: mas simple (no necesitas crear un bridge).
Desventajas: la VM no puede comunicarse con el host por esta interfaz (limitacion de macvtap).

---

## 10. Escenarios: cuando usar bridge y cuando no

### Usar bridge cuando

| Escenario | Por que bridge |
|-----------|---------------|
| VM que ofrece un servicio web accesible desde la LAN | Otros dispositivos necesitan alcanzar la VM |
| Simular una red "real" con VMs como si fueran PCs | Las VMs necesitan IPs en la misma red que el host |
| Lab de redes con routing entre subredes | Las VMs necesitan ser endpoints reales |
| VM con IP publica (hosting, servidor expuesto) | La VM debe ser directamente accesible |
| Cluster de VMs que necesitan multicast | NAT no reenvia multicast bien |

### Usar NAT cuando

| Escenario | Por que NAT |
|-----------|------------|
| Labs de desarrollo general | Solo necesitas que la VM tenga internet |
| VMs temporales (testing, build) | No importa la IP ni la visibilidad |
| Host con WiFi (sin Ethernet) | Bridge con WiFi es problematico |
| Aislamiento deseado | No quieres que la VM sea visible en la LAN |
| Setup rapido sin configuracion | NAT funciona out-of-the-box con libvirt |

### Usar red aislada cuando

| Escenario | Por que aislada |
|-----------|----------------|
| Labs de seguridad/firewall | Las VMs no deben tocar la red real |
| Labs de almacenamiento (RAID, LVM) | La red no es relevante |
| Simulacion de red interna sin internet | Por diseno |
| Entorno de malware analysis | Aislamiento total requerido |

---

## 11. El bridge en el contexto del curso

En este curso trabajaras con los tres tipos de red:

```
B02 C10 (este capitulo):
  -> Entender los conceptos (teoria)
  -> Crear un bridge manualmente (siguiente topico)

B08 QEMU/KVM:
  -> Usar la red NAT default para la mayoria de labs
  -> Crear un bridge para labs donde las VMs necesitan ser accesibles

B09/B10:
  -> Topologias multi-VM con redes aisladas y bridges
  -> Router VM conectando subredes

B11 Docker:
  -> Docker usa su propio bridge (docker0) -- mismo concepto
```

El bridge que creas manualmente en el siguiente topico (T02) es el mismo tipo de bridge que libvirt crea automaticamente (virbr0), pero conectado a la red fisica en lugar de aislado con NAT.

---

## 12. Errores comunes

### Error 1: Confundir bridge con NAT

```
"Mi VM tiene bridge, entonces tiene NAT"
-> FALSO. Bridge y NAT son mutuamente excluyentes.

Bridge: la VM esta EN la red fisica (capa 2)
NAT:    la VM esta DETRAS del host (capa 3 con traduccion)
```

virbr0 de libvirt es un bridge, si, pero con NAT encima. Un bridge "puro" (br0 conectado a enp0s3) no tiene NAT — las VMs tienen IPs directamente en la LAN.

### Error 2: Crear un bridge y perder la conexion SSH al host

```bash
# Estas conectado por SSH al host (192.168.1.50 en enp0s3)
# Creas un bridge y anades enp0s3:
sudo ip link add br0 type bridge
sudo ip link set enp0s3 master br0
# -> enp0s3 pierde su IP
# -> tu sesion SSH SE CUELGA

# Solucion: hacer TODOS los pasos en un solo comando o script:
sudo ip link add br0 type bridge && \
sudo ip link set enp0s3 master br0 && \
sudo ip addr del 192.168.1.50/24 dev enp0s3 && \
sudo ip addr add 192.168.1.50/24 dev br0 && \
sudo ip link set br0 up && \
sudo ip route add default via 192.168.1.1 dev br0
```

O mejor: configurar el bridge con NetworkManager o netplan, que maneja la transicion automaticamente.

### Error 3: Intentar bridge con WiFi

```bash
sudo ip link set wlp2s0 master br0
# RTNETLINK answers: Operation not supported
# WiFi en modo cliente NO soporta bridge
```

WiFi no permite tramas con MAC de origen diferente a la de la interfaz. Las VMs tienen MACs propias (52:54:00:xx) que el access point rechaza.

### Error 4: No mover la IP al bridge

```bash
# Creas el bridge y anades enp0s3, pero no mueves la IP:
sudo ip link add br0 type bridge
sudo ip link set enp0s3 master br0
sudo ip link set br0 up

# enp0s3 ya no tiene IP (la perdio al ser puerto del bridge)
# br0 tampoco tiene IP (no se la asignaste)
# -> el host pierde toda conectividad de red
```

Cuando enp0s3 se convierte en puerto del bridge, su IP debe moverse a br0.

### Error 5: Pensar que un bridge proporciona aislamiento

```
"Mis VMs estan en un bridge, asi que estan aisladas"
-> FALSO. Un bridge CONECTA -- no aisla.

Las VMs en un bridge son visibles para TODOS los dispositivos de la red.
Si quieres aislamiento, usa una red aislada de libvirt (sin bridge a la interfaz fisica).
```

---

## 13. Cheatsheet

```text
+======================================================================+
|                   CONCEPTO DE BRIDGE CHEATSHEET                       |
+======================================================================+
|                                                                      |
|  QUE ES                                                              |
|  Bridge = switch virtual en software (capa 2, Ethernet)              |
|  Conecta segmentos de red -> misma red a nivel L2                    |
|  Decide por MAC (no por IP) -> examina tramas, no paquetes           |
|  Aprende MACs observando el trafico (MAC learning + aging)           |
|                                                                      |
+----------------------------------------------------------------------+
|                                                                      |
|  BRIDGE vs OTROS                                                     |
|  Hub:     capa 1, reenvia TODO a todos (obsoleto)                    |
|  Bridge:  capa 2, reenvia selectivamente por MAC                     |
|  Switch:  = bridge moderno con muchos puertos                        |
|  Router:  capa 3, conecta redes DIFERENTES por IP                    |
|                                                                      |
+----------------------------------------------------------------------+
|                                                                      |
|  EN VIRTUALIZACION                                                   |
|  Bridge puro:  VM tiene IP en la LAN real del host                   |
|    VM (192.168.1.150) -> vnet0 -> br0 -> enp0s3 -> router            |
|    VM visible desde toda la LAN                                      |
|                                                                      |
|  NAT (virbr0): VM tiene IP en red privada, NAT al salir              |
|    VM (192.168.122.100) -> vnet0 -> virbr0 -> MASQUERADE -> enp0s3   |
|    VM invisible desde la LAN                                         |
|                                                                      |
+----------------------------------------------------------------------+
|                                                                      |
|  COMPONENTES                                                         |
|  br0        Bridge (switch virtual)                                  |
|  enp0s3     Interfaz fisica (uplink del bridge)                      |
|  vnetN      TAP device (cable virtual de la VM)                      |
|  Tabla MAC  Mapeo MAC -> puerto (aprendida automaticamente)          |
|  STP        Previene loops en topologias redundantes                 |
|                                                                      |
+----------------------------------------------------------------------+
|                                                                      |
|  IP DEL HOST CON BRIDGE                                              |
|  ANTES:  enp0s3 tiene la IP                                         |
|  DESPUES: enp0s3 sin IP (es puerto), br0 tiene la IP                |
|  Si no mueves la IP, el host pierde conectividad!                    |
|                                                                      |
+----------------------------------------------------------------------+
|                                                                      |
|  CUANDO USAR                                                         |
|  Bridge:  VM debe ser visible en la LAN -> servicios, labs reales    |
|  NAT:     VM solo necesita internet -> desarrollo, testing           |
|  Aislada: VM no debe tocar la red real -> seguridad, RAID labs       |
|                                                                      |
+----------------------------------------------------------------------+
|                                                                      |
|  LIMITACIONES                                                        |
|  WiFi NO soporta bridge (MAC filtering del AP)                       |
|  Crear bridge por SSH -> riesgo de perder conexion                   |
|  Bridge no aisla -- CONECTA (si quieres aislar, usa red aislada)     |
|                                                                      |
+======================================================================+
```

---

## 14. Ejercicios

### Ejercicio 1: Capa de operacion

Un bridge recibe una trama Ethernet con MAC destino `52:54:00:aa:11:11` y MAC origen `d4:3b:04:cc:33:33`. La trama contiene un paquete IP con destino 192.168.1.150.

**Pregunta:** Que informacion usa el bridge para decidir a donde reenviar la trama? Usa la IP destino, la MAC destino, o ambas?

<details><summary>Prediccion</summary>

El bridge usa **solo la MAC destino** (`52:54:00:aa:11:11`). Un bridge opera en **capa 2** (Enlace/Ethernet) y no entiende de IPs. No examina el paquete IP que va dentro de la trama — solo lee la cabecera Ethernet.

El bridge consulta su tabla MAC:
- Si `52:54:00:aa:11:11` esta en la tabla, reenvia la trama **solo** por el puerto correspondiente.
- Si no esta en la tabla, hace **flood**: reenvia por todos los puertos excepto el de origen.

La MAC origen (`d4:3b:04:cc:33:33`) no se usa para reenviar, pero si para **aprender**: el bridge registra que esa MAC esta en el puerto por donde llego la trama.

La IP 192.168.1.150 es completamente irrelevante para el bridge.

</details>

---

### Ejercicio 2: MAC learning paso a paso

Un bridge br0 tiene 3 puertos: vnet0 (VM1), vnet1 (VM2), enp0s3 (red fisica). La tabla MAC esta vacia. VM1 (MAC: AA) envia una trama a la laptop de la LAN (MAC: CC).

**Pregunta:** Describe que hace el bridge al recibir la trama de VM1. La laptop responde — describe que hace el bridge con la respuesta. Despues de ambos intercambios, cual es el estado de la tabla MAC?

<details><summary>Prediccion</summary>

**Trama de VM1 (MAC AA) hacia laptop (MAC CC):**

1. La trama llega por **vnet0** (puerto de VM1).
2. El bridge **aprende**: MAC AA esta en vnet0. La anade a la tabla.
3. El bridge busca MAC CC (destino) en la tabla -> **no la encuentra**.
4. Como no conoce el destino, hace **flood**: reenvia la trama por **vnet1** y **enp0s3** (todos los puertos excepto vnet0).
5. VM2 recibe la trama (la descarta porque no es para ella). La laptop la recibe y la procesa.

**Respuesta de la laptop (MAC CC) hacia VM1 (MAC AA):**

1. La trama llega por **enp0s3**.
2. El bridge **aprende**: MAC CC esta en enp0s3. La anade a la tabla.
3. El bridge busca MAC AA (destino) en la tabla -> **la encuentra** en vnet0.
4. Reenvia la trama **solo por vnet0**. No hace flood — ya sabe donde esta AA.

**Tabla MAC despues de ambos intercambios:**

| MAC | Puerto |
|-----|--------|
| AA  | vnet0  |
| CC  | enp0s3 |

VM2 (MAC BB) aun no ha generado trafico, asi que no aparece en la tabla.

</details>

---

### Ejercicio 3: Bridge vs NAT — flujo de paquete

Una VM ejecuta `curl http://example.com`. Traza el flujo del paquete en dos escenarios: (a) VM en red NAT con virbr0, (b) VM en bridge puro con br0 conectado a enp0s3.

**Pregunta:** En cada caso, la IP de origen del paquete cuando llega al router es la IP de la VM o la IP del host? Por que?

<details><summary>Prediccion</summary>

**(a) NAT con virbr0:**

```
VM (192.168.122.100) -> vnet0 -> virbr0 -> iptables MASQUERADE -> enp0s3 -> router
```

La IP de origen cuando llega al router es **la IP del host** (ej. 192.168.1.50). La regla MASQUERADE de iptables reemplaza 192.168.122.100 (IP privada de la VM) con la IP del host. El router nunca ve la IP 192.168.122.x.

**(b) Bridge puro con br0:**

```
VM (192.168.1.150) -> vnet0 -> br0 -> enp0s3 -> router
```

La IP de origen cuando llega al router es **la IP de la VM** (192.168.1.150). No hay traduccion. El bridge opera en capa 2 — solo reenvia tramas Ethernet sin modificar nada. El router ve la VM como otra maquina mas en la LAN.

**Diferencia clave**: NAT modifica el paquete (capa 3). Bridge solo reenvia la trama (capa 2). Por eso con bridge la VM necesita una IP en la misma red que el host — el router la trata como un dispositivo mas.

</details>

---

### Ejercicio 4: Que pasa con la IP del host

Tienes un host con `enp0s3: 192.168.1.50/24` y creas un bridge br0 anadiendo enp0s3 como puerto.

**Pregunta:** Despues de ejecutar `sudo ip link set enp0s3 master br0`, enp0s3 pierde su IP. Por que? Donde deberia estar la IP ahora? Que pasa si no la mueves?

<details><summary>Prediccion</summary>

**Por que pierde la IP**: cuando enp0s3 se convierte en puerto del bridge, deja de ser un "endpoint" de red y pasa a ser un "cable interno" del switch virtual. Un puerto de switch no tiene IP — solo reenvia tramas. Es analogo a un switch fisico: los puertos del switch no tienen IPs, solo el switch tiene una IP de gestion.

**Donde deberia estar**: la IP debe moverse a **br0**, la interfaz del bridge:
```bash
sudo ip addr add 192.168.1.50/24 dev br0
sudo ip route add default via 192.168.1.1 dev br0
```

**Si no la mueves**: el host pierde **toda conectividad IP**. enp0s3 no tiene IP (es puerto del bridge) y br0 tampoco tiene IP (no se la asignaste). No podras hacer SSH, ping, ni navegar desde el host. Las VMs conectadas al bridge tampoco funcionaran porque el bridge no tiene configuracion de capa 3.

Por eso es critico hacer la migracion de IP en un solo comando encadenado (con `&&`) o usar NetworkManager/netplan que lo gestionan automaticamente.

</details>

---

### Ejercicio 5: Por que bridge no funciona con WiFi

Tu host solo tiene WiFi (wlp2s0). Intentas crear un bridge y anadir la interfaz WiFi:

```bash
sudo ip link set wlp2s0 master br0
# RTNETLINK answers: Operation not supported
```

**Pregunta:** Por que falla? Que tiene de diferente WiFi respecto a Ethernet para bridging? Que alternativas tienes?

<details><summary>Prediccion</summary>

**Por que falla**: el bridging requiere que la interfaz pueda enviar tramas con **MACs de origen diferentes a la suya**. En Ethernet cableado esto no es problema — el cable no filtra por MAC. Pero en WiFi, el access point (AP) solo acepta tramas de MACs que ha autenticado (asociacion 802.11). Si una VM con MAC `52:54:00:xx:xx:xx` intenta enviar una trama a traves de wlp2s0 (que tiene otra MAC), el AP la rechaza.

Ademas, el protocolo 802.11 (WiFi) tiene un formato de trama diferente a Ethernet (802.3). Las tramas WiFi tienen 3 o 4 campos de direccion, no 2. El bridge de Linux espera tramas Ethernet estandar.

**Alternativas**:
1. **NAT** (lo mas practico): usar la red NAT de libvirt. Las VMs salen a internet via MASQUERADE con la MAC e IP del host.
2. **Modo 4-address (WDS)**: algunos drivers WiFi soportan este modo que permite tramas con multiples MACs. Es raro y no universal.
3. **Usar interfaz cableada**: conectar un cable Ethernet para el bridge y dejar WiFi para el trafico normal del host.

</details>

---

### Ejercicio 6: STP y loops

Creas tres bridges (br0, br1, br2) conectados en triangulo para simular una red empresarial:

```
br0 ---- br1
 |        |
 +-- br2 -+
```

**Pregunta:** Sin STP, que pasaria si una VM en br0 envia un broadcast? Con STP habilitado, como se resuelve?

<details><summary>Prediccion</summary>

**Sin STP**: el broadcast crea una **broadcast storm**. La trama broadcast llega a br1 y br2. br1 la reenvia a br2 (por el enlace br1-br2). br2 la reenvia a br0 (por el enlace br2-br0). br0 la reenvia a br1 de nuevo (por el enlace br0-br1). El ciclo se repite **indefinidamente**. Cada vez que la trama da la vuelta, se multiplica (porque cada bridge la reenvia por todos los puertos excepto el de origen, pero hay multiples caminos). En segundos, la red se satura completamente y queda inutilizable.

**Con STP**: STP (Spanning Tree Protocol) detecta el loop y **bloquea uno de los puertos redundantes**. Por ejemplo, bloquea el enlace br1-br2:

```
br0 ---- br1
 |        |
 +-- br2 -+
              X <- bloqueado
```

Ahora solo hay un camino entre cualquier par de bridges (arbol sin loops). El broadcast de la VM llega a br1 y br2, pero no puede circular. Si el enlace br0-br1 falla, STP detecta el cambio y reactiva el enlace br1-br2 como respaldo.

</details>

---

### Ejercicio 7: Tipos de bridge — Linux bridge vs OVS vs macvtap

Para cada escenario, que tipo de bridge usarias?

1. Lab de virtualizacion basico con 3 VMs en libvirt.
2. Despliegue de OpenStack con 200 VMs, VLANs y tuneles VXLAN.
3. Una sola VM que necesita acceso a la LAN sin crear bridge.

<details><summary>Prediccion</summary>

1. **Linux bridge (kernel)**: es el mas simple, integrado en el kernel, sin dependencias adicionales. Para 3 VMs en un lab es mas que suficiente. Se crea con `ip link add br0 type bridge` o automaticamente via libvirt.

2. **Open vSwitch (OVS)**: para 200 VMs con VLANs y VXLAN necesitas las funcionalidades avanzadas de OVS: soporte nativo de VLANs, tuneles VXLAN/GRE para conectar hosts, QoS por VM, SDN con OpenFlow, y mejor escalabilidad. El Linux bridge no soporta VXLAN nativamente ni OpenFlow.

3. **macvtap**: conecta la VM directamente a `enp0s3` sin crear un bridge explicito. Es la opcion mas rapida para una sola VM. Se configura en libvirt con `<interface type='direct'>`. La limitacion es que la VM no puede comunicarse con el host por esa interfaz — si necesitas SSH del host a la VM, macvtap no sirve.

</details>

---

### Ejercicio 8: Bridge en virbr0 vs bridge puro

libvirt crea virbr0 automaticamente. Tu puedes crear br0 manualmente.

**Pregunta:** Ambos son bridges Linux. Cual es la diferencia fundamental entre virbr0 y un br0 conectado a enp0s3? Lista 3 diferencias.

<details><summary>Prediccion</summary>

| Aspecto | virbr0 (libvirt NAT) | br0 (bridge puro) |
|---------|---------------------|-------------------|
| **Interfaz fisica** | No conectado a ninguna interfaz fisica. Es un bridge aislado con NAT | Conectado a enp0s3 (interfaz fisica). Sin NAT |
| **Red de las VMs** | Red privada 192.168.122.0/24. Las VMs tienen IPs que solo existen dentro del host | Red de la LAN (ej. 192.168.1.0/24). Las VMs tienen IPs visibles para todos los dispositivos |
| **DHCP/DNS** | dnsmasq de libvirt asigna IPs y hace DNS forwarding | El DHCP del router real de la LAN asigna IPs. DNS del router real |

Diferencia fundamental: virbr0 **aisla** las VMs de la LAN (necesitan NAT para salir). br0 **integra** las VMs en la LAN (son ciudadanos de primera clase). virbr0 es un bridge + router + NAT + DHCP + DNS. br0 es solo un bridge — switching puro de capa 2.

</details>

---

### Ejercicio 9: Aging de la tabla MAC

El bridge br0 tiene esta tabla MAC con aging de 300 segundos:

| MAC | Puerto | Ultima actividad |
|-----|--------|-----------------|
| 52:54:00:aa:11:11 | vnet0 | hace 10s |
| 52:54:00:aa:22:22 | vnet1 | hace 290s |
| d4:3b:04:cc:33:33 | enp0s3 | hace 310s |

**Pregunta:** Que entrada ya expiro? Si VM2 (segunda entrada) no genera trafico en los proximos 15 segundos, que pasara? Cuando VM2 envie trafico de nuevo, que hace el bridge?

<details><summary>Prediccion</summary>

- **Entrada expirada**: `d4:3b:04:cc:33:33` (ultima actividad hace 310s > 300s de aging). Ya fue eliminada de la tabla.

- **VM2 en 15 segundos**: la entrada de `52:54:00:aa:22:22` tiene 290s de edad. En 15 segundos tendra 305s, superando los 300s de aging. Se **eliminara** de la tabla. Si alguien envia una trama a esa MAC, el bridge no sabra en que puerto esta y hara **flood** (reenvia por todos los puertos excepto el de origen).

- **Cuando VM2 vuelva a enviar trafico**: el bridge lee la MAC origen de la trama, ve que `52:54:00:aa:22:22` llego por vnet1, y la **reaprende** — la anade de nuevo a la tabla con el contador de aging reiniciado a 0.

El aging garantiza que la tabla no se llena de MACs de dispositivos que ya no estan conectados. Es un mecanismo de auto-limpieza.

</details>

---

### Ejercicio 10: Decisiones de diseno

Para cada escenario, indica que tipo de red usarias (NAT, bridge, o aislada) y justifica:

1. VM donde solo compilas codigo y necesitas `apt install`.
2. VM con nginx accesible desde laptops de companeros en tu LAN.
3. Tres VMs simulando un cluster interno que NO debe tocar internet.
4. Host con solo WiFi y necesitas una VM con internet.
5. VM actuando como router entre dos subredes virtuales.

<details><summary>Prediccion</summary>

1. **NAT**: solo necesitas internet saliente (`apt install`). No necesitas que nadie acceda a la VM. NAT funciona out-of-the-box con libvirt, sin configuracion.

2. **Bridge**: los companeros necesitan alcanzar la VM desde sus laptops (estan en la misma LAN). Con bridge, la VM obtiene una IP en la LAN real (ej. 192.168.1.150) y es directamente accesible. Con NAT necesitarias port forwarding en el host, que es mas fragil.

3. **Red aislada**: las VMs necesitan comunicarse entre si pero NO con internet. Una red aislada de libvirt (sin `<forward>` en el XML) proporciona exactamente esto: las VMs se ven entre si pero no tienen ruta al exterior.

4. **NAT**: WiFi no soporta bridge (el AP rechaza MACs desconocidas). NAT funciona sobre WiFi porque el host usa su propia MAC para todo el trafico saliente, aplicando MASQUERADE.

5. **Multiples interfaces**: la VM router necesita al menos **dos interfaces**, cada una en una red diferente. Podria tener una interfaz en bridge (para acceso a la LAN real) y otra en una red aislada o NAT. O dos redes aisladas diferentes. El VM router necesita `ip_forward=1` habilitado para reenviar paquetes entre sus interfaces.

</details>

---

> **Nota**: un bridge es conceptualmente simple — es un switch implementado en software. Pero su impacto en la arquitectura de red es significativo: transforma a las VMs de entidades ocultas detras de NAT a ciudadanos de primera clase en la red. El precio es la complejidad de configuracion (mover la IP del host al bridge, incompatibilidad con WiFi, riesgo de perder conectividad SSH). Para la mayoria de labs, NAT es suficiente. Usa bridge cuando necesites que las VMs sean accesibles desde fuera del host.
