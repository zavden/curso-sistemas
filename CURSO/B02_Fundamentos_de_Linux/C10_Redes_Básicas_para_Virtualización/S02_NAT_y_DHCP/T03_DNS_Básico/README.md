# DNS Básico

## Índice

1. [El problema: los humanos no recuerdan IPs](#1-el-problema-los-humanos-no-recuerdan-ips)
2. [Qué es DNS](#2-qué-es-dns)
3. [Jerarquía DNS: del root al registro](#3-jerarquía-dns-del-root-al-registro)
4. [Tipos de registros DNS](#4-tipos-de-registros-dns)
5. [El proceso de resolución paso a paso](#5-el-proceso-de-resolución-paso-a-paso)
6. [TTL y caché](#6-ttl-y-caché)
7. [Herramientas: dig, host, nslookup](#7-herramientas-dig-host-nslookup)
8. [/etc/resolv.conf: configuración del cliente DNS](#8-etcresolvconf-configuración-del-cliente-dns)
9. [/etc/hosts: resolución local sin DNS](#9-etchosts-resolución-local-sin-dns)
10. [/etc/nsswitch.conf: orden de resolución](#10-etcnsswitchconf-orden-de-resolución)
11. [systemd-resolved: el DNS moderno de Linux](#11-systemd-resolved-el-dns-moderno-de-linux)
12. [DNS en libvirt: dnsmasq como forwarder](#12-dns-en-libvirt-dnsmasq-como-forwarder)
13. [Resolución de nombres entre VMs](#13-resolución-de-nombres-entre-vms)
14. [Errores comunes](#14-errores-comunes)
15. [Cheatsheet](#15-cheatsheet)
16. [Ejercicios](#16-ejercicios)

---

## 1. El problema: los humanos no recuerdan IPs

Las máquinas se comunican con direcciones IP (142.250.184.14), pero los humanos recordamos nombres (google.com). DNS es el sistema que traduce entre ambos mundos.

Sin DNS, cada vez que quisieras visitar un sitio web tendrías que recordar su IP. Y peor: las IPs de los servicios cambian (balanceo de carga, CDNs, migraciones), así que la IP que funcionaba ayer podría no funcionar hoy.

En virtualización, DNS es igualmente importante:

```bash
# Sin DNS: necesitas recordar la IP de cada VM
ssh user@192.168.122.100
ssh user@192.168.122.101
ssh user@192.168.122.102

# Con DNS (dnsmasq de libvirt): puedes usar hostnames
ssh user@fedora-vm
ssh user@debian-db
ssh user@centos-web
```

---

## 2. Qué es DNS

DNS (Domain Name System) es una **base de datos distribuida y jerárquica** que mapea nombres de dominio a direcciones IP (y otros datos). Opera en capa 7 (Aplicación), usando principalmente UDP puerto 53 (TCP 53 para transferencias de zona y respuestas grandes).

### Analogía

DNS funciona como un sistema de guías telefónicas distribuido:

```
"¿Cuál es el número de Juan García en Madrid?"

No hay una guía única con TODOS los números del mundo.
Pero hay un sistema de guías:
  1. Guía de España → te dice que Madrid está en la guía regional
  2. Guía de Madrid → te dice el número de Juan García

DNS funciona igual:
  1. Root servers → saben dónde están los .com, .org, .es
  2. TLD servers (.com) → saben dónde está google.com
  3. Authoritative server → tiene la IP de google.com
```

### ¿Por qué distribuido?

Una base de datos centralizada con todos los dominios del mundo sería:
- **Un punto único de fallo**: si cae, nadie puede navegar.
- **Un cuello de botella**: miles de millones de consultas por segundo.
- **Imposible de mantener**: se registran miles de dominios nuevos cada día.

La distribución jerárquica resuelve estos tres problemas: cada nivel solo conoce la siguiente capa, y las respuestas se cachean agresivamente.

---

## 3. Jerarquía DNS: del root al registro

```
                    . (root)
                    │
         ┌──────────┼──────────┐
        .com       .org       .es        ← TLD (Top Level Domain)
         │          │          │
     google.com  wikipedia.org  ...      ← Dominio de segundo nivel
         │
  ┌──────┼───────┐
mail.   www.   maps.                     ← Subdominios
google  google  google
.com    .com    .com
```

### Los 4 niveles de servidores DNS

**1. Root servers (13 clusters)**

Los root servers no conocen la IP de google.com, pero saben quién gestiona `.com`. Son la entrada al sistema DNS. Hay 13 direcciones IP de root servers (letras a–m), pero cada una es un cluster de cientos de servidores distribuidos globalmente mediante anycast.

```bash
# Ver los root servers
dig . NS +short
# a.root-servers.net.
# b.root-servers.net.
# ... (13 total)
```

**2. TLD servers**

Gestionan los dominios de primer nivel: `.com`, `.org`, `.net`, `.es`, `.io`, etc. Verisign gestiona `.com` y `.net`. Cada país tiene su propio gestor para su ccTLD (`.es`, `.uk`, `.de`).

**3. Authoritative servers**

El servidor que tiene la respuesta **definitiva** para un dominio. Si preguntas por `google.com`, el authoritative server de Google responde con la IP.

**4. Recursive resolvers (resolvers recursivos)**

El servidor que tu máquina consulta. No tiene respuestas propias — pregunta a los otros servidores por ti y cachea las respuestas. Ejemplos:

| Resolver | IP | Operador |
|----------|------|---------|
| Google Public DNS | 8.8.8.8, 8.8.4.4 | Google |
| Cloudflare | 1.1.1.1, 1.0.0.1 | Cloudflare |
| Quad9 | 9.9.9.9 | IBM/PCH |
| Tu router | 192.168.1.1 | Tu ISP (reenvía) |
| dnsmasq de libvirt | 192.168.122.1 | Reenvía al DNS del host |

---

## 4. Tipos de registros DNS

Un dominio puede tener múltiples registros de diferentes tipos:

| Tipo | Nombre | Contenido | Ejemplo |
|------|--------|-----------|---------|
| A | Address | IPv4 del dominio | `google.com → 142.250.184.14` |
| AAAA | IPv6 Address | IPv6 del dominio | `google.com → 2a00:1450:4003:80e::200e` |
| CNAME | Canonical Name | Alias a otro dominio | `www.google.com → google.com` |
| MX | Mail Exchange | Servidor de correo | `google.com → smtp.google.com (prioridad 10)` |
| NS | Name Server | Servidor autoritativo | `google.com → ns1.google.com` |
| PTR | Pointer | Resolución inversa (IP→nombre) | `14.184.250.142 → google.com` |
| TXT | Text | Texto libre (SPF, DKIM, verificación) | `google.com → "v=spf1 ..."` |
| SOA | Start of Authority | Información de la zona (serial, TTL) | Metadatos del dominio |
| SRV | Service | Servicio y puerto | `_ssh._tcp.lab.local → 192.168.122.100:22` |

### Registros relevantes para virtualización

| Registro | Uso en VMs |
|----------|-----------|
| A | Resolver el nombre de una VM a su IP (`fedora-vm → 192.168.122.100`) |
| PTR | Resolución inversa (`192.168.122.100 → fedora-vm`) — dnsmasq lo genera automáticamente |
| SRV | Localizar servicios en un lab (ej. dónde está el servidor SSH, el DB) |

---

## 5. El proceso de resolución paso a paso

Cuando escribes `curl google.com`, ocurre esto:

### Paso 1: ¿Ya lo sé localmente?

```
Aplicación (curl) → biblioteca C (getaddrinfo) → nsswitch.conf

nsswitch.conf dice: hosts: files dns mymachines
                           ─────
                           primero /etc/hosts
```

El sistema revisa `/etc/hosts`. Si encuentra `google.com`, usa esa IP y no consulta DNS.

### Paso 2: ¿Está en la caché local?

Si hay un resolver local (systemd-resolved, dnsmasq, nscd), busca en su caché. Si tiene una respuesta reciente y su TTL no expiró, la devuelve sin consultar ningún servidor.

### Paso 3: Consulta al resolver recursivo

Si no está en caché, la consulta va al servidor DNS configurado en `/etc/resolv.conf`:

```
Cliente (192.168.122.100) → dnsmasq (192.168.122.1) → DNS del host → ...
```

### Paso 4: El resolver recursivo resuelve (si no tiene caché)

```
Resolver recursivo (ej. 8.8.8.8):

1. Pregunta a un Root Server: "¿Quién gestiona .com?"
   Root → "Pregunta al TLD server de .com (192.5.6.30)"

2. Pregunta al TLD .com: "¿Quién gestiona google.com?"
   TLD → "Pregunta a ns1.google.com (216.239.32.10)"

3. Pregunta al authoritative de google.com: "¿IP de google.com?"
   Authoritative → "142.250.184.14, TTL 300 segundos"

4. El resolver cachea la respuesta (300s) y la devuelve al cliente.
```

### Diagrama completo para una VM

```
┌── VM ─────────────────────┐
│ curl google.com            │
│   ↓                        │
│ /etc/hosts → no encontrado │
│   ↓                        │
│ /etc/resolv.conf           │
│ nameserver 192.168.122.1   │
└───────────┬────────────────┘
            │ consulta DNS
            ↓
┌── HOST (dnsmasq) ─────────┐
│ 192.168.122.1              │
│ ¿Caché? → no               │
│ Reenviar a DNS del host    │
└───────────┬────────────────┘
            │
            ↓
┌── DNS del host ───────────┐
│ /etc/resolv.conf del host  │
│ nameserver 192.168.1.1     │
│ (o 8.8.8.8, 1.1.1.1)      │
└───────────┬────────────────┘
            │
            ↓
    Resolver recursivo del ISP
    o público (8.8.8.8)
            │
     root → TLD → authoritative
            │
    Respuesta: 142.250.184.14
    (viaja de vuelta por la cadena)
```

---

## 6. TTL y caché

### Qué es el TTL

TTL (Time To Live) es un campo en cada respuesta DNS que indica **cuánto tiempo se puede cachear** el resultado antes de consultarlo de nuevo:

```bash
dig google.com

# ;; ANSWER SECTION:
# google.com.        300    IN    A    142.250.184.14
#                    ───
#                    TTL: 300 segundos (5 minutos)
```

### TTLs típicos

| TTL | Uso | Por qué |
|-----|-----|---------|
| 60s (1 min) | CDNs, balanceo dinámico | Necesitan cambiar IPs frecuentemente |
| 300s (5 min) | Servicios web comunes | Balance entre rendimiento y flexibilidad |
| 3600s (1 hora) | Servidores estables | IP rara vez cambia |
| 86400s (24 horas) | Registros NS, MX | Infraestructura muy estable |

### Impacto de la caché

La caché DNS es lo que hace que DNS sea rápido en la práctica. Sin ella, cada consulta tendría que recorrer la jerarquía completa (root → TLD → authoritative), lo que añadiría cientos de milisegundos.

```bash
# Primera consulta (sin caché): lenta
dig google.com | grep "Query time"
# ;; Query time: 45 msec

# Segunda consulta (desde caché): rápida
dig google.com | grep "Query time"
# ;; Query time: 0 msec
```

### Problema: caché obsoleta

Si cambias la IP de un servicio, los clientes que tengan la IP vieja cacheada seguirán usando la IP antigua hasta que su TTL expire. Por eso:

- Antes de una migración, reduce el TTL con antelación (ej. de 3600 a 60).
- Espera a que el TTL anterior expire.
- Haz el cambio de IP.
- Los clientes renovarán rápido porque el TTL es corto.

---

## 7. Herramientas: dig, host, nslookup

### dig — la herramienta completa

`dig` (Domain Information Groper) es la herramienta estándar para consultas DNS. Muestra toda la información de la respuesta:

```bash
dig google.com
```

```
; <<>> DiG 9.18.24 <<>> google.com
;; global options: +cmd
;; Got answer:
;; ->>HEADER<<- opcode: QUERY, status: NOERROR, id: 12345
;; flags: qr rd ra; QUERY: 1, ANSWER: 1, AUTHORITY: 0, ADDITIONAL: 1

;; QUESTION SECTION:
;google.com.                    IN      A

;; ANSWER SECTION:
google.com.             300     IN      A       142.250.184.14

;; Query time: 23 msec
;; SERVER: 192.168.122.1#53(192.168.122.1) (UDP)
;; WHEN: Wed Mar 19 10:30:00 CET 2026
;; MSG SIZE  rcvd: 55
```

### Desglose de la salida

| Sección | Significado |
|---------|------------|
| `status: NOERROR` | La consulta fue exitosa |
| `flags: qr rd ra` | qr=respuesta, rd=recursión deseada, ra=recursión disponible |
| `QUESTION SECTION` | Qué preguntaste |
| `ANSWER SECTION` | La respuesta (IP, TTL, tipo de registro) |
| `Query time` | Latencia de la consulta (0ms = caché) |
| `SERVER` | Qué servidor DNS respondió |

### Variantes de dig

```bash
# Solo la respuesta (ideal para scripts)
dig +short google.com
# 142.250.184.14

# Consultar registro IPv6 (AAAA)
dig AAAA google.com +short
# 2a00:1450:4003:80e::200e

# Consultar registros MX (correo)
dig MX google.com +short
# 10 smtp.google.com.

# Consultar registros NS (name servers)
dig NS google.com +short
# ns1.google.com.
# ns2.google.com.

# Consultar un servidor DNS específico
dig @8.8.8.8 google.com +short
dig @1.1.1.1 google.com +short

# Resolución inversa (IP → nombre)
dig -x 8.8.8.8 +short
# dns.google.

# Trazar toda la cadena de resolución (root → TLD → auth)
dig +trace google.com

# Sin caché del resolver local (fuerza consulta completa)
dig +norecurse @a.root-servers.net google.com
```

### host — salida simple

```bash
host google.com
# google.com has address 142.250.184.14
# google.com has IPv6 address 2a00:1450:4003:80e::200e
# google.com mail is handled by 10 smtp.google.com.

# Resolución inversa
host 8.8.8.8
# 8.8.8.8.in-addr.arpa domain name pointer dns.google.
```

### nslookup — interactivo o directo

```bash
# Modo directo
nslookup google.com
# Server:    192.168.122.1
# Address:   192.168.122.1#53
# Non-authoritative answer:
# Name:      google.com
# Address:   142.250.184.14

# Consultar un servidor específico
nslookup google.com 8.8.8.8
```

### ¿Cuál usar?

| Herramienta | Cuándo |
|-------------|--------|
| `dig` | Diagnóstico detallado, scripts, trabajo profesional |
| `host` | Consulta rápida, verificación simple |
| `nslookup` | Familiaridad con Windows, consulta rápida |

En este curso usaremos `dig` como herramienta principal.

---

## 8. /etc/resolv.conf: configuración del cliente DNS

Este archivo le dice a tu sistema qué servidores DNS usar:

```bash
cat /etc/resolv.conf
```

```
nameserver 192.168.122.1
nameserver 8.8.8.8
search lab.local localdomain
options timeout:2 attempts:3
```

### Directivas

| Directiva | Significado | Ejemplo |
|-----------|------------|---------|
| `nameserver` | IP del servidor DNS (máximo 3, se prueban en orden) | `nameserver 8.8.8.8` |
| `search` | Sufijos añadidos a nombres cortos | `search lab.local` → `ping db` busca `db.lab.local` |
| `domain` | Dominio local (alternativa a search, para un solo dominio) | `domain lab.local` |
| `options timeout:N` | Segundos antes de reintentar con otro nameserver | `options timeout:2` |
| `options attempts:N` | Número de intentos por nameserver | `options attempts:3` |

### search en acción

```bash
# /etc/resolv.conf tiene: search lab.local

ping db
# El sistema intenta resolver "db.lab.local" automáticamente
# Si dnsmasq tiene un registro para "db" en la red, funciona

ping db.lab.local
# Equivalente al anterior — nombre completamente cualificado (FQDN)
```

### ¡Cuidado! resolv.conf puede ser gestionado automáticamente

En distros modernas, `/etc/resolv.conf` suele ser generado por otro servicio:

```bash
ls -la /etc/resolv.conf
# lrwxrwxrwx 1 root root 39 /etc/resolv.conf -> ../run/systemd/resolve/stub-resolv.conf
# ↑ symlink → gestionado por systemd-resolved

# O generado por NetworkManager:
head -1 /etc/resolv.conf
# # Generated by NetworkManager
```

Si editas el archivo manualmente, **tus cambios se perderán** al reiniciar la red o el servicio DNS. Para cambios permanentes hay que configurar NetworkManager o systemd-resolved.

---

## 9. /etc/hosts: resolución local sin DNS

`/etc/hosts` es un archivo plano que mapea nombres a IPs **sin consultar ningún servidor DNS**. Se consulta antes que DNS (según nsswitch.conf):

```bash
cat /etc/hosts
```

```
127.0.0.1       localhost localhost.localdomain
::1             localhost localhost.localdomain

# VMs del lab
192.168.122.100  fedora-vm
192.168.122.101  debian-db
192.168.122.102  centos-web
```

### Usos prácticos

**1. Acceso a VMs por nombre desde el host:**

```bash
# Añadir al /etc/hosts del HOST:
echo "192.168.122.100  fedora-vm" | sudo tee -a /etc/hosts

# Ahora puedes hacer:
ssh user@fedora-vm
curl http://fedora-vm:8080
ping fedora-vm
```

**2. Testing de dominios sin comprar ni configurar DNS:**

```bash
# Probar que nginx responde al dominio "miapp.com" antes de configurar DNS real:
echo "192.168.122.100  miapp.com" | sudo tee -a /etc/hosts

curl http://miapp.com
# nginx de la VM responde como si fuera miapp.com
```

**3. Bloqueo de dominios (ad-blocking rudimentario):**

```bash
# Redirigir dominios de publicidad a la nada:
echo "0.0.0.0  ads.example.com" | sudo tee -a /etc/hosts
echo "0.0.0.0  tracker.analytics.com" | sudo tee -a /etc/hosts
```

### Limitaciones de /etc/hosts

- **No escala**: con 50 VMs, gestionar 50 líneas en cada host es tedioso.
- **No se propaga**: cada máquina necesita su propia copia. Si la IP de una VM cambia, debes actualizar /etc/hosts en todas partes.
- **Sin TTL**: no hay caché ni expiración, el mapeo es permanente hasta que lo edites.

Para labs grandes, dnsmasq (incluido en libvirt) resuelve hostnames automáticamente — es mejor solución que /etc/hosts manual.

---

## 10. /etc/nsswitch.conf: orden de resolución

`nsswitch.conf` (Name Service Switch) define el **orden en que el sistema busca información**, incluyendo la resolución de nombres:

```bash
grep ^hosts /etc/nsswitch.conf
# hosts: files dns mymachines
```

| Fuente | Significado |
|--------|------------|
| `files` | `/etc/hosts` |
| `dns` | Servidores DNS de `/etc/resolv.conf` |
| `mymachines` | Nombres de contenedores systemd-machined (systemd-nspawn) |
| `myhostname` | El propio hostname de la máquina |
| `resolve` | systemd-resolved (si está instalado) |
| `mdns` | Multicast DNS (Avahi, nombres `.local`) |

### Orden importa

Con `hosts: files dns`:

```
1. Busca en /etc/hosts → si encuentra, PARA aquí
2. Si no, consulta DNS → respuesta o NXDOMAIN
```

Esto significa que `/etc/hosts` **siempre tiene prioridad sobre DNS**. Si pones `127.0.0.1 google.com` en /etc/hosts, nunca llegarás a Google (hasta que lo quites).

### Con systemd-resolved

En distros que usan systemd-resolved, el orden puede ser:

```
hosts: mymachines resolve [!UNAVAIL=return] files myhostname dns
```

Aquí `resolve` (systemd-resolved) tiene prioridad sobre `files` y `dns`.

---

## 11. systemd-resolved: el DNS moderno de Linux

Muchas distros modernas (Fedora, Ubuntu) usan `systemd-resolved` como resolver DNS local. Actúa como intermediario entre las aplicaciones y los servidores DNS reales.

### Cómo funciona

```
Aplicación → stub resolver (127.0.0.53:53) → systemd-resolved → DNS real
                                                    │
                                              caché local
                                              DNS per-interface
                                              DNSSEC validation
```

### Verificar si está activo

```bash
systemctl status systemd-resolved

# Ver la configuración DNS actual
resolvectl status
# Global
#   Protocols: +LLMNR +mDNS -DNSOverTLS DNSSEC=no/unsupported
#   resolv.conf mode: stub
#
# Link 2 (enp0s3)
#   Current DNS Server: 192.168.1.1
#   DNS Servers: 192.168.1.1
#   DNS Domain: ~.
```

### El stub resolver

systemd-resolved crea un "stub resolver" en `127.0.0.53`:

```bash
cat /etc/resolv.conf
# nameserver 127.0.0.53
# options edns0 trust-ad
# search localdomain

# Las aplicaciones consultan 127.0.0.53 → systemd-resolved cachea y reenvía
```

### Comandos útiles

```bash
# Ver estadísticas de caché
resolvectl statistics
# Cache
#   Current Cache Size: 42
#   Cache Hits: 156
#   Cache Misses: 38

# Limpiar la caché DNS
resolvectl flush-caches

# Resolver un nombre manualmente
resolvectl query google.com
# google.com: 142.250.184.14
#             -- link: enp0s3

# Ver qué DNS usa cada interfaz
resolvectl dns
# Global:
# Link 2 (enp0s3): 192.168.1.1
# Link 5 (virbr0): --
```

### ¿Cuándo importa systemd-resolved?

Si estás configurando DNS personalizado (ej. para un lab), debes saber si tu sistema usa systemd-resolved porque:

- Editar `/etc/resolv.conf` directamente no sirve (es un symlink).
- Los cambios DNS se hacen via `resolvectl` o NetworkManager.
- La caché puede mostrar respuestas obsoletas si no la limpias.

---

## 12. DNS en libvirt: dnsmasq como forwarder

Cuando libvirt crea una red, lanza un proceso dnsmasq que sirve tanto como servidor DHCP como **DNS forwarder** para las VMs.

### Cómo funciona el DNS de libvirt

```
┌── VM (192.168.122.100) ──────────────────┐
│ /etc/resolv.conf:                         │
│   nameserver 192.168.122.1                │
│                                           │
│ dig google.com                            │
└──────────────┬────────────────────────────┘
               │ consulta a 192.168.122.1:53
               ↓
┌── HOST: dnsmasq en virbr0 ───────────────┐
│ Escucha en 192.168.122.1:53               │
│                                           │
│ 1. ¿Es un hostname de una VM?            │
│    (ej. "fedora-vm" → 192.168.122.100)   │
│    → SÍ: responde directamente            │
│                                           │
│ 2. ¿Está en la caché de dnsmasq?         │
│    → SÍ: responde desde caché             │
│                                           │
│ 3. Reenvía al DNS del host               │
│    (lee /etc/resolv.conf del host)        │
└──────────────┬────────────────────────────┘
               │ consulta al DNS real
               ↓
        DNS del ISP / 8.8.8.8 / 1.1.1.1
```

### Resolución automática de hostnames de VMs

Cuando dnsmasq asigna una IP por DHCP, registra el hostname de la VM. Otras VMs en la misma red pueden resolver ese nombre:

```bash
# VM "fedora-vm" tiene 192.168.122.100 (asignada por DHCP con hostname)
# VM "debian-db" tiene 192.168.122.101

# Desde debian-db:
ping fedora-vm
# PING fedora-vm (192.168.122.100)

# Esto funciona porque dnsmasq registró:
# fedora-vm → 192.168.122.100 cuando asignó el lease
```

### Resolución inversa automática

dnsmasq también crea registros PTR automáticamente:

```bash
# Desde cualquier VM en la red:
dig -x 192.168.122.100 @192.168.122.1 +short
# fedora-vm
```

### DNS personalizado para un lab

Puedes añadir registros DNS personalizados al dnsmasq de libvirt:

```bash
# Método 1: archivo addnhosts
echo "192.168.122.100 web.lab.local" | sudo tee -a \
    /var/lib/libvirt/dnsmasq/default.addnhosts

# Recargar dnsmasq para que lea el archivo
sudo kill -HUP $(pidof dnsmasq)  # señal HUP = recargar config

# Método 2: en el XML de la red (virsh net-edit)
# Dentro de <network>, añadir:
#   <dns>
#     <host ip='192.168.122.100'>
#       <hostname>web.lab.local</hostname>
#     </host>
#   </dns>
```

---

## 13. Resolución de nombres entre VMs

### Escenario: lab multi-VM

```
┌── VM1: web ─────────┐  ┌── VM2: api ─────────┐  ┌── VM3: db ──────────┐
│ 192.168.122.100      │  │ 192.168.122.101      │  │ 192.168.122.102      │
│ hostname: web-server │  │ hostname: api-server │  │ hostname: db-server  │
└──────────────────────┘  └──────────────────────┘  └──────────────────────┘
            │                       │                        │
            └───────── virbr0 (dnsmasq: 192.168.122.1) ─────┘
```

### Con dnsmasq automático (si los hostnames se configuran en las VMs)

```bash
# Desde web-server:
ping api-server     # resuelve a 192.168.122.101
ping db-server      # resuelve a 192.168.122.102

# Desde api-server:
curl http://db-server:5432   # conexión a PostgreSQL por nombre
```

Para que esto funcione, cada VM debe enviar su hostname en la solicitud DHCP. La mayoría de distribuciones lo hacen automáticamente.

### Verificar que el hostname se envía en DHCP

```bash
# Dentro de la VM:
hostnamectl
# Static hostname: web-server

# Verificar que DHCP envía el hostname:
# En Fedora/RHEL con NetworkManager, se envía automáticamente.
# En Debian mínimo, puede que no. Verificar:
nmcli connection show "Wired connection 1" | grep dhcp-hostname
```

### Cuando dnsmasq no resuelve hostnames

Si `ping web-server` falla desde otra VM:

```bash
# Verificar que dnsmasq tiene el registro
dig @192.168.122.1 web-server +short
# Si no devuelve nada: el hostname no se registró

# Verificar leases (el hostname aparece aquí si se envió)
virsh net-dhcp-leases default
# ¿La columna "Hostname" muestra el nombre o está vacía?

# Solución 1: asegurar que la VM envía hostname en DHCP
sudo hostnamectl set-hostname web-server
# Y renovar DHCP: sudo dhclient -r eth0 && sudo dhclient eth0

# Solución 2: añadir el nombre manualmente via DNS en el XML de la red
virsh net-edit default
# Añadir dentro de <network>:
# <dns>
#   <host ip='192.168.122.100'><hostname>web-server</hostname></host>
# </dns>
```

### /etc/hosts como fallback

Si dnsmasq no resuelve nombres automáticamente, puedes usar /etc/hosts dentro de cada VM:

```bash
# En CADA VM, añadir:
cat >> /etc/hosts << 'EOF'
192.168.122.100  web-server
192.168.122.101  api-server
192.168.122.102  db-server
EOF
```

Funciona, pero no escala y no se actualiza automáticamente.

---

## 14. Errores comunes

### Error 1: "Temporary failure in name resolution"

```bash
ping google.com
# ping: google.com: Temporary failure in name resolution

# Pero ping por IP funciona:
ping 8.8.8.8
# 64 bytes from 8.8.8.8 ...
```

El problema es DNS, no la red. Diagnóstico:

```bash
# ¿Qué DNS está configurado?
cat /etc/resolv.conf
# Si está vacío o apunta a algo inaccesible → no hay DNS

# ¿El DNS responde?
dig @192.168.122.1 google.com
# Si timeout → dnsmasq no está corriendo

# ¿dnsmasq está activo?
ps aux | grep dnsmasq

# Solución temporal:
echo "nameserver 8.8.8.8" | sudo tee /etc/resolv.conf

# Solución permanente: arreglar la red virtual o NetworkManager
```

### Error 2: Editar /etc/resolv.conf y que se sobrescriba

```bash
# Editas /etc/resolv.conf manualmente:
sudo vim /etc/resolv.conf    # añades nameserver 8.8.8.8

# Reinicia NetworkManager o DHCP renueva:
# → Tu cambio se pierde

# Solución con NetworkManager:
nmcli connection modify "Wired connection 1" ipv4.dns "8.8.8.8 1.1.1.1"
nmcli connection up "Wired connection 1"

# Solución con systemd-resolved:
# Editar /etc/systemd/resolved.conf:
# [Resolve]
# DNS=8.8.8.8 1.1.1.1
sudo systemctl restart systemd-resolved
```

### Error 3: Caché DNS obsoleta

```bash
# Cambiaste la IP de una VM pero el nombre sigue apuntando a la IP vieja

# Con systemd-resolved:
resolvectl flush-caches

# Con dnsmasq de libvirt (reiniciar la red):
virsh net-destroy default && virsh net-start default

# O simplemente esperar a que el TTL expire
```

### Error 4: VMs no se resuelven por nombre entre sí

```bash
# Desde VM1: ping vm2 → falla
# Pero ping 192.168.122.101 → funciona

# Causas posibles:
# 1. VM2 no envió hostname en DHCP → verificar virsh net-dhcp-leases
# 2. La VM usa resolv.conf incorrecto → debe apuntar a 192.168.122.1
# 3. nsswitch.conf no consulta DNS → verificar "hosts: files dns"
```

### Error 5: Confundir 127.0.0.53 con 127.0.0.1

```bash
# /etc/resolv.conf dice: nameserver 127.0.0.53
# Esto NO es loopback — es el stub resolver de systemd-resolved

# Si systemd-resolved no está corriendo:
dig @127.0.0.53 google.com
# ;; connection timed out  ← no hay nadie escuchando

# 127.0.0.1 es loopback real
# 127.0.0.53 es el stub resolver de systemd-resolved
# Son direcciones diferentes (ambas en 127.0.0.0/8, pero distintos servicios)
```

---

## 15. Cheatsheet

```text
╔══════════════════════════════════════════════════════════════════════╗
║                      DNS BÁSICO CHEATSHEET                          ║
╠══════════════════════════════════════════════════════════════════════╣
║                                                                    ║
║  JERARQUÍA DNS                                                     ║
║  Root (.) → TLD (.com) → Dominio (google.com) → Subdominio (www.) ║
║  13 root servers → TLD servers → Authoritative → Recursive/Cache   ║
║                                                                    ║
╠══════════════════════════════════════════════════════════════════════╣
║                                                                    ║
║  REGISTROS COMUNES                                                 ║
║  A       Nombre → IPv4                                              ║
║  AAAA    Nombre → IPv6                                              ║
║  CNAME   Alias → otro nombre                                       ║
║  MX      Servidor de correo                                        ║
║  NS      Servidor de nombres autoritativo                           ║
║  PTR     IP → Nombre (resolución inversa)                           ║
║  TXT     Texto libre (SPF, verificación)                            ║
║                                                                    ║
╠══════════════════════════════════════════════════════════════════════╣
║                                                                    ║
║  HERRAMIENTAS                                                      ║
║  dig google.com              Consulta completa                     ║
║  dig +short google.com       Solo la IP                            ║
║  dig @8.8.8.8 google.com    Usar servidor específico               ║
║  dig -x 8.8.8.8             Resolución inversa                    ║
║  dig +trace google.com      Trazar la cadena completa              ║
║  dig AAAA google.com        Consultar IPv6                         ║
║  dig MX google.com          Consultar correo                       ║
║  host google.com             Consulta simple                       ║
║  nslookup google.com         Consulta simple (alt)                 ║
║                                                                    ║
╠══════════════════════════════════════════════════════════════════════╣
║                                                                    ║
║  ARCHIVOS DE CONFIGURACIÓN                                         ║
║  /etc/resolv.conf        Servidores DNS (nameserver, search)       ║
║  /etc/hosts              Mapeo local nombre → IP (prioridad)       ║
║  /etc/nsswitch.conf      Orden de resolución (files dns ...)       ║
║                                                                    ║
╠══════════════════════════════════════════════════════════════════════╣
║                                                                    ║
║  SYSTEMD-RESOLVED                                                  ║
║  resolvectl status       Estado y DNS por interfaz                  ║
║  resolvectl flush-caches Limpiar caché DNS                         ║
║  resolvectl query X      Resolver un nombre                        ║
║  resolvectl statistics   Hits/misses de caché                      ║
║  Stub resolver: 127.0.0.53 (no confundir con 127.0.0.1)           ║
║                                                                    ║
╠══════════════════════════════════════════════════════════════════════╣
║                                                                    ║
║  DNS EN LIBVIRT                                                    ║
║  dnsmasq actúa como DNS forwarder + resuelve hostnames de VMs      ║
║  VMs con hostname en DHCP → otras VMs las resuelven por nombre     ║
║  Registros PTR automáticos (resolución inversa)                    ║
║  DNS personalizado: addnhosts o <dns><host> en XML de red          ║
║  Recargar: kill -HUP $(pidof dnsmasq)                              ║
║                                                                    ║
╠══════════════════════════════════════════════════════════════════════╣
║                                                                    ║
║  TROUBLESHOOTING                                                   ║
║  "Temporary failure" → ping IP funciona pero DNS no → resolver     ║
║  resolv.conf se sobrescribe → configurar via nmcli/resolved        ║
║  Caché obsoleta → resolvectl flush-caches / reiniciar red          ║
║  VMs no se resuelven → verificar hostname en DHCP lease            ║
║                                                                    ║
╚══════════════════════════════════════════════════════════════════════╝
```

---

## 16. Ejercicios

### Ejercicio 1: Exploración DNS con dig

Ejecuta los siguientes comandos y responde:

```bash
# 1. Consulta básica
dig google.com
dig +short google.com

# 2. Diferentes tipos de registro
dig MX google.com +short
dig NS google.com +short
dig AAAA google.com +short

# 3. Resolución inversa
dig -x 8.8.8.8 +short

# 4. Tiempos de consulta
dig google.com | grep "Query time"
dig google.com | grep "Query time"   # segunda vez (caché)

# 5. Ver qué servidor respondió
dig google.com | grep "SERVER"
```

**Preguntas:**
1. ¿Cuántas IPs (registros A) devolvió google.com? ¿Por qué podría tener varias?
2. ¿Cuál es el TTL de la respuesta? Si esperas ese tiempo y consultas de nuevo, ¿esperarías el mismo TTL o uno diferente?
3. ¿El Query time bajó en la segunda consulta? ¿Qué indica eso?
4. ¿Qué servidor DNS respondió (`SERVER`)? ¿Es el dnsmasq de libvirt, systemd-resolved, o tu router?
5. ¿Qué nombre devolvió la resolución inversa de 8.8.8.8? ¿Tiene sentido?

### Ejercicio 2: Configuración DNS de tu sistema

```bash
# 1. Ver la configuración actual
cat /etc/resolv.conf
ls -la /etc/resolv.conf    # ¿es symlink?

# 2. Orden de resolución
grep ^hosts /etc/nsswitch.conf

# 3. Si tienes systemd-resolved:
resolvectl status 2>/dev/null
resolvectl statistics 2>/dev/null

# 4. Override con /etc/hosts
# (NO ejecutes esto, solo responde teóricamente)
# Si añadieras: 127.0.0.1 google.com
# ¿Qué pasaría al hacer "curl google.com"?
# ¿Y "dig google.com"?
```

**Preguntas:**
1. ¿Tu `/etc/resolv.conf` es un archivo real o un symlink? Si es symlink, ¿a dónde apunta?
2. ¿En el orden de `nsswitch.conf`, qué se consulta primero: `/etc/hosts` o DNS?
3. Si tienes systemd-resolved, ¿cuántos cache hits muestra? ¿Qué significa un ratio alto de hits?
4. Pregunta trampa: si añadieras `127.0.0.1 google.com` a `/etc/hosts`, `curl google.com` fallaría (llegaría a localhost), pero ¿`dig google.com` devolvería 127.0.0.1 o la IP real? ¿Por qué? (pista: `dig` no consulta /etc/hosts).

### Ejercicio 3: DNS en un lab de VMs

Tienes tres VMs en la red default de libvirt y quieres que se resuelvan entre sí por nombre:

| VM | IP (reserva DHCP) | Hostname | Servicio |
|----|-------------------|----------|----------|
| web | 192.168.122.10 | web.lab | nginx |
| api | 192.168.122.11 | api.lab | backend |
| db | 192.168.122.12 | db.lab | postgres |

1. Escribe el fragmento XML de `<dns>` para la red de libvirt que registre los tres nombres.
2. La VM "api" necesita conectarse a PostgreSQL en `db.lab:5432`. ¿Qué entraría en la connection string? ¿Qué pasaría si la IP de "db" cambia pero el nombre se mantiene?
3. Desde el host, ¿cómo harías para que `ssh user@web.lab` funcione? ¿Qué archivo editarías y qué línea añadirías?
4. Si las tres VMs también necesitan resolver dominios de internet (google.com, github.com), ¿necesitas configurar algo adicional o dnsmasq ya lo reenvía automáticamente?

**Pregunta reflexiva:** Compara tres formas de resolver nombres de VMs: (a) /etc/hosts en cada VM, (b) dnsmasq automático por hostname DHCP, (c) registros DNS en el XML de libvirt. ¿Cuál escalarías mejor para un lab de 20 VMs que cambian frecuentemente? ¿Cuál sobreviviría a un `virsh net-destroy && virsh net-start`?

---

> **Nota**: DNS es un sistema elegante que hace invisible la complejidad de traducir nombres a IPs. En virtualización con libvirt, dnsmasq te da DNS integrado gratis: resuelve hostnames de VMs, hace forwarding al DNS real, y genera registros PTR automáticamente. Para labs pequeños esto es suficiente. Para labs más complejos con dominios personalizados, puedes usar la sección `<dns>` del XML de la red. Antes de editar `/etc/resolv.conf` manualmente, verifica si tu sistema usa systemd-resolved o NetworkManager — de lo contrario tus cambios se perderán al reiniciar.
