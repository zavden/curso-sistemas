# firewalld — Zonas

## Índice

1. [Qué es firewalld](#1-qué-es-firewalld)
2. [Arquitectura de firewalld](#2-arquitectura-de-firewalld)
3. [El modelo de zonas](#3-el-modelo-de-zonas)
4. [Zonas predefinidas](#4-zonas-predefinidas)
5. [Asignación de interfaces a zonas](#5-asignación-de-interfaces-a-zonas)
6. [Asignación por origen (source)](#6-asignación-por-origen-source)
7. [Zona por defecto](#7-zona-por-defecto)
8. [Servicios en zonas](#8-servicios-en-zonas)
9. [Puertos directos en zonas](#9-puertos-directos-en-zonas)
10. [Runtime vs permanent](#10-runtime-vs-permanent)
11. [Archivos de configuración](#11-archivos-de-configuración)
12. [Diseño de zonas para escenarios reales](#12-diseño-de-zonas-para-escenarios-reales)
13. [Errores comunes](#13-errores-comunes)
14. [Cheatsheet](#14-cheatsheet)
15. [Ejercicios](#15-ejercicios)

---

## 1. Qué es firewalld

firewalld es un **frontend de gestión de firewall** que genera reglas nftables (o
iptables en sistemas antiguos) a partir de un modelo de alto nivel basado en
**zonas** y **servicios**. No es un firewall en sí mismo — el firewall real sigue
siendo Netfilter en el kernel.

### ¿Por qué otro nivel de abstracción?

```
Sin firewalld:
  Administrador → escribe reglas nft/iptables → kernel Netfilter
  - Necesitas conocer sintaxis de bajo nivel
  - Cambios dinámicos requieren scripts complejos
  - No hay concepto de "perfil" para distintas redes

Con firewalld:
  Administrador → firewall-cmd (zonas/servicios) → firewalld daemon
                                                    → genera reglas nft → kernel
  - Modelo declarativo: "esta interfaz está en zona X"
  - Cambios en caliente sin interrumpir conexiones existentes
  - NetworkManager puede cambiar la zona al conectar a otra red
```

### Dónde se usa

| Distribución | Estado de firewalld |
|-------------|-------------------|
| RHEL / CentOS / AlmaLinux / Rocky | **Por defecto** desde RHEL 7 |
| Fedora | **Por defecto** |
| SUSE / openSUSE | Disponible, SuSEfirewall2 → migrado a firewalld |
| Debian / Ubuntu | Disponible en repos, **no** instalado por defecto |
| Arch Linux | Disponible en repos |

En el ecosistema Red Hat, firewalld es la herramienta estándar. En Debian/Ubuntu
es más común usar nftables directamente o ufw (Uncomplicated Firewall).

---

## 2. Arquitectura de firewalld

```
┌──────────────────────────────────────────────────────────┐
│                    Interfaces de usuario                 │
│                                                          │
│  ┌──────────────┐  ┌──────────┐  ┌────────────────────┐  │
│  │ firewall-cmd │  │firewall- │  │ cockpit/webUI      │  │
│  │   (CLI)      │  │config    │  │ (navegador)        │  │
│  │              │  │  (GUI)   │  │                    │  │
│  └──────┬───────┘  └────┬─────┘  └─────────┬──────────┘  │
│         │               │                  │             │
│         └───────────────┼──────────────────┘             │
│                         │                                │
│                      D-Bus                               │
│              (org.fedoraproject.FirewallD1)               │
│                         │                                │
│              ┌──────────┴──────────┐                     │
│              │     firewalld       │                     │
│              │     (daemon)        │                     │
│              │                     │                     │
│              │  ┌───────────────┐  │                     │
│              │  │ Runtime config│  │  ← en memoria       │
│              │  └───────────────┘  │                     │
│              │  ┌───────────────┐  │                     │
│              │  │Permanent config│ │  ← archivos XML     │
│              │  └───────────────┘  │                     │
│              └──────────┬──────────┘                     │
│                         │                                │
│                    nftables API                          │
│                   (o iptables)                            │
└─────────────────────────┼────────────────────────────────┘
                          │
              ┌───────────┴───────────┐
              │  Kernel: nf_tables    │
              │  (Netfilter hooks)    │
              └───────────────────────┘
```

### Componentes clave

- **firewalld** (daemon): proceso que corre como servicio systemd, mantiene la
  configuración y genera las reglas de bajo nivel
- **firewall-cmd**: CLI para interactuar con el daemon vía D-Bus
- **firewall-config**: GUI (paquete separado, no siempre instalado)
- **D-Bus**: canal de comunicación — cualquier aplicación puede consultar o
  modificar el firewall si tiene permisos

### Backend: nftables vs iptables

```bash
# Ver qué backend usa firewalld
grep FirewallBackend /etc/firewalld/firewalld.conf
# FirewallBackend=nftables   ← moderno (RHEL 8+, Fedora 28+)
# FirewallBackend=iptables   ← legacy (RHEL 7)
```

Con el backend nftables, firewalld crea tablas con nombres como `firewalld` en
el espacio nftables. Puedes verlas con `nft list ruleset`, pero **no debes
modificarlas directamente** — firewalld las sobreescribe al recargar.

---

## 3. El modelo de zonas

Una **zona** representa un nivel de confianza para un segmento de red. Cada zona
define qué tráfico se permite entrante (incoming), y todo lo que no está
explícitamente permitido se deniega por defecto.

### Concepto fundamental

```
                    Nivel de confianza
    ◄─────────────────────────────────────────────────────►
    MÍNIMO                                           MÁXIMO

    drop    block    public    external    dmz    work    home    internal    trusted
    │         │        │          │        │       │       │         │          │
    │         │        │          │        │       │       │         │          │
    Descarta  Rechaza  Solo      NAT +    Solo    SSH +   SSH +     Como       Todo
    todo      todo     SSH +     SSH      SSH     mDNS    mDNS +    home      permitido
              (ICMP    DHCPv6                     DHCP    Samba +
              reject)                                     DHCP
```

### Cómo decide firewalld qué zona aplicar a un paquete

El orden de evaluación es determinista:

```
Paquete llega
     │
     ▼
¿Hay una zona asignada por IP origen (source)?
     │
     ├── SÍ → usar esa zona
     │
     └── NO
          │
          ▼
     ¿Hay una zona asignada a la interfaz de entrada?
          │
          ├── SÍ → usar esa zona
          │
          └── NO → usar la zona por defecto
```

**Predicción**: esto significa que una regla por source **tiene prioridad** sobre
la asignación por interfaz. Un paquete desde una IP específica puede evaluarse
en una zona diferente a la de su interfaz.

---

## 4. Zonas predefinidas

firewalld incluye zonas predefinidas ordenadas de menor a mayor confianza:

### drop

```
Nivel de confianza: mínimo absoluto
Comportamiento: todo paquete entrante se descarta silenciosamente (DROP)
                Solo se permiten conexiones salientes
Caso de uso: interfaz expuesta a red hostil, honeypot
```

No envía ni ICMP de rechazo — el origen no recibe ninguna respuesta.

### block

```
Nivel de confianza: muy bajo
Comportamiento: todo paquete entrante se rechaza con icmp-host-prohibited
                (o icmp6-adm-prohibited para IPv6)
Caso de uso: similar a drop pero "más educado" — el origen sabe que fue rechazado
```

### public (zona por defecto en la mayoría de instalaciones)

```
Nivel de confianza: bajo
Servicios permitidos: ssh, dhcpv6-client
Caso de uso: redes no confiables, WiFi público, data centers
```

Esta es la zona asignada a interfaces nuevas si no se configura otra cosa.

### external

```
Nivel de confianza: bajo
Servicios permitidos: ssh
Característica especial: masquerading (SNAT) habilitado
Caso de uso: interfaz WAN de un router — la LAN se enmascara tras esta interfaz
```

### dmz

```
Nivel de confianza: bajo-medio
Servicios permitidos: ssh
Caso de uso: servidores en zona desmilitarizada, acceso limitado desde Internet
```

### work

```
Nivel de confianza: medio
Servicios permitidos: ssh, dhcpv6-client
Caso de uso: red corporativa con cierto nivel de confianza
```

### home

```
Nivel de confianza: medio-alto
Servicios permitidos: ssh, mdns, samba-client, dhcpv6-client
Caso de uso: red doméstica, compartir archivos con otros dispositivos
```

### internal

```
Nivel de confianza: medio-alto
Servicios permitidos: ssh, mdns, samba-client, dhcpv6-client
Caso de uso: interfaz LAN del router, redes internas de oficina
```

Mismos servicios que `home` — la diferencia es semántica (nombre para distinguir
la LAN interna de un router).

### trusted

```
Nivel de confianza: máximo
Comportamiento: TODO el tráfico se acepta
Caso de uso: interfaz de loopback, redes de gestión aisladas,
             túneles VPN de plena confianza
```

**Advertencia**: asignar una interfaz pública a `trusted` deshabilita el firewall
para esa interfaz. Úsala solo para redes que controlas completamente.

### Resumen comparativo

| Zona | ssh | dhcpv6 | mdns | samba | masq | Entrada default |
|------|-----|--------|------|-------|------|-----------------|
| drop | — | — | — | — | — | DROP silencioso |
| block | — | — | — | — | — | REJECT con ICMP |
| public | ✓ | ✓ | — | — | — | REJECT |
| external | ✓ | — | — | — | **✓** | REJECT |
| dmz | ✓ | — | — | — | — | REJECT |
| work | ✓ | ✓ | — | — | — | REJECT |
| home | ✓ | ✓ | ✓ | ✓ | — | REJECT |
| internal | ✓ | ✓ | ✓ | ✓ | — | REJECT |
| trusted | ✓ | ✓ | ✓ | ✓ | — | ACCEPT todo |

---

## 5. Asignación de interfaces a zonas

Cada interfaz de red pertenece a exactamente una zona. Si no se asigna
explícitamente, usa la zona por defecto.

### Ver asignaciones actuales

```bash
# Ver todas las zonas activas (con interfaces asignadas)
firewall-cmd --get-active-zones
# public
#   interfaces: eth0
# internal
#   interfaces: eth1

# Ver la zona de una interfaz específica
firewall-cmd --get-zone-of-interface=eth0
# public
```

### Asignar interfaz a zona

```bash
# Cambiar la zona de eth0 (solo runtime)
firewall-cmd --zone=internal --change-interface=eth0

# Cambiar la zona de eth0 (permanente)
firewall-cmd --zone=internal --change-interface=eth0 --permanent
firewall-cmd --reload

# Añadir interfaz a zona (si no está en ninguna)
firewall-cmd --zone=dmz --add-interface=eth2 --permanent
firewall-cmd --reload

# Eliminar interfaz de una zona
firewall-cmd --zone=dmz --remove-interface=eth2 --permanent
```

### Integración con NetworkManager

NetworkManager puede asignar la zona automáticamente por perfil de conexión:

```bash
# Ver la zona de una conexión NetworkManager
nmcli connection show "Wired connection 1" | grep zone
# connection.zone:  public

# Asignar zona a una conexión
nmcli connection modify "Wired connection 1" connection.zone internal
nmcli connection up "Wired connection 1"

# La zona se aplica automáticamente cuando la conexión se activa
```

Esto es especialmente útil en laptops: al conectar a la red corporativa se
aplica la zona `work`, al conectar al WiFi de casa se aplica `home`, y al
conectar a un WiFi público se aplica `public`.

```bash
# Flujo automático con NetworkManager:
# 1. Laptop se conecta a WiFi "CafePublico"
# 2. NetworkManager activa el perfil con zone=public
# 3. NetworkManager notifica a firewalld vía D-Bus
# 4. firewalld aplica las reglas de la zona public a wlan0
# → Solo SSH y DHCPv6 están permitidos
```

---

## 6. Asignación por origen (source)

Además de asignar interfaces completas, puedes asignar **rangos de IP** a zonas
específicas. Esto permite reglas más granulares.

### Añadir source a zona

```bash
# Las IPs de la red de gestión van a zona trusted
firewall-cmd --zone=trusted --add-source=10.255.0.0/24 --permanent

# Una IP específica del equipo de monitorización
firewall-cmd --zone=internal --add-source=192.168.1.50 --permanent

firewall-cmd --reload
```

### Ver sources activos

```bash
firewall-cmd --get-active-zones
# trusted
#   sources: 10.255.0.0/24
# internal
#   sources: 192.168.1.50
#   interfaces: eth1
# public
#   interfaces: eth0
```

### Prioridad source vs interface

```bash
# Escenario:
# - eth0 está en zona "public" (solo SSH)
# - Source 10.255.0.0/24 está en zona "trusted" (todo permitido)

# Paquete desde 10.255.0.5 que entra por eth0:
# → Se evalúa en zona "trusted" (source tiene prioridad)
# → Todo el tráfico aceptado ✓

# Paquete desde 203.0.113.50 que entra por eth0:
# → No coincide con ningún source
# → Se evalúa en zona "public" (por interfaz)
# → Solo SSH aceptado
```

### Caso de uso: red de administración

```bash
# Interfaz pública: muy restrictiva
firewall-cmd --zone=public --add-interface=eth0 --permanent

# Red de administración (source): acceso total
firewall-cmd --zone=trusted --add-source=10.255.0.0/24 --permanent

# Red de monitorización (source): acceso a SNMP y HTTP
firewall-cmd --zone=work --add-source=10.254.0.0/24 --permanent
firewall-cmd --zone=work --add-service=snmp --permanent
firewall-cmd --zone=work --add-service=http --permanent

firewall-cmd --reload
```

---

## 7. Zona por defecto

La zona por defecto se aplica a cualquier interfaz que no tenga asignación
explícita y a cualquier paquete cuyo origen no coincida con ningún source.

```bash
# Ver la zona por defecto
firewall-cmd --get-default-zone
# public

# Cambiar la zona por defecto
firewall-cmd --set-default-zone=drop
# Esto aplica inmediatamente (no necesita --permanent ni --reload)
# Todas las interfaces sin zona explícita cambian a "drop"
```

**Predicción**: cambiar la zona por defecto a `drop` puede dejarte fuera de una
sesión SSH si la interfaz por la que conectas no tiene zona explícita asignada.
Siempre asigna la interfaz a una zona con SSH antes de cambiar la zona por
defecto.

### Buena práctica para servidores

```bash
# 1. Primero, asignar la interfaz de gestión a una zona segura
firewall-cmd --zone=internal --add-interface=eth0 --permanent
firewall-cmd --zone=internal --add-service=ssh --permanent
firewall-cmd --reload

# 2. Verificar que SSH funciona por la zona internal
firewall-cmd --zone=internal --list-all

# 3. Ahora sí, cambiar la zona por defecto a drop
firewall-cmd --set-default-zone=drop

# Resultado: cualquier interfaz nueva entra en drop por defecto
# pero eth0 sigue en internal con SSH permitido
```

---

## 8. Servicios en zonas

Un **servicio** en firewalld es una abstracción que agrupa puertos, protocolos
y módulos asociados a una aplicación. En lugar de recordar "SSH es TCP/22",
simplemente añades el servicio `ssh`.

### Listar servicios disponibles

```bash
# Todos los servicios predefinidos
firewall-cmd --get-services
# RH-Satellite-6 RH-Satellite-6-capsule ... dhcp dhcpv6 dhcpv6-client dns
# docker-registry ftp ... http https ... ssh ... wireguard ...

# Hay ~70 servicios predefinidos en la instalación estándar
```

### Ver qué incluye un servicio

```bash
# Detalle de un servicio
firewall-cmd --info-service=ssh
# ssh
#   ports: 22/tcp
#   protocols:
#   source-ports:
#   modules:
#   destination:

firewall-cmd --info-service=samba
# samba
#   ports: 137/udp 138/udp 139/tcp 445/tcp
#   protocols:
#   source-ports:
#   modules: netbios-ns
#   destination:
```

### Gestionar servicios en zonas

```bash
# Ver servicios actuales de una zona
firewall-cmd --zone=public --list-services
# dhcpv6-client ssh

# Añadir servicio
firewall-cmd --zone=public --add-service=http --permanent
firewall-cmd --zone=public --add-service=https --permanent

# Eliminar servicio
firewall-cmd --zone=public --remove-service=dhcpv6-client --permanent

# Aplicar cambios permanentes
firewall-cmd --reload

# Verificar
firewall-cmd --zone=public --list-services
# http https ssh
```

### Crear servicios personalizados

Los servicios predefinidos están en `/usr/lib/firewalld/services/`. Para crear
uno personalizado:

```bash
# Método 1: desde línea de comandos (RHEL 8+ / firewalld 0.7+)
firewall-cmd --permanent --new-service=myapp
firewall-cmd --permanent --service=myapp --set-description="Mi aplicación web"
firewall-cmd --permanent --service=myapp --add-port=8080/tcp
firewall-cmd --permanent --service=myapp --add-port=8443/tcp
firewall-cmd --reload

# Verificar
firewall-cmd --info-service=myapp

# Usar el servicio
firewall-cmd --zone=public --add-service=myapp --permanent
firewall-cmd --reload
```

```bash
# Método 2: crear archivo XML directamente
cat > /etc/firewalld/services/myapp.xml << 'EOF'
<?xml version="1.0" encoding="utf-8"?>
<service>
  <short>MyApp</short>
  <description>Mi aplicación web personalizada</description>
  <port protocol="tcp" port="8080"/>
  <port protocol="tcp" port="8443"/>
</service>
EOF

firewall-cmd --reload
```

Los servicios personalizados van en `/etc/firewalld/services/` (no en `/usr/lib/`
que se sobreescribe con actualizaciones).

---

## 9. Puertos directos en zonas

A veces necesitas abrir un puerto que no corresponde a ningún servicio predefinido
y no quieres crear un servicio personalizado.

```bash
# Abrir un puerto TCP
firewall-cmd --zone=public --add-port=3000/tcp --permanent

# Abrir un rango de puertos
firewall-cmd --zone=public --add-port=8000-8100/tcp --permanent

# Abrir un puerto UDP
firewall-cmd --zone=public --add-port=5060/udp --permanent

# Ver puertos abiertos
firewall-cmd --zone=public --list-ports
# 3000/tcp 8000-8100/tcp 5060/udp

# Eliminar
firewall-cmd --zone=public --remove-port=3000/tcp --permanent

firewall-cmd --reload
```

### Puertos vs servicios: cuándo usar cada uno

```
Servicio (--add-service):
  ✓ Cuando la aplicación tiene un servicio predefinido
  ✓ Cuando el servicio tiene múltiples puertos (ej. Samba: 4 puertos)
  ✓ Cuando quieres documentar QUÉ está permitido (nombre descriptivo)
  ✓ Cuando necesitas módulos helper (FTP, SIP)

Puerto directo (--add-port):
  ✓ Cuando es un puerto puntual y temporal
  ✓ Cuando no quieres crear un archivo de servicio
  ✗ Menos legible al auditar ("¿qué es el puerto 8443?")
```

---

## 10. Runtime vs permanent

firewalld mantiene **dos configuraciones independientes**:

```
┌─────────────────┐        ┌──────────────────┐
│  Runtime config  │        │ Permanent config  │
│  (en memoria)    │        │  (archivos XML)   │
│                  │        │                   │
│ Activa ahora     │        │ Se carga al       │
│ Se pierde al     │        │ arrancar o con    │
│ reiniciar        │        │ --reload          │
└─────────────────┘        └──────────────────┘
```

### Modos de operación

```bash
# Solo runtime (efecto inmediato, se pierde al reiniciar)
firewall-cmd --zone=public --add-service=http
# → Aplica AHORA, no se guarda en disco

# Solo permanent (se guarda, pero NO se aplica hasta reload)
firewall-cmd --zone=public --add-service=http --permanent
# → Se guarda en XML, pero NO está activo todavía

# Para que surta efecto permanente:
firewall-cmd --reload
# → Descarta runtime y carga permanent

# Runtime + permanent en un solo comando (firewalld 0.9+, RHEL 9+)
firewall-cmd --zone=public --add-service=http --permanent --runtime-to-permanent
```

### Flujo recomendado para cambios de producción

```bash
# 1. Hacer cambios en runtime (prueba inmediata)
firewall-cmd --zone=public --add-service=https

# 2. Verificar que funciona
curl -k https://localhost/    # ¿responde?
firewall-cmd --zone=public --list-all

# 3. Si funciona, convertir runtime a permanent
firewall-cmd --runtime-to-permanent

# Si no funciona: simplemente recargar para descartar cambios
firewall-cmd --reload
# → Vuelve al estado permanent anterior
```

**Predicción**: el error más frecuente es usar `--permanent` sin `--reload`,
creyendo que el cambio ya está activo. Las conexiones fallan, el administrador
piensa que la regla no funciona y añade más reglas — cuando el problema es que
nunca se recargó.

### Ver diferencias entre runtime y permanent

```bash
# Listar la configuración runtime
firewall-cmd --zone=public --list-all

# Listar la configuración permanent
firewall-cmd --zone=public --list-all --permanent

# Si difieren, hay cambios no guardados o no aplicados
```

---

## 11. Archivos de configuración

### Estructura de directorios

```
/usr/lib/firewalld/              ← Defaults del sistema (no modificar)
├── zones/                       ← Zonas predefinidas
│   ├── public.xml
│   ├── drop.xml
│   ├── trusted.xml
│   └── ...
├── services/                    ← Servicios predefinidos
│   ├── ssh.xml
│   ├── http.xml
│   └── ...
├── icmptypes/                   ← Tipos ICMP
└── helpers/                     ← Helpers de conntrack

/etc/firewalld/                  ← Configuración personalizada (sobreescribe)
├── firewalld.conf               ← Configuración general del daemon
├── zones/                       ← Zonas personalizadas o modificadas
│   └── public.xml               ← Si existe, sobreescribe /usr/lib/...
├── services/                    ← Servicios personalizados
├── direct.xml                   ← Reglas directas (deprecated)
├── ipsets/                      ← Conjuntos de IPs
└── policies/                    ← Políticas entre zonas (firewalld 0.9+)
```

**Regla**: `/etc/firewalld/` tiene prioridad sobre `/usr/lib/firewalld/`. Al
modificar una zona, firewalld copia el XML de `/usr/lib/` a `/etc/` y aplica
los cambios sobre la copia.

### Formato XML de una zona

```xml
<!-- /etc/firewalld/zones/public.xml -->
<?xml version="1.0" encoding="utf-8"?>
<zone>
  <short>Public</short>
  <description>For use in public areas. You do not trust the other
  computers on networks to not harm your computer.</description>
  <service name="ssh"/>
  <service name="dhcpv6-client"/>
</zone>
```

Después de añadir servicios y puertos:

```xml
<?xml version="1.0" encoding="utf-8"?>
<zone>
  <short>Public</short>
  <description>For use in public areas.</description>
  <service name="ssh"/>
  <service name="http"/>
  <service name="https"/>
  <port protocol="tcp" port="8080"/>
  <source address="10.255.0.0/24"/>
</zone>
```

### firewalld.conf

```bash
# /etc/firewalld/firewalld.conf — opciones principales

DefaultZone=public           # zona por defecto
FirewallBackend=nftables     # backend: nftables o iptables
LogDenied=off                # logging de paquetes denegados
#   off | all | unicast | broadcast | multicast

FlushAllOnReload=yes         # flush al recargar
CleanupOnExit=yes            # limpiar reglas al detener firewalld
CleanupModulesOnExit=yes     # descargar módulos kernel al salir
```

```bash
# Activar logging de paquetes denegados (útil para diagnóstico)
# Vía CLI:
firewall-cmd --set-log-denied=all
# Los mensajes aparecen en journalctl -k con prefijo "FINAL_REJECT"

# Vía archivo:
# Editar /etc/firewalld/firewalld.conf → LogDenied=all
# systemctl restart firewalld
```

---

## 12. Diseño de zonas para escenarios reales

### Escenario 1: servidor web simple

```
Internet ── eth0 ── [Servidor Web]
```

```bash
# eth0 en zona public, abrir HTTP y HTTPS
firewall-cmd --zone=public --add-interface=eth0 --permanent
firewall-cmd --zone=public --add-service=http --permanent
firewall-cmd --zone=public --add-service=https --permanent
firewall-cmd --reload

firewall-cmd --zone=public --list-all
# public (active)
#   interfaces: eth0
#   services: dhcpv6-client http https ssh
```

### Escenario 2: router con dos zonas

```
Internet ── eth0 (external) ── [Router] ── eth1 (internal) ── LAN
```

```bash
# WAN: zona external (masquerading incluido)
firewall-cmd --zone=external --add-interface=eth0 --permanent

# LAN: zona internal
firewall-cmd --zone=internal --add-interface=eth1 --permanent

# Servicios internos adicionales
firewall-cmd --zone=internal --add-service=dns --permanent
firewall-cmd --zone=internal --add-service=dhcp --permanent

# Cambiar zona por defecto a drop (interfaces nuevas = bloqueadas)
firewall-cmd --set-default-zone=drop

firewall-cmd --reload

# Verificar masquerading en external
firewall-cmd --zone=external --query-masquerade
# yes
```

### Escenario 3: servidor con red de gestión

```
                    ┌──────────────┐
Internet ── eth0 ───┤              ├─── eth1 ── LAN (192.168.1.0/24)
   (public)         │   Servidor   │
                    │              ├─── eth2 ── Gestión (10.255.0.0/24)
                    └──────────────┘     (trusted)
```

```bash
# Pública: solo lo necesario
firewall-cmd --zone=public --add-interface=eth0 --permanent
firewall-cmd --zone=public --add-service=http --permanent
firewall-cmd --zone=public --add-service=https --permanent
# Sin SSH en public — solo se accede desde gestión

# LAN: servicios internos
firewall-cmd --zone=internal --add-interface=eth1 --permanent
firewall-cmd --zone=internal --add-service=dns --permanent
firewall-cmd --zone=internal --add-service=nfs --permanent

# Gestión: acceso total (SSH, SNMP, todo)
firewall-cmd --zone=trusted --add-interface=eth2 --permanent

# Quitar SSH de la zona public
firewall-cmd --zone=public --remove-service=ssh --permanent

firewall-cmd --reload
```

### Escenario 4: zonas por origen para gestión sin interfaz dedicada

Cuando el servidor solo tiene una interfaz pero necesitas diferenciar accesos:

```bash
# Interfaz eth0 → public (web)
firewall-cmd --zone=public --add-interface=eth0 --permanent
firewall-cmd --zone=public --add-service=http --permanent
firewall-cmd --zone=public --add-service=https --permanent
firewall-cmd --zone=public --remove-service=ssh --permanent

# Rango de IPs de oficina → internal (SSH + web)
firewall-cmd --zone=internal --add-source=10.0.0.0/24 --permanent
firewall-cmd --zone=internal --add-service=ssh --permanent
firewall-cmd --zone=internal --add-service=http --permanent
firewall-cmd --zone=internal --add-service=https --permanent

firewall-cmd --reload

# Resultado:
# - Desde 10.0.0.x: SSH + HTTP/S (zona internal por source)
# - Desde cualquier otra IP: solo HTTP/S (zona public por interfaz)
```

---

## 13. Errores comunes

### Error 1: --permanent sin --reload

```bash
# ✗ El administrador cree que el cambio está activo
firewall-cmd --zone=public --add-service=http --permanent
# El servicio NO está abierto todavía en runtime

# ✓ Siempre recargar después de --permanent
firewall-cmd --zone=public --add-service=http --permanent
firewall-cmd --reload

# ✓ O usar el flujo runtime → permanent
firewall-cmd --zone=public --add-service=http
# probar...
firewall-cmd --runtime-to-permanent
```

### Error 2: cambiar default zone sin zona explícita para SSH

```bash
# ✗ Cambiar default a drop mientras SSH depende de la zona por defecto
firewall-cmd --set-default-zone=drop
# → La interfaz de gestión pasa a zona drop → SSH cortado → lockout

# ✓ Asignar la interfaz a zona con SSH ANTES
firewall-cmd --zone=internal --add-interface=eth0 --permanent
firewall-cmd --zone=internal --add-service=ssh --permanent
firewall-cmd --reload
# Verificar que SSH funciona por la nueva zona
firewall-cmd --set-default-zone=drop
```

### Error 3: confundir zonas activas con todas las zonas

```bash
# ✗ Creer que --list-all muestra todo
firewall-cmd --list-all
# Solo muestra la zona por defecto

# ✓ Ver TODAS las zonas activas
firewall-cmd --list-all-zones    # todas las zonas (activas e inactivas)
firewall-cmd --get-active-zones   # solo las que tienen interfaces/sources
```

### Error 4: editar XML sin recargar

```bash
# ✗ Editar /etc/firewalld/zones/public.xml manualmente
# y esperar que los cambios apliquen automáticamente
vim /etc/firewalld/zones/public.xml

# ✓ Después de editar XML, siempre recargar
firewall-cmd --reload

# ✓ O mejor aún, usar firewall-cmd que modifica XML y puede
# aplicar runtime al mismo tiempo
```

### Error 5: no verificar la zona activa de una interfaz

```bash
# ✗ Añadir servicio a la zona equivocada
firewall-cmd --zone=public --add-service=http --permanent
# Pero eth0 está en zona "internal", no en "public"

# ✓ Verificar primero dónde está la interfaz
firewall-cmd --get-zone-of-interface=eth0
# internal
firewall-cmd --zone=internal --add-service=http --permanent
```

---

## 14. Cheatsheet

```bash
# ╔══════════════════════════════════════════════════════════════════╗
# ║              FIREWALLD ZONAS — CHEATSHEET                      ║
# ╠══════════════════════════════════════════════════════════════════╣
# ║                                                                ║
# ║  ESTADO:                                                      ║
# ║  firewall-cmd --state                  # running?              ║
# ║  firewall-cmd --get-default-zone       # zona por defecto      ║
# ║  firewall-cmd --get-active-zones       # zonas con interfaces  ║
# ║  firewall-cmd --list-all               # zona por defecto      ║
# ║  firewall-cmd --zone=X --list-all      # zona específica       ║
# ║  firewall-cmd --list-all-zones         # todo detallado        ║
# ║                                                                ║
# ║  ZONAS:                                                       ║
# ║  firewall-cmd --set-default-zone=drop                          ║
# ║  firewall-cmd --get-zone-of-interface=eth0                     ║
# ║  firewall-cmd --zone=X --change-interface=eth0 --permanent     ║
# ║  firewall-cmd --zone=X --add-source=10.0.0.0/24 --permanent   ║
# ║                                                                ║
# ║  SERVICIOS:                                                   ║
# ║  firewall-cmd --get-services                                   ║
# ║  firewall-cmd --info-service=ssh                               ║
# ║  firewall-cmd --zone=X --add-service=http --permanent          ║
# ║  firewall-cmd --zone=X --remove-service=ssh --permanent        ║
# ║                                                                ║
# ║  PUERTOS:                                                     ║
# ║  firewall-cmd --zone=X --add-port=8080/tcp --permanent         ║
# ║  firewall-cmd --zone=X --list-ports                            ║
# ║                                                                ║
# ║  RUNTIME vs PERMANENT:                                        ║
# ║  firewall-cmd --zone=X --add-service=Y       # solo runtime    ║
# ║  firewall-cmd --zone=X --add-service=Y --permanent # solo perm ║
# ║  firewall-cmd --reload                       # perm → runtime  ║
# ║  firewall-cmd --runtime-to-permanent         # runtime → perm  ║
# ║                                                                ║
# ║  LOGGING:                                                     ║
# ║  firewall-cmd --set-log-denied=all                             ║
# ║  journalctl -k | grep FINAL_REJECT                            ║
# ║                                                                ║
# ╚══════════════════════════════════════════════════════════════════╝
```

---

## 15. Ejercicios

### Ejercicio 1: diseñar zonas para un servidor multired

**Contexto**: un servidor Linux tiene tres interfaces:
- `eth0` — conectada a Internet
- `eth1` — red de oficina (192.168.1.0/24)
- `eth2` — red de monitorización (10.254.0.0/24)

**Tareas**:

1. Asigna cada interfaz a la zona más apropiada
2. En la zona de Internet: permite solo HTTP y HTTPS (sin SSH)
3. En la zona de oficina: permite SSH, HTTP, HTTPS y DNS
4. En la zona de monitorización: permite todo (es red de confianza)
5. Cambia la zona por defecto a `drop`
6. Verifica con `--get-active-zones` y `--list-all` para cada zona

**Pistas**:
- ¿Qué zona predefinida incluye masquerading?
- No olvides quitar SSH de public si no lo quieres expuesto
- Cambia la zona por defecto **al final**, después de asignar interfaces

> **Pregunta de reflexión**: ¿por qué es mejor asignar la interfaz de
> monitorización a `trusted` en lugar de crear una zona personalizada que
> permita todos los puertos? ¿Cuál es la diferencia a nivel de reglas
> generadas?

---

### Ejercicio 2: zona personalizada

**Contexto**: necesitas una zona llamada `webfarm` para servidores web que:
- Permita HTTP (80), HTTPS (443) y HTTP alternativo (8080, 8443)
- Permita SSH solo desde la red de gestión (10.255.0.0/24)
- Bloquee todo lo demás

**Tareas**:

1. Crea la zona personalizada `webfarm`
2. Crea un servicio personalizado `mywebapp` con los puertos 8080/tcp y 8443/tcp
3. Añade los servicios http, https y mywebapp a la zona
4. Usa un source para permitir SSH solo desde la red de gestión
5. Asigna `eth0` a la zona `webfarm`
6. Verifica con `firewall-cmd --zone=webfarm --list-all`

**Pistas**:
- `firewall-cmd --permanent --new-zone=webfarm`
- La restricción de SSH por origen requiere pensar en la jerarquía
  source vs interface
- Un source en zona `internal` con SSH es más limpio que rich rules (por ahora)

> **Pregunta de reflexión**: la restricción "SSH solo desde 10.255.0.0/24"
> se puede implementar de dos formas: (a) con source en zona separate, o
> (b) con rich rules en la misma zona. ¿Cuál es más mantenible y por qué?
> (Las rich rules se cubren en el siguiente tema)

---

### Ejercicio 3: auditoría de firewalld

**Contexto**: heredas un servidor con la siguiente configuración:

```bash
$ firewall-cmd --get-active-zones
public
  interfaces: eth0 eth1 eth2
$ firewall-cmd --zone=public --list-all
public (active)
  interfaces: eth0 eth1 eth2
  services: cockpit dhcpv6-client http https ssh telnet ftp
  ports: 3306/tcp 5432/tcp 6379/tcp 27017/tcp
```

**Tareas**:

1. Identifica al menos 5 problemas de seguridad
2. Propón una reestructuración con zonas apropiadas
3. Escribe los comandos `firewall-cmd` para implementar tu diseño
4. Explica el orden de ejecución para no perder acceso SSH durante la migración

**Pistas**:
- ¿Deberían tres interfaces estar en la misma zona?
- ¿Debería telnet estar abierto? ¿Y FTP?
- ¿Deberían los puertos de bases de datos (MySQL, PostgreSQL, Redis, MongoDB)
  estar accesibles desde Internet?
- ¿El orden de los comandos importa cuando estás en el servidor vía SSH?

> **Pregunta de reflexión**: ¿por qué firewalld habilita `ssh` y `dhcpv6-client`
> por defecto en la zona `public`? ¿Qué pasaría si la zona por defecto no
> incluyera SSH?

---

> **Siguiente tema**: T02 — firewall-cmd (servicios, puertos, rich rules, permanent vs runtime)
