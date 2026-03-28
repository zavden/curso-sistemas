# Máscara de Red y CIDR

## Índice

1. [El problema: ¿cómo sabe un paquete si su destino está en la misma red?](#1-el-problema-cómo-sabe-un-paquete-si-su-destino-está-en-la-misma-red)
2. [Qué es una máscara de red](#2-qué-es-una-máscara-de-red)
3. [La operación AND: cómo la máscara separa red de host](#3-la-operación-and-cómo-la-máscara-separa-red-de-host)
4. [Notación CIDR](#4-notación-cidr)
5. [Tabla de referencia: las máscaras más usadas](#5-tabla-de-referencia-las-máscaras-más-usadas)
6. [Dirección de red, broadcast y hosts usables](#6-dirección-de-red-broadcast-y-hosts-usables)
7. [Cálculo de subred paso a paso](#7-cálculo-de-subred-paso-a-paso)
8. [¿Están dos IPs en la misma red?](#8-están-dos-ips-en-la-misma-red)
9. [Subnetting: dividir una red en subredes](#9-subnetting-dividir-una-red-en-subredes)
10. [La herramienta ipcalc](#10-la-herramienta-ipcalc)
11. [Máscaras en el contexto de QEMU/KVM y libvirt](#11-máscaras-en-el-contexto-de-qemukvm-y-libvirt)
12. [Errores comunes](#12-errores-comunes)
13. [Cheatsheet](#13-cheatsheet)
14. [Ejercicios](#14-ejercicios)

---

## 1. El problema: ¿cómo sabe un paquete si su destino está en la misma red?

Cuando una máquina quiere enviar un paquete a otra IP, necesita decidir:

- **¿Está el destino en mi misma red local?** → lo envío directamente (capa 2, Ethernet).
- **¿Está en otra red?** → lo envío a mi gateway (router) para que lo encamine.

Esa decisión es imposible sin saber dónde termina "mi red" y empieza "otra red". Eso es exactamente lo que define la **máscara de red**: el límite entre la parte de red y la parte de host en una dirección IP.

Ejemplo concreto en virtualización:

```
Tu VM tiene:      192.168.122.100/24
Quiere hablar con: 192.168.122.50   → misma red → envío directo
Quiere hablar con: 8.8.8.8          → otra red   → envío al gateway (192.168.122.1)
```

¿Cómo determinó la VM que 192.168.122.50 está en su red y 8.8.8.8 no? Aplicó su máscara `/24` a ambas direcciones. Eso es lo que aprenderás en este tópico.

---

## 2. Qué es una máscara de red

Una máscara de red es un número de 32 bits que divide una dirección IP en dos partes:

```
IP:       192.168.1.100
Máscara:  255.255.255.0

          ┌── parte de RED ──┐┌─ parte de HOST ─┐
IP:       192  . 168  .  1   .     100
Máscara:  255  . 255  . 255  .       0
```

- Los bits en `1` de la máscara marcan la **porción de red** (identifica a qué red pertenece).
- Los bits en `0` de la máscara marcan la **porción de host** (identifica al dispositivo dentro de esa red).

En binario es más claro:

```
IP:       11000000 . 10101000 . 00000001 . 01100100
Máscara:  11111111 . 11111111 . 11111111 . 00000000
          ├── RED (24 bits de 1s) ──────┤├ HOST ──┤
```

### Regla fundamental de las máscaras

Los bits `1` van siempre a la izquierda, agrupados. Los bits `0` van siempre a la derecha. **Nunca se mezclan**:

```
Válido:   11111111.11111111.11111100.00000000  (/22)
Válido:   11111111.11111111.11111111.11000000  (/26)
Inválido: 11111111.10101010.11111111.00000000  ← 1s y 0s intercalados
```

Esto garantiza que la máscara define un bloque contiguo de direcciones.

---

## 3. La operación AND: cómo la máscara separa red de host

Para determinar la dirección de red, se aplica una operación **AND bit a bit** entre la IP y la máscara:

```
AND lógico:
  1 AND 1 = 1
  1 AND 0 = 0
  0 AND 1 = 0
  0 AND 0 = 0
```

### Ejemplo: ¿cuál es la red de 192.168.1.100/24?

```
IP:         11000000 . 10101000 . 00000001 . 01100100   (192.168.1.100)
Máscara:    11111111 . 11111111 . 11111111 . 00000000   (255.255.255.0)
            ─────────────────────────────────────────
AND:        11000000 . 10101000 . 00000001 . 00000000   (192.168.1.0)
                                              ────────
                                              los bits de host
                                              se "borran" a 0
```

Resultado: **192.168.1.0** es la dirección de red.

La máscara actuó como un filtro: preservó los bits de red (donde hay `1`) y puso a cero los bits de host (donde hay `0`).

### Ejemplo con máscara menos común: 10.20.30.40/20

```
IP:         00001010 . 00010100 . 00011110 . 00101000   (10.20.30.40)
Máscara:    11111111 . 11111111 . 11110000 . 00000000   (255.255.240.0)
            ─────────────────────────────────────────
AND:        00001010 . 00010100 . 00010000 . 00000000   (10.20.16.0)
```

La red es **10.20.16.0/20** — no 10.20.30.0. Esto no es intuitivo mirando solo los decimales, pero el AND binario no miente.

### ¿Por qué importa esto en la práctica?

Cuando configuras una red virtual con libvirt, defines la red como `192.168.122.0/24`. Si la cambias a `/20`, el rango de IPs disponibles es radicalmente distinto. Si asignas manualmente una IP fuera de ese rango, la VM no podrá comunicarse con las demás aunque estén "en la misma red virtual" — porque la máscara dice que no lo están.

---

## 4. Notación CIDR

CIDR (Classless Inter-Domain Routing) es una forma compacta de expresar la máscara. El número después de `/` indica cuántos bits consecutivos están en `1`:

```
/24  →  11111111.11111111.11111111.00000000  →  255.255.255.0
/16  →  11111111.11111111.00000000.00000000  →  255.255.0.0
/8   →  11111111.00000000.00000000.00000000  →  255.0.0.0
/26  →  11111111.11111111.11111111.11000000  →  255.255.255.192
```

### Antes de CIDR: las clases (historia breve)

Antes de 1993, las redes se dividían en "clases" rígidas:

| Clase | Rango | Máscara fija | Redes | Hosts por red |
|-------|-------|-------------|-------|---------------|
| A | 1.0.0.0 – 126.0.0.0 | /8 | 126 | ~16 millones |
| B | 128.0.0.0 – 191.255.0.0 | /16 | ~16,000 | ~65,000 |
| C | 192.0.0.0 – 223.255.255.0 | /24 | ~2 millones | 254 |

Este sistema era enormemente ineficiente. Una empresa que necesitaba 300 hosts obtenía una clase B con 65,534 direcciones — desperdiciando 65,234. CIDR eliminó las clases y permite máscaras de cualquier longitud, asignando bloques del tamaño justo.

Hoy en día las "clases" ya no se usan para asignación, pero la terminología sobrevive coloquialmente (se sigue diciendo "clase C" para referirse a /24 o "clase A" para /8). En este curso usamos exclusivamente CIDR.

### De máscara decimal a CIDR y viceversa

Contar los `1s` en la máscara binaria te da el CIDR:

```
255.255.255.0   → 11111111.11111111.11111111.00000000 → 24 unos → /24
255.255.240.0   → 11111111.11111111.11110000.00000000 → 20 unos → /20
255.255.255.192 → 11111111.11111111.11111111.11000000 → 26 unos → /26
```

Para ir de CIDR a máscara decimal, escribe N unos seguidos de (32-N) ceros y agrupa en octetos:

```
/27 → 27 unos + 5 ceros
    → 11111111.11111111.11111111.11100000
    →    255   .   255  .   255  .   224
    = 255.255.255.224
```

---

## 5. Tabla de referencia: las máscaras más usadas

```
CIDR    Máscara decimal      IPs totales    Hosts usables    Uso típico
────    ───────────────      ───────────    ─────────────    ──────────
/32     255.255.255.255      1              0 (1 IP exacta)  Host route, loopback
/31     255.255.255.254      2              2 (punto a punto) Links punto a punto
/30     255.255.255.252      4              2                 Links entre routers
/29     255.255.255.248      8              6                 Micro-red
/28     255.255.255.240      16             14                Red muy pequeña
/27     255.255.255.224      32             30                Oficina pequeña
/26     255.255.255.192      64             62                Oficina mediana
/25     255.255.255.128      128            126               Piso de oficina
/24     255.255.255.0        256            254               Red LAN estándar ★
/23     255.255.254.0        512            510               LAN doble
/22     255.255.252.0        1,024          1,022             Campus
/20     255.255.240.0        4,096          4,094             Red corporativa
/16     255.255.0.0          65,536         65,534            Red grande
/12     255.240.0.0          1,048,576      1,048,574         Rango privado 172.16.0.0/12
/8      255.0.0.0            16,777,216     16,777,214        Rango privado 10.0.0.0/8
/0      0.0.0.0              4,294,967,296  —                 Ruta por defecto
```

### La fórmula rápida

```
IPs totales    = 2^(32 - CIDR)
Hosts usables  = 2^(32 - CIDR) - 2

/24:  2^8  = 256 IPs,  254 hosts
/26:  2^6  = 64 IPs,   62 hosts
/28:  2^4  = 16 IPs,   14 hosts
/20:  2^12 = 4096 IPs, 4094 hosts
```

¿Por qué se restan 2? Porque en cada subred hay dos direcciones reservadas:
1. La primera IP del rango es la **dirección de red** (identifica la red, no asignable).
2. La última IP del rango es la **dirección de broadcast** (enviar a todos, no asignable).

Excepción: `/31` se usa para enlaces punto a punto (RFC 3021) — no necesita broadcast ni dirección de red porque hay exactamente 2 hosts.

---

## 6. Dirección de red, broadcast y hosts usables

Para cualquier combinación IP/máscara, se derivan cuatro valores fundamentales:

### Ejemplo con 192.168.122.0/24 (red default de libvirt)

```
Dirección de red:        192.168.122.0       ← identifica la red
Primera IP usable:       192.168.122.1       ← típicamente el gateway (virbr0)
Última IP usable:        192.168.122.254     ← último host asignable
Dirección de broadcast:  192.168.122.255     ← enviar a todos en la red
Hosts usables:           254                 ← 256 - 2
```

### Ejemplo con 10.0.1.0/28

```
Red:          10.0.1.0
Primera:      10.0.1.1
Última:       10.0.1.14
Broadcast:    10.0.1.15
Hosts:        14
```

¿De dónde sale el 15? Con /28 hay 4 bits de host. El host "todo ceros" es la dirección de red y el host "todo unos" es broadcast:

```
10.0.1.    0000   → 10.0.1.0    (red)
10.0.1.    0001   → 10.0.1.1    (primer host)
10.0.1.    0010   → 10.0.1.2
...
10.0.1.    1110   → 10.0.1.14   (último host)
10.0.1.    1111   → 10.0.1.15   (broadcast)
           ────
           4 bits de host → 2^4 = 16 combinaciones, 14 asignables
```

### Ejemplo no trivial: 172.16.50.100/20

Para encontrar la dirección de red, necesitas el AND:

```
IP:       172.16.50.100
Máscara:  /20 = 255.255.240.0

Tercer octeto (donde la máscara no es ni 255 ni 0):
  50 en binario:    00110010
  240 en binario:   11110000
  AND:              00110000 = 48

Red:          172.16.48.0
Broadcast:    172.16.63.255     (48 + 16 subredes de 256 - 1 = 63.255)
Primera:      172.16.48.1
Última:       172.16.63.254
Hosts:        4094
```

La dirección de red **no es** 172.16.50.0 — eso es un error frecuente al mirar solo los decimales sin aplicar el AND.

---

## 7. Cálculo de subred paso a paso

Método sistemático para calcular los parámetros de cualquier subred:

### Paso 1: Identificar el octeto "interesante"

El octeto interesante es donde la máscara no es ni 255 ni 0:

```
/24 = 255.255.255.0      → octeto interesante: ninguno (frontera exacta en octeto 3)
/26 = 255.255.255.192    → octeto interesante: 4.º
/20 = 255.255.240.0      → octeto interesante: 3.er
/18 = 255.255.192.0      → octeto interesante: 3.er
```

Para /24, /16, /8 (fronteras de octeto) el cálculo es trivial — el octeto completo es red o host.

### Paso 2: Calcular el "tamaño del bloque"

```
Tamaño del bloque = 256 - valor de la máscara en el octeto interesante
```

Ejemplos:
```
/26:  máscara en 4.º octeto = 192  →  256 - 192 = 64   (bloques de 64)
/28:  máscara en 4.º octeto = 240  →  256 - 240 = 16   (bloques de 16)
/20:  máscara en 3.er octeto = 240  →  256 - 240 = 16   (bloques de 16)
/27:  máscara en 4.º octeto = 224  →  256 - 224 = 32   (bloques de 32)
```

### Paso 3: Encontrar la red a la que pertenece una IP

Divide el valor del octeto interesante de la IP entre el tamaño del bloque y quédate con la parte entera:

```
IP:       192.168.1.130/26
Bloque:   64
Octeto:   130

130 ÷ 64 = 2.03...  →  parte entera = 2
Red:      2 × 64 = 128  →  192.168.1.128

Siguiente red:  (2+1) × 64 = 192  →  192.168.1.192
Broadcast:      192 - 1 = 191     →  192.168.1.191
```

Resultado:
```
Red:          192.168.1.128
Primera:      192.168.1.129
Última:       192.168.1.190
Broadcast:    192.168.1.191
Hosts:        62
```

### Otro ejemplo: 10.20.130.5/20

```
Máscara /20 = 255.255.240.0
Octeto interesante: 3.er (valor de máscara = 240)
Bloque: 256 - 240 = 16

Valor del 3.er octeto en la IP: 130
130 ÷ 16 = 8.125  →  parte entera = 8
Red en 3.er octeto: 8 × 16 = 128  →  10.20.128.0
Siguiente red: (8+1) × 16 = 144   →  10.20.144.0
Broadcast: 10.20.143.255

Red:          10.20.128.0
Primera:      10.20.128.1
Última:       10.20.143.254
Broadcast:    10.20.143.255
Hosts:        4094
```

---

## 8. ¿Están dos IPs en la misma red?

Esta es la pregunta que una máquina responde cada vez que envía un paquete. Para responderla, aplica el AND de la máscara a ambas IPs y compara:

### Ejemplo: ¿192.168.122.100 y 192.168.122.50 están en la misma /24?

```
192.168.122.100  AND  255.255.255.0  =  192.168.122.0
192.168.122.50   AND  255.255.255.0  =  192.168.122.0
                                        ─────────────
                                        iguales → MISMA RED ✓
```

### Ejemplo: ¿192.168.1.130 y 192.168.1.200 están en la misma /26?

```
Máscara /26 = 255.255.255.192

130 AND 192:   10000010 AND 11000000 = 10000000 = 128
200 AND 192:   11001000 AND 11000000 = 11000000 = 192

192.168.1.130  AND  255.255.255.192  =  192.168.1.128
192.168.1.200  AND  255.255.255.192  =  192.168.1.192
                                        ─────────────
                                        diferentes → REDES DISTINTAS ✗
```

Aunque ambas IPs "parecen" estar en `192.168.1.x`, la máscara /26 las pone en subredes diferentes (192.168.1.128/26 y 192.168.1.192/26).

### ¿Por qué importa esto para VMs?

Si configuras una red virtual con `/26` y asignas manualmente una IP a una VM que cae fuera de la subred del gateway, esa VM **no podrá comunicarse** con las demás aunque esté conectada al mismo bridge. La VM enviaría todo al gateway, pero el gateway no la considera parte de su red.

---

## 9. Subnetting: dividir una red en subredes

Subnetting es tomar una red grande y dividirla en redes más pequeñas. Cada subdivisión aumenta el CIDR en 1 (duplica el número de subredes, reduce a la mitad los hosts por subred).

### Dividir 192.168.122.0/24 en 4 subredes

```
Red original:  192.168.122.0/24  (254 hosts)

Para 4 subredes: necesitamos 2 bits más de red  →  /24 + 2 = /26

Subred 1:  192.168.122.0/26     hosts: .1   – .62    (62 hosts)
Subred 2:  192.168.122.64/26    hosts: .65  – .126   (62 hosts)
Subred 3:  192.168.122.128/26   hosts: .129 – .190   (62 hosts)
Subred 4:  192.168.122.192/26   hosts: .193 – .254   (62 hosts)
```

Visualmente:

```
192.168.122.0/24  (red completa)
├── 192.168.122.0/26    [.0 ──── .63 ]   62 hosts
├── 192.168.122.64/26   [.64 ─── .127]   62 hosts
├── 192.168.122.128/26  [.128 ── .191]   62 hosts
└── 192.168.122.192/26  [.192 ── .255]   62 hosts
```

### Aplicación en virtualización

Subnetting es útil cuando quieres aislar grupos de VMs dentro de una misma red virtual:

```
Red libvirt:  192.168.122.0/24

Subred "web":     192.168.122.0/26     VMs con nginx, apache
Subred "app":     192.168.122.64/26    VMs con backend
Subred "db":      192.168.122.128/26   VMs con postgres, redis
Subred "mgmt":    192.168.122.192/26   VMs de monitoreo, bastion
```

En la práctica con libvirt, el aislamiento real se logra con redes virtuales separadas (no subredes dentro de una misma red), pero entender subnetting es fundamental para diseñar topologías y para exámenes de certificación.

---

## 10. La herramienta ipcalc

`ipcalc` automatiza todos los cálculos que hemos visto. Es invaluable para verificación rápida.

### Instalación

```bash
# Fedora/RHEL
sudo dnf install ipcalc

# Debian/Ubuntu (hay dos versiones, ambas válidas)
sudo apt install ipcalc
```

### Uso básico

```bash
ipcalc 192.168.122.0/24
```

```
Address:    192.168.122.0        11000000.10101000.01111010. 00000000
Netmask:    255.255.255.0 = 24   11111111.11111111.11111111. 00000000
Wildcard:   0.0.0.255            00000000.00000000.00000000. 11111111
=>
Network:    192.168.122.0/24     11000000.10101000.01111010. 00000000
HostMin:    192.168.122.1        11000000.10101000.01111010. 00000001
HostMax:    192.168.122.254      11000000.10101000.01111010. 11111110
Broadcast:  192.168.122.255      11000000.10101000.01111010. 11111111
Hosts/Net:  254                   Class C, Private Internet
```

La salida incluye la representación binaria con un punto separando la parte de red y la de host — eso facilita ver exactamente dónde cae la división.

### Consultar una IP específica con su máscara

```bash
ipcalc 10.20.130.5/20
```

```
Address:    10.20.130.5          00001010.00010100.1000 0010.00000101
Netmask:    255.255.240.0 = 20   11111111.11111111.1111 0000.00000000
=>
Network:    10.20.128.0/20       00001010.00010100.1000 0000.00000000
HostMin:    10.20.128.1          00001010.00010100.1000 0000.00000001
HostMax:    10.20.143.254        00001010.00010100.1000 1111.11111110
Broadcast:  10.20.143.255        00001010.00010100.1000 1111.11111111
Hosts/Net:  4094                  Class A, Private Internet
```

### Wildcard mask

En la salida de `ipcalc` verás `Wildcard`. Es el **inverso** de la máscara de red (cada `1` se convierte en `0` y viceversa):

```
Máscara:    255.255.255.0    → 11111111.11111111.11111111.00000000
Wildcard:   0.0.0.255        → 00000000.00000000.00000000.11111111
```

Se usa en configuración de ACLs en routers Cisco y en algunas herramientas de red. Para este curso no es relevante directamente, pero la verás en la salida de `ipcalc`.

---

## 11. Máscaras en el contexto de QEMU/KVM y libvirt

### La red default: 192.168.122.0/24

Cuando instalas libvirt, la red `default` se configura así:

```xml
<network>
  <name>default</name>
  <bridge name='virbr0'/>
  <forward mode='nat'/>
  <ip address='192.168.122.1' netmask='255.255.255.0'>
    <dhcp>
      <range start='192.168.122.2' end='192.168.122.254'/>
    </dhcp>
  </ip>
</network>
```

El `netmask='255.255.255.0'` (/24) define que:
- El gateway es 192.168.122.1 (la IP del host en virbr0).
- DHCP asigna de .2 a .254 (253 IPs para VMs).
- .0 es la dirección de red, .255 es broadcast.

### Crear una red con máscara diferente

Si necesitas una red pequeña para un lab con pocas VMs:

```xml
<network>
  <name>lab-small</name>
  <bridge name='virbr1'/>
  <ip address='10.0.1.1' netmask='255.255.255.240'>
    <dhcp>
      <range start='10.0.1.2' end='10.0.1.14'/>
    </dhcp>
  </ip>
</network>
```

Aquí `/28` (255.255.255.240) da solo 14 hosts — suficiente para un lab de 3-4 VMs y mucho más aislado.

### La máscara afecta el routing de las VMs

Si una VM con IP 192.168.122.100/24 hace `ping 192.168.1.50` (la LAN del host):

```
VM piensa:
  Mi red: 192.168.122.0/24
  Destino: 192.168.1.50
  192.168.1.50 AND 255.255.255.0 = 192.168.1.0  ≠  192.168.122.0
  → No está en mi red → enviar al gateway (192.168.122.1)

Host recibe el paquete en virbr0 y lo reenvía (NAT) hacia 192.168.1.50.
```

Si por error configuras la VM con `/8` en lugar de `/24`:

```
VM piensa:
  Mi red: 192.0.0.0/8   ← mal, red enorme
  Destino: 192.168.1.50
  192.168.1.50 AND 255.0.0.0 = 192.0.0.0  ==  192.0.0.0
  → Cree que está en su red → intenta envío directo (capa 2)
  → FALLA: no hay ruta directa a 192.168.1.50 por el bridge
```

La máscara incorrecta rompe la conectividad sin que haya ningún error de configuración en la red virtual de libvirt.

---

## 12. Errores comunes

### Error 1: Confundir la IP con la dirección de red

```bash
# ip addr muestra:
# inet 192.168.1.130/26

# El alumno concluye:
# "Mi red es 192.168.1.130" ← INCORRECTO
# "Mi red es 192.168.1.0"   ← INCORRECTO (sería cierto con /24, no con /26)

# Correcto: aplicar AND
# 130 AND 192 = 128 → Mi red es 192.168.1.128/26
```

La dirección de red se obtiene aplicando la máscara, no truncando el último octeto a cero.

### Error 2: Asumir que /24 siempre significa "el último octeto es host"

Es cierto para /24, pero no para otras máscaras:

```
192.168.1.100/24  → red: 192.168.1.0    (octeto 4 = host)
192.168.1.100/26  → red: 192.168.1.64   (los 2 bits altos del octeto 4 = red)
192.168.1.100/20  → red: 192.168.0.0    (parte del octeto 3 = host)
```

### Error 3: Olvidar restar las 2 direcciones reservadas

```
/28 = 16 IPs totales

"Tengo 16 hosts disponibles" ← MAL
"Tengo 14 hosts disponibles" ← CORRECTO (16 - red - broadcast = 14)
```

### Error 4: Escribir la máscara en el formato equivocado

En XML de libvirt se usa formato decimal:
```xml
<!-- Correcto -->
<ip address='192.168.122.1' netmask='255.255.255.0'>

<!-- INCORRECTO — no acepta CIDR -->
<ip address='192.168.122.1' netmask='/24'>
```

En `ip addr add` se usa CIDR:
```bash
# Correcto
sudo ip addr add 192.168.122.1/24 dev virbr0

# No funciona con máscara decimal
sudo ip addr add 192.168.122.1 netmask 255.255.255.0 dev virbr0  # ← eso es ifconfig
```

### Error 5: Asignar la dirección de red o de broadcast a un host

```bash
# INCORRECTO: .0 es la dirección de red en /24, no asignable
sudo ip addr add 192.168.1.0/24 dev eth0

# INCORRECTO: .255 es broadcast en /24
sudo ip addr add 192.168.1.255/24 dev eth0

# CORRECTO: cualquier IP entre .1 y .254
sudo ip addr add 192.168.1.50/24 dev eth0
```

Linux técnicamente permite asignar la dirección de red a una interfaz, pero causa comportamiento impredecible.

---

## 13. Cheatsheet

```text
╔══════════════════════════════════════════════════════════════════════╗
║                  MÁSCARA DE RED Y CIDR CHEATSHEET                   ║
╠══════════════════════════════════════════════════════════════════════╣
║                                                                    ║
║  CONCEPTO CLAVE                                                    ║
║  Máscara:  bits en 1 = parte de RED,  bits en 0 = parte de HOST    ║
║  CIDR /N:  N bits de red,  32-N bits de host                       ║
║  Dirección de red:  IP AND máscara                                  ║
║                                                                    ║
╠══════════════════════════════════════════════════════════════════════╣
║                                                                    ║
║  FÓRMULA RÁPIDA                                                    ║
║  IPs totales  = 2^(32 - CIDR)                                      ║
║  Hosts usables = 2^(32 - CIDR) - 2                                 ║
║  Tamaño bloque = 256 - valor máscara en octeto interesante          ║
║                                                                    ║
╠══════════════════════════════════════════════════════════════════════╣
║                                                                    ║
║  MÁSCARAS COMUNES                                                  ║
║  /8   255.0.0.0        16M hosts     Red privada 10.x.x.x         ║
║  /16  255.255.0.0      65K hosts     Red privada 172.16.x.x       ║
║  /24  255.255.255.0    254 hosts     Red LAN estándar ★            ║
║  /26  255.255.255.192  62 hosts      Oficina / lab pequeño         ║
║  /28  255.255.255.240  14 hosts      Lab mínimo de VMs            ║
║  /30  255.255.255.252  2 hosts       Enlace punto a punto          ║
║  /32  255.255.255.255  1 IP exacta   Host route / loopback         ║
║                                                                    ║
╠══════════════════════════════════════════════════════════════════════╣
║                                                                    ║
║  DIRECCIONES EN CADA SUBRED                                        ║
║  Primera IP del rango = dirección de red (no asignable)             ║
║  Primera + 1 = primera IP usable (típicamente el gateway)           ║
║  Última - 1 = última IP usable                                     ║
║  Última IP del rango = broadcast (no asignable)                     ║
║                                                                    ║
╠══════════════════════════════════════════════════════════════════════╣
║                                                                    ║
║  ¿MISMA RED?                                                       ║
║  (IP_A AND máscara) == (IP_B AND máscara)  →  misma red            ║
║  Resultado diferente → redes distintas → necesitan router           ║
║                                                                    ║
╠══════════════════════════════════════════════════════════════════════╣
║                                                                    ║
║  HERRAMIENTAS                                                      ║
║  ipcalc 192.168.1.0/24      Calcular parámetros de subred          ║
║  ipcalc 10.20.130.5/20      Encontrar la red de una IP             ║
║  ip addr                     Ver IP/máscara de tus interfaces      ║
║                                                                    ║
╠══════════════════════════════════════════════════════════════════════╣
║                                                                    ║
║  EN LIBVIRT                                                        ║
║  Red default: 192.168.122.0/24  (netmask='255.255.255.0')         ║
║  Gateway: 192.168.122.1  (virbr0 en el host)                      ║
║  DHCP: 192.168.122.2 – 192.168.122.254                            ║
║  XML usa máscara decimal, ip usa CIDR — no mezclar                 ║
║                                                                    ║
╚══════════════════════════════════════════════════════════════════════╝
```

---

## 14. Ejercicios

### Ejercicio 1: Cálculo manual de subredes

Sin usar `ipcalc`, calcula para cada red: dirección de red, primera IP usable, última IP usable, broadcast, y número de hosts:

```
a) 192.168.1.200/26
b) 10.0.50.30/20
c) 172.16.100.1/28
d) 192.168.122.0/25
```

Después verifica con `ipcalc`. Si alguno no coincide, revisa tu proceso en el octeto interesante.

**Preguntas reflexivas:**
1. ¿En cuál de las redes anteriores cabe un lab de 100 VMs? ¿En cuáles no?
2. Si libvirt usara /25 en lugar de /24 para la red default, ¿cuántas VMs menos podrías tener?
3. La red (d) va de .0 a .127. ¿Qué red cubriría .128 a .255 con la misma máscara?

### Ejercicio 2: ¿Misma red o redes distintas?

Determina si cada par de IPs está en la misma subred. Muestra el AND para justificar tu respuesta:

```
a) 192.168.122.10  y  192.168.122.200    con máscara /24
b) 192.168.122.10  y  192.168.122.200    con máscara /25
c) 10.0.1.5        y  10.0.2.5           con máscara /24
d) 10.0.1.5        y  10.0.2.5           con máscara /20
e) 172.16.50.100   y  172.16.55.200      con máscara /20
```

**Preguntas reflexivas:**
1. Observa cómo (a) y (b) tienen las mismas IPs pero máscaras distintas. ¿Qué concluyes sobre la relación entre máscara y pertenencia a la red?
2. Si dos VMs con las IPs de (c) están conectadas al mismo bridge pero con máscara /24, ¿podrán comunicarse directamente? ¿Qué pasaría si una de ellas cambia a /23?
3. En el caso (e), ¿la máscara /20 es suficiente para cubrir todo el rango 172.16.48.0 – 172.16.63.255? Verifica con `ipcalc`.

### Ejercicio 3: Diseño de red para un lab

Tienes la red 10.10.0.0/16 y necesitas crear 3 redes virtuales en libvirt para un laboratorio:

- **Red "web"**: necesita alojar hasta 30 VMs
- **Red "db"**: necesita alojar hasta 5 VMs
- **Red "mgmt"**: necesita alojar hasta 3 VMs

Para cada red:
1. Elige la máscara CIDR más ajustada (la que desperdicie menos direcciones).
2. Asigna un rango de la red 10.10.0.0/16 (sin solapar).
3. Escribe la línea XML de libvirt: `<ip address='X.X.X.X' netmask='X.X.X.X'>` con el rango DHCP.

**Preguntas reflexivas:**
1. ¿Qué máscara elegiste para "web"? ¿/26 (62 hosts) o /25 (126 hosts)? ¿Por qué?
2. Si en el futuro la red "db" necesita 20 VMs, ¿puedes ampliarla sin cambiar las otras? ¿Cómo influye la planificación inicial?
3. ¿Cuántas subredes /28 cabrían en tu /16 total? ¿Es un recurso escaso o abundante?

---

> **Nota**: la máscara de red es el concepto más importante de networking para virtualización. Define qué tráfico va directo y qué tráfico pasa por el gateway, cuántos hosts caben en una red virtual, y cómo segmentar un laboratorio en subredes aisladas. Si dominas el AND binario y la fórmula `2^(32-N) - 2`, puedes calcular cualquier subred mentalmente — y más importante, puedes diagnosticar por qué una VM "no tiene red" cuando la máscara está mal configurada.
