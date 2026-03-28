# Zonas DNS en BIND

## Índice

1. [Qué es una zona DNS](#qué-es-una-zona-dns)
2. [Zona forward vs zona reversa](#zona-forward-vs-zona-reversa)
3. [Declaración de zonas en named.conf](#declaración-de-zonas-en-namedconf)
4. [Registro SOA](#registro-soa)
5. [Estructura de un archivo de zona](#estructura-de-un-archivo-de-zona)
6. [Tipos de registro en la zona](#tipos-de-registro-en-la-zona)
7. [Zona reversa](#zona-reversa)
8. [Delegación y glue records](#delegación-y-glue-records)
9. [Zonas secundarias](#zonas-secundarias)
10. [Validación y diagnóstico](#validación-y-diagnóstico)
11. [Errores comunes](#errores-comunes)
12. [Cheatsheet](#cheatsheet)
13. [Ejercicios](#ejercicios)

---

## Qué es una zona DNS

Una zona es un segmento del espacio de nombres DNS sobre el cual un servidor tiene autoridad. No es lo mismo que un dominio:

```
    Dominio: ejemplo.com (todo el árbol bajo ejemplo.com)

    Zona: puede coincidir con el dominio, o no.

    ejemplo.com (zona)
    ├── www.ejemplo.com          ← en esta zona
    ├── mail.ejemplo.com         ← en esta zona
    ├── api.ejemplo.com          ← en esta zona
    └── dev.ejemplo.com          ← DELEGADO a otra zona
         ├── app.dev.ejemplo.com ← zona de dev.ejemplo.com
         └── db.dev.ejemplo.com  ← zona de dev.ejemplo.com

    El dominio ejemplo.com incluye todo.
    La zona ejemplo.com NO incluye lo que está bajo dev.ejemplo.com
    (porque se delegó a otro servidor).
```

Un servidor **autoritativo** es la fuente de verdad para una zona. Las respuestas autoritativas llevan el flag `aa` (Authoritative Answer) en la respuesta DNS.

---

## Zona forward vs zona reversa

### Zona forward

Traduce **nombre → IP**:

```
    www.ejemplo.com  →  192.168.1.10
    mail.ejemplo.com →  192.168.1.20
```

### Zona reversa

Traduce **IP → nombre** (resolución inversa):

```
    192.168.1.10  →  www.ejemplo.com
    192.168.1.20  →  mail.ejemplo.com
```

La zona reversa usa un dominio especial: `in-addr.arpa` para IPv4 e `ip6.arpa` para IPv6. La IP se escribe **al revés**:

```
    IP:        192.168.1.0/24
    Zona:      1.168.192.in-addr.arpa

    IP:        10.0.0.0/8
    Zona:      10.in-addr.arpa

    IP:        172.16.0.0/16
    Zona:      16.172.in-addr.arpa
```

```
    Zona forward:                  Zona reversa:
    ┌──────────────────────┐      ┌───────────────────────────────┐
    │ ejemplo.com          │      │ 1.168.192.in-addr.arpa        │
    │                      │      │                               │
    │ www  → 192.168.1.10  │      │ 10 → www.ejemplo.com.        │
    │ mail → 192.168.1.20  │      │ 20 → mail.ejemplo.com.       │
    │ ns1  → 192.168.1.1   │      │ 1  → ns1.ejemplo.com.        │
    └──────────────────────┘      └───────────────────────────────┘
```

¿Por qué importa la zona reversa?

- Algunos servicios verifican PTR (email: SPF/DKIM requieren PTR válido)
- Diagnóstico: `dig -x IP` para saber qué hostname tiene una IP
- Seguridad: validar que una IP corresponde al hostname esperado
- Logging: los logs son más legibles con nombres que con IPs

---

## Declaración de zonas en named.conf

### Zona primaria (master)

```c
// En named.conf.local (Debian) o named.conf (RHEL)

zone "ejemplo.com" IN {
    type master;                          // servidor primario
    file "/etc/bind/zones/db.ejemplo.com";  // archivo de zona
    allow-transfer { 192.168.1.20; };     // secundarios permitidos
    allow-update { none; };               // sin actualizaciones dinámicas
    notify yes;                           // notificar a secundarios al cambiar
};
```

### Zona secundaria (slave)

```c
zone "ejemplo.com" IN {
    type slave;                           // servidor secundario
    file "/var/cache/bind/db.ejemplo.com"; // copia local (se genera automáticamente)
    masters { 192.168.1.10; };            // IP del primario
    allow-transfer { none; };             // no re-transferir a terceros
};
```

### Zona reversa

```c
zone "1.168.192.in-addr.arpa" IN {
    type master;
    file "/etc/bind/zones/db.192.168.1";
    allow-transfer { 192.168.1.20; };
};
```

### Tipos de zona

| Tipo | Descripción |
|---|---|
| `master` | Servidor primario — la fuente de verdad para la zona |
| `slave` | Servidor secundario — obtiene datos del primario via AXFR/IXFR |
| `hint` | Solo para la zona root (`.`) — contiene los root servers |
| `forward` | Reenvía consultas de esta zona a servidores específicos |
| `stub` | Como slave pero solo copia los registros NS (no toda la zona) |
| `static-stub` | Stub con NS configurados estáticamente (sin transferencia) |

> **Nota de terminología**: BIND usa `master`/`slave` en su configuración. La terminología moderna DNS (RFC 8499) usa `primary`/`secondary`. BIND 9.18+ acepta ambas formas: `type primary;` y `type secondary;` son equivalentes.

---

## Registro SOA

El **SOA** (Start of Authority) es el primer registro obligatorio de toda zona. Define metadatos sobre la zona:

```
    ┌─────────────────────────────────────────────────────────┐
    │                    Registro SOA                          │
    │                                                         │
    │  MNAME:   ns1.ejemplo.com.  (servidor primario)        │
    │  RNAME:   admin.ejemplo.com. (email del admin)         │
    │  SERIAL:  2026032101        (número de versión)        │
    │  REFRESH: 3600              (cada cuánto el slave      │
    │                              verifica actualizaciones)  │
    │  RETRY:   900               (reintentar si falla       │
    │                              el refresh)               │
    │  EXPIRE:  604800            (tras cuánto el slave      │
    │                              deja de servir la zona    │
    │                              si no puede contactar     │
    │                              al master)                │
    │  MINIMUM: 86400             (TTL para respuestas       │
    │                              negativas / NXDOMAIN)     │
    └─────────────────────────────────────────────────────────┘
```

### Formato en el archivo de zona

```
@   IN  SOA  ns1.ejemplo.com. admin.ejemplo.com. (
                2026032101  ; Serial  (YYYYMMDDNN)
                3600        ; Refresh (1 hora)
                900         ; Retry   (15 minutos)
                604800      ; Expire  (7 días)
                86400       ; Minimum TTL / Negative cache (1 día)
)
```

### El serial — por qué importa

El serial es un número que identifica la versión de la zona. Los servidores secundarios comparan su serial con el del primario para decidir si necesitan transferir:

```
    Primario: serial 2026032101
    Secundario: serial 2026032101   → iguales, no transferir

    Admin edita la zona y cambia serial a 2026032102

    Primario: serial 2026032102
    Secundario: serial 2026032101   → primario es mayor, transferir!
```

**Convención del serial**: `YYYYMMDDNN` donde NN es el número de cambio del día:

```
2026032101  → 21 de marzo de 2026, primer cambio del día
2026032102  → 21 de marzo de 2026, segundo cambio del día
2026032201  → 22 de marzo de 2026, primer cambio
```

> **Regla absoluta**: cada vez que editas un archivo de zona, **debes incrementar el serial**. Si no lo haces, los secundarios no detectan el cambio y sirven datos obsoletos.

### RNAME — el email codificado

El campo RNAME es una dirección de email donde el `@` se reemplaza por `.`:

```
admin.ejemplo.com. → admin@ejemplo.com
hostmaster.ejemplo.com. → hostmaster@ejemplo.com
admin\.dept.ejemplo.com. → admin.dept@ejemplo.com  (escapar el punto literal)
```

### Tiempos del SOA — guía práctica

| Campo | Valor típico | Para zona que cambia mucho | Para zona estable |
|---|---|---|---|
| Refresh | 3600 (1h) | 900 (15m) | 86400 (24h) |
| Retry | 900 (15m) | 300 (5m) | 3600 (1h) |
| Expire | 604800 (7d) | 86400 (1d) | 2419200 (28d) |
| Minimum | 86400 (1d) | 300 (5m) | 86400 (1d) |

El Minimum TTL (último campo del SOA) tiene un significado específico desde RFC 2308: es el **TTL para respuestas negativas** (NXDOMAIN). Cuando alguien consulta un nombre que no existe, los resolvers cachean esa respuesta negativa durante este tiempo.

---

## Estructura de un archivo de zona

### Archivo de zona forward completo

```
; /etc/bind/zones/db.ejemplo.com
; Zona: ejemplo.com
;
$TTL    86400           ; TTL por defecto: 1 día (24h)
$ORIGIN ejemplo.com.    ; dominio base (el punto final es obligatorio)

; --- SOA ---
@   IN  SOA  ns1.ejemplo.com. admin.ejemplo.com. (
                2026032101  ; Serial
                3600        ; Refresh
                900         ; Retry
                604800      ; Expire
                86400       ; Negative cache TTL
)

; --- Name Servers ---
@       IN  NS      ns1.ejemplo.com.
@       IN  NS      ns2.ejemplo.com.

; --- Mail ---
@       IN  MX  10  mail.ejemplo.com.
@       IN  MX  20  mail2.ejemplo.com.

; --- A Records (nombre → IPv4) ---
ns1     IN  A       192.168.1.1
ns2     IN  A       192.168.1.2
mail    IN  A       192.168.1.10
mail2   IN  A       192.168.1.11
www     IN  A       192.168.1.20
api     IN  A       192.168.1.30

; --- AAAA Records (nombre → IPv6) ---
www     IN  AAAA    2001:db8::20

; --- CNAME (alias) ---
ftp     IN  CNAME   www.ejemplo.com.
blog    IN  CNAME   www.ejemplo.com.

; --- TXT Records ---
@       IN  TXT     "v=spf1 mx -all"
_dmarc  IN  TXT     "v=DMARC1; p=reject; rua=mailto:dmarc@ejemplo.com"

; --- SRV Records ---
_sip._tcp   IN  SRV  10 60 5060 sip.ejemplo.com.
```

### Sintaxis del archivo de zona

**Directivas** (comienzan con `$`):

| Directiva | Significado |
|---|---|
| `$TTL 86400` | TTL por defecto para todos los registros que no lo especifiquen |
| `$ORIGIN ejemplo.com.` | Dominio base — los nombres sin punto final se completan con esto |
| `$INCLUDE "archivo"` | Incluir otro archivo |
| `$GENERATE` | Generar registros en un rango (útil para subnets grandes) |

**El `@`**: representa el dominio definido en `$ORIGIN` (o el nombre de la zona si no hay `$ORIGIN`).

**El punto final**: es **obligatorio** en los nombres completos (FQDN). Sin punto final, BIND añade `$ORIGIN` automáticamente:

```
; Con $ORIGIN ejemplo.com.

www     IN  A  192.168.1.20
; Se expande a: www.ejemplo.com.  IN  A  192.168.1.20

www.ejemplo.com.  IN  A  192.168.1.20
; Ya tiene punto final — se usa tal cual

www.ejemplo.com   IN  A  192.168.1.20
; SIN punto final → se expande a: www.ejemplo.com.ejemplo.com.  ← ¡ERROR!
```

Esta es la fuente del error más frecuente en archivos de zona: **olvidar el punto final en FQDNs**.

**Nombres relativos vs absolutos**:

```
    Con $ORIGIN ejemplo.com.

    Nombre en archivo    Se resuelve como
    ─────────────────    ────────────────────────
    www                  www.ejemplo.com.         (relativo, añade $ORIGIN)
    mail                 mail.ejemplo.com.        (relativo)
    @                    ejemplo.com.             (el propio $ORIGIN)
    ns1.ejemplo.com.     ns1.ejemplo.com.         (absoluto, punto final)
    ns1.ejemplo.com      ns1.ejemplo.com.ejemplo.com.  ← ¡BUG!
```

---

## Tipos de registro en la zona

### Registros esenciales

```
; A — nombre a IPv4
www     IN  A       192.168.1.20

; AAAA — nombre a IPv6
www     IN  AAAA    2001:db8::20

; NS — servidores de nombres para esta zona
@       IN  NS      ns1.ejemplo.com.
@       IN  NS      ns2.ejemplo.com.
; Mínimo 2 NS para redundancia (requerimiento DNS)

; MX — servidores de correo (prioridad + nombre)
@       IN  MX  10  mail.ejemplo.com.
@       IN  MX  20  backup-mail.ejemplo.com.
; Prioridad: menor = preferido (10 se intenta antes que 20)
; MX DEBE apuntar a un nombre con registro A, NUNCA a un CNAME

; CNAME — alias (nombre canónico)
ftp     IN  CNAME   www.ejemplo.com.
blog    IN  CNAME   www.ejemplo.com.
; Un CNAME NO puede coexistir con otro registro del mismo nombre
; @ (la raíz de la zona) NO puede tener CNAME

; TXT — texto libre (SPF, DKIM, verificaciones)
@       IN  TXT     "v=spf1 mx -all"

; PTR — resolución inversa (en zonas reversa)
10      IN  PTR     mail.ejemplo.com.

; SRV — ubicación de servicios
_servicio._protocolo  IN  SRV  prioridad peso puerto destino
_sip._tcp  IN  SRV  10 60 5060 sip.ejemplo.com.
```

### TTL por registro

Cada registro puede tener su propio TTL que sobrescribe el `$TTL`:

```
; $TTL 86400  (1 día por defecto)

; Este registro tiene el TTL default (86400)
www     IN  A       192.168.1.20

; Este tiene un TTL específico de 5 minutos
api     300  IN  A  192.168.1.30

; Útil para registros que cambian frecuentemente
; (failover, mantenimiento, etc.)
```

### $GENERATE — registros en masa

```
; Generar registros A para un rango de hosts
$GENERATE 1-254 host$ IN A 192.168.1.$
; Genera:
; host1  IN  A  192.168.1.1
; host2  IN  A  192.168.1.2
; ...
; host254 IN A  192.168.1.254

; Generar PTR para la zona reversa
$GENERATE 1-254 $ IN PTR host$.ejemplo.com.
; Genera:
; 1  IN  PTR  host1.ejemplo.com.
; 2  IN  PTR  host2.ejemplo.com.
; ...
```

---

## Zona reversa

### Archivo de zona reversa completo

```
; /etc/bind/zones/db.192.168.1
; Zona reversa: 1.168.192.in-addr.arpa
;
$TTL    86400
$ORIGIN 1.168.192.in-addr.arpa.

@   IN  SOA  ns1.ejemplo.com. admin.ejemplo.com. (
                2026032101  ; Serial
                3600        ; Refresh
                900         ; Retry
                604800      ; Expire
                86400       ; Negative cache TTL
)

; Name Servers (mismos que la zona forward)
@       IN  NS      ns1.ejemplo.com.
@       IN  NS      ns2.ejemplo.com.

; PTR Records (IP → nombre)
1       IN  PTR     ns1.ejemplo.com.
2       IN  PTR     ns2.ejemplo.com.
10      IN  PTR     mail.ejemplo.com.
11      IN  PTR     mail2.ejemplo.com.
20      IN  PTR     www.ejemplo.com.
30      IN  PTR     api.ejemplo.com.
```

### Cómo funciona el nombre reverso

```
    IP: 192.168.1.20

    1. Invertir: 20.1.168.192
    2. Añadir sufijo: 20.1.168.192.in-addr.arpa.

    La consulta reversa es:
    dig -x 192.168.1.20
    → dig PTR 20.1.168.192.in-addr.arpa.

    En el archivo de zona "1.168.192.in-addr.arpa":
    El nombre "20" es relativo al $ORIGIN
    → 20.1.168.192.in-addr.arpa.
    → PTR www.ejemplo.com.
```

### Declaración en named.conf

```c
// Zona reversa para 192.168.1.0/24
zone "1.168.192.in-addr.arpa" IN {
    type master;
    file "/etc/bind/zones/db.192.168.1";
    allow-transfer { 192.168.1.20; };
};

// Para una /16 (ej. 10.0.0.0/16):
zone "0.10.in-addr.arpa" IN {
    type master;
    file "/etc/bind/zones/db.10.0";
};

// Para una /8 (ej. 10.0.0.0/8):
zone "10.in-addr.arpa" IN {
    type master;
    file "/etc/bind/zones/db.10";
};
```

### Zona reversa IPv6

```c
// IPv6 usa ip6.arpa con cada nibble (dígito hex) invertido
// 2001:db8:1::/48 →
zone "1.0.0.0.8.b.d.0.1.0.0.2.ip6.arpa" IN {
    type master;
    file "/etc/bind/zones/db.2001.db8.1";
};
```

```
; Archivo de zona reversa IPv6
$ORIGIN 1.0.0.0.8.b.d.0.1.0.0.2.ip6.arpa.

; Para 2001:db8:1::20 (expandido: 2001:0db8:0001:0000:0000:0000:0000:0020)
0.2.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0  IN  PTR  www.ejemplo.com.
```

> Las zonas reversas IPv6 son verbosas porque cada nibble hexadecimal se convierte en un nivel del árbol DNS. En la práctica, herramientas como `h2n` o scripts generan estos archivos.

---

## Delegación y glue records

### Delegación de subdominio

Cuando delegas un subdominio a otro servidor DNS, añades registros NS en la zona padre:

```
; En la zona ejemplo.com
; Delegamos dev.ejemplo.com a ns1.dev.ejemplo.com

dev     IN  NS      ns1.dev.ejemplo.com.
dev     IN  NS      ns2.dev.ejemplo.com.

; Glue records: las IPs de los nameservers del subdominio
; Necesarios porque ns1.dev.ejemplo.com está DENTRO del dominio delegado
; Sin glue: ¿cómo resuelves ns1.dev.ejemplo.com si necesitas
; preguntarle a él mismo para resolverlo? → dependencia circular
ns1.dev IN  A       10.0.0.10
ns2.dev IN  A       10.0.0.11
```

```
    Resolución de app.dev.ejemplo.com:

    1. Resolver pregunta a ns1.ejemplo.com por app.dev.ejemplo.com
    2. ns1.ejemplo.com dice: "yo no tengo eso, pero dev.ejemplo.com
       está en ns1.dev.ejemplo.com (10.0.0.10)"   ← delegación + glue
    3. Resolver pregunta a 10.0.0.10 por app.dev.ejemplo.com
    4. ns1.dev.ejemplo.com responde: "10.0.1.50"  ← respuesta autoritativa
```

### Cuándo se necesitan glue records

```
    Glue necesario:
    dev.ejemplo.com  NS  ns1.dev.ejemplo.com.
    ↑ el NS está DENTRO del dominio delegado → necesita glue

    Glue NO necesario:
    dev.ejemplo.com  NS  ns1.otrositio.com.
    ↑ el NS está FUERA del dominio → se resuelve normalmente
```

---

## Zonas secundarias

### Configuración del primario

```c
// En el primario (ns1, IP 192.168.1.1)
zone "ejemplo.com" IN {
    type master;
    file "/etc/bind/zones/db.ejemplo.com";
    allow-transfer { 192.168.1.2; };  // solo ns2
    also-notify { 192.168.1.2; };     // notificar a ns2 cuando cambie
    notify yes;
};
```

### Configuración del secundario

```c
// En el secundario (ns2, IP 192.168.1.2)
zone "ejemplo.com" IN {
    type slave;
    file "/var/cache/bind/db.ejemplo.com";  // copia local
    masters { 192.168.1.1; };               // IP del primario
    allow-transfer { none; };               // no re-transferir
};
```

### Flujo de transferencia

```
    Primario (ns1)                    Secundario (ns2)
    ┌──────────────┐                 ┌──────────────┐
    │ serial: 101  │                 │ serial: 100  │
    │              │                 │              │
    │ Admin edita  │                 │              │
    │ zona, serial │                 │              │
    │ → 102        │                 │              │
    │              │── NOTIFY ──────►│              │
    │              │                 │ Recibe NOTIFY│
    │              │◄── SOA query ───│ Consulta SOA │
    │ serial: 102  │── SOA reply ──►│ Compara:     │
    │              │                 │ 102 > 100    │
    │              │◄── AXFR ───────│ Pide transf. │
    │              │── zona ───────►│ Recibe zona  │
    │              │   completa     │ serial: 102  │
    └──────────────┘                 └──────────────┘
```

Tipos de transferencia:

| Tipo | Descripción |
|---|---|
| **AXFR** | Transferencia completa — envía toda la zona |
| **IXFR** | Transferencia incremental — solo los cambios desde el último serial |

IXFR es más eficiente para zonas grandes con cambios pequeños. BIND soporta ambos automáticamente.

### Verificar transferencias

```bash
# En el secundario: verificar que la zona se cargó
sudo journalctl -u named | grep "transfer of"
# transfer of 'ejemplo.com/IN' from 192.168.1.1#53: Transfer status: success
# transfer of 'ejemplo.com/IN' from 192.168.1.1#53: Transfer completed: ...

# Verificar el serial en ambos servidores
dig @192.168.1.1 ejemplo.com SOA +short
dig @192.168.1.2 ejemplo.com SOA +short
# Ambos deben mostrar el mismo serial

# Forzar una transferencia
sudo rndc retransfer ejemplo.com

# Intentar AXFR manualmente (diagnóstico)
dig @192.168.1.1 ejemplo.com AXFR
```

---

## Validación y diagnóstico

### named-checkzone

```bash
# Validar un archivo de zona
sudo named-checkzone ejemplo.com /etc/bind/zones/db.ejemplo.com
# zone ejemplo.com/IN: loaded serial 2026032101
# OK

# Validar zona reversa
sudo named-checkzone 1.168.192.in-addr.arpa /etc/bind/zones/db.192.168.1
# zone 1.168.192.in-addr.arpa/IN: loaded serial 2026032101
# OK

# Si hay error:
# zone ejemplo.com/IN: loading from master file db.ejemplo.com failed:
#   db.ejemplo.com:15: unknown RR type 'INN'
#   ↑ archivo    ↑ línea  ↑ problema
```

### Consultar la zona con dig

```bash
# Consultar un registro A
dig @127.0.0.1 www.ejemplo.com A

# Consultar el SOA
dig @127.0.0.1 ejemplo.com SOA

# Consultar los NS
dig @127.0.0.1 ejemplo.com NS

# Consultar MX
dig @127.0.0.1 ejemplo.com MX

# Consultar reverso
dig @127.0.0.1 -x 192.168.1.20

# Verificar que la respuesta es autoritativa
dig @127.0.0.1 www.ejemplo.com +norecurse
# Buscar "flags: qr aa" — el "aa" significa Authoritative Answer

# Transferencia completa (ver toda la zona)
dig @127.0.0.1 ejemplo.com AXFR
```

### Diagnóstico de problemas

```bash
# Ver si named cargó la zona
sudo rndc zonestatus ejemplo.com
# name: ejemplo.com
# type: master
# serial: 2026032101
# ...

# Recargar una zona específica (sin reiniciar named)
sudo rndc reload ejemplo.com

# Recargar todas las zonas
sudo rndc reload

# Vaciar el caché
sudo rndc flush

# Ver estadísticas
sudo rndc stats
cat /var/named/data/named_stats.txt    # RHEL
cat /var/cache/bind/named_stats.txt    # Debian

# Activar query logging temporalmente
sudo rndc querylog on
sudo journalctl -u named -f
# Verás cada consulta que recibe
sudo rndc querylog off    # desactivar
```

---

## Errores comunes

### 1. Olvidar el punto final en los FQDN

```
; MAL — sin punto final
@   IN  NS   ns1.ejemplo.com
;                            ↑ sin punto
; Se expande a: ns1.ejemplo.com.ejemplo.com. ← ROTO

; BIEN
@   IN  NS   ns1.ejemplo.com.
;                             ↑ punto final
```

Este es el error número uno en archivos de zona DNS. `named-checkzone` detecta algunos de estos errores, pero no todos (si el resultado parece un FQDN válido, no alerta).

### 2. No incrementar el serial

```
; Editaste la zona, añadiste un registro, pero no tocaste el serial:
@   IN  SOA  ns1.ejemplo.com. admin.ejemplo.com. (
                2026032101  ; ← MISMO serial que antes del cambio
                ...
)

; Resultado: los secundarios no transfieren la nueva versión
; Resolución: incrementar el serial y recargar
```

### 3. CNAME en la raíz de la zona o coexistiendo con otros registros

```
; MAL — CNAME en @ (la raíz)
@   IN  CNAME   otrohost.ejemplo.com.
; Viola RFC: @ necesita SOA y NS, que no pueden coexistir con CNAME

; MAL — CNAME junto a otro registro del mismo nombre
www  IN  A      192.168.1.20
www  IN  CNAME  otrohost.ejemplo.com.
; CNAME no puede coexistir con ningún otro tipo

; BIEN — un CNAME o registros A, no ambos
www  IN  CNAME  otrohost.ejemplo.com.
; O:
www  IN  A      192.168.1.20
www  IN  AAAA   2001:db8::20    ; A y AAAA sí pueden coexistir
```

### 4. MX apuntando a un CNAME

```
; MAL — MX no debe apuntar a un CNAME (RFC 2181)
@   IN  MX  10  mail-alias.ejemplo.com.
mail-alias  IN  CNAME  mail.proveedor.com.

; BIEN — MX apunta a un nombre con registro A
@   IN  MX  10  mail.ejemplo.com.
mail  IN  A     192.168.1.10
```

### 5. Permisos incorrectos o directorio inexistente

```bash
# La zona no carga:
# zone ejemplo.com/IN: loading from master file
#   /etc/bind/zones/db.ejemplo.com failed: file not found

# Verificar:
ls -la /etc/bind/zones/db.ejemplo.com
# ¿Existe? ¿El directorio /etc/bind/zones/ existe?

# Crear directorio si falta:
sudo mkdir -p /etc/bind/zones
sudo chown bind:bind /etc/bind/zones    # Debian
# sudo chown named:named /var/named/     # RHEL

# Permisos del archivo:
sudo chown bind:bind /etc/bind/zones/db.ejemplo.com
sudo chmod 640 /etc/bind/zones/db.ejemplo.com
```

---

## Cheatsheet

```bash
# === DECLARAR ZONAS EN named.conf ===
zone "ejemplo.com" { type master; file "db.ejemplo.com"; };
zone "ejemplo.com" { type slave; masters { IP; }; file "db.ejemplo.com"; };
zone "1.168.192.in-addr.arpa" { type master; file "db.192.168.1"; };

# === ESTRUCTURA DEL ARCHIVO DE ZONA ===
$TTL 86400                                 # TTL default
$ORIGIN ejemplo.com.                       # dominio base (con punto!)
@  IN SOA ns1.ejemplo.com. admin.ejemplo.com. (serial refresh retry expire minimum)
@  IN NS   ns1.ejemplo.com.               # nameserver (mínimo 2)
@  IN MX   10 mail.ejemplo.com.           # mail server
www IN A    192.168.1.20                   # nombre → IPv4
www IN AAAA 2001:db8::20                   # nombre → IPv6
ftp IN CNAME www.ejemplo.com.             # alias

# === ZONA REVERSA ===
# IP 192.168.1.0/24 → zona 1.168.192.in-addr.arpa
# Registros PTR:
20  IN PTR  www.ejemplo.com.              # 192.168.1.20 → www

# === SERIAL (SIEMPRE INCREMENTAR) ===
# Formato: YYYYMMDDNN
# 2026032101 → 2026032102 (segundo cambio del día)

# === VALIDACIÓN ===
sudo named-checkconf                       # validar named.conf
sudo named-checkconf -z                    # validar config + zonas
sudo named-checkzone ejemplo.com archivo   # validar archivo de zona

# === OPERACIONES ===
sudo rndc reload                           # recargar todo
sudo rndc reload ejemplo.com              # recargar una zona
sudo rndc retransfer ejemplo.com          # forzar transferencia (slave)
sudo rndc flush                            # vaciar caché
sudo rndc zonestatus ejemplo.com          # estado de la zona
sudo rndc querylog on                      # activar log de queries

# === CONSULTAS DE VERIFICACIÓN ===
dig @127.0.0.1 ejemplo.com SOA            # SOA (serial)
dig @127.0.0.1 ejemplo.com NS             # nameservers
dig @127.0.0.1 www.ejemplo.com A          # registro A
dig @127.0.0.1 ejemplo.com AXFR           # transferencia completa
dig @127.0.0.1 -x 192.168.1.20            # reverso
dig @127.0.0.1 www.ejemplo.com +norecurse # verificar aa flag

# === REGLAS DE ORO ===
# 1. Punto final en TODOS los FQDN
# 2. Incrementar serial en CADA edición
# 3. named-checkzone ANTES de recargar
# 4. Mínimo 2 registros NS
# 5. MX → registro A, nunca CNAME
# 6. CNAME no coexiste con otros registros del mismo nombre
```

---

## Ejercicios

### Ejercicio 1 — Crear una zona forward

Crea la zona `lab.local` con los siguientes registros:

| Nombre | Tipo | Valor |
|---|---|---|
| `lab.local.` | NS | `ns1.lab.local.` |
| `lab.local.` | NS | `ns2.lab.local.` |
| `lab.local.` | MX 10 | `mail.lab.local.` |
| `ns1` | A | `192.168.10.1` |
| `ns2` | A | `192.168.10.2` |
| `mail` | A | `192.168.10.10` |
| `www` | A | `192.168.10.20` |
| `api` | A | `192.168.10.30` |
| `blog` | CNAME | `www.lab.local.` |
| `lab.local.` | TXT | `"v=spf1 mx -all"` |

1. Escribe el archivo de zona completo con SOA
2. Declara la zona en named.conf
3. Valida con `named-checkzone`
4. Recarga y prueba con `dig`

<details>
<summary>Solución</summary>

```bash
# Crear directorio
sudo mkdir -p /etc/bind/zones    # Debian
# sudo mkdir -p /var/named/zones  # RHEL
```

```
; /etc/bind/zones/db.lab.local
$TTL    86400
$ORIGIN lab.local.

@   IN  SOA  ns1.lab.local. admin.lab.local. (
                2026032101  ; Serial
                3600        ; Refresh
                900         ; Retry
                604800      ; Expire
                86400       ; Negative cache TTL
)

; Nameservers
@       IN  NS      ns1.lab.local.
@       IN  NS      ns2.lab.local.

; Mail
@       IN  MX  10  mail.lab.local.

; A Records
ns1     IN  A       192.168.10.1
ns2     IN  A       192.168.10.2
mail    IN  A       192.168.10.10
www     IN  A       192.168.10.20
api     IN  A       192.168.10.30

; CNAME
blog    IN  CNAME   www.lab.local.

; TXT
@       IN  TXT     "v=spf1 mx -all"
```

```c
// En named.conf.local (Debian) o named.conf (RHEL):
zone "lab.local" IN {
    type master;
    file "/etc/bind/zones/db.lab.local";
    allow-transfer { none; };
};
```

```bash
# Permisos
sudo chown bind:bind /etc/bind/zones/db.lab.local

# Validar
sudo named-checkzone lab.local /etc/bind/zones/db.lab.local
# zone lab.local/IN: loaded serial 2026032101
# OK

sudo named-checkconf && sudo systemctl reload named

# Probar
dig @127.0.0.1 www.lab.local A +short
# 192.168.10.20

dig @127.0.0.1 blog.lab.local A +short
# www.lab.local.
# 192.168.10.20

dig @127.0.0.1 lab.local MX +short
# 10 mail.lab.local.

dig @127.0.0.1 lab.local SOA +short
# ns1.lab.local. admin.lab.local. 2026032101 3600 900 604800 86400
```

</details>

---

### Ejercicio 2 — Crear la zona reversa correspondiente

Para la zona `lab.local` del ejercicio anterior, crea la zona reversa `10.168.192.in-addr.arpa`:

1. Escribe el archivo de zona reversa con registros PTR para todos los hosts con registro A
2. Declara la zona en named.conf
3. Valida, recarga y prueba con `dig -x`

<details>
<summary>Solución</summary>

```
; /etc/bind/zones/db.192.168.10
$TTL    86400
$ORIGIN 10.168.192.in-addr.arpa.

@   IN  SOA  ns1.lab.local. admin.lab.local. (
                2026032101  ; Serial
                3600        ; Refresh
                900         ; Retry
                604800      ; Expire
                86400       ; Negative cache TTL
)

@       IN  NS      ns1.lab.local.
@       IN  NS      ns2.lab.local.

1       IN  PTR     ns1.lab.local.
2       IN  PTR     ns2.lab.local.
10      IN  PTR     mail.lab.local.
20      IN  PTR     www.lab.local.
30      IN  PTR     api.lab.local.
```

```c
zone "10.168.192.in-addr.arpa" IN {
    type master;
    file "/etc/bind/zones/db.192.168.10";
    allow-transfer { none; };
};
```

```bash
sudo chown bind:bind /etc/bind/zones/db.192.168.10
sudo named-checkzone 10.168.192.in-addr.arpa /etc/bind/zones/db.192.168.10
sudo named-checkconf && sudo systemctl reload named

# Probar
dig @127.0.0.1 -x 192.168.10.20 +short
# www.lab.local.

dig @127.0.0.1 -x 192.168.10.10 +short
# mail.lab.local.

dig @127.0.0.1 -x 192.168.10.1 +short
# ns1.lab.local.
```

</details>

---

### Ejercicio 3 — Diagnosticar una zona rota

El siguiente archivo de zona tiene **5 errores**. Identifícalos sin ejecutar nada, y luego verifica con `named-checkzone`:

```
$TTL    86400
$ORIGIN empresa.com.

@   IN  SOA  ns1.empresa.com admin.empresa.com. (
                2026032101
                3600
                900
                604800
                86400
)

@       IN  NS      ns1.empresa.com.
        IN  MX  10  mail

www     IN  A       192.168.1.20
www     IN  CNAME   otrohost.empresa.com.

@       IN  CNAME   redirect.empresa.com.

mail    IN  A       192.168.1.10

ftp     IN  A       192.168.1.30
ftp     IN  AAAA    2001:db8::30

interno IN  CNAME   www
```

<details>
<summary>Solución</summary>

Los 5 errores:

1. **SOA MNAME sin punto final**: `ns1.empresa.com` → debería ser `ns1.empresa.com.`
   Se expande a `ns1.empresa.com.empresa.com.`

2. **MX apunta a nombre relativo sin punto**: `mail` funciona (se expande a `mail.empresa.com.` que tiene registro A), pero es mejor práctica usar el FQDN: `mail.empresa.com.`

3. **CNAME coexistiendo con A**: `www` tiene un registro A Y un CNAME — prohibido por RFC.

4. **CNAME en la raíz (@)**: `@ IN CNAME redirect.empresa.com.` — la raíz de la zona tiene SOA y NS, no puede tener CNAME.

5. **CNAME apunta a nombre relativo sin punto**: `interno IN CNAME www` → se expande a `www.empresa.com.` que es correcto en este caso, pero si la intención era apuntar a un host externo, faltaría el punto. (Nota: esto es una advertencia más que un error — depende de la intención.)

Correcciones:

```
@   IN  SOA  ns1.empresa.com. admin.empresa.com. (  ← punto añadido
                ...
)
@       IN  MX  10  mail.empresa.com.    ← FQDN con punto

www     IN  A       192.168.1.20         ← eliminar el CNAME de www
; (eliminar la línea: www IN CNAME ...)

; (eliminar: @ IN CNAME redirect.empresa.com.)

interno IN  CNAME   www.empresa.com.     ← punto final explícito
```

</details>

---

### Pregunta de reflexión

> Tu empresa tiene la zona `empresa.com` en un primario y un secundario. El lunes haces un cambio en la zona pero olvidas incrementar el serial. El martes descubres que los clientes que usan el secundario tienen datos obsoletos. Corriges el serial y recargas el primario, pero el secundario sigue con datos viejos. El `Refresh` de la zona es 24 horas (86400). ¿Cuánto tardarán los secundarios en actualizarse automáticamente? ¿Cómo puedes forzar la actualización inmediata?

> **Razonamiento esperado**: con un Refresh de 24 horas, el secundario esperará hasta 24 horas para comprobar el serial del primario. Para forzar la actualización inmediata hay varias opciones: (1) en el primario, `rndc notify ejemplo.com` envía un NOTIFY al secundario, que comprueba el serial inmediatamente; (2) en el secundario, `rndc retransfer ejemplo.com` fuerza una transferencia sin esperar; (3) si `notify yes` está configurado en el primario, el NOTIFY debería haberse enviado automáticamente al recargar — si no se envió, verificar `also-notify` y que el firewall permite el tráfico. La lección: configurar `notify yes` y un Refresh razonable (1-4 horas en zonas que cambian), y **siempre** incrementar el serial como parte del proceso de edición.

---

> **Siguiente tema**: T03 — Tipos de servidor (caching-only, forwarder, authoritative)
