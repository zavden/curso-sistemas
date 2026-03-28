# T02 — Máscara de Red y CIDR

> **Objetivo:** Entender cómo la máscara de red divide una IP en parte de red y parte de host, dominar la notación CIDR, calcular subredes manualmente con el método del octeto interesante, y aplicar estos conceptos a la configuración de redes virtuales en libvirt.

## Erratas detectadas en el material original

| Archivo | Línea | Error | Corrección |
|---------|-------|-------|------------|
| `README.md` | 136 | Respuesta del ejercicio indica que el broadcast de `10.0.0.0/16` es `10.255.255.255` | El broadcast correcto de `10.0.0.0/16` es `10.0.255.255`. `10.255.255.255` sería el broadcast de `10.0.0.0/8` |

---

## 1. El problema: ¿cómo sabe un paquete si su destino está en la misma red?

Cuando una máquina quiere enviar un paquete a otra IP, necesita decidir:

- **¿Está el destino en mi misma red local?** → lo envío directamente (capa 2, Ethernet).
- **¿Está en otra red?** → lo envío a mi gateway (router) para que lo encamine.

Esa decisión es imposible sin saber dónde termina "mi red" y empieza "otra red". Eso es exactamente lo que define la **máscara de red**: el límite entre la parte de red y la parte de host.

```
Tu VM tiene:       192.168.122.100/24
Quiere hablar con: 192.168.122.50   → misma red → envío directo
Quiere hablar con: 8.8.8.8          → otra red   → envío al gateway (192.168.122.1)
```

¿Cómo determinó la VM que 192.168.122.50 está en su red y 8.8.8.8 no? Aplicó su máscara `/24` a ambas direcciones.

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

- Bits en `1` de la máscara → **porción de red** (identifica a qué red pertenece).
- Bits en `0` de la máscara → **porción de host** (identifica al dispositivo dentro de esa red).

En binario:

```
IP:       11000000 . 10101000 . 00000001 . 01100100
Máscara:  11111111 . 11111111 . 11111111 . 00000000
          ├── RED (24 bits de 1s) ──────┤├ HOST ──┤
```

### Regla fundamental

Los bits `1` van siempre a la izquierda, agrupados. Los bits `0` van siempre a la derecha. **Nunca se mezclan**:

```
Válido:   11111111.11111111.11111100.00000000  (/22)
Válido:   11111111.11111111.11111111.11000000  (/26)
Inválido: 11111111.10101010.11111111.00000000  ← 1s y 0s intercalados
```

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

Resultado: **192.168.1.0** es la dirección de red. La máscara preservó los bits de red y puso a cero los bits de host.

### Ejemplo con máscara no trivial: 10.20.30.40/20

```
IP:         00001010 . 00010100 . 00011110 . 00101000   (10.20.30.40)
Máscara:    11111111 . 11111111 . 11110000 . 00000000   (255.255.240.0)
            ─────────────────────────────────────────
AND:        00001010 . 00010100 . 00010000 . 00000000   (10.20.16.0)
```

La red es **10.20.16.0/20** — no 10.20.30.0. Esto no es intuitivo mirando solo los decimales, pero el AND binario no miente.

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

| Clase | Rango | Máscara fija | Hosts por red |
|-------|-------|-------------|---------------|
| A | 1.0.0.0 – 126.0.0.0 | /8 | ~16 millones |
| B | 128.0.0.0 – 191.255.0.0 | /16 | ~65,000 |
| C | 192.0.0.0 – 223.255.255.0 | /24 | 254 |

Este sistema era enormemente ineficiente. CIDR eliminó las clases y permite máscaras de cualquier longitud. Hoy la terminología sobrevive coloquialmente ("clase C" = /24), pero en la práctica se usa exclusivamente CIDR.

### De máscara decimal a CIDR y viceversa

```
255.255.255.0   → 24 unos → /24
255.255.240.0   → 20 unos → /20
255.255.255.192 → 26 unos → /26

/27 → 27 unos + 5 ceros
    → 11111111.11111111.11111111.11100000
    = 255.255.255.224
```

