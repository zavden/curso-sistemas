# T03 — Envío remoto

> **Fuentes:** `README.md`, `README.max.md`, `LABS.md`, `labs/README.md`
> **Regla aplicada:** 2 (`.max.md` sin `.claude.md` → crear `.claude.md`)

---

## Erratas detectadas

| # | Archivo | Línea(s) | Error | Corrección |
|---|---------|----------|-------|------------|
| 1 | README.max.md | 32 | "ARQUITURA DE LOGS CENTRALIZADOS" | **ARQUITECTURA** (falta la C) |
| 2 | README.max.md | 347–348 | Dos líneas `streamDriver.authMode` en el mismo bloque `module()`: `"x509/name"` y `"x509/fingerprint"` | Son alternativas mutuamente excluyentes; la segunda sobreescribe la primera. Solo debe incluirse una. El comentario dice "alternativa" pero al estar ambas activas, solo aplica `x509/fingerprint`. `README.md:257` lo hace correctamente con una sola |
| 3 | README.max.md | 101 | "even si rsyslog se reinicia" | Mezcla inglés/español. Debe ser **"incluso si"** o **"aun si"** |

---

## Por qué centralizar logs

```
┌────────────────────────────────────────────────────────────────────────┐
│                PROBLEMAS CON LOGS SOLO LOCALES                         │
│                                                                        │
│  1. COMPROMISO → Atacante borra/modifica logs → pierdes evidencia     │
│  2. CRASH      → Si el disco falla, los logs se pierden               │
│  3. CORRELACIÓN → Un ataque afecta 10 servidores, debes ir a cada uno │
│  4. MONITOREO  → Herramientas (ELK, Splunk, Grafana) esperan un flujo│
└────────────────────────────────────────────────────────────────────────┘

┌────────────────────────────────────────────────────────────────────────┐
│                 ARQUITECTURA DE LOGS CENTRALIZADOS                     │
│                                                                        │
│    web01 ──┐                                                          │
│    web02 ──┼── TCP/TLS ──→  SERVIDOR CENTRAL DE LOGS                 │
│    db01  ──┤                  logserver.example.com                    │
│    app01 ──┘                    │                                      │
│                                 ├── /var/log/remote/web01/            │
│                                 ├── /var/log/remote/web02/            │
│                                 ├── /var/log/remote/db01/             │
│                                 └── /var/log/remote/app01/            │
└────────────────────────────────────────────────────────────────────────┘
```

---

## Protocolos de transporte

| Protocolo | Sintaxis | Puerto | Fiabilidad | Cifrado | Uso recomendado |
|-----------|----------|--------|------------|---------|-----------------|
| **UDP** | `@servidor:514` | 514 | Baja (puede perder) | No | LAN, logs no críticos |
| **TCP** | `@@servidor:514` | 514 | Media (retransmisión) | No | Producción básica |
| **TLS/TCP** | `streamDriver="gtls"` | 6514 | Media | Sí | Producción segura |
| **RELP** | `action(type="omrelp")` | 2514 | Alta (garantía end-to-end) | Opcional | Misión crítica |

### UDP — Un @

```bash
# Fire-and-forget: sin conexión, sin confirmación
*.* @logserver.example.com:514

# Rápido, menos overhead, pero PUEDE PERDER mensajes
# Protocolo syslog original (RFC 3164)
```

### TCP — Dos @@

```bash
# Conexión persistente: retransmisión si falla
*.* @@logserver.example.com:514

# Más confiable que UDP, pero sin cifrado
# Recomendado sobre UDP para producción
```

### RELP — Reliable Event Logging Protocol

```bash
# Garantiza entrega end-to-end:
# - El cliente sabe exactamente qué mensajes fueron recibidos
# - Si rsyslog se reinicia, los mensajes pendientes se reenvían
# - No hay pérdida de mensajes

# Requiere módulo adicional:
sudo apt install rsyslog-relp    # Debian/Ubuntu
sudo dnf install rsyslog-relp    # RHEL/Fedora
```

### Diferencia clave: sobrevivir reinicios

| Escenario | UDP | TCP | RELP |
|-----------|-----|-----|------|
| Servidor caído momentáneamente | Mensajes perdidos | Mensajes perdidos (sin queue) | Mensajes retenidos |
| rsyslog del cliente se reinicia | Perdidos | Perdidos | RELP sabe qué falta |
| Con queue de disco + TCP | Guardados en disco | Guardados en disco | Guardados + verificación |

> La queue de disco es una funcionalidad de **rsyslog**, no del protocolo. RELP es el único protocolo que por sí mismo trackea qué mensajes fueron entregados.

---

## Configuración del cliente (emisor)

### UDP — Configuración mínima

```bash
# /etc/rsyslog.d/60-remote.conf

# Enviar TODO por UDP:
*.* @logserver.example.com:514

# Solo errores y superiores:
*.err @logserver.example.com:514

# Solo auth:
auth,authpriv.* @logserver.example.com:514
```

