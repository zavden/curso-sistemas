# T01 — Tipos de Redes en libvirt

## Erratas detectadas en los archivos fuente

| Archivo | Linea | Error | Correccion |
|---------|-------|-------|------------|
| `README.max.md` | 24 | `Host -> VM | ❌ (no visible)` en la tabla de NAT. El host **si** puede alcanzar VMs en la red NAT via la IP privada (192.168.122.x), ya que el host tiene virbr0 con IP 192.168.122.1 en esa misma red. | `Host -> VM | Si (via IP privada 192.168.122.x)`. El README.md lo documenta correctamente en linea 87. |

---

## 1. Redes virtuales: que problema resuelven

Cuando creas VMs con QEMU/KVM, cada una necesita conectividad de red: comunicarse con otras VMs, con el host, o con internet. Pero no todas las VMs necesitan la misma conectividad:

- Una VM de desarrollo necesita internet para instalar paquetes.
- Una VM que simula un servidor de base de datos solo necesita hablar con la VM de la aplicacion.
- Una VM donde analizas malware no debe tocar la red real bajo ningun concepto.

Libvirt resuelve esto con **redes virtuales**: redes definidas por XML que determinan exactamente como se conectan las VMs. Cada red virtual crea un bridge, opcionalmente incluye DHCP/DNS (via dnsmasq), y define si hay salida a internet y de que tipo.

---

## 2. Los modos de red en libvirt

El elemento `<forward mode='...'>` en el XML de la red define el comportamiento:

| Modo | Elemento XML | Resumen |
|------|-------------|---------|
| **NAT** | `<forward mode='nat'/>` | VMs salen a internet via NAT (MASQUERADE) |
| **Aislada** | Sin `<forward>` | VMs solo hablan entre si, sin salida |
| **Routed** | `<forward mode='route'/>` | VMs ruteadas sin NAT (requiere config en la LAN) |
| **Bridge** | `<forward mode='bridge'/>` | VMs conectadas a un bridge externo existente |
| **Open** | `<forward mode='open'/>` | Bridge sin reglas de firewall de libvirt |

Los dos primeros (NAT y aislada) son los mas usados en labs. Bridge se usa cuando las VMs deben ser accesibles desde la LAN. Routed y open son para escenarios mas especificos.

---

## 3. NAT (forward mode='nat')

El modo por defecto de libvirt. Las VMs obtienen IPs en una red privada y acceden a internet a traves de NAT (MASQUERADE) en el host.

### Topologia

```
+--------------------------- HOST ----------------------------+
|                                                              |
|  +-- VM1 ---------+   +-- VM2 ---------+                    |
|  | 192.168.122     |   | 192.168.122     |                    |
|  |      .100       |   |      .101       |                    |
|  +------+----------+   +------+----------+                    |
|         | vnet0                | vnet1                        |
|         +--------+-------------+                              |
|                  |                                             |
|  +---------------+----------------+                           |
|  |  virbr0 (192.168.122.1)        |                           |
|  |  bridge + DHCP + DNS + NAT     |                           |
|  +---------------+----------------+                           |
|                  | MASQUERADE                                 |
|                  | (iptables/nftables)                        |
|  +---------------+----------------+                           |
|  |  enp0s3 (192.168.1.50)         |                           |
|  +---------------+----------------+                           |
+------------------+--------------------------------------------+
                   |
              Router / Internet
```

### Conectividad

| Direccion | Funciona? | Como |
|-----------|-----------|------|
| VM -> Internet | Si | NAT MASQUERADE via enp0s3 |
| VM -> Host | Si | Via virbr0 (192.168.122.1) |
| VM -> VM (misma red) | Si | Directo por el bridge |
| Host -> VM | Si | Via IP de la VM (192.168.122.x) |
| LAN -> VM | No* | La VM no tiene IP en la LAN |
| Internet -> VM | No* | Doble NAT lo impide |

*Posible con port forwarding (DNAT), pero no es directo.

### Que crea libvirt automaticamente

Cuando activas la red NAT default:

1. **Bridge**: `virbr0`
2. **Proceso dnsmasq**: DHCP (192.168.122.2-254) + DNS forwarding
3. **Reglas iptables/nftables**: MASQUERADE para 192.168.122.0/24
4. **Reglas FORWARD**: permitir trafico saliente de las VMs
5. **ip_forward**: se activa a 1 si no lo estaba

### Cuando usar NAT

- Labs de desarrollo donde las VMs necesitan internet.
- Entornos donde la VM no necesita ser accesible desde fuera del host.
- La opcion mas simple — funciona sin configuracion adicional.

---

## 4. Aislada (sin forward / mode='none')

Una red sin `<forward>` o con `<forward mode='none'/>` es completamente aislada: las VMs se comunican entre si, pero no tienen salida a internet.

### Topologia

