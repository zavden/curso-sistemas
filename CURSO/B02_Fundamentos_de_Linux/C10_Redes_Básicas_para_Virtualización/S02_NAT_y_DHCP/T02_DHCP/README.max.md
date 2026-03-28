# T02 — DHCP (Dynamic Host Configuration Protocol)

## El problema

En una red con 100 máquinas, ¿te imaginas configurar manualmente la IP de cada una?

```
IP: 192.168.1.1, máscara: 255.255.255.0, gateway: 192.168.1.1, DNS: 8.8.8.8
IP: 192.168.1.2, máscara: 255.255.255.0, gateway: 192.168.1.1, DNS: 8.8.8.8
IP: 192.168.1.3, máscara: 255.255.255.0, gateway: 192.168.1.1, DNS: 8.8.8.8
... 97 líneas más
```

Peor: si cambias el gateway o el DNS, tendrías que actualizar 100 máquinas manualmente.

## La solución: DHCP

DHCP asigna IPs, máscara, gateway y DNS **automáticamente** a cada dispositivo que se conecta a la red.

## El Proceso DORA

DHCP funciona en 4 pasos:

```
[A] Cliente DHCP (tu laptop)          [B] Servidor DHCP (tu router)
       |                                      |
       │  1. DISCOVER (broadcast)            |
       |──────────────────────────────────────>
       |                                      |
       |  2. OFFER (propuesta de IP)          |
       |<──────────────────────────────────────
       |                                      |
       |  3. REQUEST (acepto la IP)           |
       |──────────────────────────────────────>
       |                                      |
       |  4. ACK (confirmación)               |
       |<──────────────────────────────────────
       |                                      |
       |  ✓ Cliente configura su IP           |
```

### Paso a paso

1. **DISCOVER**: El cliente broadcast "¿hay algún servidor DHCP?"
2. **OFFER**: El servidor ofrece una IP disponible (ej: 192.168.1.105)
3. **REQUEST**: El cliente broadcast "acepto la IP 192.168.1.105"
4. **ACK**: El servidor confirma y dice cuánto tiempo es válido el "alquiler"

## El Lease (Alquiler)

Las IPs DHCP no son permanentes — se "alquilan" por un período (típicamente 24 horas).

```
Lease time: 86400 segundos (24 horas)
```

| Tiempo | Qué pasa |
|--------|---------|
| 50% del lease | El cliente intenta **renovar** la misma IP |
| 87.5% del lease | Si renovación falla, busca otro servidor DHCP |
| Lease expira | El cliente debe pedir otra IP o dejar la red |

Para ver tus leases DHCP actuales:

```bash
# Linux (lease de NetworkManager)
cat /var/lib/NetworkManager/dhclient-*.lease

# O verificar la configuración actual
ip addr show eth0
```

## Renovación de Lease

```bash
# Liberar (devolver la IP)
sudo dhclient -r

# Renovar (pedir una nueva)
sudo dhclient

# O ambas en secuencia
sudo dhclient -r && sudo dhclient
```

## En QEMU/KVM / libvirt

libvirt incluye un **servidor DHCP integrado** para cada red NAT o aislada.

```bash
# Ver los leases DHCP de la red default de libvirt
virsh net-dhcp-leases default
```

Salida ejemplo:

```
Expiry Time          MAC address        Protocol  IP address        Hostname   Client ID
-----------------------------------------------------------------------------------------------
2024-01-15 14:32:15  52:54:00:12:34:56  ipv4      192.168.122.101   debian-lab  -
```

### Configuración DHCP de libvirt

En el XML de la red:

```xml
<ip address='192.168.122.1' netmask='255.255.255.0'>
  <dhcp>
    <range start='192.168.122.2' end='192.168.122.254'/>
    <host mac='52:54:00:12:34:56' name='debian-lab' ip='192.168.122.101'/>
  </dhcp>
</ip>
```

- `<range>`: rango de IPs dinámicas
- `<host>`: IP fija asignada a una MAC específica (DHCP reservation)

## Reservar una IP Fija por MAC

Si quieres que una VM siempre tenga la misma IP:

```bash
# Obtener la MAC de la VM
virsh dumpxml debian-lab | grep mac

# Añadir reserva en el XML de la red
virsh net-edit default
```

Añadir dentro de `<dhcp>`:

```xml
<host mac='52:54:00:12:34:56' ip='192.168.122.50'/>
```

Esto garantiza que `debian-lab` siempre obtenga `192.168.122.50`.

## DHCP y el Gateway

El servidor DHCP también entrega:
- **Gateway** (default route) — generalmente `.1` de la subred
- **DNS** — IPs de servidores DNS
- **máscara de subred**

```bash
# Ver qué entregó el DHCP
cat /etc/resolv.conf
# nameserver 192.168.1.1
# (el gateway es también DNS en la mayoría de redes domésticas)
```

## Ejercicios

1. Ejecuta en tu host:
   ```bash
   # ¿Cuántos leases DHCP tienes activos?
   cat /var/lib/NetworkManager/dhclient-*.lease 2>/dev/null || \
   cat /var/lib/dhclient/dhclient.leases 2>/dev/null
   ```

2. Verifica que DHCP funciona en tu red:
   ```bash
   # Ver configuración actual (¿fuente DHCP?)
   nmcli device show eth0 | grep DHCP
   
   # Hacer release y renew
   sudo dhclient -r && sleep 1 && dhclient
   ip addr show eth0
   ```

3. En QEMU/KVM:
   ```bash
   virsh net-dhcp-leases default
   # ¿Hay alguna VM corriendo? ¿Qué IP tiene?
   ```

4. (Avanzado) Simular el proceso DORA:
   ```bash
   # Capturar los paquetes DHCP con tcpdump
   sudo tcpdump -i eth0 -n port 67 or port 68
   # En otra terminal: sudo dhclient -r && dhclient
   # Observa los 4 pasos de DORA
   ```

5. Investiga: ¿por qué a veces después de conectar a un WiFi nuevo,
   la primera conexión a internet tarda unos segundos más?

## Resumen

| Concepto | Descripción |
|---|---|
| DHCP | Asigna IPs automáticamente (no es solo IP, también gateway/DNS) |
| DORA | Discover → Offer → Request → Acknowledge |
| Lease | Tiempo que la IP está "alquilada" (típicamente 24h) |
| Renew | El cliente intenta renovar antes de que expire el lease |
| libvirt DHCP | Integrado en libvirt para redes NAT/aisladas |
| Reserva por MAC | IP fija garantizada para una VM específica |