### TCP — Configuración básica

```bash
# /etc/rsyslog.d/60-remote.conf

# Sintaxis legacy:
*.* @@logserver.example.com:514

# RainerScript (más control):
*.* action(type="omfwd"
           target="logserver.example.com"
           port="514"
           protocol="tcp")
```

### TCP con queue de disco (producción)

Sin queue, si el servidor está caído los mensajes se pierden. La queue de disco los almacena hasta que el servidor vuelva:

```bash
# /etc/rsyslog.d/60-remote.conf
action(type="omfwd"
       target="logserver.example.com"
       port="514"
       protocol="tcp"

       # Queue de disco (si el servidor no responde):
       queue.type="LinkedList"           # queue en memoria con spillover a disco
       queue.filename="fwd-logserver"    # prefijo de archivos en /var/lib/rsyslog/
       queue.maxDiskSpace="1g"           # máximo 1GB de cola en disco
       queue.size="10000"               # máximo de mensajes en memoria
       queue.saveOnShutdown="on"         # guardar cola al apagar rsyslog

       # Reintentos:
       action.resumeRetryCount="-1"      # reintentar indefinidamente
       action.resumeInterval="30"        # cada 30 segundos
       )
```

Comportamiento:
1. Intenta enviar por TCP
2. Si el servidor no responde → almacena en cola de disco (`/var/lib/rsyslog/fwd-logserver*`)
3. Reintenta cada 30 segundos indefinidamente (`-1`)
4. Cuando el servidor vuelve → envía los mensajes acumulados
5. Al apagar rsyslog → guarda la cola en disco (`saveOnShutdown`)

### Enviar a múltiples servidores

```bash
# /etc/rsyslog.d/60-remote.conf

# Servidor primario:
*.* action(type="omfwd"
           target="logserver1.example.com"
           port="514"
           protocol="tcp"
           queue.type="LinkedList"
           queue.filename="fwd-primary"
           queue.maxDiskSpace="500m"
           queue.saveOnShutdown="on"
           action.resumeRetryCount="-1")

# Servidor secundario (copia):
*.* action(type="omfwd"
           target="logserver2.example.com"
           port="514"
           protocol="tcp"
           queue.type="LinkedList"
           queue.filename="fwd-secondary"
           queue.maxDiskSpace="500m"
           queue.saveOnShutdown="on"
           action.resumeRetryCount="-1")

# Cada mensaje se envía a AMBOS servidores.
# Cada acción tiene su propia queue independiente.
```

> **`queue.filename` debe ser único** por acción. Si dos acciones usan el mismo filename, comparten cola y se corrompen los datos.

---

## Configuración del servidor (receptor)

### Recibir por TCP

```bash
# /etc/rsyslog.d/10-remote-input.conf
module(load="imtcp")
input(type="imtcp" port="514")

# Reiniciar y verificar:
sudo systemctl restart rsyslog
ss -tlnp | grep 514
# LISTEN  0  25  *:514  *:*  users:(("rsyslogd",pid=1234,fd=5))
```

### Recibir por UDP

```bash
# /etc/rsyslog.d/10-remote-input-udp.conf
module(load="imudp")
input(type="imudp" port="514")
```

### Recibir por ambos (TCP + UDP)

```bash
# /etc/rsyslog.d/10-remote-input.conf
module(load="imtcp")
input(type="imtcp" port="514")

module(load="imudp")
input(type="imudp" port="514")
```

### Separar logs por host (dynaFile)

```bash
# /etc/rsyslog.d/30-remote-rules.conf

# Template para archivo dinámico por hostname:
template(name="RemoteLogs" type="string"
    string="/var/log/remote/%HOSTNAME%/%programname%.log"
)

# Template de formato con ISO 8601 e IP de origen:
template(name="RemoteFormat" type="string"
    string="%timegenerated:::date-rfc3339% %fromhost-ip% %programname%: %msg%\n"
)

# Todo lo que venga de remoto → su directorio:
if $fromhost-ip != "127.0.0.1" then {
    action(type="omfile"
           dynaFile="RemoteLogs"
           template="RemoteFormat"
           dirCreateMode="0755"
           fileCreateMode="0640"
           createDirs="on")
    stop
}
```

```
# Resultado en el filesystem:
/var/log/remote/
├── web01/
│   ├── nginx.log
│   ├── sshd.log
│   └── sudo.log
├── web02/
│   ├── nginx.log
│   └── sshd.log
└── db01/
    ├── postgresql.log
    └── sshd.log
```

### Alternativa: por IP en lugar de hostname

```bash
# Útil cuando los hostnames no se resuelven o no son confiables:
template(name="RemoteByIP" type="string"
    string="/var/log/remote/%fromhost-ip%/%programname%.log"
)
# Resultado: /var/log/remote/192.168.1.10/nginx.log
```

---

## Seguridad — TLS

### Por qué TLS