```
+--------------------------- HOST ----------------------------+
|                                                              |
|  +-- VM1 ---------+  +-- VM2 ---------+  +-- VM3 --------+ |
|  | 10.0.1.100      |  | 10.0.1.101      |  | 10.0.1.102    | |
|  +------+----------+  +------+----------+  +------+--------+ |
|         +---------------+-----+-----------------+             |
|                         |                                     |
|  +----------------------+----------------------+              |
|  |  virbr1 (10.0.1.1)                          |              |
|  |  bridge + DHCP + DNS                         |              |
|  |  SIN NAT -- SIN FORWARD                     |              |
|  +----------------------------------------------+              |
|                                                              |
|  Sin conexion a enp0s3 ni a internet                         |
+--------------------------------------------------------------+
```

### Conectividad

| Direccion | Funciona? |
|-----------|-----------|
| VM -> Internet | No |
| VM -> Host | Parcial* |
| VM -> VM (misma red) | Si |
| Host -> VM | Parcial* |
| LAN -> VM | No |

*El host tiene una IP en virbr1 (10.0.1.1), asi que puede hacer ping a las VMs y viceversa. Pero no hay NAT ni forwarding — la comunicacion se limita a esa red aislada. Libvirt anade reglas de firewall que restringen el trafico host<->VM en redes aisladas, pero el host siempre puede acceder via `virsh console`.

### Que crea libvirt

1. **Bridge**: `virbr1` (o el nombre que le des)
2. **Proceso dnsmasq**: DHCP + DNS (solo dentro de la red aislada)
3. **Sin reglas NAT**: no hay MASQUERADE
4. **Reglas FORWARD REJECT**: bloquea trafico entre virbr1 y otras interfaces

### Cuando usar aislada

| Escenario | Por que |
|-----------|---------|
| Labs de almacenamiento (RAID, LVM) | No necesitan red externa |
| Labs de firewall/seguridad | VMs no deben escapar |
| Simulacion de red interna | Red "corporativa" sin internet |
| Analisis de malware | Aislamiento total |
| Red backend entre VMs | App->DB sin exponer DB a nada mas |

---

## 5. Routed (forward mode='route')

Modo menos conocido pero util en ciertos escenarios. Las VMs tienen sus propias IPs (en una subred diferente a la LAN), y el host actua como **router** (sin NAT) entre la red de VMs y la LAN.

### Topologia

```
+--------------------------- HOST ----------------------------+
|                                                              |
|  +-- VM1 ---------+   +-- VM2 ---------+                    |
|  | 10.10.0.100     |   | 10.10.0.101     |                    |
|  +------+----------+   +------+----------+                    |
|         +--------+-------------+                              |
|                  |                                             |
|  +---------------+----------------+                           |
|  |  vibrN (10.10.0.1)             |                           |
|  |  bridge + routing (sin NAT)    |                           |
|  +---------------+----------------+                           |
|                  | routing (ip_forward)                       |
|                  | SIN MASQUERADE                             |
|  +---------------+----------------+                           |
|  |  enp0s3 (192.168.1.50)         |                           |
|  +---------------+----------------+                           |
+------------------+--------------------------------------------+
                   |
              Router (192.168.1.1)
```

### Diferencia clave con NAT

En NAT, la IP de la VM se **reemplaza** por la del host (MASQUERADE). En routed, la IP original de la VM **se mantiene**. Pero para que esto funcione, el router de la LAN necesita una **ruta estatica** hacia la red de VMs via el host:

```bash
# En el router de la LAN (o en cada maquina de la LAN):
ip route add 10.10.0.0/24 via 192.168.1.50
#                              --------------
#                              IP del host
```

Sin esta ruta, los dispositivos de la LAN no saben como llegar a 10.10.0.x y los paquetes de vuelta se pierden.

### Cuando usar routed

- Entornos donde necesitas que las VMs sean accesibles con sus IPs reales (sin NAT).
- Cuando tienes control del router para anadir rutas estaticas.
- Labs de routing donde quieres practicar configuracion de rutas.

En la mayoria de labs domesticos, NAT o bridge son mas practicos.

---

## 6. Bridge (forward mode='bridge')

En modo bridge, libvirt no crea un bridge propio — se conecta a un **bridge externo que ya existe en el host** (ej. `br0` creado con NetworkManager). Libvirt no gestiona DHCP ni NAT; las VMs se conectan directamente a la red fisica.

### Topologia

```
+--------------------------- HOST ----------------------------+
|                                                              |
|  +-- VM1 ---------+   +-- VM2 ---------+                    |
|  | 192.168.1       |   | 192.168.1       |   (misma red que  |
|  |      .150       |   |      .151       |    la LAN)         |
|  +------+----------+   +------+----------+                    |
|         | vnet0                | vnet1                        |
|         +--------+-------------+                              |
|                  |                                             |
|  +---------------+----------------+                           |
|  |  br0 (192.168.1.50)            |  <- bridge externo,      |
|  |  (creado con nmcli/ip link)    |     ya existia            |
|  |  enp0s3 como puerto            |                           |
|  +---------------+----------------+                           |
+------------------+--------------------------------------------+
                   |
              Router (192.168.1.1)
              DHCP asigna .150, .151 a las VMs
```

