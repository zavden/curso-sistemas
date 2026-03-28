# mailq y logs

## Índice
1. [La cola de correo (mail queue)](#1-la-cola-de-correo-mail-queue)
2. [mailq — inspeccionar la cola](#2-mailq--inspeccionar-la-cola)
3. [Gestión de la cola](#3-gestión-de-la-cola)
4. [Logs de Postfix](#4-logs-de-postfix)
5. [Interpretar líneas de log](#5-interpretar-líneas-de-log)
6. [Trazar un correo por su Queue ID](#6-trazar-un-correo-por-su-queue-id)
7. [Problemas de entrega comunes](#7-problemas-de-entrega-comunes)
8. [Herramientas de análisis](#8-herramientas-de-análisis)
9. [Errores comunes](#9-errores-comunes)
10. [Cheatsheet](#10-cheatsheet)
11. [Ejercicios](#11-ejercicios)

---

## 1. La cola de correo (mail queue)

Cuando Postfix no puede entregar un correo inmediatamente (servidor destino caído, error temporal, congestión), lo almacena en una **cola** y reintenta periódicamente.

```
┌──────────────────────────────────────────────────────────┐
│              Colas internas de Postfix                     │
│                                                          │
│  ┌──────────┐                                            │
│  │ maildrop │  Correo enviado vía sendmail CLI            │
│  └────┬─────┘  pickup lo recoge y pasa a incoming         │
│       ▼                                                  │
│  ┌──────────┐                                            │
│  │ incoming │  Correo recién llegado (SMTP o local)       │
│  └────┬─────┘  cleanup lo procesó, espera a qmgr         │
│       ▼                                                  │
│  ┌──────────┐                                            │
│  │  active  │  Correo que qmgr está intentando entregar  │
│  └────┬─────┘  Tamaño limitado (qmgr_message_active_limit)│
│       │                                                  │
│       ├── Entrega exitosa → se elimina de la cola        │
│       │                                                  │
│       ├── Error temporal (4xx) ──┐                       │
│       │                         ▼                        │
│       │                    ┌──────────┐                  │
│       │                    │ deferred │  Reintentos       │
│       │                    │          │  periódicos       │
│       │                    └──────────┘  (5 días default) │
│       │                                                  │
│       └── Error permanente (5xx) ──┐                     │
│                                    ▼                     │
│                               ┌────────┐                 │
│                               │ bounce │  NDR al emisor  │
│                               └────────┘                 │
│                                                          │
│  ┌──────────┐                                            │
│  │ corrupt  │  Mensajes dañados (rarísimo)               │
│  └──────────┘                                            │
│                                                          │
│  Directorio físico: /var/spool/postfix/                  │
│  Subdirectorios: maildrop/ incoming/ active/ deferred/   │
└──────────────────────────────────────────────────────────┘
```

### Tiempos de reintento

```bash
# Parámetros de reintento en main.cf
queue_run_delay = 300s           # Intervalo entre barridos de cola (5 min)
minimal_backoff_time = 300s      # Mínimo entre reintentos por mensaje
maximal_backoff_time = 4000s     # Máximo entre reintentos (~1 hora)
maximal_queue_lifetime = 5d      # Tiempo máximo en cola (5 días)
bounce_queue_lifetime = 5d       # Tiempo máximo para bounces en cola
```

```
Reintento de un mensaje deferred:
  T+0min     Primer intento → falla (4xx)
  T+5min     Segundo intento → falla
  T+10min    Tercer intento → falla
  T+20min    Cuarto intento → falla
  T+40min    ...backoff exponencial...
  T+66min    ...hasta maximal_backoff_time...
  ...
  T+5 días   maximal_queue_lifetime → bounce al remitente
```

---

## 2. mailq — inspeccionar la cola

### Uso básico

```bash
# Ver la cola de correo (equivale a postqueue -p)
mailq
```

Salida típica:

```
-Queue ID-  --Size-- ----Arrival Time---- -Sender/Recipient-------
ABC123DEF*    2048 Sat Mar 21 10:00:00  juan@example.com
                                         maria@otro.com

DEF456GHI     1536 Sat Mar 21 09:30:00  root@example.com
        (connect to mail.destino.com[203.0.113.10]:25: Connection timed out)
                                         admin@destino.com

GHI789JKL     4096 Fri Mar 20 14:00:00  info@example.com
        (host mail.remoto.com[198.51.100.5] said: 450 4.7.1 Try again later)
                                         user@remoto.com

-- 3 Kbytes in 3 Requests.
```

### Interpretar la salida

```
ABC123DEF*    2048 Sat Mar 21 10:00:00  juan@example.com
    │     │    │          │                    │
    │     │    │          │                    └── Remitente (envelope from)
    │     │    │          └── Fecha de llegada a la cola
    │     │    └── Tamaño del mensaje en bytes
    │     └── * = en cola active (entregándose ahora)
    │         ! = en cola hold (retenido por admin)
    │         (sin marca) = en cola deferred
    └── Queue ID (identificador único)

        (connect to mail.destino.com:25: Connection timed out)
         └── Razón del último intento fallido

                                         admin@destino.com
                                          └── Destinatario
```

### Símbolos de estado

| Símbolo | Estado | Significado |
|---------|--------|------------|
| `*` | Active | Se está entregando ahora mismo |
| `!` | Hold | Retenido por el administrador |
| (nada) | Deferred | Esperando reintento |

### Cola vacía

```bash
mailq
# Mail queue is empty
# ← Todo el correo se entregó exitosamente
```

---

## 3. Gestión de la cola

### postqueue — operaciones seguras

```bash
# Ver la cola (igual que mailq)
postqueue -p

# Forzar reintento inmediato de TODA la cola deferred
postqueue -f

# Forzar reintento de correo para un dominio específico
# (Postfix 3.4+)
postqueue -s destino.com

# Ver la cola en formato JSON (para scripts)
postqueue -j
```

### postsuper — operaciones administrativas

```bash
# Eliminar un mensaje específico de la cola
sudo postsuper -d ABC123DEF

# Eliminar TODOS los mensajes de la cola
sudo postsuper -d ALL

# Eliminar solo los mensajes deferred
sudo postsuper -d ALL deferred

# Poner un mensaje en hold (retener)
sudo postsuper -h ABC123DEF

# Liberar un mensaje de hold
sudo postsuper -H ABC123DEF

# Poner TODOS en hold
sudo postsuper -h ALL

# Re-encolar un mensaje (reprocesar)
sudo postsuper -r ABC123DEF

# Re-encolar todos
sudo postsuper -r ALL
```

### postcat — ver el contenido de un mensaje en cola

```bash
# Ver headers y cuerpo de un mensaje en cola
sudo postcat -q ABC123DEF
```

```
*** ENVELOPE RECORDS ABC123DEF ***
message_size:        2048
message_arrival_time: Sat Mar 21 10:00:00 2026
named_attribute: sender=juan@example.com
named_attribute: recipient=maria@otro.com
*** MESSAGE CONTENTS ABC123DEF ***
Received: from laptop.example.com ...
From: Juan <juan@example.com>
To: Maria <maria@otro.com>
Subject: Reunión
Date: Sat, 21 Mar 2026 10:00:00 +0000

Hola Maria...
*** HEADER EXTRACTED ABC123DEF ***
*** MESSAGE FILE END ABC123DEF ***
```

### Operaciones selectivas con scripts

```bash
# Eliminar correo de un remitente específico
mailq | grep 'spammer@evil.com' -B1 | grep '^[A-Z0-9]' | \
    awk '{print $1}' | tr -d '*!' | \
    while read id; do sudo postsuper -d "$id"; done

# Contar mensajes por dominio destino
mailq | grep '@' | grep -v '^[[:space:]]*(' | \
    awk '{print $NF}' | awk -F@ '{print $2}' | sort | uniq -c | sort -rn

# Ejemplo de salida:
#   45 destino-lento.com
#   12 gmail.com
#    3 otro.com
```

---

## 4. Logs de Postfix

Postfix registra cada operación en syslog, típicamente en `/var/log/mail.log` (Debian) o `/var/log/maillog` (RHEL).

### Ver logs

```bash
# Debian/Ubuntu
sudo tail -f /var/log/mail.log

# RHEL/Fedora
sudo tail -f /var/log/maillog

# Con journalctl
sudo journalctl -u postfix -f

# Solo errores/warnings
sudo journalctl -u postfix -p err -f
```

### Anatomía de una línea de log

```
Mar 21 10:00:01 mail postfix/smtpd[12345]: ABC123DEF: client=laptop.example.com[192.168.1.100]
│               │    │              │       │          │
│               │    │              │       │          └── Información
│               │    │              │       └── Queue ID
│               │    │              └── PID del proceso
│               │    └── Componente de Postfix
│               └── Hostname del servidor
└── Timestamp
```

### Componentes en los logs

| Componente | Qué registra |
|-----------|-------------|
| `postfix/smtpd` | Conexiones SMTP entrantes |
| `postfix/smtp` | Entregas SMTP salientes |
| `postfix/local` | Entregas a buzones locales |
| `postfix/cleanup` | Procesamiento de headers |
| `postfix/qmgr` | Gestión de cola (enqueue/dequeue) |
| `postfix/bounce` | Generación de bounces |
| `postfix/pickup` | Recogida de correo del maildrop |
| `postfix/anvil` | Rate limiting |
| `postfix/submission/smtpd` | Conexiones en puerto 587 |

---

## 5. Interpretar líneas de log

### Entrega exitosa completa

```
# 1. Cliente se conecta y envía
Mar 21 10:00:01 mail postfix/smtpd[12345]: connect from laptop.example.com[192.168.1.100]
Mar 21 10:00:01 mail postfix/smtpd[12345]: ABC123DEF: client=laptop.example.com[192.168.1.100]
Mar 21 10:00:01 mail postfix/cleanup[12346]: ABC123DEF: message-id=<unique@example.com>
Mar 21 10:00:01 mail postfix/qmgr[12340]: ABC123DEF: from=<juan@example.com>, size=2048, nrcpt=1 (queue active)

# 2. Postfix entrega al servidor destino
Mar 21 10:00:02 mail postfix/smtp[12347]: ABC123DEF: to=<maria@otro.com>, relay=mail.otro.com[203.0.113.10]:25, delay=1.2, delays=0.1/0.01/0.5/0.6, dsn=2.0.0, status=sent (250 2.0.0 Ok: queued as XYZ789)

# 3. Mensaje eliminado de la cola
Mar 21 10:00:02 mail postfix/qmgr[12340]: ABC123DEF: removed
```

### Desglose de la línea de entrega

```
ABC123DEF:
  to=<maria@otro.com>,             ← Destinatario
  relay=mail.otro.com[203.0.113.10]:25,  ← Servidor que recibió
  delay=1.2,                       ← Tiempo total (segundos)
  delays=0.1/0.01/0.5/0.6,        ← Desglose del delay
         │    │     │   │
         │    │     │   └── Transmisión del mensaje
         │    │     └── Conexión al servidor destino
         │    └── Tiempo en cola active
         └── Tiempo antes de qmgr (incoming/cleanup)
  dsn=2.0.0,                       ← Delivery Status Notification
  status=sent                      ← ¡Entrega exitosa!
  (250 2.0.0 Ok: queued as XYZ789) ← Respuesta del servidor destino
```

### Códigos de status

| Status | Significado | Acción |
|--------|------------|--------|
| `sent` | Entregado exitosamente | Eliminado de la cola |
| `deferred` | Error temporal, se reintentará | Permanece en cola |
| `bounced` | Error permanente, bounce al remitente | Eliminado de la cola |
| `expired` | Expiró en la cola (maximal_queue_lifetime) | Bounce al remitente |

### Entrega fallida (deferred)

```
Mar 21 10:00:02 mail postfix/smtp[12347]: ABC123DEF: to=<user@destino.com>,
  relay=none,
  delay=30,
  delays=0.1/0.01/30/0,
  dsn=4.4.1,
  status=deferred
  (connect to mail.destino.com[203.0.113.10]:25: Connection timed out)
```

Razones comunes de defer:

```
┌──────────────────────────────────────────────────────────┐
│          Mensajes de deferred comunes                     │
│                                                          │
│  Connection timed out                                    │
│    → Firewall bloqueando, servidor caído, puerto cerrado │
│                                                          │
│  Connection refused                                      │
│    → El servidor existe pero no escucha en puerto 25     │
│                                                          │
│  Host or domain name not found                           │
│    → DNS no resuelve el dominio o no tiene MX            │
│                                                          │
│  450 4.7.1 Try again later                               │
│    → Greylisting: el servidor rechaza temporalmente      │
│      la primera conexión de un emisor desconocido        │
│                                                          │
│  450 4.1.8 Sender address rejected                       │
│    → El servidor destino rechaza tu dirección de envío   │
│                                                          │
│  454 4.7.0 TLS not available                             │
│    → El servidor no soporta TLS pero tú lo exiges       │
└──────────────────────────────────────────────────────────┘
```

### Bounce (error permanente)

```
Mar 21 10:00:02 mail postfix/smtp[12347]: ABC123DEF: to=<noexiste@destino.com>,
  relay=mail.destino.com[203.0.113.10]:25,
  dsn=5.1.1,
  status=bounced
  (host mail.destino.com said: 550 5.1.1 <noexiste@destino.com>: Recipient address rejected: User unknown)
```

Códigos DSN (Delivery Status Notification):

| Código | Significado |
|--------|------------|
| `2.x.x` | Éxito |
| `4.x.x` | Error temporal (reintentar) |
| `5.x.x` | Error permanente (no reintentar) |
| `5.1.1` | Usuario no existe |
| `5.1.2` | Dominio no existe |
| `5.7.1` | Relay denegado |
| `4.4.1` | No se puede conectar al servidor |

---

## 6. Trazar un correo por su Queue ID

El Queue ID es la clave para seguir un mensaje a través de todas las fases.

### Buscar por Queue ID

```bash
# Seguir TODO el recorrido de un mensaje
grep 'ABC123DEF' /var/log/mail.log
```

```
10:00:01 postfix/smtpd[12345]: ABC123DEF: client=laptop[192.168.1.100]
10:00:01 postfix/cleanup[12346]: ABC123DEF: message-id=<id@example.com>
10:00:01 postfix/qmgr[12340]: ABC123DEF: from=<juan@example.com>, size=2048, nrcpt=1 (queue active)
10:00:02 postfix/smtp[12347]: ABC123DEF: to=<maria@otro.com>, relay=mail.otro.com[203.0.113.10]:25, status=sent (250 Ok)
10:00:02 postfix/qmgr[12340]: ABC123DEF: removed
```

### Buscar por dirección

```bash
# Correos enviados POR un usuario
grep 'from=<juan@example.com>' /var/log/mail.log

# Correos enviados A un usuario
grep 'to=<maria@otro.com>' /var/log/mail.log

# Correos rechazados
grep 'reject' /var/log/mail.log

# Correos bounced
grep 'status=bounced' /var/log/mail.log

# Correos deferred (problemas de entrega)
grep 'status=deferred' /var/log/mail.log
```

### Buscar por rango de tiempo

```bash
# Con journalctl (más preciso)
sudo journalctl -u postfix --since "2026-03-21 09:00" --until "2026-03-21 11:00"

# Filtrar por componente
sudo journalctl -u postfix --since "1 hour ago" | grep smtp\[
```

---

## 7. Problemas de entrega comunes

### Problema 1 — Cola creciendo sin parar

```bash
# Diagnóstico
mailq | tail -1
# -- 15234 Kbytes in 847 Requests.

# ¿A qué dominios se acumula?
mailq | grep '@' | grep -v '^\s*(' | awk '{print $NF}' | \
    awk -F@ '{print $2}' | sort | uniq -c | sort -rn | head
#  500 destino-caido.com
#  200 otro-lento.com
#  147 gmail.com

# ¿Qué error reportan?
mailq | grep -A1 'destino-caido.com' | grep '(' | sort | uniq -c
#  500 (connect to mail.destino-caido.com:25: Connection timed out)

# Solución según causa:
# - Servidor destino caído → esperar o contactar al admin
# - Tu IP en lista negra → verificar en mxtoolbox
# - DNS roto → verificar resolución
# - Cola llena de spam → limpiar y revisar seguridad
```

### Problema 2 — Correo rechazado por IP en lista negra

```bash
# Log muestra:
# status=bounced (host gmail-smtp-in.l.google.com said:
# 550-5.7.1 Our system has detected that this message is likely spam)

# Verificar tu IP en listas negras
dig +short $(echo 34.216.184.93 | awk -F. '{print $4"."$3"."$2"."$1}').zen.spamhaus.org
# 127.0.0.2 → estás en la lista

# Acciones:
# 1. Verificar que no eres open relay
postconf -n | grep relay_restrictions
# 2. Verificar que no envías spam (cuenta comprometida)
grep 'sasl_username' /var/log/mail.log | awk '{print $NF}' | sort | uniq -c | sort -rn
# 3. Solicitar delisting en la web de la RBL
```

### Problema 3 — Greylisting

```bash
# Log muestra:
# status=deferred (host mail.destino.com said: 450 4.7.1
# Greylisting in effect, please retry later)

# Greylisting es una técnica anti-spam:
# El servidor destino rechaza temporalmente la primera conexión
# Un MTA legítimo reintenta (Postfix lo hace automáticamente)
# Un spammer típicamente no reintenta

# No es un error — es comportamiento normal
# El correo se entrega en el siguiente reintento (5-30 minutos)
# Verificar:
grep 'ABC123DEF' /var/log/mail.log
# Primera línea: status=deferred (greylisting)
# Segunda línea: status=sent (entregado en el reintento)
```

### Problema 4 — DNS no resuelve

```bash
# Log:
# status=deferred (Host or domain name not found.
# Name service error for name=destino.com type=MX)

# Verificar DNS desde el servidor Postfix
dig MX destino.com
# Si no resuelve → problema de DNS

# ¿Postfix usa el DNS correcto?
postconf smtp_dns_support_level
# Si es empty → usa /etc/resolv.conf

cat /etc/resolv.conf
# Verificar que los nameservers están accesibles
dig @nameserver MX destino.com
```

---

## 8. Herramientas de análisis

### pflogsumm — resumen de logs

```bash
# Instalar
sudo apt install pflogsumm    # Debian
sudo dnf install postfix-perl-scripts    # RHEL

# Generar reporte del día
sudo pflogsumm /var/log/mail.log
```

Salida (extracto):

```
Grand Totals
------------
messages

    1250   received
    1180   delivered
      45   forwarded
       0   deferred  (25  deferrals)
      25   bounced
       0   rejected (0%)
       0   reject warnings
       0   held
       0   discarded (0%)

    2048k  bytes received
    1920k  bytes delivered
      12   senders
      85   sending hosts/domains
     340   recipients
      28   recipient hosts/domains

Per-Hour Traffic Summary
          received  delivered   deferred    bounced     rejected
    0000        52        48          2          1            0
    0100        38        36          0          2            0
    ...

Host/Domain Summary: Message Delivery
  sent count  bytes   defers  avg delay  max delay  host/domain
  --------    -----   ------  ---------  ---------  -----------
       450    820k        5      1.2 s     15.0 s   gmail.com
       280    360k        8      2.5 s     45.0 s   otro.com
       ...
```

```bash
# Reporte de ayer
sudo pflogsumm --problems-first /var/log/mail.log.1

# Reporte por rango de tiempo
sudo journalctl -u postfix --since yesterday --until today | pflogsumm

# Reporte de rebotes
sudo pflogsumm --bounce-detail=10 /var/log/mail.log
```

### qshape — distribución de la cola por tiempo

```bash
# Ver la forma de la cola deferred por dominio y antigüedad
qshape deferred
```

```
                         T  5 10 20 40 80 160 320 640 1280 1280+
                 TOTAL  25  3  5  2  3  2   5   3   1    0    1
      destino-caido.com 15  0  2  1  2  2   4   2   1    0    1
           gmail.com     5  2  1  1  0  0   1   0   0    0    0
           otro.com      5  1  2  0  1  0   0   1   0    0    0
```

Las columnas son minutos de antigüedad. Si hay muchos mensajes en `1280+`, llevan más de 21 horas en cola — probablemente el destino está permanentemente caído.

```bash
# Ver cola active
qshape active

# Ver cola incoming
qshape incoming
```

### postgrep (one-liners útiles)

```bash
# Top 10 destinatarios con más correo en cola
mailq | grep '@' | grep -v '^\s*(' | awk '{print $NF}' | sort | uniq -c | sort -rn | head

# Top 10 remitentes con más correo en cola
mailq | grep '^[A-Z0-9]' | awk '{print $NF}' | sort | uniq -c | sort -rn | head

# Correos más antiguos en la cola
mailq | grep '^[A-Z0-9]' | sort -k3,4 | head

# Usuarios que más envían (hoy)
grep 'from=<' /var/log/mail.log | grep "$(date '+%b %d')" | \
    sed 's/.*from=<\([^>]*\)>.*/\1/' | sort | uniq -c | sort -rn | head
```

---

## 9. Errores comunes

### Error 1 — Usar postsuper -d ALL sin pensar

```bash
# postsuper -d ALL elimina TODA la cola, incluyendo correo legítimo
# que simplemente está esperando reintento

# Antes de borrar, investigar:
mailq | tail -1    # ¿Cuántos mensajes hay?
qshape deferred    # ¿De qué dominios?

# Si solo quieres borrar correo para un dominio caído:
mailq | grep 'destino-caido.com' -B1 | grep '^[A-Z0-9]' | \
    awk '{print $1}' | tr -d '*!' | \
    while read id; do sudo postsuper -d "$id"; done
```

### Error 2 — Interpretar deferred como fallo definitivo

```bash
# status=deferred NO es un error permanente
# Postfix seguirá reintentando hasta maximal_queue_lifetime (5 días)
# No hacer nada es a menudo la acción correcta

# Solo preocuparse si:
# - La cola crece sin control (miles de mensajes)
# - El mismo dominio acumula mensajes por días
# - Los mensajes son spam (cuenta comprometida)
```

### Error 3 — No revisar los delays

```bash
# delays=0.1/0.01/0.5/0.6 parece normal
# delays=0.1/0.01/30/0.6 indica un problema

# delays = a/b/c/d
# a = antes de qmgr (incoming)      → alto = cleanup lento
# b = tiempo en queue active         → alto = cola congestionada
# c = conexión al servidor destino   → alto = red/DNS/firewall
# d = transmisión del mensaje        → alto = red lenta o mensaje grande

# Si "c" es consistentemente 30s → Connection timed out
# → Verificar DNS, firewall, puerto 25 bloqueado por ISP
```

### Error 4 — Ignorar el log de anvil (rate limiting)

```bash
# Log:
# postfix/anvil: statistics: max connection rate 50/60s for (smtp:10.0.0.5)

# anvil registra estadísticas de conexión por cliente
# Si un solo cliente genera muchas conexiones → posible spam o loop

# Verificar:
grep 'anvil.*max' /var/log/mail.log

# Limitar si es necesario:
# smtpd_client_connection_rate_limit = 30
# smtpd_client_message_rate_limit = 50
```

### Error 5 — No monitorear la cola regularmente

```bash
# Una cola que crece es el primer síntoma de muchos problemas:
# - Servidor destino caído → cola crece para ese dominio
# - IP en lista negra → cola crece para todos
# - Cuenta comprometida → cola llena de spam
# - DNS roto → nada se entrega

# Monitoreo simple con cron:
# /etc/cron.d/check-mailq
*/15 * * * * root count=$(mailq | tail -1 | awk '{print $5}'); \
    [ "${count:-0}" -gt 100 ] && echo "Mail queue: $count messages" | \
    mail -s "ALERTA: cola de correo" admin@example.com
```

---

## 10. Cheatsheet

```bash
# ── Cola de correo ────────────────────────────────────────
mailq                                # Ver cola (= postqueue -p)
postqueue -f                         # Forzar reintento de toda la cola
postqueue -s dominio.com             # Reintento para un dominio
postqueue -j                         # Cola en JSON

# ── Gestión de cola ───────────────────────────────────────
postsuper -d QUEUE_ID                # Eliminar un mensaje
postsuper -d ALL                     # Eliminar TODA la cola
postsuper -d ALL deferred            # Eliminar solo deferred
postsuper -h QUEUE_ID                # Poner en hold
postsuper -H QUEUE_ID                # Liberar de hold
postsuper -r QUEUE_ID                # Re-encolar (reprocesar)
postcat -q QUEUE_ID                  # Ver contenido del mensaje

# ── Logs ──────────────────────────────────────────────────
tail -f /var/log/mail.log            # Debian
tail -f /var/log/maillog             # RHEL
journalctl -u postfix -f             # systemd

# ── Buscar en logs ────────────────────────────────────────
grep 'QUEUE_ID' /var/log/mail.log    # Traza completa de un mensaje
grep 'from=<user@dom>' ...          # Por remitente
grep 'to=<user@dom>' ...            # Por destinatario
grep 'status=bounced' ...            # Solo bounces
grep 'status=deferred' ...           # Solo deferred
grep 'reject' ...                    # Solo rechazados

# ── Interpretar logs ──────────────────────────────────────
# status=sent       → entregado OK
# status=deferred   → error temporal, reintentará
# status=bounced    → error permanente, NDR al remitente
# delays=a/b/c/d    → incoming/queue/conexión/transmisión
# dsn=2.x.x         → éxito
# dsn=4.x.x         → temporal
# dsn=5.x.x         → permanente

# ── Análisis ──────────────────────────────────────────────
pflogsumm /var/log/mail.log          # Resumen estadístico
qshape deferred                      # Distribución cola por tiempo
qshape active                        # Cola active

# ── One-liners útiles ────────────────────────────────────
# Top dominios en cola:
mailq | grep '@' | grep -v '^\s*(' | awk '{print $NF}' | \
    awk -F@ '{print $2}' | sort | uniq -c | sort -rn | head

# Eliminar cola para un dominio:
mailq | grep 'dominio.com' -B1 | grep '^[A-Z0-9]' | \
    awk '{print $1}' | tr -d '*!' | \
    while read id; do sudo postsuper -d "$id"; done

# Top remitentes hoy:
grep "from=<" /var/log/mail.log | grep "$(date '+%b %d')" | \
    sed 's/.*from=<\([^>]*\)>.*/\1/' | sort | uniq -c | sort -rn | head
```

---

## 11. Ejercicios

### Ejercicio 1 — Interpretar una sesión de log completa

Dado el siguiente extracto de `/var/log/mail.log`, reconstruye qué ocurrió:

```
Mar 21 10:00:01 mail postfix/smtpd[1001]: connect from laptop[192.168.1.50]
Mar 21 10:00:01 mail postfix/smtpd[1001]: A1B2C3: client=laptop[192.168.1.50], sasl_method=PLAIN, sasl_username=juan
Mar 21 10:00:01 mail postfix/cleanup[1002]: A1B2C3: message-id=<msg001@example.com>
Mar 21 10:00:01 mail postfix/qmgr[1000]: A1B2C3: from=<juan@example.com>, size=3500, nrcpt=2 (queue active)
Mar 21 10:00:02 mail postfix/smtp[1003]: A1B2C3: to=<maria@gmail.com>, relay=gmail-smtp-in.l.google.com[142.250.x.x]:25, delay=0.8, delays=0.1/0.01/0.3/0.4, dsn=2.0.0, status=sent (250 2.0.0 OK)
Mar 21 10:00:32 mail postfix/smtp[1004]: A1B2C3: to=<pedro@destino-lento.com>, relay=mail.destino-lento.com[203.0.113.5]:25, delay=31, delays=0.1/0.01/30/0.9, dsn=4.4.1, status=deferred (connect to mail.destino-lento.com[203.0.113.5]:25: Connection timed out)
```

**Pregunta de predicción:** ¿Cuántos destinatarios tenía el mensaje? ¿Cuántos se entregaron? ¿Por qué el segundo tardó 31 segundos y qué componente del delay explica el problema?

> **Respuesta:** El mensaje tenía **2 destinatarios** (`nrcpt=2`). Uno se entregó exitosamente (maria@gmail.com, `status=sent` en 0.8s). El segundo falló temporalmente (pedro@destino-lento.com, `status=deferred`). Tardó 31 segundos porque el componente **c** del delay (conexión al servidor destino) fue **30 segundos** — `delays=0.1/0.01/30/0.9`. Esto es un Connection timeout: Postfix esperó 30s tratando de conectar al puerto 25 de `destino-lento.com` y no obtuvo respuesta. El servidor destino está caído o hay un firewall bloqueando. Postfix reintentará automáticamente.

---

### Ejercicio 2 — Diagnosticar una cola llena

La cola tiene 500 mensajes. Investiga y decide la acción.

```bash
# 1. ¿Cuántos mensajes y de qué tamaño?
mailq | tail -1
# -- 2048 Kbytes in 500 Requests.

# 2. ¿Qué dominios acumulan?
mailq | grep '@' | grep -v '^\s*(' | awk '{print $NF}' | \
    awk -F@ '{print $2}' | sort | uniq -c | sort -rn | head
#  480 empresa-caida.com
#   12 gmail.com
#    8 yahoo.com

# 3. ¿Qué error reportan?
mailq | grep -A1 'empresa-caida.com' | grep '(' | head -3
# (connect to mail.empresa-caida.com:25: Connection refused)

# 4. ¿Desde cuándo?
qshape deferred
# Mayoría en columna 1280+ → llevan más de 21 horas

# 5. Decisión:
# empresa-caida.com está caído desde hace más de un día
# Los 480 mensajes no se van a entregar pronto
# Los 20 mensajes a gmail/yahoo probablemente sí se entregarán

# Acción: eliminar solo los de empresa-caida.com
mailq | grep 'empresa-caida.com' -B1 | grep '^[A-Z0-9]' | \
    awk '{print $1}' | tr -d '*!' | \
    while read id; do sudo postsuper -d "$id"; done
# Los remitentes recibirán un bounce
```

**Pregunta de predicción:** Si en vez de eliminar los 480 mensajes, simplemente esperas, ¿qué pasa cuando se cumplan los 5 días de `maximal_queue_lifetime`?

> **Respuesta:** Postfix generará automáticamente un **bounce** (NDR) para cada uno de los 480 mensajes, enviando un correo de "Undelivered Mail Returned to Sender" al remitente original de cada mensaje. Si los 480 fueron enviados por el mismo usuario, recibirá 480 correos de bounce. Si fueron enviados por un spammer con dirección falsa, los bounces irán a esa dirección falsa (backscatter), que es un problema. Eliminar proactivamente los mensajes sin esperanza evita este aluvión de bounces.

---

### Ejercicio 3 — Monitoreo básico de Postfix

Crea un script que genere un reporte diario del estado del correo.

```bash
#!/bin/bash
# /usr/local/bin/mail-report.sh
# Ejecutar con cron: 0 8 * * * /usr/local/bin/mail-report.sh

LOG="/var/log/mail.log"
YESTERDAY=$(date -d yesterday '+%b %d' | sed 's/  / /')

echo "=== Reporte de correo — $(date -d yesterday '+%Y-%m-%d') ==="
echo ""

echo "── Resumen ──"
echo "Enviados:  $(grep "$YESTERDAY" "$LOG" | grep -c 'status=sent')"
echo "Deferred:  $(grep "$YESTERDAY" "$LOG" | grep -c 'status=deferred')"
echo "Bounced:   $(grep "$YESTERDAY" "$LOG" | grep -c 'status=bounced')"
echo "Rejected:  $(grep "$YESTERDAY" "$LOG" | grep -c 'reject:')"
echo ""

echo "── Cola actual ──"
mailq | tail -1
echo ""

echo "── Top errores deferred ──"
grep "$YESTERDAY" "$LOG" | grep 'status=deferred' | \
    sed 's/.*(\(.*\))/\1/' | sort | uniq -c | sort -rn | head -5
echo ""

echo "── Top remitentes ──"
grep "$YESTERDAY" "$LOG" | grep 'from=<' | \
    sed 's/.*from=<\([^>]*\)>.*/\1/' | sort | uniq -c | sort -rn | head -5
```

```bash
chmod 700 /usr/local/bin/mail-report.sh
```

**Pregunta de predicción:** Si el log rota a medianoche (`mail.log` → `mail.log.1`) y el script se ejecuta a las 8:00, ¿los datos de ayer estarán en `mail.log` o en `mail.log.1`?

> **Respuesta:** Los datos de ayer estarán principalmente en **`mail.log.1`** — la rotación movió el log antiguo a `.1` a medianoche, y las primeras 8 horas de hoy están en `mail.log`. El script debería buscar en `mail.log.1` para datos de ayer, o mejor aún, usar `journalctl --since yesterday --until today` que no depende de la rotación de archivos.

---

> **Fin de la Sección 2 — Email.** Siguiente sección: S03 — PAM (T01 — Arquitectura PAM: módulos, stacks).
