# T01 — Hostname: hostnamectl, /etc/hostname y /etc/hosts

## Índice

1. [¿Qué es el hostname?](#1-qué-es-el-hostname)
2. [Los tres tipos de hostname en Linux](#2-los-tres-tipos-de-hostname-en-linux)
3. [hostnamectl — Gestión moderna](#3-hostnamectl--gestión-moderna)
4. [/etc/hostname — Persistencia](#4-etchostname--persistencia)
5. [/etc/hosts — Resolución local](#5-etchosts--resolución-local)
6. [hostname y la red: FQDN, dominio, DNS](#6-hostname-y-la-red-fqdn-dominio-dns)
7. [Hostname en DHCP](#7-hostname-en-dhcp)
8. [Hostname en contenedores y cloud](#8-hostname-en-contenedores-y-cloud)
9. [Convenciones de nombres](#9-convenciones-de-nombres)
10. [Errores comunes](#10-errores-comunes)
11. [Cheatsheet](#11-cheatsheet)
12. [Ejercicios](#12-ejercicios)

---

## 1. ¿Qué es el hostname?

El hostname es el **nombre que identifica a una máquina** en una red. Es lo que aparece en el prompt del shell, en los logs de sistema, en los registros DHCP, y en las consultas DNS inversas. Es uno de los conceptos más simples de Linux y, sin embargo, una fuente sorprendente de confusión.

```
El hostname aparece en muchos contextos:

Prompt del shell:
  usuario@webserver-01:~$
            └────────┘
             hostname

Logs (journalctl, syslog):
  Mar 21 10:00:00 webserver-01 sshd[1234]: Accepted publickey...
                   └────────┘
                    hostname

DHCP (lo que el servidor ve):
  lease 192.168.1.100 {
      client-hostname "webserver-01";
  }

DNS inverso:
  100.1.168.192.in-addr.arpa → webserver-01.empresa.local
```

---

## 2. Los tres tipos de hostname en Linux

Linux mantiene **tres** hostnames distintos, gestionados por systemd:

```
┌─────────────────────────────────────────────────────┐
│               Tipos de hostname                     │
├──────────────┬──────────────────────────────────────┤
│ Static       │ El hostname persistente              │
│              │ Se guarda en /etc/hostname            │
│              │ Sigue reglas estrictas: a-z, 0-9, -  │
│              │ Ejemplo: webserver-01                 │
├──────────────┼──────────────────────────────────────┤
│ Pretty       │ Nombre libre, con UTF-8, espacios    │
│              │ Solo para mostrar (UI, login screen)  │
│              │ Se guarda en /etc/machine-info        │
│              │ Ejemplo: "Servidor Web Principal 🌐"  │
├──────────────┼──────────────────────────────────────┤
│ Transient    │ Hostname temporal en memoria          │
│              │ Se pierde al reiniciar                │
│              │ Puede ser asignado por DHCP           │
│              │ Si no hay static, usa este            │
└──────────────┴──────────────────────────────────────┘
```

```
Jerarquía de prioridad:

1. Static hostname  (si existe en /etc/hostname)
   │
   └── Es el que se usa normalmente
       Si no existe ↓

2. Transient hostname  (asignado por DHCP o kernel)
   │
   └── Temporal, se pierde al reiniciar
       Fallback si no hay static

3. Pretty hostname  (decorativo)
   │
   └── Solo para interfaces gráficas
       No afecta la red ni los logs
```

---

## 3. hostnamectl — Gestión moderna

`hostnamectl` es la herramienta de systemd para gestionar el hostname. Es la forma recomendada en cualquier sistema con systemd.

### Ver el hostname actual

```bash
hostnamectl
```

```
 Static hostname: webserver-01
       Icon name: computer-server
         Chassis: server
      Machine ID: a1b2c3d4e5f6789012345678abcdef01
         Boot ID: 12345678-abcd-ef01-2345-6789abcdef01
Operating System: Fedora Linux 41
     CPE OS Name: cpe:/o:fedoraproject:fedora:41
          Kernel: Linux 6.12.5-200.fc41.x86_64
    Architecture: x86-64
 Hardware Vendor: Dell Inc.
  Hardware Model: PowerEdge R640
Firmware Version: 2.19.1
   Firmware Date: Thu 2024-06-13
    Firmware Age: 9month 1w 1d
```

```bash
# Solo el hostname (sin decoración)
hostnamectl hostname
# webserver-01

# O el comando clásico
hostname
# webserver-01

# FQDN
hostname -f
# webserver-01.empresa.local

# Solo el dominio
hostname -d
# empresa.local

# Dirección IP asociada
hostname -I
# 192.168.1.50 10.0.0.1
```

### Cambiar el hostname

```bash
# Cambiar el static hostname (persiste al reiniciar)
sudo hostnamectl set-hostname webserver-02

# Cambiar solo el pretty hostname
sudo hostnamectl set-hostname "Servidor Web de Producción" --pretty

# Cambiar solo el transient hostname (temporal)
sudo hostnamectl set-hostname temp-name --transient

# Cambiar el static hostname y limpiar el pretty
sudo hostnamectl set-hostname dbserver-01
# (automáticamente limpia el pretty si era diferente)
```

Al ejecutar `hostnamectl set-hostname`:

```
¿Qué hace internamente?

1. Escribe el nuevo nombre en /etc/hostname
2. Llama a sethostname() del kernel (cambia el hostname en memoria)
3. Notifica a systemd-hostnamed vía D-Bus
4. Los procesos que dependen del hostname se enteran

NO hace automáticamente:
- Actualizar /etc/hosts (debes hacerlo tú)
- Actualizar DNS (necesita DDNS o manual)
- Notificar a DHCP (en la próxima renovación)
```

### Propiedades adicionales

```bash
# Tipo de chasis (server, desktop, laptop, vm, container, etc.)
sudo hostnamectl set-chassis server

# Icono (usado por algunas GUIs)
sudo hostnamectl set-icon-name computer-server

# Deployment (producción, staging, etc.)
sudo hostnamectl set-deployment production

# Location
sudo hostnamectl set-location "Rack 42, DC Madrid"
```

Estas propiedades se almacenan en `/etc/machine-info`:

```bash
cat /etc/machine-info
```

```
PRETTY_HOSTNAME="Servidor Web de Producción"
CHASSIS=server
ICON_NAME=computer-server
DEPLOYMENT=production
LOCATION="Rack 42, DC Madrid"
```

---

## 4. /etc/hostname — Persistencia

`/etc/hostname` es el archivo que almacena el **static hostname**. Es extraordinariamente simple:

```bash
cat /etc/hostname
```

```
webserver-01
```

Es literalmente una línea con el nombre. Sin comentarios, sin directivas, sin formato especial.

### Reglas del hostname estático

```
Válido:
  webserver-01         ← letras, números, guión
  db-primary           ← minúsculas preferidas
  node42               ← sin guión al inicio/final
  a                    ← mínimo 1 carácter

Inválido:
  Web Server           ← espacios (usar pretty hostname)
  -webserver           ← no puede empezar con guión
  webserver.local      ← el FQDN no va aquí, solo el hostname corto
  servidor_web         ← underscore no es recomendado (funciona pero...)
  WEBSERVER            ← funciona pero la convención es minúsculas

Límite: 64 caracteres máximo (POSIX)
Charset: a-z, 0-9, guión (-)
```

### Editar /etc/hostname directamente

Puedes editar el archivo directamente, pero necesitas un paso adicional para que el cambio tome efecto sin reiniciar:

```bash
# Editar el archivo
echo "nuevo-hostname" | sudo tee /etc/hostname

# Aplicar inmediatamente (sin reiniciar)
sudo hostnamectl set-hostname "$(cat /etc/hostname)"

# O con el comando hostname clásico (no persiste por sí solo,
# pero /etc/hostname ya lo hace persistente)
sudo hostname -F /etc/hostname
```

> **Recomendación**: usa `hostnamectl set-hostname` en vez de editar el archivo manualmente. Hace ambas cosas (archivo + kernel) en un solo paso.

---

## 5. /etc/hosts — Resolución local

`/etc/hosts` proporciona resolución de nombres **local**, sin consultar DNS. Es el primer lugar donde el sistema busca al resolver un nombre (según la configuración de nsswitch.conf).

### Formato

```
# /etc/hosts
# IP          FQDN                    alias(es)

127.0.0.1     localhost
127.0.1.1     webserver-01.empresa.local  webserver-01
::1           localhost ip6-localhost ip6-loopback

# Entradas personalizadas
192.168.1.10  db-primary.empresa.local    db-primary db
192.168.1.11  db-replica.empresa.local    db-replica
10.0.0.50     api-internal.empresa.local  api
```

```
Anatomía de una línea:

192.168.1.10  db-primary.empresa.local    db-primary db
│             │                           │          │
│             │                           │          └── Alias 2
│             │                           └── Alias 1
│             └── FQDN (nombre canónico)
└── Dirección IP

El PRIMER nombre después de la IP es el canónico (lo que devuelve hostname -f)
Los siguientes son alias
```

### 127.0.0.1 vs 127.0.1.1

Esta distinción es específica de Debian/Ubuntu y genera confusión:

```
# Patrón típico en Debian/Ubuntu:
127.0.0.1     localhost
127.0.1.1     mi-hostname.dominio.local  mi-hostname

# ¿Por qué 127.0.1.1 y no 127.0.0.1?
#
# Si mi-hostname apuntara a 127.0.0.1, los servicios que
# escuchan en "hostname" escucharían en loopback, no en
# la interfaz real. Algunos programas (ej: Apache con
# ServerName, correo) necesitan resolver su propio hostname
# a una IP que otros hosts puedan alcanzar.
#
# 127.0.1.1 es un compromiso: permite que hostname -f funcione
# sin depender de DNS, usando una dirección que es loopback
# pero distinta de 127.0.0.1.

# En sistemas con IP estática conocida, es mejor:
127.0.0.1       localhost
192.168.1.50    webserver-01.empresa.local  webserver-01
# Así hostname se resuelve a la IP real del servidor
```

### Mantener /etc/hosts sincronizado con el hostname

Cuando cambias el hostname, **debes actualizar /etc/hosts** manualmente:

```bash
# Cambiar hostname
sudo hostnamectl set-hostname nuevo-nombre

# Actualizar /etc/hosts
sudo sed -i "s/viejo-nombre/nuevo-nombre/g" /etc/hosts

# Verificar
hostname -f
# Debería mostrar: nuevo-nombre.dominio.local (si el FQDN está en hosts)
```

### Usos prácticos de /etc/hosts

```
# Bloquear sitios (apuntar a 0.0.0.0 o 127.0.0.1)
0.0.0.0   ads.tracking.com
0.0.0.0   malware.example.com

# Entornos de desarrollo (apuntar dominios a localhost)
127.0.0.1   myapp.dev
127.0.0.1   api.myapp.dev
127.0.0.1   admin.myapp.dev

# Overrides temporales (testing antes de cambiar DNS)
10.0.0.99   api.empresa.com
# "Quiero probar el nuevo servidor antes de cambiar el DNS público"

# Resolución interna sin depender de DNS
192.168.1.10  db-primary
192.168.1.11  db-replica
192.168.1.20  cache-redis
```

### /etc/hosts vs DNS

```
Flujo de resolución (con nsswitch.conf: hosts: files dns):

Aplicación quiere resolver "db-primary"
    │
    ├── 1. Busca en /etc/hosts         ← files
    │      ¿"db-primary" está aquí?
    │      ├── SÍ → devuelve IP, FIN
    │      └── NO → continúa
    │
    └── 2. Consulta DNS                 ← dns
           ¿El servidor DNS conoce "db-primary"?
           ├── SÍ → devuelve IP, FIN
           └── NO → error: host not found
```

/etc/hosts tiene prioridad sobre DNS (en la configuración por defecto). Esto puede ser una ventaja (override rápido) o un problema (entrada obsoleta que oculta el DNS real).

---

## 6. Hostname y la red: FQDN, dominio, DNS

### FQDN (Fully Qualified Domain Name)

El FQDN es el hostname completo incluyendo el dominio:

```
webserver-01.empresa.local
│            │
│            └── Dominio
└── Hostname (nombre corto)

FQDN = hostname + dominio
```

```bash
# Ver FQDN
hostname -f
# webserver-01.empresa.local

# ¿De dónde sale el dominio?
# 1. De /etc/hosts (si el FQDN está ahí)
# 2. De DNS inverso (PTR record)
# 3. De search/domain en /etc/resolv.conf (some implementations)
```

### Configurar el FQDN correctamente

El FQDN **no se configura en /etc/hostname**. Se configura en **/etc/hosts**:

```
# CORRECTO: hostname en /etc/hostname, FQDN en /etc/hosts
# /etc/hostname:
webserver-01

# /etc/hosts:
127.0.0.1     localhost
192.168.1.50  webserver-01.empresa.local  webserver-01
```

```
# INCORRECTO: FQDN en /etc/hostname
# /etc/hostname:
webserver-01.empresa.local
# Esto técnicamente funciona en algunas distros pero no es recomendado
# El hostname debe ser el nombre corto
```

### Verificar que el FQDN es correcto

```bash
# Estos tres deben ser consistentes:
hostname          # webserver-01
hostname -f       # webserver-01.empresa.local
hostname -d       # empresa.local

# Si hostname -f falla o devuelve solo el hostname corto,
# /etc/hosts probablemente no tiene el FQDN configurado

# Test con getent (usa nsswitch.conf, como las aplicaciones reales):
getent hosts webserver-01
# 192.168.1.50    webserver-01.empresa.local  webserver-01
```

---

## 7. Hostname en DHCP

El hostname interactúa con DHCP en dos direcciones:

```
Cliente → Servidor DHCP (option 12):
  "Mi nombre es webserver-01"
  → El servidor lo registra en el lease
  → Si hay DDNS, actualiza el DNS automáticamente

Servidor DHCP → Cliente (option 12):
  "Tu nombre debería ser dhcp-host-42"
  → El cliente puede aceptar o rechazar
```

### Controlar qué hostname envía el cliente

```bash
# NetworkManager: enviar hostname
nmcli connection modify "eth0" ipv4.dhcp-hostname "webserver-01"

# NetworkManager: NO enviar hostname (privacidad)
nmcli connection modify "eth0" ipv4.dhcp-send-hostname no

# systemd-networkd: en el archivo .network
# [DHCPv4]
# SendHostname=yes
# Hostname=webserver-01

# dhclient.conf:
# send host-name = gethostname();
# send host-name = "webserver-01";
```

### Controlar si el cliente acepta hostname del servidor

```bash
# systemd-networkd:
# [DHCPv4]
# UseHostname=no      # No aceptar hostname del servidor DHCP

# NetworkManager:
# No acepta hostname del DHCP por defecto (en versiones modernas)

# dhclient.conf:
# supersede host-name "mi-hostname";  # Ignorar lo que envíe el servidor
```

### DDNS (Dynamic DNS)

Cuando un servidor DHCP tiene DDNS configurado, actualiza el DNS automáticamente al asignar un lease:

```
1. Cliente envía: hostname = "laptop-juan"
2. Servidor DHCP asigna: 192.168.1.100
3. Servidor DHCP actualiza DNS:
   laptop-juan.empresa.local → 192.168.1.100 (registro A)
   100.1.168.192.in-addr.arpa → laptop-juan.empresa.local (registro PTR)
4. Ahora otros hosts pueden resolver "laptop-juan.empresa.local"
```

---

## 8. Hostname en contenedores y cloud

### Docker

```bash
# Docker asigna un hostname por defecto (ID del contenedor truncado)
docker run -it ubuntu hostname
# a1b2c3d4e5f6

# Especificar hostname
docker run -it --hostname mi-contenedor ubuntu hostname
# mi-contenedor

# En docker-compose
# services:
#   web:
#     hostname: web-container
```

Docker inyecta el hostname en `/etc/hostname` y `/etc/hosts` del contenedor. Estos archivos son montados desde el host y no persisten si se destruye el contenedor.

### Máquinas virtuales (cloud-init)

En instancias cloud (AWS, GCP, Azure), el hostname se configura típicamente por **cloud-init**:

```yaml
# cloud-init: /etc/cloud/cloud.cfg o user-data
hostname: webserver-prod-01
fqdn: webserver-prod-01.us-east-1.compute.internal
manage_etc_hosts: true
# manage_etc_hosts: true → cloud-init actualiza /etc/hosts automáticamente
```

```bash
# AWS: el hostname por defecto es el IP privado
# ip-172-31-42-100.us-east-1.compute.internal

# Cambiar requiere:
# 1. hostnamectl set-hostname nuevo-nombre
# 2. Configurar preserve_hostname en cloud-init
#    para que no lo sobreescriba al reiniciar:
```

```yaml
# /etc/cloud/cloud.cfg
preserve_hostname: true
```

### systemd-nspawn (contenedores systemd)

```bash
# systemd-nspawn usa el nombre del directorio como hostname
systemd-nspawn -D /var/lib/machines/mi-contenedor
# hostname → mi-contenedor

# O explícitamente
systemd-nspawn --hostname=custom-name -D /var/lib/machines/mi-contenedor
```

---

## 9. Convenciones de nombres

No hay un estándar universal, pero existen convenciones ampliamente adoptadas:

### Esquema funcional

```
{función}-{entorno}-{número}

web-prod-01
db-staging-02
cache-dev-01
lb-prod-01        (load balancer)
mon-prod-01       (monitoring)
```

### Esquema por ubicación

```
{ubicación}-{función}-{número}

mad-web-01        (Madrid)
lon-db-01         (London)
us-east-api-03    (cloud region)
```

### Esquema cloud / infraestructura como código

```
{proyecto}-{entorno}-{rol}-{id}

myapp-prod-api-a1b2
myapp-staging-worker-c3d4
```

### Anti-patrones

```
Nombres que evitar:
  ├── Nombres propios: "zeus", "gandalf", "pikachu"
  │   → Divertido con 5 servidores, caótico con 500
  │
  ├── Nombres genéricos: "server1", "server2"
  │   → No dan información sobre la función
  │
  ├── Nombres con info que cambia: "192-168-1-50"
  │   → Si la IP cambia, el nombre miente
  │
  └── Nombres largos: "production-web-server-madrid-rack42-primary"
      → Se truncan en logs, son difíciles de escribir
      → Mantener por debajo de 15-20 caracteres
```

### RFC 1178 — "Choosing a Name for Your Computer"

Este RFC (informativo) de 1990 sigue siendo relevante. Sus recomendaciones principales:

1. No uses el nombre de tu proyecto (los proyectos cambian de nombre)
2. No uses tu nombre propio (te irás de la empresa)
3. No uses nombres ofensivos
4. Usa nombres cortos, fáciles de escribir y recordar
5. Usa un esquema consistente y escalable

---

## 10. Errores comunes

### Error 1: hostname -f devuelve solo el hostname corto

```
PROBLEMA:
$ hostname
webserver-01
$ hostname -f
webserver-01          ← Debería ser webserver-01.empresa.local

CAUSA:
/etc/hosts no tiene el FQDN:
127.0.0.1   localhost
127.0.1.1   webserver-01    ← Solo hostname corto, sin FQDN

CORRECTO:
# /etc/hosts:
127.0.0.1   localhost
127.0.1.1   webserver-01.empresa.local  webserver-01
#            └── FQDN primero              └── alias

$ hostname -f
webserver-01.empresa.local    ← Ahora sí
```

### Error 2: Cambiar el hostname pero no actualizar /etc/hosts

```
PROBLEMA:
sudo hostnamectl set-hostname nuevo-nombre
# Pero /etc/hosts aún tiene:
# 127.0.1.1   viejo-nombre.dominio.local  viejo-nombre

# Resultado:
hostname           → nuevo-nombre
hostname -f        → viejo-nombre.dominio.local  ← ¡INCONSISTENTE!

# Aplicaciones que usan gethostbyname() ven "viejo-nombre"
# El prompt puede no actualizarse hasta un nuevo login

CORRECTO:
sudo hostnamectl set-hostname nuevo-nombre
# Y TAMBIÉN actualizar /etc/hosts:
sudo sed -i 's/viejo-nombre/nuevo-nombre/g' /etc/hosts

# Verificar consistencia:
hostname     # nuevo-nombre
hostname -f  # nuevo-nombre.dominio.local
getent hosts nuevo-nombre  # debe resolverse
```

### Error 3: Poner el FQDN en /etc/hostname

```
INCORRECTO:
# /etc/hostname:
webserver-01.empresa.local

# Esto puede funcionar en algunas distros, pero:
# - El prompt mostrará el FQDN completo: usuario@webserver-01.empresa.local
# - Algunos servicios se confunden con el punto en el hostname
# - No es el comportamiento esperado por las herramientas

CORRECTO:
# /etc/hostname (solo el hostname corto):
webserver-01

# /etc/hosts (aquí va el FQDN):
192.168.1.50  webserver-01.empresa.local  webserver-01
```

### Error 4: Hostname con underscore

```
INCORRECTO:
sudo hostnamectl set-hostname web_server_01
# El underscore (_) no es válido según RFC 952/1123
# Muchos programas (especialmente los relacionados con TLS/SSL)
# rechazan hostnames con underscore

CORRECTO:
sudo hostnamectl set-hostname web-server-01
# Usar guión (-) en lugar de underscore (_)
```

### Error 5: Cloud-init sobreescribe el hostname al reiniciar

```
PROBLEMA:
# En una instancia cloud (AWS, GCP):
sudo hostnamectl set-hostname mi-servidor
# Funciona... hasta el próximo reinicio
# cloud-init lo sobreescribe con el hostname generado

CORRECTO:
# Opción 1: configurar cloud-init para preservar
sudo tee /etc/cloud/cloud.cfg.d/99-hostname.cfg << 'EOF'
preserve_hostname: true
EOF
sudo hostnamectl set-hostname mi-servidor

# Opción 2: configurar el hostname en cloud-init
sudo tee /etc/cloud/cloud.cfg.d/99-hostname.cfg << 'EOF'
hostname: mi-servidor
fqdn: mi-servidor.empresa.local
manage_etc_hosts: true
EOF
```

---

## 11. Cheatsheet

```
VER HOSTNAME
──────────────────────────────────────────────
hostname                   Hostname corto
hostname -f                FQDN
hostname -d                Dominio
hostname -I                IPs asociadas
hostnamectl                Todo (con metadata)

CAMBIAR HOSTNAME
──────────────────────────────────────────────
hostnamectl set-hostname NOMBRE    Static (persiste)
hostnamectl set-hostname "X" --pretty   Solo pretty
hostnamectl set-hostname X --transient  Solo temporal

Después de cambiar:
  1. Actualizar /etc/hosts
  2. Verificar: hostname && hostname -f

ARCHIVOS
──────────────────────────────────────────────
/etc/hostname         Static hostname (1 línea)
/etc/hosts            Resolución local nombre→IP
/etc/machine-info     Pretty hostname, chassis, etc.

/ETC/HOSTS — FORMATO
──────────────────────────────────────────────
IP   FQDN                     alias(es)
───  ──────────────────────    ──────────
127.0.0.1   localhost
192.168.1.50  host.domain.local  host

Primera entrada tras IP = nombre canónico (FQDN)
Siguientes = alias

VERIFICACIÓN
──────────────────────────────────────────────
hostname              Hostname corto
hostname -f           FQDN (debe incluir dominio)
getent hosts NOMBRE   Resolución vía nsswitch
dig NOMBRE            Resolución vía DNS (salta hosts)

REGLAS DE NOMBRE
──────────────────────────────────────────────
Caracteres: a-z, 0-9, guión (-)
Máximo: 64 caracteres
No empezar/terminar con guión
No usar underscore, espacios, puntos
Convención: minúsculas

HOSTNAME EN DHCP
──────────────────────────────────────────────
NM:   ipv4.dhcp-hostname "X"
      ipv4.dhcp-send-hostname no
snd:  [DHCPv4]
      SendHostname=yes
      Hostname=X
      UseHostname=no

CLOUD
──────────────────────────────────────────────
preserve_hostname: true    # En /etc/cloud/cloud.cfg.d/
Docker: --hostname X
nspawn: --hostname=X
```

---

## 12. Ejercicios

### Ejercicio 1: Auditar la configuración de hostname

En tu sistema actual:

1. Ejecuta `hostnamectl` y anota: static hostname, chassis, OS.
2. Compara `hostname`, `hostname -f` y `hostname -d`. ¿Son consistentes? ¿El FQDN incluye un dominio?
3. Lee `/etc/hostname`. ¿Coincide con lo que muestra `hostname`?
4. Lee `/etc/hosts`. ¿Tu hostname aparece ahí? ¿Con FQDN o solo nombre corto?
5. Ejecuta `getent hosts $(hostname)`. ¿Resuelve? ¿A qué IP? ¿Es la IP correcta de tu máquina o es 127.0.1.1?
6. Si `hostname -f` no muestra un FQDN completo, corrígelo editando `/etc/hosts`.

### Ejercicio 2: Cambiar hostname correctamente

En una VM o contenedor:

1. Anota el hostname actual y el contenido de `/etc/hosts`.
2. Cambia el hostname a `lab-server-01` con `hostnamectl set-hostname`.
3. Verifica que `/etc/hostname` cambió.
4. Sin modificar `/etc/hosts`, ejecuta `hostname -f`. ¿Qué devuelve? ¿Por qué?
5. Actualiza `/etc/hosts` para que el FQDN sea `lab-server-01.test.local`.
6. Verifica: `hostname` → `lab-server-01`, `hostname -f` → `lab-server-01.test.local`, `hostname -d` → `test.local`.
7. Abre un nuevo shell. ¿El prompt cambió?

### Ejercicio 3: Diseñar esquema de nombres

Una empresa tiene esta infraestructura:

- 3 servidores web (2 en Madrid, 1 en Barcelona)
- 2 bases de datos (primaria en Madrid, réplica en Barcelona)
- 1 servidor de monitorización (Madrid)
- 1 load balancer (Madrid)
- Entornos: producción y staging

Diseña:
1. Un esquema de nombres consistente para todos los servidores. Justifica las convenciones elegidas.
2. Escribe las entradas de `/etc/hosts` que pondrías en el servidor de monitorización para resolver todos los demás sin depender de DNS.
3. ¿Qué problemas tiene mantener `/etc/hosts` manualmente en todos los servidores? ¿Qué alternativa propondrías?

**Pregunta de reflexión**: En un entorno con 50 servidores, ¿es mejor mantener `/etc/hosts` sincronizado en todas las máquinas (con un script, Ansible, etc.) o confiar completamente en DNS interno? ¿Qué pasa si el servidor DNS cae y los servidores no tienen entradas en `/etc/hosts`? ¿Y si las entradas de `/etc/hosts` quedan desactualizadas respecto al DNS?

---

> **Siguiente tema**: T02 — Bonding y teaming (alta disponibilidad de red, modos de bonding)
