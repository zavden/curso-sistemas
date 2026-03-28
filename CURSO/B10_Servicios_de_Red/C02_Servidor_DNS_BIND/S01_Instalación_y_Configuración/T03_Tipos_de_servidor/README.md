# Tipos de servidor DNS

## Índice

1. [Panorama de roles DNS](#panorama-de-roles-dns)
2. [Caching-only resolver](#caching-only-resolver)
3. [Forwarder](#forwarder)
4. [Servidor autoritativo](#servidor-autoritativo)
5. [Split DNS (vistas)](#split-dns-vistas)
6. [Servidor combinado vs separado](#servidor-combinado-vs-separado)
7. [Comparativa](#comparativa)
8. [Configuraciones completas](#configuraciones-completas)
9. [Errores comunes](#errores-comunes)
10. [Cheatsheet](#cheatsheet)
11. [Ejercicios](#ejercicios)

---

## Panorama de roles DNS

Un servidor BIND puede asumir diferentes roles, solos o combinados:

```
    ┌─────────────────────────────────────────────────────────────┐
    │                     Roles de un servidor DNS                │
    │                                                             │
    │  ┌────────────────┐  ┌────────────────┐  ┌───────────────┐ │
    │  │  Caching-only  │  │   Forwarder    │  │ Autoritativo  │ │
    │  │                │  │                │  │               │ │
    │  │ Resuelve       │  │ Reenvía a      │  │ Responde      │ │
    │  │ recursivamente │  │ otro resolver  │  │ sobre zonas   │ │
    │  │ desde la raíz  │  │ y cachea       │  │ que posee     │ │
    │  │ y cachea       │  │ las respuestas │  │               │ │
    │  └────────────────┘  └────────────────┘  └───────────────┘ │
    │         │                    │                    │         │
    │         │        ┌──────────┴──────────┐         │         │
    │         │        │     Combinado       │         │         │
    │         └────────┤ Autoritativo para   ├─────────┘         │
    │                  │ zonas propias +     │                   │
    │                  │ recursivo/forwarder │                   │
    │                  │ para el resto       │                   │
    │                  └─────────────────────┘                   │
    └─────────────────────────────────────────────────────────────┘
```

Cada rol responde a una necesidad diferente:

| Rol | ¿Cuándo usar? |
|---|---|
| **Caching-only** | Acelerar resolución DNS para la red local sin poseer zonas |
| **Forwarder** | Centralizar consultas DNS a través de un resolver externo (ISP, 8.8.8.8) |
| **Autoritativo** | Servir zonas propias al mundo (tu dominio público o interno) |
| **Combinado** | Oficina pequeña: autoritativo para dominio interno + resolver para todo lo demás |

---

## Caching-only resolver

### Concepto

Un caching-only resolver no tiene zonas propias. Su único trabajo es resolver consultas recursivamente (navegando la jerarquía desde los root servers) y cachear los resultados para acelerar consultas repetidas.

```
    Cliente           Caching-only           Root / TLD / Autoritativo
    ┌──────┐         ┌──────────┐           ┌──────────┐
    │      │─ query ►│          │─ recurse ►│ Root     │
    │ PC   │         │  BIND    │◄──────────│ .com     │
    │      │◄────────│  caché   │─ recurse ►│ ejemplo  │
    │      │ answer  │          │◄──────────│          │
    └──────┘         │          │           └──────────┘
                     │  2ª vez: │
    ┌──────┐         │  desde   │
    │      │─ query ►│  caché   │  (sin contactar servidores externos)
    │ PC2  │◄────────│  ✓       │
    └──────┘ answer  └──────────┘
```

### Configuración

```c
// /etc/bind/named.conf.options (Debian)
// o /etc/named.conf (RHEL)

acl "trusted" {
    localhost;
    localnets;
    192.168.1.0/24;
};

options {
    directory "/var/cache/bind";     // Debian
    // directory "/var/named";       // RHEL

    // Escuchar en la IP de la LAN
    listen-on { 127.0.0.1; 192.168.1.10; };
    listen-on-v6 { ::1; };

    // Permitir consultas solo de la red interna
    allow-query { trusted; };

    // Habilitar recursión para clientes internos
    recursion yes;
    allow-recursion { trusted; };

    // Sin forwarders — resolver desde la raíz
    // (omitir forwarders o dejarlo vacío)

    // Validar DNSSEC
    dnssec-validation auto;

    // Seguridad
    allow-transfer { none; };
    version "Refused";

    // Rendimiento del caché
    max-cache-size 256m;
    max-cache-ttl 86400;       // máximo 24h en caché
    max-ncache-ttl 3600;       // respuestas negativas: máximo 1h
};

// NO hay bloques zone (excepto los default: root hints, localhost)
// Las zonas por defecto ya vienen en named.conf.default-zones (Debian)
// o en el named.conf de stock (RHEL)
```

### Cuándo usar caching-only

- Red local sin dominio propio que resolver
- Reducir latencia DNS para los clientes (consultas repetidas desde caché)
- Control sobre el DNS sin depender del resolver del ISP
- Punto central de logging de consultas DNS
- Filtrado DNS básico (blackhole de dominios)

### Verificación

```bash
# El servidor resuelve dominios externos
dig @192.168.1.10 google.com +stats
# ;; Query time: 45 msec  ← primera consulta, recursión completa

dig @192.168.1.10 google.com +stats
# ;; Query time: 0 msec   ← segunda consulta, desde caché

# Verificar contenido del caché
sudo rndc dumpdb -cache
# Debian: /var/cache/bind/named_dump.db
# RHEL: /var/named/data/cache_dump.db
sudo grep "google.com" /var/cache/bind/named_dump.db
```

---

## Forwarder

### Concepto

Un forwarder no resuelve recursivamente por sí mismo. Reenvía todas las consultas a otro resolver (upstream) y cachea las respuestas.

```
    Cliente           Forwarder             Upstream resolver
    ┌──────┐         ┌──────────┐          ┌──────────────┐
    │      │─ query ►│          │─ query ──►│ 8.8.8.8      │
    │ PC   │         │  BIND    │           │ (Google DNS) │
    │      │◄────────│  caché   │◄─ answer ─│              │
    │      │ answer  │          │           └──────────────┘
    └──────┘         └──────────┘

    Diferencia con caching-only:
    - Caching-only: navega root → TLD → autoritativo (muchas queries)
    - Forwarder: envía UNA query al upstream que hace todo el trabajo
```

### Configuración

```c
options {
    directory "/var/cache/bind";

    listen-on { 127.0.0.1; 192.168.1.10; };

    allow-query { trusted; };

    // Habilitar recursión (necesario para procesar forwarding)
    recursion yes;
    allow-recursion { trusted; };

    // Reenviar a estos servidores
    forwarders {
        8.8.8.8;          // Google DNS primario
        8.8.4.4;          // Google DNS secundario
        1.1.1.1;          // Cloudflare
    };

    // Política de forwarding
    forward only;
    // "only": si los forwarders no responden → SERVFAIL
    // "first": intenta forwarders primero, si fallan → recursión propia

    dnssec-validation auto;
    allow-transfer { none; };
    version "Refused";
};
```

### forward only vs forward first

```
    forward only:
    ┌────────┐     ┌──────────┐     ┌──────────┐
    │Cliente │────►│Forwarder │────►│ Upstream  │
    │        │◄────│          │◄────│ 8.8.8.8  │
    └────────┘     └──────────┘     └──────────┘
                        │
                   Si upstream falla:
                        │
                   SERVFAIL al cliente
                   (no intenta resolver solo)

    forward first:
    ┌────────┐     ┌──────────┐     ┌──────────┐
    │Cliente │────►│Forwarder │────►│ Upstream  │
    │        │◄────│          │◄────│ 8.8.8.8  │
    └────────┘     └──────────┘     └──────────┘
                        │
                   Si upstream falla:
                        │
                   Intenta recursión propia
                   (navega root → TLD → auth)
```

`forward first` da más resiliencia pero requiere acceso a los root servers. `forward only` es más predecible y no requiere acceso directo a Internet para DNS.

### Forwarding por zona

Puedes configurar forwarding diferente para zonas específicas:

```c
// Global: reenviar todo a Google DNS
options {
    forwarders { 8.8.8.8; 1.1.1.1; };
    forward first;
};

// Excepto: consultas del dominio interno van al DNS corporativo
zone "empresa.internal" {
    type forward;
    forwarders { 10.0.0.1; 10.0.0.2; };
    forward only;
};

// Y: consultas de la zona reversa interna también
zone "0.10.in-addr.arpa" {
    type forward;
    forwarders { 10.0.0.1; };
    forward only;
};
```

Esto es muy común en oficinas remotas o cuando conviven un DNS corporativo con acceso a Internet:

```
    *.empresa.internal → DNS corporativo (10.0.0.1)
    *.in-addr.arpa (10.x) → DNS corporativo
    Todo lo demás → Google DNS (8.8.8.8)
```

### Cuándo usar forwarder

- Redes donde no quieres/puedes hacer recursión directa (firewall bloquea DNS saliente excepto a ciertos resolvers)
- Aprovechar el caché de un resolver grande (Google, Cloudflare tienen caché masivo)
- Oficinas remotas que necesitan resolver un dominio interno (forwarding selectivo)
- Redes con restricciones de ancho de banda (una query al forwarder vs múltiples queries recursivas)

---

## Servidor autoritativo

### Concepto

Un servidor autoritativo es la fuente de verdad para una o más zonas. No hace recursión — solo responde consultas sobre zonas que conoce.

```
    Consulta por www.ejemplo.com:

    ┌────────┐     ┌─────────────────┐
    │Resolver│────►│ Autoritativo    │
    │externo │     │ ejemplo.com     │
    │        │◄────│                 │
    └────────┘     │ "www = 1.2.3.4" │
    respuesta      │ flag: aa        │
    autoritativa   └─────────────────┘

    Consulta por www.google.com:

    ┌────────┐     ┌─────────────────┐
    │Resolver│────►│ Autoritativo    │
    │externo │     │ ejemplo.com     │
    │        │◄────│                 │
    └────────┘     │ REFUSED         │
    no recursión   │ (no es mi zona) │
                   └─────────────────┘
```

### Configuración

```c
// Servidor SOLO autoritativo — sin recursión

options {
    directory "/var/cache/bind";

    // Escuchar en la IP pública
    listen-on { 203.0.113.10; 127.0.0.1; };

    // Cualquiera puede consultar (es público)
    allow-query { any; };

    // SIN recursión — solo responde sobre sus zonas
    recursion no;

    // Seguridad
    allow-transfer { none; };    // override por zona
    version "Refused";

    // DNSSEC
    dnssec-validation auto;

    // Sin forwarders (no hace recursión)
    // No definir forwarders

    // Rate limiting (protección contra amplificación)
    rate-limit {
        responses-per-second 10;
        window 5;
    };
};

// Zona primaria
zone "ejemplo.com" IN {
    type master;
    file "/etc/bind/zones/db.ejemplo.com";
    allow-transfer { 198.51.100.20; };   // ns2
    also-notify { 198.51.100.20; };
    notify yes;
};

// Zona reversa (si tienes asignación IP propia)
zone "113.0.203.in-addr.arpa" IN {
    type master;
    file "/etc/bind/zones/db.203.0.113";
    allow-transfer { 198.51.100.20; };
};
```

### Cuándo usar autoritativo puro

- Servidor DNS público que sirve tu dominio
- Servidores DNS de un hosting o registrador
- Cuando la seguridad requiere separar roles (el autoritativo no debe hacer recursión para externos)

### Primario + Secundario

La arquitectura mínima para un servicio autoritativo es un primario y al menos un secundario:

```
    Primario (ns1)                    Secundario (ns2)
    203.0.113.10                      198.51.100.20
    ┌──────────────┐                 ┌──────────────┐
    │ type master  │── AXFR/IXFR ──►│ type slave   │
    │              │── NOTIFY ─────►│              │
    │ Editas aquí  │                 │ Solo lectura │
    │              │                 │ Copia del    │
    │              │                 │ primario     │
    └──────────────┘                 └──────────────┘
         │                                │
         └───────────┬────────────────────┘
                     │
              Ambos registrados como NS
              en el registrador de dominio
```

---

## Split DNS (vistas)

### Concepto

Split DNS (o split-horizon DNS) sirve respuestas diferentes según quién pregunte. Es común para servidores que deben dar IPs internas a clientes internos e IPs públicas a clientes externos:

```
    Desde Internet (203.0.113.50):
    dig www.empresa.com → 203.0.113.10  (IP pública)

    Desde la LAN (192.168.1.100):
    dig www.empresa.com → 192.168.1.20  (IP interna)
```

### Implementación con views

```c
// ACLs definidas al principio
acl "internal" {
    192.168.1.0/24;
    10.0.0.0/8;
    localhost;
};

// Vista interna — se evalúa primero para clientes internos
view "internal" {
    match-clients { internal; };

    // Recursión habilitada para internos
    recursion yes;
    allow-recursion { internal; };

    // Zona con IPs internas
    zone "empresa.com" IN {
        type master;
        file "/etc/bind/zones/db.empresa.com.internal";
    };

    // Zona del dominio interno (solo visible desde la LAN)
    zone "empresa.internal" IN {
        type master;
        file "/etc/bind/zones/db.empresa.internal";
    };

    // Zonas default (root hints, localhost)
    include "/etc/bind/named.conf.default-zones";
};

// Vista externa — para todos los demás
view "external" {
    match-clients { any; };

    // Sin recursión para externos
    recursion no;

    // Zona con IPs públicas
    zone "empresa.com" IN {
        type master;
        file "/etc/bind/zones/db.empresa.com.external";
    };

    // empresa.internal NO existe en esta vista
    // → los externos no pueden resolverla
};
```

### Archivos de zona para cada vista

```
; /etc/bind/zones/db.empresa.com.internal
; IPs internas
$TTL 86400
$ORIGIN empresa.com.
@   IN  SOA  ns1.empresa.com. admin.empresa.com. (2026032101 3600 900 604800 86400)
@   IN  NS   ns1.empresa.com.
@   IN  NS   ns2.empresa.com.
ns1    IN  A   192.168.1.1
ns2    IN  A   192.168.1.2
www    IN  A   192.168.1.20      ; ← IP interna
mail   IN  A   192.168.1.10
api    IN  A   192.168.1.30
```

```
; /etc/bind/zones/db.empresa.com.external
; IPs públicas
$TTL 86400
$ORIGIN empresa.com.
@   IN  SOA  ns1.empresa.com. admin.empresa.com. (2026032101 3600 900 604800 86400)
@   IN  NS   ns1.empresa.com.
@   IN  NS   ns2.empresa.com.
ns1    IN  A   203.0.113.10
ns2    IN  A   198.51.100.20
www    IN  A   203.0.113.10      ; ← IP pública
mail   IN  A   203.0.113.11
; api no existe para el exterior
```

### Reglas de las vistas

```
    1. Las vistas se evalúan en ORDEN — la primera que coincide gana
    2. Cada zona que necesite el servidor DEBE estar dentro de una vista
    3. Las zonas default (root hints, localhost) deben incluirse en CADA vista
       que haga recursión
    4. Si usas vistas, TODAS las zonas deben estar dentro de una vista
       (no puedes mezclar zonas dentro y fuera de views)
    5. Los seriales de las zonas interna/externa son independientes
       (no tienen relación, son archivos diferentes)
```

> **Error frecuente**: cuando activas vistas, las zonas que estaban fuera de un `view` block dejan de funcionar. BIND requiere que, si hay al menos un `view`, **todo** esté dentro de un `view`.

---

## Servidor combinado vs separado

### Combinado — un servidor hace todo

```c
// Típico de oficina pequeña o red doméstica

options {
    listen-on { 127.0.0.1; 192.168.1.10; };
    recursion yes;
    allow-recursion { trusted; };
    allow-query { trusted; };
    forwarders { 8.8.8.8; 1.1.1.1; };
    forward first;
};

// Autoritativo para el dominio interno
zone "oficina.local" IN {
    type master;
    file "/etc/bind/zones/db.oficina.local";
};

zone "1.168.192.in-addr.arpa" IN {
    type master;
    file "/etc/bind/zones/db.192.168.1";
};

// El resto → recursión/forwarding
```

### Separado — roles en servidores distintos

```
    Internet
       │
    ┌──┴──────────────┐           ┌────────────────┐
    │ DNS Autoritativo│           │ DNS Resolver   │
    │ (público)       │           │ (interno)      │
    │                 │           │                │
    │ recursion no;   │           │ recursion yes; │
    │ allow-query any;│           │ solo LAN       │
    │                 │           │                │
    │ Sirve:          │           │ Resuelve:      │
    │ ejemplo.com     │           │ todo Internet  │
    │ (IPs públicas)  │           │ + oficina.local│
    └─────────────────┘           └────────────────┘
```

### Cuándo separar

| Criterio | Combinado | Separado |
|---|---|---|
| Tamaño de red | Pequeña (<50 hosts) | Mediana-grande |
| Seguridad requerida | Básica | Alta (DMZ, compliance) |
| Tráfico | Bajo | Alto (miles de queries/s) |
| Complejidad | Simple | Mayor (dos servidores) |
| Riesgo de open resolver | Existe si se configura mal | Eliminado por diseño |

La razón principal para separar: **un servidor autoritativo público con recursión habilitada es un vector de ataque** (DNS amplification). Separar roles elimina este riesgo por diseño.

---

## Comparativa

```
    ┌─────────────────┬─────────────┬─────────────┬──────────────┬──────────────┐
    │                 │ Caching-    │  Forwarder  │ Autoritativo │  Combinado   │
    │                 │   only      │             │              │              │
    ├─────────────────┼─────────────┼─────────────┼──────────────┼──────────────┤
    │ recursion       │ yes         │ yes         │ no           │ yes          │
    │ forwarders      │ (vacío)     │ 8.8.8.8...  │ (vacío)      │ opcional     │
    │ forward         │ —           │ only/first  │ —            │ first        │
    │ zonas propias   │ no          │ opcional    │ sí           │ sí           │
    │ allow-query     │ LAN         │ LAN         │ any          │ LAN          │
    │ allow-recursion │ LAN         │ LAN         │ —            │ LAN          │
    │ acceso Internet │ sí (DNS)    │ solo fwd    │ no necesita  │ sí (DNS)     │
    │ uso típico      │ red local   │ oficina     │ dominio púb. │ pyme         │
    └─────────────────┴─────────────┴─────────────┴──────────────┴──────────────┘
```

---

## Configuraciones completas

### Configuración 1 — Caching-only para red doméstica

```c
// /etc/bind/named.conf.options

acl "home" {
    localhost;
    localnets;
    192.168.1.0/24;
};

options {
    directory "/var/cache/bind";
    listen-on { 127.0.0.1; 192.168.1.1; };
    listen-on-v6 { ::1; };

    allow-query { home; };
    recursion yes;
    allow-recursion { home; };

    dnssec-validation auto;
    allow-transfer { none; };
    version "Refused";

    max-cache-size 128m;
};
```

Configurar los clientes de la red para usar este DNS:

```bash
# En el router: configurar DHCP para entregar 192.168.1.1 como DNS
# O manualmente en cada cliente:
# /etc/resolv.conf: nameserver 192.168.1.1
```

### Configuración 2 — Forwarder corporativo con split forwarding

```c
// Sucursal que necesita resolver dominio interno + Internet

acl "office" {
    localhost;
    localnets;
    10.10.0.0/16;
};

options {
    directory "/var/cache/bind";
    listen-on { 127.0.0.1; 10.10.1.10; };

    allow-query { office; };
    recursion yes;
    allow-recursion { office; };

    // Por defecto: reenviar a Cloudflare
    forwarders { 1.1.1.1; 1.0.0.1; };
    forward first;

    dnssec-validation auto;
    allow-transfer { none; };
    version "Refused";
};

// Dominio interno: reenviar al DNS corporativo (sede central)
zone "corp.empresa.com" {
    type forward;
    forwarders { 10.0.0.1; 10.0.0.2; };
    forward only;
};

zone "0.10.in-addr.arpa" {
    type forward;
    forwarders { 10.0.0.1; };
    forward only;
};
```

### Configuración 3 — Autoritativo público

```c
// Solo sirve zonas propias, sin recursión

options {
    directory "/var/named";
    listen-on { 203.0.113.10; };

    allow-query { any; };
    recursion no;                // CRÍTICO para servidor público

    allow-transfer { none; };   // override por zona
    version "Refused";
    hostname none;

    dnssec-validation auto;

    rate-limit {
        responses-per-second 10;
        window 5;
    };
};

zone "ejemplo.com" IN {
    type master;
    file "zones/db.ejemplo.com";
    allow-transfer { 198.51.100.20; };
    also-notify { 198.51.100.20; };
    notify yes;
};

zone "113.0.203.in-addr.arpa" IN {
    type master;
    file "zones/db.203.0.113";
    allow-transfer { 198.51.100.20; };
};
```

### Configuración 4 — Combinado con vistas (PYME)

```c
acl "internal" {
    192.168.1.0/24;
    localhost;
};

view "lan" {
    match-clients { internal; };
    recursion yes;
    allow-recursion { internal; };

    // Zona interna con IPs privadas
    zone "empresa.local" IN {
        type master;
        file "/etc/bind/zones/db.empresa.local";
    };

    zone "1.168.192.in-addr.arpa" IN {
        type master;
        file "/etc/bind/zones/db.192.168.1";
    };

    // Forwarders para Internet
    forwarders { 8.8.8.8; 1.1.1.1; };
    forward first;

    // Zonas estándar para recursión
    include "/etc/bind/named.conf.default-zones";
};

view "public" {
    match-clients { any; };
    recursion no;

    // Si este servidor también es autoritativo público:
    zone "empresa.com" IN {
        type master;
        file "/etc/bind/zones/db.empresa.com.public";
    };
};
```

---

## Errores comunes

### 1. Servidor autoritativo con recursión abierta

```c
// MAL — open resolver accesible desde Internet
options {
    allow-query { any; };
    recursion yes;
    // Sin allow-recursion → todos pueden hacer recursión
    // = vector de amplificación DDoS
};

// BIEN — separar claramente
// Opción A: autoritativo puro
options {
    allow-query { any; };
    recursion no;
};

// Opción B: recursión solo para internos
options {
    allow-query { any; };
    recursion yes;
    allow-recursion { trusted; };  // solo LAN
};
```

### 2. forward only sin considerar la resiliencia

```c
// forward only depende completamente de los forwarders
options {
    forwarders { 8.8.8.8; };
    forward only;
};

// Si 8.8.8.8 es inalcanzable (firewall, caída de Google):
// TODAS las consultas DNS fallan con SERVFAIL

// Solución A: múltiples forwarders
forwarders { 8.8.8.8; 1.1.1.1; 9.9.9.9; };

// Solución B: forward first (fallback a recursión)
forward first;

// Solución C: monitorizar los forwarders
```

### 3. Olvidar las zonas default dentro de views

```c
// MAL — activaste views pero olvidaste incluir las zonas estándar
view "internal" {
    match-clients { internal; };
    recursion yes;

    zone "oficina.local" { type master; file "..."; };
    // Sin root hints → la recursión no funciona
    // "SERVFAIL" para todo lo que no sea oficina.local
};

// BIEN
view "internal" {
    match-clients { internal; };
    recursion yes;

    zone "oficina.local" { type master; file "..."; };

    // Incluir zonas estándar (root hints, localhost)
    include "/etc/bind/named.conf.default-zones";
};
```

### 4. Zonas fuera de views cuando existen views

```c
// MAL — mezclar zonas dentro y fuera de views
view "internal" {
    match-clients { internal; };
    zone "interna.local" { ... };
};

zone "ejemplo.com" { ... };     // ← ERROR: fuera de view
// BIND rechaza: "zone 'ejemplo.com' was not in any view"

// BIEN — todo dentro de un view
view "internal" {
    match-clients { internal; };
    zone "interna.local" { ... };
};

view "external" {
    match-clients { any; };
    zone "ejemplo.com" { ... };
};
```

### 5. Confundir forwarding global con forwarding por zona

```c
// forward global (en options): aplica a TODAS las consultas
options {
    forwarders { 8.8.8.8; };
    forward only;
};

// forward por zona: aplica SOLO a esa zona
zone "especial.com" {
    type forward;
    forwarders { 10.0.0.1; };
    forward only;
};
// ¡Cuidado! Las consultas de ejemplo.com van a 8.8.8.8
// Las consultas de especial.com van a 10.0.0.1
// Si defines forward por zona, el forward global NO aplica a esa zona
```

---

## Cheatsheet

```bash
# === CACHING-ONLY ===
# options:
#   recursion yes;
#   allow-recursion { LAN; };
#   (sin forwarders, sin zonas propias)

# === FORWARDER ===
# options:
#   recursion yes;
#   forwarders { 8.8.8.8; 1.1.1.1; };
#   forward only;    (o first para fallback)

# === AUTORITATIVO ===
# options:
#   recursion no;
#   allow-query { any; };
# zone "dominio" { type master; file "..."; };

# === COMBINADO ===
# options:
#   recursion yes;
#   allow-recursion { LAN; };
# zone "dominio" { type master; file "..."; };
# + forwarders o recursión para Internet

# === SPLIT DNS (VIEWS) ===
# view "internal" { match-clients { LAN; }; recursion yes; zone "..." { ... }; };
# view "external" { match-clients { any; }; recursion no; zone "..." { ... }; };

# === FORWARDING POR ZONA ===
# zone "especial.com" { type forward; forwarders { IP; }; forward only; };

# === VERIFICACIÓN ===
dig @servidor dominio +recurse               # probar recursión
dig @servidor dominio +norecurse             # probar autoritativo (flag aa)
dig @servidor version.bind chaos txt +short  # versión del servidor
sudo rndc dumpdb -cache                      # volcar caché a disco
sudo rndc flush                              # vaciar caché
sudo rndc status                             # estado del servidor

# === DIAGNÓSTICO ===
# ¿Hace recursión?
dig @servidor google.com
# Si responde con IP → recursión activa
# Si REFUSED o SERVFAIL → recursión desactivada

# ¿Es autoritativo para una zona?
dig @servidor dominio SOA +norecurse
# Si flag "aa" presente → es autoritativo
# Si no hay "aa" → no es autoritativo (respondió desde caché o recursión)
```

---

## Ejercicios

### Ejercicio 1 — Configurar un forwarder con split forwarding

Configura un servidor BIND que:

1. Reenvíe todas las consultas a Cloudflare (1.1.1.1, 1.0.0.1) con `forward first`
2. Reenvíe consultas de `lab.internal` a un "DNS interno" simulado (127.0.0.1 puerto 5353, o simplemente configúralo y observa el comportamiento)
3. Solo acepte consultas desde localhost y tu subred
4. Verifica que las consultas a Internet se resuelven y que las consultas a `lab.internal` intentan usar el forwarder específico

<details>
<summary>Solución</summary>

```c
acl "local" {
    localhost;
    localnets;
};

options {
    directory "/var/cache/bind";
    listen-on { 127.0.0.1; };
    allow-query { local; };
    recursion yes;
    allow-recursion { local; };

    forwarders { 1.1.1.1; 1.0.0.1; };
    forward first;

    dnssec-validation auto;
    allow-transfer { none; };
};

zone "lab.internal" {
    type forward;
    forwarders { 127.0.0.1 port 5353; };
    forward only;
};
```

```bash
sudo named-checkconf && sudo systemctl reload named

# Consulta a Internet → usa Cloudflare
dig @127.0.0.1 example.com
# Debe resolver correctamente

# Consulta a lab.internal → intenta el forwarder específico
dig @127.0.0.1 test.lab.internal
# SERVFAIL (el forwarder 127.0.0.1:5353 no existe)
# Pero en los logs verás que intentó contactar 127.0.0.1:5353,
# no 1.1.1.1 → el split forwarding funciona

sudo journalctl -u named --since "1 min ago" | grep "lab.internal"
```

</details>

---

### Ejercicio 2 — Convertir a autoritativo puro

Partiendo de la configuración por defecto de BIND (recursión habilitada):

1. Crea una zona `test.lab` con al menos 3 registros A y 1 MX
2. Configura el servidor como **autoritativo puro** (sin recursión)
3. Verifica que responde consultas sobre `test.lab` con el flag `aa`
4. Verifica que rechaza consultas recursivas para dominios externos
5. Comprueba con `dig` la diferencia entre consultar `www.test.lab` y `google.com`

<details>
<summary>Solución</summary>

```c
// options:
options {
    directory "/var/cache/bind";
    listen-on { 127.0.0.1; };
    allow-query { any; };
    recursion no;              // ← autoritativo puro
    allow-transfer { none; };
};

zone "test.lab" IN {
    type master;
    file "/etc/bind/zones/db.test.lab";
};
```

```
; /etc/bind/zones/db.test.lab
$TTL 86400
$ORIGIN test.lab.
@   IN  SOA  ns1.test.lab. admin.test.lab. (2026032101 3600 900 604800 86400)
@   IN  NS   ns1.test.lab.
@   IN  MX   10 mail.test.lab.
ns1   IN  A   192.168.1.1
www   IN  A   192.168.1.20
mail  IN  A   192.168.1.10
api   IN  A   192.168.1.30
```

```bash
sudo named-checkzone test.lab /etc/bind/zones/db.test.lab
sudo named-checkconf && sudo systemctl reload named

# Consulta autoritativa — funciona, flag aa presente
dig @127.0.0.1 www.test.lab A
# flags: qr aa rd; QUERY: 1, ANSWER: 1
#                ↑↑ Authoritative Answer

# Consulta recursiva — REFUSED
dig @127.0.0.1 google.com
# ;; flags: qr rd; QUERY: 1, ANSWER: 0
# status: REFUSED
```

</details>

---

### Ejercicio 3 — Split DNS con dos vistas

Configura split DNS para el dominio `empresa.test`:

1. Vista interna (para 192.168.1.0/24 y localhost):
   - `www.empresa.test` → 192.168.1.20 (IP interna)
   - Recursión habilitada con forwarding a 8.8.8.8
2. Vista externa (para todos los demás):
   - `www.empresa.test` → 203.0.113.10 (IP pública)
   - Sin recursión
3. Verifica que `dig @127.0.0.1 www.empresa.test` devuelve la IP interna
4. Piensa: ¿cómo probarías la vista externa desde la misma máquina?

<details>
<summary>Solución</summary>

```c
acl "internal" {
    192.168.1.0/24;
    localhost;
};

view "lan" {
    match-clients { internal; };
    recursion yes;
    allow-recursion { internal; };
    forwarders { 8.8.8.8; };
    forward first;

    zone "empresa.test" IN {
        type master;
        file "/etc/bind/zones/db.empresa.test.internal";
    };

    include "/etc/bind/named.conf.default-zones";
};

view "wan" {
    match-clients { any; };
    recursion no;

    zone "empresa.test" IN {
        type master;
        file "/etc/bind/zones/db.empresa.test.external";
    };
};
```

```
; db.empresa.test.internal
$TTL 86400
$ORIGIN empresa.test.
@   IN SOA ns1.empresa.test. admin.empresa.test. (2026032101 3600 900 604800 86400)
@   IN NS  ns1.empresa.test.
ns1 IN A   192.168.1.1
www IN A   192.168.1.20
```

```
; db.empresa.test.external
$TTL 86400
$ORIGIN empresa.test.
@   IN SOA ns1.empresa.test. admin.empresa.test. (2026032101 3600 900 604800 86400)
@   IN NS  ns1.empresa.test.
ns1 IN A   203.0.113.10
www IN A   203.0.113.10
```

```bash
# Desde localhost → vista interna
dig @127.0.0.1 www.empresa.test +short
# 192.168.1.20

# ¿Cómo probar la vista externa desde la misma máquina?
# No se puede directamente con dig @127.0.0.1 porque localhost
# coincide con la vista "lan".
# Opciones:
# 1. Desde otra máquina fuera de 192.168.1.0/24
# 2. Usar named -T (test mode) o un contenedor Docker
#    con IP fuera de la ACL "internal"
# 3. Temporalmente mover localhost fuera de la ACL y recargar
```

</details>

---

### Pregunta de reflexión

> Tu empresa tiene un solo servidor BIND configurado como combinado: es autoritativo para `empresa.com` (accesible desde Internet) y hace recursión para los 200 PCs de la oficina. Un día descubres que este servidor aparece en listas de open resolvers y está siendo usado para ataques de amplificación DNS. El tráfico saliente se ha quintuplicado. ¿Cuál es la causa raíz, cuál es la solución inmediata y cuál es la solución arquitectónica correcta?

> **Razonamiento esperado**: la causa raíz es que `allow-recursion` no está restringido, por lo que cualquier IP de Internet puede hacer consultas recursivas a través de tu servidor. **Solución inmediata**: añadir `allow-recursion { trusted; }` donde `trusted` solo incluye la LAN (192.168.0.0/16 o lo que corresponda). Esto bloquea la recursión para IPs externas pero mantiene el servicio autoritativo funcionando. **Solución arquitectónica**: separar en dos servidores (o dos instancias de BIND): un autoritativo público (`recursion no`, `allow-query { any }`) y un resolver interno (`recursion yes`, `allow-query` solo LAN). Esto elimina el riesgo por diseño — es imposible que el autoritativo sea usado como amplificador porque no hace recursión. Adicionalmente: habilitar `rate-limit` en el autoritativo como defensa en profundidad.

---

> **Siguiente tema**: T04 — DNSSEC (conceptos básicos, validación, firma de zonas)
