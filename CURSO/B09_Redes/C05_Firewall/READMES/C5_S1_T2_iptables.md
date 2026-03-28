# T02 — iptables: Reglas, Targets y Matching

## Índice

1. [Estructura del comando iptables](#1-estructura-del-comando-iptables)
2. [Operaciones sobre reglas](#2-operaciones-sobre-reglas)
3. [Criterios de matching](#3-criterios-de-matching)
4. [Match extensions](#4-match-extensions)
5. [Construir un firewall básico paso a paso](#5-construir-un-firewall-básico-paso-a-paso)
6. [Logging](#6-logging)
7. [Persistencia de reglas](#7-persistencia-de-reglas)
8. [Chains definidas por el usuario](#8-chains-definidas-por-el-usuario)
9. [Listar, contar y depurar reglas](#9-listar-contar-y-depurar-reglas)
10. [Errores comunes](#10-errores-comunes)
11. [Cheatsheet](#11-cheatsheet)
12. [Ejercicios](#12-ejercicios)

---

## 1. Estructura del comando iptables

Cada invocación de `iptables` sigue esta estructura:

```
iptables [-t TABLA] OPERACIÓN CHAIN [MATCH...] -j TARGET

         │          │          │      │           │
         │          │          │      │           └── Qué hacer si coincide
         │          │          │      └── Criterios de coincidencia
         │          │          └── En qué chain
         │          └── Qué hacer con la regla (añadir, borrar, etc.)
         └── En qué tabla (default: filter)
```

```bash
# Ejemplo concreto:
iptables -t filter -A INPUT -p tcp --dport 22 -s 10.0.0.0/8 -j ACCEPT
#        │         │  │      │               │               │
#        │         │  │      │               │               └── Aceptar
#        │         │  │      │               └── Desde red 10.0.0.0/8
#        │         │  │      └── Protocolo TCP, puerto destino 22
#        │         │  └── Chain INPUT
#        │         └── Append (añadir al final)
#        └── Tabla filter (se puede omitir, es la default)

# Equivalente (sin -t filter, es implícito):
iptables -A INPUT -p tcp --dport 22 -s 10.0.0.0/8 -j ACCEPT
```

---

## 2. Operaciones sobre reglas

### Añadir reglas

```bash
# -A: Append (añadir al FINAL de la chain)
iptables -A INPUT -p tcp --dport 80 -j ACCEPT

# -I: Insert (insertar al INICIO, o en posición N)
iptables -I INPUT -p tcp --dport 22 -j ACCEPT
# ↑ Se inserta como regla 1 (antes de todas las demás)

iptables -I INPUT 3 -p tcp --dport 443 -j ACCEPT
# ↑ Se inserta como regla 3
```

**El orden importa**: las reglas se evalúan de arriba a abajo. La primera que coincide determina el destino del paquete. Si una regla DROP está antes de una regla ACCEPT para el mismo tráfico, el paquete se descarta.

```
iptables -A INPUT -p tcp --dport 22 -j DROP
iptables -A INPUT -p tcp --dport 22 -s 10.0.0.0/8 -j ACCEPT
# ↑ La segunda regla NUNCA se ejecuta porque la primera
#   ya capturó todo el tráfico al puerto 22

# CORRECTO: regla más específica primero
iptables -A INPUT -p tcp --dport 22 -s 10.0.0.0/8 -j ACCEPT
iptables -A INPUT -p tcp --dport 22 -j DROP
```

### Eliminar reglas

```bash
# -D: Delete por especificación exacta
iptables -D INPUT -p tcp --dport 80 -j ACCEPT

# -D: Delete por número de regla
iptables -D INPUT 3
# ↑ Elimina la regla 3 de INPUT

# Cuidado: al eliminar una regla, las siguientes se renumeran
# Si borras la 3, la antigua 4 pasa a ser 3
```

### Reemplazar reglas

```bash
# -R: Replace (reemplazar regla en posición N)
iptables -R INPUT 2 -p tcp --dport 443 -j ACCEPT
# Reemplaza la regla 2 de INPUT
```

### Flush (limpiar)

```bash
# -F: Flush (eliminar todas las reglas de una chain)
iptables -F INPUT          # Limpiar solo INPUT
iptables -F                # Limpiar todas las chains de filter
iptables -t nat -F         # Limpiar todas las chains de nat

# CUIDADO: -F no cambia la política por defecto
# Si policy es DROP y haces flush, TODO se bloquea
```

### Políticas

```bash
# -P: Policy (política por defecto de una chain built-in)
iptables -P INPUT DROP
iptables -P FORWARD DROP
iptables -P OUTPUT ACCEPT
```

### Crear y eliminar chains

```bash
# -N: New chain (crear chain personalizada)
iptables -N SSH_RULES

# -X: Delete chain (eliminar chain personalizada vacía)
iptables -X SSH_RULES
# La chain debe estar vacía y no referenciada

# -E: Rename chain
iptables -E SSH_RULES SSH_FILTER
```

---

## 3. Criterios de matching

Los criterios de matching determinan qué paquetes coinciden con una regla. Se pueden combinar (AND lógico — todos deben coincidir):

### Por protocolo

```bash
# -p: protocolo
iptables -A INPUT -p tcp -j ACCEPT       # TCP
iptables -A INPUT -p udp -j ACCEPT       # UDP
iptables -A INPUT -p icmp -j ACCEPT      # ICMP (ping)
iptables -A INPUT -p all -j ACCEPT       # Cualquier protocolo

# Negación con !
iptables -A INPUT ! -p tcp -j DROP       # Todo lo que NO sea TCP
```

### Por dirección IP

```bash
# -s: source (origen)
iptables -A INPUT -s 192.168.1.100 -j ACCEPT       # IP específica
iptables -A INPUT -s 192.168.1.0/24 -j ACCEPT       # Subred
iptables -A INPUT -s 10.0.0.0/8 -j ACCEPT           # Red grande

# -d: destination (destino)
iptables -A OUTPUT -d 8.8.8.8 -j ACCEPT
iptables -A FORWARD -d 192.168.2.0/24 -j ACCEPT

# Negación
iptables -A INPUT ! -s 10.0.0.0/8 -j DROP           # No desde 10.0.0.0/8
```

### Por interfaz

```bash
# -i: input interface (solo en INPUT, FORWARD, PREROUTING)
iptables -A INPUT -i eth0 -j ACCEPT
iptables -A INPUT -i lo -j ACCEPT          # Loopback (importante)

# -o: output interface (solo en OUTPUT, FORWARD, POSTROUTING)
iptables -A OUTPUT -o eth0 -j ACCEPT
iptables -A FORWARD -o eth1 -j ACCEPT

# Wildcard
iptables -A INPUT -i eth+ -j ACCEPT       # eth0, eth1, eth2...
```

### Por puerto (requiere -p tcp o -p udp)

```bash
# --dport: destination port
iptables -A INPUT -p tcp --dport 22 -j ACCEPT
iptables -A INPUT -p tcp --dport 80 -j ACCEPT
iptables -A INPUT -p udp --dport 53 -j ACCEPT

# --sport: source port (raramente útil para filtrado)
iptables -A INPUT -p tcp --sport 443 -j ACCEPT

# Rango de puertos
iptables -A INPUT -p tcp --dport 8000:8099 -j ACCEPT   # 8000 a 8099

# Negación
iptables -A INPUT -p tcp ! --dport 22 -j DROP    # Todo menos SSH
```

### Combinaciones

```bash
# Todos los criterios son AND (deben coincidir todos):
iptables -A INPUT -p tcp -s 10.0.0.0/8 --dport 22 -i eth0 -j ACCEPT
# TCP Y desde 10.0.0.0/8 Y al puerto 22 Y por eth0 → aceptar
```

---

## 4. Match extensions

Las extensiones amplían los criterios de matching más allá de protocolo/IP/puerto:

### conntrack (estado de conexión)

```bash
# -m conntrack --ctstate: estado del connection tracking
iptables -A INPUT -m conntrack --ctstate ESTABLISHED,RELATED -j ACCEPT
iptables -A INPUT -m conntrack --ctstate NEW -p tcp --dport 22 -j ACCEPT
iptables -A INPUT -m conntrack --ctstate INVALID -j DROP
```

> **Nota**: la extensión legacy `-m state --state` sigue funcionando pero `-m conntrack --ctstate` es la forma moderna y recomendada.

### multiport (múltiples puertos)

```bash
# Hasta 15 puertos en una sola regla
iptables -A INPUT -p tcp -m multiport --dports 22,80,443,8080,8443 -j ACCEPT

# También con rangos
iptables -A INPUT -p tcp -m multiport --dports 22,80,443,8000:8099 -j ACCEPT

# Sin multiport, necesitarías una regla por puerto:
iptables -A INPUT -p tcp --dport 22 -j ACCEPT
iptables -A INPUT -p tcp --dport 80 -j ACCEPT
iptables -A INPUT -p tcp --dport 443 -j ACCEPT
# multiport es más eficiente y legible
```

### limit (limitar tasa)

```bash
# Limitar pings a 1 por segundo con ráfaga de 4
iptables -A INPUT -p icmp --icmp-type echo-request \
    -m limit --limit 1/s --limit-burst 4 -j ACCEPT
iptables -A INPUT -p icmp --icmp-type echo-request -j DROP

# Limitar logs para no inundar syslog
iptables -A INPUT -j LOG --log-prefix "DROPPED: " \
    -m limit --limit 5/min --limit-burst 10
```

Unidades de `--limit`: `/second`, `/minute`, `/hour`, `/day`.

`--limit-burst`: número de paquetes iniciales permitidos antes de aplicar el límite.

### recent (rastreo de IPs recientes)

```bash
# Protección contra brute-force SSH:
# Si más de 3 conexiones nuevas en 60 segundos → bloquear

# Registrar nuevas conexiones SSH
iptables -A INPUT -p tcp --dport 22 -m conntrack --ctstate NEW \
    -m recent --set --name SSH

# Si más de 3 en 60 segundos, DROP
iptables -A INPUT -p tcp --dport 22 -m conntrack --ctstate NEW \
    -m recent --update --seconds 60 --hitcount 4 --name SSH -j DROP

# Permitir las que pasan el filtro
iptables -A INPUT -p tcp --dport 22 -m conntrack --ctstate NEW -j ACCEPT
```

### iprange (rango de IPs)

```bash
# Rango que no es una subred CIDR limpia
iptables -A INPUT -m iprange --src-range 192.168.1.100-192.168.1.200 -j ACCEPT
```

### tcp flags

```bash
# Detectar SYN scan (solo flag SYN activo)
iptables -A INPUT -p tcp --tcp-flags ALL SYN -j LOG --log-prefix "SYN-SCAN: "

# Bloquear paquetes "Christmas tree" (todos los flags activos)
iptables -A INPUT -p tcp --tcp-flags ALL ALL -j DROP

# Bloquear paquetes sin flags (null scan)
iptables -A INPUT -p tcp --tcp-flags ALL NONE -j DROP

# Solo SYN nuevos (conexiones legítimas)
iptables -A INPUT -p tcp --syn -m conntrack --ctstate NEW -j ACCEPT
# --syn es shorthand para --tcp-flags SYN,RST,ACK,FIN SYN
```

### comment (comentarios)

```bash
# Añadir comentarios a las reglas para documentación
iptables -A INPUT -p tcp --dport 22 -j ACCEPT \
    -m comment --comment "Allow SSH access"

iptables -A INPUT -s 10.0.0.0/8 -j ACCEPT \
    -m comment --comment "Internal network full access"
```

### hashlimit (límite por IP)

```bash
# Limitar conexiones HTTP POR IP origen (no global)
iptables -A INPUT -p tcp --dport 80 -m hashlimit \
    --hashlimit-above 50/sec \
    --hashlimit-burst 100 \
    --hashlimit-mode srcip \
    --hashlimit-name http_limit \
    -j DROP
```

La diferencia con `limit`: `limit` es global (total), `hashlimit` es por IP/puerto/subred.

---

## 5. Construir un firewall básico paso a paso

### Firewall para un servidor web con SSH

```bash
#!/bin/bash
# firewall.sh — Firewall básico para servidor web

# 1. Limpiar todo (empezar de cero)
iptables -F
iptables -X
iptables -t nat -F
iptables -t mangle -F

# 2. Establecer políticas por defecto (whitelist)
iptables -P INPUT DROP
iptables -P FORWARD DROP
iptables -P OUTPUT ACCEPT

# 3. Permitir loopback (esencial para servicios locales)
iptables -A INPUT -i lo -j ACCEPT

# 4. Permitir conexiones establecidas y relacionadas
iptables -A INPUT -m conntrack --ctstate ESTABLISHED,RELATED -j ACCEPT

# 5. Descartar paquetes inválidos
iptables -A INPUT -m conntrack --ctstate INVALID -j DROP

# 6. Permitir ICMP (ping)
iptables -A INPUT -p icmp --icmp-type echo-request \
    -m limit --limit 1/s --limit-burst 4 -j ACCEPT

# 7. Permitir SSH (con protección anti brute-force)
iptables -A INPUT -p tcp --dport 22 -m conntrack --ctstate NEW \
    -m recent --set --name SSH
iptables -A INPUT -p tcp --dport 22 -m conntrack --ctstate NEW \
    -m recent --update --seconds 60 --hitcount 4 --name SSH -j DROP
iptables -A INPUT -p tcp --dport 22 -j ACCEPT

# 8. Permitir HTTP y HTTPS
iptables -A INPUT -p tcp -m multiport --dports 80,443 -j ACCEPT

# 9. Log de paquetes descartados (para debugging)
iptables -A INPUT -m limit --limit 5/min -j LOG \
    --log-prefix "IPT-DROP: " --log-level 4

# 10. DROP explícito final (redundante con policy, pero documenta)
iptables -A INPUT -j DROP
```

### Por qué cada regla está en ese orden

```
Orden de evaluación (regla 1 se evalúa primero):

1. Loopback      ← Rápido, mucho tráfico local
2. ESTABLISHED   ← Mayoría del tráfico (respuestas)
3. INVALID       ← Descartar basura temprano
4. ICMP          ← Ping (con rate limit)
5. SSH           ← Con protección brute-force
6. HTTP/HTTPS    ← Servicios web
7. LOG           ← Registrar lo que sobra
8. DROP          ← Descartar el resto

Las reglas más frecuentes van primero (rendimiento).
Las reglas de servicios van después del ESTABLISHED
(solo paquetes NEW llegan aquí).
```

### Firewall para un router/gateway NAT

```bash
#!/bin/bash
# Firewall para router que hace NAT

# Limpiar
iptables -F
iptables -X
iptables -t nat -F

# Políticas
iptables -P INPUT DROP
iptables -P FORWARD DROP
iptables -P OUTPUT ACCEPT

# INPUT: solo lo esencial para el router
iptables -A INPUT -i lo -j ACCEPT
iptables -A INPUT -m conntrack --ctstate ESTABLISHED,RELATED -j ACCEPT
iptables -A INPUT -i eth1 -p tcp --dport 22 -j ACCEPT  # SSH solo desde LAN
iptables -A INPUT -i eth1 -p udp --dport 53 -j ACCEPT  # DNS desde LAN
iptables -A INPUT -i eth1 -p udp --dport 67 -j ACCEPT  # DHCP desde LAN
iptables -A INPUT -p icmp -j ACCEPT

# FORWARD: permitir tráfico entre LAN e Internet
# eth0 = WAN (Internet), eth1 = LAN (red interna)
iptables -A FORWARD -i eth1 -o eth0 -j ACCEPT           # LAN → Internet
iptables -A FORWARD -i eth0 -o eth1 \
    -m conntrack --ctstate ESTABLISHED,RELATED -j ACCEPT # Respuestas
# No permitir conexiones nuevas desde Internet a la LAN

# NAT: enmascarar tráfico de la LAN al salir por WAN
iptables -t nat -A POSTROUTING -o eth0 -j MASQUERADE

# IP forwarding (necesario para routing)
echo 1 > /proc/sys/net/ipv4/ip_forward
```

---

## 6. Logging

### LOG target

```bash
# Registrar paquetes antes de descartarlos
iptables -A INPUT -p tcp --dport 23 -j LOG \
    --log-prefix "TELNET-ATTEMPT: " \
    --log-level 4

# LOG no es terminante, añadir DROP después
iptables -A INPUT -p tcp --dport 23 -j DROP
```

`--log-level`: usa niveles de syslog (4=warning, 6=info, 7=debug).

`--log-prefix`: cadena que precede al mensaje (máx 29 chars).

### Ver los logs

```bash
# Según distribución:
journalctl -k | grep "IPT-DROP"
# O:
grep "IPT-DROP" /var/log/kern.log
grep "IPT-DROP" /var/log/messages
# O:
dmesg | grep "IPT-DROP"
```

Formato del log:

```
Mar 21 10:00:00 server kernel: IPT-DROP: IN=eth0 OUT= MAC=aa:bb:cc:dd:ee:ff:11:22:33:44:55:66:08:00
SRC=203.0.113.50 DST=192.168.1.50 LEN=60 TOS=0x00 PREC=0x00 TTL=49 ID=12345 DF
PROTO=TCP SPT=54321 DPT=3389 WINDOW=65535 RES=0x00 SYN URGP=0
```

Campos clave:
- `IN=eth0`: interfaz de entrada
- `SRC/DST`: IPs origen/destino
- `PROTO`: protocolo
- `SPT/DPT`: puertos origen/destino
- `SYN`: flags TCP activos

### Limitar logs

```bash
# Sin limit, un ataque genera miles de líneas por segundo
# Siempre usar limit con LOG:
iptables -A INPUT -m limit --limit 5/min --limit-burst 10 \
    -j LOG --log-prefix "IPT-DROP: "
```

### NFLOG (alternativa a LOG)

```bash
# Enviar a un grupo de netfilter log (procesado por ulogd2)
iptables -A INPUT -j NFLOG --nflog-group 1 --nflog-prefix "DROP"

# ulogd2 puede escribir a archivos, bases de datos, etc.
# Más eficiente que LOG del kernel para alto volumen
```

---

## 7. Persistencia de reglas

Las reglas de iptables **se pierden al reiniciar**. Hay varias formas de hacerlas persistentes:

### iptables-save / iptables-restore

```bash
# Guardar reglas actuales a un archivo
sudo iptables-save > /etc/iptables/rules.v4

# Restaurar reglas desde un archivo
sudo iptables-restore < /etc/iptables/rules.v4

# Para IPv6:
sudo ip6tables-save > /etc/iptables/rules.v6
sudo ip6tables-restore < /etc/iptables/rules.v6
```

Formato del archivo de `iptables-save`:

```
# Generated by iptables-save
*filter
:INPUT DROP [0:0]
:FORWARD DROP [0:0]
:OUTPUT ACCEPT [0:0]
-A INPUT -i lo -j ACCEPT
-A INPUT -m conntrack --ctstate ESTABLISHED,RELATED -j ACCEPT
-A INPUT -p tcp -m tcp --dport 22 -j ACCEPT
-A INPUT -p tcp -m multiport --dports 80,443 -j ACCEPT
COMMIT
*nat
:PREROUTING ACCEPT [0:0]
:INPUT ACCEPT [0:0]
:OUTPUT ACCEPT [0:0]
:POSTROUTING ACCEPT [0:0]
-A POSTROUTING -o eth0 -j MASQUERADE
COMMIT
```

> Puedes editar este archivo directamente. Es más cómodo que ejecutar múltiples comandos `iptables`.

### Paquete iptables-persistent (Debian/Ubuntu)

```bash
sudo apt install iptables-persistent

# Durante la instalación, pregunta si quieres guardar las reglas actuales.
# Las guarda en:
# /etc/iptables/rules.v4
# /etc/iptables/rules.v6

# Para guardar cambios posteriores:
sudo netfilter-persistent save

# Para recargar:
sudo netfilter-persistent reload

# El servicio se activa automáticamente al boot:
sudo systemctl enable netfilter-persistent
```

### Servicio iptables (RHEL/CentOS)

```bash
# RHEL/CentOS con servicio iptables (legacy, no firewalld):
sudo service iptables save
# Guarda en /etc/sysconfig/iptables

sudo systemctl enable iptables
sudo systemctl start iptables
```

### Script en systemd (método genérico)

```ini
# /etc/systemd/system/iptables-rules.service
[Unit]
Description=Restore iptables rules
Before=network-pre.target
Wants=network-pre.target

[Service]
Type=oneshot
ExecStart=/sbin/iptables-restore /etc/iptables/rules.v4
RemainAfterExit=yes

[Install]
WantedBy=multi-user.target
```

```bash
sudo systemctl enable iptables-rules
```

---

## 8. Chains definidas por el usuario

Las chains personalizadas permiten organizar y reutilizar conjuntos de reglas:

```bash
# Crear chain
iptables -N SSH_RULES
iptables -N WEB_RULES
iptables -N RATE_LIMIT

# Poblar chains personalizadas
iptables -A SSH_RULES -s 10.0.0.0/8 -j ACCEPT
iptables -A SSH_RULES -s 172.16.0.0/12 -j ACCEPT
iptables -A SSH_RULES -m limit --limit 5/min -j LOG --log-prefix "SSH-DENIED: "
iptables -A SSH_RULES -j DROP

iptables -A WEB_RULES -m limit --limit 100/s --limit-burst 200 -j ACCEPT
iptables -A WEB_RULES -j DROP

# Invocar desde la chain principal
iptables -A INPUT -p tcp --dport 22 -j SSH_RULES
iptables -A INPUT -p tcp -m multiport --dports 80,443 -j WEB_RULES
```

```
Flujo de evaluación:

Chain INPUT:
  Regla 1: -p tcp --dport 22 → saltar a SSH_RULES
                                    │
                              ┌─────▼──────────────┐
                              │ SSH_RULES:          │
                              │ 1. 10.0.0.0/8 → OK │ → ACCEPT (termina)
                              │ 2. 172.16.0.0 → OK │ → ACCEPT (termina)
                              │ 3. LOG             │ → (continúa)
                              │ 4. DROP            │ → DROP (termina)
                              └────────────────────┘
                              Si ninguna coincide →
                              VUELVE a INPUT regla 2

  Regla 2: -p tcp --dport 80,443 → saltar a WEB_RULES
  ...
```

Ventajas:
- **Organización**: reglas agrupadas por servicio o función
- **Rendimiento**: solo evalúa las reglas relevantes (SSH\_RULES solo se evalúa para tráfico SSH)
- **Reutilización**: la misma chain puede invocarse desde múltiples puntos

---

## 9. Listar, contar y depurar reglas

### Listar reglas

```bash
# Listar reglas de filter (con nombres de servicio)
sudo iptables -L

# Listar con números y sin resolución DNS (más rápido)
sudo iptables -L -n --line-numbers

# Listar con contadores de paquetes/bytes
sudo iptables -L -n -v

# Listar una chain específica
sudo iptables -L INPUT -n --line-numbers

# Listar otra tabla
sudo iptables -t nat -L -n -v
sudo iptables -t mangle -L -n
```

Ejemplo de salida con `-L -n -v --line-numbers`:

```
Chain INPUT (policy DROP 1234 packets, 56789 bytes)
num   pkts bytes target     prot opt in     out     source          destination
1     98765 12M  ACCEPT     all  --  lo     *       0.0.0.0/0       0.0.0.0/0
2     87654 45M  ACCEPT     all  --  *      *       0.0.0.0/0       0.0.0.0/0  ctstate RELATED,ESTABLISHED
3       456  27K ACCEPT     tcp  --  *      *       0.0.0.0/0       0.0.0.0/0  tcp dpt:22
4      2345 140K ACCEPT     tcp  --  *      *       0.0.0.0/0       0.0.0.0/0  multiport dports 80,443
5       123 7380 LOG        all  --  *      *       0.0.0.0/0       0.0.0.0/0  limit: avg 5/min LOG "IPT-DROP: "
6       123 7380 DROP       all  --  *      *       0.0.0.0/0       0.0.0.0/0
```

Columnas:
- `num`: número de regla (para -D o -I posicional)
- `pkts/bytes`: contadores (cuántos paquetes coincidieron)
- `target`: qué hace la regla
- `prot/opt/in/out`: protocolo, opciones, interfaces
- `source/destination`: IPs

### Formato save (más legible para scripts)

```bash
# Formato que puedes copiar/pegar/restaurar
sudo iptables-save
sudo iptables-save -t filter    # Solo tabla filter
```

### Resetear contadores

```bash
# Resetear contadores de todas las chains
sudo iptables -Z

# Resetear contadores de una chain
sudo iptables -Z INPUT

# Resetear contadores de una regla específica
sudo iptables -Z INPUT 3
```

### Depurar: ¿por qué se descarta un paquete?

```bash
# 1. Ver los contadores antes y después de la prueba
sudo iptables -L INPUT -n -v --line-numbers
# Anotar los contadores
# Ejecutar el test (curl, ping, ssh...)
sudo iptables -L INPUT -n -v --line-numbers
# Comparar: ¿qué regla incrementó su contador?

# 2. Añadir LOG antes de DROP para ver qué se descarta
sudo iptables -I INPUT 1 -m limit --limit 10/s \
    -j LOG --log-prefix "DEBUG-INPUT: "
# Ver los logs:
sudo dmesg -w | grep "DEBUG-INPUT"
# ¡Eliminar cuando termines!
sudo iptables -D INPUT 1

# 3. Usar TRACE (muy detallado, solo para debugging)
# Marca paquetes para tracing
sudo iptables -t raw -A PREROUTING -p tcp --dport 80 -j TRACE
# Ver en logs cada regla que evalúa
sudo dmesg | grep "TRACE"
# ¡Genera MUCHA salida! Eliminar cuando termines
sudo iptables -t raw -D PREROUTING -p tcp --dport 80 -j TRACE
```

---

## 10. Errores comunes

### Error 1: Flush sin considerar la política

```
PROBLEMA:
iptables -P INPUT DROP
# ... muchas reglas ...
iptables -F INPUT    # Flush: elimina todas las reglas
# Resultado: policy DROP sin reglas → TODO bloqueado
# Si estás en SSH → desconectado inmediatamente

CORRECTO:
# Opción 1: cambiar la política ANTES del flush
iptables -P INPUT ACCEPT
iptables -F INPUT
# Luego reconstruir las reglas y cambiar la política al final

# Opción 2: usar un script que haga todo atómicamente
iptables-restore < /etc/iptables/rules.v4
# restore aplica todas las reglas de golpe (atómico)
```

### Error 2: Olvidar la regla ESTABLISHED,RELATED

```
PROBLEMA:
iptables -P INPUT DROP
iptables -A INPUT -p tcp --dport 22 -j ACCEPT
iptables -A INPUT -p tcp --dport 80 -j ACCEPT
# "SSH funciona pero no puedo navegar desde el servidor"
# "apt update falla"

# Las RESPUESTAS de conexiones salientes son tráfico INPUT
# Sin ESTABLISHED, las respuestas de DNS, HTTP, etc. se descartan

CORRECTO:
iptables -P INPUT DROP
iptables -A INPUT -i lo -j ACCEPT
iptables -A INPUT -m conntrack --ctstate ESTABLISHED,RELATED -j ACCEPT
iptables -A INPUT -p tcp --dport 22 -j ACCEPT
iptables -A INPUT -p tcp --dport 80 -j ACCEPT
```

### Error 3: Reglas duplicadas tras ejecutar el script varias veces

```
PROBLEMA:
# Ejecutas firewall.sh una vez → funciona
# Lo ejecutas otra vez → reglas duplicadas
# Lo ejecutas 10 veces → 10 copias de cada regla

# -A (append) siempre añade al final, no verifica duplicados

CORRECTO:
# Siempre hacer flush ANTES de añadir reglas:
#!/bin/bash
iptables -F
iptables -X
# ... luego añadir reglas ...

# O usar iptables-restore que es atómico y reemplaza todo
```

### Error 4: Bloquear OUTPUT sin necesidad

```
PROBLEMA:
iptables -P OUTPUT DROP
iptables -A OUTPUT -p tcp --dport 80 -j ACCEPT
iptables -A OUTPUT -p tcp --dport 443 -j ACCEPT
# "No funciona DNS"
# "No funciona NTP"
# "No funciona apt (requiere DNS + HTTPS)"

# Bloquear OUTPUT requiere permitir CADA servicio saliente
# explícitamente — muy tedioso y propenso a errores

CORRECTO:
# Para la mayoría de servidores, OUTPUT ACCEPT es apropiado:
iptables -P OUTPUT ACCEPT
# El firewall protege lo que ENTRA (INPUT/FORWARD)

# Solo bloquea OUTPUT si tienes un requisito específico
# (ej: servidor comprometido que no debe hacer conexiones salientes)
```

### Error 5: Orden incorrecto de reglas

```
PROBLEMA:
iptables -A INPUT -j DROP                       # Regla 1: DROP todo
iptables -A INPUT -p tcp --dport 22 -j ACCEPT   # Regla 2: nunca se evalúa

# La regla 1 coincide con TODOS los paquetes y los descarta
# La regla 2 nunca se alcanza

CORRECTO:
iptables -A INPUT -p tcp --dport 22 -j ACCEPT   # Regla 1: SSH primero
iptables -A INPUT -j DROP                        # Regla 2: DROP el resto

# O usar -I para insertar al inicio:
iptables -I INPUT 1 -p tcp --dport 22 -j ACCEPT
```

---

## 11. Cheatsheet

```
OPERACIONES
──────────────────────────────────────────────
-A CHAIN         Append (añadir al final)
-I CHAIN [N]     Insert (al inicio o posición N)
-D CHAIN N       Delete por número
-D CHAIN RULE    Delete por especificación
-R CHAIN N RULE  Replace regla N
-F [CHAIN]       Flush (limpiar reglas)
-X [CHAIN]       Delete chain personalizada
-N CHAIN         New chain personalizada
-P CHAIN TARGET  Policy por defecto
-L [-n] [-v]     List (listar reglas)
-Z [CHAIN [N]]   Zero (resetear contadores)
-t TABLE         Especificar tabla (default: filter)

MATCHING BÁSICO
──────────────────────────────────────────────
-p tcp/udp/icmp           Protocolo
-s IP[/mask]              IP origen
-d IP[/mask]              IP destino
-i IFACE                  Interfaz de entrada
-o IFACE                  Interfaz de salida
--dport PORT[:PORT]       Puerto destino (con -p tcp/udp)
--sport PORT[:PORT]       Puerto origen
! CRITERIO                Negación

MATCH EXTENSIONS
──────────────────────────────────────────────
-m conntrack --ctstate STATE     Estado de conexión
-m multiport --dports P1,P2,P3  Múltiples puertos
-m limit --limit N/TIME          Rate limit (global)
-m hashlimit ...                 Rate limit (por IP)
-m recent --set/--update         Rastreo de IPs
-m iprange --src-range A-B       Rango de IPs
-m comment --comment "TEXT"      Comentario
-m tcp --tcp-flags MASK FLAGS    Flags TCP

TARGETS
──────────────────────────────────────────────
-j ACCEPT     Aceptar
-j DROP       Descartar (silencioso)
-j REJECT     Descartar (con ICMP)
-j LOG        Registrar y continuar
-j CHAIN      Saltar a chain personalizada

PERSISTENCIA
──────────────────────────────────────────────
iptables-save > /etc/iptables/rules.v4    Guardar
iptables-restore < /etc/iptables/rules.v4 Restaurar

Debian/Ubuntu: apt install iptables-persistent
               netfilter-persistent save/reload

PLANTILLA BÁSICA
──────────────────────────────────────────────
iptables -F && iptables -X
iptables -P INPUT DROP
iptables -P FORWARD DROP
iptables -P OUTPUT ACCEPT
iptables -A INPUT -i lo -j ACCEPT
iptables -A INPUT -m conntrack --ctstate ESTABLISHED,RELATED -j ACCEPT
iptables -A INPUT -m conntrack --ctstate INVALID -j DROP
iptables -A INPUT -p icmp --icmp-type echo-request -j ACCEPT
iptables -A INPUT -p tcp --dport 22 -j ACCEPT
# ... servicios adicionales ...
iptables -A INPUT -j DROP

DEPURACIÓN
──────────────────────────────────────────────
iptables -L -n -v --line-numbers   Reglas con contadores
iptables -Z                        Resetear contadores
iptables -t raw -A PREROUTING -p tcp --dport 80 -j TRACE
dmesg | grep TRACE                 Ver evaluación paso a paso
```

---

## 12. Ejercicios

### Ejercicio 1: Construir un firewall desde cero

Escribe las reglas de iptables para un servidor que:
- Ejecuta SSH (puerto 22), solo accesible desde 10.0.0.0/8
- Ejecuta un servidor web (puertos 80, 443), accesible desde cualquier lugar
- Ejecuta PostgreSQL (puerto 5432), solo accesible desde 192.168.1.0/24
- Permite ping con rate limit de 2/segundo
- Registra en log los paquetes descartados (con rate limit en los logs)
- Política DROP para INPUT y FORWARD, ACCEPT para OUTPUT

Verifica tu solución respondiendo: si un host en 172.16.0.5 intenta SSH, ¿qué pasa? ¿Y si intenta acceder al puerto 80?

### Ejercicio 2: Interpretar y depurar reglas

Dadas estas reglas:

```bash
iptables -P INPUT ACCEPT
iptables -A INPUT -p tcp --dport 22 -s 10.0.0.0/8 -j ACCEPT
iptables -A INPUT -p tcp --dport 22 -j DROP
iptables -A INPUT -p tcp --dport 80 -j ACCEPT
iptables -A INPUT -p icmp -j DROP
```

1. ¿Qué pasa con un ping desde 8.8.8.8?
2. ¿Qué pasa con SSH desde 10.0.0.5?
3. ¿Qué pasa con SSH desde 203.0.113.1?
4. ¿Qué pasa con tráfico HTTP desde cualquier lugar?
5. ¿Qué pasa con tráfico DNS (UDP 53) desde cualquier lugar?
6. La política es ACCEPT. ¿Es un problema de seguridad? ¿Qué tráfico pasa que probablemente no debería?
7. ¿Falta la regla ESTABLISHED,RELATED? ¿Qué consecuencias tiene en este caso específico (policy ACCEPT)?

### Ejercicio 3: Contadores y diagnóstico

En un servidor con el firewall del ejercicio 1 activo:

1. Ejecuta `iptables -L INPUT -n -v --line-numbers` y copia la salida.
2. Resetea los contadores con `iptables -Z INPUT`.
3. Desde otra máquina (o con `curl localhost`), intenta acceder al puerto 80, al puerto 22, y haz un ping.
4. Vuelve a listar con `-v`. ¿Qué reglas incrementaron sus contadores?
5. Intenta acceder a un puerto no permitido (ej: 3306). ¿Qué regla captura el tráfico? ¿Aparece en los logs?
6. Si la regla LOG muestra 0 paquetes pero sabes que hay paquetes descartados, ¿qué está mal? (Pista: ¿la regla LOG está antes o después del DROP?)

**Pregunta de reflexión**: Un administrador tiene un script `firewall.sh` con 200 reglas iptables. Cada vez que modifica una regla, ejecuta el script completo. ¿Qué problemas tiene este enfoque? ¿Por qué `iptables-restore` es mejor que ejecutar 200 comandos `iptables` secuencialmente? (Pista: piensa en atomicidad y en qué pasa entre el flush y la última regla)

---

> **Siguiente tema**: T03 — NAT con iptables (SNAT, DNAT, MASQUERADE, port forwarding)
