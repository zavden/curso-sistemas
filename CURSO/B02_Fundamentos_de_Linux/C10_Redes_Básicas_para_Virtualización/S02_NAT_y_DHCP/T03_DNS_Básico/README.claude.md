# T03 — DNS Basico

## Erratas detectadas en los archivos fuente

| Archivo | Linea | Error | Correccion |
|---------|-------|-------|------------|
| `README.max.md` | 5 | `Los humanos记忆 mejor nombres que numeros.` — contiene caracteres chinos "记忆" (recordar/memorizar) en medio del texto en espanol. | `Los humanos recuerdan mejor nombres que numeros.` |
| `README.md` | 743 | `curl http://db-server:5432   # conexion a PostgreSQL por nombre` — `curl` es un cliente HTTP y PostgreSQL usa su propio protocolo de cable, no HTTP. Un estudiante que ejecute esto obtendra un error de protocolo, no una conexion a la base de datos. | Usar `psql -h db-server -p 5432` o similar para ilustrar la conexion por nombre. |

---

## 1. El problema: los humanos no recuerdan IPs

Las maquinas se comunican con direcciones IP (142.250.184.14), pero los humanos recordamos nombres (google.com). DNS es el sistema que traduce entre ambos mundos.

Sin DNS, cada vez que quisieras visitar un sitio web tendrias que recordar su IP. Y peor: las IPs de los servicios cambian (balanceo de carga, CDNs, migraciones), asi que la IP que funcionaba ayer podria no funcionar hoy.

En virtualizacion, DNS es igualmente importante:

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

## 2. Que es DNS

DNS (Domain Name System) es una **base de datos distribuida y jerarquica** que mapea nombres de dominio a direcciones IP (y otros datos). Opera en capa 7 (Aplicacion), usando principalmente UDP puerto 53 (TCP 53 para transferencias de zona y respuestas grandes).

### Analogia

DNS funciona como un sistema de guias telefonicas distribuido:

```
"Cual es el numero de Juan Garcia en Madrid?"

No hay una guia unica con TODOS los numeros del mundo.
Pero hay un sistema de guias:
  1. Guia de Espana -> te dice que Madrid esta en la guia regional
  2. Guia de Madrid -> te dice el numero de Juan Garcia

DNS funciona igual:
  1. Root servers -> saben donde estan los .com, .org, .es
  2. TLD servers (.com) -> saben donde esta google.com
  3. Authoritative server -> tiene la IP de google.com
```

### Por que distribuido

Una base de datos centralizada con todos los dominios del mundo seria:
- **Un punto unico de fallo**: si cae, nadie puede navegar.
- **Un cuello de botella**: miles de millones de consultas por segundo.
- **Imposible de mantener**: se registran miles de dominios nuevos cada dia.

La distribucion jerarquica resuelve estos tres problemas: cada nivel solo conoce la siguiente capa, y las respuestas se cachean agresivamente.

---

## 3. Jerarquia DNS: del root al registro

```
                    . (root)
                    |
         +----------+----------+
        .com       .org       .es        <- TLD (Top Level Domain)
         |          |          |
     google.com  wikipedia.org  ...      <- Dominio de segundo nivel
         |
  +------+-------+
mail.   www.   maps.                     <- Subdominios
google  google  google
.com    .com    .com
```

### Los 4 niveles de servidores DNS

**1. Root servers (13 clusters)**

Los root servers no conocen la IP de google.com, pero saben quien gestiona `.com`. Son la entrada al sistema DNS. Hay 13 direcciones IP de root servers (letras a-m), pero cada una es un cluster de cientos de servidores distribuidos globalmente mediante anycast.

```bash
# Ver los root servers
dig . NS +short
# a.root-servers.net.
# b.root-servers.net.
# ... (13 total)
```

**2. TLD servers**

Gestionan los dominios de primer nivel: `.com`, `.org`, `.net`, `.es`, `.io`, etc. Verisign gestiona `.com` y `.net`. Cada pais tiene su propio gestor para su ccTLD (`.es`, `.uk`, `.de`).

**3. Authoritative servers**

El servidor que tiene la respuesta **definitiva** para un dominio. Si preguntas por `google.com`, el authoritative server de Google responde con la IP.

**4. Recursive resolvers (resolvers recursivos)**

El servidor que tu maquina consulta. No tiene respuestas propias — pregunta a los otros servidores por ti y cachea las respuestas. Ejemplos:

| Resolver | IP | Operador |
|----------|------|---------|
| Google Public DNS | 8.8.8.8, 8.8.4.4 | Google |
| Cloudflare | 1.1.1.1, 1.0.0.1 | Cloudflare |
| Quad9 | 9.9.9.9 | IBM/PCH |
| Tu router | 192.168.1.1 | Tu ISP (reenvia) |
| dnsmasq de libvirt | 192.168.122.1 | Reenvia al DNS del host |

---

## 4. Tipos de registros DNS

Un dominio puede tener multiples registros de diferentes tipos:

| Tipo | Nombre | Contenido | Ejemplo |
|------|--------|-----------|---------|
| A | Address | IPv4 del dominio | `google.com -> 142.250.184.14` |
| AAAA | IPv6 Address | IPv6 del dominio | `google.com -> 2a00:1450:4003:80e::200e` |
| CNAME | Canonical Name | Alias a otro dominio | `www.google.com -> google.com` |
| MX | Mail Exchange | Servidor de correo | `google.com -> smtp.google.com (prioridad 10)` |
| NS | Name Server | Servidor autoritativo | `google.com -> ns1.google.com` |
| PTR | Pointer | Resolucion inversa (IP->nombre) | `8.8.8.8 -> dns.google` |
| TXT | Text | Texto libre (SPF, DKIM, verificacion) | `google.com -> "v=spf1 ..."` |
| SOA | Start of Authority | Informacion de la zona (serial, TTL) | Metadatos del dominio |
| SRV | Service | Servicio y puerto | `_ssh._tcp.lab.local -> 192.168.122.100:22` |

