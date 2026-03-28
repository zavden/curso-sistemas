# systemd-resolved

## Índice
1. [Qué es systemd-resolved](#qué-es-systemd-resolved)
2. [Arquitectura](#arquitectura)
3. [resolvectl: la herramienta de gestión](#resolvectl-la-herramienta-de-gestión)
4. [Configuración](#configuración)
5. [DNS over TLS (DoT)](#dns-over-tls-dot)
6. [DNSSEC](#dnssec)
7. [DNS split y routing por interfaz](#dns-split-y-routing-por-interfaz)
8. [mDNS y LLMNR](#mdns-y-llmnr)
9. [Modos de /etc/resolv.conf](#modos-de-etcresolvconf)
10. [Diagnóstico y troubleshooting](#diagnóstico-y-troubleshooting)
11. [Errores comunes](#errores-comunes)
12. [Cheatsheet](#cheatsheet)
13. [Ejercicios](#ejercicios)

---

## Qué es systemd-resolved

`systemd-resolved` es un servicio de resolución DNS local que actúa como intermediario entre las aplicaciones y los servidores DNS externos. Viene habilitado por defecto en Fedora, Ubuntu, Arch y la mayoría de distribuciones modernas con systemd:

```
┌──────────────────────────────────────────────────────────────┐
│  Sin systemd-resolved:                                       │
│                                                              │
│  App → glibc stub → /etc/resolv.conf → DNS externo (8.8.8.8)│
│                     ↑                                        │
│                     Sin caché local, sin DoT, sin split DNS  │
│                                                              │
│  Con systemd-resolved:                                       │
│                                                              │
│  App → glibc/NSS → systemd-resolved (127.0.0.53)            │
│                         │                                    │
│                         ├── Caché local                      │
│                         ├── DNS over TLS                     │
│                         ├── DNSSEC validation                │
│                         ├── DNS split por interfaz           │
│                         ├── mDNS (.local)                    │
│                         ├── LLMNR                            │
│                         └──── → DNS externo                  │
└──────────────────────────────────────────────────────────────┘
```

### Qué aporta sobre el stub resolver clásico

| Función | Stub resolver (glibc) | systemd-resolved |
|---------|----------------------|------------------|
| Caché DNS local | No | Sí |
| DNS over TLS | No | Sí |
| DNSSEC | No | Sí |
| DNS diferente por interfaz | No | Sí |
| mDNS (.local) | No (necesita Avahi separado) | Sí (integrado) |
| Negative caching | No | Sí |
| Estadísticas | No | Sí (resolvectl statistics) |
| Configuración dinámica | No (reiniciar red) | Sí (resolvectl en caliente) |

---

## Arquitectura

```
┌──────────────────────────────────────────────────────────────┐
│                systemd-resolved                              │
│                                                              │
│  ┌────────────────────────────────────────────┐              │
│  │  Interfaces de entrada                     │              │
│  │                                            │              │
│  │  127.0.0.53:53  ← stub listener (default)  │              │
│  │    Aplica search domains y ndots            │              │
│  │    Las apps consultan aquí                  │              │
│  │                                            │              │
│  │  127.0.0.54:53  ← listener directo         │              │
│  │    NO aplica search domains                 │              │
│  │    Para apps que gestionan sus propios       │              │
│  │    search domains (ej. dig)                 │              │
│  │                                            │              │
│  │  D-Bus API      ← interfaz programática    │              │
│  │    NSS module "resolve" usa esta vía        │              │
│  │    Más rica que DNS (devuelve interfaz,     │              │
│  │    DNSSEC status, etc.)                     │              │
│  └────────────────────────────────────────────┘              │
│                          │                                   │
│                          ▼                                   │
│  ┌────────────────────────────────────────────┐              │
│  │  Motor de resolución                       │              │
│  │                                            │              │
│  │  ┌──────────┐  ┌──────────┐  ┌──────────┐ │              │
│  │  │  Caché   │  │ Routing  │  │ DNSSEC   │ │              │
│  │  │  local   │  │ por link │  │ validador│ │              │
│  │  └──────────┘  └──────────┘  └──────────┘ │              │
│  └────────────────────────────────────────────┘              │
│                          │                                   │
│                          ▼                                   │
│  ┌────────────────────────────────────────────┐              │
│  │  Interfaces de salida                      │              │
│  │                                            │              │
│  │  Link 2 (enp0s3): DNS 192.168.1.1          │              │
│  │    Domains: ~home.local                     │              │
│  │                                            │              │
│  │  Link 5 (tun0): DNS 10.0.0.1               │              │
│  │    Domains: ~corp.example.com               │              │
│  │                                            │              │
│  │  Global fallback: DNS 8.8.8.8               │              │
│  └────────────────────────────────────────────┘              │
└──────────────────────────────────────────────────────────────┘
```

### 127.0.0.53 vs 127.0.0.54

```
┌──────────────────────────────────────────────────────────────┐
│  127.0.0.53 (stub listener):                                 │
│  • Aplica search domains de la interfaz                      │
│  • Aplica configuración ndots                                │
│  • Es lo que /etc/resolv.conf apunta por defecto             │
│  • Comportamiento: como un resolver recursivo local           │
│                                                              │
│  127.0.0.54 (direct listener):                               │
│  • NO aplica search domains                                  │
│  • Pasa la consulta tal cual al DNS upstream                 │
│  • Útil para herramientas que gestionan sus propios           │
│    search domains (como dig, que añade los suyos)            │
│  • Disponible desde systemd 252+                             │
│                                                              │
│  En la práctica, las aplicaciones usan 127.0.0.53            │
│  a través de /etc/resolv.conf y rara vez necesitas           │
│  interactuar con 127.0.0.54 directamente.                    │
└──────────────────────────────────────────────────────────────┘
```

---

## resolvectl: la herramienta de gestión

`resolvectl` (antes `systemd-resolve`) es la herramienta principal para interactuar con systemd-resolved:

### Estado general

```bash
# Estado completo: interfaces, DNS, dominios, protocolos
resolvectl status
```

```
Global
  Protocols: +LLMNR +mDNS -DNSOverTLS DNSSEC=allow-downgrade/supported
  resolv.conf mode: stub
  Current DNS Server: 8.8.8.8
  DNS Servers: 8.8.8.8 1.1.1.1
  Fallback DNS Servers: 9.9.9.9 8.8.4.4

Link 2 (enp0s3)
  Current Scopes: DNS LLMNR/IPv4 LLMNR/IPv6
  Protocols: +DefaultRoute +LLMNR -mDNS -DNSOverTLS
             DNSSEC=allow-downgrade/supported
  Current DNS Server: 192.168.1.1
  DNS Servers: 192.168.1.1
  DNS Domain: home.local
```

### Resolver nombres

```bash
# Resolver un nombre (más detallado que dig)
resolvectl query www.example.com
# www.example.com: 93.184.216.34            -- link: enp0s3
#                  2606:2800:220:1:...       -- link: enp0s3
#
# -- Information acquired via protocol DNS in 45.2ms.
# -- Data is authenticated: no; Data was acquired via local or encrypted transport: no
# -- Data from: network

# Resolver con información DNSSEC
resolvectl query --legend=no www.example.com
# Salida más limpia sin la leyenda explicativa

# Resolución inversa
resolvectl query 8.8.8.8
# 8.8.8.8: dns.google                       -- link: enp0s3
```

La salida de `resolvectl query` incluye información que dig no muestra: qué interfaz de red se usó, si los datos fueron autenticados (DNSSEC), y si se usó transporte cifrado.

### Estadísticas de caché

```bash
resolvectl statistics
```

```
Transactions
  Current Transactions: 0
    Total Transactions: 4521

Cache
  Current Cache Size: 247         ← registros en caché
    Cache Hits: 3892              ← consultas servidas desde caché
    Cache Misses: 629             ← consultas que fueron al DNS externo

DNSSEC Verdicts
    Secure: 0                     ← respuestas con DNSSEC válido
    Insecure: 4521                ← respuestas sin DNSSEC
    Bogus: 0                      ← respuestas con DNSSEC inválido
    Indeterminate: 0
```

```
┌──────────────────────────────────────────────────────────────┐
│  Cache Hit Rate = Hits / (Hits + Misses)                     │
│                 = 3892 / (3892 + 629)                        │
│                 = 86.1%                                      │
│                                                              │
│  Un hit rate alto (>80%) indica que el caché funciona bien.  │
│  Un hit rate bajo puede indicar:                             │
│  • TTLs muy cortos en los dominios consultados               │
│  • Muchas consultas únicas (cada dominio solo 1 vez)         │
│  • El caché fue limpiado recientemente                       │
└──────────────────────────────────────────────────────────────┘
```

### Gestión de caché

```bash
# Limpiar toda la caché
resolvectl flush-caches

# Verificar que se limpió
resolvectl statistics | grep "Current Cache Size"
# Current Cache Size: 0

# Reiniciar estadísticas (contadores a cero)
resolvectl reset-statistics
```

### Configuración dinámica por interfaz

```bash
# Ver DNS configurado por interfaz
resolvectl dns
# Global: 8.8.8.8 1.1.1.1
# Link 2 (enp0s3): 192.168.1.1

# Cambiar DNS de una interfaz (temporal, hasta reinicio de red)
resolvectl dns enp0s3 8.8.8.8 1.1.1.1

# Ver search domains por interfaz
resolvectl domain
# Global:
# Link 2 (enp0s3): home.local

# Cambiar search domains de una interfaz
resolvectl domain enp0s3 corp.example.com dev.example.com

# Forzar que una interfaz sea la ruta por defecto
resolvectl default-route enp0s3 yes
```

---

## Configuración

### Archivo principal

```bash
cat /etc/systemd/resolved.conf
```

```ini
[Resolve]
# Servidores DNS principales
#DNS=

# Servidores DNS de respaldo (si ninguna interfaz tiene DNS)
#FallbackDNS=1.1.1.1 9.9.9.9 8.8.8.8 2606:4700:4700::1111

# Dominios de búsqueda globales
#Domains=

# DNS over TLS
#DNSOverTLS=no

# DNSSEC
#DNSSEC=allow-downgrade

# Caché
#Cache=yes

# DNS Stub Listener (127.0.0.53)
#DNSStubListener=yes

# mDNS
#MulticastDNS=yes

# LLMNR
#LLMNR=yes

# Leer DNS de /etc/resolv.conf (modo foreign)
#ReadEtcHosts=yes
```

### Drop-in files (método recomendado)

En lugar de editar `resolved.conf`, usar archivos drop-in:

```bash
# Crear directorio si no existe
sudo mkdir -p /etc/systemd/resolved.conf.d/

# DNS personalizado
sudo tee /etc/systemd/resolved.conf.d/dns.conf << 'EOF'
[Resolve]
DNS=1.1.1.1#cloudflare-dns.com 8.8.8.8#dns.google
FallbackDNS=9.9.9.9#dns.quad9.net
EOF

# Habilitar DoT
sudo tee /etc/systemd/resolved.conf.d/dot.conf << 'EOF'
[Resolve]
DNSOverTLS=opportunistic
EOF

# Aplicar cambios
sudo systemctl restart systemd-resolved

# Verificar
resolvectl status
```

> **¿Por qué drop-in?** Los drop-in files sobreviven a actualizaciones del sistema que podrían sobrescribir `resolved.conf`. Cada archivo en `.conf.d/` se fusiona con la configuración principal.

### Prioridad de DNS

```
┌──────────────────────────────────────────────────────────────┐
│  ¿De dónde obtiene resolved los servidores DNS?              │
│                                                              │
│  1. DNS configurados por interfaz (via NetworkManager/DHCP)  │
│     resolvectl dns enp0s3 → 192.168.1.1                     │
│     Tienen prioridad para dominios de su search domain       │
│                                                              │
│  2. DNS global (en resolved.conf):                           │
│     DNS=8.8.8.8 1.1.1.1                                     │
│     Usado si ninguna interfaz tiene DNS o para consultas     │
│     que no coinciden con ningún search domain de interfaz    │
│                                                              │
│  3. FallbackDNS (en resolved.conf):                          │
│     FallbackDNS=9.9.9.9                                     │
│     Solo si NO hay DNS en ninguna interfaz NI global         │
│     Compiled-in default: 1.1.1.1 y 9.9.9.9                  │
│                                                              │
│  En la práctica, NetworkManager configura DNS por interfaz   │
│  desde DHCP, y resolved los usa automáticamente.             │
└──────────────────────────────────────────────────────────────┘
```

---

## DNS over TLS (DoT)

DNS over TLS cifra las consultas DNS para que tu ISP u otros intermediarios no puedan ver qué dominios resuelves:

```
┌──────────────────────────────────────────────────────────────┐
│  DNS tradicional (puerto 53, texto claro):                   │
│                                                              │
│  Tu PC ──── "quiero resolver google.com" ────► DNS 8.8.8.8  │
│        ◄─── "93.184.216.34" ─────────────────               │
│        │                                                     │
│   ISP/router puede ver: "este usuario visitará google.com"   │
│                                                              │
│  DNS over TLS (puerto 853, cifrado):                         │
│                                                              │
│  Tu PC ════ [TLS cifrado] ══════════════════► DNS 8.8.8.8   │
│        ◄══════════════════════════════════════               │
│        │                                                     │
│   ISP/router ve: "tráfico TLS al puerto 853 de 8.8.8.8"     │
│   NO puede ver qué dominio se resolvió                       │
└──────────────────────────────────────────────────────────────┘
```

### Configurar DoT

```bash
# Modos disponibles:
# no            → solo DNS clásico (puerto 53, texto claro)
# opportunistic → usar TLS si el servidor lo soporta, caer a 53 si no
# yes           → FORZAR TLS, fallar si el servidor no lo soporta

sudo tee /etc/systemd/resolved.conf.d/dot.conf << 'EOF'
[Resolve]
DNS=1.1.1.1#cloudflare-dns.com 8.8.8.8#dns.google
DNSOverTLS=opportunistic
EOF

sudo systemctl restart systemd-resolved
```

### El formato `IP#hostname`

```
┌──────────────────────────────────────────────────────────────┐
│  DNS=1.1.1.1#cloudflare-dns.com                              │
│      ───────  ──────────────────                             │
│        │            │                                        │
│        │            └── nombre del servidor (para verificar  │
│        │                el certificado TLS — SNI)            │
│        └── IP del servidor                                   │
│                                                              │
│  Sin el #hostname, resolved no puede verificar el            │
│  certificado TLS del servidor (¿es realmente 1.1.1.1         │
│  o un impostor?). Con opportunistic esto no es crítico,     │
│  pero con DNSOverTLS=yes es obligatorio.                     │
│                                                              │
│  Servidores que soportan DoT:                                │
│    1.1.1.1#cloudflare-dns.com                                │
│    8.8.8.8#dns.google                                        │
│    9.9.9.9#dns.quad9.net                                     │
└──────────────────────────────────────────────────────────────┘
```

### Verificar que DoT funciona

```bash
# Ver si resolved usa DoT
resolvectl status | grep DNSOverTLS
# DNSOverTLS setting: opportunistic

# Hacer una consulta y verificar
resolvectl query www.example.com
# -- Data was acquired via local or encrypted transport: yes
#                                                       ───
#                                                       ↑ DoT activo

# Verificar con ss que hay conexiones al puerto 853
ss -tnp | grep 853
# ESTAB  0  0  192.168.1.50:45231  1.1.1.1:853
```

---

## DNSSEC

DNSSEC (DNS Security Extensions) permite verificar que las respuestas DNS no fueron manipuladas en tránsito. resolved puede validar firmas DNSSEC:

```
┌──────────────────────────────────────────────────────────────┐
│  Sin DNSSEC:                                                 │
│    DNS: "example.com = 93.184.216.34"                        │
│    ¿Cómo saber si es verdad? No puedes.                      │
│    Un atacante podría haber modificado la respuesta.          │
│                                                              │
│  Con DNSSEC:                                                 │
│    DNS: "example.com = 93.184.216.34"                        │
│         + firma criptográfica que lo demuestra               │
│    resolved verifica la firma con la cadena de confianza     │
│    (root → TLD → dominio). Si es inválida → SERVFAIL.        │
└──────────────────────────────────────────────────────────────┘
```

### Modos de DNSSEC

```ini
# /etc/systemd/resolved.conf.d/dnssec.conf
[Resolve]
# Opciones:
DNSSEC=allow-downgrade    # (default y recomendado)
```

| Modo | Comportamiento |
|------|---------------|
| `no` | No validar DNSSEC nunca |
| `allow-downgrade` | Validar si hay firmas, aceptar si no hay. Protege contra manipulación pero permite dominios sin DNSSEC |
| `yes` | Validar siempre. Rechazar (SERVFAIL) si las firmas fallan o faltan |

```
┌──────────────────────────────────────────────────────────────┐
│  allow-downgrade (recomendado):                              │
│                                                              │
│  Dominio con DNSSEC válido    → aceptar (secure)             │
│  Dominio sin DNSSEC           → aceptar (insecure)           │
│  Dominio con DNSSEC INVÁLIDO  → rechazar (bogus → SERVFAIL)  │
│                                                              │
│  yes (estricto):                                             │
│                                                              │
│  Dominio con DNSSEC válido    → aceptar                      │
│  Dominio sin DNSSEC           → rechazar (SERVFAIL)          │
│  Dominio con DNSSEC INVÁLIDO  → rechazar (SERVFAIL)          │
│                                                              │
│  ⚠ Muchos dominios NO tienen DNSSEC configurado.            │
│  Usar DNSSEC=yes romperá la resolución de esos dominios.     │
│  Solo usar yes en entornos donde TODOS los dominios           │
│  consultados tienen DNSSEC correctamente configurado.        │
└──────────────────────────────────────────────────────────────┘
```

### Verificar DNSSEC

```bash
# Ver estado DNSSEC por consulta
resolvectl query sigok.verteiltesysteme.net
# -- Data is authenticated: yes      ← DNSSEC validado

resolvectl query example.com
# -- Data is authenticated: no       ← dominio sin DNSSEC

# Ver estadísticas DNSSEC
resolvectl statistics | grep -A5 "DNSSEC"
# DNSSEC Verdicts
#     Secure: 12
#     Insecure: 4509
#     Bogus: 0
#     Indeterminate: 0
```

---

## DNS split y routing por interfaz

Una de las funciones más potentes de resolved: enviar consultas DNS a diferentes servidores según el dominio, basándose en la interfaz de red:

### Concepto

```
┌──────────────────────────────────────────────────────────────┐
│  Escenario: laptop con WiFi casero y VPN corporativa         │
│                                                              │
│  Interface wlan0 (WiFi casa):                                │
│    DNS: 192.168.1.1                                          │
│    Domains: ~.              ← ruta por defecto (todo lo demás)│
│                                                              │
│  Interface tun0 (VPN corp):                                  │
│    DNS: 10.0.0.1                                             │
│    Domains: corp.example.com  ← solo estos dominios          │
│                                                              │
│  Resultado:                                                  │
│    "jira.corp.example.com"  ──► 10.0.0.1 (DNS de la VPN)    │
│    "gitlab.corp.example.com"──► 10.0.0.1 (DNS de la VPN)    │
│    "www.google.com"         ──► 192.168.1.1 (DNS casero)     │
│    "www.youtube.com"        ──► 192.168.1.1 (DNS casero)     │
│                                                              │
│  Las consultas corporativas van por la VPN.                  │
│  Todo lo demás va por la conexión casera.                    │
│  Tu ISP no ve las consultas corporativas.                    │
│  Tu empresa no ve tu navegación personal.                    │
└──────────────────────────────────────────────────────────────┘
```

### Configurar DNS split

```bash
# Ver configuración actual de dominios
resolvectl domain
# Global:
# Link 2 (enp0s3): home.local
# Link 5 (tun0): corp.example.com

# El prefijo ~ indica "routing domain" (enrutar consultas que
# coincidan con este sufijo a este DNS):
resolvectl domain tun0 "~corp.example.com"
resolvectl dns tun0 10.0.0.1

# Sin ~, el dominio es un "search domain" (se añade a nombres cortos)
# Con ~, el dominio es un "routing domain" (enruta consultas matching)

# ~. = ruta por defecto (enviar TODAS las consultas no coincidentes)
resolvectl domain enp0s3 "~."
```

### El operador `~` (routing vs search)

```
┌──────────────────────────────────────────────────────────────┐
│  search domain (sin ~):                                      │
│    Domains: corp.example.com                                 │
│    • "server" se expande a "server.corp.example.com"         │
│    • Las consultas para *.corp.example.com VAN a este DNS    │
│    • Doble función: search + routing                         │
│                                                              │
│  routing domain (con ~):                                     │
│    Domains: ~corp.example.com                                │
│    • "server" NO se expande (no es search domain)            │
│    • Las consultas para *.corp.example.com VAN a este DNS    │
│    • Solo routing, sin search                                │
│                                                              │
│  routing catch-all:                                          │
│    Domains: ~.                                               │
│    • TODAS las consultas que no coincidan con otro routing    │
│      domain van a este DNS                                   │
│    • Es la "ruta por defecto" de DNS                         │
└──────────────────────────────────────────────────────────────┘
```

### Configuración persistente con NetworkManager

```bash
# DNS split persistente para la VPN
nmcli connection modify "Corp VPN" \
  ipv4.dns "10.0.0.1" \
  ipv4.dns-search "~corp.example.com"

# Ruta por defecto en la conexión WiFi
nmcli connection modify "Home WiFi" \
  ipv4.dns "1.1.1.1 8.8.8.8" \
  ipv4.dns-search "~."

# Aplicar
nmcli connection up "Home WiFi"
nmcli connection up "Corp VPN"
```

---

## mDNS y LLMNR

resolved integra dos protocolos de resolución local que no usan servidores DNS:

### mDNS (Multicast DNS)

```
┌──────────────────────────────────────────────────────────────┐
│  mDNS resuelve nombres .local sin servidor DNS               │
│                                                              │
│  Protocolo: multicast UDP al grupo 224.0.0.251, puerto 5353  │
│                                                              │
│  Tu laptop: "¿quién es printer.local?"                       │
│    → Envía consulta multicast a toda la red local            │
│                                                              │
│  Impresora: "¡Yo soy printer.local! Mi IP es 192.168.1.100" │
│    → Responde por multicast                                  │
│                                                              │
│  Usos:                                                       │
│  • Descubrir impresoras, Chromecast, dispositivos IoT        │
│  • Acceder a máquinas por nombre en red local sin DNS        │
│  • Bonjour (Apple) y Avahi (Linux) usan mDNS                │
│                                                              │
│  Configurar en resolved:                                     │
│  MulticastDNS=yes     ← habilitado (default en muchas distros)│
│  MulticastDNS=resolve ← solo resolver, no publicar           │
│  MulticastDNS=no      ← deshabilitado                        │
└──────────────────────────────────────────────────────────────┘
```

### LLMNR (Link-Local Multicast Name Resolution)

```
┌──────────────────────────────────────────────────────────────┐
│  LLMNR: similar a mDNS pero protocolo de Microsoft           │
│                                                              │
│  • Resuelve nombres sin .local (solo el hostname)            │
│  • Protocolo: multicast UDP/TCP al grupo 224.0.0.252:5355   │
│  • Compatible con máquinas Windows en la red local           │
│  • Menos estándar que mDNS — some distros lo deshabilitan   │
│                                                              │
│  Configurar en resolved:                                     │
│  LLMNR=yes      ← habilitado                                │
│  LLMNR=resolve  ← solo resolver, no publicar                │
│  LLMNR=no       ← deshabilitado                              │
│                                                              │
│  ⚠ Riesgo de seguridad: LLMNR responde a consultas de       │
│  cualquiera en la red local. Un atacante puede usar          │
│  LLMNR poisoning (similar a ARP spoofing) para redirigir     │
│  tráfico. En entornos corporativos, considerar deshabilitarlo.│
└──────────────────────────────────────────────────────────────┘
```

```bash
# Ver estado de mDNS y LLMNR por interfaz
resolvectl status | grep -E "(mDNS|LLMNR)"
# Protocols: +LLMNR +mDNS

# Habilitar/deshabilitar por interfaz
resolvectl mdns enp0s3 yes
resolvectl llmnr enp0s3 no

# Resolver un nombre .local
resolvectl query myprinter.local
```

---

## Modos de /etc/resolv.conf

resolved puede gestionar `/etc/resolv.conf` de varias formas. El modo determina cómo las aplicaciones que no usan NSS (como dig) consultan DNS:

```bash
# Ver el modo actual
resolvectl status | grep "resolv.conf mode"
# resolv.conf mode: stub
```

```
┌──────────────────────────────────────────────────────────────┐
│  Modo STUB (recomendado):                                    │
│  /etc/resolv.conf → /run/systemd/resolve/stub-resolv.conf    │
│  Contenido: nameserver 127.0.0.53                            │
│  • TODAS las apps usan resolved (caché, DoT, split)          │
│  • dig también pasa por resolved                             │
│                                                              │
│  Modo UPLINK:                                                │
│  /etc/resolv.conf → /run/systemd/resolve/resolv.conf         │
│  Contenido: nameserver 192.168.1.1 (DNS real)                │
│  • Apps con NSS "resolve" usan resolved                      │
│  • dig/host bypasean resolved (consultan 192.168.1.1 directo)│
│  • dig no se beneficia del caché/DoT de resolved             │
│                                                              │
│  Modo STATIC:                                                │
│  /etc/resolv.conf → /usr/lib/systemd/resolv.conf             │
│  Contenido: nameserver 127.0.0.53                            │
│  • Similar a stub, archivo en ubicación diferente            │
│                                                              │
│  Modo FOREIGN:                                               │
│  /etc/resolv.conf es un archivo regular (no symlink)         │
│  • resolved lee de él para saber qué DNS usar                │
│  • Otra cosa lo gestiona (script, otro servicio)             │
└──────────────────────────────────────────────────────────────┘
```

### Cambiar el modo

```bash
# Verificar estado actual
ls -la /etc/resolv.conf

# Cambiar a modo stub (recomendado)
sudo ln -sf /run/systemd/resolve/stub-resolv.conf /etc/resolv.conf

# Cambiar a modo uplink
sudo ln -sf /run/systemd/resolve/resolv.conf /etc/resolv.conf

# Verificar
resolvectl status | grep "resolv.conf mode"
```

---

## Diagnóstico y troubleshooting

### Flujo de diagnóstico con resolved

```
┌──────────────────────────────────────────────────────────────┐
│  Problema: una aplicación no puede resolver un nombre        │
│                                                              │
│  Paso 1: ¿resolved está corriendo?                           │
│  systemctl status systemd-resolved                           │
│  → Si está muerto: systemctl start systemd-resolved          │
│                                                              │
│  Paso 2: ¿qué DNS tiene configurados?                        │
│  resolvectl status                                           │
│  → Verificar que hay DNS servers en alguna interfaz           │
│                                                              │
│  Paso 3: ¿resolved puede resolver?                           │
│  resolvectl query www.example.com                            │
│  → Si funciona: el problema es NSS o la app                 │
│  → Si falla: el problema es resolved o el DNS upstream       │
│                                                              │
│  Paso 4: ¿el DNS upstream funciona?                          │
│  dig @8.8.8.8 www.example.com                                │
│  → Si funciona: el problema es resolved (config, DNSSEC)     │
│  → Si falla: problema de red o DNS upstream                  │
│                                                              │
│  Paso 5: ¿es un problema de DNSSEC?                          │
│  resolvectl query dominio                                    │
│  → Si dice "DNSSEC validation failed":                       │
│    Cambiar a DNSSEC=allow-downgrade o verificar la zona      │
│                                                              │
│  Paso 6: ¿es un problema de routing DNS?                     │
│  resolvectl domain                                           │
│  → ¿El dominio coincide con algún routing domain?            │
│  → ¿Se envía al DNS correcto?                               │
└──────────────────────────────────────────────────────────────┘
```

### Logs de resolved

```bash
# Ver logs en tiempo real
journalctl -u systemd-resolved -f

# Habilitar logging verbose (debug)
sudo mkdir -p /etc/systemd/resolved.conf.d/
sudo tee /etc/systemd/resolved.conf.d/debug.conf << 'EOF'
[Resolve]
# No hay opción de debug en resolved.conf,
# pero se puede activar vía systemd:
EOF

# Activar debug via environment variable
sudo systemctl edit systemd-resolved
# Añadir:
# [Service]
# Environment=SYSTEMD_LOG_LEVEL=debug

sudo systemctl restart systemd-resolved
journalctl -u systemd-resolved -f
# → Muestra cada consulta DNS, qué servidor se usó, caché hits, etc.

# Desactivar debug después:
sudo systemctl revert systemd-resolved
sudo systemctl restart systemd-resolved
```

### Comandos útiles de diagnóstico

```bash
# ¿Está corriendo?
systemctl is-active systemd-resolved
# active

# ¿resolv.conf apunta a resolved?
ls -la /etc/resolv.conf
# lrwxrwxrwx → ../run/systemd/resolve/stub-resolv.conf

# ¿NSS está configurado para usar resolved?
grep ^hosts /etc/nsswitch.conf
# hosts: mymachines resolve [!UNAVAIL=return] files myhostname dns

# Comparar resolved con DNS directo
resolvectl query www.example.com      # vía resolved
dig @8.8.8.8 www.example.com +short   # DNS directo
# Si difieren: problema de caché, DNSSEC, o routing

# Ver a qué servidor se envió una consulta específica
resolvectl query -v www.example.com
# Muestra qué link y qué DNS server se usó
```

---

## Errores comunes

### 1. DNS no funciona y resolved está activo pero sin servidores

```bash
resolvectl status
# Global
#   DNS Servers: (none)
# Link 2 (enp0s3)
#   DNS Servers: (none)

# Resolved no tiene DNS configurados → no puede resolver nada.
# Causas:
# • NetworkManager no pasó los DNS de DHCP a resolved
# • La conexión no tiene DNS configurados
# • No hay FallbackDNS

# Solución rápida:
resolvectl dns enp0s3 8.8.8.8 1.1.1.1

# Solución persistente:
sudo tee /etc/systemd/resolved.conf.d/dns.conf << 'EOF'
[Resolve]
FallbackDNS=1.1.1.1 8.8.8.8
EOF
sudo systemctl restart systemd-resolved
```

### 2. DNSSEC falla para dominios legítimos

```bash
resolvectl query api.mycompany.com
# resolve call failed: DNSSEC validation failed: no-signature

# El dominio no tiene DNSSEC configurado y resolved tiene
# DNSSEC=yes (estricto).

# Solución: cambiar a allow-downgrade (recomendado)
sudo tee /etc/systemd/resolved.conf.d/dnssec.conf << 'EOF'
[Resolve]
DNSSEC=allow-downgrade
EOF
sudo systemctl restart systemd-resolved
```

### 3. nsswitch.conf no tiene el módulo "resolve"

```bash
# Si nsswitch.conf dice:
# hosts: files dns myhostname

# Las aplicaciones usan glibc DNS (ignoran resolved por completo).
# resolved corre pero nadie le pregunta via D-Bus/NSS.
# Solo funciona si resolv.conf apunta a 127.0.0.53 (modo stub).

# Con módulo resolve en nsswitch:
# hosts: mymachines resolve [!UNAVAIL=return] files myhostname dns

# Las aplicaciones usan resolved vía D-Bus (más rico:
# devuelve DNSSEC status, interfaz usada, etc.)
```

### 4. VPN no enruta DNS correctamente

```bash
# Después de conectar la VPN:
resolvectl domain
# Link 5 (tun0):    (ninguno)

# La VPN no configuró search/routing domains.
# Resultado: las consultas corporativas van al DNS de casa.

# Solución manual:
resolvectl domain tun0 "~corp.example.com"
resolvectl dns tun0 10.0.0.1

# Solución persistente (en la config de NetworkManager):
nmcli connection modify "Corp VPN" \
  ipv4.dns-search "~corp.example.com"
```

### 5. dig muestra resultado diferente a la aplicación

```bash
# dig usa resolv.conf directamente (bypass de NSS "resolve")
# Si resolv.conf está en modo uplink (no stub):

dig www.example.com      # consulta 192.168.1.1 directamente
resolvectl query www.example.com  # consulta vía resolved

# Pueden diferir si:
# • resolved tiene el resultado en caché (TTL diferente)
# • resolved aplica DNSSEC y dig no
# • resolved usa un DNS diferente por routing domain

# Solución: asegurar modo stub para que dig también use resolved
ls -la /etc/resolv.conf
# Debe ser symlink a stub-resolv.conf
```

---

## Cheatsheet

```
┌──────────────────────────────────────────────────────────────┐
│              systemd-resolved Cheatsheet                     │
├──────────────────────────────────────────────────────────────┤
│                                                              │
│  SERVICIO:                                                   │
│    systemctl status systemd-resolved    estado               │
│    systemctl restart systemd-resolved   reiniciar            │
│    journalctl -u systemd-resolved -f    logs en vivo         │
│                                                              │
│  RESOLVECTL:                                                 │
│    resolvectl status              estado completo            │
│    resolvectl query NAME          resolver un nombre         │
│    resolvectl statistics          caché hits/misses          │
│    resolvectl flush-caches        limpiar caché              │
│    resolvectl reset-statistics    reiniciar contadores       │
│    resolvectl dns                 ver DNS por interfaz       │
│    resolvectl dns IF IP...        cambiar DNS de interfaz    │
│    resolvectl domain              ver dominios por interfaz  │
│    resolvectl domain IF "~dom"    routing domain             │
│    resolvectl domain IF "dom"     search domain              │
│    resolvectl mdns IF yes/no      mDNS por interfaz          │
│    resolvectl llmnr IF yes/no     LLMNR por interfaz         │
│                                                              │
│  LISTENERS:                                                  │
│    127.0.0.53:53   stub (aplica search domains)              │
│    127.0.0.54:53   directo (no aplica search domains)        │
│                                                              │
│  CONFIGURACIÓN:                                              │
│    /etc/systemd/resolved.conf          principal             │
│    /etc/systemd/resolved.conf.d/*.conf drop-ins              │
│    DNS=IP#hostname                     servidor DNS          │
│    FallbackDNS=IP                      respaldo              │
│    DNSOverTLS=opportunistic|yes|no     cifrado               │
│    DNSSEC=allow-downgrade|yes|no       validación            │
│    MulticastDNS=yes|resolve|no         mDNS                  │
│    LLMNR=yes|resolve|no                LLMNR                 │
│    Cache=yes|no                        caché                 │
│                                                              │
│  MODOS RESOLV.CONF:                                          │
│    stub (recomendado): → stub-resolv.conf (127.0.0.53)       │
│    uplink: → resolv.conf (DNS real, bypass resolved para dig)│
│    Cambiar: ln -sf /run/systemd/resolve/stub-resolv.conf     │
│                     /etc/resolv.conf                         │
│                                                              │
│  DNS SPLIT:                                                  │
│    ~corp.example.com  → routing domain (solo routing)        │
│    corp.example.com   → search domain (search + routing)     │
│    ~.                 → ruta por defecto (catch-all)          │
│    Configurar: resolvectl domain IF "~domain"                │
│    Persistente: nmcli con mod "X" ipv4.dns-search "~domain"  │
│                                                              │
│  DoT:                                                        │
│    DNS=1.1.1.1#cloudflare-dns.com                            │
│    DNSOverTLS=opportunistic                                  │
│    Verificar: resolvectl query → "encrypted transport: yes"  │
│    Verificar: ss -tnp | grep 853                             │
│                                                              │
│  DIAGNÓSTICO:                                                │
│    1. systemctl is-active systemd-resolved                   │
│    2. resolvectl status (¿hay DNS configurados?)             │
│    3. resolvectl query NAME (¿resolved resuelve?)            │
│    4. dig @8.8.8.8 NAME (¿DNS externo funciona?)            │
│    5. grep ^hosts /etc/nsswitch.conf (¿NSS usa resolve?)    │
│    6. ls -la /etc/resolv.conf (¿modo stub?)                  │
│                                                              │
└──────────────────────────────────────────────────────────────┘
```

---

## Ejercicios

### Ejercicio 1: Explorar tu configuración de resolved

Ejecuta estos comandos en tu sistema y analiza la salida:

```bash
# 1. ¿resolved está activo?
systemctl is-active systemd-resolved

# 2. Estado completo
resolvectl status

# 3. ¿Qué modo de resolv.conf?
ls -la /etc/resolv.conf
cat /etc/resolv.conf

# 4. ¿NSS usa resolve?
grep ^hosts /etc/nsswitch.conf

# 5. Estadísticas de caché
resolvectl statistics

# 6. Resolver un dominio
resolvectl query www.google.com
```

**Responde**:
1. ¿Está resolved activo? ¿Qué DNS servers tiene configurados y de dónde vienen (DHCP, manual)?
2. ¿Está DoT habilitado? ¿Y DNSSEC? ¿En qué modo?
3. ¿El resolv.conf está en modo stub? Si no, ¿qué modo usa?
4. ¿nsswitch.conf incluye el módulo `resolve`? Si no, ¿cómo llegan las consultas a resolved?
5. ¿Cuál es el hit rate de la caché? ¿Es un porcentaje razonable?
6. En la salida de `resolvectl query`, ¿los datos fueron adquiridos por transporte cifrado?

**Pregunta de reflexión**: Si tu sistema usa resolved en modo stub pero nsswitch.conf NO tiene el módulo `resolve`, ¿las aplicaciones siguen beneficiándose del caché de resolved? ¿Cómo?

---

### Ejercicio 2: Configurar DNS over TLS

Configura systemd-resolved para usar DNS over TLS con Cloudflare:

1. Crea un archivo drop-in con DNS de Cloudflare y DoT en modo `opportunistic`.
2. Reinicia resolved.
3. Verifica que DoT funciona ejecutando:
   ```bash
   resolvectl query www.example.com
   # Busca: "encrypted transport: yes"
   ```
4. Verifica que hay una conexión TLS al puerto 853:
   ```bash
   ss -tnp | grep 853
   ```
5. Comprueba que la resolución sigue funcionando:
   ```bash
   resolvectl query www.google.com
   dig +short www.google.com
   curl -s -o /dev/null -w "%{http_code}" https://example.com
   ```

**Responde**:
1. ¿Qué diferencia hay entre `opportunistic` y `yes`? ¿Cuál elegiste y por qué?
2. Si cambiaras a `yes` y tu router de casa intercepta DNS en el puerto 853, ¿qué pasaría?
3. ¿DoT protege contra un atacante que controla tu red WiFi? ¿Contra qué protege exactamente?

**Pregunta de reflexión**: DoT usa el puerto 853, que es identificable. Un administrador de red puede bloquear el puerto 853 para forzar DNS en texto claro. ¿Qué alternativa existe que sea más difícil de bloquear? ¿Hay algún servicio que systemd-resolved NO soporte para cifrar DNS?

---

### Ejercicio 3: Simular DNS split

Simula un escenario de DNS split sin necesidad de una VPN real:

1. Verifica tu configuración actual de interfaces y DNS:
   ```bash
   resolvectl status
   ```

2. En tu interfaz principal, configura un routing domain de prueba:
   ```bash
   resolvectl domain TU_INTERFAZ "home.local" "~test.internal"
   ```

3. Verifica la configuración:
   ```bash
   resolvectl domain
   ```

4. Intenta resolver un nombre bajo `test.internal`:
   ```bash
   resolvectl query server.test.internal
   ```

5. Compara con un nombre público:
   ```bash
   resolvectl query www.google.com
   ```

**Responde**:
1. ¿Qué DNS server se usó para `server.test.internal`? ¿Es el de tu interfaz?
2. ¿La consulta a `test.internal` devolvió NXDOMAIN? ¿Por qué? (Pista: ¿hay un servidor DNS que conozca ese dominio?)
3. ¿`home.local` actúa como search domain además de routing domain? Verifica intentando resolver solo `mypc` (sin dominio completo).
4. ¿Qué diferencia habría si usaras `~home.local` (con tilde) en vez de `home.local` (sin tilde)?

Limpia después:
```bash
resolvectl revert TU_INTERFAZ
```

**Pregunta de reflexión**: En un entorno corporativo con VPN, ¿por qué es un error configurar `~.` (catch-all) en la interfaz de la VPN? ¿Qué consultas DNS pasarían por la VPN que no deberían?

---

> **Siguiente tema**: C03/S01/T01 — Funcionamiento DHCP (DORA: Discover, Offer, Request, Acknowledge)