---

## 5. Tabla de referencia: las máscaras más usadas

```
CIDR    Máscara decimal      IPs totales    Hosts usables    Uso típico
────    ───────────────      ───────────    ─────────────    ──────────
/32     255.255.255.255      1              0 (1 IP exacta)  Host route, loopback
/31     255.255.255.254      2              2 (punto a punto) Links punto a punto (RFC 3021)
/30     255.255.255.252      4              2                 Links entre routers
/29     255.255.255.248      8              6                 Micro-red
/28     255.255.255.240      16             14                Red muy pequeña / lab mínimo
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

/24:  2^8  = 256 IPs,   254 hosts
/26:  2^6  = 64 IPs,    62 hosts
/28:  2^4  = 16 IPs,    14 hosts
/20:  2^12 = 4096 IPs,  4094 hosts
```

¿Por qué se restan 2? Porque en cada subred hay dos direcciones reservadas:
1. La **primera IP** del rango es la **dirección de red** (no asignable).
2. La **última IP** del rango es la **dirección de broadcast** (no asignable).

Excepción: `/31` se usa para enlaces punto a punto (RFC 3021) — no necesita broadcast ni dirección de red.

---

## 6. Dirección de red, broadcast y hosts usables

Para cualquier combinación IP/máscara, se derivan cuatro valores fundamentales:

### Ejemplo: 192.168.122.0/24 (red default de libvirt)

```
Dirección de red:        192.168.122.0       ← identifica la red
Primera IP usable:       192.168.122.1       ← típicamente el gateway (virbr0)
Última IP usable:        192.168.122.254     ← último host asignable
Dirección de broadcast:  192.168.122.255     ← enviar a todos en la red
Hosts usables:           254                 ← 256 - 2
```

### Ejemplo: 10.0.1.0/28

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

```
IP:       172.16.50.100
Máscara:  /20 = 255.255.240.0

Tercer octeto (donde la máscara no es ni 255 ni 0):
  50 en binario:    00110010
  240 en binario:   11110000
  AND:              00110000 = 48

Red:          172.16.48.0
Primera:      172.16.48.1
Última:       172.16.63.254
Broadcast:    172.16.63.255
Hosts:        4094
```

La dirección de red **no es** 172.16.50.0 — ese es un error frecuente al mirar solo los decimales sin aplicar el AND.

---

## 7. Cálculo de subred paso a paso (método del octeto interesante)

### Paso 1: Identificar el octeto "interesante"

El octeto interesante es donde la máscara no es ni 255 ni 0:

```
/24 = 255.255.255.0      → frontera exacta (trivial)
/26 = 255.255.255.192    → octeto interesante: 4.º
/20 = 255.255.240.0      → octeto interesante: 3.er
/18 = 255.255.192.0      → octeto interesante: 3.er
```

### Paso 2: Calcular el "tamaño del bloque"

```
Tamaño del bloque = 256 - valor de la máscara en el octeto interesante

/26:  256 - 192 = 64   (bloques de 64)
/28:  256 - 240 = 16   (bloques de 16)
/20:  256 - 240 = 16   (bloques de 16 en el 3.er octeto)
/27:  256 - 224 = 32   (bloques de 32)
```

### Paso 3: Encontrar la red de una IP

Divide el valor del octeto interesante entre el tamaño del bloque y quédate con la parte entera:

```
IP:       192.168.1.130/26
Bloque:   64
Octeto:   130

130 ÷ 64 = 2.03...  →  parte entera = 2
Red:      2 × 64 = 128  →  192.168.1.128

Siguiente red:  (2+1) × 64 = 192  →  192.168.1.192
Broadcast:      192 - 1 = 191     →  192.168.1.191

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

Aplica el AND de la máscara a ambas IPs y compara:

### Ejemplo: misma red con /24

```
192.168.122.100  AND  255.255.255.0  =  192.168.122.0
192.168.122.50   AND  255.255.255.0  =  192.168.122.0
                                        ─────────────
                                        iguales → MISMA RED ✓