### Registros relevantes para virtualizacion

| Registro | Uso en VMs |
|----------|-----------|
| A | Resolver el nombre de una VM a su IP (`fedora-vm -> 192.168.122.100`) |
| PTR | Resolucion inversa (`192.168.122.100 -> fedora-vm`) — dnsmasq lo genera automaticamente |
| SRV | Localizar servicios en un lab (ej. donde esta el servidor SSH, el DB) |

---

## 5. El proceso de resolucion paso a paso

Cuando escribes `curl google.com`, ocurre esto:

### Paso 1: Ya lo se localmente?

```
Aplicacion (curl) -> biblioteca C (getaddrinfo) -> nsswitch.conf

nsswitch.conf dice: hosts: files dns mymachines
                           -----
                           primero /etc/hosts
```

El sistema revisa `/etc/hosts`. Si encuentra `google.com`, usa esa IP y no consulta DNS.

### Paso 2: Esta en la cache local?

Si hay un resolver local (systemd-resolved, dnsmasq, nscd), busca en su cache. Si tiene una respuesta reciente y su TTL no expiro, la devuelve sin consultar ningun servidor.

### Paso 3: Consulta al resolver recursivo

Si no esta en cache, la consulta va al servidor DNS configurado en `/etc/resolv.conf`:

```
Cliente (192.168.122.100) -> dnsmasq (192.168.122.1) -> DNS del host -> ...
```

### Paso 4: El resolver recursivo resuelve (si no tiene cache)

```
Resolver recursivo (ej. 8.8.8.8):

1. Pregunta a un Root Server: "Quien gestiona .com?"
   Root -> "Pregunta al TLD server de .com (192.5.6.30)"

2. Pregunta al TLD .com: "Quien gestiona google.com?"
   TLD -> "Pregunta a ns1.google.com (216.239.32.10)"

3. Pregunta al authoritative de google.com: "IP de google.com?"
   Authoritative -> "142.250.184.14, TTL 300 segundos"

4. El resolver cachea la respuesta (300s) y la devuelve al cliente.
```

### Diagrama completo para una VM

```
+-- VM ----------------------------+
| curl google.com                   |
|   |                               |
| /etc/hosts -> no encontrado       |
|   |                               |
| /etc/resolv.conf                  |
| nameserver 192.168.122.1          |
+------------+---------------------+
             | consulta DNS
             v
+-- HOST (dnsmasq) ----------------+
| 192.168.122.1                     |
| Cache? -> no                      |
| Reenviar a DNS del host           |
+------------+---------------------+
             |
             v
+-- DNS del host ------------------+
| /etc/resolv.conf del host         |
| nameserver 192.168.1.1            |
| (o 8.8.8.8, 1.1.1.1)             |
+------------+---------------------+
             |
             v
    Resolver recursivo del ISP
    o publico (8.8.8.8)
             |
     root -> TLD -> authoritative
             |
    Respuesta: 142.250.184.14
    (viaja de vuelta por la cadena)
```

---

## 6. TTL y cache

### Que es el TTL

TTL (Time To Live) es un campo en cada respuesta DNS que indica **cuanto tiempo se puede cachear** el resultado antes de consultarlo de nuevo:

```bash
dig google.com

# ;; ANSWER SECTION:
# google.com.        300    IN    A    142.250.184.14
#                    ---
#                    TTL: 300 segundos (5 minutos)
```

### TTLs tipicos

| TTL | Uso | Por que |
|-----|-----|---------|
| 60s (1 min) | CDNs, balanceo dinamico | Necesitan cambiar IPs frecuentemente |
| 300s (5 min) | Servicios web comunes | Balance entre rendimiento y flexibilidad |
| 3600s (1 hora) | Servidores estables | IP rara vez cambia |
| 86400s (24 horas) | Registros NS, MX | Infraestructura muy estable |

### Impacto de la cache

La cache DNS es lo que hace que DNS sea rapido en la practica. Sin ella, cada consulta tendria que recorrer la jerarquia completa (root -> TLD -> authoritative), lo que anadiria cientos de milisegundos.

```bash
# Primera consulta (sin cache): lenta
dig google.com | grep "Query time"
# ;; Query time: 45 msec

# Segunda consulta (desde cache): rapida
dig google.com | grep "Query time"
# ;; Query time: 0 msec
```

### Problema: cache obsoleta

Si cambias la IP de un servicio, los clientes que tengan la IP vieja cacheada seguiran usando la IP antigua hasta que su TTL expire. Por eso:

- Antes de una migracion, reduce el TTL con antelacion (ej. de 3600 a 60).
- Espera a que el TTL anterior expire.
- Haz el cambio de IP.
- Los clientes renovaran rapido porque el TTL es corto.

---

## 7. Herramientas: dig, host, nslookup

### dig — la herramienta completa

`dig` (Domain Information Groper) es la herramienta estandar para consultas DNS. Muestra toda la informacion de la respuesta:

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

| Seccion | Significado |
|---------|------------|
| `status: NOERROR` | La consulta fue exitosa |
| `flags: qr rd ra` | qr=respuesta, rd=recursion deseada, ra=recursion disponible |
| `QUESTION SECTION` | Que preguntaste |
| `ANSWER SECTION` | La respuesta (IP, TTL, tipo de registro) |
| `Query time` | Latencia de la consulta (0ms = cache) |
| `SERVER` | Que servidor DNS respondio |

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