```
SIN TLS (texto plano):
  Cliente ────────────────────► Servidor
  Un atacante en el camino puede:
  ✗ Leer mensajes (passwords, tokens, IPs en los logs)
  ✗ Inyectar logs falsos (spoofing)
  ✗ Modificar mensajes (MITM)
  ✗ Eliminar mensajes (borrar evidencia)

CON TLS (cifrado):
  Cliente ═══════════════════════► Servidor
  ✓ Mensajes cifrados (no se pueden leer)
  ✓ Integridad (no se pueden modificar)
  ✓ Autenticación del servidor (certificado)
  ✓ Opcional: autenticación del cliente (certificado mutuo)
```

### Certificados necesarios

```bash
# Se necesitan 3 elementos (como mínimo):
# 1. Certificado de la CA:        ca.crt (+ ca.key para firmar)
# 2. Certificado del servidor:    server.crt + server.key (firmado por la CA)
# 3. Certificado del cliente:     client.crt + client.key (firmado por la CA)
#    (opcional — solo si se quiere autenticación mutua)

# Paquete necesario:
sudo apt install rsyslog-gnutls    # Debian/Ubuntu
sudo dnf install rsyslog-gnutls    # RHEL/Fedora
```

### Configuración TLS — Servidor

```bash
# /etc/rsyslog.d/10-tls-server.conf
global(
    defaultNetstreamDriver="gtls"
    defaultNetstreamDriverCAFile="/etc/rsyslog.d/tls/ca.crt"
    defaultNetstreamDriverCertFile="/etc/rsyslog.d/tls/server.crt"
    defaultNetstreamDriverKeyFile="/etc/rsyslog.d/tls/server.key"
)

module(load="imtcp"
    streamDriver.name="gtls"
    streamDriver.mode="1"               # 1 = TLS obligatorio
    streamDriver.authMode="x509/name"   # verificar CN del certificado del cliente
)
# Alternativa: streamDriver.authMode="anon"    → sin verificar cliente (solo cifrado)
#              streamDriver.authMode="x509/fingerprint" → verificar por huella

input(type="imtcp" port="6514")         # Puerto estándar para syslog-TLS
```

> **`streamDriver.authMode`:** Solo puede haber **uno** por bloque `module()`. `x509/name` verifica el CN del certificado, `x509/fingerprint` verifica la huella digital, `anon` solo cifra sin autenticar al cliente.

### Configuración TLS — Cliente

```bash
# /etc/rsyslog.d/60-tls-remote.conf
global(
    defaultNetstreamDriver="gtls"
    defaultNetstreamDriverCAFile="/etc/rsyslog.d/tls/ca.crt"
    defaultNetstreamDriverCertFile="/etc/rsyslog.d/tls/client.crt"
    defaultNetstreamDriverKeyFile="/etc/rsyslog.d/tls/client.key"
)

*.* action(type="omfwd"
           target="logserver.example.com"
           port="6514"
           protocol="tcp"
           streamDriver="gtls"
           streamDriverMode="1"               # TLS obligatorio
           streamDriverAuthMode="x509/name"   # verificar CN del servidor
           streamDriverPermittedPeers="logserver.example.com"  # CN permitido

           queue.type="LinkedList"
           queue.filename="fwd-tls"
           queue.maxDiskSpace="1g"
           queue.saveOnShutdown="on"
           action.resumeRetryCount="-1")
```

---

## RELP — Configuración

### Servidor RELP

```bash
# /etc/rsyslog.d/10-relp-server.conf
module(load="imrelp")
input(type="imrelp" port="2514")
```

### Cliente RELP

```bash
# /etc/rsyslog.d/60-relp-client.conf
module(load="omrelp")

*.* action(type="omrelp"
           target="logserver.example.com"
           port="2514"
           queue.type="LinkedList"
           queue.filename="fwd-relp"
           queue.maxDiskSpace="1g"
           queue.saveOnShutdown="on"
           action.resumeRetryCount="-1")
```

> RELP también soporta TLS: añadir `tls="on"` y los parámetros de certificados en el `input()` y `action()`.

---

## Firewall

En el **servidor** receptor, abrir los puertos necesarios:

```bash
# firewalld (RHEL/Fedora):
sudo firewall-cmd --permanent --add-port=514/tcp    # syslog TCP
sudo firewall-cmd --permanent --add-port=514/udp    # syslog UDP
sudo firewall-cmd --permanent --add-port=6514/tcp   # syslog TLS
sudo firewall-cmd --permanent --add-port=2514/tcp   # RELP
sudo firewall-cmd --reload

# ufw (Debian/Ubuntu):
sudo ufw allow 514/tcp
sudo ufw allow 514/udp
sudo ufw allow 6514/tcp
sudo ufw allow 2514/tcp

# iptables (legacy):
sudo iptables -A INPUT -p tcp --dport 514 -j ACCEPT
sudo iptables -A INPUT -p udp --dport 514 -j ACCEPT
```

---

## Verificar el envío remoto