```

### Ejemplo: redes distintas con /26

```
Máscara /26 = 255.255.255.192

130 AND 192:   10000010 AND 11000000 = 10000000 = 128
200 AND 192:   11001000 AND 11000000 = 11000000 = 192

192.168.1.130  →  red 192.168.1.128
192.168.1.200  →  red 192.168.1.192
                  diferentes → REDES DISTINTAS ✗
```

Aunque ambas IPs "parecen" estar en `192.168.1.x`, la máscara /26 las pone en subredes diferentes.

### ¿Por qué importa para VMs?

Si configuras una red virtual con `/26` y asignas manualmente una IP que cae fuera de la subred del gateway, esa VM **no podrá comunicarse** con las demás — la máscara dice que no están en la misma red.

---

## 9. Subnetting: dividir una red en subredes

Subnetting es tomar una red grande y dividirla en redes más pequeñas. Cada bit extra de red duplica el número de subredes y reduce a la mitad los hosts por subred.

### Dividir 192.168.122.0/24 en 4 subredes

```
Red original:  192.168.122.0/24  (254 hosts)
Para 4 subredes: 2 bits más de red  →  /24 + 2 = /26

192.168.122.0/24  (red completa)
├── 192.168.122.0/26    [.0 ──── .63 ]   62 hosts
├── 192.168.122.64/26   [.64 ─── .127]   62 hosts
├── 192.168.122.128/26  [.128 ── .191]   62 hosts
└── 192.168.122.192/26  [.192 ── .255]   62 hosts
```

### Aplicación en virtualización

```
Red libvirt:  192.168.122.0/24

Subred "web":     192.168.122.0/26     VMs con nginx, apache
Subred "app":     192.168.122.64/26    VMs con backend
Subred "db":      192.168.122.128/26   VMs con postgres, redis
Subred "mgmt":    192.168.122.192/26   VMs de monitoreo, bastion
```

En la práctica con libvirt, el aislamiento real se logra con redes virtuales separadas, pero entender subnetting es fundamental para diseñar topologías.

---

## 10. La herramienta ipcalc

`ipcalc` automatiza todos los cálculos manuales. Invaluable para verificación rápida.

```bash
# Instalación
sudo dnf install ipcalc       # Fedora/RHEL
sudo apt install ipcalc       # Debian/Ubuntu
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

La salida incluye la representación binaria con un espacio separando la parte de red y la de host.

### Consultar una IP específica

```bash
ipcalc 10.20.130.5/20
```

```
Address:    10.20.130.5          00001010.00010100.1000 0010.00000101
Netmask:    255.255.240.0 = 20   11111111.11111111.1111 0000.00000000
=>
Network:    10.20.128.0/20       00001010.00010100.1000 0000.00000000
HostMin:    10.20.128.1
HostMax:    10.20.143.254
Broadcast:  10.20.143.255
Hosts/Net:  4094
```

### Wildcard mask

El **inverso** de la máscara de red (cada `1` ↔ `0`). Se usa en ACLs de routers Cisco. Para este curso no es directamente relevante, pero aparece en la salida de `ipcalc`.

---

## 11. Máscaras en el contexto de QEMU/KVM y libvirt

### La red default: 192.168.122.0/24

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

`netmask='255.255.255.0'` (/24) define: gateway .1, DHCP .2–.254 (253 IPs), .0 = red, .255 = broadcast.

### Crear una red con máscara diferente

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

`/28` (255.255.255.240) da solo 14 hosts — suficiente para un lab de 3-4 VMs.

### La máscara afecta el routing de las VMs

