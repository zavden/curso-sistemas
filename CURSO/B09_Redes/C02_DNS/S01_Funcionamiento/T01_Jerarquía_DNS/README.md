# Jerarquía DNS

## Índice
1. [Qué es DNS](#qué-es-dns)
2. [La estructura jerárquica](#la-estructura-jerárquica)
3. [Root servers](#root-servers)
4. [Top-Level Domains (TLDs)](#top-level-domains-tlds)
5. [Dominios de segundo nivel y subdominios](#dominios-de-segundo-nivel-y-subdominios)
6. [FQDN](#fqdn)
7. [Zonas y delegación](#zonas-y-delegación)
8. [Servidores autoritativos vs recursivos](#servidores-autoritativos-vs-recursivos)
9. [El proceso de resolución completo](#el-proceso-de-resolución-completo)
10. [DNS en Linux: primera mirada](#dns-en-linux-primera-mirada)
11. [Errores comunes](#errores-comunes)
12. [Cheatsheet](#cheatsheet)
13. [Ejercicios](#ejercicios)

---

## Qué es DNS

DNS (Domain Name System) es el sistema que traduce nombres de dominio legibles por humanos a direcciones IP usadas por las máquinas:

```
www.example.com  →  93.184.216.34      (IPv4)
www.example.com  →  2606:2800:220:1::  (IPv6)
```

Sin DNS, tendrías que recordar direcciones IP para cada sitio web. DNS es frecuentemente descrito como la **"guía telefónica de Internet"**, pero su estructura es mucho más sofisticada — es una base de datos distribuida y jerárquica que opera a escala global.

```
┌──────────────────────────────────────────────────────────────┐
│                   ¿Por qué DNS?                              │
│                                                              │
│  Sin DNS:                                                    │
│    curl http://93.184.216.34                                 │
│    curl http://142.250.185.206                               │
│    curl http://151.101.1.140                                 │
│    ¿Cuál es cuál? Imposible de recordar.                     │
│                                                              │
│  Con DNS:                                                    │
│    curl http://example.com                                   │
│    curl http://google.com                                    │
│    curl http://reddit.com                                    │
│                                                              │
│  DNS también permite:                                        │
│  • Mover un servicio a otra IP sin que los usuarios noten    │
│  • Balanceo de carga (un nombre → múltiples IPs)             │
│  • Redundancia (servidores de respaldo)                      │
│  • Email routing (registros MX)                              │
│  • Verificación de propiedad (registros TXT)                 │
│  • Descubrimiento de servicios (registros SRV)               │
└──────────────────────────────────────────────────────────────┘
```

### Características fundamentales

- **Distribuido**: no hay un solo servidor con todos los datos. Millones de servidores DNS cooperan.
- **Jerárquico**: la autoridad se delega nivel por nivel (raíz → TLD → dominio → subdominio).
- **Cacheado**: las respuestas se almacenan temporalmente en múltiples niveles para reducir la carga.
- **Puerto 53**: UDP para consultas normales (≤512 bytes, o ≤4096 con EDNS), TCP para transferencias de zona y respuestas grandes.

---

## La estructura jerárquica

DNS se organiza como un árbol invertido. Cada nodo del árbol representa un nivel del nombre de dominio:

```
┌──────────────────────────────────────────────────────────────┐
│                    Árbol DNS                                 │
│                                                              │
│                        . (root)                              │
│                       / | \                                  │
│                      /  |  \                                 │
│                   com  org  net  uk  es  io  dev  ...        │
│                  / | \          |                             │
│                 /  |  \         |                             │
│            google amazon example  gov                        │
│             / \     |         |    |                          │
│           www maps  www     www  dwp                         │
│            |               |                                 │
│           mail            www                                │
│                                                              │
│  Lectura de derecha a izquierda:                             │
│  www.example.com.                                            │
│   │    │      │  │                                           │
│   │    │      │  └── root (el punto final, usualmente omitido)│
│   │    │      └── TLD (Top-Level Domain)                     │
│   │    └── Dominio de segundo nivel (SLD)                    │
│   └── Subdominio (tercer nivel)                              │
└──────────────────────────────────────────────────────────────┘
```

### Lectura de un nombre de dominio

Los nombres DNS se leen de **derecha a izquierda** (de lo general a lo específico), aunque los escribimos de izquierda a derecha:

```
    mail.support.example.com.
    ────  ───────  ───────  ───  ─
     │       │        │      │   │
     │       │        │      │   └── Root (nivel 0)
     │       │        │      └── TLD (nivel 1)
     │       │        └── SLD (nivel 2) — lo que "compras"
     │       └── Subdominio (nivel 3)
     └── Subdominio (nivel 4)
```

Cada nivel está separado por un punto (`.`). El nombre completo puede tener hasta **253 caracteres** y cada etiqueta (entre puntos) hasta **63 caracteres**.

---

## Root servers

Los root servers son el punto de partida de toda resolución DNS. No conocen la IP de `www.example.com`, pero saben dónde están los servidores de `.com`, `.org`, `.net`, etc.

```
┌──────────────────────────────────────────────────────────────┐
│                    Root Servers                               │
│                                                              │
│  Existen 13 identidades de root servers (A-M):               │
│                                                              │
│  Letra │ Operador                 │ Ubicaciones              │
│  ──────┼──────────────────────────┼──────────────────────    │
│    A   │ Verisign                 │   Múltiples              │
│    B   │ USC-ISI                  │   Los Ángeles            │
│    C   │ Cogent                   │   Múltiples              │
│    D   │ University of Maryland   │   College Park           │
│    E   │ NASA                     │   Mountain View          │
│    F   │ ISC                      │   Múltiples              │
│    G   │ US DoD                   │   Múltiples              │
│    H   │ US Army                  │   Aberdeen               │
│    I   │ Netnod                   │   Múltiples              │
│    J   │ Verisign                 │   Múltiples              │
│    K   │ RIPE NCC                 │   Múltiples              │
│    L   │ ICANN                    │   Múltiples              │
│    M   │ WIDE Project             │   Múltiples              │
│                                                              │
│  13 identidades ≠ 13 servidores físicos                      │
│  En realidad hay ~1700+ instancias distribuidas              │
│  globalmente usando ANYCAST.                                 │
└──────────────────────────────────────────────────────────────┘
```

### Anycast

```
┌──────────────────────────────────────────────────────────────┐
│  Anycast: múltiples servidores comparten la MISMA IP.        │
│  La red enruta al más cercano geográficamente.               │
│                                                              │
│  f.root-servers.net = 192.5.5.241                            │
│                                                              │
│  Desde Madrid  → se enruta a la instancia en Madrid          │
│  Desde Tokio   → se enruta a la instancia en Tokio           │
│  Desde São Paulo → se enruta a la instancia en São Paulo     │
│                                                              │
│  Resultado:                                                  │
│  • Baja latencia (servidor cercano)                          │
│  • Alta disponibilidad (si uno cae, otro responde)           │
│  • Resistencia a DDoS (el tráfico se distribuye)             │
└──────────────────────────────────────────────────────────────┘
```

### ¿Por qué solo 13 identidades?

Limitación histórica: la respuesta DNS que lista los root servers debe caber en un paquete UDP de 512 bytes (límite original del protocolo). Con 13 nombres y sus direcciones IPv4, se alcanza ese límite. Anycast permite escalar más allá de esta restricción.

---

## Top-Level Domains (TLDs)

Los TLDs son el primer nivel bajo la raíz. Son administrados por organizaciones designadas por ICANN:

### Tipos de TLD

```
┌──────────────────────────────────────────────────────────────┐
│                    Tipos de TLD                              │
│                                                              │
│  gTLD (Generic Top-Level Domains):                           │
│  ────────────────────────────────                            │
│  Originales:                                                 │
│    .com  (comercial, el más usado)                           │
│    .org  (organizaciones)                                    │
│    .net  (infraestructura de red, ahora genérico)            │
│    .edu  (educación, restringido a EE.UU.)                   │
│    .gov  (gobierno de EE.UU.)                                │
│    .mil  (militar de EE.UU.)                                 │
│                                                              │
│  Nuevos gTLDs (desde 2012):                                  │
│    .dev  .app  .io  .cloud  .xyz  .tech  .blog ...           │
│    Más de 1200 nuevos gTLDs creados                          │
│                                                              │
│  ccTLD (Country Code Top-Level Domains):                     │
│  ──────────────────────────────────────                      │
│    .es  (España)        .mx  (México)                        │
│    .uk  (Reino Unido)   .de  (Alemania)                      │
│    .ar  (Argentina)     .co  (Colombia)                      │
│    .jp  (Japón)         .br  (Brasil)                        │
│    Basados en ISO 3166-1 (códigos de país de 2 letras)       │
│                                                              │
│  Infraestructura:                                            │
│  ───────────────                                             │
│    .arpa  (Address and Routing Parameter Area)               │
│    Usado para resolución inversa (IP → nombre)               │
│    Ejemplo: 34.216.184.93.in-addr.arpa → example.com         │
│                                                              │
│  IDN TLDs (Internationalized):                               │
│  ────────────────────────────                                │
│    .中国 (China)  .рф (Rusia)  .مصر (Egipto)                  │
│    Dominios en scripts no latinos                            │
└──────────────────────────────────────────────────────────────┘
```

### Operadores de TLD (registries)

Cada TLD tiene un **registry** (operador) responsable de mantener la base de datos de dominios registrados bajo ese TLD:

| TLD | Registry | Nota |
|-----|----------|------|
| .com, .net | Verisign | El registry más grande del mundo |
| .org | Public Interest Registry (PIR) | |
| .es | Red.es | Administración española |
| .dev, .app | Google (Charleston Road Registry) | Requieren HTTPS (HSTS preloaded) |
| .io | NIC.IO | Territorio Británico del Océano Índico |

Los **registrars** (ej. GoDaddy, Namecheap, Cloudflare) son intermediarios entre los usuarios y los registries. Compras tu dominio a un registrar, que lo registra en el registry correspondiente.

---

## Dominios de segundo nivel y subdominios

### Segundo nivel (SLD)

El dominio de segundo nivel es lo que "compras" a un registrar:

```
example.com
───────
   │
   └── SLD: "example" bajo el TLD "com"

# Esto te da control sobre:
#   example.com          (el dominio base / apex)
#   *.example.com        (cualquier subdominio)
```

### Subdominios

Los subdominios son niveles adicionales que el propietario del dominio crea libremente:

```
┌──────────────────────────────────────────────────────────────┐
│  example.com                   ← dominio apex / bare domain │
│  www.example.com               ← subdominio "www"           │
│  mail.example.com              ← subdominio "mail"          │
│  api.example.com               ← subdominio "api"           │
│  staging.api.example.com       ← sub-subdominio             │
│  us-east.cdn.example.com       ← sub-sub-subdominio         │
│                                                              │
│  Cada subdominio puede apuntar a una IP diferente:           │
│  www.example.com    →  93.184.216.34   (web server)          │
│  api.example.com    →  10.0.1.50       (API server)          │
│  mail.example.com   →  10.0.2.10       (mail server)         │
│                                                              │
│  No hay límite práctico de niveles de subdominios,           │
│  mientras el nombre total no exceda 253 caracteres.          │
└──────────────────────────────────────────────────────────────┘
```

### Dominio apex vs www

```
┌──────────────────────────────────────────────────────────────┐
│  example.com      ← "apex" / "bare" / "naked" domain        │
│  www.example.com  ← subdominio                               │
│                                                              │
│  Diferencia técnica importante:                              │
│  • El apex (example.com) no puede tener un registro CNAME    │
│    según la RFC (conflicto con SOA y NS).                    │
│  • www.example.com SÍ puede tener CNAME.                    │
│                                                              │
│  Implicación práctica:                                       │
│  Si usas un CDN (Cloudflare, AWS CloudFront) que te da       │
│  un nombre como d1234.cloudfront.net:                        │
│    www.example.com  → CNAME d1234.cloudfront.net ← funciona  │
│    example.com      → CNAME d1234.cloudfront.net ← PROHIBIDO │
│                                                              │
│  Soluciones para el apex:                                    │
│  • ALIAS record (no estándar, soportado por Route53, etc.)   │
│  • ANAME record (propuesta)                                  │
│  • Cloudflare CNAME flattening                               │
│  • Usar la IP directamente (pierde beneficios del CDN)       │
└──────────────────────────────────────────────────────────────┘
```

---

## FQDN

Un FQDN (Fully Qualified Domain Name) es un nombre de dominio completo que especifica su posición exacta en la jerarquía DNS, terminando en un punto (`.`) que representa la raíz:

```
┌──────────────────────────────────────────────────────────────┐
│  FQDN vs nombre parcial                                      │
│                                                              │
│  FQDN (termina en punto):                                    │
│    www.example.com.                                          │
│                   ↑                                          │
│                   trailing dot = "desde la raíz"             │
│                                                              │
│  Nombre parcial (sin punto final):                           │
│    www.example.com                                           │
│    → Podría ser relativo a un dominio de búsqueda            │
│    → El resolver puede intentar:                             │
│      www.example.com.miempresa.local                         │
│      www.example.com.                                        │
│                                                              │
│  En la práctica, los navegadores y la mayoría de             │
│  aplicaciones asumen el punto final. Pero en configuración   │
│  DNS (zona files, dig), el trailing dot es significativo.    │
└──────────────────────────────────────────────────────────────┘
```

### Search domains

El archivo `/etc/resolv.conf` puede definir dominios de búsqueda que se añaden automáticamente a nombres parciales:

```bash
# /etc/resolv.conf
nameserver 192.168.1.1
search miempresa.local dev.miempresa.local

# Ahora, al resolver "server1":
# 1. Intenta: server1.miempresa.local
# 2. Intenta: server1.dev.miempresa.local
# 3. Intenta: server1.  (como FQDN)

# Esto permite:
ssh server1     # en vez de ssh server1.miempresa.local
```

Un FQDN (con trailing dot) **nunca** pasa por search domains — se resuelve directamente como está escrito.

```bash
# Diferencia en dig:
dig www.example.com     # puede añadir search domain
dig www.example.com.    # FQDN, resolución directa (sin search domain)
```

---

## Zonas y delegación

Una **zona** es la porción del espacio de nombres DNS que un servidor autoritativo administra. No es lo mismo que un dominio:

```
┌──────────────────────────────────────────────────────────────┐
│                 Dominio vs Zona                              │
│                                                              │
│  Dominio example.com incluye todo:                           │
│    example.com                                               │
│    www.example.com                                           │
│    mail.example.com                                          │
│    api.example.com                                           │
│    dev.api.example.com                                       │
│    staging.api.example.com                                   │
│                                                              │
│  Pero si delegas api.example.com a otro servidor DNS:        │
│                                                              │
│  Zona 1 (servidor DNS A):          Zona 2 (servidor DNS B): │
│    example.com                       api.example.com         │
│    www.example.com                   dev.api.example.com     │
│    mail.example.com                  staging.api.example.com │
│                                                              │
│  El dominio example.com tiene 2 zonas.                       │
│  Zona 1 contiene un registro NS que delega api.example.com   │
│  a los servidores de Zona 2.                                 │
└──────────────────────────────────────────────────────────────┘
```

### Delegación

La delegación es el mecanismo por el cual un servidor DNS transfiere la responsabilidad de un subdominio a otro servidor:

```
┌──────────────────────────────────────────────────────────────┐
│                  Cadena de delegación                        │
│                                                              │
│  Root servers:                                               │
│    "¿quién maneja .com?"                                     │
│    → Delega a servidores de Verisign (a.gtld-servers.net)    │
│                                                              │
│  Servidores de .com (Verisign):                              │
│    "¿quién maneja example.com?"                              │
│    → Delega a ns1.example.com (registrado por el propietario)│
│                                                              │
│  Servidores de example.com:                                  │
│    "¿cuál es la IP de www.example.com?"                      │
│    → 93.184.216.34 (respuesta autoritativa)                  │
│                                                              │
│  Cada nivel SOLO conoce el siguiente nivel.                  │
│  Root no sabe la IP de www.example.com.                      │
│  .com no sabe la IP de www.example.com.                      │
│  Solo el servidor autoritativo de example.com lo sabe.       │
└──────────────────────────────────────────────────────────────┘
```

### Registros de delegación (glue records)

Hay un problema circular: para resolver `ns1.example.com` necesitas consultar al servidor de `example.com`, pero ese servidor ES `ns1.example.com`. Los **glue records** resuelven esto:

```
┌──────────────────────────────────────────────────────────────┐
│  Problema circular:                                          │
│                                                              │
│  example.com  NS  ns1.example.com                            │
│  → Para conectar a ns1.example.com necesito su IP            │
│  → Para obtener la IP de ns1.example.com necesito preguntar  │
│    al servidor DNS de example.com                            │
│  → El servidor DNS de example.com ES ns1.example.com         │
│  → ¿¿??                                                     │
│                                                              │
│  Solución: glue records                                      │
│  El servidor de .com incluye la IP directamente:             │
│                                                              │
│  example.com    NS    ns1.example.com                        │
│  ns1.example.com  A   93.184.216.1      ← glue record       │
│                                                              │
│  Los glue records están en la zona PADRE (.com),             │
│  no en la zona del dominio (example.com).                    │
│  Solo se necesitan cuando el nameserver está DENTRO           │
│  del dominio que sirve.                                      │
└──────────────────────────────────────────────────────────────┘
```

---

## Servidores autoritativos vs recursivos

En DNS hay dos tipos fundamentales de servidores con roles muy diferentes:

```
┌──────────────────────────────────────────────────────────────┐
│                                                              │
│  SERVIDOR RECURSIVO (resolver)                               │
│  ─────────────────────────────                               │
│  • Recibe consultas de los CLIENTES                          │
│  • No tiene datos propios — los busca                        │
│  • Recorre el árbol DNS (root → TLD → autoritativo)          │
│  • CACHEA las respuestas para futuras consultas              │
│  • Ejemplos: 8.8.8.8 (Google), 1.1.1.1 (Cloudflare),        │
│    el DNS de tu ISP, systemd-resolved en tu máquina          │
│                                                              │
│  SERVIDOR AUTORITATIVO                                       │
│  ─────────────────────                                       │
│  • Tiene los datos ORIGINALES de una zona                    │
│  • No busca en otros servidores — responde directamente      │
│  • Configurado por el propietario del dominio                │
│  • Su respuesta incluye el flag "aa" (authoritative answer)  │
│  • Ejemplos: ns1.example.com, ns1.google.com,                │
│    servidores de Cloudflare DNS, Route53                     │
│                                                              │
└──────────────────────────────────────────────────────────────┘

┌──────────────────────────────────────────────────────────────┐
│              Flujo completo                                  │
│                                                              │
│  Tu laptop                                                   │
│     │                                                        │
│     │ "¿cuál es la IP de www.example.com?"                   │
│     ▼                                                        │
│  Stub resolver (/etc/resolv.conf)                            │
│     │                                                        │
│     │ Envía consulta a resolver recursivo                    │
│     ▼                                                        │
│  Resolver recursivo (ej. 8.8.8.8)                            │
│     │                                                        │
│     ├──► Root server: "¿.com?"                               │
│     │    → "Pregunta a a.gtld-servers.net"                   │
│     │                                                        │
│     ├──► TLD server (.com): "¿example.com?"                  │
│     │    → "Pregunta a ns1.example.com (93.184.216.1)"       │
│     │                                                        │
│     ├──► Autoritativo (ns1.example.com):                     │
│     │    "¿www.example.com?"                                 │
│     │    → "93.184.216.34" (respuesta autoritativa, aa=1)    │
│     │                                                        │
│     ▼                                                        │
│  Cachea la respuesta (según TTL)                             │
│     │                                                        │
│     ▼                                                        │
│  Devuelve 93.184.216.34 a tu laptop                          │
└──────────────────────────────────────────────────────────────┘
```

### Resolución iterativa vs recursiva

```
┌──────────────────────────────────────────────────────────────┐
│                                                              │
│  RECURSIVA: el resolver hace TODO el trabajo                 │
│  ──────────                                                  │
│  Cliente → Resolver: "dime la IP de www.example.com"         │
│  Resolver consulta root, TLD, autoritativo                   │
│  Resolver → Cliente: "93.184.216.34"                         │
│                                                              │
│  (Tu laptop pide de forma recursiva a 8.8.8.8)               │
│                                                              │
│  ITERATIVA: cada servidor da una referencia al siguiente     │
│  ──────────                                                  │
│  Resolver → Root: "¿www.example.com?"                        │
│  Root → Resolver: "No sé, pero .com está en a.gtld-servers"  │
│  Resolver → TLD: "¿www.example.com?"                         │
│  TLD → Resolver: "No sé, pero example.com está en ns1..."   │
│  Resolver → Autoritativo: "¿www.example.com?"                │
│  Autoritativo → Resolver: "93.184.216.34"                    │
│                                                              │
│  (8.8.8.8 consulta root/TLD de forma iterativa)              │
│                                                              │
│  En la práctica:                                             │
│  • Cliente → Resolver recursivo: consulta RECURSIVA          │
│  • Resolver → servidores DNS: consultas ITERATIVAS           │
└──────────────────────────────────────────────────────────────┘
```

---

## El proceso de resolución completo

Veamos paso a paso qué ocurre cuando escribes `https://www.example.com` en tu navegador:

```
┌──────────────────────────────────────────────────────────────┐
│  Paso 1: Caché del navegador                                 │
│  ¿He resuelto www.example.com recientemente?                 │
│  → Sí: usar IP cacheada (no consulta DNS)                    │
│  → No: siguiente paso                                        │
│                                                              │
│  Paso 2: Caché del sistema operativo                         │
│  ¿El SO tiene la respuesta en caché?                         │
│  (systemd-resolved, nscd, etc.)                              │
│  → Sí: devolver al navegador                                 │
│  → No: siguiente paso                                        │
│                                                              │
│  Paso 3: /etc/hosts                                          │
│  ¿Hay una entrada manual para www.example.com?               │
│  (depende de nsswitch.conf — normalmente hosts antes de DNS) │
│  → Sí: devolver esa IP                                       │
│  → No: siguiente paso                                        │
│                                                              │
│  Paso 4: Resolver recursivo (ej. 8.8.8.8)                    │
│  ¿Tiene la respuesta en caché?                               │
│  → Sí: devolver (con TTL reducido)                           │
│  → No: iniciar resolución iterativa                          │
│                                                              │
│  Paso 5: Root server                                         │
│  El resolver pregunta a un root server.                      │
│  Root responde: "No sé, pero .com está en estos servidores"  │
│  (referral con NS records + glue records)                    │
│                                                              │
│  Paso 6: TLD server (.com)                                   │
│  El resolver pregunta al servidor de .com.                   │
│  .com responde: "No sé, pero example.com está en ns1..."     │
│  (referral con NS records + glue records)                    │
│                                                              │
│  Paso 7: Servidor autoritativo de example.com                │
│  El resolver pregunta al servidor autoritativo.              │
│  Responde: "www.example.com = 93.184.216.34, TTL=86400"      │
│  (authoritative answer, aa=1)                                │
│                                                              │
│  Paso 8: El resolver cachea la respuesta (86400 seg = 24h)   │
│  y la devuelve al SO, que la pasa al navegador.              │
└──────────────────────────────────────────────────────────────┘
```

### TTL y caché

Cada respuesta DNS incluye un **TTL** (Time To Live) que indica cuántos segundos se puede cachear:

```
┌──────────────────────────────────────────────────────────────┐
│  Valores de TTL comunes:                                     │
│                                                              │
│  300     (5 min)   — sitios que cambian de IP frecuentemente │
│  3600    (1 hora)  — valor típico para la mayoría            │
│  86400   (24 horas)— dominios muy estables                   │
│  172800  (48 horas)— registros NS de TLDs                    │
│                                                              │
│  TTL bajo (300s):                                            │
│  + Cambios de IP se propagan rápido                          │
│  - Más consultas DNS (más carga en el autoritativo)          │
│                                                              │
│  TTL alto (86400s):                                          │
│  + Menos consultas (mejor rendimiento)                       │
│  - Cambios de IP tardan hasta 24h en propagarse              │
│                                                              │
│  Estrategia antes de una migración:                          │
│  1. Reducir TTL a 300s (días antes)                          │
│  2. Esperar a que el TTL anterior expire                     │
│  3. Cambiar la IP                                            │
│  4. Los clientes recargan en ≤5 min                          │
│  5. Después de la migración, volver a subir el TTL           │
└──────────────────────────────────────────────────────────────┘
```

---

## DNS en Linux: primera mirada

### Ver tu configuración DNS actual

```bash
# Resolvers configurados
cat /etc/resolv.conf

# Ejemplo de salida:
# nameserver 192.168.1.1
# nameserver 8.8.8.8
# search home.local

# Con systemd-resolved (Fedora, Ubuntu moderno):
resolvectl status
# Muestra: DNS servers, search domains, DNSSEC status
```

### Consulta rápida

```bash
# Resolución básica
dig www.example.com

# Solo la respuesta (sin la sección extra)
dig +short www.example.com
# 93.184.216.34

# Ver qué servidor respondió
dig www.example.com | grep "SERVER"
# ;; SERVER: 192.168.1.1#53(192.168.1.1)

# Ver el TTL
dig www.example.com | grep -A1 "ANSWER SECTION"
# www.example.com.    86400  IN  A  93.184.216.34
#                     ─────
#                     TTL en segundos
```

### /etc/hosts

```bash
# Resolución local sin DNS — el archivo más simple
cat /etc/hosts

# 127.0.0.1   localhost
# ::1         localhost
# 192.168.1.50  myserver.local myserver
```

> **Nota**: `dig` no consulta `/etc/hosts` — solo consulta servidores DNS. Para resolver como lo hace el sistema (incluyendo /etc/hosts), usa `getent hosts nombre`.

```bash
# Resolver como lo hace el sistema (respeta /etc/hosts + nsswitch.conf):
getent hosts www.example.com

# Resolver solo vía DNS (ignora /etc/hosts):
dig +short www.example.com
```

---

## Errores comunes

### 1. Confundir dominio con zona

```
# Un dominio es un nombre en la jerarquía DNS.
# Una zona es la porción que un servidor autoritativo administra.
#
# Dominio example.com puede tener múltiples zonas:
#   Zona example.com (en servidor A)
#   Zona api.example.com (delegada a servidor B)
#
# O puede tener una sola zona que cubra todo el dominio.
```

### 2. Olvidar el trailing dot en archivos de zona

```
# En un archivo de zona DNS:
# INCORRECTO:
www  IN  A  93.184.216.34              ← correcto (relativo al dominio)
mx   IN  CNAME  mail.example.com      ← INCORRECTO
# → Se interpreta como: mail.example.com.example.com.

# CORRECTO:
mx   IN  CNAME  mail.example.com.     ← con trailing dot = FQDN
```

### 3. Pensar que los root servers se consultan para cada petición

```
# La PRIMERA resolución de un dominio .com puede consultar a root.
# Pero el resolver cachea la referencia al servidor de .com
# (TTL de NS del root ≈ 48 horas).
#
# Después, las consultas para CUALQUIER dominio .com van
# directamente al servidor de .com, sin pasar por root.
#
# En la práctica, un resolver ocupado consulta root servers
# muy raramente.
```

### 4. Usar CNAME en el apex del dominio

```
# INCORRECTO:
example.com.  IN  CNAME  loadbalancer.cdn.com.
# → Viola la RFC: CNAME no puede coexistir con NS/SOA
#    (y el apex SIEMPRE tiene NS y SOA)

# CORRECTO (opciones):
# • Usar registro A/AAAA directo
# • Usar ALIAS/ANAME (extensión no estándar del proveedor)
# • Usar Cloudflare CNAME flattening
```

### 5. No considerar el TTL antes de cambiar registros DNS

```
# Escenario: cambiar la IP de www.example.com
# TTL actual: 86400 (24 horas)
#
# Si simplemente cambias el registro A:
# → Algunos usuarios seguirán usando la IP vieja hasta 24h
#
# Proceso correcto:
# 1. Bajar TTL a 300 (5 min) — esperar 24h a que expire el TTL viejo
# 2. Cambiar la IP
# 3. En 5 minutos, todos usan la IP nueva
# 4. Subir TTL de nuevo a 86400
```

---

## Cheatsheet

```
┌──────────────────────────────────────────────────────────────┐
│                 Jerarquía DNS Cheatsheet                     │
├──────────────────────────────────────────────────────────────┤
│                                                              │
│  JERARQUÍA:                                                  │
│    . (root) → TLD (.com) → SLD (example) → sub (www)        │
│    Se lee de DERECHA a IZQUIERDA                             │
│    Máximo 253 caracteres, 63 por etiqueta                    │
│                                                              │
│  ROOT SERVERS:                                               │
│    13 identidades (A-M), ~1700+ instancias (anycast)         │
│    Conocen los servidores de cada TLD, nada más              │
│                                                              │
│  TLDs:                                                       │
│    gTLD: .com .org .net .dev .io ...                         │
│    ccTLD: .es .mx .uk .de .jp ...                            │
│    .arpa: resolución inversa                                 │
│    Registry (operador) ≠ Registrar (vendedor)                │
│                                                              │
│  FQDN:                                                       │
│    Termina en punto: www.example.com.                        │
│    Sin punto → puede expandirse con search domain            │
│    dig respeta FQDN; sin punto puede añadir search           │
│                                                              │
│  ZONAS:                                                      │
│    Zona ≠ dominio (un dominio puede tener múltiples zonas)   │
│    Delegación: NS records + glue records                     │
│    Glue: IP del nameserver en la zona PADRE                  │
│                                                              │
│  SERVIDORES:                                                 │
│    Recursivo (resolver): busca, cachea, sirve a clientes     │
│      8.8.8.8 (Google), 1.1.1.1 (Cloudflare), ISP            │
│    Autoritativo: tiene datos originales, responde con aa=1   │
│      ns1.example.com, servidores del registrar               │
│                                                              │
│  RESOLUCIÓN:                                                 │
│    Cliente→Resolver: consulta RECURSIVA                      │
│    Resolver→servidores: consultas ITERATIVAS                 │
│    Orden: caché browser → caché SO → /etc/hosts → resolver   │
│          → root → TLD → autoritativo                         │
│                                                              │
│  TTL:                                                        │
│    300s = 5 min (cambios rápidos)                            │
│    3600s = 1h (valor típico)                                 │
│    86400s = 24h (estable)                                    │
│    Bajar TTL ANTES de migrar, no después                     │
│                                                              │
│  LINUX:                                                      │
│    cat /etc/resolv.conf       configuración DNS              │
│    resolvectl status          con systemd-resolved           │
│    dig +short dominio         consulta rápida                │
│    getent hosts dominio       resolución del sistema         │
│    /etc/hosts                 resolución local manual         │
│                                                              │
│  APEX DOMAIN:                                                │
│    example.com (sin www) NO puede tener CNAME (RFC)          │
│    Usar A/AAAA directo o ALIAS/ANAME del proveedor           │
│                                                              │
│  PUERTO: 53 (UDP consultas normales, TCP zona transfer)      │
│                                                              │
└──────────────────────────────────────────────────────────────┘
```

---

## Ejercicios

### Ejercicio 1: Descomponer la jerarquía

Para cada uno de estos nombres de dominio, identifica: el root (implícito), el TLD, el SLD, los subdominios (si los hay), y si es un FQDN:

```
1. mail.google.com
2. www.bbc.co.uk.
3. api.v2.staging.myapp.dev
4. ns1.example.com
5. 34.216.184.93.in-addr.arpa.
```

**Responde**:
1. ¿Cuál de estos es un FQDN estricto? ¿Cómo lo sabes?
2. En `www.bbc.co.uk`, ¿cuántos niveles hay? ¿Es `.co` un TLD o un SLD?
3. ¿Qué tiene de especial el dominio número 5? ¿Para qué se usa `.in-addr.arpa`?
4. Si `ns1.example.com` es el nameserver de la zona `example.com`, ¿dónde se almacena su dirección IP para resolver la dependencia circular?

**Pregunta de reflexión**: ¿Por qué DNS se diseñó como un sistema jerárquico en vez de una base de datos centralizada plana? ¿Qué pasaría si un solo servidor tuviera todos los registros DNS del mundo?

---

### Ejercicio 2: Trazar una resolución

Simula manualmente el proceso de resolución de `blog.example.com`, paso a paso, asumiendo que **ningún caché** tiene datos previos.

Para cada paso, indica:
- Quién pregunta a quién
- Qué pregunta exactamente
- Qué responde (referral o respuesta final)
- Si la respuesta es iterativa o recursiva

Después, responde:
1. ¿Cuántas consultas DNS se necesitaron en total?
2. Si 2 segundos después otro usuario pide `shop.example.com`, ¿cuántas consultas DNS se necesitan ahora? ¿Por qué menos?
3. Si el TTL de `blog.example.com` es 300 y el de los NS de `.com` es 172800, ¿qué se expira primero del caché? ¿Qué implicación tiene?

**Pregunta de reflexión**: ¿Por qué el resolver recursivo usa consultas iterativas hacia los servidores autoritativos en lugar de pedirles recursión? ¿Qué problema de seguridad y carga habría si los root servers aceptaran consultas recursivas?

---

### Ejercicio 3: Explorar DNS en tu sistema

Ejecuta los siguientes comandos en tu sistema y analiza la salida:

```bash
# 1. ¿Qué resolvers tienes configurados?
cat /etc/resolv.conf

# 2. ¿Tienes systemd-resolved?
resolvectl status 2>/dev/null || echo "No systemd-resolved"

# 3. Consulta un dominio y observa el TTL
dig www.google.com

# 4. Espera 10 segundos y vuelve a consultar
dig www.google.com

# 5. Compara la resolución del sistema con dig
getent hosts www.google.com
dig +short www.google.com

# 6. Verifica si tienes entradas en /etc/hosts
cat /etc/hosts
```

**Responde**:
1. ¿Qué servidores DNS usa tu sistema? ¿Son los de tu router, tu ISP, o públicos?
2. ¿Cambió el TTL entre la primera y segunda consulta con `dig`? ¿Por qué?
3. ¿La IP de `getent hosts` y `dig +short` coincide? ¿Por qué podrían diferir?
4. Si añadieras `127.0.0.1  www.google.com` a `/etc/hosts`, ¿qué respondería `getent hosts`? ¿Y `dig`? ¿Por qué la diferencia?

**Pregunta de reflexión**: ¿Qué ventajas y riesgos tiene usar `8.8.8.8` (Google DNS) como resolver en vez del DNS de tu ISP? Piensa en privacidad, rendimiento, y censura.

---

> **Siguiente tema**: T02 — Tipos de registro (A, AAAA, CNAME, MX, NS, PTR, SOA, TXT, SRV)
