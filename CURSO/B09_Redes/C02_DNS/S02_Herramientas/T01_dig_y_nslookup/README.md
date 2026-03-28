# dig y nslookup

## Índice
1. [dig: la herramienta de referencia](#dig-la-herramienta-de-referencia)
2. [Anatomía de la salida de dig](#anatomía-de-la-salida-de-dig)
3. [Opciones esenciales de dig](#opciones-esenciales-de-dig)
4. [Consultas avanzadas con dig](#consultas-avanzadas-con-dig)
5. [dig +trace: trazar la resolución](#dig-trace-trazar-la-resolución)
6. [dig para diagnóstico de email](#dig-para-diagnóstico-de-email)
7. [nslookup: la alternativa clásica](#nslookup-la-alternativa-clásica)
8. [dig vs nslookup: cuándo usar cada uno](#dig-vs-nslookup-cuándo-usar-cada-uno)
9. [Errores comunes](#errores-comunes)
10. [Cheatsheet](#cheatsheet)
11. [Ejercicios](#ejercicios)

---

## dig: la herramienta de referencia

`dig` (Domain Information Groper) es la herramienta estándar para consultas DNS en Linux. A diferencia de `getent hosts` (que usa nsswitch.conf y /etc/hosts), dig consulta **exclusivamente DNS** — envía paquetes al resolver y muestra la respuesta cruda:

```bash
# Instalación (si no está disponible)
sudo dnf install bind-utils    # Fedora/RHEL
sudo apt install dnsutils      # Debian/Ubuntu
```

### Sintaxis general

```
dig [@servidor] nombre [tipo] [opciones]

dig                www.example.com              # A record (default)
dig                www.example.com  AAAA        # AAAA record
dig  @8.8.8.8     www.example.com  MX          # MX, preguntando a 8.8.8.8
dig  @ns1.example.com  example.com  SOA  +short # SOA, salida compacta
```

Si no se especifica `@servidor`, dig usa los nameservers de `/etc/resolv.conf`.
Si no se especifica tipo, consulta `A` por defecto.

---

## Anatomía de la salida de dig

```bash
dig www.example.com
```

```
┌──────────────────────────────────────────────────────────────┐
│  ; <<>> DiG 9.18.24 <<>> www.example.com                     │
│  ;; global options: +cmd                                     │
│  ──────────────────────────────────── Versión y comando      │
│                                                              │
│  ;; Got answer:                                              │
│  ;; ->>HEADER<<- opcode: QUERY, status: NOERROR, id: 41523  │
│  ;; flags: qr rd ra; QUERY: 1, ANSWER: 1, AUTHORITY: 0,     │
│  ;;                  ADDITIONAL: 1                           │
│  ──────────────────────────────────── Header                 │
│  │                                                           │
│  │  status: NOERROR    → consulta exitosa                    │
│  │  flags: qr rd ra    → es respuesta, recursión pedida      │
│  │                       y disponible                        │
│  │  QUERY: 1           → 1 pregunta                          │
│  │  ANSWER: 1          → 1 registro en la respuesta          │
│  │  AUTHORITY: 0       → 0 NS en authority section           │
│  │  ADDITIONAL: 1      → 1 registro adicional (OPT/EDNS)    │
│                                                              │
│  ;; OPT PSEUDOSECTION:                                       │
│  ; EDNS: version: 0, flags:; udp: 1232                       │
│  ──────────────────────────────────── EDNS info              │
│  │  udp: 1232 → tamaño máximo UDP soportado                 │
│                                                              │
│  ;; QUESTION SECTION:                                        │
│  ;www.example.com.              IN      A                    │
│  ──────────────────────────────────── Lo que preguntaste     │
│                                                              │
│  ;; ANSWER SECTION:                                          │
│  www.example.com.       86400   IN      A       93.184.216.34│
│  ──────────────────────────────────── La respuesta           │
│  │  nombre          TTL    clase  tipo   datos               │
│                                                              │
│  ;; Query time: 45 msec                                      │
│  ;; SERVER: 192.168.1.1#53(192.168.1.1) (UDP)               │
│  ;; WHEN: Fri Mar 21 10:30:00 CET 2026                      │
│  ;; MSG SIZE  rcvd: 58                                       │
│  ──────────────────────────────────── Estadísticas           │
│  │  Query time: latencia de la consulta                      │
│  │  SERVER: qué resolver respondió                           │
│  │  MSG SIZE: tamaño del paquete de respuesta                │
└──────────────────────────────────────────────────────────────┘
```

### Qué buscar en cada sección

| Sección | Información clave |
|---------|-------------------|
| HEADER | `status` (NOERROR/NXDOMAIN/SERVFAIL), flags (`aa` = autoritativo) |
| QUESTION | Confirmar que preguntaste lo correcto |
| ANSWER | Los registros encontrados, con TTL |
| AUTHORITY | NS del dominio (en referrals o respuestas completas) |
| ADDITIONAL | IPs de los NS (glue records) |
| Footer | Qué servidor respondió, latencia, protocolo (UDP/TCP) |

---

## Opciones esenciales de dig

### Control de salida

```bash
# Solo el valor (sin decoración)
dig +short www.example.com
# 93.184.216.34

# Solo el valor de un tipo específico
dig +short google.com MX
# 10 smtp.google.com.
# 20 smtp2.google.com.

# Sin comentarios (solo datos)
dig +nocomments www.example.com

# Sin todo excepto la respuesta
dig +noall +answer www.example.com
# www.example.com.    86400  IN  A  93.184.216.34

# Sin todo excepto answer + authority
dig +noall +answer +authority example.com NS

# Modo ultra-compacto (desactivar todo lo extra)
dig +nocmd +noall +answer +noquestion www.example.com
```

### Selección de servidor

```bash
# Consultar un servidor específico
dig @8.8.8.8 www.example.com
dig @1.1.1.1 www.example.com

# Consultar directamente al autoritativo
dig @ns1.example.com www.example.com
# La respuesta incluirá flag aa (authoritative answer)

# Comparar respuestas entre servidores
dig @8.8.8.8 +short example.com A
dig @1.1.1.1 +short example.com A
# Si difieren: posible problema de propagación o caché
```

### Selección de tipo de registro

```bash
dig example.com A          # IPv4
dig example.com AAAA       # IPv6
dig example.com MX         # Mail servers
dig example.com NS         # Name servers
dig example.com TXT        # Registros de texto
dig example.com SOA        # Start of Authority
dig example.com CNAME      # Alias
dig example.com CAA        # Certificate Authority Authorization
dig example.com SRV        # Service records
dig example.com ANY        # Todos (muchos servidores lo bloquean)

# Resolución inversa (PTR)
dig -x 8.8.8.8
# Equivale a: dig 8.8.8.8.in-addr.arpa PTR
```

### Protocolo y transporte

```bash
# Forzar TCP (en vez del UDP default)
dig +tcp www.example.com

# Usar un puerto diferente
dig -p 5353 @127.0.0.1 www.example.com   # ej. DNS en puerto no estándar

# Ver el tamaño del paquete
dig www.example.com | grep "MSG SIZE"
# ;; MSG SIZE  rcvd: 58

# Configurar tamaño EDNS
dig +bufsize=4096 www.example.com
```

---

## Consultas avanzadas con dig

### Consultar múltiples registros a la vez

```bash
# Archivo con múltiples consultas
cat << 'EOF' > /tmp/queries.txt
www.example.com A
example.com MX
example.com NS
example.com TXT
EOF

dig -f /tmp/queries.txt +short
```

### Consultas con opciones de caché

```bash
# Ver si la respuesta viene del caché (flag aa ausente)
dig www.example.com | grep flags
# ;; flags: qr rd ra;     ← sin aa = viene del caché
# ;; flags: qr aa rd ra;  ← con aa = autoritativa

# Comparar TTL para detectar caché
dig www.example.com | grep "ANSWER SECTION" -A1
# www.example.com.    82341  IN  A  93.184.216.34
#                     ─────
# 82341 < 86400 → el TTL decrementó → viene del caché
# Si el TTL coincide con el original → respuesta fresca
```

### DNSSEC

```bash
# Ver información DNSSEC
dig +dnssec example.com A
# Añade registros RRSIG (firmas)

# Verificar validación DNSSEC
dig +dnssec +cd example.com A    # CD=1: deshabilitar validación
dig +dnssec example.com A        # validación normal
# Comparar: si una funciona y otra no → problema de DNSSEC

# Ver el registro DS (Delegation Signer)
dig example.com DS

# Ver DNSKEY (claves públicas de la zona)
dig example.com DNSKEY
```

### Consulta de registro SOA detallada

```bash
dig example.com SOA +multiline
```

```
# Salida formateada:
# example.com.    86400  IN  SOA  ns1.example.com. admin.example.com. (
#                              2026032101 ; serial
#                              7200       ; refresh (2 hours)
#                              3600       ; retry (1 hour)
#                              1209600    ; expire (2 weeks)
#                              86400      ; minimum (1 day)
#                              )
#
# +multiline formatea el SOA legiblemente con comentarios
```

### Medir tiempos

```bash
# Tiempo de consulta
dig www.example.com | grep "Query time"
# ;; Query time: 45 msec

# Comparar tiempos entre resolvers
dig @8.8.8.8 www.example.com | grep "Query time"
dig @1.1.1.1 www.example.com | grep "Query time"
dig @9.9.9.9 www.example.com | grep "Query time"

# Primera consulta vs segunda (efecto del caché)
dig @8.8.8.8 uncommon-domain.example.com | grep "Query time"
# ;; Query time: 120 msec  ← primera vez, sin caché
dig @8.8.8.8 uncommon-domain.example.com | grep "Query time"
# ;; Query time: 1 msec    ← segunda vez, cacheado
```

---

## dig +trace: trazar la resolución

`+trace` hace que dig simule el proceso de resolución iterativa completo, consultando root → TLD → autoritativo:

```bash
dig +trace www.example.com
```

```
┌──────────────────────────────────────────────────────────────┐
│  ; <<>> DiG 9.18.24 <<>> +trace www.example.com             │
│                                                              │
│  ─── Paso 1: Root servers ───                                │
│  .          518400  IN  NS  a.root-servers.net.              │
│  .          518400  IN  NS  b.root-servers.net.              │
│  .          518400  IN  NS  ...                              │
│  ;; Received 525 bytes from 192.168.1.1#53 in 12 ms         │
│  ↑ Tu resolver local proporcionó la lista de root servers    │
│                                                              │
│  ─── Paso 2: TLD (.com) ───                                  │
│  com.       172800  IN  NS  a.gtld-servers.net.              │
│  com.       172800  IN  NS  b.gtld-servers.net.              │
│  com.       172800  IN  NS  ...                              │
│  ;; Received 1170 bytes from 198.41.0.4#53(a.root) in 25 ms │
│  ↑ El root server a.root-servers.net devolvió los NS de .com │
│                                                              │
│  ─── Paso 3: Autoritativo de example.com ───                 │
│  example.com.  172800  IN  NS  ns1.example.com.              │
│  example.com.  172800  IN  NS  ns2.example.com.              │
│  ;; Received 772 bytes from 192.5.6.30#53(a.gtld) in 30 ms  │
│  ↑ El TLD de .com devolvió los NS de example.com             │
│                                                              │
│  ─── Paso 4: Respuesta final ───                             │
│  www.example.com. 86400  IN  A  93.184.216.34                │
│  ;; Received 56 bytes from 93.184.216.1#53(ns1.example) 40ms│
│  ↑ El autoritativo devolvió la IP                            │
└──────────────────────────────────────────────────────────────┘
```

### Cuándo usar +trace

```
┌──────────────────────────────────────────────────────────────┐
│  Usar +trace para:                                           │
│                                                              │
│  • Verificar que la delegación es correcta                   │
│    (¿los NS en .com apuntan al servidor correcto?)           │
│                                                              │
│  • Identificar dónde se rompe la cadena de resolución        │
│    (si falla en paso 3, el problema está en los NS del TLD)  │
│                                                              │
│  • Confirmar que un cambio DNS llegó al autoritativo          │
│    (aunque los resolvers aún tengan el valor viejo cacheado) │
│                                                              │
│  • Diagnosticar problemas de DNSSEC                          │
│    (dig +trace +dnssec muestra las firmas en cada paso)      │
│                                                              │
│  Nota: +trace BYPASEA el caché de tu resolver.               │
│  Consulta directamente a root, TLD, y autoritativo.          │
│  Útil para verificar el estado ACTUAL sin caché.             │
└──────────────────────────────────────────────────────────────┘
```

---

## dig para diagnóstico de email

Los problemas de email frecuentemente son problemas de DNS. dig es esencial para diagnosticarlos:

### Verificar MX

```bash
# ¿Qué servidores reciben email para example.com?
dig example.com MX +short
# 10 mail1.example.com.
# 20 mail2.example.com.

# Verificar que los MX resuelven a IPs
dig mail1.example.com A +short
# 203.0.113.20

# MX NO debe apuntar a CNAME — verificar
dig mail1.example.com CNAME +short
# (debe estar vacío)
```

### Verificar SPF

```bash
# ¿Quién puede enviar email por example.com?
dig example.com TXT +short | grep spf
# "v=spf1 include:_spf.google.com -all"

# Seguir los includes recursivamente
dig _spf.google.com TXT +short
# "v=spf1 include:_netblocks.google.com include:_netblocks2..."

dig _netblocks.google.com TXT +short
# "v=spf1 ip4:35.190.247.0/24 ip4:64.233.160.0/19 ..."
# ↑ Las IPs finales autorizadas para enviar email
```

### Verificar DKIM

```bash
# Selector DKIM (el selector depende del proveedor)
dig google._domainkey.example.com TXT +short
# "v=DKIM1; k=rsa; p=MIGfMA0GCSqGSIb3DQEBAQUAA..."
```

### Verificar DMARC

```bash
dig _dmarc.example.com TXT +short
# "v=DMARC1; p=reject; rua=mailto:dmarc@example.com"
# p=reject: rechazar email que falle SPF+DKIM
```

### Verificar PTR (resolución inversa)

```bash
# ¿El servidor de email tiene PTR correcto?
dig -x 203.0.113.20 +short
# mail1.example.com.

# Forward-confirmed reverse DNS (FCrDNS):
# La IP resuelve al nombre Y el nombre resuelve a la IP
dig -x 203.0.113.20 +short         # → mail1.example.com
dig mail1.example.com A +short     # → 203.0.113.20
# ¡Coinciden! FCrDNS es válido.
```

### Checklist completo de email DNS

```bash
DOMAIN="example.com"

echo "=== MX ==="
dig $DOMAIN MX +short

echo "=== SPF ==="
dig $DOMAIN TXT +short | grep spf

echo "=== DMARC ==="
dig _dmarc.$DOMAIN TXT +short

echo "=== PTR del primer MX ==="
MX_HOST=$(dig $DOMAIN MX +short | sort -n | head -1 | awk '{print $2}')
MX_IP=$(dig $MX_HOST A +short | head -1)
echo "MX: $MX_HOST → IP: $MX_IP"
dig -x $MX_IP +short
```

---

## nslookup: la alternativa clásica

`nslookup` es una herramienta más antigua y simple que dig. Está disponible en prácticamente todos los sistemas operativos (incluidos Windows y macOS):

### Modo no interactivo

```bash
# Consulta simple
nslookup www.example.com
# Server:    192.168.1.1
# Address:   192.168.1.1#53
#
# Non-authoritative answer:
# Name:      www.example.com
# Address:   93.184.216.34

# Consultar tipo específico
nslookup -type=MX example.com
nslookup -type=NS example.com
nslookup -type=TXT example.com
nslookup -type=SOA example.com

# Consultar un servidor específico
nslookup www.example.com 8.8.8.8

# Resolución inversa
nslookup 8.8.8.8
```

### Modo interactivo

```bash
nslookup
# Entra en modo interactivo:
> server 8.8.8.8           # cambiar servidor DNS
> set type=MX              # cambiar tipo de consulta
> example.com              # consultar
> set type=A               # volver a tipo A
> www.example.com          # consultar
> set debug                # activar modo debug (verbose)
> www.example.com          # consultar con debug
> exit                     # salir
```

### Interpretar la salida de nslookup

```
┌──────────────────────────────────────────────────────────────┐
│  $ nslookup www.example.com                                  │
│                                                              │
│  Server:    192.168.1.1          ← quién respondió           │
│  Address:   192.168.1.1#53      ← IP y puerto del resolver   │
│                                                              │
│  Non-authoritative answer:       ← viene del caché (no aa)   │
│  Name:      www.example.com      ← nombre consultado         │
│  Address:   93.184.216.34        ← la IP                     │
│                                                              │
│  "Non-authoritative" = la respuesta viene del caché de un    │
│  resolver recursivo, no directamente del autoritativo.       │
│  Esto es NORMAL para la mayoría de consultas.                │
│                                                              │
│  "Authoritative" aparece cuando consultas directamente       │
│  al servidor autoritativo del dominio.                       │
└──────────────────────────────────────────────────────────────┘
```

### nslookup con debug

```bash
nslookup -debug www.example.com
```

```
# Salida verbose que muestra:
# - El paquete DNS enviado (header, question)
# - El paquete DNS recibido (header, answer, authority, additional)
# - TTL, clase, tipo de cada registro
# - Flags del header (similar a dig pero menos detallado)
```

---

## dig vs nslookup: cuándo usar cada uno

```
┌─────────────────────┬────────────────────┬───────────────────┐
│                     │ dig                │ nslookup          │
├─────────────────────┼────────────────────┼───────────────────┤
│ Detalle de salida   │ Muy detallado      │ Básico            │
│ Secciones visibles  │ Todas (Q/A/Auth/   │ Solo answer       │
│                     │ Additional)        │                   │
│ Flags DNS           │ Sí (qr rd ra aa)   │ Solo auth/no-auth │
│ TTL visible         │ Sí                 │ Con -debug         │
│ +trace              │ Sí                 │ No                │
│ +short              │ Sí                 │ No                │
│ DNSSEC              │ +dnssec            │ Limitado          │
│ Batch queries       │ -f archivo         │ No (solo interactivo)│
│ Disponibilidad      │ Necesita bind-utils│ Casi universal    │
│ En Windows          │ No (nativo)        │ Sí                │
│ Recomendación       │ Diagnóstico serio  │ Consultas rápidas │
│ Scriptable          │ Muy (+short, +noall)│ Difícil de parsear│
└─────────────────────┴────────────────────┴───────────────────┘

Recomendación:
• Usar dig para diagnóstico y administración de sistemas
• Usar nslookup solo cuando dig no está disponible
  o para consultas rápidas casuales
• En scripts: siempre dig (salida predecible, +short parseble)
```

---

## Errores comunes

### 1. Confundir la salida de dig con la resolución del sistema

```bash
# dig SOLO consulta DNS. NO usa /etc/hosts ni nsswitch.conf.

# Si /etc/hosts tiene:
# 10.0.0.5  api.internal

dig api.internal +short
# (vacío — NXDOMAIN, no existe en DNS)

getent hosts api.internal
# 10.0.0.5  api.internal  (encontrado en /etc/hosts)

# La aplicación usará getent (nsswitch), no dig.
# Si dig falla pero getent funciona, NO hay problema de DNS.
```

### 2. Olvidar que +short puede devolver CNAME en vez de IP

```bash
dig +short www.example.com
# example.com.                 ← ¡esto es un CNAME, no la IP!

# Si el registro es CNAME, +short muestra el CNAME.
# Para obtener la IP final:
dig +short www.example.com A
# example.com.
# 93.184.216.34
# ↑ Muestra el CNAME Y luego la IP resolvida

# Para SOLO la IP cuando hay CNAME:
dig +short www.example.com A | tail -1
# 93.184.216.34
```

### 3. No especificar @ al diagnosticar problemas entre resolvers

```bash
# "El DNS no funciona" — pero ¿qué resolver?

# INCORRECTO: consultar solo tu resolver default
dig example.com    # usa /etc/resolv.conf

# CORRECTO: comparar múltiples resolvers
dig @192.168.1.1 example.com +short    # tu router
dig @8.8.8.8 example.com +short       # Google
dig @1.1.1.1 example.com +short       # Cloudflare

# Si tu router falla pero 8.8.8.8 funciona:
# → problema con el DNS del router, no con el dominio
```

### 4. Interpretar "Non-authoritative answer" como un error

```
# nslookup dice "Non-authoritative answer"
# Esto NO es un error. Significa:
#   La respuesta viene del caché de un resolver recursivo,
#   no directamente del servidor autoritativo.
#
# Esto es el comportamiento NORMAL.
# Si quieres una respuesta autoritativa:
nslookup www.example.com ns1.example.com
# o
dig @ns1.example.com www.example.com
```

### 5. Usar ANY esperando "todos los registros"

```bash
# dig example.com ANY
# Muchos servidores y resolvers BLOQUEAN consultas ANY
# (Cloudflare devuelve HINFO en vez de los registros reales)
# RFC 8482 permite que los servidores rechacen ANY

# En vez de ANY, consultar cada tipo individualmente:
dig example.com A
dig example.com AAAA
dig example.com MX
dig example.com NS
dig example.com TXT
```

---

## Cheatsheet

```
┌──────────────────────────────────────────────────────────────┐
│                dig & nslookup Cheatsheet                     │
├──────────────────────────────────────────────────────────────┤
│                                                              │
│  DIG BÁSICO:                                                 │
│    dig domain                   A record (default)           │
│    dig domain TYPE              tipo específico              │
│    dig @server domain           consultar resolver específico│
│    dig -x IP                    resolución inversa (PTR)     │
│                                                              │
│  DIG SALIDA:                                                 │
│    +short                       solo el valor                │
│    +noall +answer               solo la sección answer       │
│    +noall +answer +authority    answer + authority           │
│    +multiline                   SOA formateado legible       │
│    +nocmd +noall +answer        ultra-compacto               │
│                                                              │
│  DIG AVANZADO:                                               │
│    +trace                       trazar root→TLD→auth         │
│    +tcp                         forzar TCP                   │
│    +dnssec                      ver firmas DNSSEC            │
│    +cd                          deshabilitar validación DNSSEC│
│    -f archivo                   batch de consultas           │
│    -p puerto                    puerto no estándar           │
│                                                              │
│  DIG TIPOS:                                                  │
│    A AAAA MX NS TXT SOA CNAME PTR SRV CAA DNSKEY DS ANY     │
│                                                              │
│  DIG INTERPRETAR:                                            │
│    status: NOERROR    éxito (answer puede estar vacía)       │
│    status: NXDOMAIN   dominio no existe                      │
│    status: SERVFAIL   error del servidor                     │
│    flags: aa          respuesta autoritativa                 │
│    flags: rd ra       recursión pedida y disponible          │
│    Query time         latencia de la consulta                │
│    SERVER             qué resolver respondió                 │
│    TTL decrementando  respuesta desde caché                  │
│                                                              │
│  DIG EMAIL:                                                  │
│    dig domain MX +short               servidores de correo   │
│    dig domain TXT +short | grep spf   SPF                    │
│    dig _dmarc.domain TXT +short       DMARC                  │
│    dig sel._domainkey.domain TXT      DKIM                   │
│    dig -x IP +short                   PTR del mail server    │
│                                                              │
│  NSLOOKUP:                                                   │
│    nslookup domain                    consulta simple        │
│    nslookup -type=TYPE domain         tipo específico        │
│    nslookup domain server             resolver específico    │
│    nslookup IP                        resolución inversa     │
│    nslookup -debug domain             modo verbose           │
│                                                              │
│  COMPARAR:                                                   │
│    dig = detallado, scriptable, secciones, flags, +trace     │
│    nslookup = simple, universal, rápido, pero limitado       │
│    getent hosts = como el SO resuelve (incluye /etc/hosts)   │
│                                                              │
│  RECUERDA:                                                   │
│    dig NO consulta /etc/hosts (solo DNS)                     │
│    "Non-authoritative" NO es un error                        │
│    +short con CNAME puede mostrar el alias, no la IP         │
│    ANY está bloqueado en muchos servidores                    │
│    TTL < original → la respuesta viene del caché             │
│                                                              │
└──────────────────────────────────────────────────────────────┘
```

---

## Ejercicios

### Ejercicio 1: Interpretar salida de dig

Analiza esta salida completa y responde las preguntas:

```
; <<>> DiG 9.18.24 <<>> api.mycompany.com
;; global options: +cmd
;; Got answer:
;; ->>HEADER<<- opcode: QUERY, status: NOERROR, id: 38291
;; flags: qr rd ra; QUERY: 1, ANSWER: 2, AUTHORITY: 0, ADDITIONAL: 1

;; OPT PSEUDOSECTION:
; EDNS: version: 0, flags:; udp: 1232
;; QUESTION SECTION:
;api.mycompany.com.         IN      A

;; ANSWER SECTION:
api.mycompany.com.  247     IN      CNAME   lb-prod.us-east-1.elb.amazonaws.com.
lb-prod.us-east-1.elb.amazonaws.com. 42 IN A  52.20.150.33

;; Query time: 3 msec
;; SERVER: 127.0.0.53#53(127.0.0.53) (UDP)
;; WHEN: Fri Mar 21 10:30:00 CET 2026
;; MSG SIZE  rcvd: 112
```

**Responde**:
1. ¿`api.mycompany.com` tiene un registro A directo o es un CNAME? ¿A dónde apunta?
2. ¿La respuesta es autoritativa? ¿Cómo lo sabes?
3. El TTL de `api.mycompany.com` es 247. Si el TTL original era 300, ¿hace cuánto tiempo se cacheó este registro?
4. El TTL del registro A del ALB es 42. ¿Por qué AWS usa TTLs tan cortos para los load balancers?
5. ¿Qué resolver respondió? ¿Qué te dice `127.0.0.53`?
6. Query time es 3 msec. ¿Esto indica respuesta fresca o cacheada? ¿Por qué?
7. Si hicieras `dig +short api.mycompany.com`, ¿qué salida esperarías? (Pista: hay un CNAME.)

**Pregunta de reflexión**: ¿Por qué AWS ELB usa TTL cortos (~60s) mientras que un registro A estático podría tener TTL de 86400? ¿Qué tiene que ver con la arquitectura del balanceador?

---

### Ejercicio 2: Diagnóstico con dig

Ejecuta estos comandos contra dominios reales y analiza los resultados:

```bash
# Parte A: Comparar resolvers
dig @8.8.8.8 +short cloudflare.com A
dig @1.1.1.1 +short cloudflare.com A
dig @9.9.9.9 +short cloudflare.com A

# Parte B: Trazar la resolución
dig +trace cloudflare.com

# Parte C: Efecto del caché
dig cloudflare.com | grep "Query time"
dig cloudflare.com | grep "Query time"

# Parte D: Email records
dig cloudflare.com MX +short
dig cloudflare.com TXT +short | grep spf
dig _dmarc.cloudflare.com TXT +short
```

**Responde**:
1. ¿Los tres resolvers devuelven las mismas IPs? ¿En el mismo orden?
2. En el +trace, ¿cuántos pasos hay? ¿Qué root server y TLD server se usaron?
3. ¿Cuánto fue el Query time en la primera vs segunda consulta? ¿Por qué la diferencia?
4. ¿Cuántos servidores MX tiene cloudflare.com? ¿Qué prioridades?
5. ¿Qué dice el SPF — usa `include`, lista IPs directas, o ambas?
6. ¿Tiene política DMARC? ¿Es `none`, `quarantine`, o `reject`?

**Pregunta de reflexión**: Si haces `dig +trace` y uno de los pasos muestra `SERVFAIL`, ¿qué información tienes para diagnosticar? ¿Dónde buscarías el problema?

---

### Ejercicio 3: Script de auditoría DNS

Escribe un one-liner de bash (o una secuencia de comandos) que, dado un dominio como argumento, muestre:

1. Registros A (IPs)
2. Registros AAAA (IPv6)
3. Registros MX (servidores de correo)
4. Registros NS (nameservers)
5. SPF (filtrado de TXT)
6. DMARC
7. PTR de la primera IP del registro A

Ejemplo de uso esperado:
```bash
./dns-audit.sh example.com
```

Después de escribir el script, ejecútalo contra `google.com` y responde:
- ¿Cuántas IPs (A) devuelve?
- ¿Tiene IPv6?
- ¿El PTR de la primera IP coincide con `google.com`?
- ¿Su política DMARC es estricta?

**Pregunta de reflexión**: ¿Por qué `dig +short` es preferible a `nslookup` para scripts? ¿Qué problemas de parseo tendría un script que usa la salida de `nslookup`?

---

> **Siguiente tema**: T02 — host (consultas DNS simples)
