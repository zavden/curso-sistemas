# host

## Índice
1. [Qué es host](#qué-es-host)
2. [Consultas básicas](#consultas-básicas)
3. [Selección de tipo de registro](#selección-de-tipo-de-registro)
4. [Resolución inversa](#resolución-inversa)
5. [Opciones útiles](#opciones-útiles)
6. [host en scripts](#host-en-scripts)
7. [host vs dig vs nslookup](#host-vs-dig-vs-nslookup)
8. [Errores comunes](#errores-comunes)
9. [Cheatsheet](#cheatsheet)
10. [Ejercicios](#ejercicios)

---

## Qué es host

`host` es una herramienta DNS simple y concisa. Mientras que `dig` muestra toda la información posible y `nslookup` tiene un modo interactivo, `host` se enfoca en dar respuestas rápidas y legibles en una sola línea:

```bash
# Instalación (mismo paquete que dig)
sudo dnf install bind-utils    # Fedora/RHEL
sudo apt install dnsutils      # Debian/Ubuntu
```

```
┌──────────────────────────────────────────────────────────────┐
│  Comparación de salida para la misma consulta:               │
│                                                              │
│  $ dig www.example.com                                       │
│  ; <<>> DiG 9.18.24 <<>> www.example.com                     │
│  ;; global options: +cmd                                     │
│  ;; Got answer:                                              │
│  ;; ->>HEADER<<- opcode: QUERY, status: NOERROR, id: 41523  │
│  ;; flags: qr rd ra; QUERY: 1, ANSWER: 1, ...               │
│  ;; QUESTION SECTION:                                        │
│  ;www.example.com.            IN    A                        │
│  ;; ANSWER SECTION:                                          │
│  www.example.com.    86400    IN    A    93.184.216.34        │
│  ;; Query time: 45 msec                                      │
│  ;; SERVER: 192.168.1.1#53                                   │
│  ...                                    ← ~15 líneas         │
│                                                              │
│  $ host www.example.com                                      │
│  www.example.com has address 93.184.216.34                    │
│                                         ← 1 línea            │
└──────────────────────────────────────────────────────────────┘
```

`host` es ideal para consultas rápidas donde no necesitas flags DNS, TTL, secciones authority/additional, ni estadísticas de la consulta.

---

## Consultas básicas

### Sintaxis

```
host [-t tipo] [-a] [-v] nombre [servidor]
```

### Resolver un nombre

```bash
# Consulta por defecto: muestra A, AAAA y MX si existen
host example.com
# example.com has address 93.184.216.34
# example.com has IPv6 address 2606:2800:220:1:248:1893:25c8:1946
# example.com mail is handled by 0 .

host google.com
# google.com has address 142.250.185.206
# google.com has IPv6 address 2607:f8b0:4004:800::200e
# google.com mail is handled by 10 smtp.google.com.
# google.com mail is handled by 20 smtp2.google.com.
# ...
```

A diferencia de `dig` (que solo muestra A por defecto), `host` sin argumentos muestra automáticamente A, AAAA y MX — los tres registros más comunes.

### Consultar un servidor específico

```bash
# Usar Google DNS
host www.example.com 8.8.8.8
# Using domain server:
# Name: 8.8.8.8
# Address: 8.8.8.8#53
# Aliases:
#
# www.example.com has address 93.184.216.34

# Consultar directamente al autoritativo
host www.example.com ns1.example.com
```

---

## Selección de tipo de registro

```bash
# Tipo específico con -t
host -t A example.com          # solo IPv4
host -t AAAA example.com       # solo IPv6
host -t MX example.com         # servidores de correo
host -t NS example.com         # nameservers
host -t TXT example.com        # registros de texto
host -t SOA example.com        # Start of Authority
host -t CNAME www.example.com  # alias
host -t SRV _sip._tcp.example.com  # servicios
host -t CAA example.com        # CAs autorizadas

# Todos los registros
host -a example.com
# Equivale a una consulta ANY — puede ser bloqueada
```

### Ejemplos de salida por tipo

```bash
$ host -t NS example.com
example.com name server ns1.example.com.
example.com name server ns2.example.com.

$ host -t MX google.com
google.com mail is handled by 5 gmail-smtp-in.l.google.com.
google.com mail is handled by 10 alt1.gmail-smtp-in.l.google.com.
google.com mail is handled by 20 alt2.gmail-smtp-in.l.google.com.

$ host -t SOA example.com
example.com has SOA record ns1.example.com. admin.example.com. 2026032101 7200 3600 1209600 86400

$ host -t TXT google.com
google.com descriptive text "v=spf1 include:_spf.google.com ~all"
google.com descriptive text "globalsign-smime-dv=..."
```

---

## Resolución inversa

`host` detecta automáticamente si el argumento es una IP y hace resolución inversa (PTR):

```bash
# IPv4 — host detecta que es una IP
host 8.8.8.8
# 8.8.8.8.in-addr.arpa domain name pointer dns.google.

host 1.1.1.1
# 1.1.1.1.in-addr.arpa domain name pointer one.one.one.one.

# IPv6
host 2606:4700:4700::1111
# 1.1.1.1.0.0.0.0.0.0.7.4.0.0.7.4.6.0.6.2.ip6.arpa domain name pointer one.one.one.one.

# Si no hay PTR:
host 203.0.113.1
# Host 1.113.0.203.in-addr.arpa. not found: 3(NXDOMAIN)
```

No necesitas invertir la IP manualmente ni añadir `.in-addr.arpa` — `host` lo hace por ti. Esto es más conveniente que `dig`, donde debes usar `dig -x IP`.

---

## Opciones útiles

### Modo verbose (-v)

```bash
host -v www.example.com
```

```
# Salida similar a dig: muestra secciones, flags, TTL
# Trying "www.example.com"
# ;; ->>HEADER<<- opcode: QUERY, status: NOERROR, id: 12345
# ;; flags: qr rd ra; QUERY: 1, ANSWER: 1, AUTHORITY: 0, ADDITIONAL: 0
#
# ;; QUESTION SECTION:
# ;www.example.com.           IN    A
#
# ;; ANSWER SECTION:
# www.example.com.    86400   IN    A    93.184.216.34
#
# Received 48 bytes from 192.168.1.1#53 in 45 ms
```

Con `-v`, `host` produce una salida parecida a `dig`, mostrando secciones, flags y TTL. Útil cuando quieres más detalle sin recordar las opciones de `dig`.

### Otras opciones

```bash
# Esperar solo N segundos por respuesta (-W)
host -W 2 www.example.com
# Timeout de 2 segundos (default: ~10s)

# Número de reintentos (-R)
host -R 1 www.example.com
# Solo 1 intento (default: 1)

# Modo silencioso para scripts — host no tiene un +short,
# pero su salida ya es concisa:
host www.example.com | awk '{print $NF}'
# 93.184.216.34
```

---

## host en scripts

Aunque `dig +short` es preferible para scripts robustos, `host` puede usarse para verificaciones rápidas:

### Verificar si un dominio existe

```bash
# host devuelve exit code 0 si encuentra, 1 si no
if host -t A example.com > /dev/null 2>&1; then
    echo "El dominio existe"
else
    echo "El dominio NO existe"
fi
```

### Obtener solo la IP

```bash
# Extraer la IP de la salida
host www.example.com | awk '/has address/ {print $4}'
# 93.184.216.34

# Si hay múltiples IPs
host google.com | awk '/has address/ {print $4}'
# 142.250.185.206

# Extraer IPv6
host example.com | awk '/has IPv6/ {print $5}'
# 2606:2800:220:1:248:1893:25c8:1946
```

### Verificar registros de email

```bash
# ¿Tiene MX?
host -t MX example.com | grep -q "mail is handled"
echo $?    # 0 = tiene MX, 1 = no tiene

# Listar MX ordenados por prioridad
host -t MX google.com | sort -t' ' -k6 -n
# google.com mail is handled by 5 gmail-smtp-in.l.google.com.
# google.com mail is handled by 10 alt1.gmail-smtp-in.l.google.com.
# ...
```

### Resolución inversa masiva

```bash
# Resolver múltiples IPs desde un archivo
while read ip; do
    result=$(host "$ip" 2>/dev/null | awk '/pointer/ {print $NF}')
    echo "$ip → ${result:-NO PTR}"
done < ip_list.txt
```

---

## host vs dig vs nslookup

```
┌─────────────────────┬──────────┬──────────────┬─────────────┐
│ Característica      │ host     │ dig          │ nslookup    │
├─────────────────────┼──────────┼──────────────┼─────────────┤
│ Salida default      │ 1-3 líneas│ ~15 líneas  │ ~6 líneas   │
│ Default muestra     │ A+AAAA+MX│ Solo A       │ A+AAAA      │
│ TTL visible         │ Con -v   │ Siempre      │ Con -debug  │
│ Flags DNS           │ Con -v   │ Siempre      │ No          │
│ +trace              │ No       │ Sí           │ No          │
│ Modo interactivo    │ No       │ No           │ Sí          │
│ PTR automático      │ Sí (auto)│ dig -x IP    │ Sí (auto)   │
│ Complejidad         │ Baja     │ Alta         │ Media       │
│ Scriptable          │ Medio    │ Muy bueno    │ Difícil     │
│ Ideal para          │ Consultas│ Diagnóstico  │ Windows     │
│                     │ rápidas  │ detallado    │ compatibilidad│
└─────────────────────┴──────────┴──────────────┴─────────────┘
```

### Cuándo usar host

```
┌──────────────────────────────────────────────────────────────┐
│  Usar host cuando:                                           │
│  • Solo quieres saber la IP de un dominio (rápido)           │
│  • Resolución inversa rápida (host IP)                       │
│  • Verificar si un MX existe                                 │
│  • Consulta casual desde la terminal                         │
│                                                              │
│  Usar dig cuando:                                            │
│  • Necesitas TTL, flags, secciones authority/additional      │
│  • Diagnóstico de problemas DNS                              │
│  • +trace para trazar la resolución                          │
│  • DNSSEC                                                    │
│  • Scripts robustos (parsear +short)                         │
│  • Comparar respuestas entre servidores                      │
│                                                              │
│  En resumen: host para preguntas simples,                    │
│  dig para diagnóstico serio.                                 │
└──────────────────────────────────────────────────────────────┘
```

---

## Errores comunes

### 1. Esperar que host muestre TTL por defecto

```bash
# host NO muestra TTL en modo normal
host www.example.com
# www.example.com has address 93.184.216.34
# ¿Cuál es el TTL? No se ve.

# Para ver TTL, usar -v o usar dig directamente
host -v www.example.com | grep "ANSWER SECTION" -A1
# www.example.com.    86400    IN    A    93.184.216.34
#                     ─────
#                     TTL visible con -v
```

### 2. Confundir "not found: 3(NXDOMAIN)" con un error de red

```bash
host noexiste.example.com
# Host noexiste.example.com not found: 3(NXDOMAIN)

# NXDOMAIN (código 3) NO es un error de red.
# Significa: "el dominio no existe en DNS"
# La consulta DNS funcionó correctamente — la respuesta es "no existe".

# Un error de red se vería así:
host www.example.com 10.255.255.1
# ;; connection timed out; no servers could be reached
```

### 3. Parsear la salida de host de forma frágil

```bash
# INCORRECTO: asumir posición fija
host example.com | cut -d' ' -f4
# Falla si la salida tiene "has IPv6 address" (5 palabras antes de la IP)

# CORRECTO: filtrar por patrón
host example.com | awk '/has address/ {print $4}'      # IPv4
host example.com | awk '/has IPv6 address/ {print $5}' # IPv6
host example.com | awk '/mail is handled/ {print $7}'  # MX target

# AÚN MEJOR: usar dig +short para scripts
dig +short example.com A      # solo IPs, una por línea, sin texto
```

### 4. Olvidar que host -a puede ser bloqueado

```bash
# host -a equivale a consulta ANY
host -a example.com
# Muchos servidores bloquean ANY (RFC 8482)
# Puede devolver resultados incompletos o HINFO

# Mejor: consultar cada tipo individualmente
host -t A example.com
host -t MX example.com
host -t NS example.com
```

### 5. No especificar servidor al diagnosticar

```bash
# "host dice que el dominio no existe"
host newsite.example.com
# Host newsite.example.com not found: 3(NXDOMAIN)

# ¿Es que no existe o es caché negativa de tu resolver?
# Verificar contra otro resolver:
host newsite.example.com 8.8.8.8
# → Si existe en 8.8.8.8 pero no en tu resolver:
#   negative cache en tu resolver local

# Verificar directamente en el autoritativo:
host -t NS example.com    # obtener NS
host newsite.example.com ns1.example.com
# → Si existe aquí: el registro es válido,
#   solo falta propagación / expiración de caché
```

---

## Cheatsheet

```
┌──────────────────────────────────────────────────────────────┐
│                     host Cheatsheet                          │
├──────────────────────────────────────────────────────────────┤
│                                                              │
│  CONSULTAS:                                                  │
│    host domain                 A + AAAA + MX (todo junto)    │
│    host -t TYPE domain         tipo específico               │
│    host domain server          consultar resolver específico │
│    host IP                     resolución inversa (PTR auto) │
│    host -a domain              todos los registros (ANY)     │
│                                                              │
│  TIPOS (-t):                                                 │
│    A  AAAA  MX  NS  TXT  SOA  CNAME  SRV  CAA  PTR          │
│                                                              │
│  OPCIONES:                                                   │
│    -v              verbose (secciones, flags, TTL)            │
│    -W N            timeout en segundos                       │
│    -R N            número de reintentos                      │
│    -a              consulta ANY                              │
│                                                              │
│  SALIDA:                                                     │
│    "has address"          → registro A                        │
│    "has IPv6 address"     → registro AAAA                    │
│    "mail is handled by"   → registro MX (con prioridad)      │
│    "name server"          → registro NS                      │
│    "descriptive text"     → registro TXT                     │
│    "domain name pointer"  → registro PTR                     │
│    "is an alias for"      → registro CNAME                   │
│    "not found: 3(NXDOMAIN)" → dominio no existe             │
│                                                              │
│  EN SCRIPTS:                                                 │
│    host domain | awk '/has address/ {print $4}'    IPv4      │
│    host domain | awk '/has IPv6/ {print $5}'       IPv6      │
│    host domain | awk '/mail is handled/ {print $7}' MX      │
│    Exit code: 0 = encontrado, 1 = no encontrado              │
│                                                              │
│  VS DIG:                                                     │
│    host = rápido, conciso, consulta casual                   │
│    dig = detallado, +trace, +dnssec, scripts robustos        │
│    Para diagnóstico serio → dig                              │
│    Para "¿cuál es la IP de X?" → host                        │
│                                                              │
└──────────────────────────────────────────────────────────────┘
```

---

## Ejercicios

### Ejercicio 1: Exploración rápida con host

Usa `host` para responder estas preguntas sobre `github.com`:

```bash
host github.com
host -t NS github.com
host -t MX github.com
host -t TXT github.com
host -t SOA github.com
```

**Responde**:
1. ¿Cuántas IPs (A) tiene `github.com`? ¿Tiene IPv6?
2. ¿Qué nameservers usa? ¿Son propios o de un proveedor DNS externo?
3. ¿Tiene registros MX? Si sí, ¿a qué proveedor de email apuntan?
4. ¿Tiene registro SPF? ¿Qué servidores autoriza a enviar email?
5. Haz resolución inversa de la primera IP de github.com. ¿El PTR coincide con `github.com`?

**Pregunta de reflexión**: Ejecutaste 5 comandos `host` para obtener una visión completa. Si hubieras usado `dig`, ¿podrías obtener la misma información en menos comandos? ¿Y con un solo comando `host`?

---

### Ejercicio 2: Comparar host, dig y getent

Ejecuta estos tres comandos para el mismo nombre y compara:

```bash
host www.google.com
dig www.google.com
getent hosts www.google.com
```

Ahora añade una entrada temporal a `/etc/hosts`:
```bash
sudo sh -c 'echo "10.0.0.99  test-override.example.com" >> /etc/hosts'
```

Ejecuta:
```bash
host test-override.example.com
dig +short test-override.example.com
getent hosts test-override.example.com
```

**Responde**:
1. ¿Cuál de las tres herramientas encontró `test-override.example.com`? ¿Por qué las otras dos fallaron?
2. Si una aplicación como `curl` intenta acceder a `test-override.example.com`, ¿usará la IP de `/etc/hosts`? ¿Por qué?
3. ¿En qué situación sería útil que `host` y `dig` NO consulten `/etc/hosts`?

Limpia cuando termines:
```bash
sudo sed -i '/test-override.example.com/d' /etc/hosts
```

**Pregunta de reflexión**: Un administrador junior dice "usé `host` para verificar que mi entrada en `/etc/hosts` funciona y dice NXDOMAIN — algo está roto". ¿Qué le responderías?

---

### Ejercicio 3: Auditoría rápida de múltiples dominios

Usa `host` para hacer una auditoría rápida de estos dominios y llena la tabla:

| Dominio | IPs (A) | IPv6 | MX | NS provider |
|---------|---------|------|----|----|
| `cloudflare.com` | | | | |
| `amazon.com` | | | | |
| `example.com` | | | | |

Comandos a ejecutar:
```bash
for domain in cloudflare.com amazon.com example.com; do
    echo "=== $domain ==="
    host $domain
    host -t NS $domain
    echo ""
done
```

**Responde**:
1. ¿Cuál dominio tiene más registros A? ¿Por qué tendría múltiples?
2. ¿Todos tienen IPv6? ¿Cuál no? ¿Es un problema?
3. ¿Alguno tiene MX con prioridad 0? ¿Qué significa prioridad 0?
4. Compara los nameservers: ¿alguno usa DNS propio vs DNS de un proveedor como Cloudflare o AWS?

Ahora usa `host -v` en uno de los dominios y compara la salida con la salida normal:
```bash
host example.com
host -v example.com
```

**Pregunta de reflexión**: Si tuvieras que elegir UNA sola herramienta DNS para instalar en un servidor mínimo (sin `dig` ni `nslookup`), ¿elegirías `host`? ¿Por qué? ¿Qué perderías respecto a `dig`?

---

> **Siguiente tema**: T03 — systemd-resolved (configuración moderna, resolvectl)
