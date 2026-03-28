# T01 — Tipos de Redes en libvirt

## Redes en libvirt

libvirt permite crear **redes virtuales** que definen cómo las VMs se comunican entre sí y con el exterior. Cada red tiene un tipo que determina su comportamiento.

## Los 4 Tipos de Redes

### 1. NAT (mode='nat') — Por Defecto

La red más común. Las VMs acceden a internet a través del host, pero **no son visibles** desde el host ni desde internet.

```
[VM] ──→ [virbr0: NAT] ──→ [Host eth0] ──→ [Internet]
```

| Característica | Valor |
|---|---|
| Red por defecto | 192.168.122.0/24 |
| Gateway | 192.168.122.1 |
| DHCP range | 192.168.122.2 – .254 |
| VM → Host | ✅ (a través de NAT) |
| VM → Internet | ✅ |
| Host → VM | ❌ (no visible) |
| Internet → VM | ❌ |
| VMs entre sí | ✅ |

**Uso**: labs donde las VMs necesitan internet pero no necesitan ser accedidas desde fuera.

### 2. Aislada (mode='none' o forward mode='none')

Red **completamente aislada**. Las VMs pueden comunicarse entre sí, pero no tienen acceso a internet ni al host.

```
[VM1] ──┐
[VM2] ──┼──→ [virbr1: sin NAT]  (sin salida al exterior)
[VM3] ──┘
```

| Característica | Valor |
|---|---|
| VM → Host | ❌ |
| VM → Internet | ❌ |
| Host → VM | ❌ |
| VMs entre sí | ✅ |

**Uso en el curso**:
- Labs de **almacenamiento/RAID/LVM**: discos virtuales, no necesitan internet
- Labs de **firewall** (B11): simular entorno sin riesgo de afectar la red real
- Labs de **seguridad**: VMs comprometidas no pueden atacar el exterior

### 3. Bridge (mode='bridge' o forwarded + bridge)

Las VMs se conectan directamente a un **bridge del host**, apareciendo en la red física como máquinas físicas.

```
[VM] ──→ [virbr0 (bridge)] ──→ [Switch físico] ──→ [Router]
```

| Característica | Valor |
|---|---|
| VM → Host | ✅ |
| VM → Internet | ✅ |
| Host → VM | ✅ |
| Internet → VM | ✅ (si el router hace port forwarding) |
| VMs entre sí | ✅ |
| VMs visibles desde la red | ✅ |

**Uso**: cuando necesitas que las VMs sean accesibles como cualquier máquina de la red.

### 4. Redirección de Puertos (Port Forwarding)

No es un tipo de red diferente, sino una **configuración adicional** sobre NAT o Bridge.

```
[Internet] → [Host:2222] → [VM:22]
```

Se configura con reglas DNAT en el host:

```bash
# Ejemplo con iptables (host)
iptables -t nat -A PREROUTING -p tcp --dport 2222 \
  -j DNAT --to-destination 192.168.122.101:22
```

Útil para SSH a VMs sin bridge:

```bash
ssh -p 2222 user@IP-del-host
```

## Comparación Visual

```
                    ┌─────────────────────────────────────────────┐
                    │ NAT                     AISLADA            │
                    │                                                 │
  Internet ──╱     │    Internet ──╱        Internet ──╱          │
    ──╱            │      ──╱              ──╱                     │
                    │                                                 │
                    │  [VM1]                  [VM1]                  │
                    │    │                      │                   │
                    │    ↓                      │ (sin salida)      │
                    │  [virbr0] ──→ Host        [VM2]                │
                    │                         VMs se ven           │
                    │  VMs ↓ sí               entre sí               │
                    │  ven internet                                │
                    │                        VMs NO ven             │
                    │  VMs ven al host       internet               │
                    │  (↓ NAT)               Host                    │
                    │                                                 │
                    └─────────────────────────────────────────────┘

                    ┌─────────────────────────────────────────────┐
                    │ BRIDGE                                     │
                    │                                             │
  Internet ────────→│──── Switch físico ───→ [VM]                │
                    │                    │                        │
  Host ────────────→│──── br0 ──────────┘                        │
                    │                                             │
                    │  VM = máquina física en la red              │
                    │  Todos la ven                               │
                    └─────────────────────────────────────────────┘
```

## En el XML de libvirt

```xml
<!-- NAT -->
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

<!-- Aislada -->
<network>
  <name>isolated</name>
  <forward mode='none'/>
  <bridge name='virbr1' stp='on' delay='0'/>
  <ip address='10.0.1.1' netmask='255.255.255.0'>
    <dhcp>
      <range start='10.0.1.2' end='10.0.1.254'/>
    </dhcp>
  </ip>
</network>

<!-- Bridge externo -->
<network>
  <name>host-bridge</name>
  <forward mode='bridge'/>
  <bridge name='br0'/>
</network>
```

## Tabla Resumen para el Curso

| Bloque | Tipo de red | Por qué |
|---|---|---|
| B08 (Almacenamiento) | Aislada | Labs con discos, sin necesidad de internet |
| B09 (Redes) | Aislada + NAT | Simular topologías sin riesgo |
| B10 (Servicios) | Aislada + NAT | Servidor DNS/DHCP en red aislada |
| B11 (Seguridad) | Aislada | Labs de firewall, VMs comprometidas no escapan |
| C00 (QEMU) | NAT + Aislada | Setup inicial de labs |

## Ejercicios

1. Ver las redes disponibles en tu sistema:
   ```bash
   virsh net-list --all
   virsh net-info default
   ```

2. Para la red default, responder:
   - ¿Qué subred usa?
   - ¿El DHCP está activo?
   - ¿Cuántos leases hay actualmente?

3. En el syllabus de C00, sección S07 (Almacenamiento Virtual),
   se mencionan discos extra. ¿Qué tipo de red se recomienda
   para esos labs y por qué?

4. Investiga: si quisieras que una VM en NAT fuera accesible desde
   el host (pero no desde internet), ¿qué opciones hay?
   (Pista: busca "ssh reverse tunnel", "virt-viewer", "SPICE")

## Resumen

| Tipo | modo='...' | VMs → Internet | Host → VM | VMs entre sí |
|---|---|---|---|---|
| NAT | nat | ✅ | ❌ | ✅ |
| Aislada | none | ❌ | ❌ | ✅ |
| Bridge | bridge | ✅ | ✅ | ✅ |
| Port forwarding | (sobre NAT/bridge) | ✅ | ✅ (via puerto) | ✅ |

La **aislada** y la **NAT** son las más usadas en labs. Bridge se usa cuando la VM debe ser una máquina "de primera clase" en la red.
