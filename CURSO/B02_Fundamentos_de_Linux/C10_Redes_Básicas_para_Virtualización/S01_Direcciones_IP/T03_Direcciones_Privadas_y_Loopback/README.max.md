# T03 — Direcciones Privadas y Loopback

## El Problema: No suficientes IPs públicas

IPv4 tiene solo ~4.3 mil millones de direcciones, pero hay miles de millones de dispositivos. Peor: cuando IPv4 se diseñó (1983), nadie imaginaba tantos dispositivos.

**Solución**: Reservar rangos de IPs para uso **privado** (interno), que se pueden reutilizar en TODAS las redes del mundo, siempre que no sean ruteables en internet público.

## Rangos de Direcciones Privadas (RFC 1918)

| Rango | CIDR | Direcciones disponibles |
|-------|------|--------------------------|
| 10.0.0.0 – 10.255.255.255 | 10.0.0.0/8 | 16,777,216 (~16M) |
| 172.16.0.0 – 172.31.255.255 | 172.16.0.0/12 | 1,048,576 (~1M) |
| 192.168.0.0 – 192.168.255.255 | 192.168.0.0/16 | 65,536 (~65K) |

### ¿Cuándo usar cada uno?

| Rango | Uso típico |
|-------|-----------|
| 10.0.0.0/8 | Redes corporativas grandes, data centers, nubes privadas |
| 172.16.0.0/12 | AWS VPCs, algunas redes corporativas |
| 192.168.0.0/16 | Redes domésticas, pequeñas oficinas, labs (el más común) |

### Ejemplos reales

```
Tu router WiFi doméstica:      192.168.1.1   (gateway)
Tu laptop:                     192.168.1.100 (asignada por DHCP)
Red NAT de libvirt (QEMU/KVM): 192.168.122.1 (gateway de la VM)
Red Docker:                    172.17.0.0/16
AWS VPC por defecto:           172.31.0.0/16
```

## Cómo acceden las IPs privadas a internet?

No pueden. Una IP privada **nunca sale a internet** directamente.

La solución es **NAT** (Network Address Translation), que se explica en el siguiente tema. El gateway (tu router) hace NAT: reemplaza la IP privada origen por su IP pública antes de enviar el paquete a internet.

```
[Mi laptop: 192.168.1.100]
    ↓ (paquete a google.com)
[Mi router: 192.168.1.1 → 203.0.113.50]  ← NAT happening here
    ↓
[Internet]
```

## Loopback (localhost)

La dirección de loopback `127.0.0.1` siempre apunta a **tu propia máquina**. Es una dirección especial que nunca sale por ninguna interfaz de red.

```
┌──────────────────────────────────────┐
│  Tu máquina (localhost)              │
│                                      │
│  127.0.0.1  ←────────────────┐       │
│  ::1 (IPv6)                  │       │
│                              │       │
│  Cualquier servicio que      │       │
│  escuches en 127.0.0.1:80    │       │
│  solo es accesible aquí ─────┘       │
└──────────────────────────────────────┘
```

### ¿Para qué sirve?

```bash
# Probar que un servicio web funciona sin abrir puertos
curl http://127.0.0.1:8080

# Ver si SSH está escuchando en localhost
ss -tlnp | grep 127.0.0.1

# Un servidor MySQL escuchando solo en loopback (más seguro)
# En /etc/mysql/my.cnf: bind-address = 127.0.0.1
```

En IPv6, loopback es `::1` (un solo paso, no 127.x.x.x).

## all-zeros y all-ones (referencia rápida)

```
0.0.0.0        → "esta red" o "cualquier IP" (contexto-dependiente)
255.255.255.255 → broadcast de la red actual (no es ruteable)
```

`0.0.0.0` se usa en:
- Rutas por defecto: "para llegar a cualquier red, usa este gateway"
- Bind de servicios: "escuchar en todas las interfaces disponibles"

## Ejercicios

1. Ejecuta `ip addr` y responde:
   - ¿Tu IP es privada o pública?
   - ¿En qué rango cae? (10.x, 172.16–31.x, 192.168.x)

2. Ejecuta `curl ifconfig.me` (o visita ifconfig.me) para ver tu IP pública.
   Compara con tu IP privada — ¿son iguales?

3. Haz `ping 127.0.0.1` — ¿cuál es la latencia? ¿Por qué es tan baja?

4. Investiga la IP de tu teléfono cuando está en WiFi vs datos móviles:
   - En WiFi: probablemente 192.168.x.x (privada)
   - En datos móviles: probablemente una IP pública (asignada por tu operador)

5. En QEMU/KVM, la red NAT por defecto usa `192.168.122.0/24`:
   - ¿Es este rango routable en internet?
   - ¿Qué hace falta para que una VM en esa red acceda a internet?

## Resumen

| Concepto | Descripción |
|---|---|
| IPs privadas | 10.x, 172.16–31.x, 192.168.x — no routables en internet |
| NAT | Traduce IPs privadas a pública en el gateway |
| 192.168.x.x | El rango más común en redes domésticas |
| 10.x | Para redes grandes (corporaciones, clouds) |
| 127.0.0.1 / ::1 | Loopback — la propia máquina |
| 0.0.0.0 | "esta red" o "todas las interfaces" |
