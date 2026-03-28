# Tipos de registro DNS

## Índice
1. [Estructura de un registro DNS](#estructura-de-un-registro-dns)
2. [Registros de dirección: A y AAAA](#registros-de-dirección-a-y-aaaa)
3. [Registro CNAME](#registro-cname)
4. [Registros de correo: MX](#registros-de-correo-mx)
5. [Registros de delegación: NS](#registros-de-delegación-ns)
6. [Registro SOA](#registro-soa)
7. [Registro PTR](#registro-ptr)
8. [Registro TXT](#registro-txt)
9. [Registro SRV](#registro-srv)
10. [Otros registros relevantes](#otros-registros-relevantes)
11. [Errores comunes](#errores-comunes)
12. [Cheatsheet](#cheatsheet)
13. [Ejercicios](#ejercicios)

---

## Estructura de un registro DNS

Todos los registros DNS siguen el mismo formato base, llamado **Resource Record (RR)**:

```
┌──────────────────────────────────────────────────────────────┐
│                  Resource Record (RR)                         │
│                                                              │
│  NAME          TTL     CLASS   TYPE    RDATA                 │
│  ────          ───     ─────   ────    ─────                 │
│  www.example.com.  3600   IN     A     93.184.216.34         │
│  │                 │     │      │      │                     │
│  │                 │     │      │      └── Datos (varía por tipo)
│  │                 │     │      └── Tipo de registro         │
│  │                 │     └── Clase (IN = Internet, siempre)  │
│  │                 └── Time To Live en segundos              │
│  └── Nombre del dominio                                      │
│                                                              │
│  En la práctica, CLASS es siempre IN (Internet).             │
│  Existen CH (Chaos) y HS (Hesiod) pero son obsoletas.        │
└──────────────────────────────────────────────────────────────┘
```

### Consultar registros con dig

```bash
# Consultar un tipo específico
dig example.com A           # registros A
dig example.com AAAA        # registros AAAA
dig example.com MX          # registros MX
dig example.com NS          # registros NS
dig example.com TXT         # registros TXT
dig example.com SOA         # registro SOA
dig example.com ANY         # todos los tipos (muchos servidores lo bloquean)

# Salida de dig (secciones):
# ;; QUESTION SECTION:   → lo que preguntaste
# ;; ANSWER SECTION:     → la respuesta
# ;; AUTHORITY SECTION:  → NS autoritativos de la zona
# ;; ADDITIONAL SECTION: → IPs de los NS (glue records)
```

---

## Registros de dirección: A y AAAA

Los registros más fundamentales — mapean un nombre a una dirección IP:

### Registro A (IPv4)

```
www.example.com.    3600    IN    A    93.184.216.34
```

Un dominio puede tener **múltiples registros A** para balanceo de carga o redundancia:

```bash
dig +short google.com A
# 142.250.185.206
# 142.250.185.238
# 142.250.185.174
# ...

# El resolver devuelve varios registros.
# El cliente elige uno (típicamente el primero, o aleatoriamente).
# DNS round-robin: cada consulta puede rotar el orden.
```

### Registro AAAA (IPv6)

Llamado "quad-A" porque una dirección IPv6 tiene 4 veces el tamaño de IPv4 (128 vs 32 bits):

```
www.example.com.    3600    IN    AAAA    2606:2800:220:1:248:1893:25c8:1946
```

```bash
dig +short google.com AAAA
# 2607:f8b0:4004:800::200e
```

### Dual stack

Un dominio con ambos registros A y AAAA soporta IPv4 e IPv6. El cliente elige qué usar (los navegadores modernos usan Happy Eyeballs — intentan ambos y usan el que responda primero):

```
example.com.    3600    IN    A       93.184.216.34
example.com.    3600    IN    AAAA    2606:2800:220:1::
```

---

## Registro CNAME

CNAME (Canonical Name) crea un **alias** — un nombre que apunta a otro nombre, no a una IP:

```
┌──────────────────────────────────────────────────────────────┐
│                     CNAME                                    │
│                                                              │
│  blog.example.com.  3600  IN  CNAME  example.github.io.     │
│  ────────────────                     ──────────────────     │
│  alias                                nombre canónico        │
│                                                              │
│  Resolución:                                                 │
│  1. "¿IP de blog.example.com?"                               │
│  2. → CNAME example.github.io (no es una IP, seguir)        │
│  3. "¿IP de example.github.io?"                              │
│  4. → A 185.199.108.153 (IP final)                           │
│                                                              │
│  El resolver sigue la cadena CNAME automáticamente.          │
└──────────────────────────────────────────────────────────────┘
```

### Usos comunes de CNAME

```
# Apuntar a un servicio externo:
blog.example.com.     CNAME  example.github.io.
shop.example.com.     CNAME  shops.myshopify.com.
docs.example.com.     CNAME  example.readthedocs.io.

# Apuntar a un load balancer (AWS, GCP, etc.):
api.example.com.      CNAME  lb-1234.us-east-1.elb.amazonaws.com.

# Crear un alias dentro del mismo dominio:
www.example.com.      CNAME  example.com.
```

### Restricciones fundamentales de CNAME

```
┌──────────────────────────────────────────────────────────────┐
│  Regla 1: CNAME NO puede coexistir con otros registros      │
│  ──────────────────────────────────────────────────          │
│                                                              │
│  INCORRECTO:                                                 │
│  example.com.    IN    CNAME   other.com.                    │
│  example.com.    IN    MX      10 mail.example.com.          │
│  → Violación: CNAME prohíbe cualquier otro registro         │
│    en el mismo nombre.                                       │
│                                                              │
│  Regla 2: CNAME NO puede usarse en el apex del dominio      │
│  ──────────────────────────────────────────────────          │
│                                                              │
│  INCORRECTO:                                                 │
│  example.com.    IN    CNAME   loadbalancer.cdn.com.         │
│  → El apex siempre tiene SOA y NS → conflicto con regla 1.  │
│                                                              │
│  Regla 3: Cadenas de CNAME NO deben crear ciclos             │
│  ──────────────────────────────────────────────              │
│                                                              │
│  INCORRECTO:                                                 │
│  a.example.com.  CNAME  b.example.com.                      │
│  b.example.com.  CNAME  a.example.com.                      │
│  → Bucle infinito.                                           │
└──────────────────────────────────────────────────────────────┘
```

### CNAME vs A: cuándo usar cada uno

```
┌──────────────────────────────────────────────────────────────┐
│  Usar A/AAAA cuando:                                         │
│  • Controlas la IP directamente                              │
│  • Es el apex del dominio                                    │
│  • Necesitas máximo rendimiento (1 consulta vs 2+)           │
│                                                              │
│  Usar CNAME cuando:                                          │
│  • Apuntas a un servicio externo (CDN, PaaS, SaaS)          │
│  • El proveedor puede cambiar la IP sin avisarte             │
│  • Quieres que el proveedor gestione el DNS del destino      │
│                                                              │
│  CNAME implica una consulta DNS extra (resolver el alias     │
│  + resolver el destino). En la práctica, la diferencia       │
│  de latencia es negligible gracias al caché.                 │
└──────────────────────────────────────────────────────────────┘
```

---

## Registros de correo: MX

MX (Mail Exchanger) indica qué servidores reciben email para un dominio:

```
example.com.    3600    IN    MX    10    mail1.example.com.
example.com.    3600    IN    MX    20    mail2.example.com.
example.com.    3600    IN    MX    30    mail3.backup.com.
                                   ──    ──────────────────
                                   │     │
                                   │     └── Servidor de correo
                                   └── Prioridad (menor = preferido)
```

### Prioridad

```
┌──────────────────────────────────────────────────────────────┐
│  Cuando alguien envía un email a user@example.com:           │
│                                                              │
│  1. El servidor emisor consulta los registros MX             │
│     de example.com                                           │
│                                                              │
│  2. Ordena por prioridad (menor número = mayor prioridad):   │
│     MX 10 mail1.example.com.  ← intentar primero            │
│     MX 20 mail2.example.com.  ← si mail1 falla              │
│     MX 30 mail3.backup.com.   ← último recurso              │
│                                                              │
│  3. Intenta entregar a mail1. Si falla (timeout,             │
│     conexión rechazada), prueba mail2. Etc.                  │
│                                                              │
│  4. Si TODOS fallan, reintenta después (según RFC,           │
│     hasta 5 días antes de generar un bounce).                │
│                                                              │
│  Misma prioridad = balanceo de carga:                        │
│     MX 10 mail1.example.com.                                 │
│     MX 10 mail2.example.com.                                 │
│     → El emisor elige aleatoriamente entre ambos             │
└──────────────────────────────────────────────────────────────┘
```

### MX NO puede apuntar a CNAME

```
# INCORRECTO (según RFC 2181 y RFC 5321):
example.com.    MX    10    mail.example.com.
mail.example.com.    CNAME    mail.google.com.

# CORRECTO:
example.com.    MX    10    aspmx.l.google.com.
# (apuntar directamente al nombre final)
```

```bash
# Consultar MX de un dominio
dig example.com MX +short
# 10 mail1.example.com.
# 20 mail2.example.com.

# Verificar MX de Gmail
dig gmail.com MX +short
# 5 gmail-smtp-in.l.google.com.
# 10 alt1.gmail-smtp-in.l.google.com.
# 20 alt2.gmail-smtp-in.l.google.com.
# ...
```

---

## Registros de delegación: NS

NS (Name Server) indica qué servidores DNS son autoritativos para una zona:

```
example.com.    172800    IN    NS    ns1.example.com.
example.com.    172800    IN    NS    ns2.example.com.
```

### Dos contextos de NS

```
┌──────────────────────────────────────────────────────────────┐
│  1. NS en la zona PADRE (delegación):                        │
│     Almacenados en el servidor de .com                       │
│     "example.com está gestionado por estos nameservers"      │
│     Incluyen glue records si el NS está en la propia zona    │
│                                                              │
│  2. NS en la zona PROPIA (autoridad):                        │
│     Almacenados en el propio servidor autoritativo            │
│     "Yo soy autoritativo, y mis compañeros son estos"        │
│     Deben coincidir con los del padre                        │
│                                                              │
│  Ejemplo (en el servidor de .com):                           │
│  example.com.        NS    ns1.example.com.   ← delegación  │
│  example.com.        NS    ns2.example.com.   ← delegación  │
│  ns1.example.com.    A     93.184.216.1       ← glue        │
│  ns2.example.com.    A     93.184.216.2       ← glue        │
│                                                              │
│  Ejemplo (en el servidor de example.com):                    │
│  example.com.        NS    ns1.example.com.   ← autoridad   │
│  example.com.        NS    ns2.example.com.   ← autoridad   │
└──────────────────────────────────────────────────────────────┘
```

### Buenas prácticas

- **Mínimo 2 NS** por zona (redundancia). Muchos registrars exigen al menos 2.
- NS en **redes distintas** (si un datacenter cae, el otro NS responde).
- TTL alto para NS (típicamente 172800 = 48 horas).

```bash
# Ver los NS de un dominio
dig example.com NS +short
# ns1.example.com.
# ns2.example.com.

# Consultar directamente a un NS específico
dig @ns1.example.com www.example.com A
```

---

## Registro SOA

SOA (Start of Authority) es el primer registro de cada zona DNS. Contiene metadatos administrativos:

```
example.com.  86400  IN  SOA  ns1.example.com. admin.example.com. (
                              2026032101  ; Serial
                              7200        ; Refresh
                              3600        ; Retry
                              1209600     ; Expire
                              86400       ; Minimum TTL / Negative TTL
                              )
```

```
┌──────────────────────────────────────────────────────────────┐
│                    Campos del SOA                            │
│                                                              │
│  ns1.example.com.     ← MNAME: primary nameserver            │
│  admin.example.com.   ← RNAME: email del admin              │
│                          (el primer . reemplaza al @:         │
│                           admin.example.com = admin@example.com)
│                                                              │
│  2026032101  ← Serial: número de versión de la zona          │
│                Convención: YYYYMMDDNN (fecha + secuencia)    │
│                Se incrementa con cada cambio                 │
│                Los secundarios comparan serial para saber     │
│                si necesitan actualizar (zone transfer)        │
│                                                              │
│  7200        ← Refresh: cada cuántos segundos el secundario  │
│                consulta al primario por cambios (2 horas)    │
│                                                              │
│  3600        ← Retry: si el refresh falla, reintentar        │
│                después de estos segundos (1 hora)            │
│                                                              │
│  1209600     ← Expire: si el secundario no contacta al       │
│                primario en este tiempo, deja de responder    │
│                (14 días — la zona se considera obsoleta)      │
│                                                              │
│  86400       ← Minimum: TTL para respuestas NEGATIVAS        │
│                (NXDOMAIN — "este dominio no existe")         │
│                Cuánto tiempo cachear "no existe" (24 horas)  │
└──────────────────────────────────────────────────────────────┘
```

### Negative caching (TTL negativo)

```
┌──────────────────────────────────────────────────────────────┐
│  dig noexiste.example.com                                    │
│                                                              │
│  Respuesta: NXDOMAIN (este nombre no existe)                 │
│  El resolver cachea este "no existe" por el tiempo           │
│  del campo Minimum del SOA.                                  │
│                                                              │
│  Si Minimum = 86400:                                         │
│  → "noexiste.example.com no existe" se cachea 24 horas      │
│  → Si creas el registro, nadie lo verá hasta que expire      │
│    el negative cache                                         │
│                                                              │
│  Por eso el Minimum no debe ser excesivamente alto.          │
│  Valores típicos: 300 a 86400 segundos.                      │
└──────────────────────────────────────────────────────────────┘
```

```bash
# Consultar SOA
dig example.com SOA

# Ejemplo de salida:
# example.com.  86400  IN  SOA  ns1.example.com. admin.example.com. (
#               2026032101 7200 3600 1209600 86400 )
```

---

## Registro PTR

PTR (Pointer) hace la **resolución inversa**: de IP a nombre. Es el complemento de los registros A/AAAA:

```
┌──────────────────────────────────────────────────────────────┐
│  Resolución directa (A):                                     │
│    www.example.com → 93.184.216.34                           │
│                                                              │
│  Resolución inversa (PTR):                                   │
│    93.184.216.34 → www.example.com                           │
│                                                              │
│  ¿Cómo se almacena? Usando el dominio especial .in-addr.arpa│
│  La IP se invierte y se añade .in-addr.arpa:                 │
│                                                              │
│  93.184.216.34                                               │
│  → 34.216.184.93.in-addr.arpa.  IN  PTR  www.example.com.   │
│    ──────────────                                            │
│    IP invertida                                              │
│                                                              │
│  ¿Por qué invertida?                                         │
│  DNS busca de derecha a izquierda (de general a específico). │
│  Las IPs van de general a específico de izquierda a derecha. │
│  Invertir la IP alinea ambos sistemas:                       │
│    93        → red (general)                                 │
│    .184.216  → subred                                        │
│    .34       → host (específico)                             │
│  Se convierte en:                                            │
│    34.216.184.93.in-addr.arpa.                               │
│    host → subred → red (ahora de izquierda a derecha,        │
│    alineado con la búsqueda DNS)                             │
└──────────────────────────────────────────────────────────────┘
```

### IPv6 inversa

Para IPv6, se usa `ip6.arpa` y cada nibble (4 bits) es un nivel:

```
# IPv6: 2001:0db8:0000:0000:0000:0000:0000:0001
# → Se expande completamente, se invierte nibble por nibble:
# 1.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.8.b.d.0.1.0.0.2.ip6.arpa.
```

### Uso práctico de PTR

```
┌──────────────────────────────────────────────────────────────┐
│  ¿Quién configura los PTR?                                   │
│  El propietario del bloque de IPs (normalmente el ISP        │
│  o el proveedor de hosting). NO el propietario del dominio.  │
│                                                              │
│  ¿Para qué sirve PTR?                                        │
│  • Verificación de email: muchos servidores de correo        │
│    rechazan email si la IP del emisor no tiene PTR           │
│    o si el PTR no coincide con el HELO/EHLO.                │
│  • Logs legibles: los servidores web pueden mostrar          │
│    nombres en vez de IPs en los logs.                        │
│  • Diagnóstico: traceroute muestra nombres de routers.       │
│  • Seguridad: verificación básica anti-spoofing.             │
└──────────────────────────────────────────────────────────────┘
```

```bash
# Consultar PTR (resolución inversa)
dig -x 93.184.216.34
# Equivale a:
dig 34.216.184.93.in-addr.arpa. PTR

# Más rápido:
dig -x 8.8.8.8 +short
# dns.google.

# Con host (más simple):
host 8.8.8.8
# 8.8.8.8.in-addr.arpa domain name pointer dns.google.
```

---

## Registro TXT

TXT almacena texto arbitrario asociado a un dominio. Originalmente pensado para información legible por humanos, ahora es fundamental para verificación y seguridad:

```
example.com.  3600  IN  TXT  "v=spf1 include:_spf.google.com ~all"
```

### Usos principales de TXT

```
┌──────────────────────────────────────────────────────────────┐
│  1. SPF (Sender Policy Framework)                            │
│     Indica qué servidores pueden enviar email por tu dominio │
│                                                              │
│     example.com.  TXT  "v=spf1 include:_spf.google.com ~all"│
│                                                              │
│     v=spf1             → versión SPF                         │
│     include:_spf.google.com → Google puede enviar por mí    │
│     ~all               → softfail para todos los demás       │
│     -all               → hardfail (rechazar definitivamente) │
│                                                              │
│  2. DKIM (DomainKeys Identified Mail)                        │
│     Clave pública para verificar firmas de email             │
│                                                              │
│     selector._domainkey.example.com.  TXT  "v=DKIM1; k=rsa; │
│       p=MIGfMA0GCSqGSIb3DQEBAQUAA..."                       │
│                                                              │
│  3. DMARC (Domain-based Message Authentication)              │
│     Política de autenticación de email                       │
│                                                              │
│     _dmarc.example.com.  TXT  "v=DMARC1; p=reject;          │
│       rua=mailto:dmarc@example.com"                          │
│                                                              │
│  4. Verificación de propiedad                                │
│     Google, Microsoft, Let's Encrypt piden añadir un TXT     │
│     para demostrar que controlas el dominio:                 │
│                                                              │
│     example.com.  TXT  "google-site-verification=abc123..."  │
│                                                              │
│  5. ACME (Let's Encrypt) DNS-01 challenge                    │
│     _acme-challenge.example.com.  TXT  "token-aleatorio"    │
└──────────────────────────────────────────────────────────────┘
```

### SPF, DKIM, DMARC: la tríada de email

```
┌──────────────────────────────────────────────────────────────┐
│              Autenticación de email                          │
│                                                              │
│  SPF:   ¿QUIÉN puede enviar email por este dominio?          │
│         (lista de IPs/servidores autorizados)                │
│                                                              │
│  DKIM:  ¿El email fue MODIFICADO en tránsito?                │
│         (firma criptográfica en cabeceras del email)         │
│                                                              │
│  DMARC: ¿Qué hacer si SPF o DKIM fallan?                    │
│         (p=none: no hacer nada, p=quarantine: spam,          │
│          p=reject: rechazar)                                 │
│                                                              │
│  Sin estos tres registros TXT, tu email tiene alta           │
│  probabilidad de ir a spam o ser rechazado.                  │
└──────────────────────────────────────────────────────────────┘
```

```bash
# Consultar registros TXT
dig example.com TXT +short
# "v=spf1 -all"
# "wgyf8z8..."

# Ver SPF de un dominio
dig google.com TXT +short | grep spf
# "v=spf1 include:_spf.google.com ~all"

# Ver DMARC
dig _dmarc.google.com TXT +short
# "v=DMARC1; p=reject; ..."
```

---

## Registro SRV

SRV (Service) permite descubrimiento de servicios: indica el host y puerto de un servicio específico:

```
_service._protocol.name.  TTL  IN  SRV  priority  weight  port  target

_sip._tcp.example.com.  3600  IN  SRV  10  60  5060  sip1.example.com.
_sip._tcp.example.com.  3600  IN  SRV  10  40  5060  sip2.example.com.
_sip._tcp.example.com.  3600  IN  SRV  20   0  5060  sip3.backup.com.
```

```
┌──────────────────────────────────────────────────────────────┐
│  _sip._tcp.example.com.  SRV  10  60  5060  sip1.example.com.
│  ─────────────────────        ──  ──  ────  ─────────────────
│  │                            │   │   │     │
│  │                            │   │   │     └── Target (servidor)
│  │                            │   │   └── Puerto del servicio
│  │                            │   └── Weight (balanceo entre misma priority)
│  │                            └── Priority (menor = preferido)
│  └── _servicio._protocolo.dominio                            │
│                                                              │
│  Priority: funciona como MX (menor = intentar primero)       │
│  Weight: entre servidores de misma prioridad, distribuir      │
│    proporcionalmente (60% a sip1, 40% a sip2)               │
│  Puerto: el servicio escucha en este puerto                  │
└──────────────────────────────────────────────────────────────┘
```

### Usos comunes de SRV

| Servicio | Registro | Ejemplo |
|----------|----------|---------|
| LDAP | `_ldap._tcp.example.com` | Active Directory |
| Kerberos | `_kerberos._tcp.example.com` | Autenticación |
| SIP (VoIP) | `_sip._tcp.example.com` | Telefonía |
| XMPP (chat) | `_xmpp-server._tcp.example.com` | Jabber |
| Minecraft | `_minecraft._tcp.example.com` | Game server |

> **Limitación**: HTTP no usa SRV (históricamente). Los navegadores no consultan `_http._tcp.example.com`. Esto obliga a usar los puertos estándar 80/443 o redirecciones.

```bash
# Consultar SRV
dig _sip._tcp.example.com SRV +short
# 10 60 5060 sip1.example.com.
# 10 40 5060 sip2.example.com.

# SRV de un servidor LDAP (Active Directory)
dig _ldap._tcp.corp.example.com SRV +short
```

---

## Otros registros relevantes

### CAA (Certificate Authority Authorization)

Indica qué autoridades de certificación (CA) pueden emitir certificados TLS para tu dominio:

```
example.com.  3600  IN  CAA  0  issue  "letsencrypt.org"
example.com.  3600  IN  CAA  0  issuewild  "letsencrypt.org"
example.com.  3600  IN  CAA  0  iodef  "mailto:security@example.com"
```

| Tag | Significado |
|-----|-------------|
| `issue` | CAs autorizadas para certificados normales |
| `issuewild` | CAs autorizadas para certificados wildcard |
| `iodef` | Email/URL para reportar violaciones |

```bash
dig example.com CAA +short
# 0 issue "letsencrypt.org"
```

### SSHFP (SSH Fingerprint)

Publica la huella digital de la clave SSH del servidor en DNS, permitiendo verificación automática:

```
server.example.com.  3600  IN  SSHFP  2  1  123456789abcdef...
#                                      │  │  │
#                                      │  │  └── Fingerprint
#                                      │  └── Algorithm (1=SHA-1, 2=SHA-256)
#                                      └── Key type (1=RSA, 2=DSA, 3=ECDSA, 4=Ed25519)
```

### HTTPS/SVCB (Service Binding)

Registros modernos (RFC 9460) que combinan la funcionalidad de A/AAAA con metadatos del servicio:

```
example.com.  300  IN  HTTPS  1  .  alpn="h2,h3" ipv4hint=93.184.216.34
```

Permiten al cliente saber de antemano que el servidor soporta HTTP/2 y HTTP/3, reduciendo RTTs de negociación. Aún en adopción temprana.

---

## Errores comunes

### 1. Poner CNAME en el apex del dominio

```
# INCORRECTO:
example.com.    CNAME    myapp.herokuapp.com.
# → Conflicto con SOA y NS que siempre existen en el apex

# CORRECTO:
www.example.com.   CNAME    myapp.herokuapp.com.
# o usar ALIAS/ANAME si tu proveedor lo soporta
```

### 2. MX apuntando a un CNAME

```
# INCORRECTO:
example.com.          MX    10    mail.example.com.
mail.example.com.     CNAME       mail.google.com.
# → La RFC prohíbe que MX apunte a un CNAME

# CORRECTO:
example.com.          MX    10    aspmx.l.google.com.
# → MX apunta directamente al nombre final
```

### 3. Olvidar el trailing dot en archivos de zona

```
# En un archivo de zona para example.com:

# INCORRECTO:
www     A      93.184.216.34           ← OK (relativo)
mail    CNAME  mail.google.com         ← ERROR
# → Se interpreta como: mail.google.com.example.com.

# CORRECTO:
mail    CNAME  mail.google.com.        ← con trailing dot
# → Se interpreta como: mail.google.com. (FQDN)

# Regla: si el valor es un nombre DNS externo al dominio,
# SIEMPRE usar trailing dot.
```

### 4. No configurar SPF/DKIM/DMARC

```
# Sin estos registros TXT, tu dominio:
# • Los emails que envías van a spam frecuentemente
# • Cualquiera puede enviar email "desde" tu dominio (spoofing)
# • Proveedores como Gmail y Microsoft rechazan tus emails

# Mínimo viable:
example.com.          TXT  "v=spf1 include:_spf.google.com -all"
_dmarc.example.com.   TXT  "v=DMARC1; p=quarantine"
# + Configurar DKIM en tu proveedor de email
```

### 5. Confundir la dirección de PTR con A

```
# A: nombre → IP     (tú lo configuras en tu zona DNS)
# PTR: IP → nombre   (lo configura el dueño del bloque de IPs)

# Si alquilas un VPS con IP 203.0.113.50:
# • Tú configuras: server.example.com  A  203.0.113.50
# • Tu proveedor de VPS configura: 50.113.0.203.in-addr.arpa  PTR  server.example.com

# A y PTR son independientes. Puedes tener A sin PTR y viceversa.
# Pero para email, AMBOS deben coincidir (forward-confirmed reverse DNS).
```

---

## Cheatsheet

```
┌──────────────────────────────────────────────────────────────┐
│              Tipos de registro DNS Cheatsheet                │
├──────────────────────────────────────────────────────────────┤
│                                                              │
│  FORMATO: NAME  TTL  IN  TYPE  RDATA                         │
│                                                              │
│  REGISTROS DE DIRECCIÓN:                                     │
│    A       nombre → IPv4        (www → 93.184.216.34)        │
│    AAAA    nombre → IPv6        (www → 2606:2800:...)        │
│    Múltiples A/AAAA = round-robin / redundancia              │
│                                                              │
│  ALIAS:                                                      │
│    CNAME   alias → nombre canónico                           │
│    NO en apex (conflicto SOA/NS)                             │
│    NO coexiste con otros registros en el mismo nombre        │
│    NO como destino de MX                                     │
│                                                              │
│  EMAIL:                                                      │
│    MX      dominio → servidor de correo + prioridad          │
│    Menor prioridad = preferido (10 antes que 20)             │
│    Misma prioridad = balanceo de carga                       │
│    TXT(SPF)  quién puede enviar email por tu dominio         │
│    TXT(DKIM) clave pública para firma de email               │
│    TXT(DMARC) política si SPF/DKIM fallan                    │
│                                                              │
│  DELEGACIÓN:                                                 │
│    NS      zona → servidores autoritativos (mínimo 2)        │
│    SOA     metadatos de zona (serial, refresh, expire...)    │
│      MNAME: primary NS                                       │
│      RNAME: admin email (. en vez de @)                      │
│      Serial: YYYYMMDDNN (incrementar con cada cambio)        │
│      Minimum: TTL para negative cache (NXDOMAIN)            │
│                                                              │
│  INVERSA:                                                    │
│    PTR     IP → nombre (en .in-addr.arpa / .ip6.arpa)        │
│    IP invertida: 93.184.216.34 → 34.216.184.93.in-addr.arpa │
│    Configura el dueño del bloque IP, no del dominio          │
│    Crítico para email (FCrDNS)                               │
│                                                              │
│  TEXTO:                                                      │
│    TXT     texto arbitrario (SPF, DKIM, DMARC, verificación)│
│                                                              │
│  SERVICIOS:                                                  │
│    SRV     _servicio._proto  priority weight port target     │
│    Descubrimiento de servicios (LDAP, SIP, Kerberos)         │
│    HTTP no usa SRV (limitación histórica)                    │
│                                                              │
│  SEGURIDAD:                                                  │
│    CAA     qué CAs pueden emitir certificados TLS            │
│    SSHFP   huella SSH del servidor                           │
│                                                              │
│  CONSULTAS CON DIG:                                          │
│    dig dominio TYPE                consultar tipo específico  │
│    dig dominio TYPE +short         solo el valor             │
│    dig -x IP                       resolución inversa (PTR)  │
│    dig @servidor dominio TYPE      preguntar a NS específico │
│                                                              │
└──────────────────────────────────────────────────────────────┘
```

---

## Ejercicios

### Ejercicio 1: Interpretar registros DNS

Dada esta zona DNS simplificada para `acme.com`:

```
acme.com.           86400   IN  SOA   ns1.acme.com. hostmaster.acme.com. (
                                      2026032101 7200 3600 1209600 3600 )
acme.com.           86400   IN  NS    ns1.acme.com.
acme.com.           86400   IN  NS    ns2.acme.com.
acme.com.           3600    IN  A     203.0.113.10
acme.com.           3600    IN  MX    10  mail1.acme.com.
acme.com.           3600    IN  MX    20  mail2.acme.com.
acme.com.           3600    IN  TXT   "v=spf1 ip4:203.0.113.0/24 -all"
www.acme.com.       3600    IN  CNAME acme.com.
mail1.acme.com.     3600    IN  A     203.0.113.20
mail2.acme.com.     3600    IN  A     203.0.113.21
api.acme.com.       300     IN  A     203.0.113.50
ns1.acme.com.       86400   IN  A     203.0.113.1
ns2.acme.com.       86400   IN  A     203.0.113.2
```

**Responde**:
1. ¿Cuál es el email del administrador de la zona? (Convierte el formato DNS a email.)
2. Si envío un email a `user@acme.com`, ¿a qué servidor se entregará primero? ¿Qué IP tiene?
3. ¿Qué IPs están autorizadas a enviar email por `acme.com` según el SPF?
4. ¿Por qué `api.acme.com` tiene un TTL de 300 mientras que `acme.com` tiene 3600? ¿Qué sugiere esto operativamente?
5. Si `www.acme.com` es un CNAME a `acme.com`, ¿cuál es la IP final de `www.acme.com`? ¿Cuántas consultas DNS se necesitan?
6. Si alguien consulta `noexiste.acme.com`, ¿cuánto tiempo se cacheará la respuesta NXDOMAIN?

**Pregunta de reflexión**: El registro `www.acme.com CNAME acme.com` es técnicamente válido aquí. ¿Sería válido al revés (`acme.com CNAME www.acme.com`)? ¿Por qué o por qué no?

---

### Ejercicio 2: Diseñar registros DNS

Estás configurando DNS para una startup llamada `devtools.io`. Necesitas:

1. El sitio web principal en `devtools.io` apuntando a IP `198.51.100.10`.
2. `www.devtools.io` como alias del apex.
3. La API en `api.devtools.io` detrás de un ALB de AWS: `myalb-123.us-east-1.elb.amazonaws.com`.
4. La documentación en `docs.devtools.io` hospedada en Read the Docs: `devtools.readthedocs.io`.
5. Email gestionado por Google Workspace (servidores MX de Google).
6. SPF que autorice a Google a enviar email.
7. Verificación de propiedad de Google: token `google-site-verification=ABCDEF123`.
8. Solo Let's Encrypt puede emitir certificados TLS.

Escribe todos los registros DNS necesarios. Para cada uno, indica qué tipo de registro usas y por qué.

**Pregunta de reflexión**: ¿Podrías usar CNAME para `api.devtools.io`? ¿Y para `devtools.io`? ¿Cuál es la diferencia?

---

### Ejercicio 3: Investigar registros reales

Usa `dig` para investigar los registros DNS de `google.com`:

```bash
# Ejecuta estos comandos:
dig google.com A +short
dig google.com AAAA +short
dig google.com MX +short
dig google.com NS +short
dig google.com TXT +short
dig google.com SOA
dig -x 8.8.8.8 +short
dig google.com CAA +short
```

**Responde**:
1. ¿Cuántos registros A tiene `google.com`? ¿Por qué crees que tiene más de uno?
2. ¿Qué revela el registro SOA sobre la gestión de la zona? (Serial, Minimum TTL, email de contacto.)
3. ¿Qué servidores de email reciben correo para `google.com`? ¿Cuántos niveles de prioridad hay?
4. ¿Qué dice el SPF de `google.com`? ¿Usa `include` o lista IPs directamente?
5. ¿Qué PTR tiene `8.8.8.8`? ¿Coincide con el nombre que esperabas?
6. ¿Tiene `google.com` registros CAA? Si sí, ¿qué CAs están autorizadas?

**Pregunta de reflexión**: Si `google.com` tiene 6 registros A con IPs diferentes, ¿cómo decide tu sistema operativo a cuál conectarse? ¿Siempre es la misma IP? Haz la consulta dos veces y compara el orden.

---

> **Siguiente tema**: T03 — Resolución DNS (recursiva vs iterativa, caching, TTL)