# Consultar un servidor DNS especifico
dig @8.8.8.8 google.com +short
dig @1.1.1.1 google.com +short

# Resolucion inversa (IP -> nombre)
dig -x 8.8.8.8 +short
# dns.google.

# Trazar toda la cadena de resolucion (root -> TLD -> auth)
dig +trace google.com

# Sin cache del resolver local (fuerza consulta completa)
dig +norecurse @a.root-servers.net google.com
```

### host — salida simple

```bash
host google.com
# google.com has address 142.250.184.14
# google.com has IPv6 address 2a00:1450:4003:80e::200e
# google.com mail is handled by 10 smtp.google.com.

# Resolucion inversa
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

# Consultar un servidor especifico
nslookup google.com 8.8.8.8
```

### Cual usar

| Herramienta | Cuando |
|-------------|--------|
| `dig` | Diagnostico detallado, scripts, trabajo profesional |
| `host` | Consulta rapida, verificacion simple |
| `nslookup` | Familiaridad con Windows, consulta rapida |

En este curso usaremos `dig` como herramienta principal.

---

## 8. /etc/resolv.conf: configuracion del cliente DNS

Este archivo le dice a tu sistema que servidores DNS usar:

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
| `nameserver` | IP del servidor DNS (maximo 3, se prueban en orden) | `nameserver 8.8.8.8` |
| `search` | Sufijos anadidos a nombres cortos | `search lab.local` -> `ping db` busca `db.lab.local` |
| `domain` | Dominio local (alternativa a search, para un solo dominio) | `domain lab.local` |
| `options timeout:N` | Segundos antes de reintentar con otro nameserver | `options timeout:2` |
| `options attempts:N` | Numero de intentos por nameserver | `options attempts:3` |

### search en accion

```bash
# /etc/resolv.conf tiene: search lab.local

ping db
# El sistema intenta resolver "db.lab.local" automaticamente
# Si dnsmasq tiene un registro para "db" en la red, funciona

ping db.lab.local
# Equivalente al anterior — nombre completamente cualificado (FQDN)
```

### Cuidado: resolv.conf puede ser gestionado automaticamente

En distros modernas, `/etc/resolv.conf` suele ser generado por otro servicio:

```bash
ls -la /etc/resolv.conf
# lrwxrwxrwx 1 root root 39 /etc/resolv.conf -> ../run/systemd/resolve/stub-resolv.conf
# ^ symlink -> gestionado por systemd-resolved

# O generado por NetworkManager:
head -1 /etc/resolv.conf
# # Generated by NetworkManager
```

Si editas el archivo manualmente, **tus cambios se perderan** al reiniciar la red o el servicio DNS. Para cambios permanentes hay que configurar NetworkManager o systemd-resolved.

---

## 9. /etc/hosts: resolucion local sin DNS

`/etc/hosts` es un archivo plano que mapea nombres a IPs **sin consultar ningun servidor DNS**. Se consulta antes que DNS (segun nsswitch.conf):

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

### Usos practicos

**1. Acceso a VMs por nombre desde el host:**

```bash
# Anadir al /etc/hosts del HOST:
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

- **No escala**: con 50 VMs, gestionar 50 lineas en cada host es tedioso.
- **No se propaga**: cada maquina necesita su propia copia. Si la IP de una VM cambia, debes actualizar /etc/hosts en todas partes.
- **Sin TTL**: no hay cache ni expiracion, el mapeo es permanente hasta que lo edites.

Para labs grandes, dnsmasq (incluido en libvirt) resuelve hostnames automaticamente — es mejor solucion que /etc/hosts manual.

---

## 10. /etc/nsswitch.conf: orden de resolucion

`nsswitch.conf` (Name Service Switch) define el **orden en que el sistema busca informacion**, incluyendo la resolucion de nombres:

```bash
grep ^hosts /etc/nsswitch.conf
# hosts: files dns mymachines
```

| Fuente | Significado |
|--------|------------|
| `files` | `/etc/hosts` |
| `dns` | Servidores DNS de `/etc/resolv.conf` |
| `mymachines` | Nombres de contenedores systemd-machined (systemd-nspawn) |
| `myhostname` | El propio hostname de la maquina |
| `resolve` | systemd-resolved (si esta instalado) |
| `mdns` | Multicast DNS (Avahi, nombres `.local`) |

### Orden importa

Con `hosts: files dns`:

```
1. Busca en /etc/hosts -> si encuentra, PARA aqui
2. Si no, consulta DNS -> respuesta o NXDOMAIN
```

Esto significa que `/etc/hosts` **siempre tiene prioridad sobre DNS**. Si pones `127.0.0.1 google.com` en /etc/hosts, nunca llegaras a Google (hasta que lo quites).

### Con systemd-resolved

En distros que usan systemd-resolved, el orden puede ser:

```
hosts: mymachines resolve [!UNAVAIL=return] files myhostname dns
```

Aqui `resolve` (systemd-resolved) tiene prioridad sobre `files` y `dns`.

---

## 11. systemd-resolved: el DNS moderno de Linux

Muchas distros modernas (Fedora, Ubuntu) usan `systemd-resolved` como resolver DNS local. Actua como intermediario entre las aplicaciones y los servidores DNS reales.

### Como funciona

```
Aplicacion -> stub resolver (127.0.0.53:53) -> systemd-resolved -> DNS real
                                                    |
                                              cache local
                                              DNS per-interface
                                              DNSSEC validation
```

### Verificar si esta activo

```bash
systemctl status systemd-resolved

# Ver la configuracion DNS actual
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

# Las aplicaciones consultan 127.0.0.53 -> systemd-resolved cachea y reenvia
```

### Comandos utiles