### Que hace libvirt en modo bridge

Casi nada. Libvirt solo:
1. Crea el TAP device (vnetN) para cada VM.
2. Lo conecta al bridge externo especificado.

No crea bridge, no lanza dnsmasq, no configura NAT ni firewall. Todo depende de la configuracion preexistente del bridge y de la LAN.

### Cuando usar bridge

- VMs que ofrecen servicios accesibles desde la LAN (web servers, DBs).
- Entornos que simulan una red "real" con VMs como maquinas fisicas.
- Cuando necesitas que otros dispositivos de la red alcancen las VMs directamente.

---

## 7. Open (forward mode='open')

El modo open es como NAT sin las reglas de firewall automaticas de libvirt. Libvirt crea el bridge pero **no anade ninguna regla de iptables/nftables**. Tu controlas completamente el firewall.

### Cuando usar open

- Cuando usas tu propio firewall (nftables personalizado) y las reglas de libvirt interfieren.
- Entornos donde necesitas control total del trafico entre la red virtual y el exterior.
- Cuando las reglas automaticas de libvirt causan conflictos con otras configuraciones.

```xml
<network>
  <name>custom-firewall</name>
  <forward mode='open'/>
  <bridge name='virbr5' stp='on' delay='0'/>
  <ip address='10.50.0.1' netmask='255.255.255.0'>
    <dhcp>
      <range start='10.50.0.2' end='10.50.0.254'/>
    </dhcp>
  </ip>
</network>
```

Libvirt crea el bridge y dnsmasq, pero no toca iptables. Si quieres que las VMs tengan internet, debes anadir tus propias reglas MASQUERADE.

---

## 8. Comparacion completa de los 5 modos

### Tabla de conectividad

```
                    VM->       VM->      Host->    LAN->     VM<->VM
                    Internet   Host      VM        VM        (misma red)
------------------- --------- -------- --------- --------- ----------
NAT                 Si         Si       Si*       No        Si
Aislada             No         Parcial  Parcial   No        Si
Routed              Si**       Si       Si        Si**      Si
Bridge              Si         Si       Si        Si        Si
Open                Depende    Depende  Depende   Depende   Si

*  Host accede a la VM por su IP privada (192.168.122.x)
** Requiere rutas estaticas en el router de la LAN
```

### Tabla de componentes

| Componente | NAT | Aislada | Routed | Bridge | Open |
|-----------|-----|---------|--------|--------|------|
| Bridge creado por libvirt | Si | Si | Si | No (externo) | Si |
| dnsmasq (DHCP+DNS) | Si | Si | Si | No | Si |
| Reglas de firewall | Si (MASQUERADE) | Si (REJECT forward) | Si (ACCEPT forward) | No | No |
| ip_forward necesario | Si | No | Si | No | Depende |
| Interfaz fisica en bridge | No | No | No | Si | No |

### Diagrama de decision

```
La VM necesita internet?
+-- NO -> Las VMs necesitan comunicarse entre si?
|         +-- SI -> AISLADA
|         +-- NO -> (para que necesitas red?)
|
+-- SI -> La VM debe ser accesible desde la LAN?
          +-- NO -> NAT (el default, lo mas simple)
          |
          +-- SI -> Tienes Ethernet (no WiFi)?
                    +-- SI -> BRIDGE
                    +-- NO -> NAT + port forwarding
                              (o bridge macvtap si lo soporta)
```

---

## 9. XML de cada tipo de red

### NAT (con todas las opciones)

```xml
<network>
  <name>lab-nat</name>
  <forward mode='nat'>
    <nat>
      <port start='1024' end='65535'/>
    </nat>
  </forward>
  <bridge name='virbr0' stp='on' delay='0'/>
  <mac address='52:54:00:aa:bb:cc'/>
  <domain name='lab.nat' localOnly='yes'/>
  <dns>
    <host ip='192.168.122.10'>
      <hostname>web.lab.nat</hostname>
    </host>
  </dns>
  <ip address='192.168.122.1' netmask='255.255.255.0'>
    <dhcp>
      <range start='192.168.122.2' end='192.168.122.254'/>
      <host mac='52:54:00:11:22:33' name='web-server' ip='192.168.122.10'/>
    </dhcp>
  </ip>
</network>
```

| Elemento | Funcion |
|----------|---------|
| `<forward mode='nat'>` | Habilitar NAT |
| `<port start/end>` | Rango de puertos para traduccion NAT |
| `<bridge name>` | Nombre del bridge (virbr0, vibrN) |
| `<mac address>` | MAC del bridge (generada automaticamente si se omite) |
| `<domain name localOnly>` | Dominio DNS local (solo dentro de la red) |
| `<dns><host>` | Registros DNS personalizados |
| `<dhcp><range>` | Pool de IPs dinamicas |
| `<dhcp><host>` | Reserva DHCP (IP fija por MAC) |

### Aislada (minima)