```
VM con 192.168.122.100/24 hace ping a 192.168.1.50:
  Mi red: 192.168.122.0/24
  192.168.1.50 AND 255.255.255.0 = 192.168.1.0 ≠ 192.168.122.0
  → Otra red → enviar al gateway (192.168.122.1) → NAT → llega

Si por error la VM tiene /8 en lugar de /24:
  Mi red: 192.0.0.0/8
  192.168.1.50 AND 255.0.0.0 = 192.0.0.0 == 192.0.0.0
  → Cree que está en su red → intenta envío directo → FALLA
```

La máscara incorrecta rompe la conectividad sin ningún error en la red virtual.

### Formatos: XML vs ip

```bash
# XML de libvirt usa máscara decimal:
netmask='255.255.255.0'

# ip addr add usa CIDR:
sudo ip addr add 192.168.122.1/24 dev virbr0

# No mezclar formatos
```

---

## 12. Errores comunes

### Error 1: Confundir la IP con la dirección de red

```
ip addr muestra: inet 192.168.1.130/26

"Mi red es 192.168.1.0"   ← INCORRECTO (sería cierto con /24, no con /26)
Correcto: 130 AND 192 = 128 → Mi red es 192.168.1.128/26
```

### Error 2: Asumir que /24 siempre significa "último octeto = host"

```
192.168.1.100/24  → red: 192.168.1.0    (octeto 4 completo = host)
192.168.1.100/26  → red: 192.168.1.64   (2 bits altos del octeto 4 = red)
192.168.1.100/20  → red: 192.168.0.0    (parte del octeto 3 = host)
```

### Error 3: Olvidar restar las 2 direcciones reservadas

```
/28 = 16 IPs totales → 14 hosts usables (no 16)
```

### Error 4: Formato equivocado (XML vs CIDR)

```xml
<!-- Correcto en XML de libvirt -->
<ip address='192.168.122.1' netmask='255.255.255.0'>

<!-- INCORRECTO — XML no acepta CIDR -->
<ip address='192.168.122.1' netmask='/24'>
```

### Error 5: Asignar la dirección de red o de broadcast a un host

```bash
# INCORRECTO: .0 es la dirección de red en /24
sudo ip addr add 192.168.1.0/24 dev eth0

# INCORRECTO: .255 es broadcast en /24
sudo ip addr add 192.168.1.255/24 dev eth0

# CORRECTO: cualquier IP entre .1 y .254
sudo ip addr add 192.168.1.50/24 dev eth0
```

---

## Cheatsheet

```text
╔══════════════════════════════════════════════════════════════════════╗
║                  MÁSCARA DE RED Y CIDR CHEATSHEET                   ║
╠══════════════════════════════════════════════════════════════════════╣
║                                                                    ║
║  CONCEPTO CLAVE                                                    ║
║  Máscara:  bits en 1 = RED,  bits en 0 = HOST                     ║
║  CIDR /N:  N bits de red,  32-N bits de host                       ║
║  Dirección de red:  IP AND máscara                                  ║
║                                                                    ║
╠══════════════════════════════════════════════════════════════════════╣
║                                                                    ║
║  FÓRMULA RÁPIDA                                                    ║
║  IPs totales   = 2^(32 - CIDR)                                     ║
║  Hosts usables = 2^(32 - CIDR) - 2                                 ║
║  Tamaño bloque = 256 - máscara en octeto interesante                ║
║                                                                    ║
╠══════════════════════════════════════════════════════════════════════╣
║                                                                    ║
║  MÁSCARAS COMUNES                                                  ║
║  /8   255.0.0.0        16M hosts     Red 10.x.x.x                 ║
║  /16  255.255.0.0      65K hosts     Red 172.16.x.x               ║
║  /24  255.255.255.0    254 hosts     LAN estándar ★                ║
║  /26  255.255.255.192  62 hosts      Lab / oficina                 ║
║  /28  255.255.255.240  14 hosts      Lab mínimo                    ║
║  /30  255.255.255.252  2 hosts       Punto a punto                 ║
║                                                                    ║
╠══════════════════════════════════════════════════════════════════════╣
║                                                                    ║
║  ¿MISMA RED?                                                       ║
║  (IP_A AND máscara) == (IP_B AND máscara)  →  misma red            ║
║                                                                    ║
╠══════════════════════════════════════════════════════════════════════╣
║                                                                    ║
║  EN LIBVIRT                                                        ║
║  XML: netmask='255.255.255.0'   (formato decimal)                  ║
║  ip:  192.168.122.1/24          (formato CIDR)                     ║
║  No mezclar formatos                                               ║
║                                                                    ║
╚══════════════════════════════════════════════════════════════════════╝
```

