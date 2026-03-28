# SSH Tunneling — Port Forwarding

## Índice

1. [Concepto de túnel SSH](#concepto-de-túnel-ssh)
2. [Local port forwarding (-L)](#local-port-forwarding--l)
3. [Remote port forwarding (-R)](#remote-port-forwarding--r)
4. [Dynamic port forwarding (-D)](#dynamic-port-forwarding--d)
5. [Túneles persistentes y en background](#túneles-persistentes-y-en-background)
6. [Túneles encadenados y multi-hop](#túneles-encadenados-y-multi-hop)
7. [ProxyJump vs túneles manuales](#proxyjump-vs-túneles-manuales)
8. [Directivas de sshd_config relacionadas](#directivas-de-sshd_config-relacionadas)
9. [Escenarios prácticos](#escenarios-prácticos)
10. [Errores comunes](#errores-comunes)
11. [Cheatsheet](#cheatsheet)
12. [Ejercicios](#ejercicios)

---

## Concepto de túnel SSH

Un túnel SSH encapsula tráfico de otro protocolo dentro de una conexión SSH cifrada. Esto permite:

- Acceder a servicios en redes remotas que no son directamente accesibles
- Cifrar protocolos que no tienen cifrado propio
- Atravesar firewalls que solo permiten SSH

```
    Sin túnel:
    ┌────────┐                    ┌────────┐
    │Cliente │──── HTTP :80 ─────►│Servidor│
    │        │   (sin cifrar,     │ web    │
    │        │    bloqueado por   │        │
    └────────┘    firewall)       └────────┘

    Con túnel SSH:
    ┌────────┐                    ┌────────┐       ┌────────┐
    │Cliente │══ SSH cifrado ════►│ SSH    │──────►│Servidor│
    │        │  (puerto 22,       │ Server │ HTTP  │ web    │
    │        │   permitido)       │        │ local │ :80    │
    └────────┘                    └────────┘       └────────┘
                                  ▲
                                  │
                          El tráfico HTTP viaja
                          DENTRO del túnel SSH
```

Hay tres tipos de port forwarding:

| Tipo | Flag | Dirección | Uso típico |
|---|---|---|---|
| **Local** | `-L` | Cliente → Servidor | Acceder a servicios remotos |
| **Remote** | `-R` | Servidor → Cliente | Exponer servicios locales |
| **Dynamic** | `-D` | Cliente como proxy SOCKS | Navegar a través del servidor |

---

## Local port forwarding (-L)

### Concepto

"Abro un puerto **local** en mi máquina que reenvía tráfico a un destino a través del servidor SSH."

```bash
ssh -L [bind_address:]local_port:destination:dest_port usuario@ssh_server
```

```
    Tu máquina                 SSH Server              Destino
    ┌──────────────┐          ┌──────────────┐       ┌──────────┐
    │              │          │              │       │          │
    │ localhost    │══ SSH ══►│              │──────►│ db       │
    │    :9090     │  cifrado │              │ TCP   │ :5432    │
    │              │          │              │ plano │          │
    │ app conecta  │          │              │       │          │
    │ a localhost  │          │              │       │          │
    │    :9090     │          │              │       │          │
    └──────────────┘          └──────────────┘       └──────────┘

    ssh -L 9090:db.internal:5432 usuario@ssh-server
```

El flujo:

1. SSH abre el puerto 9090 en tu máquina local
2. Cualquier conexión a `localhost:9090` se cifra y envía al SSH server
3. El SSH server abre una conexión TCP a `db.internal:5432`
4. Los datos fluyen bidirecccionalmente a través del túnel

### Ejemplo: acceder a una base de datos remota

```bash
# La base de datos PostgreSQL está en una red interna, solo accesible
# desde el servidor SSH (bastion)
ssh -L 5432:db.internal:5432 admin@bastion.ejemplo.com

# Ahora en otra terminal:
psql -h localhost -p 5432 -U appuser -d mydb
# Conecta a la DB remota como si fuera local
```

### Ejemplo: acceder a una interfaz web interna

```bash
# Grafana corre en un servidor interno en el puerto 3000
ssh -L 8080:monitoring.internal:3000 admin@bastion

# En el navegador:
# http://localhost:8080
# Ves la interfaz de Grafana que está en la red interna
```

### Puerto local = puerto remoto

No es necesario que los puertos coincidan, pero puede facilitar la comprensión:

```bash
# Puerto local diferente al remoto (recomendado para evitar conflictos)
ssh -L 9090:db.internal:5432 admin@bastion
# local:9090 → remoto:5432

# Mismo puerto (requiere que el puerto local esté libre)
ssh -L 5432:db.internal:5432 admin@bastion
# local:5432 → remoto:5432
# Cuidado: si ya tienes PostgreSQL local, el puerto estará ocupado
```

### Bind address — quién puede usar el túnel

```bash
# Default: solo localhost puede usar el túnel
ssh -L 9090:db.internal:5432 admin@bastion
# Equivalente a:
ssh -L 127.0.0.1:9090:db.internal:5432 admin@bastion
# Solo tu máquina puede conectar a :9090

# Permitir que otras máquinas de tu red usen el túnel
ssh -L 0.0.0.0:9090:db.internal:5432 admin@bastion
# Cualquiera que pueda alcanzar tu máquina puede usar :9090
# ¡Cuidado! Estás exponiendo el servicio a tu red local

# Bind a una interfaz específica
ssh -L 192.168.1.100:9090:db.internal:5432 admin@bastion
```

### Múltiples forwards en una conexión

```bash
# Acceder a varios servicios internos a la vez
ssh -L 5432:db.internal:5432 \
    -L 3000:monitoring.internal:3000 \
    -L 8080:jenkins.internal:8080 \
    admin@bastion
```

### El destino puede ser localhost del servidor

```bash
# Acceder a un servicio que corre en el propio servidor SSH
# y solo escucha en 127.0.0.1
ssh -L 9090:localhost:9090 admin@servidor
#                ↑
#      "localhost" se resuelve en el SERVIDOR,
#      no en tu máquina
```

Esto es un punto de confusión frecuente:

```
    ssh -L 9090:localhost:8080 admin@servidor

    "localhost" en -L se refiere al servidor SSH,
    NO a tu máquina local.

    Tu máquina          Servidor SSH
    ┌──────────┐       ┌──────────────────┐
    │          │       │                  │
    │ :9090 ═══╪══════►│ localhost:8080   │
    │          │       │    ↑              │
    │          │       │    este localhost │
    │          │       │    es el servidor │
    └──────────┘       └──────────────────┘
```

---

## Remote port forwarding (-R)

### Concepto

"Abro un puerto en el **servidor SSH** que reenvía tráfico hacia mi máquina local (o un destino accesible desde mi máquina)."

Es el inverso del local forwarding:

```bash
ssh -R [bind_address:]remote_port:destination:dest_port usuario@ssh_server
```

```
    Tu máquina                 SSH Server
    ┌──────────────┐          ┌──────────────┐
    │              │          │              │
    │ webapp       │◄── TCP ──│ :8080        │◄── Usuarios
    │ :3000        │   plano  │              │    externos
    │              │          │              │
    │              │══ SSH ══►│              │
    │              │  cifrado │              │
    └──────────────┘          └──────────────┘

    ssh -R 8080:localhost:3000 usuario@ssh-server
```

El flujo:

1. SSH abre el puerto 8080 en el **servidor SSH**
2. Cuando alguien conecta a `servidor:8080`, el tráfico viaja por el túnel SSH
3. Sale por tu máquina y conecta a `localhost:3000`

### Caso de uso: exponer un servicio de desarrollo

```bash
# Estás desarrollando una webapp en tu portátil (localhost:3000)
# Quieres que un compañero la vea desde el servidor compartido

ssh -R 8080:localhost:3000 admin@servidor-compartido

# Tu compañero puede ver tu app en:
# http://servidor-compartido:8080
```

### Caso de uso: dar acceso a un servidor detrás de NAT

```bash
# Tu servidor en casa está detrás de un router (sin IP pública)
# Tienes un VPS con IP pública

# Desde tu servidor casero:
ssh -R 2222:localhost:22 admin@vps-publico

# Ahora, desde cualquier lugar:
ssh -p 2222 usuario@vps-publico
# Conecta al SSH de tu servidor casero a través del VPS
```

```
    Internet        VPS público           Tu casa (detrás de NAT)
    ┌──────┐       ┌──────────┐          ┌──────────┐
    │      │       │          │          │          │
    │ user │──────►│ :2222    │◄══ SSH ══│ sshd :22 │
    │      │  SSH  │          │  tunnel  │          │
    │      │       │          │          │          │
    └──────┘       └──────────┘          └──────────┘
```

### GatewayPorts — bind address del remote forward

Por defecto, el remote forward solo escucha en `127.0.0.1` del servidor. Para que escuche en todas las interfaces:

```bash
# El servidor SSH necesita tener en sshd_config:
GatewayPorts yes            # escucha en 0.0.0.0
# O:
GatewayPorts clientspecified  # el cliente decide

# Con clientspecified, el cliente puede especificar:
ssh -R 0.0.0.0:8080:localhost:3000 admin@servidor
# O:
ssh -R '*:8080:localhost:3000' admin@servidor
```

Sin `GatewayPorts`, el remote forward solo es accesible desde el propio servidor:

```bash
# GatewayPorts no (default):
# servidor$ curl localhost:8080    ← funciona
# otro-pc$ curl servidor:8080     ← Connection refused

# GatewayPorts yes:
# servidor$ curl localhost:8080    ← funciona
# otro-pc$ curl servidor:8080     ← funciona
```

---

## Dynamic port forwarding (-D)

### Concepto

"Abro un proxy **SOCKS** local que enruta todo el tráfico a través del servidor SSH."

A diferencia de `-L` (un destino fijo) y `-R` (un origen fijo), `-D` permite conectar a **cualquier destino** a través del servidor SSH:

```bash
ssh -D [bind_address:]local_port usuario@ssh_server
```

```
    Tu máquina                  SSH Server              Internet
    ┌──────────────┐           ┌──────────┐          ┌──────────┐
    │              │           │          │          │          │
    │ Navegador ──►│ SOCKS    │          │          │ web1.com │
    │              │ :1080 ═══╪══ SSH ══►│──────────│ web2.com │
    │ curl     ──►│           │          │          │ api.net  │
    │              │           │          │          │          │
    └──────────────┘           └──────────┘          └──────────┘

    ssh -D 1080 usuario@ssh-server
```

### Uso con navegador

```bash
# Abrir el proxy SOCKS
ssh -D 1080 admin@servidor-remoto

# Configurar el navegador:
# Firefox: Ajustes → Red → Proxy manual
#   SOCKS Host: 127.0.0.1    Port: 1080
#   SOCKS v5 ✓
#   "Proxy DNS when using SOCKS v5" ✓  ← importante
```

Con "Proxy DNS" activado, las consultas DNS también pasan por el túnel. Sin esto, tu ISP ve a qué dominios accedes aunque el tráfico vaya por el túnel.

### Uso con herramientas de línea de comandos

```bash
# Con curl (soporta SOCKS nativamente)
curl --socks5-hostname localhost:1080 http://internal.ejemplo.com
# --socks5-hostname: resuelve DNS en el servidor remoto
# --socks5: resuelve DNS localmente (menos privado)

# Con variables de entorno (para programas que respetan proxy)
export ALL_PROXY=socks5h://127.0.0.1:1080
# socks5h = SOCKS5 con resolución DNS remota (la "h" es de hostname)

curl http://internal.ejemplo.com  # usa el proxy automáticamente

# Con proxychains (para programas que no soportan proxy)
# Instalar: sudo apt install proxychains4
# Configurar /etc/proxychains4.conf:
#   socks5 127.0.0.1 1080

proxychains4 nmap -sT internal.ejemplo.com
```

### Comparativa de los tres tipos

```
    LOCAL (-L):       REMOTE (-R):      DYNAMIC (-D):
    Un destino fijo   Un origen fijo    Cualquier destino

    local:A ──►       ◄── remote:A     local:SOCKS ──►
    tunnel ═══►       ◄═══ tunnel      tunnel ════════►
    ──► dest:B        origin:B ──►     ──► cualquier:*

    "Traer un          "Enviar un        "Todo mi
     servicio           servicio          tráfico pasa
     remoto a           local al          por allá"
     mi máquina"        servidor"
```

---

## Túneles persistentes y en background

### Flags útiles

```bash
# -f: pasar a background después de la autenticación
# -N: no ejecutar comando remoto (solo túnel, sin shell)
# -T: no asignar terminal (ahorra recursos)

# Túnel local en background sin shell
ssh -f -N -T -L 9090:db.internal:5432 admin@bastion
# La sesión SSH corre en background, sin terminal,
# solo manteniendo el túnel

# Verificar que el túnel está activo
ss -tlnp | grep 9090
# LISTEN  0  128  127.0.0.1:9090  0.0.0.0:*  users:(("ssh",pid=12345,...))

# Matar el túnel
kill 12345
# O encontrar el PID:
ps aux | grep "ssh.*-L.*9090"
```

### Mantener túneles vivos con ServerAliveInterval

```bash
# El túnel se cae si la conexión se pierde (red inestable, NAT timeout)
# ServerAliveInterval envía keepalives SSH

ssh -o ServerAliveInterval=60 -o ServerAliveCountMax=3 \
    -f -N -L 9090:db.internal:5432 admin@bastion
# Envía keepalive cada 60s, cierra tras 3 fallos (3 min)
```

### autossh — reconexión automática

`autossh` monitoriza la conexión SSH y la reinicia si se cae:

```bash
# Instalar
sudo apt install autossh    # Debian/Ubuntu
sudo dnf install autossh    # Fedora/RHEL

# Túnel con reconexión automática
autossh -M 0 \
    -o "ServerAliveInterval=30" \
    -o "ServerAliveCountMax=3" \
    -f -N -L 9090:db.internal:5432 admin@bastion

# -M 0: usar ServerAliveInterval para monitorización
#        (más moderno que el puerto de monitorización legacy)
```

### Túnel como servicio systemd

Para túneles que deben sobrevivir reinicios:

```ini
# /etc/systemd/system/ssh-tunnel-db.service
[Unit]
Description=SSH Tunnel to internal database
After=network-online.target
Wants=network-online.target

[Service]
Type=simple
ExecStart=/usr/bin/ssh -N -T \
    -o ServerAliveInterval=60 \
    -o ServerAliveCountMax=3 \
    -o ExitOnForwardFailure=yes \
    -L 5432:db.internal:5432 \
    -i /etc/ssh/tunnel_key \
    tunnel-user@bastion
Restart=always
RestartSec=10

[Install]
WantedBy=multi-user.target
```

```bash
sudo systemctl daemon-reload
sudo systemctl enable --now ssh-tunnel-db
sudo systemctl status ssh-tunnel-db
```

`ExitOnForwardFailure=yes` es clave: si el puerto local ya está en uso (por ejemplo, otra instancia del túnel), SSH falla inmediatamente en vez de conectar sin el forward.

---

## Túneles encadenados y multi-hop

### Escenario: acceder a un servicio a través de dos saltos

```
    Tu máquina → Bastion → Servidor interno → Base de datos
                 (acceso   (acceso a db,
                  SSH)      no directo)
```

```bash
# Opción 1: túnel anidado (dos comandos)
# Primero: túnel al servidor interno a través del bastion
ssh -L 2222:interno:22 admin@bastion -f -N

# Segundo: túnel a la DB a través del servidor interno (vía el primer túnel)
ssh -L 5432:db:5432 admin@localhost -p 2222 -f -N

# Opción 2: túnel directo con ProxyJump (un comando)
ssh -J bastion -L 5432:db:5432 admin@interno -f -N
# ProxyJump maneja el salto intermedio automáticamente
```

### En ~/.ssh/config

```bash
Host bastion
    HostName bastion.ejemplo.com
    User admin
    IdentityFile ~/.ssh/id_ed25519

Host interno
    HostName interno.private
    User admin
    ProxyJump bastion
    # SSH conecta a bastion primero, luego salta a interno

Host db-tunnel
    HostName interno.private
    User admin
    ProxyJump bastion
    LocalForward 5432 db.internal:5432
    # ssh db-tunnel  ← abre el túnel con un solo comando
```

---

## ProxyJump vs túneles manuales

### ProxyJump (-J)

Introducido en OpenSSH 7.3, es el método moderno para saltar a través de hosts intermedios:

```bash
# Conectar a servidor-interno pasando por bastion
ssh -J bastion usuario@servidor-interno

# Múltiples saltos
ssh -J bastion1,bastion2 usuario@servidor-final

# Equivalente en ssh_config:
Host servidor-interno
    ProxyJump bastion
```

### Cómo funciona ProxyJump internamente

```
    ProxyJump:
    ┌────────┐     ┌──────────┐     ┌──────────────┐
    │Cliente │═══► │ Bastion  │═══► │ Srv interno  │
    │        │SSH  │          │SSH  │              │
    │        │     │ stdio    │     │              │
    └────────┘     │ proxy    │     └──────────────┘
                   └──────────┘

    La conexión SSH al servidor interno pasa por el bastion
    como un stream de bytes. El bastion NO ve el contenido
    (cifrado end-to-end entre cliente y servidor interno).
```

Esto es fundamentalmente diferente de agent forwarding:

```
    Agent Forwarding (-A):
    - El bastion puede usar tus claves (via socket)
    - Riesgo si el bastion está comprometido

    ProxyJump (-J):
    - El bastion solo ve bytes cifrados
    - No puede suplantar tu identidad
    - Más seguro
```

### ProxyCommand — el método legacy

Antes de ProxyJump, se usaba `ProxyCommand`:

```bash
# En ssh_config:
Host servidor-interno
    ProxyCommand ssh -W %h:%p bastion
    # -W: modo stdio forwarding (el predecesor de -J)

# ProxyJump es equivalente pero más limpio:
Host servidor-interno
    ProxyJump bastion
```

`ProxyCommand` sigue siendo útil cuando necesitas lógica personalizada:

```bash
Host servidor-con-vpn
    ProxyCommand bash -c 'if ip route | grep -q 10.0.0.0/8; then nc %h %p; else ssh -W %h:%p bastion; fi'
    # Si la VPN está activa, conecta directo; si no, usa bastion
```

---

## Directivas de sshd_config relacionadas

### Control del forwarding en el servidor

```bash
# Habilitar/deshabilitar port forwarding
AllowTcpForwarding yes          # default — permite -L y -R
AllowTcpForwarding no           # deshabilita todo forwarding
AllowTcpForwarding local        # solo -L
AllowTcpForwarding remote       # solo -R

# Controlar qué puertos puede abrir un remote forward (-R)
# Sin restricción (default):
# El cliente puede abrir cualquier puerto en el servidor

# Con restricción (en authorized_keys):
# permitopen="localhost:8080" ssh-ed25519 AAAA...
# Solo permite forward hacia localhost:8080

# PermitOpen en sshd_config:
PermitOpen localhost:8080 192.168.1.0/24:*
# Solo permite local forwards a estos destinos

# GatewayPorts — quién puede conectar al remote forward
GatewayPorts no               # default, solo localhost del servidor
GatewayPorts yes              # cualquier IP
GatewayPorts clientspecified  # el cliente decide

# Permitir forwarding de UNIX domain sockets
AllowStreamLocalForwarding yes   # default

# Túneles de capa 2/3 (VPN sobre SSH)
PermitTunnel no               # default
PermitTunnel yes              # permite -w (tun device)
PermitTunnel point-to-point   # solo capa 3
PermitTunnel ethernet         # solo capa 2
```

### Restricciones por usuario con Match

```bash
# Usuarios de deploy: sin forwarding
Match User deploy
    AllowTcpForwarding no

# Administradores: todo permitido
Match Group admins
    AllowTcpForwarding yes
    GatewayPorts clientspecified
    PermitTunnel yes

# Usuarios SFTP: nada de forwarding
Match Group sftponly
    AllowTcpForwarding no
    AllowStreamLocalForwarding no
    PermitTunnel no
```

---

## Escenarios prácticos

### Escenario 1 — Acceso a servicios internos desde casa

Tu empresa tiene servicios internos (Jira, GitLab, Grafana) accesibles solo desde la red de oficina. Tienes acceso SSH a un bastion:

```bash
# En ~/.ssh/config:
Host bastion-trabajo
    HostName bastion.empresa.com
    User tu-usuario
    IdentityFile ~/.ssh/id_ed25519_trabajo

Host trabajo-tuneles
    HostName bastion.empresa.com
    User tu-usuario
    IdentityFile ~/.ssh/id_ed25519_trabajo
    # Jira
    LocalForward 8081 jira.internal:8080
    # GitLab
    LocalForward 8082 gitlab.internal:80
    # Grafana
    LocalForward 3000 grafana.internal:3000
    # PostgreSQL de staging
    LocalForward 5432 db-staging.internal:5432
```

```bash
# Un solo comando abre todos los túneles
ssh -N trabajo-tuneles

# En el navegador:
# http://localhost:8081  → Jira
# http://localhost:8082  → GitLab
# http://localhost:3000  → Grafana

# En la terminal:
psql -h localhost -p 5432 -U app staging_db
```

### Escenario 2 — Exponer un servicio de desarrollo para testing

Estás desarrollando una API en tu laptop y el equipo de QA necesita probarla:

```bash
# Tu API corre en localhost:8000
# Tienes un servidor compartido accesible por todos

# Opción 1: remote forward temporal
ssh -R 9000:localhost:8000 admin@shared-server
# QA accede a shared-server:9000

# Opción 2: con autossh para estabilidad
autossh -M 0 -o ServerAliveInterval=30 \
    -R 9000:localhost:8000 admin@shared-server -f -N

# Opción 3: si el servidor tiene GatewayPorts no (default),
# QA solo puede acceder desde el propio servidor.
# Solución: combinar con un proxy nginx en el servidor:
# server { listen 9000; proxy_pass http://127.0.0.1:9000; }
```

### Escenario 3 — Proxy SOCKS para navegar desde otra ubicación

Necesitas acceder a un servicio web que solo acepta conexiones desde una IP específica (tu servidor):

```bash
# Abrir proxy SOCKS
ssh -D 1080 -f -N admin@servidor-con-ip-permitida

# Usar curl con el proxy
curl --socks5-hostname localhost:1080 https://servicio-restringido.com/api

# O configurar todo el navegador para usar el proxy
# Firefox → Proxy → Manual → SOCKS5: 127.0.0.1:1080
```

### Escenario 4 — Túnel SSH para evitar redes hostiles

Estás en un Wi-Fi público (aeropuerto, café) y quieres cifrar todo tu tráfico:

```bash
# Dynamic proxy SOCKS
ssh -D 1080 -f -N -C admin@tu-vps
# -C: habilitar compresión (útil en conexiones lentas)

# Configurar el sistema para usar el proxy:
# GNOME: Ajustes → Red → Proxy → Manual → SOCKS: 127.0.0.1:1080

# Para DNS: asegurarse de que las consultas DNS también van por el proxy
# Firefox: about:config → network.proxy.socks_remote_dns = true
```

> **Limitación**: un proxy SOCKS solo protege el tráfico de aplicaciones que lo usan. No es una VPN — no captura todo el tráfico del sistema. Para eso, necesitas WireGuard u OpenVPN.

### Escenario 5 — Diagnóstico con túnel temporal

Necesitas ejecutar `tcpdump` o `nmap` en una red remota accesible solo por SSH:

```bash
# Opción 1: tunnel + herramienta local
ssh -L 8888:target.internal:80 admin@bastion -f -N
nmap -sT -p 80 localhost -Pn
# Escanea el puerto 80 del target a través del túnel

# Opción 2: SOCKS proxy + proxychains (más versátil)
ssh -D 1080 admin@bastion -f -N

# proxychains4.conf: socks5 127.0.0.1 1080
proxychains4 nmap -sT -Pn target.internal
# nmap ejecuta todas sus conexiones a través del túnel

# Nota: solo funciona con TCP connect scan (-sT),
# no con SYN scan (-sS) que requiere raw sockets
```

---

## Errores comunes

### 1. Confundir "localhost" en el contexto de -L

```bash
# ¿Quién es "localhost" aquí?
ssh -L 9090:localhost:3000 admin@servidor

# Muchos piensan: "localhost:3000 es MI máquina"
# Realidad: localhost se resuelve en el SERVIDOR SSH

# Si quieres acceder a un servicio en TU máquina:
# No necesitas un túnel — ya estás ahí.

# Si quieres acceder a un servicio en el SERVIDOR:
ssh -L 9090:localhost:3000 admin@servidor    ← correcto
# "localhost" = servidor

# Si quieres acceder a un TERCER host:
ssh -L 9090:tercer-host:3000 admin@servidor  ← correcto
# "tercer-host" se resuelve desde el servidor
```

### 2. El puerto local ya está en uso

```bash
ssh -L 5432:db.internal:5432 admin@bastion
# bind [127.0.0.1]:5432: Address already in use
# channel_setup_fwd_listener_tcpip: cannot listen to port: 5432

# Tienes PostgreSQL local corriendo en 5432
# Solución: usar otro puerto local
ssh -L 15432:db.internal:5432 admin@bastion
# Conectar con: psql -h localhost -p 15432
```

### 3. Remote forward que no acepta conexiones externas

```bash
# Problema: ssh -R 8080:localhost:3000 admin@servidor
# Desde otro PC: curl servidor:8080 → Connection refused

# Causa: GatewayPorts no (default)
# El forward solo escucha en 127.0.0.1 del servidor

# Solución 1 (en sshd_config del servidor):
GatewayPorts clientspecified
# Luego:
ssh -R '*:8080:localhost:3000' admin@servidor

# Solución 2 (sin cambiar sshd_config):
# Usar socat o nginx en el servidor para reenviar:
# socat TCP-LISTEN:8080,fork,reuseaddr TCP:127.0.0.1:8080
```

### 4. Túnel en background sin forma de cerrarlo

```bash
# Lanzaste el túnel en background
ssh -f -N -L 9090:db.internal:5432 admin@bastion
# ¿Cómo lo cierras?

# Encontrar el PID
ps aux | grep "ssh.*9090"
# O mejor:
pgrep -f "ssh.*-L.*9090"

# Matar
kill $(pgrep -f "ssh.*-L.*9090")

# Prevención: usar un control socket
ssh -M -S /tmp/tunnel-db -f -N -L 9090:db.internal:5432 admin@bastion

# Cerrar limpiamente:
ssh -S /tmp/tunnel-db -O exit bastion

# Ver estado:
ssh -S /tmp/tunnel-db -O check bastion
# Master running (pid=12345)
```

### 5. Olvidar ExitOnForwardFailure

```bash
# Sin ExitOnForwardFailure:
ssh -f -N -L 9090:db:5432 admin@bastion
# Si el puerto 9090 ya está en uso, SSH conecta igualmente
# pero sin el forward. El túnel "funciona" (la sesión SSH está activa)
# pero no reenvía nada → confusión

# Con ExitOnForwardFailure:
ssh -f -N -o ExitOnForwardFailure=yes -L 9090:db:5432 admin@bastion
# Si el forward falla, SSH termina con error
# Sabes inmediatamente que algo está mal
```

---

## Cheatsheet

```bash
# === LOCAL FORWARD (-L) ===
ssh -L 9090:destino:80 user@server              # básico
ssh -L 0.0.0.0:9090:destino:80 user@server      # accesible por otros
ssh -L 9090:localhost:80 user@server             # servicio EN el server
ssh -L 5432:db:5432 -L 3000:web:3000 user@srv   # múltiples

# === REMOTE FORWARD (-R) ===
ssh -R 8080:localhost:3000 user@server           # exponer servicio local
ssh -R '*:8080:localhost:3000' user@server       # en todas las interfaces
ssh -R 2222:localhost:22 user@vps                # acceso SSH inverso

# === DYNAMIC FORWARD (-D) ===
ssh -D 1080 user@server                          # proxy SOCKS5
curl --socks5-hostname localhost:1080 URL         # usar con curl
export ALL_PROXY=socks5h://127.0.0.1:1080         # usar globalmente

# === FLAGS ÚTILES ===
-f          # background (tras autenticación)
-N          # sin comando remoto (solo túnel)
-T          # sin terminal
-C          # compresión
-o ExitOnForwardFailure=yes     # fallar si forward falla
-o ServerAliveInterval=60       # keepalive cada 60s
-o ServerAliveCountMax=3        # cerrar tras 3 fallos

# === BACKGROUND Y CONTROL ===
ssh -f -N -T -L 9090:db:5432 user@srv            # background básico
ssh -M -S /tmp/ctrl -f -N -L 9090:db:5432 u@s    # con control socket
ssh -S /tmp/ctrl -O check bastion                 # verificar estado
ssh -S /tmp/ctrl -O exit bastion                  # cerrar limpiamente
kill $(pgrep -f "ssh.*-L.*9090")                  # matar por PID

# === AUTOSSH ===
autossh -M 0 -o ServerAliveInterval=30 \
    -f -N -L 9090:db:5432 user@srv                # reconexión automática

# === PROXYJUMP ===
ssh -J bastion user@interno                       # saltar por bastion
ssh -J host1,host2 user@final                     # múltiples saltos

# === SSH_CONFIG ===
# LocalForward 9090 db.internal:5432              # -L en config
# RemoteForward 8080 localhost:3000               # -R en config
# DynamicForward 1080                             # -D en config
# ProxyJump bastion                               # -J en config

# === SSHD_CONFIG (SERVIDOR) ===
AllowTcpForwarding yes|no|local|remote
GatewayPorts no|yes|clientspecified
PermitTunnel no|yes|point-to-point|ethernet
```

---

## Ejercicios

### Ejercicio 1 — Túnel local a un servicio web

1. Levanta un contenedor Docker con nginx: `docker run -d --name web-interno nginx:alpine`
2. Obtén la IP del contenedor
3. Crea un túnel SSH local que mapee tu `localhost:8888` al puerto 80 del contenedor (pasando por tu propio SSH local como servidor)
4. Accede a `http://localhost:8888` desde el navegador y verifica que ves la página de nginx
5. Cierra el túnel limpiamente

<details>
<summary>Pista</summary>

El "servidor SSH" puede ser tu propia máquina (`localhost`). Usa `ssh -L 8888:<ip-contenedor>:80 tu-usuario@localhost`.

</details>

<details>
<summary>Solución</summary>

```bash
# 1. Levantar nginx
docker run -d --name web-interno nginx:alpine

# 2. Obtener IP
WEB_IP=$(docker inspect -f '{{range .NetworkSettings.Networks}}{{.IPAddress}}{{end}}' web-interno)
echo $WEB_IP
# Ej: 172.17.0.2

# 3. Crear túnel (usa tu propia máquina como servidor SSH)
ssh -f -N -L 8888:$WEB_IP:80 $(whoami)@localhost

# 4. Verificar
curl http://localhost:8888
# <!DOCTYPE html>... Welcome to nginx! ...

# 5. Cerrar túnel
kill $(pgrep -f "ssh.*-L.*8888")

# Limpiar
docker rm -f web-interno
```

</details>

---

### Ejercicio 2 — Túnel con control socket

1. Crea un túnel local usando un control socket en `/tmp/tunnel-ejercicio`
2. Verifica que el control socket existe y que el túnel está activo
3. Usa el control socket para consultar el estado del túnel
4. Cierra el túnel usando el control socket (no con `kill`)

<details>
<summary>Solución</summary>

```bash
# 1. Crear túnel con control socket
docker run -d --name web2 -p 8888:80 nginx:alpine
ssh -M -S /tmp/tunnel-ejercicio -f -N -T \
    -L 9999:localhost:8888 $(whoami)@localhost

# 2. Verificar socket y túnel
ls -la /tmp/tunnel-ejercicio
# srw------- ... /tmp/tunnel-ejercicio  ← socket UNIX

ss -tlnp | grep 9999
# LISTEN ... 127.0.0.1:9999 ...

curl localhost:9999
# Welcome to nginx!

# 3. Consultar estado
ssh -S /tmp/tunnel-ejercicio -O check localhost
# Master running (pid=XXXXX)

# 4. Cerrar limpiamente
ssh -S /tmp/tunnel-ejercicio -O exit localhost
# Exit request sent.

# Verificar que se cerró
ss -tlnp | grep 9999
# (sin resultados)

# Limpiar
docker rm -f web2
```

</details>

---

### Ejercicio 3 — Proxy SOCKS para resolución DNS remota

1. Abre un proxy SOCKS dinámico en el puerto 1080 contra tu propia máquina
2. Usa `curl` con `--socks5` (sin hostname) para acceder a `http://example.com` — observa que la resolución DNS se hace **localmente**
3. Repite con `--socks5-hostname` — la resolución DNS se hace en el **servidor SSH**
4. Explica la diferencia de seguridad entre ambos

<details>
<summary>Solución</summary>

```bash
# 1. Abrir proxy SOCKS
ssh -D 1080 -f -N $(whoami)@localhost

# 2. Con --socks5 (DNS local)
curl -v --socks5 localhost:1080 http://example.com 2>&1 | grep -E "Trying|Connected"
# * Trying 93.184.216.34:80...  ← tu máquina resolvió el DNS
# El nombre "example.com" fue resuelto ANTES de enviar al proxy

# 3. Con --socks5-hostname (DNS remoto)
curl -v --socks5-hostname localhost:1080 http://example.com 2>&1 | grep -E "Trying|Connected"
# * SOCKS5 connect to example.com:80  ← el nombre se envía al proxy
# El servidor SSH resuelve "example.com"

# 4. Diferencia de seguridad:
# --socks5: tu ISP/red local ve las consultas DNS (sabe que accediste a example.com)
#           aunque el tráfico HTTP vaya por el túnel
# --socks5-hostname: ni DNS ni HTTP salen por tu red local
#           todo pasa por el túnel SSH — máxima privacidad
# Equivalente en variable: socks5:// vs socks5h:// (la "h" = hostname resolution)

# Limpiar
kill $(pgrep -f "ssh.*-D.*1080")
```

</details>

---

### Pregunta de reflexión

> Estás en una red corporativa que bloquea todo excepto HTTP (80) y HTTPS (443) en el firewall de salida. Tu servidor SSH remoto escucha en el puerto 22, que está bloqueado. ¿Cómo puedes establecer un túnel SSH a pesar del firewall? ¿Qué consideraciones éticas y de política de empresa aplican?

> **Razonamiento esperado**: la solución técnica es configurar sshd para escuchar en el puerto 443 (`Port 443` en sshd_config) o usar `ProxyCommand` con herramientas como `corkscrew` o `ncat` para encapsular SSH dentro de un túnel HTTPS a través del proxy corporativo (`ProxyCommand ncat --proxy proxy.corp:3128 --proxy-type http %h %p`). SSH y TLS se parecen lo suficiente en su handshake para pasar inspección básica, aunque un firewall con deep packet inspection (DPI) puede distinguirlos. Éticamente: eludir controles de seguridad corporativos **puede violar la política de uso aceptable** de la empresa, incluso si es técnicamente posible. Estas técnicas son legítimas para: administradores autorizados, pentesters con permiso, o acceso a recursos que la empresa aprobó pero el firewall bloquea por error. Sin autorización, puede ser motivo de despido o acción legal.

---

> **Siguiente tema**: T04 — SSH hardening (fail2ban, rate limiting, cifrados)
