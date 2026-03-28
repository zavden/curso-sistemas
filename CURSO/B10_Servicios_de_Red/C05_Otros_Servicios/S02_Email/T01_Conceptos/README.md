# Conceptos de Email

## Índice
1. [Componentes del sistema de correo](#1-componentes-del-sistema-de-correo)
2. [Protocolos de correo](#2-protocolos-de-correo)
3. [Flujo completo de un email](#3-flujo-completo-de-un-email)
4. [DNS y correo — registros MX](#4-dns-y-correo--registros-mx)
5. [Anatomía de un email](#5-anatomía-de-un-email)
6. [Seguridad del correo](#6-seguridad-del-correo)
7. [MTAs en Linux](#7-mtas-en-linux)
8. [Errores comunes](#8-errores-comunes)
9. [Cheatsheet](#9-cheatsheet)
10. [Ejercicios](#10-ejercicios)

---

## 1. Componentes del sistema de correo

El sistema de correo electrónico se compone de tres tipos de agentes, cada uno con una función distinta:

```
┌──────────────────────────────────────────────────────────┐
│              Componentes del correo                       │
│                                                          │
│  ┌─────────┐                                             │
│  │   MUA   │  Mail User Agent                            │
│  │         │  Lo que el usuario ve y usa                 │
│  │         │  Thunderbird, Outlook, mutt, webmail        │
│  │         │  Compone, lee, envía correos                │
│  └────┬────┘                                             │
│       │ SMTP (envío)                                     │
│       ▼                                                  │
│  ┌─────────┐                                             │
│  │   MTA   │  Mail Transfer Agent                        │
│  │         │  El "cartero" — transporta el correo        │
│  │         │  Postfix, Sendmail, Exim                    │
│  │         │  Enruta entre servidores vía SMTP           │
│  └────┬────┘                                             │
│       │ entrega local                                    │
│       ▼                                                  │
│  ┌─────────┐                                             │
│  │   MDA   │  Mail Delivery Agent                        │
│  │         │  Deposita el correo en el buzón             │
│  │         │  procmail, maildrop, Dovecot LDA            │
│  │         │  Aplica filtros (Sieve), almacena           │
│  └────┬────┘                                             │
│       │                                                  │
│       ▼                                                  │
│  ┌─────────┐                                             │
│  │  Buzón  │  Mailbox / Maildir                          │
│  │         │  /var/mail/user (mbox)                      │
│  │         │  ~/Maildir/ (Maildir)                       │
│  └────┬────┘                                             │
│       │ IMAP/POP3 (lectura)                              │
│       ▼                                                  │
│  ┌─────────┐                                             │
│  │   MUA   │  El usuario lee el correo                   │
│  └─────────┘                                             │
└──────────────────────────────────────────────────────────┘
```

### MUA — Mail User Agent

El programa que el usuario utiliza para leer y componer correos:

| Tipo | Ejemplos |
|------|----------|
| Gráfico | Thunderbird, Outlook, Evolution |
| Terminal | mutt, alpine, mail |
| Webmail | Roundcube, Gmail, Outlook Web |
| Móvil | K-9 Mail, iOS Mail |

El MUA habla **SMTP** para enviar y **IMAP/POP3** para leer.

### MTA — Mail Transfer Agent

El servidor que transporta correos entre dominios. Es el componente central:

| MTA | Características |
|-----|----------------|
| **Postfix** | Modular, seguro, fácil de configurar. Estándar de facto |
| **Sendmail** | Histórico (1983), configuración compleja (m4 macros) |
| **Exim** | Flexible, popular en cPanel/Debian |
| **qmail** | Seguro pero abandonado, licencia restrictiva |

El MTA escucha en **puerto 25** (SMTP entre servidores) y opcionalmente en **587** (submission desde MUAs).

### MDA — Mail Delivery Agent

Deposita el correo en el buzón del usuario y puede aplicar filtros:

| MDA | Características |
|-----|----------------|
| **procmail** | Clásico, reglas complejas, en declive |
| **maildrop** | Moderno, reemplazo de procmail |
| **Dovecot LDA** | Integrado con Dovecot, soporta Sieve |
| **Sieve** | Lenguaje de filtrado estándar (RFC 5228) |

### Componente adicional — MRA (Mail Retrieval Agent)

```
┌─────────┐
│   MRA   │  Servidor IMAP/POP3
│         │  Permite que el MUA LEA el buzón
│         │  Dovecot, Courier, Cyrus
└─────────┘
```

Dovecot actúa tanto como MDA (entrega) como MRA (lectura IMAP/POP3).

---

## 2. Protocolos de correo

### SMTP — Simple Mail Transfer Protocol

```
Propósito:  Enviar y transportar correo
Puertos:    25  (MTA ↔ MTA, sin autenticación)
            587 (MUA → MTA, con autenticación — submission)
            465 (SMTPS — SMTP sobre TLS implícito, re-estandarizado)
Seguridad:  STARTTLS (oportunístico) o TLS implícito (465)
```

```
┌──────────────────────────────────────────────────────────┐
│              Puertos SMTP                                 │
│                                                          │
│  Puerto 25:                                              │
│    MTA ←→ MTA (servidor a servidor)                      │
│    Sin autenticación requerida                           │
│    Muchos ISPs lo bloquean para usuarios residenciales   │
│                                                          │
│  Puerto 587 (submission):                                │
│    MUA → MTA (cliente envía correo)                      │
│    Requiere autenticación (SMTP AUTH)                    │
│    Usa STARTTLS para cifrar                              │
│    ← Recomendado para envío desde MUAs                   │
│                                                          │
│  Puerto 465 (submissions):                               │
│    MUA → MTA (cliente envía correo)                      │
│    TLS implícito desde la conexión                       │
│    Requiere autenticación                                │
│    ← Alternativa a 587, re-estandarizado en RFC 8314     │
└──────────────────────────────────────────────────────────┘
```

### Conversación SMTP

```bash
# Simular envío SMTP con telnet (solo para entender el protocolo)
telnet mail.example.com 25
```

```
220 mail.example.com ESMTP Postfix
EHLO client.example.com                    ← Saludo (identificación)
250-mail.example.com
250-STARTTLS                               ← Servidor soporta TLS
250-AUTH PLAIN LOGIN                       ← Métodos de autenticación
250 OK
MAIL FROM:<juan@example.com>               ← Remitente (envelope)
250 OK
RCPT TO:<maria@otro.com>                   ← Destinatario (envelope)
250 OK
DATA                                       ← Inicio del mensaje
354 End data with <CR><LF>.<CR><LF>
From: Juan <juan@example.com>              ← Headers del mensaje
To: Maria <maria@otro.com>
Subject: Prueba

Hola Maria, esto es una prueba.            ← Cuerpo del mensaje
.                                          ← Línea con solo un punto = fin
250 OK: queued as ABC123DEF
QUIT
221 Bye
```

### IMAP — Internet Message Access Protocol

```
Propósito:  Leer correo dejándolo en el servidor
Puerto:     143 (STARTTLS) / 993 (TLS implícito)
Modelo:     Los correos viven en el SERVIDOR
            Múltiples dispositivos ven el mismo buzón
            Carpetas, flags (leído/no leído), búsqueda server-side
```

### POP3 — Post Office Protocol v3

```
Propósito:  Descargar correo al dispositivo local
Puerto:     110 (STARTTLS) / 995 (TLS implícito)
Modelo:     Los correos se DESCARGAN y opcionalmente se borran
            Un solo dispositivo
            Simple, poco ancho de banda
```

### IMAP vs POP3

```
┌──────────────────────────────────────────────────────────┐
│              IMAP vs POP3                                 │
│                                                          │
│  IMAP:                                                   │
│  ┌─────────┐  ┌─────────┐  ┌─────────┐                 │
│  │ Laptop  │  │ Teléfono│  │ Webmail │                  │
│  │  ↕      │  │  ↕      │  │  ↕      │                  │
│  └────┬────┘  └────┬────┘  └────┬────┘                  │
│       └────────────┼────────────┘                        │
│                    ▼                                     │
│              ┌──────────┐                                │
│              │ Servidor │  ← Correos viven aquí          │
│              │ IMAP     │  Todos los dispositivos        │
│              │ (Dovecot)│  ven lo mismo                  │
│              └──────────┘                                │
│                                                          │
│  POP3:                                                   │
│  ┌─────────┐                                             │
│  │ PC      │ ← Descarga correos                         │
│  │         │   (y opcionalmente borra del servidor)      │
│  └────┬────┘                                             │
│       ▼                                                  │
│  ┌──────────┐                                            │
│  │ Servidor │  ← Buzón vacío después de descargar        │
│  │ POP3     │                                            │
│  └──────────┘                                            │
│                                                          │
│  Hoy: IMAP es el estándar. POP3 solo para casos          │
│  especiales (ancho de banda limitado, buzón temporal)     │
└──────────────────────────────────────────────────────────┘
```

### Tabla resumen de puertos

| Protocolo | Puerto | TLS | Dirección |
|-----------|--------|-----|-----------|
| SMTP (relay) | 25 | STARTTLS | MTA ↔ MTA |
| SMTP (submission) | 587 | STARTTLS | MUA → MTA |
| SMTPS | 465 | Implícito | MUA → MTA |
| IMAP | 143 | STARTTLS | MUA ← MRA |
| IMAPS | 993 | Implícito | MUA ← MRA |
| POP3 | 110 | STARTTLS | MUA ← MRA |
| POP3S | 995 | Implícito | MUA ← MRA |

---

## 3. Flujo completo de un email

Cuando `juan@example.com` envía un correo a `maria@otro.com`:

```
┌──────────────────────────────────────────────────────────┐
│     juan@example.com → maria@otro.com                     │
│                                                          │
│  1. MUA (Thunderbird)                                    │
│     Juan compone el correo y pulsa Enviar                │
│     │                                                    │
│     │ SMTP (puerto 587, autenticado, TLS)                │
│     ▼                                                    │
│  2. MTA emisor (mail.example.com)                        │
│     Postfix recibe el correo                             │
│     Consulta DNS: "¿MX de otro.com?"                    │
│     │                                                    │
│     │ DNS query                                          │
│     ▼                                                    │
│  3. DNS responde:                                        │
│     otro.com. MX 10 mail.otro.com.                       │
│     mail.otro.com. A 203.0.113.10                        │
│     │                                                    │
│     │ SMTP (puerto 25, STARTTLS)                         │
│     ▼                                                    │
│  4. MTA receptor (mail.otro.com)                         │
│     Postfix recibe el correo                             │
│     Verifica: ¿maria es usuario local?                   │
│     │                                                    │
│     │ Entrega local                                      │
│     ▼                                                    │
│  5. MDA (Dovecot LDA)                                    │
│     Aplica filtros Sieve                                 │
│     Deposita en ~/Maildir/new/                           │
│     │                                                    │
│     │ IMAP (puerto 993, TLS)                             │
│     ▼                                                    │
│  6. MUA de María                                         │
│     Thunderbird se conecta a Dovecot                     │
│     Descarga y muestra el correo                         │
└──────────────────────────────────────────────────────────┘
```

### Qué pasa si el destino no está disponible

```
MTA emisor intenta entregar a mail.otro.com:
  │
  ├── Conexión exitosa → entrega inmediata
  │
  ├── Conexión falla → el correo entra en la COLA (queue)
  │   ├── Reintento en 5 minutos
  │   ├── Reintento en 30 minutos
  │   ├── Reintento en 1 hora
  │   ├── Reintentos periódicos...
  │   └── Tras 5 días (configurable) → bounce
  │       El remitente recibe un correo de error:
  │       "Undelivered Mail Returned to Sender"
  │
  └── Error permanente (usuario no existe) → bounce inmediato
      "550 User unknown"
```

---

## 4. DNS y correo — registros MX

### Registros MX

El registro **MX (Mail Exchanger)** indica qué servidor recibe correo para un dominio:

```bash
# Consultar registros MX
dig MX example.com +short
```

```
10 mail1.example.com.
20 mail2.example.com.
```

El número es la **prioridad** — menor número = mayor prioridad. El MTA emisor intenta primero el servidor con prioridad 10. Si falla, intenta el de prioridad 20 (backup).

```
┌──────────────────────────────────────────────────────────┐
│              Registros MX con prioridad                   │
│                                                          │
│  example.com.  MX  10  mail1.example.com.  ← primario    │
│  example.com.  MX  20  mail2.example.com.  ← backup      │
│  example.com.  MX  30  mail3.example.com.  ← tercer nivel│
│                                                          │
│  Flujo de entrega:                                       │
│  1. Intentar mail1 (prioridad 10)                        │
│     ├── OK → entregar                                    │
│     └── Falla → paso 2                                   │
│  2. Intentar mail2 (prioridad 20)                        │
│     ├── OK → entregar                                    │
│     └── Falla → paso 3                                   │
│  3. Intentar mail3 (prioridad 30)                        │
│     ├── OK → entregar                                    │
│     └── Falla → encolar y reintentar más tarde           │
│                                                          │
│  Si dos MX tienen la misma prioridad, se elige al azar   │
│  (round-robin — distribución de carga)                   │
└──────────────────────────────────────────────────────────┘
```

### Registros DNS adicionales para correo

| Registro | Propósito | Ejemplo |
|----------|-----------|---------|
| **MX** | Servidor de correo entrante | `10 mail.example.com.` |
| **A/AAAA** | IP del servidor MX | `mail.example.com. A 93.184.216.34` |
| **PTR** | Reverse DNS (IP → nombre) | `34.216.184.93 → mail.example.com` |
| **SPF** (TXT) | Quién puede enviar correo por este dominio | `v=spf1 mx -all` |
| **DKIM** (TXT) | Firma criptográfica del correo | `k=rsa; p=MIGf...` |
| **DMARC** (TXT) | Política de autenticación | `v=DMARC1; p=reject` |

### SPF, DKIM y DMARC — la tríada anti-spoofing

```
┌──────────────────────────────────────────────────────────┐
│         Autenticación de correo                           │
│                                                          │
│  SPF (Sender Policy Framework):                          │
│    "Solo estos servidores pueden enviar correo como       │
│     @example.com"                                        │
│    TXT: v=spf1 mx ip4:93.184.216.34 -all                │
│                                                          │
│  DKIM (DomainKeys Identified Mail):                      │
│    "Este correo tiene una firma criptográfica que        │
│     demuestra que no fue alterado y que salió de          │
│     un servidor autorizado"                              │
│    El servidor firma con clave privada                   │
│    El receptor verifica con clave pública en DNS         │
│                                                          │
│  DMARC (Domain-based Message Authentication):            │
│    "Si un correo falla SPF Y DKIM, hacer esto:           │
│     none (monitorear), quarantine, reject"               │
│    TXT: v=DMARC1; p=reject; rua=mailto:dmarc@example.com│
│                                                          │
│  Juntos: SPF dice QUIÉN puede enviar,                    │
│          DKIM prueba que el contenido es AUTÉNTICO,      │
│          DMARC dice QUÉ HACER si fallan                  │
└──────────────────────────────────────────────────────────┘
```

---

## 5. Anatomía de un email

Un correo electrónico tiene dos partes: **headers** (metadatos) y **body** (contenido), separados por una línea vacía.

### Headers principales

```
Return-Path: <juan@example.com>                    ← Envelope from
Delivered-To: maria@otro.com                       ← Envelope to
Received: from mail.example.com (mail.example.com [93.184.216.34])
    by mail.otro.com (Postfix) with ESMTPS id ABC123
    for <maria@otro.com>;
    Sat, 21 Mar 2026 10:00:00 +0000 (UTC)          ← Traza de ruta
Received: from [192.168.1.100] (laptop.example.com [93.184.216.100])
    by mail.example.com (Postfix) with ESMTPSA id DEF456
    for <maria@otro.com>;
    Sat, 21 Mar 2026 09:59:59 +0000 (UTC)          ← Servidor emisor
DKIM-Signature: v=1; a=rsa-sha256; d=example.com; ...
From: Juan García <juan@example.com>               ← Remitente visible
To: Maria López <maria@otro.com>                   ← Destinatario visible
Subject: Reunión del lunes                         ← Asunto
Date: Sat, 21 Mar 2026 09:59:58 +0000             ← Fecha de composición
Message-ID: <unique-id@example.com>                ← ID único del mensaje
MIME-Version: 1.0                                  ← Versión MIME
Content-Type: text/plain; charset=UTF-8            ← Tipo de contenido
                                                   ← Línea vacía = separador
Hola María,

¿Podemos mover la reunión al martes?               ← Body

Saludos,
Juan
```

### Envelope vs Headers

```
┌──────────────────────────────────────────────────────────┐
│        Envelope vs Headers (como correo postal)           │
│                                                          │
│  Envelope (sobre):                                       │
│    MAIL FROM: juan@example.com     ← Return-Path         │
│    RCPT TO: maria@otro.com         ← Delivered-To        │
│    → Lo que usa el MTA para enrutar                      │
│    → Se configura en la conversación SMTP                │
│                                                          │
│  Headers (carta dentro del sobre):                       │
│    From: Juan <juan@example.com>                         │
│    To: Maria <maria@otro.com>                            │
│    → Lo que ve el MUA del usuario                        │
│    → PUEDEN ser diferentes del envelope                  │
│                                                          │
│  ¡El From: del header puede ser FALSO!                   │
│  Es como escribir un remitente falso en un sobre postal  │
│  SPF/DKIM/DMARC existen para detectar esto               │
└──────────────────────────────────────────────────────────┘
```

### Headers Received (traza de ruta)

Los headers `Received:` se leen de **abajo hacia arriba** — cada servidor que toca el correo agrega un header al principio:

```
Received: from mail.example.com     ← Último hop (servidor destino)
    by mail.otro.com
    Sat, 21 Mar 2026 10:00:00

Received: from [192.168.1.100]      ← Primer hop (MUA → MTA emisor)
    by mail.example.com
    Sat, 21 Mar 2026 09:59:59

Ruta: laptop → mail.example.com → mail.otro.com
         ↑ abajo                    ↑ arriba
```

### Formatos de buzón

| Formato | Estructura | Características |
|---------|-----------|----------------|
| **mbox** | Un archivo por buzón (`/var/mail/user`) | Simple, locking necesario, un archivo enorme |
| **Maildir** | Un archivo por mensaje (`~/Maildir/new/`, `cur/`, `tmp/`) | Sin locking, rendimiento, NFS-safe |
| **mdbox** | Formato Dovecot (mensajes agrupados en archivos dbox) | Mejor rendimiento que Maildir |

```
Maildir/
├── new/          ← Mensajes nuevos (no leídos)
│   ├── 1616323200.V801I1.hostname:2,
│   └── 1616323500.V801I2.hostname:2,
├── cur/          ← Mensajes leídos
│   └── 1616320000.V801I0.hostname:2,S
└── tmp/          ← Mensajes en proceso de entrega
```

---

## 6. Seguridad del correo

### STARTTLS vs TLS implícito

```
┌──────────────────────────────────────────────────────────┐
│         STARTTLS vs TLS implícito                         │
│                                                          │
│  STARTTLS (puertos 25, 587, 143, 110):                   │
│    1. Conexión TCP en texto plano                        │
│    2. Cliente envía STARTTLS                             │
│    3. Se negocia TLS                                     │
│    4. Todo el tráfico posterior va cifrado                │
│    Riesgo: un atacante puede bloquear el comando          │
│    STARTTLS (downgrade attack) ← mitigado con MTA-STS    │
│                                                          │
│  TLS implícito (puertos 465, 993, 995):                  │
│    1. Conexión TLS desde el primer byte                  │
│    2. No hay fase en texto plano                         │
│    Ventaja: no hay posibilidad de downgrade              │
│    ← Recomendado para conexiones MUA ↔ servidor          │
└──────────────────────────────────────────────────────────┘
```

### Open relay — el peligro principal

Un **open relay** es un MTA que acepta correo de cualquiera y lo reenvía a cualquier destino sin autenticación:

```bash
# Un spammer se conecta a tu servidor:
MAIL FROM:<spam@evil.com>
RCPT TO:<victima@gmail.com>
DATA
Buy cheap pills...
.
# Si tu servidor acepta esto → eres un OPEN RELAY
# Tu IP será bloqueada en listas negras (RBLs) en horas
```

Protecciones:
1. **Solo aceptar relay de usuarios autenticados** (SMTP AUTH en puerto 587)
2. **Solo aceptar correo entrante para dominios propios** (puerto 25)
3. **`mynetworks`** en Postfix — solo redes confiables pueden hacer relay
4. Verificar con herramientas online (mxtoolbox.com) que no eres open relay

### Listas negras (RBL/DNSBL)

Si tu servidor es identificado como fuente de spam, tu IP se agrega a listas negras:

```bash
# Verificar si una IP está en listas negras
# Consultar la IP invertida contra la zona DNSBL
dig +short 34.216.184.93.zen.spamhaus.org
# Si retorna 127.0.0.x → estás en la lista negra

# Herramientas:
# - mxtoolbox.com/blacklists.aspx
# - multirbl.valli.org
```

---

## 7. MTAs en Linux

### Comparación de MTAs

```
┌──────────────────────────────────────────────────────────┐
│              MTAs principales en Linux                    │
│                                                          │
│  Postfix:                                                │
│    Creado por Wietse Venema (1997)                       │
│    Diseñado como reemplazo seguro de Sendmail            │
│    Arquitectura modular (cada función = proceso separado)│
│    main.cf + master.cf                                   │
│    ★ Estándar de facto — el MTA que debes conocer        │
│                                                          │
│  Sendmail:                                               │
│    El MTA original de Unix (1983)                        │
│    Configuración en macros m4 (sendmail.mc → sendmail.cf)│
│    Monolítico, historial de vulnerabilidades             │
│    Todavía en producción en sistemas legacy              │
│                                                          │
│  Exim:                                                   │
│    Popular en Debian y cPanel                            │
│    Un solo binario, configuración muy flexible           │
│    Buen soporte para reglas de routing complejas         │
│                                                          │
│  Todos implementan SMTP y son intercambiables como       │
│  /usr/sbin/sendmail (interfaz compatible)                │
└──────────────────────────────────────────────────────────┘
```

### El comando sendmail (interfaz universal)

Independientemente del MTA instalado, todos proveen `/usr/sbin/sendmail` como interfaz de línea de comandos compatible:

```bash
# Enviar un correo desde la línea de comandos
echo "Cuerpo del mensaje" | sendmail -t <<EOF
From: root@example.com
To: admin@example.com
Subject: Alerta del servidor

El disco está al 90%.
EOF

# Esto funciona con Postfix, Sendmail y Exim
# Las aplicaciones usan esta interfaz para enviar correo
# (cron, logwatch, scripts de monitoreo)
```

### El sistema local de correo

Incluso sin un MTA configurado para Internet, Linux usa correo local internamente:

```bash
# Cron envía la salida de jobs por correo local
# root recibe alertas del sistema
# /var/mail/root contiene estos correos

# Leer correo local
mail
# o
mutt -f /var/mail/root
```

---

## 8. Errores comunes

### Error 1 — Confundir SMTP con IMAP/POP3

```
# SMTP = ENVIAR correo (MUA→MTA, MTA→MTA)
# IMAP/POP3 = LEER correo (MUA←MRA)

# Un servidor de correo completo necesita AMBOS:
# - Postfix (SMTP) para enviar/recibir
# - Dovecot (IMAP/POP3) para que los usuarios lean

# Configurar solo Postfix → puedes enviar pero no leer
# Configurar solo Dovecot → puedes leer pero no recibir
```

### Error 2 — No configurar reverse DNS (PTR)

```bash
# Muchos servidores rechazan correo si el PTR no coincide
# con el hostname del MTA

# Verificar:
dig -x 93.184.216.34 +short
# mail.example.com.   ← Debe coincidir con el EHLO del MTA

# Si el PTR dice "host-93-184-216-34.isp.com" en vez de
# "mail.example.com" → muchos servidores rechazan o marcan como spam
# El PTR se configura en el proveedor de hosting/ISP
```

### Error 3 — Open relay por mynetworks demasiado amplio

```bash
# MAL — permite relay desde cualquier IP
mynetworks = 0.0.0.0/0

# BIEN — solo localhost y red local
mynetworks = 127.0.0.0/8 [::1]/128 192.168.1.0/24
```

### Error 4 — Confundir envelope from con header From

```bash
# Un bounce llega a bounce@example.com pero el From del correo
# original decía juan@example.com
# ¿Por qué?

# Porque el envelope (MAIL FROM) es lo que el MTA usa para
# devolver bounces. El header From es lo que ve el usuario.
# Pueden ser diferentes (y frecuentemente lo son en listas
# de correo, correo masivo, etc.)
```

### Error 5 — No tener registros MX

```bash
# Sin registro MX, los servidores emisores intentan el registro A
# del dominio como fallback (RFC 5321)
# Pero muchos servidores modernos rechazan dominios sin MX

# Verificar:
dig MX example.com
# Si no hay respuesta → configurar en el DNS:
# example.com.  MX  10  mail.example.com.
```

---

## 9. Cheatsheet

```bash
# ── Componentes ───────────────────────────────────────────
# MUA = cliente (Thunderbird, mutt)      → SMTP para enviar
# MTA = transporte (Postfix, Exim)       → SMTP entre servidores
# MDA = entrega (Dovecot LDA, procmail)  → deposita en buzón
# MRA = lectura (Dovecot)                → IMAP/POP3

# ── Puertos ───────────────────────────────────────────────
# 25   SMTP relay      MTA↔MTA     STARTTLS
# 587  SMTP submission MUA→MTA     STARTTLS + AUTH
# 465  SMTPS           MUA→MTA     TLS implícito + AUTH
# 143  IMAP            MUA←MRA     STARTTLS
# 993  IMAPS           MUA←MRA     TLS implícito
# 110  POP3            MUA←MRA     STARTTLS
# 995  POP3S           MUA←MRA     TLS implícito

# ── DNS para correo ──────────────────────────────────────
dig MX domain.com +short           # Servidor de correo
dig TXT domain.com +short          # SPF/DKIM/DMARC
dig -x IP +short                   # Reverse DNS (PTR)

# ── Registros DNS necesarios ──────────────────────────────
# MX:    domain.com. MX 10 mail.domain.com.
# A:     mail.domain.com. A 93.184.216.34
# PTR:   34.216.184.93 → mail.domain.com (en el ISP)
# SPF:   TXT "v=spf1 mx -all"
# DKIM:  TXT (clave pública)
# DMARC: TXT "v=DMARC1; p=reject; rua=mailto:..."

# ── Flujo de envío ───────────────────────────────────────
# MUA ─(587/SMTP AUTH)→ MTA emisor ─(DNS MX)→ MTA receptor
#   → MDA → Buzón → MRA ─(993/IMAP)→ MUA receptor

# ── Formatos de buzón ─────────────────────────────────────
# mbox:    /var/mail/user (un archivo, necesita locking)
# Maildir: ~/Maildir/{new,cur,tmp}/ (un archivo por mensaje)

# ── Correo local ──────────────────────────────────────────
mail                                 # Leer correo local
echo "msg" | mail -s "subj" user    # Enviar correo local
cat /var/mail/root                   # Buzón de root

# ── Autenticación (anti-spoofing) ─────────────────────────
# SPF:   quién puede enviar ← registro TXT
# DKIM:  firma criptográfica ← registro TXT + firma en header
# DMARC: qué hacer si fallan ← registro TXT
```

---

## 10. Ejercicios

### Ejercicio 1 — Trazar el flujo de un correo

Dado el siguiente escenario, describe paso a paso cada componente y protocolo involucrado.

```
Escenario: ana@empresa.com envía un correo a pedro@universidad.edu
           Ana usa Thunderbird
           Pedro usa Gmail (webmail)
```

```
1. Ana compone el correo en Thunderbird (MUA)
2. Thunderbird envía vía SMTP al puerto 587 de mail.empresa.com
   → Autenticación: ana + contraseña
   → STARTTLS cifra la conexión
3. Postfix (MTA en mail.empresa.com) recibe el correo
4. Postfix consulta DNS: dig MX universidad.edu
   → 10 mx1.universidad.edu
5. Postfix conecta a mx1.universidad.edu:25 vía SMTP
   → STARTTLS si el receptor lo soporta
6. MTA de universidad.edu recibe el correo
   → Verifica SPF: ¿mail.empresa.com puede enviar para empresa.com?
   → Verifica DKIM: ¿la firma es válida?
7. MDA entrega al buzón de pedro (Maildir)
8. Pedro abre Gmail (MUA webmail)
   → Gmail conecta al servidor IMAP (993) interno
   → Muestra el correo de Ana
```

**Pregunta de predicción:** Si `mail.empresa.com` no tiene registro PTR (reverse DNS), ¿qué es probable que ocurra cuando intente entregar correo a `mx1.universidad.edu`?

> **Respuesta:** El servidor de la universidad probablemente **rechazará** el correo o lo marcará como spam. Muchos MTAs verifican que el PTR de la IP del emisor coincida con el hostname del EHLO. Si la IP `93.184.216.34` no tiene PTR, o su PTR dice `vps34.hosting.com` en lugar de `mail.empresa.com`, el receptor lo considera sospechoso. Es uno de los requisitos básicos para entregar correo de forma confiable.

---

### Ejercicio 2 — Interpretar headers de un correo

Dado el siguiente extracto de headers, responde las preguntas:

```
Received: from mail.otro.com (mail.otro.com [203.0.113.10])
    by mail.empresa.com (Postfix) with ESMTPS id A1B2C3
    for <ana@empresa.com>;
    Sat, 21 Mar 2026 10:00:05 +0000
Received: from laptop.casa.local (dsl-85-10-200.isp.com [85.10.0.200])
    by mail.otro.com (Postfix) with ESMTPSA id D4E5F6
    for <ana@empresa.com>;
    Sat, 21 Mar 2026 10:00:02 +0000
From: Carlos Admin <carlos@otro.com>
To: Ana <ana@empresa.com>
Subject: Informe semanal
```

**Pregunta de predicción:** ¿Desde qué equipo se envió originalmente el correo? ¿Por cuántos servidores pasó? ¿El usuario se autenticó (ESMTPSA vs ESMTPS)?

> **Respuesta:** El correo se envió desde `laptop.casa.local` con IP pública `85.10.0.200` (probablemente una conexión residencial DSL). Pasó por **2 servidores**: primero `mail.otro.com` (MTA emisor) y luego `mail.empresa.com` (MTA receptor). El primer hop usa **ESMTPSA** (la "A" indica que hubo autenticación SMTP AUTH — Carlos se autenticó con usuario/contraseña). El segundo hop usa **ESMTPS** (la "S" indica STARTTLS pero sin AUTH — es comunicación servidor a servidor por puerto 25, que no requiere autenticación).

---

### Ejercicio 3 — Verificar DNS de un dominio de correo

Comprueba si un dominio tiene la configuración DNS mínima para recibir correo.

```bash
# 1. ¿Tiene registro MX?
dig MX example.com +short
# 10 mail.example.com.  ← Sí

# 2. ¿El MX resuelve a una IP?
dig A mail.example.com +short
# 93.184.216.34  ← Sí

# 3. ¿Tiene reverse DNS?
dig -x 93.184.216.34 +short
# mail.example.com.  ← Coincide ✓

# 4. ¿Tiene SPF?
dig TXT example.com +short | grep spf
# "v=spf1 mx ip4:93.184.216.34 -all"  ← Sí

# 5. ¿Tiene DMARC?
dig TXT _dmarc.example.com +short
# "v=DMARC1; p=reject; rua=mailto:dmarc@example.com"  ← Sí

# 6. ¿El puerto 25 está abierto?
nc -zv mail.example.com 25
# Connection to mail.example.com 25 port [tcp/smtp] succeeded!
```

**Pregunta de predicción:** Un dominio tiene registro MX y el servidor responde en el puerto 25, pero no tiene registro SPF. ¿Puede recibir correo? ¿Puede enviar correo sin problemas?

> **Respuesta:** Puede **recibir** correo sin problemas — SPF no afecta la recepción. Pero al **enviar**, los servidores receptores verificarán el SPF del dominio emisor y no encontrarán ninguno. Muchos servidores tratan "sin SPF" como neutral (ni pasa ni falla), pero combinado con la ausencia de DKIM y DMARC, el correo tendrá mayor probabilidad de ser clasificado como spam. SPF no es obligatorio, pero es casi imprescindible en la práctica.

---

> **Siguiente:** T02 — Postfix básico (main.cf, configuración mínima, relay).