```xml
<network>
  <name>lab-isolated</name>
  <bridge name='virbr1' stp='on' delay='0'/>
  <ip address='10.0.1.1' netmask='255.255.255.0'>
    <dhcp>
      <range start='10.0.1.2' end='10.0.1.254'/>
    </dhcp>
  </ip>
</network>
```

Sin `<forward>`: la red es aislada por defecto.

### Aislada (sin DHCP — IPs estaticas)

```xml
<network>
  <name>static-isolated</name>
  <bridge name='virbr2' stp='on' delay='0'/>
  <ip address='10.0.2.1' netmask='255.255.255.0'/>
</network>
```

Sin `<dhcp>`: las VMs necesitan IP estatica configurada manualmente.

### Routed

```xml
<network>
  <name>lab-routed</name>
  <forward mode='route' dev='enp0s3'/>
  <bridge name='virbr3' stp='on' delay='0'/>
  <ip address='10.10.0.1' netmask='255.255.255.0'>
    <dhcp>
      <range start='10.10.0.2' end='10.10.0.254'/>
    </dhcp>
  </ip>
</network>
```

`dev='enp0s3'`: especifica por que interfaz sale el trafico ruteado.

### Bridge (externo)

```xml
<network>
  <name>host-bridge</name>
  <forward mode='bridge'/>
  <bridge name='br0'/>
</network>
```

El XML mas simple: solo apunta al bridge externo. No tiene `<ip>` ni `<dhcp>` porque eso lo gestiona la red real.

### Open

```xml
<network>
  <name>lab-open</name>
  <forward mode='open'/>
  <bridge name='virbr5' stp='on' delay='0'/>
  <ip address='10.50.0.1' netmask='255.255.255.0'>
    <dhcp>
      <range start='10.50.0.2' end='10.50.0.254'/>
    </dhcp>
  </ip>
</network>
```

---

## 10. Multiples redes: VMs con varias interfaces

Una VM puede tener **multiples interfaces de red**, cada una conectada a una red diferente. Esto es esencial para simular topologias complejas.

### Caso: VM router entre red NAT y red aislada

```xml
<!-- VM con dos interfaces -->
<interface type='network'>
  <source network='default'/>          <!-- NAT: acceso a internet -->
  <model type='virtio'/>
</interface>
<interface type='network'>
  <source network='lab-isolated'/>     <!-- Aislada: red interna -->
  <model type='virtio'/>
</interface>
```

Dentro de la VM:
```bash
ip addr
# eth0: 192.168.122.100/24   <- red NAT (internet)
# eth1: 10.0.1.100/24        <- red aislada (interna)
```

### Topologia multi-red

```
Internet
    |
+---+---- Red NAT (default) ---------------------------+
|   192.168.122.0/24                                    |
|                                                       |
|   +-- Router-VM ---+                                  |
|   | eth0: .122.100  |---- virbr0 ---- (NAT) ---- Internet
|   | eth1: .1.100    |                                  |
|   +-------+---------+                                  |
+-----------+-------------------------------------------+
            |
+-----------+-- Red aislada (lab-isolated) -------------+
|   10.0.1.0/24                                         |
|                                                       |
|   +-- Web-VM --------+   +-- DB-VM ---------+        |
|   | eth0: .1.101      |   | eth0: .1.102      |        |
|   +-------------------+   +-------------------+        |
|                                                       |
|   Estas VMs NO tienen internet directamente.          |
|   Si Router-VM habilita ip_forward + NAT,             |
|   pueden salir a internet a traves de Router-VM.      |
+-------------------------------------------------------+
```

Este patron es fundamental para los bloques B09/B10 del curso.

### Crear una VM con multiples redes (virt-install)

```bash
virt-install \
    --name router-vm \
    --network network=default \
    --network network=lab-isolated \
    --disk size=10 \
    --os-variant fedora39 \
    ...
```

---

## 11. Que tipo de red usar en cada escenario del curso

| Bloque | Escenario | Red recomendada | Justificacion |
|--------|-----------|----------------|---------------|
| B08 C00 | Setup inicial QEMU/KVM | NAT (default) | Las VMs necesitan internet para instalar paquetes |
| B08 | Labs de almacenamiento (RAID, LVM) | Aislada | La red no es relevante, solo discos |
| B09 | Topologias multi-VM | NAT + Aislada | VM router con dos interfaces |
| B09 | Simular una red corporativa | Multiples aisladas | Cada subred es una red aislada separada |
| B10 | Servidor DNS/DHCP | NAT + Aislada | VM servidor con internet + red interna |
| B10 | Cluster de servicios | Aislada | Comunicacion inter-VM sin internet |
| B11 | Labs de firewall | Aislada | VMs no deben escapar a la red real |
| B11 | Docker | NAT (default de Docker) | docker0 es un bridge NAT, mismo concepto |
| General | VM de desarrollo | NAT | Internet para apt/dnf, sin exposicion |
| General | VM con web server accesible | Bridge | Visible desde la LAN |

---

## 12. Errores comunes

### Error 1: Confundir "aislada" con "sin red"