```bash
# Ver estadisticas de cache
resolvectl statistics
# Cache
#   Current Cache Size: 42
#   Cache Hits: 156
#   Cache Misses: 38

# Limpiar la cache DNS
resolvectl flush-caches

# Resolver un nombre manualmente
resolvectl query google.com
# google.com: 142.250.184.14
#             -- link: enp0s3

# Ver que DNS usa cada interfaz
resolvectl dns
# Global:
# Link 2 (enp0s3): 192.168.1.1
# Link 5 (virbr0): --
```

### Cuando importa systemd-resolved

Si estas configurando DNS personalizado (ej. para un lab), debes saber si tu sistema usa systemd-resolved porque:

- Editar `/etc/resolv.conf` directamente no sirve (es un symlink).
- Los cambios DNS se hacen via `resolvectl` o NetworkManager.
- La cache puede mostrar respuestas obsoletas si no la limpias.

---

## 12. DNS en libvirt: dnsmasq como forwarder

Cuando libvirt crea una red, lanza un proceso dnsmasq que sirve tanto como servidor DHCP como **DNS forwarder** para las VMs.

### Como funciona el DNS de libvirt

```
+-- VM (192.168.122.100) --------------------------+
| /etc/resolv.conf:                                 |
|   nameserver 192.168.122.1                        |
|                                                   |
| dig google.com                                    |
+--------------+------------------------------------+
               | consulta a 192.168.122.1:53
               v
+-- HOST: dnsmasq en virbr0 -----------------------+
| Escucha en 192.168.122.1:53                       |
|                                                   |
| 1. Es un hostname de una VM?                      |
|    (ej. "fedora-vm" -> 192.168.122.100)           |
|    -> SI: responde directamente                   |
|                                                   |
| 2. Esta en la cache de dnsmasq?                   |
|    -> SI: responde desde cache                    |
|                                                   |
| 3. Reenvia al DNS del host                        |
|    (lee /etc/resolv.conf del host)                |
+--------------+------------------------------------+
               | consulta al DNS real
               v
        DNS del ISP / 8.8.8.8 / 1.1.1.1
```

### Resolucion automatica de hostnames de VMs

Cuando dnsmasq asigna una IP por DHCP, registra el hostname de la VM. Otras VMs en la misma red pueden resolver ese nombre:

```bash
# VM "fedora-vm" tiene 192.168.122.100 (asignada por DHCP con hostname)
# VM "debian-db" tiene 192.168.122.101

# Desde debian-db:
ping fedora-vm
# PING fedora-vm (192.168.122.100)

# Esto funciona porque dnsmasq registro:
# fedora-vm -> 192.168.122.100 cuando asigno el lease
```

### Resolucion inversa automatica

dnsmasq tambien crea registros PTR automaticamente:

```bash
# Desde cualquier VM en la red:
dig -x 192.168.122.100 @192.168.122.1 +short
# fedora-vm
```

### DNS personalizado para un lab

Puedes anadir registros DNS personalizados al dnsmasq de libvirt:

```bash
# Metodo 1: archivo addnhosts
echo "192.168.122.100 web.lab.local" | sudo tee -a \
    /var/lib/libvirt/dnsmasq/default.addnhosts

# Recargar dnsmasq para que lea el archivo
sudo kill -HUP $(pidof dnsmasq)  # senal HUP = recargar config

# Metodo 2: en el XML de la red (virsh net-edit)
# Dentro de <network>, anadir:
#   <dns>
#     <host ip='192.168.122.100'>
#       <hostname>web.lab.local</hostname>
#     </host>
#   </dns>
```

---

## 13. Resolucion de nombres entre VMs

### Escenario: lab multi-VM

```
+-- VM1: web ----------+  +-- VM2: api ----------+  +-- VM3: db -----------+
| 192.168.122.100       |  | 192.168.122.101       |  | 192.168.122.102       |
| hostname: web-server  |  | hostname: api-server  |  | hostname: db-server   |
+-----------------------+  +-----------------------+  +-----------------------+
            |                       |                        |
            +--------- virbr0 (dnsmasq: 192.168.122.1) -----+
```

### Con dnsmasq automatico (si los hostnames se configuran en las VMs)

```bash
# Desde web-server:
ping api-server     # resuelve a 192.168.122.101
ping db-server      # resuelve a 192.168.122.102

# Desde api-server:
psql -h db-server -p 5432 -U myuser mydb   # conexion a PostgreSQL por nombre
```

Para que esto funcione, cada VM debe enviar su hostname en la solicitud DHCP. La mayoria de distribuciones lo hacen automaticamente.

### Verificar que el hostname se envia en DHCP

```bash
# Dentro de la VM:
hostnamectl
# Static hostname: web-server

# Verificar que DHCP envia el hostname:
# En Fedora/RHEL con NetworkManager, se envia automaticamente.
# En Debian minimo, puede que no. Verificar:
nmcli connection show "Wired connection 1" | grep dhcp-hostname
```

### Cuando dnsmasq no resuelve hostnames

Si `ping web-server` falla desde otra VM:

```bash
# Verificar que dnsmasq tiene el registro
dig @192.168.122.1 web-server +short
# Si no devuelve nada: el hostname no se registro

# Verificar leases (el hostname aparece aqui si se envio)
virsh net-dhcp-leases default
# La columna "Hostname" muestra el nombre o esta vacia?

# Solucion 1: asegurar que la VM envia hostname en DHCP
sudo hostnamectl set-hostname web-server
# Y renovar DHCP: sudo dhclient -r eth0 && sudo dhclient eth0

# Solucion 2: anadir el nombre manualmente via DNS en el XML de la red
virsh net-edit default
# Anadir dentro de <network>:
# <dns>
#   <host ip='192.168.122.100'><hostname>web-server</hostname></host>
# </dns>
```

