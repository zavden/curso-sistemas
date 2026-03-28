# T01 — NAT (Network Address Translation)

## El problema

Las direcciones IPv4 privadas (10.x, 172.16–31.x, 192.168.x) **no son ruteables en internet**. Si una VM con IP 192.168.122.100 envía un paquete a google.com, los routers de internet lo descartarán porque:

1. La IP origen 192.168.122.100 es de rango privado
2. Solo los routers saben cómo entregar paquetes a IPs **públicas**

## La solución: NAT

**NAT (Network Address Translation)** es el mecanismo por el cual un dispositivo (generalmente el gateway/router) traduce IPs privadas a IPs públicas cuando los paquetes salen a internet, y hace la traducción inversa cuando las respuestas regresan.

```
┌─────────────────────────────────────────────────────────────────┐
│ RED PRIVADA                           INTERNET                  │
│                                                                 │
│  192.168.122.100 (VM Debian)              google.com              │
│         │                                     │                 │
│         │  src: 192.168.122.100 → dst: 8.8.8.8 │                 │
│         │                                     │                 │
│    ┌────┴──────────────────────────────┐      │                 │
│    │ Gateway libvirt: 192.168.122.1     │      │                 │
│    │ (tabla NAT)                        │      │                 │
│    │                                    │      │                 │
│    │  NAT:                               │      │                 │
│    │  192.168.122.100:45678 → 8.8.8.8    │      │                 │
│    │       ↓ (traduce)                  │      │                 │
│    │  203.0.113.50:45678 → 8.8.8.8       │──────┤                 │
│    │                                    │      │                 │
│    │  La tabla recuerda que el puerto   │      │                 │
│    │  45678 de 203.0.113.50 corresponde  │      │                 │
│    │  a la VM 192.168.122.100            │      │                 │
│    └────────────────────────────────────┘      │                 │
└─────────────────────────────────────────────────────────────┘
```

## Tipos de NAT

### SNAT (Source NAT)

Modifica la **IP origen** del paquete saliente.

```
Antes:  src: 192.168.122.100 → dst: 8.8.8.8
Después: src: 203.0.113.50 → dst: 8.8.8.8
```

El gateway anota en su tabla: "cuando reciba respuesta en 203.0.113.50:45678, reenviar a 192.168.122.100".

### DNAT (Destination NAT)

Modifica la **IP destino** del paquete entrante. Se usa para **redirigir puertos** (port forwarding).

```
Antes:  src: 203.0.113.50:80 → dst: 203.0.113.50
Después: src: 203.0.113.50:80 → dst: 192.168.122.100:80
```

Cuando accedes a `http://tu-ip-publica` desde internet, DNAT redirige el tráfico a tu VM.

### MASQUERADE

Es SNAT automático. En lugar de especificar `203.0.113.50`, el gateway usa **su propia IP pública actual**. Es útil cuando la IP pública es dinámica (DHCP del ISP).

```bash
# Equivalente a:
iptables -t nat -A POSTROUTING -s 192.168.122.0/24 -j MASQUERADE
```

## En QEMU/KVM: NAT por Defecto

Cuando libvirt crea la red `default`, configura automáticamente:

- Subred: `192.168.122.0/24`
- Gateway: `192.168.122.1` (el host)
- Servidor DHCP: `192.168.122.1` (range 192.168.122.2–192.168.122.254)
- NAT hacia la red física del host

```bash
# Ver la configuración NAT de libvirt
virsh net-dumpxml default
```

La salida incluye algo como:

```xml
<network>
  <name>default</name>
  <forward mode='nat'/>
  <bridge name='virbr0' stp='on' delay='0'/>
  <ip address='192.168.122.1' netmask='255.255.255.0'>
    <dhcp>
      <range start='192.168.122.2' end='192.168.122.254'/>
    </dhcp>
  </ip>
</network>
```

`<forward mode='nat'/>` es lo que habilita que las VMs accedan a internet.

## Limitaciones de NAT

1. **La VM no es visible desde internet** — solo puede iniciar conexiones salientes
2. **Algunos protocolos complicados** — FTP activo, SIP, H.323 pueden tener problemas
3. **Latencia adicional** — cada paquete debe ser traducido en el gateway
4. **Punto único de falla** — si el gateway cae, las VMs pierden acceso a internet

## Cuándo Usar NAT vs Bridge

| Escenario | NAT | Bridge |
|---|---|---|
| Lab donde VMs necesitan internet | ✅ | ✅ |
| VMs visibles desde la red del host | ❌ | ✅ |
| Topología de red aislada (labs de firewall) | ❌ (aislada es mejor) | ❌ |
| Acceso rápido sin configurar red | ✅ | ❌ |

## Ejercicios

1. En tu host con QEMU/KVM instalado, ejecuta:
   ```bash
   virsh net-list
   # ¿Hay alguna red en modo NAT?
   
   ip addr show virbr0
   # ¿Cuál es la IP del bridge de libvirt?
   ```

2. Inicia una VM con libvirt y verifica:
   ```bash
   # Dentro de la VM:
   ip addr show
   # ¿En qué subred está?
   
   ping 8.8.8.8
   # ¿Puede acceder a internet?
   
   curl google.com
   # ¿DNS funciona?
   ```

3. Ejecuta `iptables -t nat -L -n` en el host para ver las reglas NAT de libvirt.

4. (Avanzado) Verifica la tabla NAT activa:
   ```bash
   conntrack -L 2>/dev/null | grep 192.168.122
   # Muestra conexiones NAT activas
   ```

## Resumen

| Concepto | Descripción |
|---|---|
| NAT | Traduce IPs privadas a públicas (y viceversa) en el gateway |
| SNAT | Cambia la IP/port origen (para salir a internet) |
| DNAT | Cambia la IP/port destino (para recibir tráfico entrante) |
| MASQUERADE | SNAT automático usando la IP del gateway |
| libvirt NAT | `<forward mode='nat'/>` en el XML de red |
| Limitación | VMs con NAT no son visibles desde internet |