### En el CLIENTE

```bash
# 1. Enviar mensaje de prueba:
logger -t remote-test "Prueba de envío remoto desde $(hostname)"

# 2. Verificar errores de rsyslog:
journalctl -u rsyslog --since "5 min ago" --no-pager

# 3. Verificar conexión TCP activa:
ss -tnp | grep 514
# ESTAB  0  0  192.168.1.10:45678  192.168.1.100:514

# 4. Ver estado de la queue:
ls -la /var/lib/rsyslog/
# Archivos .qi = queue info, .dat = datos de cola
```

### En el SERVIDOR

```bash
# 1. Buscar el mensaje de prueba:
grep "remote-test" /var/log/remote/*/*.log 2>/dev/null

# 2. Verificar que está escuchando:
ss -tlnp | grep 514

# 3. Ver conexiones activas:
ss -tnp | grep rsyslog

# 4. Monitor en tiempo real:
tail -f /var/log/remote/*/*.log
```

### Diagnóstico de problemas

```bash
# PROBLEMA: Mensajes no llegan al servidor

# CLIENTE:
systemctl status rsyslog                                    # ¿está corriendo?
journalctl -u rsyslog --since "10 min ago" | grep -i error # ¿errores de conexión?
grep -rE "@|omfwd|omrelp" /etc/rsyslog.d/*.conf            # ¿config correcta?
nc -zv logserver.example.com 514                            # ¿conectividad?

# SERVIDOR:
ss -tlnp | grep 514                                        # ¿está escuchando?
journalctl -u rsyslog --since "10 min ago"                  # ¿errores de recepción?
sudo firewall-cmd --list-ports 2>/dev/null                  # ¿firewall bloquea?
```

---

## Quick reference

```
CLIENTE (enviar logs):
  UDP:   *.* @servidor:514
  TCP:   *.* @@servidor:514
  TLS:   action(type="omfwd" ... streamDriver="gtls" streamDriverMode="1")
  RELP:  action(type="omrelp" target="..." port="2514")

SERVIDOR (recibir logs):
  TCP:   module(load="imtcp") + input(type="imtcp" port="514")
  UDP:   module(load="imudp") + input(type="imudp" port="514")
  TLS:   module(load="imtcp" streamDriver.name="gtls" ...) port 6514
  RELP:  module(load="imrelp") + input(type="imrelp" port="2514")

QUEUE DE DISCO:
  queue.type="LinkedList"
  queue.filename="fwd-servidor"      (ÚNICO por acción)
  queue.maxDiskSpace="1g"
  queue.saveOnShutdown="on"
  action.resumeRetryCount="-1"       (reintentar indefinidamente)
  action.resumeInterval="30"         (cada 30 segundos)

PUERTOS:
  514/udp   syslog UDP
  514/tcp   syslog TCP
  6514/tcp  syslog TLS
  2514/tcp  RELP
```

---

## Labs

### Parte 1 — Protocolos y cliente

**Paso 1.1: Por qué centralizar logs**

4 problemas con logs solo locales: compromiso (atacante los borra), crash (inaccesibles), correlación (hay que ir a cada servidor), monitoreo (no hay punto único). La solución es enviar los logs a un servidor central.

**Paso 1.2: TCP vs UDP vs RELP**

```
| Aspecto            | UDP (@)          | TCP (@@)         | RELP             |
|--------------------|------------------|------------------|------------------|
| Puerto             | 514              | 514              | 2514             |
| Fiabilidad         | Baja             | Media            | Alta             |
| Pérdida mensajes   | Posible          | Rara             | No               |
| Sobrevive restart  | No               | No               | Sí               |
| Uso                | LAN, no crítico  | Producción       | Prod. crítica    |

Módulos:
  Input:  imudp (UDP), imtcp (TCP), imrelp (RELP)
  Output: omfwd (UDP/TCP), omrelp (RELP)
```

**Paso 1.3: Cliente UDP**

```bash
# Un @ = UDP (fire-and-forget)
*.* @logserver.example.com:514       # enviar todo
*.err @logserver.example.com:514     # solo errores+
auth,authpriv.* @logserver.example.com:514  # solo auth
```

**Paso 1.4: Cliente TCP**

```bash
# Dos @@ = TCP (conexión persistente)
*.* @@logserver.example.com:514

# Equivalente en RainerScript:
*.* action(type="omfwd"
           target="logserver.example.com"
           port="514"
           protocol="tcp")
```

TCP detecta si el servidor no responde, pero sin queue los mensajes se pierden durante la caída.

**Paso 1.5: TCP con queue de disco (producción)**

```bash
action(type="omfwd"
       target="logserver.example.com"
       port="514"
       protocol="tcp"
       queue.type="LinkedList"
       queue.filename="fwd-to-logserver"
       queue.maxDiskSpace="1g"
       queue.saveOnShutdown="on"
       queue.size="10000"
       action.resumeRetryCount="-1"
       action.resumeInterval="30")
```

