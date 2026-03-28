# firewall-cmd

## Índice

1. [Visión general de firewall-cmd](#1-visión-general-de-firewall-cmd)
2. [Consultas de estado](#2-consultas-de-estado)
3. [Gestión de servicios](#3-gestión-de-servicios)
4. [Gestión de puertos](#4-gestión-de-puertos)
5. [Protocolos e ICMP](#5-protocolos-e-icmp)
6. [Masquerading y port forwarding](#6-masquerading-y-port-forwarding)
7. [Rich rules](#7-rich-rules)
8. [IPSets](#8-ipsets)
9. [Políticas entre zonas (policies)](#9-políticas-entre-zonas-policies)
10. [Logging y diagnóstico](#10-logging-y-diagnóstico)
11. [Gestión del servicio](#11-gestión-del-servicio)
12. [Errores comunes](#12-errores-comunes)
13. [Cheatsheet](#13-cheatsheet)
14. [Ejercicios](#14-ejercicios)

---

## 1. Visión general de firewall-cmd

`firewall-cmd` es la herramienta de línea de comandos para interactuar con el
daemon firewalld. Todos los cambios pasan por D-Bus hacia el daemon, que a su
vez genera las reglas nftables correspondientes.

### Estructura general del comando

```
firewall-cmd  [--zone=ZONA]  [--permanent]  ACCIÓN  [PARÁMETROS]
```

Los tres modificadores que aparecen en casi todos los comandos:

| Modificador | Efecto |
|-------------|--------|
| *(sin --zone)* | Opera sobre la zona por defecto |
| `--zone=ZONA` | Opera sobre la zona especificada |
| *(sin --permanent)* | Cambio solo en runtime (se pierde al reiniciar) |
| `--permanent` | Cambio solo en disco (no activo hasta `--reload`) |

### Patrón de trabajo recomendado

```bash
# 1. Hacer cambio en runtime (efecto inmediato, para probar)
firewall-cmd --zone=public --add-service=https

# 2. Verificar que funciona
firewall-cmd --zone=public --list-all
curl -k https://localhost/

# 3. Persistir si todo va bien
firewall-cmd --runtime-to-permanent

# --- Si algo sale mal ---
# Descartar cambios runtime y volver al estado guardado
firewall-cmd --reload
```

---

## 2. Consultas de estado

### Estado general

```bash
# ¿firewalld está corriendo?
firewall-cmd --state
# running

# Zona por defecto
firewall-cmd --get-default-zone
# public

# Zonas activas (con interfaces o sources asignados)
firewall-cmd --get-active-zones
# public
#   interfaces: eth0
# internal
#   interfaces: eth1
#   sources: 10.0.0.0/24

# Todas las zonas disponibles
firewall-cmd --get-zones
# block dmz drop external home internal public trusted work
```

### Inspeccionar una zona

```bash
# Detalle completo de una zona
firewall-cmd --zone=public --list-all
# public (active)
#   target: default
#   icmp-block-inversion: no
#   interfaces: eth0
#   sources:
#   services: dhcpv6-client http https ssh
#   ports: 8080/tcp
#   protocols:
#   forward: yes
#   masquerade: no
#   forward-ports:
#   source-ports:
#   icmp-blocks:
#   rich rules:

# Todas las zonas (activas e inactivas)
firewall-cmd --list-all-zones
```

### Consultas específicas

```bash
# ¿Está un servicio activo en una zona?
firewall-cmd --zone=public --query-service=http
# yes / no

# ¿Está un puerto abierto?
firewall-cmd --zone=public --query-port=8080/tcp

# ¿Está masquerade activo?
firewall-cmd --zone=external --query-masquerade

# ¿A qué zona pertenece una interfaz?
firewall-cmd --get-zone-of-interface=eth0
```

---

## 3. Gestión de servicios

### Servicios disponibles

```bash
# Listar todos los servicios predefinidos (~70+)
firewall-cmd --get-services

# Información detallada de un servicio
firewall-cmd --info-service=http
# http
#   ports: 80/tcp
#   protocols:
#   source-ports:
#   modules:
#   destination:
#   includes:

firewall-cmd --info-service=ftp
# ftp
#   ports: 21/tcp
#   protocols:
#   source-ports:
#   modules: nf_conntrack_ftp    ← helper para FTP pasivo
#   destination:
```

### Añadir y eliminar servicios

```bash
# Añadir servicio a zona (runtime)
firewall-cmd --zone=public --add-service=http

# Añadir servicio (permanente)
firewall-cmd --zone=public --add-service=http --permanent

# Múltiples servicios en un comando
firewall-cmd --zone=public --add-service={http,https,dns} --permanent

# Eliminar servicio
firewall-cmd --zone=public --remove-service=dhcpv6-client --permanent

# Aplicar cambios permanentes
firewall-cmd --reload
```

### Crear servicios personalizados

```bash
# Crear servicio nuevo
firewall-cmd --permanent --new-service=myapp
firewall-cmd --permanent --service=myapp --set-short="My Application"
firewall-cmd --permanent --service=myapp --set-description="Custom web application"
firewall-cmd --permanent --service=myapp --add-port=8080/tcp
firewall-cmd --permanent --service=myapp --add-port=8443/tcp

# Recargar para que el servicio sea visible
firewall-cmd --reload

# Verificar
firewall-cmd --info-service=myapp
# myapp
#   ports: 8080/tcp 8443/tcp

# Usar el servicio
firewall-cmd --zone=public --add-service=myapp --permanent
firewall-cmd --reload
```

### Crear servicio desde archivo XML

```xml
<!-- /etc/firewalld/services/grafana.xml -->
<?xml version="1.0" encoding="utf-8"?>
<service>
  <short>Grafana</short>
  <description>Grafana monitoring dashboard</description>
  <port protocol="tcp" port="3000"/>
</service>
```

```bash
firewall-cmd --reload
firewall-cmd --info-service=grafana
```

### Modificar servicio existente

Para modificar un servicio del sistema, firewalld lo copia automáticamente a
`/etc/firewalld/services/` al usar `--permanent`:

```bash
# Ejemplo: SSH en puerto no estándar
# Opción 1: modificar el servicio ssh
firewall-cmd --permanent --service=ssh --add-port=2222/tcp
firewall-cmd --reload
# Ahora el servicio ssh abre 22/tcp Y 2222/tcp

# Opción 2: crear servicio separado (más limpio)
firewall-cmd --permanent --new-service=ssh-alt
firewall-cmd --permanent --service=ssh-alt --add-port=2222/tcp
firewall-cmd --reload
```

---

## 4. Gestión de puertos

### Puertos individuales y rangos

```bash
# Abrir un puerto TCP
firewall-cmd --zone=public --add-port=3000/tcp --permanent

# Abrir un puerto UDP
firewall-cmd --zone=public --add-port=5060/udp --permanent

# Rango de puertos
firewall-cmd --zone=public --add-port=8000-8100/tcp --permanent

# Múltiples puertos
firewall-cmd --zone=public --add-port={3000/tcp,5060/udp,9090/tcp} --permanent

# Ver puertos abiertos
firewall-cmd --zone=public --list-ports
# 3000/tcp 5060/udp 8000-8100/tcp 9090/tcp

# Eliminar
firewall-cmd --zone=public --remove-port=3000/tcp --permanent

firewall-cmd --reload
```

### Source ports

En raras ocasiones necesitas filtrar por puerto de origen (no es lo habitual):

```bash
# Permitir tráfico desde un puerto origen específico
firewall-cmd --zone=public --add-source-port=67/udp --permanent

# Listar
firewall-cmd --zone=public --list-source-ports
```

### Cuándo usar puertos vs servicios

```bash
# Puerto directo: rápido, para pruebas o puertos puntuales
firewall-cmd --zone=public --add-port=9090/tcp

# Servicio: autodocumentado, múltiples puertos, helpers
firewall-cmd --zone=public --add-service=cockpit
# cockpit = 9090/tcp, pero el nombre explica QUÉ es

# En producción, prefiere servicios (predefinidos o personalizados)
# para que "firewall-cmd --list-all" sea legible
```

---

## 5. Protocolos e ICMP

### Protocolos

Además de TCP y UDP, puedes permitir protocolos completos:

```bash
# Permitir GRE (protocolo 47, usado en VPN PPTP y túneles)
firewall-cmd --zone=public --add-protocol=gre --permanent

# Permitir OSPF (protocolo 89, enrutamiento dinámico)
firewall-cmd --zone=internal --add-protocol=ospf --permanent

# Permitir VRRP (protocolo 112, keepalived)
firewall-cmd --zone=internal --add-protocol=vrrp --permanent

# Listar protocolos permitidos
firewall-cmd --zone=public --list-protocols

firewall-cmd --reload
```

### Bloqueo de tipos ICMP

Por defecto, firewalld permite ICMP. Puedes bloquear tipos específicos:

```bash
# Ver tipos ICMP disponibles
firewall-cmd --get-icmptypes
# destination-unreachable echo-reply echo-request
# parameter-problem redirect router-advertisement
# router-solicitation source-quench time-exceeded timestamp-reply timestamp-request

# Bloquear ping (echo-request)
firewall-cmd --zone=public --add-icmp-block=echo-request --permanent

# Bloquear redirect (ataques de redirección)
firewall-cmd --zone=public --add-icmp-block=redirect --permanent

# Invertir lógica: bloquear TODO excepto los listados
firewall-cmd --zone=public --add-icmp-block-inversion --permanent

# Listar bloqueos
firewall-cmd --zone=public --list-icmp-blocks

firewall-cmd --reload
```

**Nota**: bloquear ICMP completamente puede causar problemas con Path MTU
Discovery y diagnóstico de red. La práctica recomendada es permitir al menos
`destination-unreachable`, `time-exceeded` y `parameter-problem`.

---

## 6. Masquerading y port forwarding

### Masquerading (SNAT)

```bash
# Activar masquerading en una zona (NAT saliente)
firewall-cmd --zone=external --add-masquerade --permanent

# Verificar
firewall-cmd --zone=external --query-masquerade
# yes

# Desactivar
firewall-cmd --zone=external --remove-masquerade --permanent

firewall-cmd --reload
```

La zona `external` tiene masquerading habilitado por defecto. Para otras zonas
debes activarlo explícitamente.

**Importante**: masquerading solo enmascara el tráfico saliente. Todavía necesitas
que el kernel tenga `ip_forward=1`:

```bash
# Verificar
sysctl net.ipv4.ip_forward

# firewalld puede gestionar forwarding si configuras zonas con forward
firewall-cmd --zone=external --query-forward
```

### Port forwarding

Port forwarding en firewalld combina DNAT + regla de forward internamente:

```bash
# Sintaxis general
firewall-cmd --zone=ZONA --add-forward-port=\
port=PUERTO_EXTERNO:proto=PROTOCOLO:toport=PUERTO_INTERNO:toaddr=IP_DESTINO

# Ejemplo: redirigir puerto 80 a servidor interno
firewall-cmd --zone=external --add-forward-port=\
port=80:proto=tcp:toport=80:toaddr=192.168.1.100 --permanent

# Cambio de puerto: 2222 externo → 22 interno
firewall-cmd --zone=external --add-forward-port=\
port=2222:proto=tcp:toport=22:toaddr=192.168.1.101 --permanent

# Solo cambio de puerto local (sin cambiar IP, redirige en el propio host)
firewall-cmd --zone=public --add-forward-port=\
port=80:proto=tcp:toport=8080 --permanent

# Ver forward ports
firewall-cmd --zone=external --list-forward-ports
# port=80:proto=tcp:toport=80:toaddr=192.168.1.100
# port=2222:proto=tcp:toport=22:toaddr=192.168.1.101

# Eliminar
firewall-cmd --zone=external --remove-forward-port=\
port=80:proto=tcp:toport=80:toaddr=192.168.1.100 --permanent

firewall-cmd --reload
```

### Escenario completo: router con port forwarding

```bash
# 1. Interfaces asignadas
firewall-cmd --zone=external --add-interface=eth0 --permanent   # WAN
firewall-cmd --zone=internal --add-interface=eth1 --permanent   # LAN

# 2. Masquerading en WAN (external ya lo tiene por defecto)
firewall-cmd --zone=external --query-masquerade
# yes

# 3. Port forwarding
# Web server
firewall-cmd --zone=external --add-forward-port=\
port=80:proto=tcp:toport=80:toaddr=192.168.1.100 --permanent
firewall-cmd --zone=external --add-forward-port=\
port=443:proto=tcp:toport=443:toaddr=192.168.1.100 --permanent

# SSH a servidor interno (puerto externo 2222)
firewall-cmd --zone=external --add-forward-port=\
port=2222:proto=tcp:toport=22:toaddr=192.168.1.101 --permanent

# 4. ip_forward
sysctl -w net.ipv4.ip_forward=1
echo "net.ipv4.ip_forward = 1" > /etc/sysctl.d/99-forward.conf

firewall-cmd --reload
```

---

## 7. Rich rules

Las rich rules son la funcionalidad más potente de firewall-cmd. Permiten
reglas condicionales complejas que combinan origen, destino, servicio, puerto,
protocolo, logging y acción en una sola expresión.

### Sintaxis

```
rule
  [family="ipv4|ipv6"]
  [source address="CIDR" [invert="true"]]
  [destination address="CIDR" [invert="true"]]
  [service name="SERVICIO" | port port="PUERTO" protocol="PROTO" |
   protocol value="PROTO" | icmp-type name="TIPO" |
   forward-port port="P" protocol="PR" to-port="P2" to-addr="IP"]
  [log [prefix="TEXTO"] [level="NIVEL"] [limit value="RATE"]]
  [audit]
  [accept | reject [type="TIPO"] | drop | mark set="VALOR"]
```

### Reglas por IP de origen

```bash
# Permitir SSH solo desde una red específica
firewall-cmd --zone=public --add-rich-rule='
  rule family="ipv4"
  source address="10.0.0.0/24"
  service name="ssh"
  accept' --permanent

# Bloquear una IP específica
firewall-cmd --zone=public --add-rich-rule='
  rule family="ipv4"
  source address="203.0.113.50"
  drop' --permanent

# Rechazar un rango con mensaje ICMP
firewall-cmd --zone=public --add-rich-rule='
  rule family="ipv4"
  source address="198.51.100.0/24"
  reject type="icmp-host-prohibited"' --permanent
```

### Reglas con logging

```bash
# Loguear y aceptar conexiones SSH desde la oficina
firewall-cmd --zone=public --add-rich-rule='
  rule family="ipv4"
  source address="10.0.0.0/24"
  service name="ssh"
  log prefix="SSH_OFFICE: " level="info"
  accept' --permanent

# Loguear y descartar intentos a MySQL desde fuera
firewall-cmd --zone=public --add-rich-rule='
  rule family="ipv4"
  port port="3306" protocol="tcp"
  log prefix="MYSQL_BLOCKED: " level="warning" limit value="5/m"
  drop' --permanent
```

Los niveles de log son: `emerg`, `alert`, `crit`, `error`, `warning`, `notice`,
`info`, `debug`.

`limit value="5/m"` limita a 5 mensajes por minuto para no inundar los logs.

### Rate limiting

```bash
# Limitar conexiones SSH a 3 por minuto por IP
firewall-cmd --zone=public --add-rich-rule='
  rule family="ipv4"
  service name="ssh"
  accept
  limit value="3/m"' --permanent
```

**Nota**: este rate limit es global, no por IP de origen. Para rate limiting
por IP se necesitan técnicas más avanzadas (ipsets dinámicos o nft directo).

### Reglas con destino

```bash
# Permitir acceso desde la LAN al servidor DNS interno
firewall-cmd --zone=internal --add-rich-rule='
  rule family="ipv4"
  destination address="192.168.1.53"
  service name="dns"
  accept' --permanent
```

### Port forwarding con rich rules

```bash
# Forward con restricción de origen
firewall-cmd --zone=external --add-rich-rule='
  rule family="ipv4"
  source address="203.0.113.0/24"
  forward-port port="2222" protocol="tcp"
  to-port="22" to-addr="192.168.1.101"' --permanent
```

### Gestionar rich rules

```bash
# Listar rich rules de una zona
firewall-cmd --zone=public --list-rich-rules

# Eliminar una rich rule (copiar la regla exacta)
firewall-cmd --zone=public --remove-rich-rule='
  rule family="ipv4"
  source address="203.0.113.50"
  drop' --permanent

firewall-cmd --reload
```

### Orden de evaluación dentro de una zona

```
1. Port forwarding (forward-port) y masquerading
2. Logging (log, audit)
3. Rich rules con deny (drop/reject)
4. Rich rules con allow (accept)
5. Servicios y puertos de la zona
6. ICMP blocks
7. Target por defecto de la zona (default = reject)
```

**Predicción**: una rich rule con `drop` se evalúa ANTES que los servicios
de la zona. Si bloqueas una IP con rich rule drop, esa IP no podrá acceder
a ningún servicio de la zona aunque esté listado.

### Ejemplos prácticos de rich rules

```bash
# 1. Servidor web: HTTP/HTTPS público, SSH solo desde oficina
firewall-cmd --zone=public --add-service=http --permanent
firewall-cmd --zone=public --add-service=https --permanent
firewall-cmd --zone=public --remove-service=ssh --permanent
firewall-cmd --zone=public --add-rich-rule='
  rule family="ipv4"
  source address="10.0.0.0/24"
  service name="ssh"
  accept' --permanent

# 2. Bloquear un país/rango + loguear
firewall-cmd --zone=public --add-rich-rule='
  rule family="ipv4"
  source address="192.0.2.0/24"
  log prefix="BLOCKED_RANGE: " level="warning" limit value="1/m"
  drop' --permanent

# 3. Permitir PostgreSQL solo desde el servidor de aplicaciones
firewall-cmd --zone=public --add-rich-rule='
  rule family="ipv4"
  source address="192.168.1.50"
  port port="5432" protocol="tcp"
  accept' --permanent

# 4. Redirigir puerto con logging
firewall-cmd --zone=external --add-rich-rule='
  rule family="ipv4"
  forward-port port="8080" protocol="tcp"
  to-port="80" to-addr="192.168.1.100"
  log prefix="FWD_HTTP: " level="info" limit value="10/m"' --permanent

firewall-cmd --reload
```

---

## 8. IPSets

Los IPSets permiten agrupar direcciones IP, redes o pares MAC en conjuntos
reutilizables. Son más eficientes que múltiples rich rules para listas grandes.

### Crear y gestionar IPSets

```bash
# Crear un ipset de tipo hash:ip (direcciones individuales)
firewall-cmd --permanent --new-ipset=blocked --type=hash:ip

# Crear un ipset de tipo hash:net (redes/CIDR)
firewall-cmd --permanent --new-ipset=trusted_nets --type=hash:net

# Añadir elementos
firewall-cmd --permanent --ipset=blocked --add-entry=203.0.113.50
firewall-cmd --permanent --ipset=blocked --add-entry=198.51.100.99

firewall-cmd --permanent --ipset=trusted_nets --add-entry=10.0.0.0/24
firewall-cmd --permanent --ipset=trusted_nets --add-entry=172.16.0.0/16

# Listar contenido
firewall-cmd --permanent --ipset=blocked --get-entries

# Eliminar entrada
firewall-cmd --permanent --ipset=blocked --remove-entry=203.0.113.50

# Cargar desde archivo (una IP/red por línea)
firewall-cmd --permanent --ipset=blocked --add-entries-from-file=/etc/blocked_ips.txt

firewall-cmd --reload
```

### Tipos de IPSet

| Tipo | Contenido | Ejemplo |
|------|----------|---------|
| `hash:ip` | IPs individuales | 192.168.1.100 |
| `hash:net` | Redes CIDR | 10.0.0.0/8 |
| `hash:mac` | Direcciones MAC | AA:BB:CC:DD:EE:FF |
| `hash:ip,port` | Pares IP+puerto | 192.168.1.100,tcp:80 |
| `hash:net,port` | Pares red+puerto | 10.0.0.0/8,tcp:22 |

### Usar IPSets en zonas y rich rules

```bash
# Añadir ipset como source de una zona
firewall-cmd --zone=drop --add-source=ipset:blocked --permanent

# Resultado: todas las IPs del ipset "blocked" van a zona drop

# Usar en rich rule
firewall-cmd --zone=public --add-rich-rule='
  rule family="ipv4"
  source ipset="trusted_nets"
  service name="ssh"
  accept' --permanent

firewall-cmd --reload
```

### Caso de uso: lista de bloqueo externa

```bash
# Crear ipset
firewall-cmd --permanent --new-ipset=blocklist --type=hash:net \
    --option=maxelem=100000

# Script para actualizar la lista periódicamente
cat > /usr/local/bin/update-blocklist.sh << 'SCRIPT'
#!/bin/bash
# Download and update blocklist
TMPFILE=$(mktemp)
curl -s https://example.com/blocklist.txt > "$TMPFILE"

# Flush existing entries
firewall-cmd --permanent --ipset=blocklist --remove-entries-from-file=<(
    firewall-cmd --permanent --ipset=blocklist --get-entries
) 2>/dev/null

# Load new entries
firewall-cmd --permanent --ipset=blocklist --add-entries-from-file="$TMPFILE"
firewall-cmd --reload

rm "$TMPFILE"
SCRIPT
chmod +x /usr/local/bin/update-blocklist.sh

# Asignar a zona drop
firewall-cmd --zone=drop --add-source=ipset:blocklist --permanent
firewall-cmd --reload
```

---

## 9. Políticas entre zonas (policies)

A partir de firewalld 0.9.0 (RHEL 9, Fedora 33+), existen **policies** que
definen reglas para tráfico **entre** zonas. Antes de policies, el tráfico
forward entre zonas solo se controlaba con direct rules.

### Concepto

```
                  Zona A               Zona B
               ┌──────────┐         ┌──────────┐
               │  public   │         │ internal  │
               │  (eth0)   │◄───────▶│  (eth1)   │
               └──────────┘  Policy  └──────────┘
                             entre
                             zonas

La policy define: ¿qué tráfico puede fluir de A→B y de B→A?
```

### Crear una policy

```bash
# Crear policy para tráfico internal → public
firewall-cmd --permanent --new-policy=internal-to-public

# Asignar zonas de ingreso y egreso
firewall-cmd --permanent --policy=internal-to-public \
    --add-ingress-zone=internal
firewall-cmd --permanent --policy=internal-to-public \
    --add-egress-zone=public

# Establecer target (default = CONTINUE, que delega a la zona)
firewall-cmd --permanent --policy=internal-to-public \
    --set-target=ACCEPT

# Resultado: todo el tráfico de internal→public se acepta (la LAN sale a Internet)

firewall-cmd --reload
```

### Policy restrictiva para DMZ

```bash
# DMZ puede salir a Internet pero NO acceder a la LAN
firewall-cmd --permanent --new-policy=dmz-to-internal
firewall-cmd --permanent --policy=dmz-to-internal \
    --add-ingress-zone=dmz
firewall-cmd --permanent --policy=dmz-to-internal \
    --add-egress-zone=internal
firewall-cmd --permanent --policy=dmz-to-internal \
    --set-target=DROP

# Internet → DMZ: solo servicios publicados
firewall-cmd --permanent --new-policy=public-to-dmz
firewall-cmd --permanent --policy=public-to-dmz \
    --add-ingress-zone=public
firewall-cmd --permanent --policy=public-to-dmz \
    --add-egress-zone=dmz
firewall-cmd --permanent --policy=public-to-dmz \
    --set-target=DROP
firewall-cmd --permanent --policy=public-to-dmz \
    --add-service=http
firewall-cmd --permanent --policy=public-to-dmz \
    --add-service=https

firewall-cmd --reload
```

### Políticas con HOST y ANY

```bash
# HOST = tráfico destinado al propio firewall (no forwarded)
# ANY  = todas las zonas

# Ejemplo: limitar SSH al firewall desde cualquier zona
firewall-cmd --permanent --new-policy=ssh-limited
firewall-cmd --permanent --policy=ssh-limited \
    --add-ingress-zone=ANY
firewall-cmd --permanent --policy=ssh-limited \
    --add-egress-zone=HOST
firewall-cmd --permanent --policy=ssh-limited \
    --add-rich-rule='rule service name="ssh" accept limit value="5/m"'

firewall-cmd --reload
```

### Ver policies

```bash
# Listar todas las policies
firewall-cmd --get-policies

# Detalle de una policy
firewall-cmd --info-policy=internal-to-public
# internal-to-public (active)
#   ingress-zones: internal
#   egress-zones: public
#   target: ACCEPT
#   services:
#   ports:
#   ...

# Listar policies con --permanent
firewall-cmd --get-policies --permanent
```

---

## 10. Logging y diagnóstico

### Log de paquetes denegados

```bash
# Activar logging global de paquetes rechazados
firewall-cmd --set-log-denied=all

# Opciones: off | all | unicast | broadcast | multicast
firewall-cmd --get-log-denied
# all

# Ver los logs
journalctl -k | grep FINAL_REJECT
# Mar 21 14:23:01 server kernel: FINAL_REJECT: IN=eth0 OUT=
#   SRC=203.0.113.50 DST=192.168.1.1 ... PROTO=TCP SPT=45832 DPT=3306

# Filtrar por interfaz o IP
journalctl -k | grep "FINAL_REJECT.*DPT=22"
```

### Logging con rich rules

```bash
# Log selectivo: solo loguear intentos al puerto MySQL
firewall-cmd --zone=public --add-rich-rule='
  rule family="ipv4"
  port port="3306" protocol="tcp"
  log prefix="MYSQL_ATTEMPT: " level="info" limit value="5/m"
  drop' --permanent

# Log de conexiones aceptadas (útil para auditoría)
firewall-cmd --zone=public --add-rich-rule='
  rule family="ipv4"
  service name="ssh"
  log prefix="SSH_ACCESS: " level="info" limit value="10/m"
  accept' --permanent

firewall-cmd --reload
```

### Ver las reglas nftables generadas

```bash
# firewalld genera reglas nftables internamente
# Puedes verlas para entender qué hace cada configuración
nft list ruleset | head -50

# Buscar reglas de una zona específica
nft list ruleset | grep -A 5 "public"
```

### Diagnóstico: ¿por qué no funciona mi regla?

```bash
# Lista de verificación:
# 1. ¿firewalld está corriendo?
firewall-cmd --state

# 2. ¿La interfaz está en la zona correcta?
firewall-cmd --get-zone-of-interface=eth0

# 3. ¿El servicio/puerto está en la zona correcta?
firewall-cmd --zone=public --list-all

# 4. ¿Estás viendo runtime o permanent?
firewall-cmd --zone=public --list-all             # runtime
firewall-cmd --zone=public --list-all --permanent  # permanent

# 5. ¿Hay una rich rule que bloquea antes?
firewall-cmd --zone=public --list-rich-rules

# 6. ¿Hay un source que envía el tráfico a otra zona?
firewall-cmd --get-active-zones

# 7. ¿Los logs muestran el rechazo?
firewall-cmd --set-log-denied=all
journalctl -kf | grep REJECT
# Intenta la conexión en otra terminal...
```

### Panic mode

En caso de emergencia (ataque activo), se puede bloquear **todo** el tráfico:

```bash
# Activar modo pánico (bloquea TODO el tráfico de red)
firewall-cmd --panic-on

# Verificar
firewall-cmd --query-panic
# yes

# Desactivar
firewall-cmd --panic-off
```

**Advertencia**: el panic mode incluye tráfico SSH. Solo úsalo si tienes acceso
por consola física, IPMI/iLO, o un mecanismo out-of-band.

---

## 11. Gestión del servicio

### Operaciones systemd

```bash
# Estado del daemon
systemctl status firewalld

# Iniciar / detener
systemctl start firewalld
systemctl stop firewalld

# Habilitar / deshabilitar al arranque
systemctl enable firewalld
systemctl disable firewalld

# Reiniciar (recarga todo, interrumpe conexiones)
systemctl restart firewalld

# Reload (recarga configuración sin interrumpir)
firewall-cmd --reload

# Complete reload (reconstruye todo, puede interrumpir)
firewall-cmd --complete-reload
```

### Diferencia entre reload y complete-reload

```
--reload:
  - Recarga la configuración permanent
  - Mantiene las conexiones establecidas
  - Operación normal y segura

--complete-reload:
  - Descarga todos los módulos de firewall
  - Reconstruye las reglas desde cero
  - Puede interrumpir conexiones
  - Usar solo cuando --reload no resuelve el problema
```

### Coexistencia con otros firewalls

```bash
# firewalld vs iptables service: mutuamente excluyentes
# Si activas uno, desactiva el otro
systemctl stop iptables
systemctl disable iptables
systemctl mask iptables       # previene que se inicie accidentalmente

systemctl unmask firewalld
systemctl enable firewalld
systemctl start firewalld

# firewalld vs nftables service: firewalld USA nftables
# No actives ambos servicios
systemctl disable nftables    # si usas firewalld
```

### Backup y restauración

```bash
# Backup: copiar todo /etc/firewalld/
tar czf firewalld-backup-$(date +%F).tar.gz /etc/firewalld/

# Restaurar
tar xzf firewalld-backup-2026-03-21.tar.gz -C /
firewall-cmd --complete-reload
```

---

## 12. Errores comunes

### Error 1: servicio en la zona equivocada

```bash
# ✗ Añadir HTTP a la zona por defecto cuando la interfaz está en otra zona
firewall-cmd --add-service=http --permanent
# Se añade a la zona por defecto (ej. public)
# Pero eth0 está en zona "internal"

# ✓ Verificar la zona y especificarla
firewall-cmd --get-zone-of-interface=eth0
# internal
firewall-cmd --zone=internal --add-service=http --permanent
```

### Error 2: rich rule con sintaxis incorrecta

```bash
# ✗ Olvidar family en rich rule con IP
firewall-cmd --zone=public --add-rich-rule='
  rule source address="10.0.0.0/24"
  service name="ssh" accept'
# Error: source address requiere family

# ✓ Incluir family
firewall-cmd --zone=public --add-rich-rule='
  rule family="ipv4"
  source address="10.0.0.0/24"
  service name="ssh"
  accept'
```

### Error 3: forward-port sin masquerade ni ip_forward

```bash
# ✗ Port forwarding a otra máquina sin forwarding habilitado
firewall-cmd --zone=public --add-forward-port=\
port=80:proto=tcp:toport=80:toaddr=192.168.1.100

# El DNAT se aplica pero el paquete no se reenvía

# ✓ Asegurar forwarding
sysctl -w net.ipv4.ip_forward=1
firewall-cmd --zone=public --add-masquerade   # si la respuesta no vuelve
```

### Error 4: confundir list-all con list-all-zones

```bash
# ✗ "No veo mis reglas"
firewall-cmd --list-all
# Solo muestra la zona por defecto

# ✓ Ver todo
firewall-cmd --list-all-zones    # todas las zonas
firewall-cmd --get-active-zones  # solo las activas (resumen)
```

### Error 5: ipset vacío causa error al asignar como source

```bash
# ✗ Crear ipset, asignarlo a zona, pero no añadir entradas
firewall-cmd --permanent --new-ipset=mylist --type=hash:ip
firewall-cmd --zone=drop --add-source=ipset:mylist --permanent
firewall-cmd --reload
# Puede funcionar, pero un ipset vacío no bloquea nada
# y puede confundir al administrador

# ✓ Añadir al menos una entrada de prueba y verificar
firewall-cmd --permanent --ipset=mylist --add-entry=192.0.2.1
firewall-cmd --reload
firewall-cmd --permanent --ipset=mylist --get-entries
```

---

## 13. Cheatsheet

```bash
# ╔══════════════════════════════════════════════════════════════════╗
# ║               FIREWALL-CMD — CHEATSHEET                        ║
# ╠══════════════════════════════════════════════════════════════════╣
# ║                                                                ║
# ║  ESTADO:                                                      ║
# ║  firewall-cmd --state                                          ║
# ║  firewall-cmd --get-default-zone                               ║
# ║  firewall-cmd --get-active-zones                               ║
# ║  firewall-cmd --zone=X --list-all [--permanent]                ║
# ║                                                                ║
# ║  SERVICIOS:                                                   ║
# ║  firewall-cmd --get-services                                   ║
# ║  firewall-cmd --zone=X --add-service=Y --permanent             ║
# ║  firewall-cmd --zone=X --remove-service=Y --permanent          ║
# ║  firewall-cmd --permanent --new-service=NAME                   ║
# ║  firewall-cmd --permanent --service=NAME --add-port=P/tcp      ║
# ║                                                                ║
# ║  PUERTOS:                                                     ║
# ║  firewall-cmd --zone=X --add-port=8080/tcp --permanent         ║
# ║  firewall-cmd --zone=X --add-port=8000-8100/tcp --permanent    ║
# ║                                                                ║
# ║  NAT / FORWARDING:                                            ║
# ║  firewall-cmd --zone=X --add-masquerade --permanent            ║
# ║  firewall-cmd --zone=X --add-forward-port=\                    ║
# ║    port=80:proto=tcp:toport=80:toaddr=192.168.1.100 --perm     ║
# ║                                                                ║
# ║  RICH RULES:                                                  ║
# ║  firewall-cmd --zone=X --add-rich-rule='                       ║
# ║    rule family="ipv4"                                          ║
# ║    source address="10.0.0.0/24"                                ║
# ║    service name="ssh"                                          ║
# ║    log prefix="SSH: " level="info" limit value="5/m"           ║
# ║    accept' --permanent                                         ║
# ║                                                                ║
# ║  IPSETS:                                                      ║
# ║  firewall-cmd --permanent --new-ipset=NAME --type=hash:ip      ║
# ║  firewall-cmd --permanent --ipset=NAME --add-entry=IP          ║
# ║  firewall-cmd --zone=drop --add-source=ipset:NAME --perm       ║
# ║                                                                ║
# ║  RUNTIME / PERMANENT:                                         ║
# ║  (sin flag)              → solo runtime                        ║
# ║  --permanent             → solo disco, necesita --reload       ║
# ║  --runtime-to-permanent  → guardar runtime actual              ║
# ║  --reload                → cargar permanent en runtime         ║
# ║                                                                ║
# ║  EMERGENCIA:                                                  ║
# ║  firewall-cmd --panic-on                                       ║
# ║  firewall-cmd --panic-off                                      ║
# ║                                                                ║
# ║  LOGGING:                                                     ║
# ║  firewall-cmd --set-log-denied=all                             ║
# ║  journalctl -k | grep FINAL_REJECT                            ║
# ║                                                                ║
# ╚══════════════════════════════════════════════════════════════════╝
```

---

## 14. Ejercicios

### Ejercicio 1: configurar un servidor web con acceso restringido

**Contexto**: servidor Linux con una interfaz `eth0`, zona public.

**Tareas**:

1. Permite HTTP y HTTPS para todo el mundo
2. Restringe SSH solo a la red de oficina `10.0.0.0/24` usando una rich rule
3. Elimina SSH del listado general de servicios de la zona
4. Crea un servicio personalizado `monitoring` con puertos 9090/tcp y 9100/tcp
5. Permite el servicio `monitoring` solo desde `10.254.0.0/24` con rich rule
6. Añade logging para intentos SSH desde IPs no autorizadas (con rate limit)
7. Persiste todos los cambios y verifica con `--list-all`

**Pistas**:
- Orden: primero añade la rich rule de SSH, luego elimina el servicio global
- El log de SSH denegado aparece por `--set-log-denied=all` o con rich rule
  explícita
- Verifica que no pierdes acceso SSH antes de persistir

> **Pregunta de reflexión**: ¿por qué es preferible usar `--runtime-to-permanent`
> en lugar de `--permanent` + `--reload` cuando estás accediendo por SSH? ¿Qué
> ventaja tiene el flujo "probar en runtime, luego persistir"?

---

### Ejercicio 2: router con port forwarding y policies

**Contexto**: router Linux con:
- `eth0` — WAN (zona external)
- `eth1` — LAN (zona internal, 192.168.1.0/24)
- `eth2` — DMZ (zona dmz, 10.0.0.0/24)

Servidores en DMZ:
- `10.0.0.10` — servidor web (HTTP/HTTPS)
- `10.0.0.11` — servidor de correo (SMTP/IMAP)

**Tareas**:

1. Asigna interfaces a zonas
2. Configura masquerading en la zona external
3. Configura port forwarding para web y correo
4. Crea policies (si tu versión lo soporta):
   - LAN → Internet: ACCEPT
   - LAN → DMZ: ACCEPT
   - DMZ → Internet: ACCEPT (para updates)
   - DMZ → LAN: DROP (aislamiento de seguridad)
   - Internet → DMZ: solo servicios publicados
5. Verifica con `firewall-cmd --info-policy` para cada policy

**Pistas**:
- La zona external ya tiene masquerading
- `--add-forward-port` en external para los servicios publicados
- Las policies necesitan firewalld 0.9+ (RHEL 9+)
- Si tu versión no soporta policies, documenta cómo lo harías con direct rules

> **Pregunta de reflexión**: antes de las policies (firewalld < 0.9), ¿cómo se
> controlaba el tráfico entre zonas? ¿Por qué las policies representan una mejora
> sustancial en el modelo de seguridad de firewalld?

---

### Ejercicio 3: protección con IPSets

**Contexto**: un servidor público recibe ataques de fuerza bruta SSH desde
múltiples IPs.

**Tareas**:

1. Crea un ipset `ssh_blocklist` de tipo `hash:ip`
2. Crea un ipset `trusted_admins` de tipo `hash:net`
3. Añade 5 IPs ficticias al blocklist
4. Añade la red de oficina al trusted_admins
5. Asigna `ssh_blocklist` como source de la zona `drop`
6. Crea una rich rule que permita SSH solo desde `trusted_admins`
7. Elimina SSH del listado general de servicios
8. Crea un script que añada una IP al blocklist (recibe la IP como argumento)
9. Verifica que:
   - IPs del blocklist no pueden conectar (zona drop)
   - IPs de trusted_admins pueden SSH
   - Otras IPs no pueden SSH (servicio eliminado)

**Pistas**:
- El source ipset en zona drop se evalúa antes que la zona de la interfaz
- Verificar con `firewall-cmd --permanent --ipset=ssh_blocklist --get-entries`
- El script debe usar `--permanent` y `--reload` (o `--runtime-to-permanent`)

> **Pregunta de reflexión**: esta solución con ipsets es estática (alguien debe
> añadir IPs manualmente). ¿Cómo integrarías fail2ban con firewalld para
> automatizar el bloqueo? ¿Qué acción de fail2ban usarías (`firewallcmd-ipset`
> o `firewallcmd-rich-rules`) y por qué?

---

> **Siguiente tema**: T03 — Direct rules (integración con iptables/nftables subyacente, cuándo son necesarias)