```bash
# "La red aislada no tiene red, asi que no necesito configurar nada"
# -> INCORRECTO. La red aislada SI tiene red -- las VMs se comunican entre si.
#   Solo no tiene salida a internet.

# Una VM en red aislada con DHCP obtiene:
# IP: 10.0.1.100
# Gateway: 10.0.1.1 (pero no lleva a internet)
# DNS: 10.0.1.1 (solo resuelve hostnames de VMs locales)
```

### Error 2: Crear multiples redes NAT con el mismo rango

```bash
virsh net-dumpxml default
# 192.168.122.0/24

virsh net-dumpxml my-nat
# 192.168.122.0/24    <- MISMO RANGO -> CONFLICTO

# Resultado: routing ambiguo, una de las dos redes no funciona.
# Solucion: cada red NAT necesita un rango diferente.
```

### Error 3: Usar bridge sin tener un bridge externo creado

```xml
<!-- Defines una red bridge: -->
<network>
  <name>my-bridge</name>
  <forward mode='bridge'/>
  <bridge name='br0'/>
</network>
```

```bash
virsh net-start my-bridge
# error: failed to start network 'my-bridge'
# Network interface 'br0' not found
```

El modo bridge requiere que `br0` **ya exista** en el host. Libvirt no lo crea.

### Error 4: Esperar DHCP en modo bridge

```bash
# La VM esta en modo bridge pero no obtiene IP por DHCP

# El DHCP de donde vendria?
# En modo bridge, libvirt NO lanza dnsmasq.
# El DHCP debe venir del router real de la LAN.
# Si el router no tiene DHCP o no llega al bridge, la VM no obtiene IP.

# Diagnostico dentro de la VM:
sudo dhclient -v eth0
# Si dice "No DHCPOFFERS" -> el DHCP del router no llega
```

### Error 5: No entender que NAT y aislada tienen DHCP propio

```bash
# "Mi VM en red aislada no puede obtener IP por DHCP"
# -> Si puede. libvirt lanza dnsmasq para redes aisladas y NAT.
# -> Verificar: virsh net-dhcp-leases lab-isolated

# "Mi VM en modo bridge no obtiene IP"
# -> En bridge, NO hay dnsmasq de libvirt. El DHCP viene del router real.
```

---

## 13. Cheatsheet

```text
+======================================================================+
|                 TIPOS DE REDES EN LIBVIRT CHEATSHEET                  |
+======================================================================+
|                                                                      |
|  MODOS DE RED                                                        |
|  NAT       <forward mode='nat'/>     Internet via MASQUERADE         |
|  Aislada   (sin <forward>)           Solo VM<->VM, sin internet      |
|  Routed    <forward mode='route'/>   Routing sin NAT                 |
|  Bridge    <forward mode='bridge'/>  Bridge externo existente        |
|  Open      <forward mode='open'/>    Sin reglas firewall libvirt     |
|                                                                      |
+----------------------------------------------------------------------+
|                                                                      |
|  CONECTIVIDAD                                                        |
|              VM->Net  VM->Host  Host->VM  LAN->VM  VM<->VM          |
|  NAT          Si       Si        Si*       No        Si              |
|  Aislada      No       ~         ~         No        Si              |
|  Routed       Si+      Si        Si        Si+       Si              |
|  Bridge       Si       Si        Si        Si        Si              |
|  * Via IP privada    + Requiere rutas estaticas                      |
|                                                                      |
+----------------------------------------------------------------------+
|                                                                      |
|  QUE GESTIONA LIBVIRT EN CADA MODO                                   |
|              Bridge  dnsmasq  Firewall  NAT                          |
|  NAT          Crea    Si       Si        Si                          |
|  Aislada      Crea    Si       Si        No                          |
|  Routed       Crea    Si       Si        No                          |
|  Bridge       No      No       No        No                          |
|  Open         Crea    Si       No        No                          |
|                                                                      |
+----------------------------------------------------------------------+
|                                                                      |
|  DECISION RAPIDA                                                     |
|  Solo necesita internet?              -> NAT                         |
|  Solo VM<->VM sin internet?           -> Aislada                     |
|  Accesible desde la LAN?             -> Bridge                       |
|  Router entre redes?                  -> VM con 2+ interfaces        |
|                                                                      |
+----------------------------------------------------------------------+
|                                                                      |
|  MULTIPLES INTERFACES                                                |
|  virt-install --network network=default --network network=lab        |
|  virsh edit: multiples <interface>                                   |
|  Dentro de la VM: eth0 = primera red, eth1 = segunda red             |
|                                                                      |
+----------------------------------------------------------------------+
|                                                                      |
|  RANGOS COMUNES                                                      |
|  NAT default:    192.168.122.0/24                                    |
|  NAT adicional:  192.168.123.0/24, 192.168.124.0/24, ...            |
|  Aislada:        10.0.1.0/24, 10.0.2.0/24, ...                     |
|  Bridge:         (IP de la LAN real del host)                        |
|  Cada red necesita un rango UNICO!                                   |
|                                                                      |
+======================================================================+
```

