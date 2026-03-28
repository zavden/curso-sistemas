# T01 — Concepto de Bridge

## Qué es un Bridge (Puente)

Un **bridge** (puente) es un dispositivo (o software) que conecta **dos redes** y las hace funcionar como si fueran una sola, operando en la **Capa 2** (Enlace de datos / Ethernet) del modelo OSI.

Cuando un bridge recibe una trama Ethernet, mira la dirección MAC destino y decide:
- Si el destino está en **la misma red** (del mismo lado del bridge), **no reenvía**
- Si el destino está en **la otra red**, **reenvía**

Esto reduce tráfico innecesario: las tramas solo cruzan el bridge cuando es necesario.

## Bridge vs Switch vs Router

| Dispositivo | Capa OSI | Qué hace |
|---|---|---|
| **Hub** | Capa 1 | Reenvía a todos los puertos (broadcast) |
| **Bridge** | Capa 2 | Aprende MACs, reenvía selectivamente |
| **Switch** | Capa 2 | Bridge con muchos puertos (moderno) |
| **Router** | Capa 3 | Conecta redes diferentes (ej: red 192.168.1.0 con 10.0.0.0) |

En la práctica, **switch y bridge son sinónimos** en redes modernas. El término "switch" se usa para dispositivos con muchos puertos, "bridge" para conexiones punto a punto o implementaciones software.

## Ejemplo Físico: Puente entre Dos Segmentos

```
Segmento A (red 1)          BRIDGE           Segmento B (red 2)
┌─────────┐                  ┌────┐            ┌─────────┐
│  MAQ A  │MAC: AA:AA        │    │       MAC: CC:CC
│  MAC:AA:AA                 │    │──────→ MAQ C
└─────────┘                  │    │       
┌─────────┐                  │    │──────→ MAQ D (MAC:DD:DD)
│  MAQ B  │MAC: BB:BB        │    │
└─────────┘                  └────┘
```

Tabla MAC aprendida por el bridge:

| MAC | Puerto |
|-----|--------|
| AA:AA | Segmento A |
| BB:BB | Segmento A |
| CC:CC | Segmento B |
| DD:DD | Segmento B |

## Bridge en Virtualización

En QEMU/KVM, el bridge se usa para que las VMs sean **visibles en la red física** del host, como si fueran máquinas físicas conectadas al mismo switch.

### Sin Bridge (NAT)

```
[VM] ──→ [Bridge virbr0 (NAT)] ──→ [Host] ──→ [Internet]
```

La VM está "oculta" detrás del host. Internet ve solo la IP del host.

### Con Bridge

```
[VM] ──→ [Bridge br0] ──→ [Switch físico] ──→ [Router] ──→ [Internet]
```

La VM tiene su propia IP en la red del router. Los demás dispositivos de la red la ven como una máquina más.

```
┌────────────────────────────────────────────────────────────────┐
│ HOST Linux                                                     │
│                                                                │
│  ┌──────────┐     ┌──────────┐                                 │
│  │  VM 1     │     │  VM 2    │                                │
│  │ eth0      │     │ eth0      │                                │
│  │ 192.168.1.10│     │ 192.168.1.11│                                │
│  └────┬─────┘     └────┬─────┘                                │
│       │                │                                        │
│       ↓                ↓                                        │
│  ┌─────────────────────────────────┐                            │
│  │         Bridge: br0            │                            │
│  │  (funciona como switch virtual) │                            │
│  └───────────────┬─────────────────┘                            │
│                  │                                               │
│                  ↓                                               │
│            [eth0 física del host]                                │
│            192.168.1.100                                          │
│                  │                                               │
│                  ↓                                               │
│            [Router/Switch de la red física]                    │
│            192.168.1.1                                            │
└────────────────────────────────────────────────────────────────┘
```

## Bridge vs NAT en QEMU/KVM

| Característica | NAT (default) | Bridge (br0) |
|---|---|---|
| La VM es visible desde la red | ❌ | ✅ |
| La VM tiene IP en la red real | ❌ | ✅ |
| Funciona sin configurar red | ✅ | ❌ ( requiere setup del host) |
| VMs aisladas entre sí | No (pueden verse) | No (están en la misma red) |
| Para labs de red | Suficiente | Necesario para casos complejos |

## Cuándo usar Bridge

- Cuando necesitas **acceder a la VM desde otro dispositivo** de la red
- Cuando la VM debe提供服务 (web server, SSH server) **visible para otros**
- Cuando estás estudiando **redes con múltiples subnets** y routing real

## Cuándo NO necesitas Bridge

- Labs de almacenamiento, particiones, LVM
- VMs de prueba con internet (NAT es suficiente)
- Cuando solo accedes a la VM desde el host (`virsh console`, `virt-viewer`)

## Ejercicios

1. Dibuja la topología de tu red doméstica:
   - ¿Tu laptop está en NAT, bridge, o directamente conectada al router?
   - ¿Cuántos dispositivos hay en tu red local?

2. Investiga: ¿qué tipo de conexión usa Docker por defecto (bridge o host)?

3. Investiga: si tienes un router WiFi con 4 puertos Ethernet,
   ¿cuántos bridges tiene internamente?

4. Reflexiona: ¿por qué en una red WiFi es más difícil hacer bridge que en una red cableada?

## Resumen

| Concepto | Descripción |
|---|---|
| Bridge | Conecta dos segmentos de red en Capa 2 (Ethernet) |
| Switch | Bridge moderno con múltiples puertos |
| Bridge en Linux | Convierte al host en un switch virtual |
| Bridge en QEMU | VMs visibles en la red física del host |
| Bridge vs NAT | Bridge = VMs en la red real; NAT = VMs ocultas tras el host |