---

## Ejercicios

### Ejercicio 1 — Fórmula rápida: hosts por máscara

Calcula mentalmente cuántos hosts usables tiene cada CIDR:

```
a) /24
b) /28
c) /26
d) /20
e) /30
```

<details><summary>Predicción</summary>

Fórmula: hosts usables = 2^(32 - CIDR) - 2

a) `/24`: 2^8 - 2 = 256 - 2 = **254 hosts**
b) `/28`: 2^4 - 2 = 16 - 2 = **14 hosts**
c) `/26`: 2^6 - 2 = 64 - 2 = **62 hosts**
d) `/20`: 2^12 - 2 = 4096 - 2 = **4094 hosts**
e) `/30`: 2^2 - 2 = 4 - 2 = **2 hosts** (típico para enlaces punto a punto entre routers)

</details>

### Ejercicio 2 — Cálculo manual de subred (frontera de octeto)

Sin `ipcalc`, calcula dirección de red, primera usable, última usable, broadcast y hosts:

```
a) 192.168.122.0/24
b) 10.0.0.0/8
c) 172.16.0.0/16
```

<details><summary>Predicción</summary>

Estos son casos triviales (la máscara cae en frontera de octeto):

a) `192.168.122.0/24`:
- Red: 192.168.122.0
- Primera: 192.168.122.1
- Última: 192.168.122.254
- Broadcast: 192.168.122.255
- Hosts: 254

b) `10.0.0.0/8`:
- Red: 10.0.0.0
- Primera: 10.0.0.1
- Última: 10.255.255.254
- Broadcast: 10.255.255.255
- Hosts: 16,777,214

c) `172.16.0.0/16`:
- Red: 172.16.0.0
- Primera: 172.16.0.1
- Última: 172.16.255.254
- Broadcast: 172.16.255.255
- Hosts: 65,534

</details>

### Ejercicio 3 — Cálculo manual de subred (octeto interesante)

Sin `ipcalc`, calcula los mismos 5 valores para cada red:

```
a) 192.168.1.200/26
b) 10.0.50.30/20
c) 172.16.100.1/28
d) 192.168.122.0/25
```

Verifica después con `ipcalc`.

<details><summary>Predicción</summary>

a) `192.168.1.200/26`:
- Máscara 4.º octeto: 192. Bloque: 256 - 192 = 64.
- 200 ÷ 64 = 3.125 → parte entera 3. Red: 3 × 64 = 192.
- Siguiente red: 4 × 64 = 256 (fuera del octeto).
- **Red: 192.168.1.192**, Primera: .193, Última: .254, **Broadcast: 192.168.1.255**, Hosts: 62

b) `10.0.50.30/20`:
- Máscara 3.er octeto: 240. Bloque: 256 - 240 = 16.
- 50 ÷ 16 = 3.125 → parte entera 3. Red 3.er octeto: 3 × 16 = 48.
- Siguiente red: 4 × 16 = 64.
- **Red: 10.0.48.0**, Primera: 10.0.48.1, Última: 10.0.63.254, **Broadcast: 10.0.63.255**, Hosts: 4094

c) `172.16.100.1/28`:
- Máscara 4.º octeto: 240. Bloque: 256 - 240 = 16.
- 1 ÷ 16 = 0.0625 → parte entera 0. Red: 0 × 16 = 0.
- Siguiente red: 1 × 16 = 16.
- **Red: 172.16.100.0**, Primera: .1, Última: .14, **Broadcast: 172.16.100.15**, Hosts: 14