Archivos de cola en `/var/lib/rsyslog/` (`.qi` = queue info, `.dat` = datos).

**Paso 1.6: Múltiples servidores**

Cada acción `omfwd` con `*.*` envía a su target. Cada una necesita su propio `queue.filename` único. El mensaje se envía a todos los servidores configurados.

### Parte 2 — Servidor y separación por host

**Paso 2.1: Servidor receptor TCP**

```bash
# /etc/rsyslog.d/10-remote-input.conf
module(load="imtcp")
input(type="imtcp" port="514")
```

Verificar con `ss -tlnp | grep 514` (debe mostrar LISTEN).

**Paso 2.2: Separar logs por hostname**

```bash
# /etc/rsyslog.d/30-remote-rules.conf
template(name="RemoteLogs" type="string"
    string="/var/log/remote/%HOSTNAME%/%programname%.log"
)

if $fromhost-ip != "127.0.0.1" then {
    action(type="omfile"
           dynaFile="RemoteLogs"
           dirCreateMode="0755"
           fileCreateMode="0640"
           createDirs="on")
    stop
}
```

`createDirs="on"` crea los directorios automáticamente. `stop` evita que los mensajes remotos se mezclen con los locales.

**Paso 2.3: Separar por IP**

Alternativa cuando los hostnames no son confiables o no se resuelven por DNS:

```bash
template(name="RemoteByIP" type="string"
    string="/var/log/remote/%fromhost-ip%/%programname%.log"
)
```

**Paso 2.4: Verificar configuración existente**

```bash
# Reglas de envío remoto:
grep -rE "@{1,2}" /etc/rsyslog.conf /etc/rsyslog.d/ 2>/dev/null | grep -v "^#"
grep -r "omfwd\|omrelp" /etc/rsyslog.conf /etc/rsyslog.d/ 2>/dev/null | grep -v "^#"

# Módulos de red:
grep -rE "imtcp|imudp|imrelp|omfwd|omrelp" /etc/rsyslog.conf /etc/rsyslog.d/ 2>/dev/null

# Puertos escuchando:
ss -tlnp | grep -E ":(514|6514|2514)\b"
```

### Parte 3 — Seguridad y verificación

**Paso 3.1: TLS — Servidor**

```bash
# /etc/rsyslog.d/10-tls-server.conf
global(
    defaultNetstreamDriver="gtls"
    defaultNetstreamDriverCAFile="/etc/rsyslog.d/tls/ca.crt"
    defaultNetstreamDriverCertFile="/etc/rsyslog.d/tls/server.crt"
    defaultNetstreamDriverKeyFile="/etc/rsyslog.d/tls/server.key"
)

module(load="imtcp"
    streamDriver.name="gtls"
    streamDriver.mode="1"
    streamDriver.authMode="x509/name"
)

input(type="imtcp" port="6514")
```

Requiere `rsyslog-gnutls` instalado.

**Paso 3.2: TLS — Cliente**

```bash
# /etc/rsyslog.d/60-tls-remote.conf
global(
    defaultNetstreamDriver="gtls"
    defaultNetstreamDriverCAFile="/etc/rsyslog.d/tls/ca.crt"
    defaultNetstreamDriverCertFile="/etc/rsyslog.d/tls/client.crt"
    defaultNetstreamDriverKeyFile="/etc/rsyslog.d/tls/client.key"
)

*.* action(type="omfwd"
           target="logserver.example.com"
           port="6514"
           protocol="tcp"
           streamDriver="gtls"
           streamDriverMode="1"
           streamDriverAuthMode="x509/name"
           streamDriverPermittedPeers="logserver.example.com"
           queue.type="LinkedList"
           queue.filename="fwd-tls"
           queue.maxDiskSpace="1g"
           queue.saveOnShutdown="on"
           action.resumeRetryCount="-1")
```

**Paso 3.3: RELP**

```bash
# Servidor:
module(load="imrelp")
input(type="imrelp" port="2514")

# Cliente:
module(load="omrelp")
*.* action(type="omrelp"
           target="logserver.example.com"
           port="2514"
           queue.type="LinkedList"
           queue.filename="fwd-relp"
           queue.maxDiskSpace="1g"
           queue.saveOnShutdown="on"
           action.resumeRetryCount="-1")
```

Requiere `rsyslog-relp` instalado.

**Paso 3.4: Firewall**

Abrir en el servidor: `514/tcp`, `514/udp`, `6514/tcp` (TLS), `2514/tcp` (RELP). Con `ufw allow` o `firewall-cmd --permanent --add-port`.

**Paso 3.5: Verificación**

- **Cliente:** `logger -t remote-test "prueba"`, verificar errores con `journalctl -u rsyslog`, conexión con `ss -tnp | grep 514`
- **Servidor:** `grep "remote-test" /var/log/remote/*/*.log`, verificar escucha con `ss -tlnp | grep 514`, monitor con `tail -f /var/log/remote/*/*.log`