---

## 14. Ejercicios

### Ejercicio 1: Identificar el modo de red por XML

Dado este XML, indica que modo de red es y justifica:

```xml
<network>
  <name>mystery</name>
  <bridge name='virbr3' stp='on' delay='0'/>
  <ip address='10.0.5.1' netmask='255.255.255.0'>
    <dhcp>
      <range start='10.0.5.2' end='10.0.5.254'/>
    </dhcp>
  </ip>
</network>
```

<details><summary>Prediccion</summary>

Es una red **aislada**. No tiene elemento `<forward>`, lo que significa que libvirt no configura NAT, routing, ni bridge externo. Solo crea un bridge interno (virbr3) con DHCP (dnsmasq).

Las VMs en esta red pueden comunicarse entre si (via virbr3 en 10.0.5.0/24), pero no tienen salida a internet. El host tiene IP 10.0.5.1 en virbr3 y puede alcanzar las VMs (parcialmente, sujeto a reglas de firewall de libvirt).

Si tuviera `<forward mode='nat'/>`, seria NAT. Si tuviera `<forward mode='route'/>`, seria routed. La ausencia de `<forward>` es la marca de una red aislada.

</details>

---

### Ejercicio 2: Tabla de conectividad

Para la red NAT default (192.168.122.0/24), responde si o no a cada caso y explica por que:

1. VM (192.168.122.100) hace `ping 8.8.8.8`
2. VM (192.168.122.100) hace `ping 192.168.1.50` (IP del host en la LAN)
3. Host hace `ping 192.168.122.100`
4. Laptop de un companero (192.168.1.200) hace `ping 192.168.122.100`
5. VM1 (192.168.122.100) hace `ping 192.168.122.101` (VM2)

<details><summary>Prediccion</summary>

1. **Si**. La VM sale a internet via MASQUERADE. El paquete va a virbr0 -> iptables reemplaza la IP origen (192.168.122.100) por la del host (192.168.1.50) -> sale por enp0s3 -> llega a 8.8.8.8. La respuesta vuelve por el mismo camino (conntrack).

2. **Si**. La VM puede alcanzar la IP del host en la LAN (192.168.1.50) porque el host actua como router/gateway (192.168.122.1). El paquete sale de la VM -> virbr0 -> el kernel del host lo rutea porque es una de sus propias IPs. No necesita MASQUERADE para esto.

3. **Si**. El host tiene IP 192.168.122.1 en virbr0, que esta en la misma subred que la VM. Puede hacer ping directamente a 192.168.122.100 sin ningun routing adicional.

4. **No**. La laptop de un companero esta en 192.168.1.0/24. No conoce la red 192.168.122.0/24 — esa red es privada y solo existe dentro del host. El router de la LAN no tiene ruta hacia 192.168.122.0/24. El ping se pierde.

5. **Si**. Ambas VMs estan en la misma red (192.168.122.0/24) conectadas al mismo bridge (virbr0). El trafico va directo por el bridge sin pasar por NAT.

</details>

---

### Ejercicio 3: Componentes por modo

Para cada modo, indica si libvirt crea los siguientes componentes: bridge, dnsmasq, reglas MASQUERADE, reglas FORWARD REJECT.

| | NAT | Aislada | Bridge |
|---|---|---|---|
| Bridge propio | ? | ? | ? |
| dnsmasq | ? | ? | ? |
| MASQUERADE | ? | ? | ? |
| FORWARD REJECT | ? | ? | ? |

<details><summary>Prediccion</summary>

| | NAT | Aislada | Bridge |
|---|---|---|---|
| Bridge propio | **Si** (virbr0) | **Si** (virbr1) | **No** (usa br0 externo) |
| dnsmasq | **Si** (DHCP + DNS) | **Si** (DHCP + DNS) | **No** (DHCP del router real) |
| MASQUERADE | **Si** (NAT saliente) | **No** (sin salida) | **No** (sin NAT) |
| FORWARD REJECT | **No** (ACCEPT saliente) | **Si** (bloquea salida) | **No** (sin reglas) |

Punto clave: NAT y aislada crean la misma infraestructura basica (bridge + dnsmasq), pero difieren en las reglas de firewall. NAT permite salida con MASQUERADE. Aislada bloquea la salida con REJECT. Bridge no crea nada — solo conecta VMs a un bridge preexistente.

</details>

---

### Ejercicio 4: Rangos sin conflicto

Tu sistema tiene estas redes:
- default (NAT): 192.168.122.0/24
- lab-net (NAT): 192.168.123.0/24

Necesitas crear dos redes mas: una NAT adicional y una aislada.

**Pregunta:** Que rangos usarias para evitar conflictos? Y si tu LAN domestica es 192.168.1.0/24, eso es un conflicto?

<details><summary>Prediccion</summary>

**Rangos sugeridos:**
- NAT adicional: **192.168.124.0/24** (sigue el patron .122, .123, .124)
- Aislada: **10.0.1.0/24** (usa el espacio 10.x.x.x para redes aisladas, claramente diferenciado de 192.168.x)

