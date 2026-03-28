# T02 — Máscara de Red y CIDR

## Qué es una máscara de red

Una **máscara de red** indica qué porción de una dirección IP identifica la **red** y qué porción identifica el **host** (dispositivo individual dentro de esa red).

La máscara funciona con **bits encendidos (1)** para la parte de red y **bits apagados (0)** para la parte de host.

```
IP:       192.168.1.100    = 11000000.10101000.00000001.01100100
Máscara:  255.255.255.0     = 11111111.11111111.11111111.00000000
          ────────────── = ────────────── = ──────────────
          Red (24 bits)    Host (8 bits)
```

- **Red**: `192.168.1.0` (los primeros 24 bits)
- **Host**: `100` (el último octeto)

## Máscara Tradicional vs CIDR

### Notación tradicional (decimal punteado)

```
255.255.255.0
```

### Notación CIDR (Classless Inter-Domain Routing)

```
/24
```

El número después de `/` indica **cuántos bits son de red**. Es simplemente una forma más corta de escribir la máscara.

| CIDR | Máscara decimal | IPs disponibles | Hosts usables |
|------|-----------------|-----------------|--------------|
| /30 | 255.255.255.252 | 4 | 2 |
| /29 | 255.255.255.248 | 8 | 6 |
| /28 | 255.255.255.240 | 16 | 14 |
| /27 | 255.255.255.224 | 32 | 30 |
| /26 | 255.255.255.192 | 64 | 62 |
| /25 | 255.255.255.128 | 128 | 126 |
| **/24** | **255.255.255.0** | **256** | **254** |
| /23 | 255.255.254.0 | 512 | 510 |
| /22 | 255.255.252.0 | 1024 | 1022 |
| /16 | 255.255.0.0 | 65,536 | 65,534 |
| /8 | 255.0.0.0 | 16,777,216 | 16,777,214 |

**¿Por qué "254 usables"?** En toda subred hay 2 direcciones reservadas:
1. **Dirección de red** (primer IP, ej: 192.168.1.0) — no asignable a ningún host
2. **Dirección de broadcast** (última IP, ej: 192.168.1.255) — para enviar a todos

## Cálculo de Subred — Paso a Paso

Dada la red `192.168.1.0/24`:

```
Dirección de red:     192.168.1.0      (primera IP, identifica la red)
Primera IP usable:    192.168.1.1      (generalmente el gateway)
Última IP usable:     192.168.1.254    (último host antes del broadcast)
Dirección broadcast:  192.168.1.255    (última IP, para todos)
Total de hosts:        256 - 2 = 254
```

## Ejemplo con CIDR Distinto

Dada la red `10.0.0.0/8`:

```
Dirección de red:     10.0.0.0
Primera IP usable:   10.0.0.1
Última IP usable:    10.255.255.254
Broadcast:           10.255.255.255
Total de hosts:      16,777,216 - 2 = 16,777,214
```

## Herramienta ipcalc

`ipcalc` calcula automáticamente todos los parámetros de una subred:

```bash
dnf install ipcalc       # Fedora/RHEL
apt install ipcalc       # Debian/Ubuntu

ipcalc 192.168.1.0/24
```

Salida:

```
Address:    192.168.1.0
Netmask:    255.255.255.0 = 24
Wildcard:   0.0.0.255
Network:    192.168.1.0/24
Broadcast:  192.168.1.255
HostMin:    192.168.1.1
HostMax:    192.168.1.254
Hosts/Net:  254
```

## Por qué /24 es el Más Común

En redes domésticas y de oficina, `/24` (254 hosts) es el balance perfecto:
- 254 hosts es suficiente para la mayoría de casos
- /25 (126 hosts) puede ser demasiado pequeño
- /23 (510 hosts) es innecesariamente grande

En QEMU/KVM, la red NAT por defecto usa `192.168.122.0/24`:
- 254 VMs máximo en esa red
- Suficiente para labs individuales

## Ejercicios

1. Calcula mentalmente (sin script):
   - Rango de hosts en `192.168.0.0/26`
   - Broadcast de `10.0.0.0/16`
   - Primera IP usable en `172.16.0.0/24`

2. Usando `ipcalc`:
   ```bash
   ipcalc 192.168.122.0/24
   # Responde: ¿Cuántos hosts hay en esta red?
   # ¿Cuál es el gateway típico (primera IP usable)?
   ```

3. En QEMU/KVM, si creas una red aislada con `10.0.1.0/28`:
   - ¿Cuántos discos extra puedes adjuntar a UNA VM antes de necesitar más IPs?
   - ¿Cuál es el rango de IPs disponibles?

4. Investiga: ¿por qué las redes domésticas suelen ser `192.168.1.0/24` y no `10.0.0.0/24`?

## Respuestas a los Ejercicios

**Ejercicio 1:**
- `192.168.0.0/26`: hosts 192.168.0.1–192.168.0.62, broadcast 192.168.0.63
- `10.0.0.0/16`: broadcast 10.255.255.255
- `172.16.0.0/24`: primera IP usable 172.16.0.1

**Ejercicio 2:**
- Hosts: 254
- Gateway típico: 192.168.122.1

**Ejercicio 3:**
- /28 = 16 IPs totales, 14 usables
- IPs disponibles: 10.0.1.1–10.0.1.14

## Resumen

| Concepto | Descripción |
|---|---|
| Máscara de red | Indica qué bits son red (1) y cuáles son host (0) |
| CIDR (/24) | Forma corta de escribir la máscara |
| /24 = 255.255.255.0 | 256 IPs totales, 254 usables |
| broadcast | Última IP de la subred, envía a todos los hosts |
| `ipcalc` | Calcula automáticamente rangos de subred |