---

## Ejercicios

### Ejercicio 1 — UDP vs TCP: sintaxis

¿Qué protocolo usa cada una de estas reglas?

```bash
# Regla A:
*.* @192.168.1.100:514

# Regla B:
*.* @@192.168.1.100:514

# Regla C:
*.* action(type="omfwd" target="192.168.1.100" port="514" protocol="tcp")

# Regla D:
*.* action(type="omrelp" target="192.168.1.100" port="2514")
```

<details><summary>Predicción</summary>

- **Regla A:** **UDP** — un solo `@`
- **Regla B:** **TCP** — dos `@@`
- **Regla C:** **TCP** — `protocol="tcp"` explícito en RainerScript
- **Regla D:** **RELP** — `type="omrelp"` (módulo de output RELP)

Las reglas A y B son sintaxis legacy. Las reglas C y D son RainerScript con `action()`, que permite más parámetros (queue, reintentos, etc.).

</details>

### Ejercicio 2 — Queue de disco

Esta configuración tiene un problema. ¿Cuál es?

```bash
*.* action(type="omfwd"
           target="logserver1.example.com"
           port="514"
           protocol="tcp"
           queue.type="LinkedList"
           queue.filename="fwd-logs"
           queue.maxDiskSpace="1g"
           action.resumeRetryCount="-1")

*.* action(type="omfwd"
           target="logserver2.example.com"
           port="514"
           protocol="tcp"
           queue.type="LinkedList"
           queue.filename="fwd-logs"
           queue.maxDiskSpace="1g"
           action.resumeRetryCount="-1")
```

<details><summary>Predicción</summary>

**Ambas acciones usan el mismo `queue.filename="fwd-logs"`.**

Los archivos de cola se almacenan en `/var/lib/rsyslog/` con el prefijo del `queue.filename`. Si dos acciones comparten el mismo nombre, ambas intentan leer/escribir los mismos archivos de cola, corrompiendo los datos.

Corrección: usar nombres únicos:
- Primera acción: `queue.filename="fwd-primary"`
- Segunda acción: `queue.filename="fwd-secondary"`

También falta `queue.saveOnShutdown="on"` — sin este parámetro, al apagar rsyslog los mensajes en la cola en memoria se pierden (solo los que ya se escribieron a disco se conservan).

</details>

### Ejercicio 3 — Separación por host

Dado este template y regla:

```bash
template(name="RemoteLogs" type="string"
    string="/var/log/remote/%HOSTNAME%/%programname%.log"
)

if $fromhost-ip != "127.0.0.1" then {
    action(type="omfile" dynaFile="RemoteLogs" createDirs="on")
    stop
}
```

Si `web01` envía un mensaje de `nginx` y otro de `sshd`, ¿qué archivos se crean? ¿Qué pasa con los mensajes locales del propio servidor?

<details><summary>Predicción</summary>

**Archivos creados:**
- `/var/log/remote/web01/nginx.log`
- `/var/log/remote/web01/sshd.log`

**Mensajes locales:** No son afectados. La condición `$fromhost-ip != "127.0.0.1"` excluye los mensajes locales (que tienen `fromhost-ip = 127.0.0.1`). Los mensajes locales pasan de largo esta regla y son procesados por las reglas siguientes (syslog, auth.log, etc.).

El `stop` solo aplica a mensajes remotos, evitando que se mezclen con los logs locales en `/var/log/syslog`.

</details>

### Ejercicio 4 — ¿Por qué stop?

¿Qué pasaría si se elimina el `stop` de la regla del servidor del Ejercicio 3? Asumiendo que después existe la regla `*.*  /var/log/syslog`:

<details><summary>Predicción</summary>

Sin `stop`, los mensajes remotos se procesarían contra las reglas siguientes. Con `*.*  /var/log/syslog`, los mensajes de `web01` aparecerían en:

1. `/var/log/remote/web01/nginx.log` (por el dynaFile)
2. `/var/log/syslog` (por la regla `*.*`)

Los logs remotos se **mezclarían** con los logs locales en syslog. En un servidor que recibe logs de 50 hosts, syslog se llenaría rápidamente con mensajes ajenos, dificultando la lectura de los logs propios del servidor.

El `stop` es esencial en un servidor de logs centralizado para mantener la separación entre logs remotos y locales.

</details>

### Ejercicio 5 — TLS: authMode

Un administrador configura el servidor TLS así:

```bash
module(load="imtcp"
    streamDriver.name="gtls"
    streamDriver.mode="1"
    streamDriver.authMode="anon"
)
```

Y el cliente así:

```bash
*.* action(type="omfwd"
           ...
           streamDriverAuthMode="x509/name"
           streamDriverPermittedPeers="logserver.example.com")
```

**a)** ¿Funciona la conexión TLS?
**b)** ¿Qué verifica cada extremo?

<details><summary>Predicción</summary>

**a)** **Sí**, la conexión TLS funciona. El `authMode` del servidor y del cliente son independientes.