**La LAN 192.168.1.0/24 no es un conflicto** con las redes de libvirt porque:
- 192.168.1.0/24 es la LAN real del host.
- 192.168.122.0/24, .123.0/24, .124.0/24 son redes privadas de libvirt.
- Estan en subredes diferentes del espacio 192.168.x.x.

Solo habria conflicto si dos redes de libvirt usaran el mismo rango (ej. dos redes con 192.168.122.0/24), o si una red de libvirt usara el mismo rango que la LAN (ej. 192.168.1.0/24 como red NAT — el host no sabria si rutear hacia la LAN o hacia virbr).

</details>

---

### Ejercicio 5: Diagrama de decision

Para cada escenario, sigue el diagrama de decision y elige el modo:

1. 3 VMs compilando software con `dnf install`. No necesitan ser accesibles.
2. VM con nginx que tus companeros acceden desde sus laptops.
3. 2 VMs que simulan cliente-servidor en una red corporativa interna.
4. VM donde analizas malware sospechoso.
5. VM con nftables personalizado donde las reglas de libvirt interfieren.

<details><summary>Prediccion</summary>

1. **NAT**. Necesitan internet (dnf install) -> si. Accesibles desde la LAN -> no. NAT es la respuesta directa.

2. **Bridge**. Necesitan internet -> si. Accesibles desde la LAN -> si (companeros). Tienes Ethernet -> (asumiendo si) -> Bridge.

3. **Aislada**. Necesitan internet -> no. VMs se comunican entre si -> si. Red aislada.

4. **Aislada**. Necesitan internet -> definitivamente no (malware no debe escapar). Red aislada con aislamiento total.

5. **Open**. Es un caso especial: necesitas internet pero quieres controlar el firewall tu mismo. `<forward mode='open'/>` crea bridge y dnsmasq pero no toca iptables. Tu configuras las reglas MASQUERADE y FORWARD manualmente.

</details>

---

### Ejercicio 6: Red bridge — prerrequisitos

Escribes este XML y ejecutas `virsh net-start`:

```xml
<network>
  <name>my-bridge</name>
  <forward mode='bridge'/>
  <bridge name='br0'/>
</network>
```

**Pregunta:** Que debe existir antes de que este comando funcione? Si la VM no obtiene IP despues de conectarse, donde buscarias el DHCP?

<details><summary>Prediccion</summary>

**Prerrequisitos:**
1. El bridge **br0 debe existir** en el host (creado con `nmcli`, `ip link add`, o similar).
2. **enp0s3** (u otra interfaz fisica) debe ser **puerto de br0**.
3. **br0 debe tener IP** (o DHCP configurado) para que el host mantenga conectividad.
4. El bridge debe estar **up** (`ip link set br0 up`).

Si `br0` no existe, `virsh net-start` falla con "Network interface 'br0' not found".

**DHCP para la VM:**
En modo bridge, libvirt **no lanza dnsmasq**. No hay DHCP de libvirt. El DHCP debe venir del **router real de la LAN** (ej. 192.168.1.1). Si la VM no obtiene IP:
- Verificar que el router tiene DHCP activo.
- Verificar que el bridge esta correctamente conectado a la red fisica.
- Dentro de la VM: `sudo dhclient -v eth0` para ver si llegan DHCPOFFERS.

</details>

---

### Ejercicio 7: Multiples interfaces — VM router

Necesitas una VM "gateway" que conecte una red aislada a internet. La VM tendra dos interfaces.

**Pregunta:** Escribe las dos secciones `<interface>` del XML de la VM. Dentro de la VM, que configuracion necesitas habilitar para que las VMs de la red aislada puedan acceder a internet a traves de "gateway"?

<details><summary>Prediccion</summary>

**XML de la VM:**

```xml
<interface type='network'>
  <source network='default'/>          <!-- NAT: internet -->
  <model type='virtio'/>
</interface>
<interface type='network'>
  <source network='lab-isolated'/>     <!-- Aislada: red interna -->
  <model type='virtio'/>
</interface>
```

**Configuracion dentro de la VM "gateway":**

```bash
# 1. Habilitar IP forwarding
sudo sysctl -w net.ipv4.ip_forward=1

# 2. Configurar NAT (MASQUERADE) para el trafico de la red aislada
sudo iptables -t nat -A POSTROUTING -s 10.0.1.0/24 -o eth0 -j MASQUERADE
sudo iptables -A FORWARD -i eth1 -o eth0 -j ACCEPT
sudo iptables -A FORWARD -i eth0 -o eth1 -m state --state RELATED,ESTABLISHED -j ACCEPT
```

Donde eth0 = NAT (internet) y eth1 = aislada (10.0.1.0/24).

Las VMs de la red aislada deben usar la IP de "gateway" en eth1 (10.0.1.100) como su **gateway por defecto**. Si usan DHCP de la red aislada, el gateway sera 10.0.1.1 (virbr1 del host) — hay que cambiarlo a 10.0.1.100 o configurarlo via DHCP options.

