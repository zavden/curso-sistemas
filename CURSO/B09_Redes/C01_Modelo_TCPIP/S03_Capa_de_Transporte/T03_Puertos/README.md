# Puertos: Well-Known, Registered, Ephemeral y /etc/services

## Índice

1. [Qué es un puerto](#1-qué-es-un-puerto)
2. [Rangos de puertos](#2-rangos-de-puertos)
3. [Well-known ports (0-1023)](#3-well-known-ports-0-1023)
4. [Registered ports (1024-49151)](#4-registered-ports-1024-49151)
5. [Ephemeral ports (49152-65535)](#5-ephemeral-ports-49152-65535)
6. [/etc/services](#6-etcservices)
7. [Sockets: la combinación IP + puerto](#7-sockets-la-combinación-ip--puerto)
8. [Puertos en Linux](#8-puertos-en-linux)
9. [Seguridad y puertos](#9-seguridad-y-puertos)
10. [Errores comunes](#10-errores-comunes)
11. [Cheatsheet](#11-cheatsheet)
12. [Ejercicios](#12-ejercicios)

---

## 1. Qué es un puerto

Un puerto es un número de 16 bits (0-65535) que identifica un **proceso o servicio**
dentro de un host. La dirección IP lleva el paquete a la máquina correcta; el puerto
lo entrega al proceso correcto dentro de esa máquina.

```
Servidor (192.168.1.100):
┌────────────────────────────────────────┐
│                                        │
│  Puerto 22  ──► sshd                   │
│  Puerto 80  ──► nginx (HTTP)           │
│  Puerto 443 ──► nginx (HTTPS)          │
│  Puerto 3306 ──► mysqld                │
│  Puerto 5432 ──► postgresql            │
│                                        │
│  IP identifica la MÁQUINA              │
│  Puerto identifica el PROCESO          │
└────────────────────────────────────────┘
```

### Analogía

```
IP = dirección del edificio (Calle Mayor 10)
Puerto = número de apartamento (4B)

Sin el puerto, el paquete llega al edificio pero no sabe a qué piso ir.
```

### TCP y UDP tienen espacios de puertos independientes

```
TCP puerto 53  →  puede ser un servicio
UDP puerto 53  →  puede ser otro servicio (o el mismo, como DNS)

Son espacios de nombres SEPARADOS. TCP:80 y UDP:80 son dos puertos distintos.
En la práctica, cuando un protocolo usa ambos (como DNS en el 53), suele ser
el mismo servicio escuchando en TCP y UDP simultáneamente.
```

---

## 2. Rangos de puertos

```
Puerto    0 ─────── 1023 ──────── 49151 ──────── 65535
          │  Well-known  │  Registered  │  Ephemeral  │
          │  (sistema)   │  (aplicaciones)│ (temporales)│
          └──────────────┴───────────────┴─────────────┘

IANA (Internet Assigned Numbers Authority) gestiona la asignación oficial.
```

| Rango | Nombre | Quién los usa | Privilegios |
|---|---|---|---|
| **0-1023** | Well-known | Servicios del sistema (SSH, HTTP, DNS) | Requieren root para `bind()` |
| **1024-49151** | Registered | Aplicaciones (MySQL, PostgreSQL, Redis) | No requieren root |
| **49152-65535** | Ephemeral (dynamic) | Asignados automáticamente al cliente | Asignados por el kernel |

---

## 3. Well-known ports (0-1023)

### Puertos que debes conocer

```
Puerto  Proto   Servicio         Descripción
──────  ──────  ───────────────  ─────────────────────────────────
  20    TCP     FTP-data         Transferencia de datos FTP
  21    TCP     FTP              Control de conexión FTP
  22    TCP     SSH              Acceso remoto seguro
  23    TCP     Telnet           Acceso remoto sin cifrado (obsoleto)
  25    TCP     SMTP             Envío de email
  53    TCP/UDP DNS              Resolución de nombres
  67    UDP     DHCP server      Servidor DHCP
  68    UDP     DHCP client      Cliente DHCP
  69    UDP     TFTP             Transferencia simple de archivos
  80    TCP     HTTP             Web sin cifrado
 110    TCP     POP3             Recepción de email
 123    UDP     NTP              Sincronización de tiempo
 143    TCP     IMAP             Recepción de email (acceso remoto)
 161    UDP     SNMP             Monitorización de red
 389    TCP     LDAP             Directorio (autenticación)
 443    TCP     HTTPS            Web con TLS
 445    TCP     SMB              Compartir archivos (Windows/Samba)
 514    UDP     Syslog           Logging remoto
 587    TCP     SMTP submission  Envío de email autenticado
 636    TCP     LDAPS            LDAP con TLS
 993    TCP     IMAPS            IMAP con TLS
 995    TCP     POP3S            POP3 con TLS
```

### Privilegios de root

En Linux, solo root (o procesos con `CAP_NET_BIND_SERVICE`) pueden hacer `bind()`
a puertos menores a 1024:

```bash
# Como usuario normal:
python3 -c "import socket; s=socket.socket(); s.bind(('',80))"
# PermissionError: [Errno 13] Permission denied

# Como root:
sudo python3 -c "import socket; s=socket.socket(); s.bind(('',80))"
# Funciona
```

Esto es una medida de seguridad: impide que un usuario no privilegiado suplante
servicios del sistema. Si alguien logra escuchar en el puerto 22, podría capturar
contraseñas SSH.

### Alternativas a correr como root

```bash
# 1. Capability específica (recomendado)
sudo setcap 'cap_net_bind_service=+ep' /usr/bin/mi_app
# mi_app puede hacer bind a puertos < 1024 sin ser root

# 2. Reverse proxy (lo más común en producción)
# nginx escucha en 80/443 (como root, luego baja privilegios)
# Tu app escucha en 8080 (sin privilegios)
# nginx redirige tráfico a 8080

# 3. sysctl (cambia el límite global, menos seguro)
sudo sysctl -w net.ipv4.ip_unprivileged_port_start=80
```

---

## 4. Registered ports (1024-49151)

Puertos registrados ante IANA por aplicaciones y servicios. No requieren root.

```
Puerto  Proto   Servicio         Descripción
──────  ──────  ───────────────  ─────────────────────────────────
 1433   TCP     MS SQL Server    Base de datos Microsoft
 2049   TCP     NFS              Filesystem de red
 3306   TCP     MySQL/MariaDB    Base de datos
 3389   TCP     RDP              Remote Desktop (Windows)
 5432   TCP     PostgreSQL       Base de datos
 5900   TCP     VNC              Escritorio remoto
 6379   TCP     Redis            Cache/key-value store
 8080   TCP     HTTP alt         Servidor web alternativo
 8443   TCP     HTTPS alt        HTTPS alternativo
 9090   TCP     Prometheus       Monitorización
27017   TCP     MongoDB          Base de datos NoSQL
```

### Convención, no obligación

Que MySQL esté registrado en el 3306 no significa que DEBA usar el 3306. Cualquier
aplicación puede escuchar en cualquier puerto libre:

```bash
# MySQL en su puerto estándar
mysqld --port=3306

# MySQL en un puerto alternativo (válido pero no convencional)
mysqld --port=13306

# El registro de IANA es una CONVENCIÓN, no una restricción técnica
```

---

## 5. Ephemeral ports (49152-65535)

### Qué son

Cuando un cliente inicia una conexión (ej: `curl http://example.com`), el kernel
le asigna automáticamente un **puerto de origen** temporal. Este es el puerto
efímero.

```
Tu PC (192.168.1.10)                    Servidor (93.184.216.34)
 Puerto 54321 (efímero, asignado auto)  Puerto 80 (well-known)
      │                                       │
      │  SYN  src=54321, dst=80              │
      │──────────────────────────────────────►│
      │                                       │
      │  SYN+ACK  src=80, dst=54321          │
      │◄──────────────────────────────────────│

El cliente no elige el 54321 — el kernel lo asigna del pool de efímeros.
```

### Rango configurable en Linux

```bash
# Ver el rango actual
sysctl net.ipv4.ip_local_port_range
# 32768    60999

# IANA dice 49152-65535, pero Linux usa 32768-60999 por defecto
# Esto da: 60999 - 32768 + 1 = 28,232 puertos efímeros disponibles

# Ampliar si necesitas más conexiones salientes simultáneas
sudo sysctl -w net.ipv4.ip_local_port_range="1024 65535"
# 64,512 puertos efímeros
```

### Agotamiento de puertos efímeros

```
Cada conexión TCP ocupa un puerto efímero.
Un servidor con muchas conexiones salientes (proxy, load balancer):

  28,232 puertos efímeros disponibles
  - Conexiones activas (ESTABLISHED)
  - Conexiones en TIME_WAIT (60 segundos tras cerrar)
  = Puertos disponibles

Si se agotan → "Cannot assign requested address" al intentar conectar.

Soluciones:
  1. Ampliar el rango: ip_local_port_range
  2. Reutilizar TIME_WAIT: tcp_tw_reuse=1
  3. Añadir más IPs de origen (cada IP tiene su propio pool)
  4. Usar connection pooling en la aplicación
```

### Un puerto efímero por conexión

```
Si abres 3 conexiones a google.com:80:

  192.168.1.10:54321 → 93.184.216.34:80   (conexión 1)
  192.168.1.10:54322 → 93.184.216.34:80   (conexión 2)
  192.168.1.10:54323 → 93.184.216.34:80   (conexión 3)

Cada conexión usa un puerto efímero diferente.
La combinación (src IP, src port, dst IP, dst port) debe ser ÚNICA.
```

---

## 6. /etc/services

### Qué es

El archivo `/etc/services` mapea nombres de servicio a números de puerto. Lo usan
herramientas como `ss`, `netstat`, `nmap` y funciones como `getservbyname()`.

```bash
# Ver el archivo
head -30 /etc/services
```

```
# /etc/services:
# $Id: services,v 1.49 2017/08/18 12:43:23 ovasik Exp $
#
# service-name  port/protocol  [aliases ...]
ssh             22/tcp
smtp            25/tcp          mail
domain          53/tcp                          # DNS
domain          53/udp                          # DNS
http            80/tcp          www www-http
pop3            110/tcp         pop-3
https           443/tcp
```

### Formato

```
nombre_servicio    puerto/protocolo    [alias ...]    [# comentario]

Ejemplos:
  ssh        22/tcp
  domain     53/tcp         ← DNS sobre TCP
  domain     53/udp         ← DNS sobre UDP
  http       80/tcp    www  ← "www" es un alias
```

### Cómo lo usan las herramientas

```bash
# ss traduce números a nombres usando /etc/services
ss -tln
# Local Address:Port
# 0.0.0.0:ssh            ← muestra "ssh" en vez de "22"

# Con -n, muestra números
ss -tlnp
# 0.0.0.0:22             ← muestra "22"

# getent consulta el archivo
getent services ssh
# ssh                 22/tcp

getent services 3306/tcp
# mysql               3306/tcp
```

### Modificar /etc/services

Puedes añadir servicios custom para que las herramientas los reconozcan:

```bash
# Añadir tu aplicación
echo "myapp    8888/tcp" | sudo tee -a /etc/services

# Ahora ss mostrará "myapp" en vez de "8888"
ss -tl
# 0.0.0.0:myapp
```

> **Nota**: `/etc/services` es solo una tabla de nombres. No abre ni cierra puertos,
> no configura firewalls, no inicia servicios. Es puramente cosmético para las
> herramientas de diagnóstico.

---

## 7. Sockets: la combinación IP + puerto

### Qué identifica una conexión

Una conexión TCP se identifica de forma única por una **tupla de 5 elementos**:

```
(protocolo, IP origen, puerto origen, IP destino, puerto destino)

Ejemplo:
  (TCP, 192.168.1.10, 54321, 93.184.216.34, 80)

Dos conexiones son DISTINTAS si cualquier elemento de la tupla difiere.
```

### Múltiples conexiones al mismo destino

```
Todas estas son conexiones DIFERENTES (puerto origen distinto):
  (TCP, 192.168.1.10, 54321, 93.184.216.34, 80)
  (TCP, 192.168.1.10, 54322, 93.184.216.34, 80)
  (TCP, 192.168.1.10, 54323, 93.184.216.34, 80)
```

### Múltiples clientes al mismo servidor

```
Un servidor nginx en 0.0.0.0:80 puede tener miles de conexiones simultáneas:

  (TCP, 10.0.0.5,    49001, 192.168.1.100, 80)  ← cliente A
  (TCP, 10.0.0.5,    49002, 192.168.1.100, 80)  ← cliente A (otra pestaña)
  (TCP, 10.0.0.6,    49001, 192.168.1.100, 80)  ← cliente B
  (TCP, 172.16.0.10, 55000, 192.168.1.100, 80)  ← cliente C

Todas van al MISMO puerto 80 pero son conexiones distintas.
El servidor distingue cada una por la tupla completa.
```

### Límite teórico de conexiones

```
Para un servidor escuchando en un puerto fijo (ej: 80):

  Variables: IP origen (32 bits) × Puerto origen (16 bits)
  = 2^32 × 2^16 = 2^48 ≈ 281 billones de conexiones teóricas

En la práctica, el límite es la memoria (cada conexión consume ~3-10 KB).
Un servidor con 16 GB de RAM puede manejar ~1-2 millones de conexiones
concurrentes con optimización.
```

### Escuchar en 0.0.0.0 vs una IP específica

```bash
# Escuchar en TODAS las interfaces
bind(0.0.0.0:80)
# Acepta conexiones desde cualquier interfaz de red

# Escuchar en una IP específica
bind(192.168.1.100:80)
# Solo acepta conexiones destinadas a esa IP

# Escuchar en loopback
bind(127.0.0.1:80)
# Solo acepta conexiones locales (seguro para servicios internos)
```

```bash
ss -tlnp
# LISTEN  0  128  0.0.0.0:22     0.0.0.0:*    sshd     ← todas las interfaces
# LISTEN  0  128  127.0.0.1:3306 0.0.0.0:*    mysqld   ← solo localhost
```

---

## 8. Puertos en Linux

### Listar puertos abiertos

```bash
# TCP escuchando
ss -tlnp
# State   Recv-Q  Send-Q  Local Address:Port  Peer Address:Port  Process
# LISTEN  0       128     0.0.0.0:22           0.0.0.0:*          sshd
# LISTEN  0       511     0.0.0.0:80           0.0.0.0:*          nginx

# UDP escuchando
ss -ulnp

# Ambos (TCP + UDP)
ss -tulnp

# Conexiones activas (establecidas)
ss -tnp
```

### Encontrar qué proceso usa un puerto

```bash
# ¿Quién usa el puerto 80?
ss -tlnp | grep :80
# LISTEN  0  511  0.0.0.0:80  0.0.0.0:*  users:(("nginx",pid=1234,fd=6))

# O con lsof
sudo lsof -i :80
# COMMAND  PID   USER   FD   TYPE  DEVICE  SIZE/OFF  NODE  NAME
# nginx    1234  root    6u  IPv4  12345    0t0       TCP   *:http (LISTEN)

# ¿Qué puertos usa un proceso específico?
ss -tlnp | grep nginx
```

### Verificar si un puerto está accesible remotamente

```bash
# Desde otra máquina
# TCP:
nc -zv 192.168.1.100 80
# Connection to 192.168.1.100 80 port [tcp/http] succeeded!

nc -zv 192.168.1.100 9999
# nc: connect to 192.168.1.100 port 9999 (tcp) failed: Connection refused

# Escaneo de múltiples puertos
nc -zv 192.168.1.100 20-25
```

### Puerto en uso (bind error)

```bash
# Si intentas iniciar un servicio en un puerto ocupado:
nginx: [emerg] bind() to 0.0.0.0:80 failed (98: Address already in use)

# Encontrar quién lo ocupa
ss -tlnp | grep :80
sudo lsof -i :80

# SO_REUSEADDR: permitir reutilizar un puerto en TIME_WAIT
# Esto lo configura la aplicación en su código, no el administrador
```

---

## 9. Seguridad y puertos

### Principio de mínimo privilegio

```
Solo abrir los puertos que el servicio NECESITA.
Cada puerto abierto es una superficie de ataque potencial.

Servidor web:
  ✔ Puerto 80 (HTTP)
  ✔ Puerto 443 (HTTPS)
  ✔ Puerto 22 (SSH para administración)
  ✗ Puerto 3306 (MySQL — solo localhost)
  ✗ Puerto 6379 (Redis — solo localhost)
```

### Servicios que solo deben escuchar en localhost

```bash
# MySQL solo accesible localmente
# En my.cnf:
bind-address = 127.0.0.1

# Redis solo accesible localmente
# En redis.conf:
bind 127.0.0.1

# Verificar que NO escuchan en 0.0.0.0
ss -tlnp | grep -E '3306|6379'
# LISTEN  0  128  127.0.0.1:3306  0.0.0.0:*   mysqld    ← OK
# LISTEN  0  128  127.0.0.1:6379  0.0.0.0:*   redis     ← OK

# Si ves 0.0.0.0:3306 → MySQL accesible desde toda la red → PELIGRO
```

### Port scanning

```bash
# Escaneo básico con nmap (solo en TU red, con autorización)
nmap -sT 192.168.1.100         # TCP connect scan
nmap -sU 192.168.1.100         # UDP scan (más lento)
nmap -sT -p 1-1024 192.168.1.100  # Solo well-known ports

# Resultado:
# PORT    STATE SERVICE
# 22/tcp  open  ssh
# 80/tcp  open  http
# 443/tcp open  https
```

### Puertos y firewall

```bash
# Abrir un puerto en firewalld
sudo firewall-cmd --add-port=8080/tcp --permanent
sudo firewall-cmd --reload

# O por nombre de servicio (usa /etc/services)
sudo firewall-cmd --add-service=http --permanent

# Verificar puertos abiertos en el firewall
sudo firewall-cmd --list-ports
sudo firewall-cmd --list-services
```

---

## 10. Errores comunes

### Error 1: "El puerto 80 está abierto" confundido entre ss y firewall

```
"Abierto en ss" y "abierto en el firewall" son cosas DIFERENTES:

ss -tlnp muestra → qué procesos escuchan (dentro del servidor)
firewall-cmd muestra → qué permite el firewall (desde fuera)

Un servicio puede escuchar en el puerto 80 (ss lo muestra)
pero el firewall puede bloquearlo (nadie externo puede acceder).

Para que un servicio sea accesible remotamente necesitas AMBOS:
  1. Proceso escuchando en el puerto (ss)
  2. Firewall permite ese puerto (firewall-cmd / iptables)
```

### Error 2: Confundir TCP y UDP del mismo número de puerto

```bash
# "El puerto 53 está abierto"
# ¿TCP 53 o UDP 53? Son puertos DIFERENTES

ss -tlnp | grep :53   # TCP 53
ss -ulnp | grep :53   # UDP 53

# DNS usa AMBOS, pero por razones distintas:
# UDP 53: consultas normales
# TCP 53: transferencias de zona, respuestas grandes
```

### Error 3: Asumir que un puerto cerrado significa servicio caído

```
Puerto cerrado puede significar:
  1. El servicio no está corriendo
  2. El servicio escucha en otra IP (127.0.0.1 vs 0.0.0.0)
  3. El servicio escucha en otro puerto
  4. El firewall bloquea ese puerto
  5. Un firewall intermedio bloquea el tráfico

Diagnóstico:
  ss -tlnp en el servidor     → ¿el proceso escucha?
  firewall-cmd --list-all     → ¿el firewall permite?
  tcpdump en el servidor      → ¿llegan los paquetes?
```

### Error 4: Exponer servicios internos en 0.0.0.0

```bash
# PELIGROSO: Redis accesible desde toda la red, sin contraseña
redis-server --bind 0.0.0.0
# Cualquiera puede conectarse y ejecutar comandos

# CORRECTO: Redis solo en localhost
redis-server --bind 127.0.0.1

# Si otro servicio necesita acceder: usa un túnel SSH o VPN,
# no expongas el puerto directamente
```

### Error 5: No entender los puertos efímeros en diagnóstico

```bash
ss -tn
# ESTAB  0  0  192.168.1.10:54321  93.184.216.34:80

# "¿Qué servicio usa el puerto 54321?"
# NINGUNO — es un puerto efímero asignado al CLIENTE.
# La conexión es HACIA el puerto 80 del servidor.

# El puerto LOCAL alto (>32768) en conexiones ESTABLISHED
# casi siempre es efímero (el lado cliente de la conexión).
```

---

## 11. Cheatsheet

```
╔══════════════════════════════════════════════════════════════════════╗
║                    PUERTOS — REFERENCIA RÁPIDA                       ║
╠══════════════════════════════════════════════════════════════════════╣
║                                                                      ║
║  RANGOS                                                              ║
║  0-1023       Well-known (requieren root para bind)                  ║
║  1024-49151   Registered (aplicaciones, sin root)                    ║
║  49152-65535  Ephemeral (asignados automáticamente al cliente)        ║
║  Linux default: 32768-60999                                          ║
║                                                                      ║
║  PUERTOS ESENCIALES                                                  ║
║   22  SSH        │  53  DNS       │  80  HTTP     │  443 HTTPS       ║
║   25  SMTP       │  67  DHCP(S)   │ 110  POP3     │  143 IMAP        ║
║  123  NTP        │ 389  LDAP      │ 445  SMB      │  514 Syslog      ║
║  587  SMTP sub   │ 993  IMAPS     │ 3306 MySQL    │ 5432 PostgreSQL  ║
║                                                                      ║
║  TUPLA DE CONEXIÓN (5 elementos)                                     ║
║  (protocolo, IP src, port src, IP dst, port dst)                     ║
║  Debe ser ÚNICA para cada conexión                                   ║
║                                                                      ║
║  /etc/services                                                       ║
║  Mapa nombre↔puerto para herramientas (ss, nmap, getent)             ║
║  NO abre/cierra puertos, NO configura firewall                       ║
║  getent services ssh        Consultar el archivo                     ║
║                                                                      ║
║  COMANDOS                                                            ║
║  ss -tulnp                  Puertos TCP+UDP escuchando               ║
║  ss -tnp                    Conexiones TCP activas                   ║
║  sudo lsof -i :80           ¿Quién usa el puerto 80?                ║
║  nc -zv HOST PORT           ¿Puerto accesible remotamente?           ║
║  nmap -sT HOST              Escaneo de puertos TCP                   ║
║                                                                      ║
║  DIAGNÓSTICO                                                         ║
║  Puerto accesible = proceso escuchando + firewall permite            ║
║  Puerto local alto + ESTABLISHED = puerto efímero (cliente)          ║
║  0.0.0.0:PORT = todas las interfaces                                 ║
║  127.0.0.1:PORT = solo localhost                                     ║
║                                                                      ║
║  SEGURIDAD                                                           ║
║  Servicios internos (DB, cache) → bind 127.0.0.1                    ║
║  Mínimos puertos abiertos en firewall                                ║
║  nmap para auditar puertos expuestos                                 ║
║  No root para puertos < 1024: setcap o reverse proxy                ║
║                                                                      ║
║  PUERTOS EFÍMEROS                                                    ║
║  sysctl net.ipv4.ip_local_port_range    Ver/cambiar rango            ║
║  Agotamiento → "Cannot assign requested address"                     ║
║  Solución: ampliar rango, tcp_tw_reuse, connection pooling           ║
║                                                                      ║
╚══════════════════════════════════════════════════════════════════════╝
```

---

## 12. Ejercicios

### Ejercicio 1: Inventario de puertos en tu sistema

1. Lista todos los puertos TCP y UDP en los que tu sistema escucha:
   ```bash
   ss -tulnp
   ```

2. Para cada servicio que encuentres, clasifícalo:

   ```
   ┌──────────┬────────┬──────────────┬────────────────────────┐
   │ Puerto   │ Proto  │ Proceso      │ Rango (WK/Reg/Eph)     │
   ├──────────┼────────┼──────────────┼────────────────────────┤
   │ 22       │ TCP    │ sshd         │ Well-known             │
   │ ???      │ ???    │ ???          │ ???                    │
   └──────────┴────────┴──────────────┴────────────────────────┘
   ```

3. Para cada servicio, identifica si escucha en `0.0.0.0` (todas las interfaces)
   o en `127.0.0.1` (solo localhost). ¿Alguno debería cambiar por seguridad?

4. Busca cada servicio en `/etc/services`:
   ```bash
   getent services 22/tcp
   getent services 53/udp
   ```

**Pregunta de reflexión**: si encuentras un servicio escuchando en `0.0.0.0:6379`
(Redis), ¿por qué es un riesgo de seguridad? ¿Qué dos acciones tomarías para
protegerlo?

### Ejercicio 2: Observar puertos efímeros

1. Antes de abrir cualquier conexión, cuenta los puertos en uso:
   ```bash
   ss -tn | wc -l
   ```

2. Abre varias conexiones simultáneas:
   ```bash
   for i in $(seq 1 5); do curl -s http://example.com > /dev/null & done
   ```

3. Inmediatamente lista las conexiones:
   ```bash
   ss -tn | grep example
   ```

4. Observa los puertos de origen (locales) — son efímeros.
   ¿Están dentro del rango configurado?
   ```bash
   sysctl net.ipv4.ip_local_port_range
   ```

5. Espera unos segundos y lista las conexiones de nuevo. ¿Cuántas pasaron a
   TIME_WAIT?
   ```bash
   ss -tn state time-wait
   ```

**Pregunta de predicción**: si ejecutas el bucle de curl 10,000 veces, ¿cuántos
puertos efímeros consumirás incluyendo los TIME_WAIT? ¿Podrías agotar el pool?

### Ejercicio 3: Seguridad de puertos

1. Escanea los puertos abiertos de tu propia máquina:
   ```bash
   # Solo los well-known
   nmap -sT -p 1-1024 localhost
   ```

2. Compara con lo que muestra `ss`:
   ```bash
   ss -tlnp
   ```
   ¿Son los mismos puertos? ¿Hay diferencia entre lo que `nmap` ve desde fuera
   y lo que `ss` ve desde dentro?

3. Si tienes firewall activo, verifica:
   ```bash
   sudo firewall-cmd --list-all
   ```
   ¿El firewall bloquea algún puerto que `ss` muestra como abierto?

4. Intenta hacer bind a un puerto privilegiado sin root:
   ```bash
   python3 -c "import socket; s=socket.socket(); s.bind(('',80))"
   ```
   ¿Qué error recibes?

5. Intenta con un puerto no privilegiado:
   ```bash
   python3 -c "import socket; s=socket.socket(); s.bind(('',8080)); print('OK')"
   ```

**Pregunta de reflexión**: ¿por qué la restricción de puertos < 1024 es una medida
de seguridad? Si un atacante obtiene acceso como usuario normal (no root), ¿qué
servicios podría suplantar y cuáles no? ¿Sigue siendo relevante esta protección en
la era de contenedores donde el proceso corre como root dentro del contenedor?
