# nmap — Escaneo de puertos y detección de servicios

## Índice

1. [Qué es nmap y uso legítimo](#1-qué-es-nmap-y-uso-legítimo)
2. [Instalación y sintaxis básica](#2-instalación-y-sintaxis-básica)
3. [Descubrimiento de hosts](#3-descubrimiento-de-hosts)
4. [Técnicas de escaneo de puertos](#4-técnicas-de-escaneo-de-puertos)
5. [Especificación de puertos](#5-especificación-de-puertos)
6. [Estados de puerto](#6-estados-de-puerto)
7. [Detección de servicios y versiones](#7-detección-de-servicios-y-versiones)
8. [Detección de sistema operativo](#8-detección-de-sistema-operativo)
9. [Scripts NSE (Nmap Scripting Engine)](#9-scripts-nse-nmap-scripting-engine)
10. [Formatos de salida](#10-formatos-de-salida)
11. [Escenarios de uso legítimo](#11-escenarios-de-uso-legítimo)
12. [Errores comunes](#12-errores-comunes)
13. [Cheatsheet](#13-cheatsheet)
14. [Ejercicios](#14-ejercicios)

---

## 1. Qué es nmap y uso legítimo

nmap (Network Mapper) es la herramienta estándar de la industria para
descubrimiento de red y auditoría de seguridad. Permite determinar qué hosts
están activos, qué puertos tienen abiertos, qué servicios corren y qué sistema
operativo usan.

### Uso legítimo vs ilegítimo

```
✓ USO LEGÍTIMO:
  - Auditar TUS propios servidores y redes
  - Pentesting autorizado (con contrato/permiso escrito)
  - Verificar que el firewall bloquea lo esperado
  - Inventariar servicios en tu infraestructura
  - CTF (Capture The Flag) y labs de práctica
  - Verificar que un servidor solo expone los puertos necesarios

✗ USO ILEGÍTIMO:
  - Escanear redes o servidores ajenos sin autorización
  - Buscar vulnerabilidades en sistemas sin permiso
  - Escanear infraestructura de producción ajena
```

**Importante**: escanear sistemas sin autorización es ilegal en la mayoría
de jurisdicciones. En un contexto profesional, siempre ten autorización
escrita antes de escanear. En este curso, todos los ejercicios se realizan
contra máquinas locales o labs controlados.

### Instalación

```bash
# Debian/Ubuntu
apt install nmap

# RHEL/Fedora
dnf install nmap

# Verificar
nmap --version
```

---

## 2. Instalación y sintaxis básica

### Formato general

```
nmap [tipo de escaneo] [opciones] <objetivo>
```

### Especificar objetivos

```bash
# IP individual
nmap 192.168.1.1

# Múltiples IPs
nmap 192.168.1.1 192.168.1.2 192.168.1.3

# Rango de IPs
nmap 192.168.1.1-50

# Subred completa (CIDR)
nmap 192.168.1.0/24

# Nombre de host
nmap server.example.com

# Desde archivo (una IP/hostname por línea)
nmap -iL targets.txt

# Excluir hosts
nmap 192.168.1.0/24 --exclude 192.168.1.1,192.168.1.2
nmap 192.168.1.0/24 --excludefile excluded.txt
```

### Escaneo más básico

```bash
# Escaneo por defecto (sin root): TCP Connect a los 1000 puertos más comunes
nmap 192.168.1.1

# Salida típica:
# Starting Nmap 7.94 ( https://nmap.org )
# Nmap scan report for 192.168.1.1
# Host is up (0.0015s latency).
# Not shown: 997 closed tcp ports (conn-refused)
# PORT    STATE SERVICE
# 22/tcp  open  ssh
# 80/tcp  open  http
# 443/tcp open  https
#
# Nmap done: 1 IP address (1 host up) scanned in 0.52 seconds
```

---

## 3. Descubrimiento de hosts

Antes de escanear puertos, nmap determina qué hosts están activos. En una
red local esto es más fiable que en Internet.

### Ping scan (descubrimiento sin escaneo de puertos)

```bash
# Solo descubrir hosts activos (no escanear puertos)
nmap -sn 192.168.1.0/24

# Salida:
# Nmap scan report for 192.168.1.1
# Host is up (0.0010s latency).
# Nmap scan report for 192.168.1.10
# Host is up (0.0025s latency).
# Nmap scan report for 192.168.1.50
# Host is up (0.0032s latency).
# Nmap done: 256 IP addresses (3 hosts up) scanned in 2.41 seconds
```

### Métodos de descubrimiento

```bash
# ARP ping (red local, más fiable, requiere root)
sudo nmap -sn -PR 192.168.1.0/24

# ICMP echo (ping clásico)
nmap -sn -PE 192.168.1.0/24

# TCP SYN al puerto 443 (atraviesa firewalls que bloquean ICMP)
nmap -sn -PS443 192.168.1.0/24

# TCP ACK al puerto 80
nmap -sn -PA80 192.168.1.0/24

# UDP al puerto 53
nmap -sn -PU53 192.168.1.0/24

# Combinar métodos (más fiable)
sudo nmap -sn -PE -PS22,80,443 -PA80,443 192.168.1.0/24
```

### Comportamiento según privilegios

```
Sin root (usuario normal):
  - Red local: TCP Connect al puerto 80 y 443
  - No puede usar ARP ni raw sockets

Con root:
  - Red local: ARP ping (más fiable y rápido)
  - Red remota: ICMP echo + TCP SYN 443 + TCP ACK 80
```

### Saltar el descubrimiento

```bash
# Tratar todos los hosts como activos (útil cuando ICMP está bloqueado)
nmap -Pn 192.168.1.1
# Escanea puertos directamente sin verificar si el host responde
# Más lento si hay IPs inactivas en el rango
```

---

## 4. Técnicas de escaneo de puertos

### TCP Connect (-sT) — sin root

```bash
nmap -sT 192.168.1.1
```

Completa el handshake TCP completo (SYN → SYN-ACK → ACK). Es el único
escaneo disponible sin privilegios de root.

```
Tu PC                    Servidor
  │── SYN ──────────────▶│
  │◀── SYN-ACK ─────────│   Puerto abierto: completa handshake
  │── ACK ──────────────▶│
  │── RST ──────────────▶│   Cierra la conexión inmediatamente

  │── SYN ──────────────▶│
  │◀── RST ──────────────│   Puerto cerrado: recibe RST

  │── SYN ──────────────▶│
  │        (silencio)     │   Puerto filtrado: sin respuesta
```

Ventaja: no requiere root.
Desventaja: más lento, más detectable (logs en el servidor).

### TCP SYN (-sS) — con root (default)

```bash
sudo nmap -sS 192.168.1.1
```

Envía SYN pero **no completa el handshake** (half-open scan). Al recibir
SYN-ACK, envía RST en lugar de ACK.

```
Tu PC                    Servidor
  │── SYN ──────────────▶│
  │◀── SYN-ACK ─────────│   Puerto abierto: SYN-ACK recibido
  │── RST ──────────────▶│   NO se completa la conexión

  │── SYN ──────────────▶│
  │◀── RST ──────────────│   Puerto cerrado: RST recibido

  │── SYN ──────────────▶│
  │        (silencio)     │   Puerto filtrado: sin respuesta
```

Ventaja: más rápido, menos probable que genere logs (no hay conexión completa).
Es el escaneo **por defecto cuando se ejecuta como root**.

### UDP (-sU)

```bash
sudo nmap -sU 192.168.1.1
```

Los servicios UDP (DNS, DHCP, SNMP, NTP) no responden con handshake. nmap
envía un paquete UDP y espera:

```
Respuesta UDP        → Puerto abierto
ICMP Port Unreach    → Puerto cerrado
Sin respuesta        → open|filtered (no se puede saber)
```

**Predicción**: el escaneo UDP es mucho más lento que TCP porque hay que
esperar timeouts. Un escaneo UDP de 1000 puertos puede tardar minutos.

```bash
# Escanear solo los puertos UDP más importantes
sudo nmap -sU --top-ports 20 192.168.1.1
```

### Combinar TCP y UDP

```bash
# Escaneo TCP SYN + UDP en un solo comando
sudo nmap -sS -sU --top-ports 100 192.168.1.1
```

### Otros tipos de escaneo

```bash
# TCP ACK (-sA): no determina si el puerto está abierto,
# sino si está FILTRADO por un firewall stateful
sudo nmap -sA 192.168.1.1
# unfiltered = el firewall no bloquea (pero el puerto puede estar cerrado)
# filtered = el firewall bloquea

# TCP Window (-sW): similar a ACK pero examina el window size
sudo nmap -sW 192.168.1.1

# TCP Null, FIN, Xmas: envían paquetes con flags inusuales
sudo nmap -sN 192.168.1.1    # sin flags
sudo nmap -sF 192.168.1.1    # solo FIN
sudo nmap -sX 192.168.1.1    # FIN+PSH+URG
# Según RFC, puertos cerrados responden con RST, abiertos no responden
# No funciona en Windows (no sigue la RFC)
```

### Comparación de escaneos

| Tipo | Flag | Root | Velocidad | Detectabilidad | Fiabilidad |
|------|------|------|-----------|----------------|------------|
| TCP Connect | `-sT` | No | Media | Alta (logs) | Alta |
| TCP SYN | `-sS` | Sí | Rápida | Media | Alta |
| UDP | `-sU` | Sí | Lenta | Baja | Media |
| TCP ACK | `-sA` | Sí | Rápida | Media | Solo filtraje |

---

## 5. Especificación de puertos

### Opciones de selección

```bash
# Puertos específicos
nmap -p 22,80,443 192.168.1.1

# Rango de puertos
nmap -p 1-1024 192.168.1.1

# Todos los puertos (1-65535)
nmap -p- 192.168.1.1

# Puertos más comunes (default: 1000)
nmap --top-ports 100 192.168.1.1
nmap --top-ports 10 192.168.1.1

# Especificar protocolo
nmap -p T:80,443,U:53,161 192.168.1.1
#        TCP          UDP

# Por nombre de servicio
nmap -p ssh,http,https 192.168.1.1
```

### Los 1000 puertos por defecto

nmap no escanea los puertos del 1 al 1000 por defecto. Escanea los **1000
puertos más comúnmente encontrados abiertos**, basados en estadísticas reales.
Esto incluye puertos como 8080, 3306, 8443, etc. que están fuera del rango
1-1024.

```bash
# Ver qué puertos se escanean por defecto (top 10)
nmap --top-ports 10 -oG - 192.168.1.1 | grep "Ports scanned"
# Ports scanned: TCP(10;21-23,25,80,110,139,443,445,3389)
```

### Velocidad vs completitud

```bash
# Rápido: solo top 100 puertos (~2 segundos)
nmap --top-ports 100 192.168.1.1

# Default: top 1000 puertos (~5-10 segundos)
nmap 192.168.1.1

# Completo: todos los 65535 puertos (~5-15 minutos)
nmap -p- 192.168.1.1

# Completo + rápido (con timing agresivo, solo en tu red)
nmap -p- -T4 192.168.1.1
```

---

## 6. Estados de puerto

nmap clasifica cada puerto en uno de seis estados:

### open

```
PORT    STATE SERVICE
22/tcp  open  ssh
```

El puerto acepta conexiones. Un servicio está escuchando.

### closed

```
PORT    STATE  SERVICE
23/tcp  closed telnet
```

El puerto es accesible (responde con RST) pero no hay servicio escuchando.
Indica que el host está activo y no hay firewall bloqueando.

### filtered

```
PORT    STATE    SERVICE
22/tcp  filtered ssh
```

No se puede determinar si está abierto o cerrado. Un firewall, filtro de
paquetes u otro dispositivo impide que los sondeos lleguen al puerto.
nmap no recibe respuesta ni RST.

### unfiltered

```
PORT    STATE      SERVICE
80/tcp  unfiltered http
```

El puerto es accesible pero nmap no puede determinar si está abierto o
cerrado. Solo aparece con escaneo ACK (`-sA`). Indica que no hay firewall
stateful filtrando.

### open|filtered

```
PORT    STATE         SERVICE
53/udp  open|filtered domain
```

nmap no puede distinguir entre abierto y filtrado. Típico de escaneos UDP
donde no hay respuesta: puede ser que el servicio no respondió o que un
firewall descartó el paquete.

### closed|filtered

Raro. Solo aparece con escaneos IP ID idle.

### Tabla resumen

```
Estado          Qué respondió          Significado
──────────────  ─────────────────────  ─────────────────────────
open            SYN-ACK (TCP)          Servicio escuchando
                Respuesta UDP (UDP)
closed          RST (TCP)              Sin servicio, sin firewall
                ICMP unreach (UDP)
filtered        Sin respuesta          Firewall descarta
                ICMP prohibited
unfiltered      (solo con -sA)         Sin firewall stateful
open|filtered   Sin respuesta (UDP)    No se puede determinar
```

---

## 7. Detección de servicios y versiones

### -sV: detección de versiones

```bash
nmap -sV 192.168.1.1

# Salida:
# PORT    STATE SERVICE  VERSION
# 22/tcp  open  ssh      OpenSSH 8.9p1 Ubuntu 3ubuntu0.6 (Ubuntu Linux; protocol 2.0)
# 80/tcp  open  http     nginx 1.24.0
# 443/tcp open  ssl/http nginx 1.24.0
# 3306/tcp open mysql    MySQL 8.0.36-0ubuntu0.22.04.1
```

nmap se conecta al puerto y analiza los banners y respuestas del servicio
para determinar el software y la versión exacta.

### Niveles de intensidad

```bash
# Intensidad 0-9 (default: 7)
nmap -sV --version-intensity 0 192.168.1.1    # light, solo banners
nmap -sV --version-intensity 9 192.168.1.1    # máximo, todas las sondas

# Atajos
nmap -sV --version-light 192.168.1.1          # intensidad 2
nmap -sV --version-all 192.168.1.1            # intensidad 9
```

### Qué revela la detección de versiones

```bash
$ sudo nmap -sV -p 22,80,443,3306,5432,6379,27017 192.168.1.100

PORT      STATE  SERVICE    VERSION
22/tcp    open   ssh        OpenSSH 8.9p1 Ubuntu 3 (protocol 2.0)
80/tcp    open   http       nginx 1.24.0
443/tcp   open   ssl/http   nginx 1.24.0
3306/tcp  open   mysql      MySQL 8.0.36
5432/tcp  closed postgresql
6379/tcp  open   redis      Redis key-value store 7.2.4
27017/tcp open   mongodb    MongoDB 7.0.5
```

Esta información es valiosa para:
- **Inventario**: saber qué versiones corren en producción
- **Vulnerabilidades**: verificar si las versiones tienen CVEs conocidos
- **Auditoría**: detectar servicios que no deberían estar expuestos

**Predicción**: en el ejemplo anterior, Redis (6379) y MongoDB (27017)
abiertos a la red son señales de alarma — normalmente solo deberían
escuchar en localhost.

---

## 8. Detección de sistema operativo

### -O: OS detection

```bash
sudo nmap -O 192.168.1.1

# Salida:
# OS details: Linux 5.15 - 6.1
# Network Distance: 1 hop

# O más detallado con posibilidades:
# OS CPE: cpe:/o:linux:linux_kernel:5.15
# OS details: Linux 5.15 - 6.1
# Aggressive OS guesses: Linux 5.15 (98%), Linux 6.1 (95%), ...
```

nmap analiza peculiaridades de la pila TCP/IP (valores iniciales de TTL,
window size, opciones TCP, respuestas a sondeos inusuales) para adivinar
el sistema operativo.

```bash
# Requiere al menos un puerto abierto y uno cerrado
# Si no encuentra un puerto cerrado:
# Warning: OSScan results may be unreliable because we could not find
# at least 1 open and 1 closed port

# Limitar intentos de detección de OS
sudo nmap -O --osscan-limit 192.168.1.1     # solo si hay condiciones ideales
sudo nmap -O --osscan-guess 192.168.1.1     # adivinar más agresivamente
```

### Combinación completa: -A

```bash
# -A = -sV + -O + scripts default + traceroute
sudo nmap -A 192.168.1.1

# Es el escaneo más informativo pero también el más lento y ruidoso
```

---

## 9. Scripts NSE (Nmap Scripting Engine)

NSE permite ejecutar scripts Lua que extienden la funcionalidad de nmap.
Hay cientos de scripts para detección de vulnerabilidades, enumeración de
servicios y más.

### Categorías de scripts

| Categoría | Descripción | Seguro para producción |
|-----------|-------------|----------------------|
| `default` | Scripts seguros y útiles | Sí |
| `safe` | No afectan al objetivo | Sí |
| `discovery` | Descubrimiento de información | Generalmente sí |
| `version` | Detección avanzada de versiones | Sí |
| `auth` | Verificar autenticación | Con cuidado |
| `vuln` | Detección de vulnerabilidades | Con precaución |
| `intrusive` | Pueden afectar al servicio | No sin autorización |
| `exploit` | Intentan explotar vulnerabilidades | Solo en labs |
| `brute` | Fuerza bruta de credenciales | Solo en labs |

### Usar scripts

```bash
# Scripts por defecto (-sC es equivalente a --script=default)
nmap -sC 192.168.1.1

# Categoría específica
nmap --script=safe 192.168.1.1
nmap --script=vuln 192.168.1.1

# Script individual
nmap --script=http-title 192.168.1.1
nmap --script=ssh-hostkey 192.168.1.1

# Múltiples scripts
nmap --script=http-title,http-headers 192.168.1.1

# Todos los scripts HTTP
nmap --script="http-*" 192.168.1.1

# Pasar argumentos a scripts
nmap --script=http-brute --script-args='userdb=users.txt,passdb=pass.txt' 192.168.1.1
```

### Scripts útiles para administración

```bash
# Ver título de páginas web
nmap --script=http-title -p 80,443,8080 192.168.1.0/24

# Verificar versiones SSL/TLS
nmap --script=ssl-enum-ciphers -p 443 192.168.1.1

# Detectar certificados SSL
nmap --script=ssl-cert -p 443 192.168.1.1

# Enumerar métodos HTTP permitidos
nmap --script=http-methods -p 80,443 192.168.1.1

# Verificar si SSH permite autenticación por contraseña
nmap --script=ssh-auth-methods -p 22 192.168.1.1

# Información de SMB (compartir archivos Windows/Samba)
nmap --script=smb-os-discovery -p 445 192.168.1.1

# DNS zone transfer (verificar si el DNS permite transferencia)
nmap --script=dns-zone-transfer --script-args='dns-zone-transfer.domain=example.com' -p 53 192.168.1.1
```

### Listar scripts disponibles

```bash
# Ver todos los scripts instalados
ls /usr/share/nmap/scripts/

# Buscar scripts por nombre
ls /usr/share/nmap/scripts/ | grep ssh
ls /usr/share/nmap/scripts/ | grep http

# Ver ayuda de un script
nmap --script-help=http-title
```

---

## 10. Formatos de salida

### Salida a archivo

```bash
# Normal (mismo formato que pantalla)
nmap -oN scan_results.txt 192.168.1.0/24

# XML (para parsear con herramientas)
nmap -oX scan_results.xml 192.168.1.0/24

# Grepable (una línea por host, fácil de filtrar)
nmap -oG scan_results.gnmap 192.168.1.0/24

# Los tres formatos a la vez
nmap -oA scan_results 192.168.1.0/24
# Crea: scan_results.nmap, scan_results.xml, scan_results.gnmap
```

### Formato grepable — útil para scripts

```bash
nmap -oG - 192.168.1.0/24

# Salida:
# Host: 192.168.1.1 ()  Ports: 22/open/tcp//ssh///, 80/open/tcp//http///
# Host: 192.168.1.10 () Ports: 22/open/tcp//ssh///, 3306/open/tcp//mysql///

# Filtrar hosts con puerto 80 abierto
nmap -oG - 192.168.1.0/24 | grep "80/open"

# Extraer IPs con SSH abierto
nmap -oG - 192.168.1.0/24 | grep "22/open" | awk '{print $2}'
```

### Verbosidad y debugging

```bash
# Verbosidad (más información durante el escaneo)
nmap -v 192.168.1.1      # nivel 1
nmap -vv 192.168.1.1     # nivel 2

# Debug (mucho más detalle, para troubleshooting)
nmap -d 192.168.1.1
nmap -dd 192.168.1.1

# Mostrar razón del estado del puerto
nmap --reason 192.168.1.1
# PORT   STATE  SERVICE REASON
# 22/tcp open   ssh     syn-ack ttl 64
# 23/tcp closed telnet  reset ttl 64
```

---

## 11. Escenarios de uso legítimo

### Escenario 1: auditoría de servidor propio

```bash
# ¿Qué puertos tengo expuestos? (debería coincidir con lo esperado)
sudo nmap -sS -sV -p- 192.168.1.100

# Comparar con lo que ss muestra internamente:
ssh user@192.168.1.100 'sudo ss -tlnp'

# Si nmap muestra puertos que ss no muestra:
# → un dispositivo intermedio responde en nombre del servidor
# Si ss muestra puertos que nmap no muestra:
# → el firewall los bloquea correctamente
```

### Escenario 2: verificar firewall

```bash
# Desde FUERA del firewall: ¿qué ve un atacante?
nmap -sS -sV --top-ports 1000 mi-servidor-publico.com

# Resultado esperado para un servidor web:
# 22/tcp  open  ssh     (solo si SSH está publicado)
# 80/tcp  open  http
# 443/tcp open  https
# Todo lo demás: filtered o closed

# Si aparece algo inesperado (3306, 6379, 27017):
# → el firewall no está bien configurado
```

### Escenario 3: inventario de red

```bash
# Descubrir todos los hosts activos
sudo nmap -sn 192.168.1.0/24 -oG - | grep "Up" | awk '{print $2}'

# Escanear puertos comunes de todos los hosts activos
sudo nmap -sS -sV --top-ports 20 192.168.1.0/24 -oA network_audit

# Buscar servicios específicos en la red
sudo nmap -sS -p 22 192.168.1.0/24 -oG - | grep "22/open"    # SSH
sudo nmap -sS -p 3389 192.168.1.0/24 -oG - | grep "3389/open" # RDP
sudo nmap -sS -p 445 192.168.1.0/24 -oG - | grep "445/open"   # SMB
```

### Escenario 4: verificar que un servicio solo escucha localmente

```bash
# El servidor PostgreSQL debería escuchar solo en localhost
# Desde otra máquina:
nmap -p 5432 192.168.1.100
# Resultado esperado: closed o filtered
# Si dice "open": PostgreSQL está expuesto → corregir bind_address

# Verificar Redis (debe ser solo local)
nmap -p 6379 192.168.1.100
# Resultado esperado: closed o filtered

# Verificar MongoDB (debe ser solo local)
nmap -p 27017 192.168.1.100
```

### Escenario 5: verificar TLS/SSL

```bash
# Verificar configuración SSL de tu servidor web
nmap --script=ssl-enum-ciphers -p 443 mi-servidor.com

# Buscar:
# - TLSv1.0 o TLSv1.1 → obsoletos, deben deshabilitarse
# - Ciphers débiles (RC4, DES, 3DES) → deben eliminarse
# - TLSv1.2 y TLSv1.3 → correctos

# Verificar certificado
nmap --script=ssl-cert -p 443 mi-servidor.com
# Muestra: subject, issuer, validez, SANs
```

### Timing y rendimiento

```bash
# Plantillas de timing
nmap -T0 192.168.1.1    # paranoid (muy lento, evasión IDS)
nmap -T1 192.168.1.1    # sneaky
nmap -T2 192.168.1.1    # polite
nmap -T3 192.168.1.1    # normal (default)
nmap -T4 192.168.1.1    # aggressive (bueno para red local)
nmap -T5 192.168.1.1    # insane (puede perder resultados)

# Para auditoría de tu propia red:
sudo nmap -T4 -sS -sV -p- 192.168.1.0/24
# -T4 es seguro en red local, NO usar en redes ajenas
```

---

## 12. Errores comunes

### Error 1: escanear sin root y esperar resultados completos

```bash
# ✗ Sin root: solo TCP Connect, sin detección de OS, sin SYN scan
nmap -sS -O 192.168.1.1
# "You requested a SYN scan but you are not root"

# ✓ Usar sudo para escaneos avanzados
sudo nmap -sS -O 192.168.1.1

# ✓ O usar TCP Connect (funciona sin root, menos eficiente)
nmap -sT 192.168.1.1
```

### Error 2: asumir que "filtered" significa "seguro"

```bash
# ✗ "El puerto 3306 está filtered, así que está protegido"
# filtered solo significa que NMAP no puede determinar el estado
# desde ESA ubicación de red

# ✓ Verificar desde diferentes ubicaciones
# Desde Internet: filtered (bien, el firewall bloquea)
# Desde la LAN interna: puede estar open
# → El servicio SÍ está expuesto, solo no desde tu punto de escaneo
```

### Error 3: interpretar "closed" como problema

```bash
# ✗ "El puerto 80 está closed, el servidor web no funciona"
# closed = el host responde con RST, el puerto es accesible
# pero no hay servicio escuchando → normal si no hay web server

# ✓ closed es diferente de filtered
# closed = no hay servicio (y no hay firewall bloqueando)
# filtered = hay firewall (no se sabe si hay servicio o no)
```

### Error 4: escanear -p- sin pensar en el tiempo

```bash
# ✗ Escanear todos los puertos de toda una subred
nmap -p- 192.168.1.0/24
# 256 hosts × 65535 puertos = podría tardar HORAS

# ✓ Ser selectivo
# Primero descubrir hosts
sudo nmap -sn 192.168.1.0/24
# Luego escanear puertos comunes
sudo nmap -sS --top-ports 100 192.168.1.1,10,50,100
# Solo -p- en hosts específicos de interés
sudo nmap -sS -p- 192.168.1.100
```

### Error 5: no guardar resultados

```bash
# ✗ Escaneo largo sin guardar
sudo nmap -sS -sV -p- 192.168.1.0/24
# Si la terminal se cierra, pierdes todo

# ✓ Siempre guardar con -oA
sudo nmap -sS -sV -p- 192.168.1.0/24 -oA scan_$(date +%F)
# Crea scan_2026-03-21.nmap, .xml, .gnmap
```

---

## 13. Cheatsheet

```bash
# ╔══════════════════════════════════════════════════════════════════╗
# ║                    NMAP — CHEATSHEET                           ║
# ╠══════════════════════════════════════════════════════════════════╣
# ║                                                                ║
# ║  DESCUBRIMIENTO:                                              ║
# ║  nmap -sn 192.168.1.0/24           # hosts activos            ║
# ║  nmap -Pn 192.168.1.1              # saltar descubrimiento    ║
# ║                                                                ║
# ║  ESCANEO DE PUERTOS:                                          ║
# ║  nmap 192.168.1.1                   # top 1000 (TCP Connect)  ║
# ║  sudo nmap -sS 192.168.1.1         # TCP SYN (stealth)       ║
# ║  sudo nmap -sU 192.168.1.1         # UDP                     ║
# ║  sudo nmap -sS -sU 192.168.1.1     # TCP + UDP               ║
# ║                                                                ║
# ║  PUERTOS:                                                     ║
# ║  -p 22,80,443                       # puertos específicos     ║
# ║  -p 1-1024                          # rango                   ║
# ║  -p-                                # todos (65535)           ║
# ║  --top-ports 100                    # los 100 más comunes     ║
# ║  -p T:80,U:53                       # TCP y UDP específicos   ║
# ║                                                                ║
# ║  DETECCIÓN:                                                   ║
# ║  -sV                                # versión de servicios    ║
# ║  -O                                 # sistema operativo       ║
# ║  -A                                 # todo (-sV -O -sC)       ║
# ║                                                                ║
# ║  SCRIPTS:                                                     ║
# ║  -sC                                # scripts default         ║
# ║  --script=vuln                      # buscar vulnerabilidades ║
# ║  --script=ssl-enum-ciphers          # verificar TLS           ║
# ║  --script=http-title                # títulos de páginas web  ║
# ║                                                                ║
# ║  SALIDA:                                                      ║
# ║  -oN file.txt                       # formato normal          ║
# ║  -oX file.xml                       # XML                     ║
# ║  -oG file.gnmap                     # grepable                ║
# ║  -oA basename                       # los tres formatos       ║
# ║  --reason                           # mostrar razón           ║
# ║                                                                ║
# ║  VELOCIDAD:                                                   ║
# ║  -T4                                # agresivo (red local)    ║
# ║  -T3                                # normal (default)        ║
# ║                                                                ║
# ║  COMBINACIÓN ÚTIL:                                            ║
# ║  sudo nmap -sS -sV -O -sC -T4 -p- -oA scan target           ║
# ║                                                                ║
# ╚══════════════════════════════════════════════════════════════════╝
```

---

## 14. Ejercicios

### Ejercicio 1: auditoría de tu propia máquina

**Contexto**: necesitas verificar qué puertos están expuestos en tu servidor
o máquina local.

**Tareas**:

1. Ejecuta `ss -tlnp` para ver qué servicios escuchan internamente
2. Desde otra máquina (o usando la interfaz de red, no loopback), ejecuta
   un escaneo nmap completo de puertos TCP
3. Compara las dos listas:
   - ¿Hay puertos que `ss` muestra pero nmap no? (firewall los bloquea)
   - ¿Hay puertos que nmap muestra pero `ss` no? (no debería ocurrir)
4. Ejecuta detección de versiones (`-sV`) en los puertos abiertos
5. Identifica si algún servicio no debería estar expuesto y documenta cómo
   corregirlo (firewall o bind address)

**Pistas**:
- Usa `nmap -sS -sV -p- -T4` para un escaneo completo
- Servicios que escuchan en `0.0.0.0` aparecen en nmap; los que escuchan
  en `127.0.0.1` no
- Bases de datos, Redis, memcached nunca deberían estar expuestos

> **Pregunta de reflexión**: si un servicio escucha en `0.0.0.0:6379` (Redis)
> pero el firewall lo bloquea, ¿es suficiente? ¿Qué pasaría si alguien
> desactiva el firewall temporalmente para diagnosticar algo? ¿Por qué la
> defensa en profundidad (bind address + firewall) es mejor que depender
> de un solo control?

---

### Ejercicio 2: inventario de red local

**Contexto**: acabas de conectar a una red y necesitas inventariar qué
dispositivos y servicios hay.

**Tareas**:

1. Descubre todos los hosts activos con ping scan (`-sn`)
2. Escanea los top 20 puertos de todos los hosts descubiertos
3. Para los hosts con puertos abiertos interesantes, ejecuta detección de
   versiones
4. Genera un reporte en los tres formatos (`-oA`)
5. Usa el formato grepable para responder:
   - ¿Cuántos hosts tienen SSH abierto?
   - ¿Cuántos tienen un servidor web?
   - ¿Hay algún servicio de base de datos expuesto?

**Pistas**:
- Primer paso: `sudo nmap -sn 192.168.1.0/24`
- Segundo paso: `sudo nmap -sS --top-ports 20 -oA inventory <IPs>`
- `grep "22/open" inventory.gnmap | wc -l` para contar SSH
- Solo escanear tu propia red (WiFi de casa, lab virtual)

> **Pregunta de reflexión**: un inventario de red debería hacerse
> periódicamente, no solo una vez. ¿Cómo automatizarías este proceso para
> detectar nuevos servicios que aparecen sin autorización? ¿Qué herramientas
> (ndiff, Nmap + cron) compararían escaneos sucesivos?

---

### Ejercicio 3: verificar la configuración del firewall

**Contexto**: configuraste un firewall (iptables, nftables o firewalld) en
un servidor y necesitas verificar que funciona correctamente.

**Tareas**:

1. Documenta las reglas del firewall y qué puertos DEBERÍAN estar abiertos
2. Desde una máquina externa, ejecuta nmap con diferentes técnicas:
   - TCP SYN scan a todos los puertos
   - UDP scan a los top 20 puertos
   - TCP ACK scan (`-sA`) para verificar el filtrado stateful
3. Compara los resultados con las expectativas:
   - ¿Los puertos que deberían estar abiertos aparecen como `open`?
   - ¿Los puertos que deberían estar bloqueados aparecen como `filtered`?
   - ¿Hay algún puerto `closed` que debería estar `filtered`?
4. Usa `--reason` para entender por qué nmap clasifica cada puerto
5. Si encuentras discrepancias, ajusta las reglas del firewall

**Pistas**:
- `closed` vs `filtered`: closed envía RST (revela que el host existe),
  filtered no responde (más seguro, no da información)
- `nmap -sA` solo verifica si hay firewall stateful, no si el puerto está abierto
- Un firewall con `DROP` produce `filtered`, con `REJECT` produce `closed`

> **Pregunta de reflexión**: ¿es mejor que un firewall use DROP (que produce
> `filtered` en nmap) o REJECT (que produce `closed`)? DROP no revela
> información pero puede causar timeouts en clientes legítimos. REJECT
> responde rápido pero confirma la existencia del host. ¿Cuál es la mejor
> práctica y depende del contexto?

---

> **Siguiente tema**: T05 — curl y wget (pruebas HTTP, flags útiles para diagnóstico)