### /etc/hosts como fallback

Si dnsmasq no resuelve nombres automaticamente, puedes usar /etc/hosts dentro de cada VM:

```bash
# En CADA VM, anadir:
cat >> /etc/hosts << 'EOF'
192.168.122.100  web-server
192.168.122.101  api-server
192.168.122.102  db-server
EOF
```

Funciona, pero no escala y no se actualiza automaticamente.

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

El problema es DNS, no la red. Diagnostico:

```bash
# 1. Que DNS esta configurado?
cat /etc/resolv.conf
# Si esta vacio o apunta a algo inaccesible -> no hay DNS

# 2. El DNS responde?
dig @192.168.122.1 google.com
# Si timeout -> dnsmasq no esta corriendo

# 3. dnsmasq esta activo?
ps aux | grep dnsmasq

# Solucion temporal:
echo "nameserver 8.8.8.8" | sudo tee /etc/resolv.conf

# Solucion permanente: arreglar la red virtual o NetworkManager
```

### Error 2: Editar /etc/resolv.conf y que se sobrescriba

```bash
# Editas /etc/resolv.conf manualmente:
sudo vim /etc/resolv.conf    # anades nameserver 8.8.8.8

# Reinicia NetworkManager o DHCP renueva:
# -> Tu cambio se pierde

# Solucion con NetworkManager:
nmcli connection modify "Wired connection 1" ipv4.dns "8.8.8.8 1.1.1.1"
nmcli connection up "Wired connection 1"

# Solucion con systemd-resolved:
# Editar /etc/systemd/resolved.conf:
# [Resolve]
# DNS=8.8.8.8 1.1.1.1
sudo systemctl restart systemd-resolved
```

### Error 3: Cache DNS obsoleta

```bash
# Cambiaste la IP de una VM pero el nombre sigue apuntando a la IP vieja

# Con systemd-resolved:
resolvectl flush-caches

# Con dnsmasq de libvirt (reiniciar la red):
virsh net-destroy default && virsh net-start default

# O simplemente esperar a que el TTL expire
```

### Error 4: VMs no se resuelven por nombre entre si

```bash
# Desde VM1: ping vm2 -> falla
# Pero ping 192.168.122.101 -> funciona

# Causas posibles:
# 1. VM2 no envio hostname en DHCP -> verificar virsh net-dhcp-leases
# 2. La VM usa resolv.conf incorrecto -> debe apuntar a 192.168.122.1
# 3. nsswitch.conf no consulta DNS -> verificar "hosts: files dns"
```

### Error 5: Confundir 127.0.0.53 con 127.0.0.1

```bash
# /etc/resolv.conf dice: nameserver 127.0.0.53
# Esto NO es loopback general — es el stub resolver de systemd-resolved

# Si systemd-resolved no esta corriendo:
dig @127.0.0.53 google.com
# ;; connection timed out  <- no hay nadie escuchando

# 127.0.0.1 es loopback real
# 127.0.0.53 es el stub resolver de systemd-resolved
# Son direcciones diferentes (ambas en 127.0.0.0/8, pero distintos servicios)
```

---

## 15. Cheatsheet

```text
+======================================================================+
|                      DNS BASICO CHEATSHEET                            |
+======================================================================+
|                                                                      |
|  JERARQUIA DNS                                                       |
|  Root (.) -> TLD (.com) -> Dominio (google.com) -> Subdominio (www.) |
|  13 root servers -> TLD servers -> Authoritative -> Recursive/Cache  |
|                                                                      |
+----------------------------------------------------------------------+
|                                                                      |
|  REGISTROS COMUNES                                                   |
|  A       Nombre -> IPv4                                              |
|  AAAA    Nombre -> IPv6                                              |
|  CNAME   Alias -> otro nombre                                        |
|  MX      Servidor de correo                                          |
|  NS      Servidor de nombres autoritativo                            |
|  PTR     IP -> Nombre (resolucion inversa)                           |
|  TXT     Texto libre (SPF, verificacion)                             |
|                                                                      |
+----------------------------------------------------------------------+
|                                                                      |
|  HERRAMIENTAS                                                        |
|  dig google.com              Consulta completa                       |
|  dig +short google.com       Solo la IP                              |
|  dig @8.8.8.8 google.com    Usar servidor especifico                 |
|  dig -x 8.8.8.8             Resolucion inversa                      |
|  dig +trace google.com      Trazar la cadena completa                |
|  dig AAAA google.com        Consultar IPv6                           |
|  dig MX google.com          Consultar correo                         |
|  host google.com             Consulta simple                         |
|  nslookup google.com         Consulta simple (alt)                   |
|                                                                      |
+----------------------------------------------------------------------+
|                                                                      |
|  ARCHIVOS DE CONFIGURACION                                           |
|  /etc/resolv.conf        Servidores DNS (nameserver, search)         |
|  /etc/hosts              Mapeo local nombre -> IP (prioridad)        |
|  /etc/nsswitch.conf      Orden de resolucion (files dns ...)         |
|                                                                      |
+----------------------------------------------------------------------+
|                                                                      |
|  SYSTEMD-RESOLVED                                                    |
|  resolvectl status       Estado y DNS por interfaz                   |
|  resolvectl flush-caches Limpiar cache DNS                           |
|  resolvectl query X      Resolver un nombre                         |
|  resolvectl statistics   Hits/misses de cache                        |
|  Stub resolver: 127.0.0.53 (no confundir con 127.0.0.1)             |
|                                                                      |
+----------------------------------------------------------------------+
|                                                                      |
|  DNS EN LIBVIRT                                                      |
|  dnsmasq actua como DNS forwarder + resuelve hostnames de VMs        |
|  VMs con hostname en DHCP -> otras VMs las resuelven por nombre      |
|  Registros PTR automaticos (resolucion inversa)                      |
|  DNS personalizado: addnhosts o <dns><host> en XML de red            |
|  Recargar: kill -HUP $(pidof dnsmasq)                                |
|                                                                      |
+----------------------------------------------------------------------+
|                                                                      |
|  TROUBLESHOOTING                                                     |
|  "Temporary failure" -> ping IP funciona pero DNS no -> resolver     |
|  resolv.conf se sobrescribe -> configurar via nmcli/resolved         |
|  Cache obsoleta -> resolvectl flush-caches / reiniciar red           |
|  VMs no se resuelven -> verificar hostname en DHCP lease             |
|                                                                      |
+======================================================================+
```