d) `192.168.122.0/25`:
- Máscara 4.º octeto: 128. Bloque: 256 - 128 = 128.
- 0 ÷ 128 = 0. Red: 0.
- Siguiente red: 128.
- **Red: 192.168.122.0**, Primera: .1, Última: .126, **Broadcast: 192.168.122.127**, Hosts: 126

</details>

### Ejercicio 4 — ¿Misma red o redes distintas?

Determina si cada par está en la misma subred. Justifica con AND:

```
a) 192.168.122.10  y  192.168.122.200    con /24
b) 192.168.122.10  y  192.168.122.200    con /25
c) 10.0.1.5        y  10.0.2.5           con /24
d) 10.0.1.5        y  10.0.2.5           con /20
```

<details><summary>Predicción</summary>

a) `/24` (255.255.255.0):
- 192.168.122.10 AND 255.255.255.0 = 192.168.122.0
- 192.168.122.200 AND 255.255.255.0 = 192.168.122.0
- **MISMA RED** ✓ (ambas en 192.168.122.0/24)

b) `/25` (255.255.255.128):
- 10 AND 128 = 0 → red 192.168.122.0
- 200 AND 128 = 128 → red 192.168.122.128
- **REDES DISTINTAS** ✗ (.10 está en la primera mitad, .200 en la segunda)

c) `/24` (255.255.255.0):
- 10.0.1.5 → red 10.0.1.0
- 10.0.2.5 → red 10.0.2.0
- **REDES DISTINTAS** ✗ (el tercer octeto difiere)

d) `/20` (255.255.240.0):
- 1 AND 240 = 0 → red 10.0.0.0
- 2 AND 240 = 0 → red 10.0.0.0
- **MISMA RED** ✓ (la /20 abarca octetos 3.er 0-15, ambos caen dentro)

Observación clave: las mismas IPs pueden estar en la misma red o en redes distintas dependiendo de la máscara. Casos (a) y (b) lo demuestran con las mismas IPs.

</details>

### Ejercicio 5 — Subnetting: dividir una red

Divide `192.168.122.0/24` en:

a) 2 subredes iguales
b) 4 subredes iguales
c) 8 subredes iguales

Para cada caso, indica el nuevo CIDR y lista las subredes con sus rangos.

<details><summary>Predicción</summary>

a) **2 subredes** → 1 bit extra → /24 + 1 = **/25** (126 hosts cada una):
```
192.168.122.0/25    [.0 ──── .127]
192.168.122.128/25  [.128 ── .255]
```

b) **4 subredes** → 2 bits extra → /24 + 2 = **/26** (62 hosts cada una):
```
192.168.122.0/26    [.0 ──── .63 ]
192.168.122.64/26   [.64 ─── .127]
192.168.122.128/26  [.128 ── .191]
192.168.122.192/26  [.192 ── .255]
```

c) **8 subredes** → 3 bits extra → /24 + 3 = **/27** (30 hosts cada una):
```
192.168.122.0/27    [.0 ──── .31 ]
192.168.122.32/27   [.32 ─── .63 ]
192.168.122.64/27   [.64 ─── .95 ]
192.168.122.96/27   [.96 ─── .127]
192.168.122.128/27  [.128 ── .159]
192.168.122.160/27  [.160 ── .191]
192.168.122.192/27  [.192 ── .223]
192.168.122.224/27  [.224 ── .255]
```

Patrón: cada bit extra duplica subredes y reduce hosts a la mitad.

</details>

### Ejercicio 6 — Verificar con ipcalc

```bash
# Ejecuta y analiza la salida:
ipcalc 192.168.1.130/26
ipcalc 10.20.130.5/20
ipcalc 172.16.50.100/20
```

Preguntas:
1. ¿La dirección de red de 192.168.1.130/26 es .0 o .128?
2. ¿Cuántos hosts tiene 10.20.130.5/20?
3. ¿172.16.50.100/20 y 172.16.55.200/20 están en la misma red?