**b)**
- **Servidor** (`anon`): **No** verifica el certificado del cliente. Cualquier cliente puede conectarse. Solo se establece cifrado.
- **Cliente** (`x509/name`): **Sí** verifica que el certificado del servidor tenga CN = `logserver.example.com`. Si el CN no coincide, el cliente rechaza la conexión.

Este es un escenario común: el servidor acepta conexiones de cualquier cliente (no exige certificado de cliente), pero el cliente verifica que se conecta al servidor correcto (evita MITM). Para autenticación mutua, el servidor debería usar `x509/name` o `x509/fingerprint`.

</details>

### Ejercicio 6 — Comparar protocolos

Para cada escenario, ¿qué protocolo recomiendas y por qué?

1. Logs de desarrollo en una LAN aislada, no son críticos
2. Logs de autenticación de servidores de producción
3. Logs de un sistema financiero regulado (auditoría obligatoria, no se puede perder ningún evento)
4. Logs de servidores en distintos datacenters conectados por internet

<details><summary>Predicción</summary>

1. **UDP** — LAN aislada, no críticos. UDP es el más simple y rápido. La pérdida ocasional de mensajes es aceptable en desarrollo.

2. **TCP con queue de disco** — Producción básica. Los logs de auth son importantes pero no requieren garantía absoluta. TCP con queue evita pérdida durante caídas del servidor.

3. **RELP** — Misión crítica. RELP garantiza entrega end-to-end y sobrevive reinicios. Para auditoría regulada, no se puede aceptar pérdida de ningún evento.

4. **TLS (TCP con cifrado)** — Los logs viajan por internet, donde pueden ser interceptados. TLS cifra el contenido y autentica los endpoints. Combinar con queue de disco para resiliencia. Si además es misión crítica, RELP sobre TLS.

</details>

### Ejercicio 7 — Diagnóstico

Un cliente envía logs con `*.* @@logserver:514` pero los mensajes no aparecen en el servidor. Ordena estos pasos de diagnóstico de más rápido/probable a más lento/improbable:

A. Verificar firewall del servidor
B. Verificar que rsyslog está corriendo en ambos lados
C. Verificar que el servidor tiene `imtcp` cargado y escucha en 514
D. Probar conectividad con `nc -zv logserver 514`
E. Verificar resolución DNS del nombre "logserver"

<details><summary>Predicción</summary>

Orden recomendado:

1. **B** — Verificar que rsyslog está corriendo (`systemctl status rsyslog`). Es lo más básico y rápido.
2. **C** — Verificar que el servidor tiene `imtcp` cargado y escucha (`ss -tlnp | grep 514`). Si no escucha, todo lo demás es irrelevante.
3. **E** — Verificar resolución DNS (`host logserver` o `dig logserver`). Si "logserver" no resuelve, el cliente no puede conectar.
4. **D** — Probar conectividad TCP (`nc -zv logserver 514`). Descarta problemas de red/routing.
5. **A** — Verificar firewall (`firewall-cmd --list-ports` o `ufw status`). Si `nc` falla pero el servicio escucha, el firewall es el sospechoso.

Patrón general: verificar servicio → verificar DNS → verificar red → verificar firewall.

</details>

### Ejercicio 8 — Queue: comportamiento

Un cliente tiene esta configuración y el servidor de logs ha estado caído 2 horas:

```bash
action(type="omfwd"
       target="logserver.example.com"
       port="514"
       protocol="tcp"
       queue.type="LinkedList"
       queue.filename="fwd-logserver"
       queue.maxDiskSpace="500m"
       queue.saveOnShutdown="on"
       action.resumeRetryCount="-1"
       action.resumeInterval="60")
```

**a)** ¿Qué pasa con los mensajes durante las 2 horas?
**b)** ¿Qué pasa cuando el servidor vuelve?
**c)** ¿Qué pasa si se reinicia rsyslog en el cliente durante la caída?
**d)** ¿Qué pasa si la cola llega a 500MB?

<details><summary>Predicción</summary>

**a)** Los mensajes se acumulan en la cola de disco en `/var/lib/rsyslog/fwd-logserver*`. rsyslog intenta reconectar cada 60 segundos (`resumeInterval="60"`) indefinidamente (`resumeRetryCount="-1"`).

**b)** rsyslog detecta que el servidor vuelve en el siguiente intento de reconexión (máximo 60s). Envía todos los mensajes acumulados en la cola, en orden. Una vez vaciada la cola, vuelve al envío en tiempo real.

**c)** Gracias a `saveOnShutdown="on"`, al apagar rsyslog la cola en memoria se escribe a disco. Al reiniciar, rsyslog lee los archivos de cola (`fwd-logserver*.qi` y `.dat`) y retoma el envío cuando el servidor esté disponible. **Sin este parámetro**, los mensajes en memoria se perderían.