---

## 16. Ejercicios

### Ejercicio 1: Jerarquia y tipos de servidor

Un usuario escribe `curl maps.google.com` en una VM con libvirt.

**Pregunta:** Nombra los 4 tipos de servidores DNS involucrados en la resolucion (asumiendo cache vacia en todos los niveles) y que pregunta/respuesta ocurre en cada uno.

<details><summary>Prediccion</summary>

1. **Resolver recursivo** (dnsmasq en 192.168.122.1, que reenvia al DNS del host/ISP): recibe la consulta de la VM. No tiene cache. Comienza la busqueda.

2. **Root server** (ej. a.root-servers.net): el resolver le pregunta "quien gestiona `.com`?". Responde con la IP del TLD server de .com.

3. **TLD server de .com** (ej. Verisign): el resolver le pregunta "quien gestiona `google.com`?". Responde con la IP del authoritative server de Google (ej. ns1.google.com).

4. **Authoritative server** (ns1.google.com): el resolver le pregunta "cual es la IP de `maps.google.com`?". Responde con la IP (ej. 142.250.184.14, TTL 300).

El resolver cachea la respuesta y la devuelve a la VM. Notar que `maps.google.com` es un subdominio — el TLD server solo conoce `google.com`, y el authoritative de Google resuelve tanto `google.com` como sus subdominios.

</details>

---

### Ejercicio 2: Tipos de registros DNS

Ejecuta estos comandos y responde:

```bash
dig +short google.com          # A
dig +short AAAA google.com     # AAAA
dig +short MX google.com       # MX
dig +short NS google.com       # NS
dig -x 8.8.8.8 +short          # PTR
```

**Pregunta:** Para cada comando, que tipo de registro consultas y que informacion esperas obtener? Cual es la diferencia entre un registro A y un AAAA?

<details><summary>Prediccion</summary>

| Comando | Tipo | Informacion |
|---------|------|-------------|
| `dig +short google.com` | A | Direccion(es) IPv4 de google.com (ej. 142.250.184.14) |
| `dig +short AAAA google.com` | AAAA | Direccion(es) IPv6 de google.com (ej. 2a00:1450:4003:80e::200e) |
| `dig +short MX google.com` | MX | Servidor(es) de correo con prioridad (ej. 10 smtp.google.com.) |
| `dig +short NS google.com` | NS | Servidores de nombres autoritativos (ej. ns1.google.com., ns2.google.com.) |
| `dig -x 8.8.8.8 +short` | PTR | Resolucion inversa: nombre asociado a la IP (ej. dns.google.) |

**A vs AAAA**: A mapea un nombre a IPv4 (32 bits, 4 octetos). AAAA mapea a IPv6 (128 bits, 8 grupos hex). Se llama "AAAA" porque IPv6 es 4 veces mas largo que IPv4 (4 x A). Un dominio puede tener ambos registros simultaneamente (dual-stack).

</details>

---

### Ejercicio 3: TTL y cache

Ejecutas `dig google.com` dos veces seguidas:

```
# Primera vez:
;; Query time: 45 msec
google.com.    300    IN    A    142.250.184.14

# Segunda vez (inmediatamente despues):
;; Query time: 0 msec
google.com.    287    IN    A    142.250.184.14
```

**Pregunta:** Por que el Query time bajo a 0ms? Por que el TTL cambio de 300 a 287? Si necesitas migrar google.com a otra IP, que estrategia usarias con el TTL?

<details><summary>Prediccion</summary>

- **Query time 0ms**: la segunda consulta se respondio desde la **cache** del resolver (dnsmasq, systemd-resolved, o el resolver recursivo). No hubo consulta a ningun servidor externo.

- **TTL 300 -> 287**: el TTL es un **contador descendente**. Cuando el resolver cacheo la respuesta, el TTL era 300. Pasaron 13 segundos entre las dos consultas, asi que el TTL restante es 300 - 13 = 287. Cuando llegue a 0, la cache expira y la proxima consulta hara una resolucion completa.

- **Estrategia de migracion**:
  1. Reducir el TTL de 300 a 60 (o menos) con dias de antelacion.
  2. Esperar al menos 300 segundos (el TTL anterior) para que todas las caches expiren el valor viejo.
  3. Cambiar la IP en el registro A.
  4. Los clientes renovaran en maximo 60 segundos (el nuevo TTL corto).
  5. Una vez estabilizado, subir el TTL de vuelta a 300.

</details>

---

### Ejercicio 4: /etc/hosts vs dig

Anades esta linea a `/etc/hosts`:

```
127.0.0.1  google.com
```

**Pregunta:** Despues de este cambio, que resultado daria `ping google.com`? Y `dig google.com`? Por que son diferentes?

<details><summary>Prediccion</summary>

- **`ping google.com`** -> resuelve a **127.0.0.1** (localhost). El ping respondera con respuestas de loopback. `ping` usa la biblioteca C `getaddrinfo()`, que respeta el orden de `nsswitch.conf` (`hosts: files dns`). Como `files` (/etc/hosts) va primero y tiene una entrada para google.com, devuelve 127.0.0.1 sin consultar DNS.