<details><summary>Predicción</summary>

1. `ipcalc 192.168.1.130/26`:
   - Red: **192.168.1.128** (no .0). Bloque de 64: 130 cae en el bloque [128-191].
   - Rango: .129 – .190, broadcast: .191, hosts: 62.

2. `ipcalc 10.20.130.5/20`:
   - Red: 10.20.128.0. Hosts: **4094**.
   - 130 AND 240 = 128. Rango: 10.20.128.1 – 10.20.143.254.

3. `ipcalc 172.16.50.100/20` y `172.16.55.200/20`:
   - 50 AND 240 = 48 → red 172.16.48.0
   - 55 AND 240 = 48 → red 172.16.48.0
   - **Sí, misma red** (172.16.48.0/20 abarca .48.0 a .63.255).

</details>

### Ejercicio 7 — Máscara incorrecta en una VM

Una VM tiene `192.168.122.100` con máscara `/8` (en lugar de `/24`). Analiza qué pasa cuando intenta:

1. `ping 192.168.122.1` (el gateway)
2. `ping 192.168.1.50` (host en la LAN)
3. `ping 8.8.8.8` (internet)

<details><summary>Predicción</summary>

Con `/8`, la VM cree que su red es `192.0.0.0/8` (todo lo que empiece con 192).

1. `ping 192.168.122.1`:
   - 192.168.122.1 AND 255.0.0.0 = 192.0.0.0 == su red
   - La VM intenta envío directo (ARP) → **funciona** porque 192.168.122.1 está realmente en el mismo bridge. ARP lo resuelve.

2. `ping 192.168.1.50`:
   - 192.168.1.50 AND 255.0.0.0 = 192.0.0.0 == su red
   - La VM cree que está en su red → intenta envío directo (ARP en el bridge)
   - **FALLA**: 192.168.1.50 no está en el bridge, nadie responde al ARP.
   - Con /24, habría ido al gateway (NAT) y habría funcionado.

3. `ping 8.8.8.8`:
   - 8.0.0.0 ≠ 192.0.0.0 → otra red → envía al gateway
   - **Funciona** (porque sí usa el gateway correctamente para destinos fuera de 192.x.x.x).

Moraleja: la máscara incorrecta no rompe todo, sino el routing a destinos que la VM erróneamente cree que están en su red local.

</details>

### Ejercicio 8 — Diseño de red para un lab

Tienes `10.10.0.0/16` y necesitas 3 redes virtuales en libvirt:

- **"web"**: hasta 30 VMs
- **"db"**: hasta 5 VMs
- **"mgmt"**: hasta 3 VMs

Para cada una, elige la máscara CIDR más ajustada y escribe el XML de `<ip>`.

<details><summary>Predicción</summary>

Elegir la máscara que cubra las VMs + gateway + algo de margen:

- **web (30 VMs)**: necesita ≥32 IPs → **/27** (30 hosts usables, justo). O **/26** (62 hosts) si quieres margen.
  - Con /27: 30 hosts exactos. Con gateway + 30 VMs = 31 → cabe justo.
  - Opción conservadora: /26 para tener margen.

- **db (5 VMs)**: necesita ≥7 IPs → **/29** (6 hosts usables). O **/28** (14 hosts) con margen.

- **mgmt (3 VMs)**: necesita ≥5 IPs → **/29** (6 hosts usables, suficiente).

XML sugerido (sin solapar, dentro de 10.10.0.0/16):

```xml
<!-- web: /26, 62 hosts -->
<ip address='10.10.1.1' netmask='255.255.255.192'>
  <dhcp><range start='10.10.1.2' end='10.10.1.62'/></dhcp>
</ip>

<!-- db: /28, 14 hosts -->
<ip address='10.10.2.1' netmask='255.255.255.240'>
  <dhcp><range start='10.10.2.2' end='10.10.2.14'/></dhcp>
</ip>

<!-- mgmt: /29, 6 hosts -->
<ip address='10.10.3.1' netmask='255.255.255.248'>
  <dhcp><range start='10.10.3.2' end='10.10.3.6'/></dhcp>
</ip>
```

