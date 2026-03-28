# /etc/resolv.conf y /etc/hosts

## Índice
1. [El sistema de resolución de nombres en Linux](#el-sistema-de-resolución-de-nombres-en-linux)
2. [/etc/hosts](#etchosts)
3. [/etc/resolv.conf](#etcresolvconf)
4. [nsswitch.conf: el orquestador](#nsswitchconf-el-orquestador)
5. [systemd-resolved](#systemd-resolved)
6. [NetworkManager y resolución DNS](#networkmanager-y-resolución-dns)
7. [Escenarios de configuración](#escenarios-de-configuración)
8. [Diagnóstico de problemas de resolución](#diagnóstico-de-problemas-de-resolución)
9. [Errores comunes](#errores-comunes)
10. [Cheatsheet](#cheatsheet)
11. [Ejercicios](#ejercicios)

---

## El sistema de resolución de nombres en Linux

Cuando una aplicación necesita resolver un nombre, intervienen varios archivos y servicios. Entender su interacción es fundamental para diagnosticar problemas DNS:

```
┌──────────────────────────────────────────────────────────────┐
│         Flujo de resolución de nombres en Linux              │
│                                                              │
│  Aplicación (curl, firefox, nginx)                           │
│      │                                                       │
│      │ getaddrinfo("www.example.com")                        │
│      ▼                                                       │
│  glibc (NSS - Name Service Switch)                           │
│      │                                                       │
│      │ consulta /etc/nsswitch.conf                           │
│      │ hosts: files dns myhostname                           │
│      │         ─────  ───  ──────────                        │
│      │           │     │       │                             │
│      │           ▼     │       │                             │
│      │  /etc/hosts     │       │                             │
│      │  ¿encontrado?   │       │                             │
│      │  Sí → devolver  │       │                             │
│      │  No ────────────┘       │                             │
│      │           │             │                             │
│      │           ▼             │                             │
│      │  /etc/resolv.conf       │                             │
│      │  (o systemd-resolved)   │                             │
│      │  ¿encontrado?           │                             │
│      │  Sí → devolver          │                             │
│      │  No ────────────────────┘                             │
│      │           │                                           │
│      │           ▼                                           │
│      │  myhostname (hostname local)                          │
│      │  ¿coincide? → devolver                                │
│      │  No → error: nombre no resuelto                       │
│      ▼                                                       │
│  Resultado → aplicación                                      │
└──────────────────────────────────────────────────────────────┘
```

Los archivos clave:

| Archivo | Función |
|---------|---------|
| `/etc/nsswitch.conf` | Define el **orden** de las fuentes de resolución |
| `/etc/hosts` | Tabla local nombre→IP (sin DNS) |
| `/etc/resolv.conf` | Configuración del cliente DNS (qué resolver usar) |
| `/etc/hostname` | El nombre de la máquina |

---

## /etc/hosts

El archivo más antiguo y simple de resolución de nombres. Cada línea mapea una IP a uno o más nombres:

```bash
cat /etc/hosts
```

```
# Formato: IP    FQDN              alias1   alias2
127.0.0.1       localhost
::1             localhost           ip6-localhost

# Entradas personalizadas:
192.168.1.10    db.internal         db
192.168.1.20    app.internal        app
192.168.1.30    cache.internal      cache redis

# Bloquear dominios (apuntar a localhost):
127.0.0.1       ads.example.com
127.0.0.1       tracker.example.com
0.0.0.0         malware.evil.com
```

### Reglas de formato

```
┌──────────────────────────────────────────────────────────────┐
│  Reglas de /etc/hosts:                                       │
│                                                              │
│  • Una IP por línea, seguida de uno o más nombres            │
│  • El primer nombre es el "canónico", los demás son aliases  │
│  • Comentarios con #                                         │
│  • Sin TTL: los cambios son inmediatos (no hay caché)        │
│  • Sin wildcards: *.example.com NO funciona                  │
│  • IPv4 e IPv6 se ponen en líneas separadas                  │
│  • Se procesa de arriba a abajo: la PRIMERA coincidencia gana│
│  • Cambios son locales a esta máquina únicamente             │
│                                                              │
│  ⚠ /etc/hosts NO soporta:                                   │
│  • Registros MX, CNAME, SRV, TXT                            │
│  • Wildcards (*.dominio)                                     │
│  • TTL o expiración                                          │
│  • Distribución a otras máquinas                             │
│  Solo soporta mapeo directo nombre → IP (como registros A/AAAA)
└──────────────────────────────────────────────────────────────┘
```

### Usos prácticos de /etc/hosts

```
┌──────────────────────────────────────────────────────────────┐
│  1. DESARROLLO LOCAL                                         │
│     Probar un sitio antes de cambiar DNS:                    │
│     192.168.1.100  staging.myapp.com                         │
│     → curl https://staging.myapp.com usa la IP local         │
│                                                              │
│  2. REDES INTERNAS SIN DNS                                   │
│     Pequeñas redes sin servidor DNS propio:                  │
│     192.168.1.10  fileserver                                 │
│     192.168.1.20  printer                                    │
│     → ssh fileserver funciona sin DNS                        │
│                                                              │
│  3. BLOQUEO DE DOMINIOS (ad-blocking básico)                 │
│     0.0.0.0  ads.doubleclick.net                             │
│     0.0.0.0  tracking.analytics.com                          │
│     → Las peticiones a estos dominios fallan silenciosamente │
│     (0.0.0.0 es preferible a 127.0.0.1: falla más rápido)   │
│                                                              │
│  4. OVERRIDE TEMPORAL DE DNS                                 │
│     Forzar resolución durante una migración:                 │
│     203.0.113.50  www.mysite.com                             │
│     → Verificar que el nuevo servidor funciona antes         │
│       de cambiar el DNS público                              │
│                                                              │
│  5. LOCALHOST ALIASES                                        │
│     127.0.0.1  myapp.local                                   │
│     127.0.0.1  api.local                                     │
│     → Desarrollo con virtual hosts locales                   │
└──────────────────────────────────────────────────────────────┘
```

### 0.0.0.0 vs 127.0.0.1 para bloqueo

```
┌──────────────────────────────────────────────────────────────┐
│  127.0.0.1  ads.example.com                                  │
│  → La conexión se establece con localhost                    │
│  → Si hay un servicio en el puerto, lo alcanza               │
│  → Si no hay servicio, "Connection refused" (tras intento)   │
│  → Más lento: el kernel intenta conectar                     │
│                                                              │
│  0.0.0.0  ads.example.com                                    │
│  → La conexión falla inmediatamente                          │
│  → No hay intento de conexión TCP                            │
│  → Más rápido para bloqueo de dominios                       │
│                                                              │
│  Para bloqueo: preferir 0.0.0.0                              │
│  Para desarrollo: usar 127.0.0.1 (conectar a localhost)      │
└──────────────────────────────────────────────────────────────┘
```

---

## /etc/resolv.conf

Configura el cliente DNS (stub resolver): qué servidores usar, qué dominios buscar, y opciones de comportamiento:

```bash
cat /etc/resolv.conf
```

```
# Servidores DNS (máximo 3)
nameserver 192.168.1.1
nameserver 8.8.8.8
nameserver 1.1.1.1

# Dominios de búsqueda
search home.local corp.example.com

# Opciones
options timeout:2 attempts:3 ndots:1 rotate
```

### Directiva nameserver

```
┌──────────────────────────────────────────────────────────────┐
│  nameserver IP                                               │
│                                                              │
│  • Máximo 3 líneas nameserver (las demás se ignoran)         │
│  • El stub resolver usa el PRIMERO por defecto               │
│  • Si el primero no responde (timeout), prueba el segundo    │
│  • NO es balanceo de carga, es failover secuencial           │
│                                                              │
│  nameserver 192.168.1.1     ← primario (tu router)           │
│  nameserver 8.8.8.8         ← secundario (Google DNS)        │
│  nameserver 1.1.1.1         ← terciario (Cloudflare)         │
│                                                              │
│  Con options rotate:                                         │
│  Las consultas rotan entre los nameservers.                  │
│  Consulta 1 → 192.168.1.1                                   │
│  Consulta 2 → 8.8.8.8                                       │
│  Consulta 3 → 1.1.1.1                                       │
│  (balanceo de carga rudimentario)                            │
│                                                              │
│  Solo acepta IPs, NO nombres de dominio.                     │
│  nameserver dns.google.com  ← INCORRECTO (circular)          │
│  nameserver 8.8.8.8         ← CORRECTO                       │
└──────────────────────────────────────────────────────────────┘
```

### Directiva search y domain

```
┌──────────────────────────────────────────────────────────────┐
│  search dom1.com dom2.com dom3.com                           │
│                                                              │
│  Cuando resuelves un nombre "corto" (sin suficientes puntos),│
│  el resolver prueba añadirle cada search domain:             │
│                                                              │
│  search home.local corp.example.com                          │
│                                                              │
│  Resolver "server1":                                         │
│    1. server1.home.local        → ¿existe? NXDOMAIN          │
│    2. server1.corp.example.com  → ¿existe? ¡Sí! Devolver IP │
│                                                              │
│  Resolver "server1.home.local":                              │
│    → Tiene 2 puntos ≥ ndots:1 → probar como FQDN primero   │
│    1. server1.home.local.       → ¡Sí! Devolver IP          │
│                                                              │
│  Máximo 6 search domains (256 caracteres total).             │
│                                                              │
│  "domain" vs "search":                                       │
│  • domain: establece UN solo dominio de búsqueda             │
│  • search: establece VARIOS dominios de búsqueda             │
│  • Si ambos aparecen, el ÚLTIMO gana                         │
│  • En la práctica, usar siempre "search"                     │
└──────────────────────────────────────────────────────────────┘
```

### Directiva options

| Opción | Default | Descripción |
|--------|---------|-------------|
| `ndots:N` | 1 | Si el nombre tiene < N puntos, probar search domains primero |
| `timeout:N` | 5 | Segundos de espera por respuesta (por intento) |
| `attempts:N` | 2 | Número de intentos por nameserver antes de pasar al siguiente |
| `rotate` | off | Rotar entre nameservers (balanceo rudimentario) |
| `single-request` | off | No enviar A y AAAA en paralelo (soluciona firewalls rotos) |
| `single-request-reopen` | off | Como single-request pero reabre el socket |
| `edns0` | on (moderno) | Habilitar EDNS0 (paquetes UDP > 512 bytes) |
| `trust-ad` | off | Confiar en el flag AD (DNSSEC) del resolver |

### ndots explicado

```
┌──────────────────────────────────────────────────────────────┐
│  options ndots:1 (default)                                   │
│                                                              │
│  Nombre con 0 puntos: "server"                               │
│    0 < 1 → probar search domains primero                     │
│    1. server.home.local → NXDOMAIN                           │
│    2. server. (como FQDN) → NXDOMAIN                        │
│                                                              │
│  Nombre con 1 punto: "server.home"                           │
│    1 ≥ 1 → probar como FQDN primero                         │
│    1. server.home. → NXDOMAIN                                │
│    2. server.home.home.local → ...                           │
│                                                              │
│  options ndots:5 (Kubernetes)                                │
│                                                              │
│  Nombre con 2 puntos: "api.example.com"                      │
│    2 < 5 → probar search domains primero (¡3 fallidas!)      │
│    1. api.example.com.default.svc.cluster.local              │
│    2. api.example.com.svc.cluster.local                      │
│    3. api.example.com.cluster.local                          │
│    4. api.example.com. (finalmente como FQDN)                │
│                                                              │
│  Trailing dot (FQDN explícito): "api.example.com."           │
│    → IGNORA ndots y search domains completamente             │
│    → 1 sola consulta directa                                 │
└──────────────────────────────────────────────────────────────┘
```

### ¿Quién gestiona /etc/resolv.conf?

```
┌──────────────────────────────────────────────────────────────┐
│  /etc/resolv.conf puede ser gestionado por:                  │
│                                                              │
│  1. MANUALMENTE (editado a mano)                             │
│     → Sencillo, pero se sobrescribe al reiniciar red         │
│                                                              │
│  2. NetworkManager                                           │
│     → Lo genera automáticamente desde DHCP o config          │
│     → Opciones en nmcli/nmtui                                │
│                                                              │
│  3. systemd-resolved                                         │
│     → /etc/resolv.conf es un symlink a                       │
│       /run/systemd/resolve/stub-resolv.conf                  │
│     → Contiene nameserver 127.0.0.53 (el stub local)         │
│     → La configuración real está en resolved.conf            │
│                                                              │
│  4. DHCP client (dhclient, dhcpcd)                           │
│     → Lo genera desde la respuesta DHCP del router           │
│                                                              │
│  ⚠ Para saber quién lo gestiona:                            │
│  ls -la /etc/resolv.conf                                     │
│  # Si es symlink → probablemente systemd-resolved            │
│  head -1 /etc/resolv.conf                                    │
│  # Suele tener un comentario indicando el generador          │
└──────────────────────────────────────────────────────────────┘
```

```bash
# ¿Quién genera mi resolv.conf?
ls -la /etc/resolv.conf
# lrwxrwxrwx 1 root root 39 Mar 1 /etc/resolv.conf -> ../run/systemd/resolve/stub-resolv.conf
# ↑ Es un symlink de systemd-resolved

head -3 /etc/resolv.conf
# # This is /run/systemd/resolve/stub-resolv.conf managed by man:systemd-resolved(8).
# # Do not edit.
# nameserver 127.0.0.53
```

---

## nsswitch.conf: el orquestador

`/etc/nsswitch.conf` (Name Service Switch) define qué fuentes se consultan y en qué orden para diferentes tipos de información del sistema:

```bash
grep ^hosts /etc/nsswitch.conf
```

```
┌──────────────────────────────────────────────────────────────┐
│  hosts: files dns myhostname                                 │
│         ─────  ───  ──────────                               │
│           │     │       │                                    │
│           │     │       └── nss-myhostname: resuelve el      │
│           │     │           hostname de la máquina local      │
│           │     │                                            │
│           │     └── nss-dns: consulta DNS                    │
│           │         (usa /etc/resolv.conf)                    │
│           │                                                  │
│           └── nss-files: lee /etc/hosts                      │
│                                                              │
│  Orden de búsqueda: de izquierda a derecha.                  │
│  Si una fuente devuelve resultado → se detiene.              │
│  Si devuelve "not found" → pasa a la siguiente.              │
└──────────────────────────────────────────────────────────────┘
```

### Módulos NSS comunes

| Módulo | Fuente | Propósito |
|--------|--------|-----------|
| `files` | /etc/hosts | Resolución local estática |
| `dns` | DNS (via resolv.conf) | Resolución por DNS estándar |
| `myhostname` | hostname del SO | Resolver el propio nombre de la máquina |
| `resolve` | systemd-resolved | Resolver vía resolved (caché, DoT, mDNS) |
| `mymachines` | containers systemd | Resolver nombres de containers/VMs locales |
| `mdns4_minimal` | mDNS (Avahi) | Resolución .local por multicast DNS |
| `wins` | WINS (Samba) | Resolución NetBIOS |

### Configuración con systemd-resolved

```
# Distribuciones con systemd-resolved (Fedora, Ubuntu):
hosts: mymachines resolve [!UNAVAIL=return] files myhostname dns

# Desglose:
# mymachines      → buscar en containers/VMs de systemd-nspawn
# resolve         → usar systemd-resolved
# [!UNAVAIL=return] → si resolved no está corriendo (UNAVAIL),
#                     continuar al siguiente módulo.
#                     Para cualquier otra respuesta, detenerse.
# files           → /etc/hosts (fallback si resolved no corre)
# myhostname      → hostname local
# dns             → DNS directo (fallback final)
```

### Acciones de control NSS

```
┌──────────────────────────────────────────────────────────────┐
│  Entre corchetes se pueden poner acciones condicionales:     │
│                                                              │
│  [!UNAVAIL=return]                                           │
│   │  │        │                                              │
│   │  │        └── acción: return (detenerse)                 │
│   │  └── condición: UNAVAIL (servicio no disponible)         │
│   └── negación: ! (si NO es UNAVAIL)                         │
│                                                              │
│  Traducción: "si el módulo anterior devuelve cualquier       │
│  cosa EXCEPTO 'no disponible', detenerse aquí"               │
│                                                              │
│  Estados posibles:                                           │
│  SUCCESS    → encontrado (devolver resultado)                │
│  NOTFOUND   → no encontrado (probar siguiente módulo)        │
│  UNAVAIL    → servicio no disponible (probar siguiente)      │
│  TRYAGAIN   → error temporal (reintentar o siguiente)        │
│                                                              │
│  Acciones posibles:                                          │
│  return     → devolver al llamador (parar la búsqueda)       │
│  continue   → probar el siguiente módulo                     │
│  merge      → combinar resultados (raro)                     │
└──────────────────────────────────────────────────────────────┘
```

---

## systemd-resolved

La mayoría de distribuciones modernas (Fedora, Ubuntu, Arch) usan `systemd-resolved` como resolver local. Funciona como un **intermediario** entre las aplicaciones y los servidores DNS externos:

```
┌──────────────────────────────────────────────────────────────┐
│             systemd-resolved                                 │
│                                                              │
│  Aplicación                                                  │
│      │                                                       │
│      │ getaddrinfo() → NSS → módulo "resolve"                │
│      ▼                                                       │
│  ┌─────────────────────────────────────┐                     │
│  │  systemd-resolved                   │                     │
│  │  (127.0.0.53:53 / 127.0.0.54:53)   │                     │
│  │                                     │                     │
│  │  • Caché local de DNS               │                     │
│  │  • DNSSEC validation (opcional)     │                     │
│  │  • DNS over TLS (DoT)              │                     │
│  │  • mDNS (.local)                    │                     │
│  │  • LLMNR                            │                     │
│  │  • DNS por interfaz de red          │                     │
│  │  • Search domains por interfaz      │                     │
│  └───────────┬─────────────────────────┘                     │
│              │                                               │
│              ├──► DNS externo (ej. 8.8.8.8)                  │
│              ├──► DNS de la VPN (ej. 10.0.0.1)               │
│              └──► mDNS multicast (.local)                    │
│                                                              │
│  /etc/resolv.conf es un symlink que apunta a:                │
│    /run/systemd/resolve/stub-resolv.conf                     │
│    → nameserver 127.0.0.53                                   │
│                                                              │
│  127.0.0.53 = stub listener (aplica search domains, caché)   │
│  127.0.0.54 = listener directo (no aplica search domains)    │
└──────────────────────────────────────────────────────────────┘
```

### Configuración de resolved

```bash
# Archivo de configuración principal:
cat /etc/systemd/resolved.conf
```

```ini
[Resolve]
# Servidores DNS (fallback si DHCP no proporciona ninguno)
#DNS=8.8.8.8 1.1.1.1
#FallbackDNS=9.9.9.9

# DNS over TLS
#DNSOverTLS=opportunistic
# opportunistic = usar TLS si el servidor lo soporta
# yes = forzar TLS (falla si no soporta)
# no = no usar TLS

# DNSSEC
#DNSSEC=allow-downgrade
# yes = validar siempre (estricto)
# allow-downgrade = validar si hay firmas, aceptar si no hay
# no = no validar

# Dominios de búsqueda
#Domains=~.
# ~. = enrutar TODAS las consultas por este resolver

# mDNS y LLMNR
#MulticastDNS=yes
#LLMNR=yes

# Caché
#Cache=yes
#CacheFromLocalhost=no
```

### Comandos resolvectl

```bash
# Estado completo (interfaces, DNS servers, search domains)
resolvectl status

# Ejemplo de salida:
# Global
#   Protocols: +LLMNR +mDNS -DNSOverTLS DNSSEC=allow-downgrade
#   resolv.conf mode: stub
# Link 2 (enp0s3)
#   Current Scopes: DNS LLMNR/IPv4
#   Protocols: +DefaultRoute +LLMNR
#   DNS Servers: 192.168.1.1
#   DNS Domain: home.local

# Resolver un nombre
resolvectl query www.example.com
# www.example.com: 93.184.216.34
#                  -- link: enp0s3
#                  -- information source: network

# Estadísticas de caché
resolvectl statistics
# Transactions
#   Current: 0
#   Total: 2847
# Cache
#   Current Size: 142
#   Cache Hits: 1847
#   Cache Misses: 523

# Limpiar caché
resolvectl flush-caches

# Ver configuración DNS por interfaz
resolvectl dns
# Global:
# Link 2 (enp0s3): 192.168.1.1

# Cambiar DNS de una interfaz (temporal, hasta reinicio)
resolvectl dns enp0s3 8.8.8.8 1.1.1.1

# Cambiar search domain de una interfaz
resolvectl domain enp0s3 corp.example.com
```

### Los cuatro modos de resolv.conf con resolved

```
┌──────────────────────────────────────────────────────────────┐
│  systemd-resolved puede configurar resolv.conf de 4 formas: │
│                                                              │
│  1. STUB (recomendado):                                      │
│     /etc/resolv.conf → /run/systemd/resolve/stub-resolv.conf │
│     nameserver 127.0.0.53                                    │
│     → Todas las consultas pasan por resolved (caché + DoT)   │
│                                                              │
│  2. STATIC:                                                  │
│     /etc/resolv.conf → /usr/lib/systemd/resolv.conf          │
│     nameserver 127.0.0.53                                    │
│     → Similar a stub pero archivo diferente                  │
│                                                              │
│  3. UPLINK (transparente):                                   │
│     /etc/resolv.conf → /run/systemd/resolve/resolv.conf      │
│     nameserver 192.168.1.1   (DNS real, no 127.0.0.53)       │
│     → Aplicaciones consultan directamente al DNS externo     │
│     → Sin caché local de resolved                            │
│                                                              │
│  4. FOREIGN:                                                 │
│     /etc/resolv.conf es un archivo regular (no symlink)      │
│     → resolved lo lee como input (para saber qué DNS usar)   │
│     → No lo modifica                                         │
│                                                              │
│  Verificar el modo actual:                                   │
│  ls -la /etc/resolv.conf                                     │
│  resolvectl status | grep "resolv.conf mode"                 │
└──────────────────────────────────────────────────────────────┘
```

---

## NetworkManager y resolución DNS

NetworkManager gestiona las conexiones de red y, por lo tanto, influye en la configuración DNS:

```
┌──────────────────────────────────────────────────────────────┐
│           NetworkManager y DNS                               │
│                                                              │
│  DHCP Server                                                 │
│     │ "Tu DNS es 192.168.1.1, domain home.local"             │
│     ▼                                                        │
│  NetworkManager                                              │
│     │                                                        │
│     ├── Si systemd-resolved está activo:                     │
│     │   NM pasa los DNS a resolved via D-Bus                 │
│     │   resolved actualiza su configuración por interfaz     │
│     │                                                        │
│     ├── Si resolved NO está activo:                          │
│     │   NM escribe /etc/resolv.conf directamente             │
│     │   (o a través de un plugin como dnsmasq)               │
│     │                                                        │
│     └── Plugin dns (en NetworkManager.conf):                 │
│         [main]                                               │
│         dns=systemd-resolved   ← pasar a resolved (default)  │
│         dns=default            ← escribir resolv.conf directo│
│         dns=dnsmasq            ← usar dnsmasq local          │
│         dns=none               ← no tocar resolv.conf        │
└──────────────────────────────────────────────────────────────┘
```

### Configurar DNS con nmcli

```bash
# Ver la conexión activa
nmcli connection show

# Ver configuración DNS de una conexión
nmcli connection show "Wired connection 1" | grep -i dns

# Configurar DNS manualmente (sobreescribe DHCP)
nmcli connection modify "Wired connection 1" \
  ipv4.dns "8.8.8.8 1.1.1.1" \
  ipv4.dns-search "corp.example.com"

# Ignorar DNS del DHCP y usar solo los configurados manualmente
nmcli connection modify "Wired connection 1" \
  ipv4.ignore-auto-dns yes

# Aplicar los cambios
nmcli connection up "Wired connection 1"

# Verificar
resolvectl status
# o
cat /etc/resolv.conf
```

### DNS por conexión (VPN)

```
┌──────────────────────────────────────────────────────────────┐
│  Un caso poderoso de systemd-resolved: DNS split             │
│                                                              │
│  Interfaz enp0s3 (LAN):                                     │
│    DNS: 192.168.1.1                                          │
│    Search: home.local                                        │
│                                                              │
│  Interfaz tun0 (VPN corporativa):                            │
│    DNS: 10.0.0.1                                             │
│    Search: corp.example.com                                  │
│                                                              │
│  Resultado:                                                  │
│    "server.corp.example.com" → consulta a 10.0.0.1 (VPN DNS)│
│    "google.com"              → consulta a 192.168.1.1 (LAN) │
│                                                              │
│  resolved enruta cada consulta al DNS correcto               │
│  según el search domain de cada interfaz.                    │
│  Esto se llama "DNS split" o "split-horizon DNS".            │
└──────────────────────────────────────────────────────────────┘
```

---

## Escenarios de configuración

### Servidor sin systemd-resolved (RHEL/CentOS mínimo)

```bash
# Configurar DNS directamente
cat > /etc/resolv.conf << 'EOF'
nameserver 8.8.8.8
nameserver 1.1.1.1
search example.com
options timeout:2 attempts:3
EOF

# Verificar que nsswitch usa dns
grep ^hosts /etc/nsswitch.conf
# hosts: files dns myhostname

# Proteger de sobrescritura (si no hay NM)
chattr +i /etc/resolv.conf    # inmutable
# Deshacer: chattr -i /etc/resolv.conf
```

### Desktop con systemd-resolved (Fedora/Ubuntu)

```bash
# Usar DNS personalizados con resolved
sudo mkdir -p /etc/systemd/resolved.conf.d/

cat << 'EOF' | sudo tee /etc/systemd/resolved.conf.d/custom.conf
[Resolve]
DNS=1.1.1.1 8.8.8.8
FallbackDNS=9.9.9.9
DNSOverTLS=opportunistic
EOF

sudo systemctl restart systemd-resolved

# Verificar
resolvectl status
```

### DNS interno para desarrollo

```bash
# Añadir entradas temporales para desarrollo
sudo tee -a /etc/hosts << 'EOF'
127.0.0.1  api.local
127.0.0.1  app.local
127.0.0.1  db.local
EOF

# Verificar
getent hosts api.local
# 127.0.0.1  api.local
```

---

## Diagnóstico de problemas de resolución

### Flujo de diagnóstico

```
┌──────────────────────────────────────────────────────────────┐
│  Problema: "no puedo resolver www.example.com"               │
│                                                              │
│  Paso 1: ¿Qué dice getent? (resolución del sistema)         │
│  getent hosts www.example.com                                │
│  → Si funciona: el problema es la aplicación, no DNS         │
│  → Si falla: continuar                                       │
│                                                              │
│  Paso 2: ¿Qué dice dig? (solo DNS, ignora /etc/hosts)       │
│  dig www.example.com                                         │
│  → Si funciona: el problema es /etc/hosts o nsswitch         │
│  → Si falla: continuar                                       │
│                                                              │
│  Paso 3: ¿El resolver es alcanzable?                         │
│  dig @8.8.8.8 www.example.com                                │
│  → Si funciona: el problema es tu resolver local             │
│  → Si falla: problema de red o firewall                      │
│                                                              │
│  Paso 4: ¿Es un problema de red?                             │
│  ping 8.8.8.8                                                │
│  → Si falla: no hay conectividad IP                          │
│  → Si funciona: ¿firewall bloquea 53/udp o 53/tcp?           │
│                                                              │
│  Paso 5: Verificar la cadena de resolución                   │
│  dig +trace www.example.com                                  │
│  → Muestra cada paso: root → TLD → auth                     │
│  → Identifica dónde se rompe la cadena                       │
└──────────────────────────────────────────────────────────────┘
```

### Comandos de diagnóstico

```bash
# 1. Resolución del sistema (respeta nsswitch + /etc/hosts)
getent hosts www.example.com

# 2. Consulta DNS directa (ignora /etc/hosts)
dig www.example.com
dig +short www.example.com

# 3. Consultar un resolver específico
dig @8.8.8.8 www.example.com
dig @1.1.1.1 www.example.com

# 4. Trazar la resolución completa
dig +trace www.example.com

# 5. ¿Quién gestiona mi DNS?
ls -la /etc/resolv.conf
cat /etc/resolv.conf
resolvectl status 2>/dev/null

# 6. ¿Qué dice nsswitch?
grep ^hosts /etc/nsswitch.conf

# 7. ¿Hay algo en /etc/hosts que interfiera?
grep example.com /etc/hosts

# 8. Estado de systemd-resolved
resolvectl statistics
systemctl status systemd-resolved

# 9. ¿Puedo llegar al DNS por UDP y TCP?
dig @8.8.8.8 example.com          # UDP
dig @8.8.8.8 example.com +tcp     # TCP
```

### dig +trace

```bash
dig +trace www.example.com
```

```
┌──────────────────────────────────────────────────────────────┐
│  Salida de dig +trace:                                       │
│                                                              │
│  .                   518400  IN  NS  a.root-servers.net.     │
│  .                   518400  IN  NS  b.root-servers.net.     │
│  ...                                                         │
│  ;; Received 525 bytes from 192.168.1.1 in 12 ms             │
│                                                              │
│  com.                172800  IN  NS  a.gtld-servers.net.     │
│  com.                172800  IN  NS  b.gtld-servers.net.     │
│  ...                                                         │
│  ;; Received 1170 bytes from 198.41.0.4 (a.root) in 25 ms   │
│                                                              │
│  example.com.        172800  IN  NS  ns1.example.com.        │
│  example.com.        172800  IN  NS  ns2.example.com.        │
│  ...                                                         │
│  ;; Received 772 bytes from 192.5.6.30 (.com TLD) in 30 ms  │
│                                                              │
│  www.example.com.    86400   IN  A   93.184.216.34            │
│  ;; Received 56 bytes from 93.184.216.1 (ns1.example) in 40ms│
│                                                              │
│  Cada bloque muestra:                                        │
│  • Qué registros devolvió cada servidor                      │
│  • Desde qué IP y en cuánto tiempo                           │
│  • Si un paso falla, identifica dónde se rompe               │
└──────────────────────────────────────────────────────────────┘
```

---

## Errores comunes

### 1. Editar /etc/resolv.conf cuando lo gestiona otro servicio

```bash
# INCORRECTO: editar directamente cuando es un symlink
sudo nano /etc/resolv.conf    # el cambio se sobrescribirá

# Verificar primero:
ls -la /etc/resolv.conf
# Si es symlink a systemd-resolved:
#   → Configurar en /etc/systemd/resolved.conf.d/
# Si lo gestiona NetworkManager:
#   → Configurar con nmcli

# Si REALMENTE necesitas un resolv.conf manual:
# Opción A: quitar el symlink y crear archivo manual
sudo rm /etc/resolv.conf
sudo tee /etc/resolv.conf << 'EOF'
nameserver 8.8.8.8
EOF

# Opción B: decirle a NM que no toque resolv.conf
# /etc/NetworkManager/NetworkManager.conf
# [main]
# dns=none
```

### 2. Poner más de 3 nameservers

```
# INCORRECTO: glibc ignora del 4º en adelante
nameserver 8.8.8.8
nameserver 1.1.1.1
nameserver 9.9.9.9
nameserver 208.67.222.222    ← IGNORADO
nameserver 208.67.220.220    ← IGNORADO

# Máximo 3 nameservers. Elegir los 3 más importantes.
```

### 3. Confundir dig con getent para diagnóstico

```bash
# Un administrador añade esto a /etc/hosts:
# 10.0.0.5  internal-api.example.com

# Después prueba:
dig internal-api.example.com +short
# → NXDOMAIN (dig NO consulta /etc/hosts)

# "¡No funciona!" — pero la aplicación sí resuelve porque:
getent hosts internal-api.example.com
# 10.0.0.5  internal-api.example.com  ← funciona vía /etc/hosts

# Regla: dig = solo DNS; getent hosts = como el sistema resuelve
```

### 4. No verificar nsswitch.conf después de cambiar el método de resolución

```bash
# Instalas systemd-resolved pero nsswitch sigue con:
# hosts: files dns myhostname
#              ───
#              └── usa glibc DNS (ignora resolved)

# Debería ser:
# hosts: mymachines resolve [!UNAVAIL=return] files myhostname dns
#                   ───────
#                   └── usa systemd-resolved

# Sin cambiar nsswitch.conf, resolved corre pero nadie le pregunta
```

### 5. Olvidar recargar la conexión tras cambiar DNS con nmcli

```bash
# INCORRECTO: modificar y no aplicar
nmcli connection modify "eth0" ipv4.dns "8.8.8.8"
# → El cambio está guardado pero NO aplicado

# CORRECTO: aplicar después
nmcli connection modify "eth0" ipv4.dns "8.8.8.8"
nmcli connection up "eth0"    # ← aplica los cambios
```

---

## Cheatsheet

```
┌──────────────────────────────────────────────────────────────┐
│          /etc/resolv.conf y /etc/hosts Cheatsheet            │
├──────────────────────────────────────────────────────────────┤
│                                                              │
│  ARCHIVOS CLAVE:                                             │
│    /etc/hosts          tabla local nombre→IP (sin DNS)       │
│    /etc/resolv.conf    config del stub resolver (nameservers)│
│    /etc/nsswitch.conf  orden de fuentes (files, dns, resolve)│
│    /etc/hostname       nombre de la máquina                  │
│                                                              │
│  /etc/hosts:                                                 │
│    IP   FQDN   alias1   alias2                               │
│    127.0.0.1  localhost                                      │
│    192.168.1.10  server.internal  server                     │
│    0.0.0.0  ads.example.com   ← bloqueo (más rápido que     │
│                                  127.0.0.1)                  │
│    Sin wildcards, sin TTL, solo A/AAAA equivalente           │
│    Primera coincidencia gana                                 │
│                                                              │
│  /etc/resolv.conf:                                           │
│    nameserver IP        resolver DNS (máx 3)                 │
│    search dom1 dom2     search domains (máx 6)               │
│    options ndots:N      puntos mínimos para FQDN             │
│    options timeout:N    segundos por intento                  │
│    options attempts:N   intentos por nameserver               │
│    options rotate       rotar entre nameservers              │
│    127.0.0.53 = systemd-resolved stub                        │
│                                                              │
│  nsswitch.conf (hosts:):                                     │
│    files         → /etc/hosts                                │
│    dns           → DNS vía resolv.conf (glibc)               │
│    resolve       → systemd-resolved                          │
│    myhostname    → hostname local                            │
│    mymachines    → containers systemd                        │
│    mdns4_minimal → mDNS (.local)                             │
│                                                              │
│  systemd-resolved:                                           │
│    resolvectl status         estado completo                 │
│    resolvectl query NAME     resolver un nombre              │
│    resolvectl statistics     caché hits/misses               │
│    resolvectl flush-caches   limpiar caché                   │
│    resolvectl dns IF IP...   cambiar DNS de interfaz         │
│    Config: /etc/systemd/resolved.conf                        │
│    Drop-in: /etc/systemd/resolved.conf.d/*.conf              │
│    DNSOverTLS=opportunistic  TLS oportunístico               │
│                                                              │
│  NetworkManager:                                             │
│    nmcli con show              listar conexiones             │
│    nmcli con mod "X" ipv4.dns "8.8.8.8"   configurar DNS    │
│    nmcli con mod "X" ipv4.ignore-auto-dns yes  ignorar DHCP │
│    nmcli con up "X"            aplicar cambios               │
│                                                              │
│  DIAGNÓSTICO:                                                │
│    getent hosts NAME    como el SO resuelve (hosts + DNS)    │
│    dig NAME             solo DNS (ignora /etc/hosts)         │
│    dig @IP NAME         consultar DNS específico             │
│    dig +trace NAME      trazar resolución completa           │
│    ls -la /etc/resolv.conf   ¿quién lo gestiona?             │
│    grep ^hosts /etc/nsswitch.conf   orden de fuentes         │
│                                                              │
│  REGLAS:                                                     │
│    dig ≠ getent (dig ignora /etc/hosts)                      │
│    Trailing dot = FQDN (ignora search domains)               │
│    No editar resolv.conf si es symlink                       │
│    Máximo 3 nameservers (el 4º se ignora)                    │
│    nmcli modify + nmcli up (modificar Y aplicar)             │
│                                                              │
└──────────────────────────────────────────────────────────────┘
```

---

## Ejercicios

### Ejercicio 1: Trazar la resolución del sistema

Tienes esta configuración en un servidor:

```
# /etc/nsswitch.conf
hosts: files dns myhostname

# /etc/hosts
127.0.0.1       localhost
192.168.1.100   internal-api    api
10.0.0.5        db.prod.local   db

# /etc/resolv.conf
nameserver 192.168.1.1
nameserver 8.8.8.8
search prod.local corp.example.com
options ndots:1
```

**Para cada nombre, traza cómo se resuelve** (indicando qué archivo/fuente consulta, qué search domains prueba, y el resultado final):

1. `localhost`
2. `db`
3. `api`
4. `www.google.com`
5. `server1` (no existe en /etc/hosts ni en DNS)
6. `redis.prod.local`
7. `internal-api`

Después responde:
- ¿`getent hosts db` y `dig db` devuelven lo mismo? ¿Por qué?
- Si cambias el orden a `hosts: dns files myhostname`, ¿qué cambia para `internal-api`?

**Pregunta de reflexión**: ¿Por qué el orden en nsswitch.conf importa tanto? ¿Qué problema de seguridad podría surgir si `dns` va antes de `files` y un atacante controla tu servidor DNS?

---

### Ejercicio 2: Diagnosticar un problema de DNS

Un desarrollador reporta: "No puedo conectar a `api.staging.myapp.com` desde mi servidor Fedora, pero funciona desde mi laptop."

Ejecutas estos comandos en el servidor:

```bash
$ getent hosts api.staging.myapp.com
(sin salida — falla)

$ dig api.staging.myapp.com +short
203.0.113.50

$ dig @8.8.8.8 api.staging.myapp.com +short
203.0.113.50

$ cat /etc/resolv.conf
# This is /run/systemd/resolve/stub-resolv.conf
nameserver 127.0.0.53

$ resolvectl query api.staging.myapp.com
api.staging.myapp.com: resolve call failed: DNSSEC validation failed: no-signature

$ grep ^hosts /etc/nsswitch.conf
hosts: mymachines resolve [!UNAVAIL=return] files myhostname dns
```

**Responde**:
1. ¿Por qué `dig` funciona pero `getent` falla?
2. ¿Cuál es la causa raíz del problema?
3. ¿Cómo lo solucionarías (temporal e idealmente)?
4. ¿Por qué desde la laptop funciona? (Pista: la laptop probablemente no tiene resolved con DNSSEC estricto.)

**Pregunta de reflexión**: ¿DNSSEC debería configurarse en modo `yes` (estricto) o `allow-downgrade` en un servidor de producción? ¿Cuáles son los riesgos de cada opción?

---

### Ejercicio 3: Configurar DNS para múltiples entornos

Eres sysadmin y necesitas configurar la resolución DNS de un servidor Fedora que tiene:

- Conexión LAN (enp0s3) con DNS del router: 192.168.1.1
- VPN corporativa (tun0) con DNS interno: 10.0.0.1
- Los dominios internos `*.corp.example.com` deben resolverse vía VPN
- Todo lo demás debe ir por el DNS público

**Tareas**:

1. Escribe los comandos `resolvectl` para configurar DNS split:
   - enp0s3: DNS 192.168.1.1, search domain general
   - tun0: DNS 10.0.0.1, search domain `corp.example.com`

2. Explica qué pasa cuando resuelves:
   - `server.corp.example.com` — ¿qué DNS se consulta?
   - `www.google.com` — ¿qué DNS se consulta?
   - `printer.local` — ¿cómo se resuelve? (pista: mDNS)

3. Para hacerlo persistente con NetworkManager, escribe los comandos `nmcli`.

4. Añade `db.internal` (192.168.1.50) a /etc/hosts para desarrollo local. Verifica que funciona con `getent` y explica por qué `dig` no lo encontrará.

**Pregunta de reflexión**: ¿Por qué DNS split es tan importante cuando se usa una VPN? ¿Qué problema de privacidad tendrías si TODAS las consultas DNS fueran por la VPN corporativa?

---

> **Siguiente tema**: C02/S02/T01 — dig y nslookup (consultas DNS, interpretar la salida)