- **`dig google.com`** -> resuelve a la IP real (ej. **142.250.184.14**). `dig` **no** consulta /etc/hosts. Es una herramienta de diagnostico DNS pura: envia la consulta directamente al servidor DNS configurado en /etc/resolv.conf (o al que especifiques con @). Ignora completamente nsswitch.conf y /etc/hosts.

- **Por que son diferentes**: `ping` (y `curl`, `ssh`, navegadores, etc.) usan el resolver del sistema (getaddrinfo -> nsswitch.conf -> files -> dns). `dig` bypasea el sistema y consulta DNS directamente. Esta diferencia es critica para diagnostico: si `ping` falla pero `dig` funciona, el problema esta en /etc/hosts o nsswitch.conf, no en DNS.

</details>

---

### Ejercicio 5: nsswitch.conf y orden de resolucion

Tu nsswitch.conf dice:

```
hosts: files dns mymachines
```

**Pregunta:** Si /etc/hosts tiene `192.168.1.50 miapp.local` y DNS tiene un registro A `miapp.local -> 10.0.0.99`, que IP devolvera `getaddrinfo("miapp.local")`? Que pasaria si el orden fuera `hosts: dns files`?

<details><summary>Prediccion</summary>

- **Con `files dns`**: devuelve **192.168.1.50**. El sistema consulta /etc/hosts primero (`files`). Encuentra `miapp.local -> 192.168.1.50` y **para ahi** — nunca llega a consultar DNS.

- **Con `dns files`**: devuelve **10.0.0.99**. El sistema consulta DNS primero. Encuentra el registro A `miapp.local -> 10.0.0.99` y para ahi — nunca llega a consultar /etc/hosts.

El orden en nsswitch.conf determina la prioridad. En la practica, `files dns` es el orden estandar porque permite overrides locales via /etc/hosts (util para desarrollo, testing, bloqueo de dominios). Invertirlo haria que /etc/hosts fuera ignorado siempre que DNS tenga respuesta.

</details>

---

### Ejercicio 6: systemd-resolved vs resolver tradicional

Tu /etc/resolv.conf contiene:

```
nameserver 127.0.0.53
```

**Pregunta:** Que servicio esta detras de 127.0.0.53? Si ejecutas `sudo systemctl stop systemd-resolved` y luego `dig google.com`, que pasara? Como lo arreglarias temporalmente?

<details><summary>Prediccion</summary>

- **127.0.0.53** es el **stub resolver** de `systemd-resolved`. No es loopback general (127.0.0.1). Es un servicio que escucha en esa IP especifica del rango 127.0.0.0/8, cachea respuestas DNS, y reenvia consultas al DNS real configurado por interfaz.

- **Tras detener systemd-resolved**: `dig google.com` dara **timeout** (`;; connection timed out; no servers could be reached`). No hay nadie escuchando en 127.0.0.53:53. La resolucion DNS del sistema completo se rompe — `ping google.com`, `curl`, navegadores, todo fallara.

- **Solucion temporal**: reemplazar el symlink o contenido de /etc/resolv.conf con un DNS real:
  ```bash
  echo "nameserver 8.8.8.8" | sudo tee /etc/resolv.conf
  ```
  Esto bypasea systemd-resolved y usa Google DNS directamente. Se perdera al reiniciar systemd-resolved (que sobrescribira el archivo).

</details>

---

### Ejercicio 7: DNS en libvirt — dnsmasq como forwarder

Tienes una VM en la red default de libvirt. Dentro de la VM, `/etc/resolv.conf` dice `nameserver 192.168.122.1`.

**Pregunta:** Cuando la VM ejecuta `dig github.com`, describe el recorrido completo de la consulta. Cuantos "saltos" DNS hay entre la VM y la respuesta final?

<details><summary>Prediccion</summary>

Recorrido completo (asumiendo cache vacia en todos los niveles):

1. **VM -> dnsmasq (192.168.122.1:53)**: la VM envia la consulta al DNS configurado. dnsmasq es el servidor DNS de libvirt corriendo en el host, escuchando en el bridge virbr0.

2. **dnsmasq -> DNS del host**: "github.com" no es un hostname de VM (no tiene lease DHCP para ese nombre) ni esta en cache. dnsmasq lee el /etc/resolv.conf **del host** y reenvia la consulta al DNS configurado ahi (ej. 192.168.1.1 del router, o 8.8.8.8).

3. **DNS del host/router -> resolver recursivo del ISP (o 8.8.8.8)**: si el router domestico solo reenvia, la consulta llega al resolver recursivo real.

4. **Resolver recursivo -> root -> TLD .com -> authoritative de github.com**: el resolver hace la resolucion iterativa completa y obtiene la IP.

5. **Respuesta viaja de vuelta**: authoritative -> resolver -> router -> dnsmasq -> VM.

Hay **3 saltos DNS** (VM->dnsmasq, dnsmasq->DNS del host, DNS del host->resolver recursivo), y despues el resolver hace 3 consultas iterativas (root, TLD, authoritative). Cada nivel que tiene cache puede acortar la cadena.

</details>

---

### Ejercicio 8: Resolucion entre VMs

Tienes dos VMs en la red default: "web" (192.168.122.100) y "api" (192.168.122.101). Desde "web" ejecutas `ping api` y funciona. Luego creas una tercera VM "db" pero `ping db` desde "web" falla con "Name or service not known".

**Pregunta:** Cual es la causa mas probable y como la diagnosticarias? Da 3 posibles razones.

<details><summary>Prediccion</summary>

Las 3 razones mas probables:

1. **La VM "db" no envio su hostname en la solicitud DHCP**. Diagnostico:
   ```bash
   virsh net-dhcp-leases default
   ```
   Si la columna "Hostname" esta vacia para la IP de "db", el hostname no se registro. Solucion: dentro de "db", ejecutar `sudo hostnamectl set-hostname db` y luego `sudo dhclient -r eth0 && sudo dhclient eth0`.

2. **La VM "db" tiene un hostname diferente al que esperas**. Quizas el hostname es "db-server" o "localhost" en vez de "db". Diagnostico: dentro de "db", ejecutar `hostnamectl` y verificar. El nombre que dnsmasq registra es el que el cliente DHCP envia, no necesariamente el que asumes.

3. **La VM "db" tiene IP estatica (sin DHCP)**. Si configuraste la VM con IP manual, no hubo intercambio DHCP y dnsmasq no registro ningun hostname. Diagnostico: `nmcli device show eth0 | grep METHOD` — si dice "manual" en vez de "auto", no hay DHCP. Solucion: usar reserva DHCP en vez de IP estatica, o anadir el registro DNS manualmente en el XML de la red.

</details>

---

### Ejercicio 9: DNS personalizado en XML de libvirt

Necesitas que las VMs de tu lab resuelvan estos nombres:

| Nombre | IP |
|--------|-----|
| web.lab | 192.168.122.10 |
| api.lab | 192.168.122.11 |
| db.lab | 192.168.122.12 |

**Pregunta:** Escribe el fragmento XML `<dns>` para `virsh net-edit default`. Despues de anadirlo, desde la VM "api" se ejecuta `psql -h db.lab`. Que pasaria si el host (no una VM) ejecuta `dig db.lab`? Funcionaria?

<details><summary>Prediccion</summary>

Fragmento XML (dentro de `<network>`, antes de `<ip>`):

```xml
<dns>
  <host ip='192.168.122.10'>
    <hostname>web.lab</hostname>
  </host>
  <host ip='192.168.122.11'>
    <hostname>api.lab</hostname>
  </host>
  <host ip='192.168.122.12'>
    <hostname>db.lab</hostname>
  </host>
</dns>
```

- **Desde la VM "api"**: `psql -h db.lab` **funciona**. La VM consulta a 192.168.122.1 (dnsmasq), que tiene el registro `db.lab -> 192.168.122.12` configurado en el XML. dnsmasq responde directamente.

- **Desde el host**: `dig db.lab` **no funcionaria** por defecto. El host no usa el dnsmasq de libvirt como su DNS — usa su propio DNS (router, systemd-resolved, etc.). dnsmasq de libvirt solo escucha en 192.168.122.1 (virbr0), no en la interfaz del host.

  Solucion para el host:
  - `dig @192.168.122.1 db.lab` — consultar dnsmasq directamente (funciona pero no es automatico).
  - Anadir las entradas a `/etc/hosts` del host.
  - Configurar systemd-resolved para usar 192.168.122.1 como DNS para el dominio `.lab`.

</details>

---

### Ejercicio 10: Diagnostico completo — "Temporary failure in name resolution"

Dentro de una VM ejecutas:

```bash
ping google.com
# ping: google.com: Temporary failure in name resolution

ping 8.8.8.8
# 64 bytes from 8.8.8.8: icmp_seq=1 ttl=117 time=12.3 ms
```

**Pregunta:** La red funciona (ping por IP OK) pero DNS no. Describe los 5 pasos de diagnostico en orden, que comando ejecutarias en cada paso, y que buscas.

<details><summary>Prediccion</summary>

1. **Verificar /etc/resolv.conf**:
   ```bash
   cat /etc/resolv.conf
   ```
   Buscar: que haya al menos un `nameserver` valido. Si el archivo esta vacio o apunta a una IP incorrecta, ahi esta el problema. En una VM libvirt, debe decir `nameserver 192.168.122.1`.

2. **Probar el DNS configurado directamente**:
   ```bash
   dig @192.168.122.1 google.com
   ```
   Buscar: si responde, el servidor DNS funciona y el problema esta en resolv.conf o nsswitch.conf. Si da timeout, el problema esta en dnsmasq.

3. **Verificar nsswitch.conf**:
   ```bash
   grep ^hosts /etc/nsswitch.conf
   ```
   Buscar: que incluya `dns` en la linea. Si solo dice `hosts: files`, el sistema nunca consulta DNS.

4. **Desde el host — verificar dnsmasq**:
   ```bash
   ps aux | grep dnsmasq
   virsh net-list --all
   ```
   Buscar: que dnsmasq este corriendo y la red este activa. Si la red esta "inactive", dnsmasq no existe y no hay servidor DNS para las VMs.

5. **Desde el host — verificar conectividad del bridge**:
   ```bash
   ip addr show virbr0
   ```
   Buscar: que virbr0 exista y tenga la IP 192.168.122.1. Si no existe, la red virtual no esta configurada correctamente.

Solucion temporal inmediata dentro de la VM: `echo "nameserver 8.8.8.8" | sudo tee /etc/resolv.conf` (bypasea dnsmasq y usa Google DNS directamente).

</details>

---

> **Nota**: DNS es un sistema elegante que hace invisible la complejidad de traducir nombres a IPs. En virtualizacion con libvirt, dnsmasq te da DNS integrado gratis: resuelve hostnames de VMs, hace forwarding al DNS real, y genera registros PTR automaticamente. Para labs pequenos esto es suficiente. Para labs mas complejos con dominios personalizados, puedes usar la seccion `<dns>` del XML de la red. Antes de editar `/etc/resolv.conf` manualmente, verifica si tu sistema usa systemd-resolved o NetworkManager — de lo contrario tus cambios se perderan al reiniciar.