Cuántas subredes /28 caben en /16: 2^(28-16) = 2^12 = 4096 subredes. Recurso abundante.

</details>

### Ejercicio 9 — Conversión CIDR ↔ máscara decimal

Convierte sin herramientas:

```
De CIDR a decimal:
a) /22
b) /27
c) /19

De decimal a CIDR:
d) 255.255.255.128
e) 255.255.248.0
f) 255.255.255.252
```

<details><summary>Predicción</summary>

**De CIDR a decimal:**

a) `/22`: 22 unos + 10 ceros
   - 11111111.11111111.11111100.00000000
   - = **255.255.252.0**

b) `/27`: 27 unos + 5 ceros
   - 11111111.11111111.11111111.11100000
   - = **255.255.255.224**

c) `/19`: 19 unos + 13 ceros
   - 11111111.11111111.11100000.00000000
   - = **255.255.224.0**

**De decimal a CIDR:**

d) `255.255.255.128`:
   - 128 = 10000000 → 1 uno en el 4.º octeto
   - 8+8+8+1 = **17** → **/25**.

   Espera, 128 = 10000000, eso es 1 bit. 24+1 = /25. **Correcto: /25**

e) `255.255.248.0`:
   - 248 = 11111000 → 5 unos en el 3.er octeto
   - 8+8+5 = **21** → **/21**

f) `255.255.255.252`:
   - 252 = 11111100 → 6 unos en el 4.º octeto
   - 8+8+8+6 = **30** → **/30**

</details>

### Ejercicio 10 — Escenario integrado: diagnosticar conectividad

Tienes esta configuración:

```
Host:   virbr0 = 192.168.122.1/24
VM-A:   eth0   = 192.168.122.50/24   (DHCP)
VM-B:   eth0   = 192.168.122.200/26  (manual, error de máscara)
```

VM-B fue configurada manualmente con /26 en lugar de /24.

1. ¿VM-A puede hacer ping a VM-B?
2. ¿VM-B puede hacer ping a VM-A?
3. ¿VM-B puede hacer ping al gateway (192.168.122.1)?
4. ¿VM-B puede hacer ping a internet (8.8.8.8)?

<details><summary>Predicción</summary>

El bridge (virbr0) opera en capa 2, así que ambas VMs están conectadas al mismo "cable virtual". Pero la máscara afecta las decisiones de routing de capa 3.

**VM-B cree que su red es 192.168.122.192/26** (200 AND 192 = 192). Su rango de "misma red" es .192–.255.

1. **VM-A → VM-B**: VM-A tiene /24, cree que todo 192.168.122.x está en su red. 192.168.122.200 está en su rango → envío directo → ARP → **funciona** ✓

2. **VM-B → VM-A**: VM-B tiene /26, su red es .192–.255. VM-A es .50 → .50 AND 192 = 0 ≠ 192 → VM-B cree que .50 está en otra red → envía al gateway (.1). Pero .1 AND 192 = 0 ≠ 192 → ¡el gateway tampoco está en la "red" de VM-B! → VM-B no tiene ruta para llegar a .50 → **FALLA** ✗

3. **VM-B → gateway (.1)**: Misma lógica que arriba. .1 AND 192 = 0 ≠ 192 → VM-B cree que el gateway está en otra red → necesita un gateway para llegar al gateway → **FALLA** ✗ (no tiene ruta)

4. **VM-B → 8.8.8.8**: No puede llegar al gateway → **FALLA** ✗

**Resultado**: VM-B solo puede comunicarse con IPs en .192–.255. Perdió conectividad con el gateway y con todo lo demás. Solución: corregir la máscara a /24 (`ip addr del ... && ip addr add 192.168.122.200/24 dev eth0`).

Este es el error más común al configurar IPs estáticas en VMs: la máscara incorrecta aísla la VM silenciosamente.

</details>