**d)** Cuando la cola alcanza 500MB (`maxDiskSpace="500m"`), rsyslog **descarta los mensajes nuevos** — no puede almacenar más. Los mensajes anteriores en la cola se conservan. Esto se registra como error en el journal de rsyslog.

</details>

### Ejercicio 9 — Configuración completa

Diseña la configuración de envío remoto para un servidor web que:
1. Envía todos los logs por TCP con TLS al servidor `logs.company.com:6514`
2. Tiene queue de disco de máximo 2GB
3. Reintenta cada 30 segundos indefinidamente
4. Los certificados están en `/etc/rsyslog.d/tls/`

<details><summary>Predicción</summary>

```bash
# /etc/rsyslog.d/60-remote-tls.conf

# Configuración TLS global:
global(
    defaultNetstreamDriver="gtls"
    defaultNetstreamDriverCAFile="/etc/rsyslog.d/tls/ca.crt"
    defaultNetstreamDriverCertFile="/etc/rsyslog.d/tls/client.crt"
    defaultNetstreamDriverKeyFile="/etc/rsyslog.d/tls/client.key"
)

# Enviar todo con TLS + queue de disco:
*.* action(type="omfwd"
           target="logs.company.com"
           port="6514"
           protocol="tcp"
           streamDriver="gtls"
           streamDriverMode="1"
           streamDriverAuthMode="x509/name"
           streamDriverPermittedPeers="logs.company.com"

           queue.type="LinkedList"
           queue.filename="fwd-company-tls"
           queue.maxDiskSpace="2g"
           queue.saveOnShutdown="on"

           action.resumeRetryCount="-1"
           action.resumeInterval="30")
```

Elementos clave:
- `streamDriverMode="1"` → TLS obligatorio (no fallback a texto plano)
- `streamDriverPermittedPeers` → verifica que el CN del servidor coincida
- `queue.filename` único → evita colisiones con otras queues
- `queue.saveOnShutdown="on"` → conserva cola al reiniciar rsyslog

</details>

### Ejercicio 10 — Servidor: configuración completa

Diseña la configuración del **servidor** receptor que:
1. Recibe por TCP y UDP en puerto 514
2. Recibe por TLS en puerto 6514
3. Separa los logs remotos en `/var/log/remote/HOSTNAME/PROGRAMA.log`
4. Usa formato ISO 8601 con la IP del cliente
5. Los logs locales del servidor no deben mezclarse con los remotos

<details><summary>Predicción</summary>

```bash
# /etc/rsyslog.d/10-remote-input.conf

# Recibir TCP y UDP (puerto 514):
module(load="imtcp")
input(type="imtcp" port="514")

module(load="imudp")
input(type="imudp" port="514")

# Recibir TLS (puerto 6514):
global(
    defaultNetstreamDriver="gtls"
    defaultNetstreamDriverCAFile="/etc/rsyslog.d/tls/ca.crt"
    defaultNetstreamDriverCertFile="/etc/rsyslog.d/tls/server.crt"
    defaultNetstreamDriverKeyFile="/etc/rsyslog.d/tls/server.key"
)

module(load="imtcp"
    streamDriver.name="gtls"
    streamDriver.mode="1"
    streamDriver.authMode="x509/name")

input(type="imtcp" port="6514")
```

```bash
# /etc/rsyslog.d/30-remote-rules.conf

# Template de ruta dinámica:
template(name="RemoteLogs" type="string"
    string="/var/log/remote/%HOSTNAME%/%programname%.log"
)

# Template de formato ISO 8601 con IP:
template(name="RemoteFormat" type="string"
    string="%timegenerated:::date-rfc3339% %fromhost-ip% %programname%: %msg:::drop-last-lf%\n"
)

# Todo lo remoto → su directorio (no se mezcla con logs locales):
if $fromhost-ip != "127.0.0.1" then {
    action(type="omfile"
           dynaFile="RemoteLogs"
           template="RemoteFormat"
           dirCreateMode="0755"
           fileCreateMode="0640"
           createDirs="on")
    stop
}
```

Elementos clave:
- Dos archivos separados (10- para inputs, 30- para reglas) siguiendo la convención de numeración
- `stop` evita que los remotos lleguen a syslog local
- `$fromhost-ip != "127.0.0.1"` filtra solo mensajes remotos
- Puertos abiertos en firewall: 514/tcp, 514/udp, 6514/tcp

**Nota:** Cargar `imtcp` dos veces (una vez para 514 sin TLS y otra para 6514 con TLS) requiere cuidado. En rsyslog, `module(load="imtcp")` solo se puede cargar una vez. Para tener TCP sin TLS en 514 y TCP con TLS en 6514, se usa la configuración global de TLS y se distingue por input con `streamDriver.mode`:

```bash
module(load="imtcp")
input(type="imtcp" port="514")                              # sin TLS
input(type="imtcp" port="6514"
      streamDriver.name="gtls"
      streamDriver.mode="1"
      streamDriver.authMode="x509/name")                    # con TLS
```

</details>