</details>

---

### Ejercicio 8: XML completo de red aislada

Escribe el XML completo para una red aislada llamada "lab-internal" con:
- Rango: 10.0.10.0/24
- DHCP: .50 a .200
- Reserva: MAC 52:54:00:db:00:01 -> IP 10.0.10.5, nombre "db-server"

<details><summary>Prediccion</summary>

```xml
<network>
  <name>lab-internal</name>
  <bridge name='virbr-lab' stp='on' delay='0'/>
  <ip address='10.0.10.1' netmask='255.255.255.0'>
    <dhcp>
      <range start='10.0.10.50' end='10.0.10.200'/>
      <host mac='52:54:00:db:00:01' name='db-server' ip='10.0.10.5'/>
    </dhcp>
  </ip>
</network>
```

Puntos clave:
- **Sin `<forward>`**: la ausencia de este elemento hace la red aislada.
- La reserva DHCP (IP .5) esta **fuera** del rango dinamico (.50-.200) — buena practica para evitar conflictos.
- El bridge se llama `virbr-lab` (libvirt lo crea automaticamente).
- El host tendra IP 10.0.10.1 en virbr-lab.

Para usarla:
```bash
virsh net-define lab-internal.xml
virsh net-start lab-internal
virsh net-autostart lab-internal
```

</details>

---

### Ejercicio 9: Error de rango duplicado

Creas dos redes NAT:

```xml
<!-- Red 1 -->
<network>
  <name>dev-net</name>
  <forward mode='nat'/>
  <bridge name='virbr10'/>
  <ip address='192.168.122.1' netmask='255.255.255.0'>
    <dhcp><range start='192.168.122.2' end='192.168.122.254'/></dhcp>
  </ip>
</network>

<!-- Red 2 (la default ya existente) -->
<!-- Tambien usa 192.168.122.0/24 -->
```

**Pregunta:** Que pasara? Ambas redes pueden funcionar simultaneamente? Como detectarias el conflicto?

<details><summary>Prediccion</summary>

**Que pasara**: las dos redes usan el **mismo rango** (192.168.122.0/24). Esto causa **routing ambiguo** en el host. El kernel tiene dos bridges (virbr0 y virbr10) con la misma IP de gateway (192.168.122.1). Los paquetes de las VMs pueden ir al bridge equivocado. Los leases DHCP de dnsmasq pueden conflictar. En la practica, una de las dos redes dejara de funcionar correctamente.

**Deteccion:**

```bash
# Listar todas las redes y sus rangos
virsh net-list --all
virsh net-dumpxml default | grep "ip address"
virsh net-dumpxml dev-net | grep "ip address"

# Si ambas muestran 192.168.122.1 -> conflicto

# Tambien visible con ip addr:
ip addr show | grep 192.168.122
# Si aparecen dos interfaces con la misma IP -> problema
```

**Solucion**: cambiar el rango de dev-net a 192.168.123.0/24 (o cualquier otro rango unico). Cada red virtual necesita su propio rango de IPs.

</details>

---

### Ejercicio 10: Diseno completo de lab

Necesitas un lab con esta topologia:

- **VM "router"**: 2 interfaces (internet + red interna)
- **VM "web"**: 1 interfaz (red interna), nginx
- **VM "db"**: 1 interfaz (red interna), PostgreSQL
- Solo "router" tiene internet. "web" y "db" acceden a internet a traves de "router".

**Pregunta:** Cuantas redes libvirt necesitas? Escribe los nombres, modos y rangos. Que red usaria cada VM?

<details><summary>Prediccion</summary>

**2 redes:**

1. **default** (NAT, 192.168.122.0/24) — ya existe, proporciona internet.
2. **lab-backend** (Aislada, 10.0.1.0/24) — red interna para web y db.

**Asignacion de VMs:**

| VM | Redes | Interfaces |
|----|-------|-----------|
| router | default + lab-backend | eth0: 192.168.122.x (NAT), eth1: 10.0.1.x (aislada) |
| web | lab-backend | eth0: 10.0.1.x |
| db | lab-backend | eth0: 10.0.1.x |

**Flujo de internet para web/db:**
1. web/db envian paquetes a su gateway (10.0.1.x de router, eth1).
2. router tiene ip_forward=1 y MASQUERADE en eth0.
3. Los paquetes salen por la red NAT (192.168.122.0/24) -> virbr0 -> MASQUERADE del host -> internet.

Es un **doble NAT**: VM web -> router VM (NAT 1) -> host (NAT 2) -> internet. Funciona pero anade latencia. Para labs es aceptable.

</details>

---

> **Nota**: los tipos de red en libvirt son la herramienta que define la topologia de tu lab. NAT y aislada cubren el 90% de los casos. La clave es entender que cada tipo genera diferentes componentes (bridge, dnsmasq, reglas de firewall) y que la eleccion correcta depende de una pregunta simple: quien necesita hablar con quien? Una vez respondes eso, el diagrama de decision te lleva directamente al modo correcto.
