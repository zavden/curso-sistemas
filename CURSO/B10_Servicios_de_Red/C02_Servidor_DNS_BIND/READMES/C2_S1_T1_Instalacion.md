# Instalación de BIND — Servidor DNS

## Índice

1. [Qué es BIND](#qué-es-bind)
2. [Instalación por familia de distro](#instalación-por-familia-de-distro)
3. [Estructura de archivos](#estructura-de-archivos)
4. [named.conf — anatomía del archivo principal](#namedconf--anatomía-del-archivo-principal)
5. [Opciones globales](#opciones-globales)
6. [ACLs](#acls)
7. [Logging](#logging)
8. [Primer arranque y verificación](#primer-arranque-y-verificación)
9. [SELinux y AppArmor](#selinux-y-apparmor)
10. [Firewall](#firewall)
11. [Errores comunes](#errores-comunes)
12. [Cheatsheet](#cheatsheet)
13. [Ejercicios](#ejercicios)

---

## Qué es BIND

BIND (Berkeley Internet Name Domain) es el servidor DNS más usado en Internet. La versión actual es **BIND 9**, mantenida por ISC (Internet Systems Consortium).

```
    ┌────────────────────────────────────────────────┐
    │              Ecosistema DNS en Linux            │
    │                                                │
    │  Servidores autoritativos:                     │
    │  ├── BIND 9        ← el estándar, lo más usado│
    │  ├── PowerDNS      ← backend SQL              │
    │  ├── NSD            ← solo autoritativo        │
    │  └── Knot DNS       ← alto rendimiento         │
    │                                                │
    │  Resolvers recursivos:                         │
    │  ├── BIND 9        ← también hace recursión    │
    │  ├── Unbound       ← solo resolver             │
    │  └── systemd-resolved ← cliente local          │
    │                                                │
    │  BIND es el único que hace ambas cosas.        │
    └────────────────────────────────────────────────┘
```

BIND puede funcionar como:

| Rol | Descripción |
|---|---|
| **Autoritativo** | Responde consultas sobre zonas que posee (es la fuente de verdad) |
| **Recursivo** | Resuelve consultas para clientes navegando la jerarquía DNS |
| **Forwarder** | Reenvía consultas a otro resolver (proxy DNS) |
| **Caching** | Cachea respuestas para acelerar consultas repetidas |
| **Combinado** | Cualquier combinación de los anteriores |

En este tema instalamos BIND y entendemos su configuración base. Los temas siguientes cubren zonas, tipos de servidor y DNSSEC.

---

## Instalación por familia de distro

### Debian/Ubuntu

```bash
# Instalar el servidor DNS
sudo apt update
sudo apt install bind9 bind9utils bind9-dnsutils

# Paquetes:
# bind9          — el servidor (named)
# bind9utils     — herramientas de administración (rndc, named-checkconf, etc.)
# bind9-dnsutils — herramientas de consulta (dig, nslookup, nsupdate)

# Verificar versión
named -v
# BIND 9.18.28-0ubuntu0.24.04.1-Ubuntu (Extended Support Version)
```

### Fedora/RHEL/AlmaLinux

```bash
# Instalar
sudo dnf install bind bind-utils

# Paquetes:
# bind           — servidor + herramientas de administración
# bind-utils     — herramientas de consulta (dig, nslookup)

# Verificar versión
named -v
# BIND 9.18.27 (Extended Support Version)
```

### Diferencias clave entre familias

| Aspecto | Debian/Ubuntu | Fedora/RHEL |
|---|---|---|
| Paquete principal | `bind9` | `bind` |
| Nombre del servicio | `named` / `bind9` | `named` |
| Config principal | `/etc/bind/named.conf` | `/etc/named.conf` |
| Directorio de zonas | `/etc/bind/` o `/var/cache/bind/` | `/var/named/` |
| Directorio de config | `/etc/bind/` | `/etc/named/` |
| Usuario de ejecución | `bind` | `named` |
| Chroot | No por defecto | Disponible (`bind-chroot`) |
| SELinux | No (AppArmor) | Sí (`named_t`) |
| Archivos de zona root | `/usr/share/dns/root.hints` | `/var/named/named.ca` |

---

## Estructura de archivos

### Debian/Ubuntu

```
/etc/bind/
├── named.conf                ← archivo principal (incluye los otros)
├── named.conf.options        ← opciones globales
├── named.conf.local          ← zonas locales (tú defines aquí)
├── named.conf.default-zones  ← zonas por defecto (root, localhost)
├── db.root                   ← root hints (servidores raíz DNS)
├── db.local                  ← zona localhost
├── db.127                    ← zona reversa 127.0.0.0
├── db.0                      ← zona reversa 0.0.0.0
├── db.255                    ← zona reversa 255.0.0.0
├── db.empty                  ← zona vacía (placeholder)
├── rndc.key                  ← clave para rndc (control remoto)
└── zones.rfc1918             ← zonas para IPs privadas (incluido por default)
```

```bash
# El named.conf de Debian es minimalista:
cat /etc/bind/named.conf
# include "/etc/bind/named.conf.options";
# include "/etc/bind/named.conf.local";
# include "/etc/bind/named.conf.default-zones";
```

### Fedora/RHEL

```
/etc/
├── named.conf                ← archivo principal (todo en uno o con includes)
├── named/
│   └── (archivos de configuración adicionales)
└── rndc.key                  ← clave para rndc

/var/named/
├── named.ca                  ← root hints
├── named.localhost           ← zona localhost
├── named.loopback            ← zona reversa 127.0.0
├── named.empty               ← zona vacía
├── data/                     ← datos de runtime (stats, cache dump)
├── dynamic/                  ← zonas dinámicas (DDNS)
└── slaves/                   ← zonas secundarias recibidas
```

### Relación entre archivos

```
    named.conf (principal)
        │
        ├── include → named.conf.options    (opciones globales)
        │               ├── listen-on
        │               ├── forwarders
        │               ├── recursion
        │               └── allow-query
        │
        ├── include → named.conf.local      (TUS zonas)
        │               ├── zone "ejemplo.com" { ... }
        │               └── zone "1.168.192.in-addr.arpa" { ... }
        │
        └── include → named.conf.default-zones
                        ├── zone "." { hint }    (root hints)
                        ├── zone "localhost" { }
                        └── zone "127.in-addr.arpa" { }
```

---

## named.conf — anatomía del archivo principal

### Sintaxis

```c
// named.conf usa una sintaxis tipo C
// Comentarios: //, /* */, o #

// Cada sentencia termina en punto y coma
// Los bloques usan llaves { }

options {
    directiva valor;
    directiva "valor con espacios";
    directiva { sub-valor; sub-valor; };
};

// Los includes son relativos al directorio de trabajo de named
include "/etc/bind/named.conf.options";
```

### Bloques principales

```c
// 1. options — configuración global del servidor
options {
    directory "/var/cache/bind";     // directorio de trabajo
    listen-on { 127.0.0.1; };       // IPs donde escuchar
    recursion yes;                   // permitir recursión
};

// 2. acl — listas de control de acceso (nombradas)
acl "trusted" {
    192.168.1.0/24;
    10.0.0.0/8;
    localhost;
};

// 3. zone — definición de una zona DNS
zone "ejemplo.com" {
    type master;
    file "db.ejemplo.com";
};

// 4. logging — configuración de logs
logging {
    channel default_log {
        file "/var/log/named/default.log" versions 3 size 5m;
        severity info;
        print-time yes;
    };
    category default { default_log; };
};

// 5. controls — acceso de rndc
controls {
    inet 127.0.0.1 port 953
        allow { 127.0.0.1; }
        keys { "rndc-key"; };
};

// 6. key — claves para TSIG/rndc
key "rndc-key" {
    algorithm hmac-sha256;
    secret "base64-encoded-secret";
};

// 7. view — vistas (respuestas diferentes según quién pregunte)
view "internal" {
    match-clients { 192.168.1.0/24; };
    zone "ejemplo.com" { ... };
};
```

---

## Opciones globales

### Opciones esenciales

```c
options {
    // --- Directorio de trabajo ---
    directory "/var/cache/bind";      // Debian
    // directory "/var/named";        // RHEL

    // --- Interfaces de escucha ---
    listen-on { 127.0.0.1; 192.168.1.10; };
    // Solo escuchar en estas IPs
    // "any" para todas las interfaces
    // Por defecto: todas

    listen-on-v6 { ::1; };
    // IPv6 — "none" para deshabilitar

    // --- Puerto (default 53) ---
    // listen-on port 53 { ... };

    // --- Recursión ---
    recursion yes;
    // yes: resolver consultas recursivamente para clientes
    // no:  solo responder consultas de zonas propias (autoritativo puro)

    // --- Quién puede consultar ---
    allow-query { localhost; 192.168.1.0/24; };
    // Quién puede hacer consultas a este servidor
    // "any" para abrir a todos (cuidado: DNS amplification)

    // --- Quién puede hacer recursión ---
    allow-recursion { localhost; 192.168.1.0/24; };
    // Más restrictivo que allow-query
    // Solo estos clientes pueden pedir recursión

    // --- Quién puede transferir zonas ---
    allow-transfer { none; };
    // "none" por defecto — solo habilitar para secundarios conocidos
    // Las transferencias de zona (AXFR) exponen todo el contenido de una zona

    // --- Forwarders ---
    forwarders {
        8.8.8.8;
        1.1.1.1;
    };
    // Reenviar consultas que no puede resolver a estos servidores
    // Útil para caching-only y forwarder

    forward only;
    // "only": si los forwarders no responden, devuelve error
    // "first": intenta forwarders primero, luego recursión propia

    // --- DNSSEC ---
    dnssec-validation auto;
    // auto: valida DNSSEC usando las trust anchors incluidas
    // yes:  valida, pero necesitas configurar trust anchors
    // no:   no valida (inseguro)

    // --- Seguridad ---
    version "not disclosed";
    // Ocultar la versión de BIND (evitar fingerprinting)
    // Por defecto muestra: "BIND 9.18.28..."
    // dig @servidor version.bind chaos txt

    hostname none;
    // Ocultar hostname del servidor

    // --- Rendimiento ---
    max-cache-size 256m;
    // Tamaño máximo del caché DNS en memoria

    max-cache-ttl 86400;
    // TTL máximo para entradas en caché (24h)

    // --- Archivos ---
    pid-file "/run/named/named.pid";
    session-keyfile "/run/named/session.key";

    // --- Managed keys (DNSSEC trust anchors) ---
    managed-keys-directory "/var/cache/bind/managed-keys";    // Debian
    // managed-keys-directory "/var/named/dynamic";            // RHEL
};
```

### Opciones de seguridad adicionales

```c
options {
    // No responder a consultas recursivas de fuera de la red
    allow-recursion { trusted; };     // usa la ACL "trusted"

    // No responder a consultas de versión
    version "Refused";

    // Limitar el tamaño de las respuestas UDP (mitigar amplificación)
    max-udp-size 4096;

    // Rate limiting de respuestas (mitigar DDoS reflexión)
    rate-limit {
        responses-per-second 5;
        window 5;
    };

    // Deshabilitar transferencias por defecto
    allow-transfer { none; };

    // Deshabilitar actualizaciones dinámicas por defecto
    allow-update { none; };

    // Deshabilitar notificaciones a secundarios por defecto
    notify no;
};
```

---

## ACLs

Las ACLs nombradas simplifican la gestión de acceso. Se definen **antes** de `options`:

```c
// Definir ACLs al principio del archivo
acl "trusted" {
    localhost;             // loopback
    localnets;             // redes directamente conectadas
    192.168.1.0/24;        // LAN
    10.0.0.0/8;            // red interna
};

acl "secondary-servers" {
    192.168.1.20;          // ns2.ejemplo.com
    192.168.1.21;          // ns3.ejemplo.com
};

acl "forbidden" {
    0.0.0.0/8;
    224.0.0.0/3;           // multicast + reserved
    // IPs bogon que no deberían hacer consultas DNS
};

// Usar en options:
options {
    allow-query { trusted; };
    allow-recursion { trusted; };
    allow-transfer { secondary-servers; };
    blackhole { forbidden; };   // ignorar completamente estas IPs
};
```

ACLs predefinidas:

| ACL | Significado |
|---|---|
| `any` | Cualquier IP |
| `none` | Ninguna IP |
| `localhost` | IPs del host local (todas las interfaces) |
| `localnets` | Redes a las que el host está conectado directamente |

---

## Logging

BIND tiene un sistema de logging granular con **channels** (destinos) y **categories** (tipos de mensaje):

### Channels — dónde van los logs

```c
logging {
    // Canal a archivo
    channel default_log {
        file "/var/log/named/default.log" versions 5 size 10m;
        // versions: rotación (mantener 5 archivos)
        // size: tamaño máximo antes de rotar
        severity info;
        // Niveles: critical, error, warning, notice, info, debug [level]
        print-time yes;
        print-severity yes;
        print-category yes;
    };

    // Canal a syslog
    channel syslog_channel {
        syslog daemon;           // facility: daemon
        severity warning;
    };

    // Canal a stderr (útil para debugging)
    channel stderr_channel {
        stderr;
        severity debug 3;       // debug nivel 3 (muy detallado)
    };

    // Canal nulo (descartar)
    channel null_channel {
        null;
    };
};
```

### Categories — qué se registra

```c
logging {
    // ... channels definidos arriba ...

    // Asignar categorías a channels
    category default { default_log; syslog_channel; };
    // "default" captura todo lo que no tiene categoría específica

    category queries { queries_log; };
    // Cada consulta DNS que recibe el servidor

    category security { security_log; };
    // Eventos de seguridad (rechazos, TSIG failures)

    category xfer-in { xfer_log; };
    category xfer-out { xfer_log; };
    // Transferencias de zona (entrantes y salientes)

    category dnssec { dnssec_log; };
    // Validación DNSSEC

    category lame-servers { null_channel; };
    // Servidores que dan respuestas incorrectas (ruido, silenciar)

    category edns-disabled { null_channel; };
    // EDNS fallbacks (ruido)
};
```

### Configuración de logging recomendada

```c
logging {
    channel general_log {
        file "/var/log/named/general.log" versions 3 size 10m;
        severity info;
        print-time yes;
        print-severity yes;
        print-category yes;
    };

    channel query_log {
        file "/var/log/named/queries.log" versions 5 size 50m;
        severity info;
        print-time yes;
    };

    channel security_log {
        file "/var/log/named/security.log" versions 3 size 10m;
        severity info;
        print-time yes;
        print-severity yes;
    };

    category default { general_log; };
    category queries { query_log; };
    category security { security_log; };
    category lame-servers { null; };
};
```

```bash
# Crear el directorio de logs
sudo mkdir -p /var/log/named
sudo chown bind:bind /var/log/named     # Debian
# sudo chown named:named /var/log/named # RHEL
```

---

## Primer arranque y verificación

### Validar la configuración antes de arrancar

```bash
# Validar named.conf (sintaxis)
sudo named-checkconf
# Si no hay salida → configuración válida
# Si hay error → muestra la línea y el problema

# Validar con verbose (muestra los includes)
sudo named-checkconf -p
# Imprime toda la configuración resuelta (con includes expandidos)

# Validar un archivo de zona
sudo named-checkzone ejemplo.com /etc/bind/db.ejemplo.com
# zone ejemplo.com/IN: loaded serial 2024032101
# OK

# Validar con más rigor
sudo named-checkconf -z
# Valida la config Y carga todas las zonas referenciadas
```

### Arrancar el servicio

```bash
# Debian/Ubuntu
sudo systemctl enable --now named
# O alternativamente:
sudo systemctl enable --now bind9

# Fedora/RHEL
sudo systemctl enable --now named

# Verificar estado
sudo systemctl status named
# ● named.service - BIND Domain Name Server
#    Active: active (running) since ...
```

### Verificar que escucha

```bash
# Verificar que named escucha en el puerto 53
ss -ulnp | grep :53     # UDP
ss -tlnp | grep :53     # TCP

# Debería mostrar:
# udp   UNCONN  0  0  127.0.0.1:53  0.0.0.0:*  users:(("named",pid=...))
# tcp   LISTEN  0  10 127.0.0.1:53  0.0.0.0:*  users:(("named",pid=...))

# También el puerto 953 para rndc:
ss -tlnp | grep :953
```

### Probar con dig

```bash
# Consultar al servidor local
dig @127.0.0.1 localhost
# ;; ANSWER SECTION:
# localhost.  604800  IN  A  127.0.0.1

# Consultar recursión (si está habilitada)
dig @127.0.0.1 example.com
# Debería resolver si recursion está habilitado y allow-recursion lo permite

# Consultar la versión del servidor
dig @127.0.0.1 version.bind chaos txt
# Si no lo ocultaste:
# version.bind.  0  CH  TXT  "BIND 9.18.28..."
# Si lo ocultaste con version "not disclosed":
# version.bind.  0  CH  TXT  "not disclosed"

# Verificar los root hints
dig @127.0.0.1 . NS
# Debe devolver los 13 root servers (a.root-servers.net, etc.)
```

### Verificar los logs

```bash
# Con journalctl (systemd)
sudo journalctl -u named --since "5 min ago"
# named[1234]: starting BIND 9.18.28 ...
# named[1234]: listening on IPv4 interface lo, 127.0.0.1#53
# named[1234]: running

# Con archivos de log (si configuraste logging)
sudo tail -f /var/log/named/general.log
```

---

## SELinux y AppArmor

### SELinux (Fedora/RHEL)

BIND corre en el contexto SELinux `named_t`, que restringe qué archivos puede leer/escribir:

```bash
# Verificar el contexto
ps -eZ | grep named
# system_u:system_r:named_t:s0   ... /usr/sbin/named

# Archivos de zona deben tener el contexto correcto
ls -Z /var/named/
# system_u:object_r:named_zone_t:s0  named.ca
# system_u:object_r:named_zone_t:s0  named.localhost

# Si creas un archivo de zona nuevo:
sudo touch /var/named/db.ejemplo.com
sudo restorecon -v /var/named/db.ejemplo.com
# Asigna named_zone_t automáticamente

# Si los contextos están mal:
sudo restorecon -Rv /var/named/
```

Problemas comunes con SELinux y BIND:

```bash
# 1. named no puede leer un archivo de zona
# Causa: contexto incorrecto (ej. admin_home_t en vez de named_zone_t)
# Diagnóstico:
sudo ausearch -m AVC -ts recent | grep named
# Solución:
sudo restorecon -v /var/named/db.ejemplo.com

# 2. named no puede escribir en un directorio
# Causa: zonas dinámicas necesitan permiso de escritura
# Para /var/named/dynamic/:
sudo setsebool -P named_write_master_zones on
# Este booleano permite a named escribir archivos de zona (DDNS)

# 3. named en un puerto no estándar
sudo semanage port -a -t dns_port_t -p tcp 5353
sudo semanage port -a -t dns_port_t -p udp 5353
```

### BIND con chroot (Fedora/RHEL)

BIND puede correr en un chroot para aislamiento adicional:

```bash
# Instalar soporte de chroot
sudo dnf install bind-chroot

# El chroot reubica todo bajo /var/named/chroot/
# /var/named/chroot/etc/named.conf
# /var/named/chroot/var/named/
# /var/named/chroot/var/log/named/

# Habilitar el servicio chroot (en lugar del normal)
sudo systemctl disable named
sudo systemctl enable --now named-chroot
```

### AppArmor (Debian/Ubuntu)

```bash
# Verificar el perfil de AppArmor para named
sudo aa-status | grep named
# /usr/sbin/named

# El perfil está en:
cat /etc/apparmor.d/usr.sbin.named

# Si necesitas que named acceda a un directorio no permitido:
# Edita el perfil o crea un override en:
# /etc/apparmor.d/local/usr.sbin.named

# Recargar el perfil:
sudo apparmor_parser -r /etc/apparmor.d/usr.sbin.named
```

---

## Firewall

### Abrir el puerto DNS

```bash
# firewalld (Fedora/RHEL)
sudo firewall-cmd --permanent --add-service=dns
sudo firewall-cmd --reload

# Verificar
sudo firewall-cmd --list-services
# ... dns ...

# ufw (Debian/Ubuntu)
sudo ufw allow 53/tcp
sudo ufw allow 53/udp
# O con el perfil de aplicación:
sudo ufw allow Bind9

# nftables directo
sudo nft add rule inet filter input tcp dport 53 accept
sudo nft add rule inet filter input udp dport 53 accept
```

### Restringir por origen

```bash
# Solo permitir DNS desde la LAN
sudo firewall-cmd --permanent --add-rich-rule='
    rule family="ipv4"
    source address="192.168.1.0/24"
    service name="dns"
    accept'

# rndc solo desde localhost (ya es el default, pero verificar)
sudo firewall-cmd --permanent --add-rich-rule='
    rule family="ipv4"
    source address="127.0.0.1"
    port port="953" protocol="tcp"
    accept'

sudo firewall-cmd --reload
```

> **DNS y amplificación**: un servidor DNS abierto a Internet (`allow-query { any; }` con recursión) puede ser usado para ataques de amplificación DDoS. El atacante envía consultas con IP de origen falsificada (la víctima), y el servidor DNS responde con paquetes mucho más grandes hacia la víctima. Siempre restringir `allow-recursion` a clientes conocidos.

---

## Errores comunes

### 1. Dejar recursión abierta a Internet

```c
// MAL — cualquiera puede usar tu servidor como resolver
options {
    recursion yes;
    allow-query { any; };
    // Sin allow-recursion → todos pueden hacer recursión
};

// BIEN — recursión solo para clientes internos
options {
    recursion yes;
    allow-query { any; };              // responder consultas autoritativas a todos
    allow-recursion { trusted; };       // recursión solo para la LAN
};
```

### 2. Olvidar validar antes de recargar

```bash
# MAL — recargar sin validar
sudo vim /etc/bind/named.conf.local
sudo systemctl reload named
# Si hay error de sintaxis, named puede dejar de funcionar

# BIEN — siempre validar
sudo named-checkconf && sudo systemctl reload named
# named-checkconf falla si hay errores → && previene el reload
```

### 3. Permisos y propiedad incorrectos en archivos de zona

```bash
# Los archivos de zona deben pertenecer al usuario de BIND:
# Debian: bind:bind
# RHEL: named:named

# MAL
ls -la /var/named/db.ejemplo.com
# -rw-r--r-- 1 root root ...

# BIEN
sudo chown named:named /var/named/db.ejemplo.com    # RHEL
sudo chown bind:bind /etc/bind/db.ejemplo.com        # Debian
sudo chmod 640 /var/named/db.ejemplo.com

# Log de error típico:
# zone ejemplo.com/IN: loading from master file db.ejemplo.com failed: permission denied
```

### 4. Conflicto con systemd-resolved en el puerto 53

```bash
# En Ubuntu/Fedora, systemd-resolved escucha en 127.0.0.53:53
# BIND quiere escuchar en 127.0.0.1:53 — no hay conflicto directo
# PERO si configuras BIND para escuchar en todas las interfaces:

# Verificar quién usa el puerto 53
ss -ulnp | grep :53
# Si ves systemd-resolved Y quieres que BIND escuche en 127.0.0.53:

# Opción 1: deshabilitar el stub listener de resolved
sudo mkdir -p /etc/systemd/resolved.conf.d/
sudo tee /etc/systemd/resolved.conf.d/no-stub.conf << 'EOF'
[Resolve]
DNSStubListener=no
EOF
sudo systemctl restart systemd-resolved

# Opción 2: configurar BIND para no competir
# listen-on { 192.168.1.10; };  ← solo la IP de la LAN, no localhost
```

### 5. allow-transfer sin restricción

```c
// MAL — cualquiera puede pedir una transferencia de zona completa
zone "ejemplo.com" {
    type master;
    file "db.ejemplo.com";
    // Sin allow-transfer → usa el default global
    // Si el default global es "any"... todo expuesto
};

// BIEN — solo los secundarios conocidos
zone "ejemplo.com" {
    type master;
    file "db.ejemplo.com";
    allow-transfer { 192.168.1.20; };   // solo ns2
};

// Verificar: intentar transferencia desde otra IP
dig @tu-servidor ejemplo.com AXFR
# Debe devolver "Transfer failed" desde IPs no autorizadas
```

---

## Cheatsheet

```bash
# === INSTALACIÓN ===
# Debian/Ubuntu:
sudo apt install bind9 bind9utils bind9-dnsutils
# Fedora/RHEL:
sudo dnf install bind bind-utils

# === SERVICIO ===
sudo systemctl enable --now named
sudo systemctl status named
sudo systemctl reload named             # recargar config sin restart
sudo systemctl restart named             # restart completo

# === VALIDACIÓN ===
sudo named-checkconf                     # validar named.conf
sudo named-checkconf -p                  # validar y mostrar config resuelta
sudo named-checkconf -z                  # validar config + cargar zonas
sudo named-checkzone zona archivo        # validar archivo de zona

# === VERIFICACIÓN ===
ss -ulnp | grep :53                      # verificar puerto UDP
ss -tlnp | grep :53                      # verificar puerto TCP
dig @127.0.0.1 localhost                 # consulta de prueba
dig @127.0.0.1 version.bind chaos txt    # versión del servidor
dig @127.0.0.1 . NS                     # root hints

# === LOGS ===
sudo journalctl -u named -f              # logs en tiempo real
sudo journalctl -u named --since "1h ago"

# === FIREWALL ===
sudo firewall-cmd --permanent --add-service=dns && sudo firewall-cmd --reload
sudo ufw allow Bind9                     # Debian

# === SELINUX ===
sudo restorecon -Rv /var/named/           # restaurar contextos
sudo setsebool -P named_write_master_zones on  # permitir escritura

# === ARCHIVOS CLAVE ===
# Debian: /etc/bind/named.conf, /etc/bind/named.conf.options
# RHEL:   /etc/named.conf, /var/named/

# === PATRÓN SEGURO ===
# named-checkconf && systemctl reload named
```

---

## Ejercicios

### Ejercicio 1 — Instalación y primer arranque

1. Instala BIND en tu sistema (o en un contenedor Docker)
2. Verifica que el servicio está activo y escucha en el puerto 53
3. Haz una consulta a `localhost` con `dig @127.0.0.1 localhost`
4. Consulta la versión del servidor con `dig @127.0.0.1 version.bind chaos txt`
5. Oculta la versión añadiendo `version "Refused";` en las opciones
6. Recarga el servicio y verifica que la versión ya no se muestra

<details>
<summary>Solución</summary>

```bash
# 1. Instalar
sudo apt install bind9 bind9utils bind9-dnsutils  # Debian
# sudo dnf install bind bind-utils                # RHEL

# 2. Verificar
sudo systemctl enable --now named
sudo systemctl status named
ss -ulnp | grep :53
# UNCONN 0 0 127.0.0.1:53 0.0.0.0:* users:(("named",...))

# 3. Consulta localhost
dig @127.0.0.1 localhost
# ;; ANSWER SECTION:
# localhost.  604800  IN  A  127.0.0.1

# 4. Consulta versión
dig @127.0.0.1 version.bind chaos txt +short
# "BIND 9.18.28..."

# 5. Ocultar versión
# Debian: editar /etc/bind/named.conf.options
# RHEL: editar /etc/named.conf, dentro del bloque options
# Añadir dentro de options { }:
#   version "Refused";

# 6. Recargar y verificar
sudo named-checkconf && sudo systemctl reload named
dig @127.0.0.1 version.bind chaos txt +short
# "Refused"
```

</details>

---

### Ejercicio 2 — Configurar ACLs y restricciones

1. Define una ACL llamada `internal` que incluya `localhost`, `localnets` y `192.168.1.0/24`
2. Configura `allow-query` para aceptar solo desde `internal`
3. Configura `allow-recursion` para aceptar solo desde `internal`
4. Configura `allow-transfer` como `none`
5. Valida, recarga y prueba:
   - `dig @127.0.0.1 example.com` debe funcionar (recursión desde localhost)
   - `dig @<tu-IP> example.com` desde otra máquina en la misma red debe funcionar
   - Imagina qué pasaría si consultas desde una IP fuera de la ACL

<details>
<summary>Solución</summary>

```c
// Al principio de named.conf (antes de options):
acl "internal" {
    localhost;
    localnets;
    192.168.1.0/24;
};

// En options:
options {
    // ... directivas existentes ...
    allow-query { internal; };
    allow-recursion { internal; };
    allow-transfer { none; };
};
```

```bash
# Validar y recargar
sudo named-checkconf && sudo systemctl reload named

# Test desde localhost
dig @127.0.0.1 example.com
# Debe resolver (recursión permitida)

# Test desde otra máquina en la LAN
dig @192.168.1.10 example.com
# Debe resolver (está en la ACL)

# Desde fuera de la ACL:
# dig @IP-publica example.com
# ;; connection timed out   ← query rechazado
```

</details>

---

### Ejercicio 3 — Configurar logging

1. Crea el directorio `/var/log/named/` con permisos correctos
2. Configura un channel para queries que escriba en `/var/log/named/queries.log` con rotación de 3 versiones y tamaño máximo 10MB
3. Configura un channel para seguridad en `/var/log/named/security.log`
4. Silencia la categoría `lame-servers` (enviar a null)
5. Valida, recarga y genera tráfico con `dig`. Verifica que los queries aparecen en el log

<details>
<summary>Solución</summary>

```bash
# 1. Crear directorio
sudo mkdir -p /var/log/named
sudo chown bind:bind /var/log/named    # Debian
# sudo chown named:named /var/log/named  # RHEL
sudo chmod 750 /var/log/named
```

```c
// 2-4. Añadir a named.conf (o named.conf.options):
logging {
    channel query_log {
        file "/var/log/named/queries.log" versions 3 size 10m;
        severity info;
        print-time yes;
    };

    channel security_log {
        file "/var/log/named/security.log" versions 3 size 5m;
        severity info;
        print-time yes;
        print-severity yes;
    };

    category queries { query_log; };
    category security { security_log; };
    category lame-servers { null; };
};
```

```bash
# 5. Validar, recargar y verificar
sudo named-checkconf && sudo systemctl reload named

# Generar queries
dig @127.0.0.1 example.com
dig @127.0.0.1 google.com
dig @127.0.0.1 github.com

# Verificar log
sudo cat /var/log/named/queries.log
# 21-Mar-2026 15:30:01 client @0x... 127.0.0.1#54321 (example.com): query: example.com IN A ...
# 21-Mar-2026 15:30:02 client @0x... 127.0.0.1#54322 (google.com): query: google.com IN A ...
```

</details>

---

### Pregunta de reflexión

> Instalaste BIND en un servidor con IP pública, configuraste `allow-query { any; }` y `recursion yes` sin `allow-recursion`. El servidor funciona perfectamente para tus clientes internos. Dos semanas después, el proveedor de hosting te contacta diciendo que tu servidor está generando enormes cantidades de tráfico DNS saliente. ¿Qué está ocurriendo y cómo lo solucionas?

> **Razonamiento esperado**: tu servidor se convirtió en un **open resolver** — un servidor DNS recursivo accesible a todo Internet. Los atacantes lo están usando para **ataques de amplificación DNS**: envían consultas con IP de origen falsificada (spoofed, la IP de la víctima), pidiendo registros grandes (ANY, TXT). Tu servidor resuelve la consulta recursivamente y envía la respuesta (amplificada) a la víctima. Un paquete de consulta de ~60 bytes genera una respuesta de ~3000 bytes — factor de amplificación ~50x. Solución inmediata: añadir `allow-recursion { internal; }` donde `internal` solo incluye tu red. Verificación: `dig @tu-IP example.com` desde Internet debe devolver `REFUSED` para consultas recursivas. La regla de oro: **nunca** ofrecer recursión a IPs que no controlas. Los servidores autoritativos públicos deben tener `recursion no`.

---

> **Siguiente tema**: T02 — Zonas (zona forward, zona reversa, archivos de zona, registros SOA)
