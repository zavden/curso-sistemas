# Postfix básico

## Índice
1. [Arquitectura de Postfix](#1-arquitectura-de-postfix)
2. [Instalación](#2-instalación)
3. [main.cf — configuración principal](#3-maincf--configuración-principal)
4. [master.cf — servicios internos](#4-mastercf--servicios-internos)
5. [Configuración mínima funcional](#5-configuración-mínima-funcional)
6. [Relay — controlar quién puede enviar](#6-relay--controlar-quién-puede-enviar)
7. [Aliases y redirecciones](#7-aliases-y-redirecciones)
8. [TLS en Postfix](#8-tls-en-postfix)
9. [SMTP AUTH (SASL)](#9-smtp-auth-sasl)
10. [Errores comunes](#10-errores-comunes)
11. [Cheatsheet](#11-cheatsheet)
12. [Ejercicios](#12-ejercicios)

---

## 1. Arquitectura de Postfix

Postfix no es un único proceso monolítico. Es un conjunto de **procesos especializados** que se comunican a través de colas (queues) internas. Cada proceso tiene una función mínima y se ejecuta con los permisos mínimos necesarios.

```
┌──────────────────────────────────────────────────────────┐
│              Arquitectura de Postfix                      │
│                                                          │
│  Red (SMTP)        Local (sendmail)      Maildrop        │
│       │                  │                  │            │
│       ▼                  ▼                  ▼            │
│  ┌─────────┐       ┌──────────┐       ┌─────────┐      │
│  │  smtpd  │       │ sendmail │       │ pickup  │       │
│  │ (puerto │       │ (CLI)    │       │         │       │
│  │  25/587)│       │          │       │         │       │
│  └────┬────┘       └────┬─────┘       └────┬────┘      │
│       │                 │                   │           │
│       └────────┬────────┘───────────────────┘           │
│                ▼                                        │
│          ┌──────────┐                                   │
│          │ cleanup  │  ← Agrega headers, reescribe      │
│          └────┬─────┘    direcciones                    │
│               ▼                                        │
│          ┌──────────┐                                   │
│          │  qmgr    │  ← Queue manager (central)        │
│          │          │    Decide cómo entregar            │
│          └────┬─────┘                                   │
│               │                                        │
│       ┌───────┼───────┐                                 │
│       ▼       ▼       ▼                                 │
│  ┌────────┐ ┌─────┐ ┌──────┐                           │
│  │  smtp  │ │local│ │virtual│                           │
│  │(client)│ │     │ │      │                            │
│  │ Envía  │ │Buzón│ │Buzón │                            │
│  │ a otro │ │local│ │virt. │                            │
│  │ MTA    │ │     │ │      │                            │
│  └────────┘ └─────┘ └──────┘                            │
│                                                         │
│  Cada caja es un PROCESO SEPARADO                       │
│  Ninguno corre como root (excepto master)               │
└──────────────────────────────────────────────────────────┘
```

### Procesos principales

| Proceso | Función |
|---------|---------|
| `master` | Proceso padre, lanza y supervisa todos los demás |
| `smtpd` | Recibe correo vía SMTP (puerto 25/587) |
| `smtp` | Envía correo a otros servidores vía SMTP |
| `cleanup` | Procesa headers, aplica reescrituras, inserta en cola |
| `qmgr` | Queue manager — decide a dónde va cada mensaje |
| `local` | Entrega a buzones locales Unix |
| `virtual` | Entrega a buzones de dominios virtuales |
| `pickup` | Recoge correo del directorio maildrop (enviado vía CLI) |
| `bounce` | Genera mensajes de rebote (NDR) |
| `trivial-rewrite` | Reescribe direcciones (canonical, virtual maps) |

---

## 2. Instalación

### Debian/Ubuntu

```bash
# Instalar Postfix
sudo apt install postfix

# Durante la instalación, seleccionar:
# - "Internet Site" para un servidor que envía/recibe directamente
# - "System mail name": el dominio principal (example.com)

# Si necesitas reconfigurar:
sudo dpkg-reconfigure postfix
```

### RHEL/Fedora

```bash
# Instalar Postfix (suele venir preinstalado)
sudo dnf install postfix

# Establecer como MTA por defecto (si hay varios instalados)
sudo alternatives --set mta /usr/sbin/sendmail.postfix

# Habilitar e iniciar
sudo systemctl enable --now postfix
```

### Verificar instalación

```bash
# Versión
postconf mail_version
# mail_version = 3.8.x

# Estado del servicio
sudo systemctl status postfix

# Verificar que escucha en el puerto 25
ss -tlnp | grep :25
```

---

## 3. main.cf — configuración principal

`/etc/postfix/main.cf` es el archivo de configuración central de Postfix. Define el comportamiento del servidor mediante pares `parámetro = valor`.

### Ubicación y sintaxis

```bash
# Archivo principal
/etc/postfix/main.cf

# Sintaxis
parametro = valor
parametro = valor1, valor2, valor3    # Lista separada por comas
parametro =                           # Valor vacío (deshabilita)

# Comentarios
# Esto es un comentario

# Continuación de línea (indentación)
parametro = valor1,
    valor2,
    valor3

# Los parámetros NO son case-sensitive
# myhostname = MYhostname (son iguales)
```

### postconf — consultar y modificar configuración

```bash
# Ver TODOS los parámetros activos (con defaults)
postconf

# Ver solo los que difieren del default
postconf -n

# Ver un parámetro específico
postconf myhostname
# myhostname = mail.example.com

# Ver el valor default de un parámetro
postconf -d myhostname

# Modificar un parámetro (edita main.cf)
sudo postconf -e 'myhostname = mail.example.com'

# Verificar sintaxis
sudo postfix check
```

`postconf -n` es fundamental: muestra la configuración **efectiva** sin ruido de defaults. Es lo primero que hay que revisar al diagnosticar problemas.

### Parámetros de identidad

```bash
# Hostname completo del servidor (FQDN)
myhostname = mail.example.com

# Dominio del correo (parte después del @ para correo local)
mydomain = example.com

# Dirección de origen para correo generado localmente
myorigin = $mydomain
# Un correo de "root" aparecerá como root@example.com

# Banner SMTP (lo que ve el cliente al conectar)
smtpd_banner = $myhostname ESMTP
# No revelar versión de Postfix (seguridad)
```

### Parámetros de red

```bash
# Interfaces donde Postfix escucha
inet_interfaces = all              # Todas las interfaces
# inet_interfaces = loopback-only  # Solo localhost (satellite)

# Protocolos IP
inet_protocols = all               # IPv4 + IPv6
# inet_protocols = ipv4            # Solo IPv4

# Dominios para los que este servidor es destino final
mydestination = $myhostname, localhost.$mydomain, localhost, $mydomain
# Correo para estos dominios se entrega localmente

# Redes confiables (pueden hacer relay sin autenticación)
mynetworks = 127.0.0.0/8 [::1]/128 192.168.1.0/24
```

### Parámetros de entrega

```bash
# Transporte de entrega para correo local
mailbox_command = /usr/lib/dovecot/deliver    # Usar Dovecot LDA
# o
home_mailbox = Maildir/                        # Formato Maildir directo
# o (default)
# mail_spool_directory = /var/mail              # mbox en /var/mail/user
```

---

## 4. master.cf — servicios internos

`/etc/postfix/master.cf` define qué procesos de Postfix se ejecutan y cómo. Cada línea es un servicio.

### Formato

```
# service  type  private  unpriv  chroot  wakeup  maxproc  command
smtp       inet  n        -       y       -       -        smtpd
pickup     unix  n        -       y       60      1        pickup
cleanup    unix  n        -       y       -       0        cleanup
qmgr       unix  n        -       n       300     1        qmgr
```

| Campo | Significado |
|-------|------------|
| service | Nombre del servicio (o puerto) |
| type | `inet` (red) o `unix` (socket) |
| private | ¿Acceso restringido al sistema de correo? |
| unpriv | ¿Ejecutar sin privilegios? |
| chroot | ¿Ejecutar en chroot? |
| wakeup | Intervalo de activación (segundos) |
| maxproc | Máximo de procesos simultáneos |
| command | Programa a ejecutar |

### Habilitar submission (puerto 587)

```bash
# En /etc/postfix/master.cf, descomentar o agregar:
submission inet n       -       y       -       -       smtpd
  -o syslog_name=postfix/submission
  -o smtpd_tls_security_level=encrypt
  -o smtpd_sasl_auth_enable=yes
  -o smtpd_relay_restrictions=permit_sasl_authenticated,reject
  -o smtpd_recipient_restrictions=permit_sasl_authenticated,reject
```

Esto crea un servicio SMTP en el puerto 587 que:
- Requiere TLS (`encrypt`)
- Requiere autenticación SASL (`sasl_auth_enable=yes`)
- Solo permite relay a usuarios autenticados

### Aplicar cambios

```bash
# Después de modificar main.cf o master.cf:
sudo postfix reload      # Recarga sin desconectar sesiones activas

# O reiniciar completamente:
sudo systemctl restart postfix

# Verificar que no hay errores:
sudo postfix check
```

---

## 5. Configuración mínima funcional

### Servidor para enviar correo local (satellite system)

El caso más simple: un servidor que envía correo del sistema (cron, alertas) a través de un relay externo, sin recibir correo de Internet.

```bash
# /etc/postfix/main.cf — satellite system

# Identidad
myhostname = server1.example.com
mydomain = example.com
myorigin = $mydomain

# Solo escuchar en localhost
inet_interfaces = loopback-only

# No somos destino final de ningún dominio
mydestination =

# Enviar todo a través del relay del ISP o smarthost
relayhost = [smtp.example.com]:587

# Solo localhost puede enviar
mynetworks = 127.0.0.0/8 [::1]/128

# Autenticación con el relay
smtp_sasl_auth_enable = yes
smtp_sasl_password_maps = hash:/etc/postfix/sasl_passwd
smtp_sasl_security_options = noanonymous
smtp_tls_security_level = encrypt
```

```bash
# Crear archivo de credenciales para el relay
sudo bash -c 'echo "[smtp.example.com]:587 usuario:contraseña" > /etc/postfix/sasl_passwd'
sudo chmod 600 /etc/postfix/sasl_passwd
sudo postmap /etc/postfix/sasl_passwd
sudo postfix reload
```

### Servidor que recibe correo para un dominio

```bash
# /etc/postfix/main.cf — Internet site

# Identidad
myhostname = mail.example.com
mydomain = example.com
myorigin = $mydomain

# Escuchar en todas las interfaces
inet_interfaces = all
inet_protocols = all

# Dominios para los que aceptamos correo
mydestination = $myhostname, localhost.$mydomain, localhost, $mydomain

# Redes confiables
mynetworks = 127.0.0.0/8 [::1]/128

# No relay a través de otro servidor
relayhost =

# Entrega en formato Maildir
home_mailbox = Maildir/

# Restricciones de relay (CRÍTICO — anti open relay)
smtpd_relay_restrictions =
    permit_mynetworks,
    permit_sasl_authenticated,
    defer_unauth_destination

# Tamaño máximo de mensaje (50 MB)
message_size_limit = 52428800
```

### Probar el envío

```bash
# Enviar correo de prueba
echo "Test desde Postfix" | mail -s "Prueba" root@localhost

# Verificar la cola
mailq

# Verificar el log
sudo journalctl -u postfix --since "5 minutes ago"

# Leer el buzón
cat /var/mail/root
# o si usas Maildir:
ls ~/Maildir/new/
```

---

## 6. Relay — controlar quién puede enviar

**Relay** es cuando Postfix acepta un correo y lo reenvía a otro servidor (porque el destino no es local). Un servidor mal configurado que permite relay sin restricción es un **open relay** — será usado para spam y bloqueado en horas.

### Restricciones de relay

```bash
# main.cf — la directiva MÁS IMPORTANTE de seguridad
smtpd_relay_restrictions =
    permit_mynetworks,              # Redes en mynetworks pueden hacer relay
    permit_sasl_authenticated,      # Usuarios autenticados pueden hacer relay
    defer_unauth_destination        # Todo lo demás → rechazar
```

```
┌──────────────────────────────────────────────────────────┐
│          Evaluación de relay restrictions                  │
│                                                          │
│  Correo llega de 10.0.0.5 para user@gmail.com            │
│  (gmail.com NO está en mydestination → es relay)          │
│                                                          │
│  1. permit_mynetworks                                    │
│     ¿10.0.0.5 está en mynetworks?                        │
│     ├── Sí → PERMIT (puede enviar)                       │
│     └── No → siguiente regla                             │
│                                                          │
│  2. permit_sasl_authenticated                            │
│     ¿El cliente se autenticó con SMTP AUTH?               │
│     ├── Sí → PERMIT                                      │
│     └── No → siguiente regla                             │
│                                                          │
│  3. defer_unauth_destination                             │
│     El destino no es local y no está autorizado           │
│     → DEFER (rechazar temporalmente, código 450)         │
│                                                          │
│  Si ninguna regla permite → el relay se DENIEGA ✓        │
└──────────────────────────────────────────────────────────┘
```

### Restricciones de destinatario

```bash
# Control más granular sobre quién puede recibir qué
smtpd_recipient_restrictions =
    permit_mynetworks,
    permit_sasl_authenticated,
    reject_unauth_destination,        # Rechazar relay no autorizado
    reject_unknown_recipient_domain,  # Rechazar dominios que no existen
    reject_non_fqdn_recipient         # Rechazar direcciones sin dominio
```

### relayhost — enviar a través de otro servidor

```bash
# Todo el correo saliente pasa por este servidor
relayhost = [smtp.isp.com]           # Corchetes = no buscar MX
relayhost = [smtp.isp.com]:587       # Puerto específico

# Sin corchetes = Postfix busca el MX del dominio
relayhost = isp.com                  # Busca MX de isp.com

# Vacío = entrega directa consultando MX del destino
relayhost =                          # Servidor autónomo
```

### relay_domains — dominios para los que hacemos relay

```bash
# Aceptar correo para estos dominios y reenviarlo
# (diferente de mydestination — no se entrega localmente)
relay_domains = subdomain.example.com, partner.com

# Se usa con transport_maps para definir a dónde reenviarlo
transport_maps = hash:/etc/postfix/transport
```

```bash
# /etc/postfix/transport
subdomain.example.com    smtp:[internal-mail.example.com]
partner.com              smtp:[mail.partner.com]:25
```

```bash
sudo postmap /etc/postfix/transport
sudo postfix reload
```

### Verificar que NO eres open relay

```bash
# Prueba manual con telnet desde un equipo EXTERNO:
telnet mail.example.com 25
EHLO test
MAIL FROM:<test@evil.com>
RCPT TO:<victima@gmail.com>
# Debe responder: 454 Relay access denied
# Si responde 250 OK → ERES OPEN RELAY — corregir inmediatamente
```

---

## 7. Aliases y redirecciones

### /etc/aliases — redirección local

```bash
# /etc/aliases — redirecciones de correo local
# Formato: destino_local: destino_real

# Redirigir correo de root a un usuario real
root: admin

# Redirigir a múltiples usuarios
webmaster: juan, maria

# Redirigir a dirección externa
soporte: soporte@empresa.com

# Descartar correo (black hole)
noreply: /dev/null

# Enviar a un programa (pipe)
tickets: |/usr/local/bin/process-ticket.sh
```

```bash
# IMPORTANTE: después de editar /etc/aliases, ejecutar:
sudo newaliases
# Esto regenera /etc/aliases.db (formato hash)

# Verificar
postconf alias_maps
# alias_maps = hash:/etc/aliases

# Probar
echo "test" | mail -s "Prueba alias" root
# → Se entrega a "admin" según el alias
```

### virtual_alias_maps — redirecciones por dominio

```bash
# /etc/postfix/main.cf
virtual_alias_maps = hash:/etc/postfix/virtual

# /etc/postfix/virtual
# Correo para info@example.com va a juan
info@example.com    juan@example.com

# Catch-all (todo el correo de un dominio)
@example.com        admin@example.com

# Reescritura de dominio completo
@viejo-dominio.com  @nuevo-dominio.com
```

```bash
# Generar la base de datos
sudo postmap /etc/postfix/virtual
sudo postfix reload
```

### canonical_maps — reescribir direcciones

```bash
# Cambiar la dirección "From:" de correo saliente
# Útil cuando el hostname interno no es el dominio público

# /etc/postfix/main.cf
sender_canonical_maps = hash:/etc/postfix/sender_canonical

# /etc/postfix/sender_canonical
root@server1.internal    noreply@example.com
@internal.example.com    @example.com
```

```bash
sudo postmap /etc/postfix/sender_canonical
sudo postfix reload
```

---

## 8. TLS en Postfix

### TLS para correo entrante (smtpd)

```bash
# /etc/postfix/main.cf

# Certificado y clave
smtpd_tls_cert_file = /etc/letsencrypt/live/mail.example.com/fullchain.pem
smtpd_tls_key_file = /etc/letsencrypt/live/mail.example.com/privkey.pem

# Nivel de seguridad para SMTP entrante
smtpd_tls_security_level = may
# may = ofrecer STARTTLS pero no exigirlo (otros MTAs pueden no soportar TLS)
# encrypt = exigir TLS (solo para submission/587, NO para puerto 25)

# Protocolos y cifrados
smtpd_tls_mandatory_protocols = !SSLv2, !SSLv3, !TLSv1, !TLSv1.1
smtpd_tls_protocols = !SSLv2, !SSLv3, !TLSv1, !TLSv1.1

# Log TLS
smtpd_tls_loglevel = 1    # 0=off, 1=resumen, 2=detalle
```

### TLS para correo saliente (smtp)

```bash
# TLS al enviar a otros servidores
smtp_tls_security_level = may
# may = usar TLS si el servidor destino lo soporta (oportunístico)
# encrypt = exigir TLS (puede causar que no se entregue correo)
# dane = verificar certificados vía DNSSEC/DANE (más seguro)

smtp_tls_loglevel = 1

# Caché de sesiones TLS (rendimiento)
smtp_tls_session_cache_database = btree:${data_directory}/smtp_scache
smtpd_tls_session_cache_database = btree:${data_directory}/smtpd_scache
```

### Por qué `may` y no `encrypt` en puerto 25

```
┌──────────────────────────────────────────────────────────┐
│     smtpd_tls_security_level en puerto 25                 │
│                                                          │
│  Puerto 25 (MTA ↔ MTA):                                 │
│    DEBE ser "may" (oportunístico)                        │
│    Si pones "encrypt", los servidores que no soporten    │
│    TLS no podrán entregarte correo → pierdes correo      │
│                                                          │
│  Puerto 587 (submission, MUA → MTA):                     │
│    DEBE ser "encrypt"                                    │
│    Los MUAs modernos siempre soportan TLS                │
│    No hay razón para permitir texto plano                │
│                                                          │
│  Configurar en master.cf para submission:                │
│  submission inet n - y - - smtpd                         │
│    -o smtpd_tls_security_level=encrypt                   │
└──────────────────────────────────────────────────────────┘
```

---

## 9. SMTP AUTH (SASL)

SMTP AUTH permite que los usuarios se autentiquen antes de enviar correo. Postfix delega la autenticación a **SASL** (Dovecot SASL o Cyrus SASL).

### Configurar con Dovecot SASL (recomendado)

```bash
# /etc/postfix/main.cf
smtpd_sasl_auth_enable = yes
smtpd_sasl_type = dovecot
smtpd_sasl_path = private/auth

# No permitir autenticación anónima
smtpd_sasl_security_options = noanonymous

# No anunciar AUTH sin TLS activo
smtpd_tls_auth_only = yes
```

```bash
# /etc/dovecot/conf.d/10-master.conf
# Configurar el socket SASL para Postfix
service auth {
    unix_listener /var/spool/postfix/private/auth {
        mode = 0660
        user = postfix
        group = postfix
    }
}
```

```bash
# /etc/dovecot/conf.d/10-auth.conf
auth_mechanisms = plain login
```

### Flujo de autenticación

```
MUA (Thunderbird)                    Postfix           Dovecot
      │                                │                  │
      │ EHLO → 250 AUTH PLAIN LOGIN    │                  │
      │ STARTTLS                       │                  │
      │ AUTH PLAIN base64(user:pass) ──►│                  │
      │                                │──verify──►       │
      │                                │◄──OK────         │
      │ ◄── 235 Authentication OK      │                  │
      │ MAIL FROM / RCPT TO / DATA     │                  │
```

### Autenticación para envío saliente (smarthost)

Cuando Postfix envía a través de un relay que requiere autenticación:

```bash
# /etc/postfix/main.cf
relayhost = [smtp.gmail.com]:587
smtp_sasl_auth_enable = yes
smtp_sasl_password_maps = hash:/etc/postfix/sasl_passwd
smtp_sasl_security_options = noanonymous
smtp_tls_security_level = encrypt

# /etc/postfix/sasl_passwd
[smtp.gmail.com]:587    usuario@gmail.com:app_password
```

```bash
sudo chmod 600 /etc/postfix/sasl_passwd
sudo postmap /etc/postfix/sasl_passwd
sudo postfix reload
```

---

## 10. Errores comunes

### Error 1 — Open relay por smtpd_relay_restrictions vacío

```bash
# MAL — sin restricciones, acepta relay de cualquiera
smtpd_relay_restrictions =

# BIEN — restricción mínima
smtpd_relay_restrictions =
    permit_mynetworks,
    permit_sasl_authenticated,
    defer_unauth_destination
```

Verificar siempre con `postconf -n | grep relay` después de cambiar la configuración.

### Error 2 — Olvidar postmap después de editar tablas

```bash
# Editaste /etc/postfix/virtual pero no ejecutaste postmap
# Postfix sigue usando la versión anterior de la base de datos

# Archivos que necesitan postmap:
sudo postmap /etc/postfix/virtual          # virtual_alias_maps
sudo postmap /etc/postfix/transport        # transport_maps
sudo postmap /etc/postfix/sasl_passwd      # smtp_sasl_password_maps
sudo postmap /etc/postfix/sender_canonical # sender_canonical_maps

# /etc/aliases usa newaliases (no postmap):
sudo newaliases
```

### Error 3 — mydestination incorrecto

```bash
# Si mydestination no incluye tu dominio, Postfix no acepta correo
# para usuarios locales de ese dominio

# Verificar:
postconf mydestination
# mydestination = localhost  ← falta el dominio

# Correcto para un servidor de correo:
mydestination = $myhostname, localhost.$mydomain, localhost, $mydomain
```

### Error 4 — smtpd_tls_security_level = encrypt en puerto 25

```bash
# Si pones encrypt en puerto 25:
smtpd_tls_security_level = encrypt

# Servidores que no soporten TLS no pueden entregarte correo
# Resultado: correos legítimos rebotados

# Correcto: may en puerto 25, encrypt solo en submission (587)
smtpd_tls_security_level = may
# Y en master.cf para submission:
# submission inet n - y - - smtpd
#   -o smtpd_tls_security_level=encrypt
```

### Error 5 — Correo sale como root@hostname.localdomain

```bash
# El correo del sistema (cron, logwatch) aparece como
# root@server1.localdomain en lugar de root@example.com

# Falta configurar myorigin:
myorigin = $mydomain

# Y/o configurar canonical maps para reescribir:
sender_canonical_maps = hash:/etc/postfix/sender_canonical
# root@server1.localdomain    noreply@example.com
```

---

## 11. Cheatsheet

```bash
# ── Archivos ──────────────────────────────────────────────
/etc/postfix/main.cf                 # Configuración principal
/etc/postfix/master.cf               # Servicios (smtpd, smtp, etc.)
/etc/aliases                         # Aliases locales
/etc/postfix/virtual                 # Aliases virtuales
/etc/postfix/transport               # Rutas de entrega
/etc/postfix/sasl_passwd             # Credenciales para relay

# ── postconf ──────────────────────────────────────────────
postconf -n                          # Config efectiva (sin defaults)
postconf parametro                   # Ver un parámetro
postconf -d parametro                # Ver default
postconf -e 'param = valor'          # Modificar parámetro
postfix check                        # Verificar sintaxis

# ── Operaciones ──────────────────────────────────────────
postfix reload                       # Recargar config
postfix start / stop                 # Iniciar / parar
systemctl restart postfix            # Reiniciar vía systemd
newaliases                           # Regenerar /etc/aliases.db
postmap /etc/postfix/ARCHIVO         # Regenerar tabla hash

# ── main.cf — identidad ──────────────────────────────────
# myhostname = mail.example.com
# mydomain = example.com
# myorigin = $mydomain
# smtpd_banner = $myhostname ESMTP

# ── main.cf — red ────────────────────────────────────────
# inet_interfaces = all              # Dónde escuchar
# mydestination = $myhostname, localhost, $mydomain
# mynetworks = 127.0.0.0/8 192.168.1.0/24
# relayhost = [smtp.isp.com]:587    # Smarthost

# ── main.cf — seguridad (anti open relay) ────────────────
# smtpd_relay_restrictions =
#     permit_mynetworks,
#     permit_sasl_authenticated,
#     defer_unauth_destination

# ── main.cf — TLS ────────────────────────────────────────
# smtpd_tls_cert_file = /path/fullchain.pem
# smtpd_tls_key_file = /path/privkey.pem
# smtpd_tls_security_level = may    # Puerto 25
# smtp_tls_security_level = may     # Saliente

# ── main.cf — SASL ───────────────────────────────────────
# smtpd_sasl_auth_enable = yes
# smtpd_sasl_type = dovecot
# smtpd_sasl_path = private/auth
# smtpd_tls_auth_only = yes

# ── Entrega ───────────────────────────────────────────────
# home_mailbox = Maildir/            # Formato Maildir
# mailbox_command = /usr/lib/dovecot/deliver  # Dovecot LDA

# ── Pruebas ───────────────────────────────────────────────
echo "test" | mail -s "Subj" user    # Enviar correo local
mailq                                # Ver cola de mensajes
postqueue -f                         # Forzar reintento de la cola
postsuper -d ALL                     # Borrar toda la cola
```

---

## 12. Ejercicios

### Ejercicio 1 — Configurar un satellite system

Configura un servidor que envíe alertas del sistema a través de un relay externo (smarthost).

```bash
# 1. Instalar
sudo apt install postfix libsasl2-modules

# 2. Configurar main.cf
sudo postconf -e 'myhostname = server1.example.com'
sudo postconf -e 'mydomain = example.com'
sudo postconf -e 'myorigin = $mydomain'
sudo postconf -e 'inet_interfaces = loopback-only'
sudo postconf -e 'mydestination ='
sudo postconf -e 'relayhost = [smtp.gmail.com]:587'
sudo postconf -e 'mynetworks = 127.0.0.0/8 [::1]/128'
sudo postconf -e 'smtp_sasl_auth_enable = yes'
sudo postconf -e 'smtp_sasl_password_maps = hash:/etc/postfix/sasl_passwd'
sudo postconf -e 'smtp_sasl_security_options = noanonymous'
sudo postconf -e 'smtp_tls_security_level = encrypt'

# 3. Crear credenciales
echo '[smtp.gmail.com]:587 user@gmail.com:app_password' | \
    sudo tee /etc/postfix/sasl_passwd
sudo chmod 600 /etc/postfix/sasl_passwd
sudo postmap /etc/postfix/sasl_passwd

# 4. Recargar y probar
sudo postfix reload
echo "Alerta de prueba" | mail -s "Test satellite" admin@example.com
mailq    # Verificar que la cola se vacía
```

**Pregunta de predicción:** Si `mydestination` está vacío y un cron job envía correo a `root@server1.example.com`, ¿se entrega localmente o se envía al relay?

> **Respuesta:** Se envía al relay. Con `mydestination` vacío, Postfix no considera ningún dominio como local — todo el correo se reenvía a través del `relayhost`. Para que el correo a `root` se entregue localmente, `server1.example.com` o `localhost` deben estar en `mydestination`. En un satellite system, esto es intencional: quieres que las alertas salgan al exterior.

---

### Ejercicio 2 — Aliases y redirecciones

Configura aliases para que el correo del sistema llegue al administrador correcto.

```bash
# 1. Editar /etc/aliases
sudo tee -a /etc/aliases << 'EOF'
# Redirigir correo del sistema
root: admin
postmaster: admin
abuse: admin
webmaster: admin, juan

# Redirigir a dirección externa
alertas: oncall@empresa.com

# Descartar
noreply: /dev/null
EOF

# 2. Regenerar base de datos
sudo newaliases

# 3. Probar
echo "test alias" | mail -s "Prueba" root
# → Se entrega a "admin"

echo "test webmaster" | mail -s "Prueba" webmaster
# → Se entrega a "admin" Y a "juan"
```

**Pregunta de predicción:** Si `root: admin` y `admin: juan`, ¿un correo a `root` llega a `juan`? ¿Hay límite de cuántos niveles de redirección puede seguir Postfix?

> **Respuesta:** Sí, Postfix sigue la cadena: root → admin → juan. El correo se entrega al buzón de `juan`. Postfix tiene un límite configurable de profundidad de alias (`virtual_alias_recursion_limit` y `alias_maps_recursion_limit`, default 1000), pero en la práctica una cadena de 2-3 niveles es lo habitual. Si se crea un loop (a → b → a), Postfix lo detecta y genera un bounce.

---

### Ejercicio 3 — Verificar seguridad de relay

Comprueba que tu servidor Postfix NO es un open relay.

```bash
# 1. Ver configuración de relay
postconf -n | grep -E "relay_restrictions|mynetworks|relay_domains"

# 2. Verificar desde un equipo EXTERNO (fuera de mynetworks)
telnet mail.example.com 25
EHLO test.external.com
MAIL FROM:<test@external.com>
RCPT TO:<victima@gmail.com>
# Respuesta esperada: 454 4.7.1: Relay access denied ✓

# 3. Verificar que SÍ acepta correo para dominios locales
RCPT TO:<usuario@example.com>
# Respuesta esperada: 250 Ok ✓ (si example.com está en mydestination)

QUIT

# 4. Verificar con herramientas online
# mxtoolbox.com → SMTP Open Relay Test
```

**Pregunta de predicción:** Si `mynetworks` incluye `0.0.0.0/0`, ¿qué ocurre con la regla `permit_mynetworks`?

> **Respuesta:** `0.0.0.0/0` incluye **todas** las IPs del mundo. La regla `permit_mynetworks` permitiría relay desde cualquier IP sin autenticación — tu servidor sería un **open relay completo**. `mynetworks` debe contener SOLO las redes que controlas (localhost, tu LAN). Nunca incluir `0.0.0.0/0` ni rangos públicos amplios.

---

> **Siguiente:** T03 — mailq y logs (diagnóstico de problemas de entrega).
