# Subnetting: Máscara de Red, Cálculo de Subredes, Broadcast y Hosts

## Índice

1. [Qué es subnetting](#1-qué-es-subnetting)
2. [La mecánica del subnetting](#2-la-mecánica-del-subnetting)
3. [Método de cálculo paso a paso](#3-método-de-cálculo-paso-a-paso)
4. [Subnetting en el tercer octeto (prefijos /17 a /24)](#4-subnetting-en-el-tercer-octeto-prefijos-17-a-24)
5. [Subnetting en el cuarto octeto (prefijos /25 a /30)](#5-subnetting-en-el-cuarto-octeto-prefijos-25-a-30)
6. [Diseñar un esquema de subredes](#6-diseñar-un-esquema-de-subredes)
7. [VLSM (Variable Length Subnet Mask)](#7-vlsm-variable-length-subnet-mask)
8. [Supernetting (agregación de rutas)](#8-supernetting-agregación-de-rutas)
9. [Subnetting en Linux](#9-subnetting-en-linux)
10. [Errores comunes](#10-errores-comunes)
11. [Cheatsheet](#11-cheatsheet)
12. [Ejercicios](#12-ejercicios)

---

## 1. Qué es subnetting

Subnetting es el proceso de dividir una red grande en redes más pequeñas (subredes).
Tomas bits de la parte de **host** y los conviertes en parte de **red**, creando más
redes con menos hosts cada una.

```
Sin subnetting:
  192.168.1.0/24 → 1 red con 254 hosts
  Todos los dispositivos en la misma red

Con subnetting a /26:
  192.168.1.0/26   → subred 1: 62 hosts (oficina A)
  192.168.1.64/26  → subred 2: 62 hosts (oficina B)
  192.168.1.128/26 → subred 3: 62 hosts (servidores)
  192.168.1.192/26 → subred 4: 62 hosts (invitados)

  Misma red original, 4 subredes independientes
```

### Por qué se hace

```
1. SEGURIDAD:
   Los hosts de una subred no ven el tráfico broadcast de otra
   → servidores aislados de la red de invitados

2. RENDIMIENTO:
   Menos hosts por subred = menos tráfico broadcast
   → una red /24 con 254 hosts genera más broadcast que cuatro /26

3. ORGANIZACIÓN:
   Asignar subredes por departamento, piso, función
   → facilita administración y troubleshooting

4. EFICIENCIA:
   No desperdiciar direcciones (un enlace punto a punto no necesita /24)
   → /30 da 2 hosts, perfecto para un enlace router-router
```

---

## 2. La mecánica del subnetting

### Bits prestados

Subnetting funciona "prestando" bits de la porción de host para crear la porción de
subred:

```
Red original: 192.168.1.0/24

  IP:       11000000.10101000.00000001.00000000
  Máscara:  11111111.11111111.11111111.00000000
            ──────── red (24 bits) ──── host(8)

Subnetted a /26 (prestamos 2 bits):

  Máscara:  11111111.11111111.11111111.11000000
            ──────── red (24 bits) ────│sb│host│
                                       ──  ────
                                      2 bits  6 bits
                                     subred   host

  2 bits de subred → 2² = 4 subredes
  6 bits de host   → 2⁶ - 2 = 62 hosts por subred
```

### Fórmulas fundamentales

```
Número de subredes    = 2^(bits prestados)
Hosts por subred      = 2^(bits de host restantes) - 2
Tamaño de bloque      = 2^(bits de host restantes)
                      = 256 - valor del octeto de la máscara (para el octeto variable)
```

### El octeto "interesante"

Solo necesitas calcular en el octeto donde la máscara **no es 255 ni 0** — es el
octeto donde ocurre la división entre red y host:

```
/24 → 255.255.255.0     → Octeto interesante: 4º (valor 0)
/26 → 255.255.255.192   → Octeto interesante: 4º (valor 192)
/20 → 255.255.240.0     → Octeto interesante: 3º (valor 240)
/12 → 255.240.0.0       → Octeto interesante: 2º (valor 240)
```

---

## 3. Método de cálculo paso a paso

### Dado: una IP con máscara, encontrar red, broadcast, rango

**Ejemplo: 192.168.1.130/26**

**Paso 1**: Identificar la máscara y el octeto interesante

```
/26 → 255.255.255.192
Octeto interesante: 4º (192)
```

**Paso 2**: Calcular el tamaño de bloque (block size)

```
Tamaño de bloque = 256 - 192 = 64

Las subredes empiezan en múltiplos de 64:
  0, 64, 128, 192
```

**Paso 3**: ¿En qué subred cae la IP?

```
IP: 192.168.1.130
Octeto interesante: 130

Subredes:  0, 64, 128, 192
           130 cae entre 128 y 192
           → Subred: 192.168.1.128
```

**Paso 4**: Calcular broadcast y rango

```
Dirección de red:    192.168.1.128    (inicio del bloque)
Siguiente subred:    192.168.1.192    (inicio del siguiente bloque)
Broadcast:           192.168.1.191    (siguiente subred - 1)
Primer host:         192.168.1.129    (red + 1)
Último host:         192.168.1.190    (broadcast - 1)
Hosts usables:       62               (2⁶ - 2)
```

### Resumen visual

```
192.168.1.0/26:

  .0────────.63    .64───────.127    .128──────.191    .192──────.255
  ┌──────────┐     ┌──────────┐     ┌──────────┐     ┌──────────┐
  │ Subred 0 │     │ Subred 1 │     │ Subred 2 │     │ Subred 3 │
  │ Red: .0  │     │ Red: .64 │     │ Red: .128│     │ Red: .192│
  │ Bcast:.63│     │ Bcast:127│     │ Bcast:191│     │ Bcast:255│
  │ Hosts:   │     │ Hosts:   │     │ Hosts:   │     │ Hosts:   │
  │ .1 - .62 │     │ .65 -.126│     │ .129-.190│     │ .193-.254│
  └──────────┘     └──────────┘     └──────────┘     └──────────┘
                                     ▲
                                     │
                              192.168.1.130 cae aquí
```

---

## 4. Subnetting en el tercer octeto (prefijos /17 a /24)

Cuando el octeto interesante es el **tercero**, el cálculo es el mismo pero los
bloques se expresan en el tercer octeto:

### Ejemplo: 172.16.5.100/20

**Paso 1**: Máscara

```
/20 → 255.255.240.0
Octeto interesante: 3º (240)
```

**Paso 2**: Tamaño de bloque

```
256 - 240 = 16
Las subredes empiezan cada 16 unidades del 3er octeto:
  172.16.0.0, 172.16.16.0, 172.16.32.0, ...
```

**Paso 3**: ¿Dónde cae la IP?

```
3er octeto de la IP: 5
Múltiplos de 16: 0, 16, 32...
5 cae entre 0 y 16
→ Subred: 172.16.0.0/20
```

**Paso 4**: Red, broadcast, rango

```
Red:          172.16.0.0
Broadcast:    172.16.15.255    (siguiente subred 172.16.16.0, restamos 1)
Primer host:  172.16.0.1
Último host:  172.16.15.254
Hosts usables: 2^12 - 2 = 4094
```

### Por qué el broadcast es 172.16.15.255

```
Subred: 172.16.0.0/20

20 bits de red, 12 bits de host

Los 12 bits de host abarcan:
  - 4 bits del 3er octeto (los que no cubre la máscara 240 = 11110000)
  - 8 bits del 4º octeto completo

Dirección de red (todos los bits de host = 0):
  172.16. 0000 0000 . 00000000  → 172.16.0.0
          ──── ────   ────────
          red  host   host

Broadcast (todos los bits de host = 1):
  172.16. 0000 1111 . 11111111  → 172.16.15.255
          ──── ────   ────────
          red  host   host

El rango del 3er octeto va de 0 (0000) a 15 (1111).
```

---

## 5. Subnetting en el cuarto octeto (prefijos /25 a /30)

Estas son las subredes más comunes en la práctica diaria:

### Referencia rápida

```
/25  Máscara: 255.255.255.128   Bloque: 128   Subredes: 2    Hosts: 126
     .0-.127  |  .128-.255

/26  Máscara: 255.255.255.192   Bloque: 64    Subredes: 4    Hosts: 62
     .0-.63  |  .64-.127  |  .128-.191  |  .192-.255

/27  Máscara: 255.255.255.224   Bloque: 32    Subredes: 8    Hosts: 30
     .0-.31  |  .32-.63  |  .64-.95  |  .96-.127  | ...

/28  Máscara: 255.255.255.240   Bloque: 16    Subredes: 16   Hosts: 14
     .0-.15  |  .16-.31  |  .32-.47  |  .48-.63  | ...

/29  Máscara: 255.255.255.248   Bloque: 8     Subredes: 32   Hosts: 6
     .0-.7  |  .8-.15  |  .16-.23  | ...

/30  Máscara: 255.255.255.252   Bloque: 4     Subredes: 64   Hosts: 2
     .0-.3  |  .4-.7  |  .8-.11  | ...
```

### Ejemplo rápido: 10.0.0.200/28

```
Máscara: 255.255.255.240
Bloque: 256 - 240 = 16

Subredes del 4º octeto: 0, 16, 32, 48, 64, 80, 96, 112,
                         128, 144, 160, 176, 192, 208, 224, 240

200 cae entre 192 y 208

Red:       10.0.0.192
Broadcast: 10.0.0.207
Rango:     10.0.0.193 - 10.0.0.206  (14 hosts)
```

### /30 — enlaces punto a punto

```
Red 10.0.0.0/30:
  Red:       10.0.0.0
  Host 1:    10.0.0.1   ← Router A
  Host 2:    10.0.0.2   ← Router B
  Broadcast: 10.0.0.3

Siguiente enlace: 10.0.0.4/30
  Red:       10.0.0.4
  Host 1:    10.0.0.5   ← Router C
  Host 2:    10.0.0.6   ← Router D
  Broadcast: 10.0.0.7

Perfecto para enlaces WAN entre routers — solo necesitas 2 IPs.
```

---

## 6. Diseñar un esquema de subredes

### Problema típico

"Tienes la red 192.168.10.0/24. Necesitas crear subredes para:
- Departamento A: 50 hosts
- Departamento B: 25 hosts
- Departamento C: 10 hosts
- Enlace entre routers: 2 hosts"

### Método: del más grande al más pequeño

Siempre asigna las subredes más grandes primero para evitar fragmentación del espacio
de direcciones.

```
Paso 1: Determinar el prefijo necesario para cada subred

  Dept A (50 hosts):  2^6 = 64 → 62 usables → /26 ✔
  Dept B (25 hosts):  2^5 = 32 → 30 usables → /27 ✔
  Dept C (10 hosts):  2^4 = 16 → 14 usables → /28 ✔
  Enlace (2 hosts):   2^2 = 4  → 2 usables  → /30 ✔

Paso 2: Asignar en orden de mayor a menor

  192.168.10.0/26    Dept A  (62 hosts)  .0-.63
  192.168.10.64/27   Dept B  (30 hosts)  .64-.95
  192.168.10.96/28   Dept C  (14 hosts)  .96-.111
  192.168.10.112/30  Enlace  (2 hosts)   .112-.115
  192.168.10.116-255          Libre para futuro uso
```

### Visualización

```
192.168.10.0                                              192.168.10.255
├─────────────────────┬──────────┬──────┬──┬──────────────────────────┤
│     Dept A /26      │ Dept B   │Dept C│En│         LIBRE            │
│   .0 ─── .63       │/27       │ /28  │la│                          │
│   (62 hosts)        │.64─.95   │.96   │ce│                          │
│                     │(30 hosts)│.111  │/30                          │
│                     │          │(14)  │.112-.115                    │
├─────────────────────┴──────────┴──────┴──┴──────────────────────────┤
```

---

## 7. VLSM (Variable Length Subnet Mask)

### Qué es

VLSM es simplemente la capacidad de usar **diferentes tamaños de máscara** dentro
de la misma red. Es lo que hicimos en el ejemplo anterior: /26, /27, /28 y /30
dentro de la misma 192.168.10.0/24.

```
Sin VLSM (clásico):
  Todas las subredes deben ser del mismo tamaño.
  192.168.10.0/24 dividido en /26 → 4 subredes de 62 hosts cada una
  El enlace punto a punto desperdicia 60 direcciones.

Con VLSM:
  Cada subred tiene el tamaño exacto que necesita.
  /26 para 50 hosts, /30 para un enlace → sin desperdicio.
```

### Regla clave para VLSM

Las subredes no pueden solaparse. Cada bloque debe empezar en un múltiplo de su
propio tamaño de bloque:

```
✔ CORRECTO:
  192.168.10.0/26    (bloque 64, empieza en 0   → 0 es múltiplo de 64)
  192.168.10.64/27   (bloque 32, empieza en 64  → 64 es múltiplo de 32)
  192.168.10.96/28   (bloque 16, empieza en 96  → 96 es múltiplo de 16)

✗ INCORRECTO:
  192.168.10.0/26    (bloque 64, .0-.63)
  192.168.10.50/27   (bloque 32, .50-.81)  ← ¡se solapa con la primera!
```

---

## 8. Supernetting (agregación de rutas)

### Qué es

Supernetting es lo opuesto a subnetting: combinar varias redes pequeñas en una ruta
resumida más grande. Se usa para reducir el tamaño de las tablas de routing.

```
Sin supernetting (4 rutas en la tabla):
  192.168.0.0/24
  192.168.1.0/24
  192.168.2.0/24
  192.168.3.0/24

Con supernetting (1 ruta resumida):
  192.168.0.0/22    ← cubre las 4 redes anteriores
```

### Cómo funciona

```
192.168.0.0   = 11000000.10101000.000000 00.00000000
192.168.1.0   = 11000000.10101000.000000 01.00000000
192.168.2.0   = 11000000.10101000.000000 10.00000000
192.168.3.0   = 11000000.10101000.000000 11.00000000
                                  ──────
                          22 bits comunes → /22

Los primeros 22 bits son idénticos → se pueden resumir en 192.168.0.0/22
```

### Requisito para agregar

Las redes deben ser **contiguas** y **alineadas** en una potencia de 2:

```
✔ Agregable:     192.168.0.0/24 + 192.168.1.0/24 = 192.168.0.0/23
✔ Agregable:     192.168.0.0/24 + .1.0 + .2.0 + .3.0 = 192.168.0.0/22
✗ No agregable:  192.168.1.0/24 + 192.168.3.0/24 (no contiguas)
✗ No agregable:  192.168.1.0/24 + 192.168.2.0/24 (contiguas pero no alineadas a /23)
```

---

## 9. Subnetting en Linux

### Verificar a qué subred pertenece un host

```bash
# Ver la subred de tu interfaz
ip -4 addr show enp0s3
# inet 192.168.1.10/24 brd 192.168.1.255 scope global

# Herramienta ipcalc (instalar si no está)
# Fedora/RHEL
sudo dnf install ipcalc

ipcalc 192.168.1.130/26
```

Salida de `ipcalc`:

```
Address:   192.168.1.130
Network:   192.168.1.128/26
Netmask:   255.255.255.192
HostMin:   192.168.1.129
HostMax:   192.168.1.190
Broadcast: 192.168.1.191
Hosts/Net: 62
```

### Verificar si dos IPs están en la misma subred

```
¿Están 192.168.1.10/24 y 192.168.1.200/24 en la misma subred?

  10  AND 255 = 0   →  192.168.1.0
  200 AND 255 = 0   →  192.168.1.0
  Misma red → SÍ, comunicación directa

¿Están 192.168.1.10/26 y 192.168.1.200/26 en la misma subred?

  10  AND 192 = 0   →  192.168.1.0
  200 AND 192 = 192 →  192.168.1.192
  Redes distintas → NO, necesitan un router
```

### Routing entre subredes

```bash
# Tu PC está en 192.168.1.0/26 y quieres llegar a 192.168.1.192/26
# Necesitas una ruta que pase por un router

ip route show
# 192.168.1.0/26 dev enp0s3 scope link
# Para llegar a .192/26, el paquete va al gateway

# Si administras el router, configuras rutas a ambas subredes
sudo ip route add 192.168.1.192/26 via 192.168.1.1
```

### Crear una topología con subredes en VMs

```bash
# VM1 en subred A
sudo ip addr add 10.0.1.1/24 dev enp0s8

# VM2 en subred A (puede comunicarse con VM1 directamente)
sudo ip addr add 10.0.1.2/24 dev enp0s8

# VM3 en subred B (necesita router para hablar con VM1)
sudo ip addr add 10.0.2.1/24 dev enp0s8

# VM-Router con una interfaz en cada subred
sudo ip addr add 10.0.1.254/24 dev enp0s8    # Subred A
sudo ip addr add 10.0.2.254/24 dev enp0s9    # Subred B
sudo sysctl -w net.ipv4.ip_forward=1          # Habilitar forwarding
```

---

## 10. Errores comunes

### Error 1: Olvidar restar 2 al calcular hosts usables

```
/26 → 2^6 = 64 direcciones
Hosts usables = 64 - 2 = 62  ← Restar red y broadcast

NO: "Tengo 64 hosts disponibles"
SÍ: "Tengo 62 hosts asignables"

Excepción: /31 (RFC 3021) no resta, da exactamente 2 hosts.
Usado solo en enlaces punto a punto.
```

### Error 2: Empezar una subred en una dirección no alineada

```
✗ "Subred 192.168.1.100/26"
  Block size = 64
  Subredes válidas: .0, .64, .128, .192
  .100 no es un inicio válido de /26

✔ 192.168.1.100/26 → la subred es 192.168.1.64/26
  (la IP .100 pertenece a la subred que empieza en .64)
```

### Error 3: Confundir "número de subredes" con "hosts por subred"

```
/24 → /26 (prestamos 2 bits)

Subredes = 2^(bits prestados) = 2² = 4
Hosts    = 2^(bits de host)   = 2⁶ = 64 - 2 = 62

NO es: "4 subredes con 4 hosts cada una"
```

### Error 4: Solapar subredes al diseñar VLSM

```
✗ SOLAPAMIENTO:
  192.168.1.0/26    → .0  a .63
  192.168.1.32/27   → .32 a .63   ← ¡.32-.63 está en ambas!

✔ CORRECTO:
  192.168.1.0/26    → .0  a .63
  192.168.1.64/27   → .64 a .95   ← sin solapamiento
```

Siempre verifica que el inicio de una subred está **después** del broadcast de la
anterior.

### Error 5: Asumir que la máscara de la interfaz refleja la red completa

```bash
ip addr show
# inet 10.0.0.5/30

"Estoy en la red 10.0.0.0"  ← INCORRECTO
"Estoy en la red 10.0.0.4"  ← CORRECTO

Con /30:
  5 AND 252 = 4  → Red: 10.0.0.4/30
  No 10.0.0.0 (esa sería otra subred /30)
```

---

## 11. Cheatsheet

```
╔══════════════════════════════════════════════════════════════════════╗
║                   SUBNETTING — REFERENCIA RÁPIDA                     ║
╠══════════════════════════════════════════════════════════════════════╣
║                                                                      ║
║  FÓRMULAS                                                            ║
║  Subredes      = 2^(bits prestados)                                  ║
║  Hosts/subred  = 2^(bits de host) - 2                                ║
║  Bloque        = 256 - valor del octeto de la máscara                ║
║  Red           = IP AND máscara                                      ║
║  Broadcast     = siguiente subred - 1                                ║
║                                                                      ║
║  MÉTODO RÁPIDO                                                       ║
║  1. Máscara → octeto interesante                                     ║
║  2. Bloque = 256 - octeto                                            ║
║  3. Listar múltiplos: 0, bloque, 2×bloque, ...                      ║
║  4. La IP cae entre dos múltiplos → el menor es la red               ║
║  5. Broadcast = siguiente múltiplo - 1                               ║
║                                                                      ║
║  TABLA DE REFERENCIA (4º octeto)                                     ║
║  /25  128  │ 2 subredes  │ 126 hosts │ bloque 128                   ║
║  /26  192  │ 4 subredes  │  62 hosts │ bloque 64                    ║
║  /27  224  │ 8 subredes  │  30 hosts │ bloque 32                    ║
║  /28  240  │ 16 subredes │  14 hosts │ bloque 16                    ║
║  /29  248  │ 32 subredes │   6 hosts │ bloque 8                     ║
║  /30  252  │ 64 subredes │   2 hosts │ bloque 4                     ║
║                                                                      ║
║  DISEÑO DE SUBREDES                                                  ║
║  1. Ordenar necesidades de mayor a menor                             ║
║  2. Asignar prefijo que cubra cada necesidad                         ║
║  3. Asignar secuencialmente (la siguiente empieza después             ║
║     del broadcast de la anterior)                                    ║
║  4. Verificar: ¿hay solapamiento? ¿queda espacio libre?             ║
║                                                                      ║
║  VLSM: diferentes máscaras dentro de la misma red principal          ║
║  SUPERNETTING: resumir redes contiguas en una ruta más grande        ║
║                                                                      ║
║  HERRAMIENTAS LINUX                                                  ║
║  ipcalc 192.168.1.130/26     Calcular red/broadcast/rango           ║
║  ip addr show                Ver subredes configuradas               ║
║  ip route show               Ver rutas entre subredes                ║
║                                                                      ║
╚══════════════════════════════════════════════════════════════════════╝
```

---

## 12. Ejercicios

### Ejercicio 1: Cálculo de subredes

Para cada IP/máscara, determina: dirección de red, broadcast, primer host, último
host, y número de hosts usables. Muestra el trabajo con el método del bloque.

1. `10.0.0.77/28`
2. `172.16.45.200/21`
3. `192.168.5.100/27`
4. `10.10.10.10/30`
5. `192.168.0.130/25`

Verifica tus respuestas con `ipcalc`:

```bash
ipcalc 10.0.0.77/28
```

**Pregunta de predicción**: antes de ejecutar `ipcalc`, ¿podrías determinar en qué
subred cae `172.16.45.200/21` sin hacer el AND binario completo, usando solo el método
del bloque?

### Ejercicio 2: Diseñar un esquema de subredes

Tu empresa recibe la red `10.20.0.0/22` (1022 hosts disponibles). Necesitas crear
subredes para:

| Subred | Hosts necesarios |
|---|---|
| Producción | 200 |
| Desarrollo | 100 |
| QA | 50 |
| Management | 25 |
| DMZ | 10 |
| Enlace router A-B | 2 |
| Enlace router B-C | 2 |

1. Determina el prefijo necesario para cada subred
2. Asigna las subredes en orden de mayor a menor
3. Escribe la dirección de red, broadcast y rango de cada una
4. Calcula cuántas direcciones quedan libres
5. Dibuja un diagrama similar al de la sección 6

**Pregunta de reflexión**: ¿qué pasa si después de asignar todo, el departamento de
producción necesita crecer a 300 hosts? ¿Podrías expandir su subred sin afectar a las
demás? ¿Qué precaución habrías podido tomar al diseñar el esquema inicial?

### Ejercicio 3: Subnetting práctico en VMs

Si tienes dos VMs con una red interna (host-only o internal network en VirtualBox/KVM):

1. Asigna a la VM1 la IP `10.0.1.1/24` y a la VM2 `10.0.1.2/24`
2. Haz ping entre ellas — debe funcionar (misma subred)
3. Ahora cambia la VM2 a `10.0.1.2/25` (sin cambiar la VM1)
4. Haz ping de nuevo — ¿funciona?

**Pregunta de predicción**: con VM1 en `/24` y VM2 en `/25`, ¿podrá VM1 hacer ping a
VM2? ¿Y VM2 a VM1? Piensa en qué subred cree cada una que está.

5. Cambia VM2 a `10.0.2.1/24` (diferente subred)
6. Haz ping — ¿funciona?
7. Para que funcione, necesitas una tercera VM como router:
   ```bash
   # VM-Router: interfaz en cada subred + forwarding
   sudo ip addr add 10.0.1.254/24 dev enp0s8
   sudo ip addr add 10.0.2.254/24 dev enp0s9
   sudo sysctl -w net.ipv4.ip_forward=1
   ```
8. Configura rutas en VM1 y VM2:
   ```bash
   # VM1
   sudo ip route add 10.0.2.0/24 via 10.0.1.254
   # VM2
   sudo ip route add 10.0.1.0/24 via 10.0.2.254
   ```
9. Haz ping entre VM1 y VM2 a través del router

**Pregunta de reflexión**: ¿por qué dos hosts en subredes diferentes no pueden
comunicarse directamente aunque estén conectados al mismo switch físico? ¿Qué capa
del modelo TCP/IP impide la comunicación directa?
