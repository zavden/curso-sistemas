# T01 — Arquitectura Netfilter: Tablas, Chains y el Recorrido del Paquete

## Índice

1. [¿Qué es Netfilter?](#1-qué-es-netfilter)
2. [El recorrido del paquete por el kernel](#2-el-recorrido-del-paquete-por-el-kernel)
3. [Hooks de Netfilter](#3-hooks-de-netfilter)
4. [Tablas](#4-tablas)
5. [Chains (cadenas)](#5-chains-cadenas)
6. [Orden de evaluación: tablas × chains](#6-orden-de-evaluación-tablas--chains)
7. [Targets (destinos)](#7-targets-destinos)
8. [Connection tracking (conntrack)](#8-connection-tracking-conntrack)
9. [La relación entre iptables, nftables y Netfilter](#9-la-relación-entre-iptables-nftables-y-netfilter)
10. [Errores comunes](#10-errores-comunes)
11. [Cheatsheet](#11-cheatsheet)
12. [Ejercicios](#12-ejercicios)

---

## 1. ¿Qué es Netfilter?

Netfilter es el **framework del kernel Linux** que permite interceptar, modificar y filtrar paquetes de red. No es un programa que ejecutas — es una infraestructura dentro del kernel que proporciona "ganchos" (hooks) en puntos clave del procesamiento de paquetes.

```
Relación entre los componentes:

┌───────────────────────────────────────────────┐
│               Espacio de usuario              │
│                                               │
│  iptables    nftables    firewalld    ufw     │
│  (comando)   (comando)   (daemon)   (wrapper) │
│      │           │           │          │     │
│      └─────┬─────┘     ┌────┘          │     │
│            │            │               │     │
│      Netlink / nf_tables API            │     │
└────────────┼────────────┼───────────────┼─────┘
             │            │               │
┌────────────┼────────────┼───────────────┼─────┐
│            ▼            ▼               ▼     │
│         NETFILTER (framework del kernel)      │
│                                               │
│  Hooks en 5 puntos del recorrido del paquete  │
│  Connection tracking (conntrack)              │
│  NAT engine                                   │
│               Kernel Linux                    │
└───────────────────────────────────────────────┘
```

**iptables**, **nftables**, **firewalld** y **ufw** son solo **interfaces** para configurar Netfilter. El filtrado real ocurre en el kernel, dentro de Netfilter. Puedes cambiar la herramienta de usuario sin cambiar el mecanismo subyacente.

---

## 2. El recorrido del paquete por el kernel

Para entender Netfilter, hay que entender cómo un paquete viaja a través del kernel Linux. El recorrido depende de si el paquete es **para este host**, **desde este host**, o **pasa a través de este host** (routing/forwarding):

```
Paquete que LLEGA a este host (INPUT):

  Red → [NIC] → PREROUTING → Decisión de → INPUT → Proceso local
                              routing         │        (sshd, nginx)
                              "¿Es para mí?"  │
                                    │          │
                                    SÍ ────────┘

Paquete que SALE de este host (OUTPUT):

  Proceso local → OUTPUT → Decisión de → POSTROUTING → [NIC] → Red
  (curl, ping)              routing
                            "¿Por dónde sale?"

Paquete que ATRAVIESA este host (FORWARD):

  Red → [NIC] → PREROUTING → Decisión de → FORWARD → POSTROUTING → [NIC] → Red
                              routing                     │
                              "¿Es para mí?"              │
                                    │                     │
                                    NO ───────────────────┘
```

### Diagrama completo

```
                               PROCESO LOCAL
                              (aplicaciones)
                                ▲       │
                                │       │
                              INPUT   OUTPUT
                                │       │
                                │       ▼
PAQUETE ──► NIC ──► PREROUTING ──► ROUTING ──► FORWARD ──► POSTROUTING ──► NIC ──► RED
  entra                         DECISION                                    sale
                                  │
                          ¿Para este host?
                          SÍ → INPUT → app
                          NO → FORWARD → sale
```

Los cinco puntos (PREROUTING, INPUT, FORWARD, OUTPUT, POSTROUTING) son los **hooks** donde Netfilter puede interceptar paquetes.

---

## 3. Hooks de Netfilter

Cada hook se ejecuta en un momento específico del recorrido:

| Hook | Cuándo se ejecuta | Tráfico que ve |
|------|-------------------|----------------|
| **PREROUTING** | Justo al llegar, antes de decidir si es local o forward | Todo el tráfico entrante |
| **INPUT** | Después de routing, si el paquete es para este host | Solo tráfico destinado a procesos locales |
| **FORWARD** | Después de routing, si el paquete va a otro host | Solo tráfico que pasa a través (requiere ip_forward=1) |
| **OUTPUT** | Cuando un proceso local genera un paquete | Solo tráfico generado localmente |
| **POSTROUTING** | Justo antes de salir por la NIC | Todo el tráfico saliente (local + forwarded) |

```
Ejemplos de qué tráfico ve cada hook:

SSH al servidor (destino: este host):
  PREROUTING → INPUT → sshd

ping desde este servidor a 8.8.8.8:
  OUTPUT → POSTROUTING → NIC

Servidor actúa como router/NAT (paquete pasa a través):
  PREROUTING → FORWARD → POSTROUTING

Respuesta de un servicio local:
  OUTPUT → POSTROUTING
```

---

## 4. Tablas

Las tablas agrupan reglas por **propósito**. Cada tabla tiene un subconjunto de los hooks disponibles:

### Tabla filter (filtrado)

La tabla principal para decidir si un paquete se acepta o se descarta:

```
filter:
  ├── INPUT     → Filtrar tráfico que llega a procesos locales
  ├── FORWARD   → Filtrar tráfico que pasa a través (routing)
  └── OUTPUT    → Filtrar tráfico que generan procesos locales
```

Es la tabla por defecto. Cuando ejecutas `iptables -A INPUT ...` sin especificar tabla, se usa `filter`.

### Tabla nat (traducción de direcciones)

Para modificar direcciones IP y puertos (SNAT, DNAT, MASQUERADE):

```
nat:
  ├── PREROUTING  → DNAT (cambiar destino antes de routing)
  ├── OUTPUT      → DNAT para tráfico generado localmente
  └── POSTROUTING → SNAT/MASQUERADE (cambiar origen al salir)
```

> **Importante**: las reglas de NAT solo se aplican al **primer paquete** de cada conexión. Los paquetes siguientes de la misma conexión se traducen automáticamente por el connection tracking.

### Tabla mangle (modificación)

Para modificar campos del paquete (TTL, TOS, marcas):

```
mangle:
  ├── PREROUTING
  ├── INPUT
  ├── FORWARD
  ├── OUTPUT
  └── POSTROUTING
```

Disponible en todos los hooks. Usos principales:
- Cambiar el TTL
- Marcar paquetes (MARK) para policy routing
- Cambiar TOS/DSCP para QoS
- Modificar MSS (TCPMSS)

### Tabla raw (sin tracking)

Para marcar paquetes que no deben pasar por connection tracking:

```
raw:
  ├── PREROUTING  → Tráfico entrante
  └── OUTPUT      → Tráfico saliente
```

Se ejecuta antes que connection tracking. Uso principal: `NOTRACK` para paquetes de alto volumen donde conntrack sería un cuello de botella (servidores DNS/NTP con muchas conexiones).

### Tabla security (SELinux)

```
security:
  ├── INPUT
  ├── FORWARD
  └── OUTPUT
```

Para asignar contextos de seguridad SELinux a paquetes. Raramente configurada manualmente.

### Resumen: qué tabla tiene qué chain

```
           PREROUTING  INPUT  FORWARD  OUTPUT  POSTROUTING
raw            ✓                        ✓
mangle         ✓         ✓      ✓       ✓         ✓
nat (DNAT)     ✓                        ✓
filter                   ✓      ✓       ✓
security                 ✓      ✓       ✓
nat (SNAT)                                        ✓
```

---

## 5. Chains (cadenas)

Una **chain** (cadena) es una lista ordenada de reglas que se evalúan secuencialmente. Hay dos tipos:

### Built-in chains (integradas)

Las 5 chains correspondientes a los hooks de Netfilter: PREROUTING, INPUT, FORWARD, OUTPUT, POSTROUTING. No se pueden eliminar y tienen una **política por defecto** (default policy):

```bash
# Ver políticas actuales
sudo iptables -L -n | grep "Chain.*policy"
```

```
Chain INPUT (policy ACCEPT)
Chain FORWARD (policy DROP)
Chain OUTPUT (policy ACCEPT)
```

La política por defecto se aplica cuando **ninguna regla coincide**:
- `ACCEPT`: aceptar todo lo que no se bloquee explícitamente (**blacklist**)
- `DROP`: descartar todo lo que no se permita explícitamente (**whitelist**)

```
Enfoque blacklist (policy ACCEPT):
  "Acepto todo EXCEPTO lo que prohíba explícitamente"
  ├── Más permisivo
  ├── Fácil de empezar
  └── Riesgo: olvidar bloquear algo

Enfoque whitelist (policy DROP):
  "Descarto todo EXCEPTO lo que permita explícitamente"
  ├── Más seguro
  ├── Requiere pensar cada servicio que necesitas
  └── Riesgo: quedarte sin acceso (SSH bloqueado)
```

### User-defined chains (definidas por el usuario)

Chains personalizadas para organizar reglas. Se invocan con `-j NOMBRE` desde una chain built-in:

```
Chain INPUT (policy DROP)
  ├── Regla 1: -p tcp --dport 22 -j SSH_RULES    ← Salta a chain custom
  ├── Regla 2: -p tcp --dport 80 -j WEB_RULES
  └── Regla 3: ... (si SSH_RULES/WEB_RULES no hicieron match, sigue aquí)

Chain SSH_RULES (0 references)
  ├── Regla 1: -s 10.0.0.0/8 -j ACCEPT   ← SSH solo desde red interna
  ├── Regla 2: -j LOG --log-prefix "SSH-DENIED: "
  └── Regla 3: -j DROP
```

Si un paquete llega al final de una user-defined chain sin match, **vuelve a la chain que lo invocó** y continúa con la siguiente regla.

---

## 6. Orden de evaluación: tablas × chains

Cuando un paquete pasa por un hook, las tablas se evalúan en un orden estricto. Este es el orden completo para cada tipo de tráfico:

### Paquete entrante (destinado a este host)

```
1. raw        PREROUTING     (¿skip conntrack?)
2. conntrack  (connection tracking)
3. mangle     PREROUTING     (modificar paquete)
4. nat        PREROUTING     (DNAT: cambiar destino)
5.            ROUTING DECISION
6. mangle     INPUT          (modificar)
7. filter     INPUT          (¿aceptar o descartar?)
8. security   INPUT          (SELinux)
9.            → PROCESO LOCAL
```

### Paquete saliente (generado localmente)

```
1.            PROCESO LOCAL →
2. raw        OUTPUT         (¿skip conntrack?)
3. conntrack  (connection tracking)
4. mangle     OUTPUT         (modificar)
5. nat        OUTPUT         (DNAT local)
6. filter     OUTPUT         (¿permitir salida?)
7. security   OUTPUT         (SELinux)
8.            ROUTING DECISION
9. mangle     POSTROUTING    (modificar)
10. nat       POSTROUTING    (SNAT/MASQUERADE)
11.           → NIC → RED
```

### Paquete en tránsito (forwarding)

```
1. raw        PREROUTING
2. conntrack
3. mangle     PREROUTING
4. nat        PREROUTING     (DNAT)
5.            ROUTING DECISION (no es para mí → FORWARD)
6. mangle     FORWARD
7. filter     FORWARD        (¿permitir el tránsito?)
8. security   FORWARD
9. mangle     POSTROUTING
10. nat       POSTROUTING    (SNAT/MASQUERADE)
11.           → NIC → RED
```

### Diagrama completo de flujo

```
                           PAQUETE ENTRANTE
                                 │
                     ┌───────────▼───────────┐
                     │   raw PREROUTING      │
                     │   conntrack           │
                     │   mangle PREROUTING   │
                     │   nat PREROUTING      │
                     └───────────┬───────────┘
                                 │
                     ┌───────────▼───────────┐
                     │   ROUTING DECISION    │
                     └──┬──────────────┬─────┘
                        │              │
                   Para mí        Para otro host
                        │              │
              ┌─────────▼────┐  ┌──────▼──────────┐
              │mangle INPUT  │  │ mangle FORWARD   │
              │filter INPUT  │  │ filter FORWARD   │
              │security INPUT│  │ security FORWARD │
              └─────────┬────┘  └──────┬──────────┘
                        │              │
                  PROCESO LOCAL        │
                        │              │
              ┌─────────▼────┐         │
              │ raw OUTPUT   │         │
              │ conntrack    │         │
              │ mangle OUTPUT│         │
              │ nat OUTPUT   │         │
              │ filter OUTPUT│         │
              │security OUT  │         │
              └─────────┬────┘         │
                        │              │
                     ┌──▼──────────────▼──┐
                     │ mangle POSTROUTING │
                     │ nat POSTROUTING    │
                     └────────┬───────────┘
                              │
                        PAQUETE SALE
```

---

## 7. Targets (destinos)

Cuando una regla coincide con un paquete, el **target** determina qué hacer con él:

### Targets terminantes

Detienen el procesamiento de la chain actual:

| Target | Acción | Respuesta al emisor |
|--------|--------|---------------------|
| `ACCEPT` | Aceptar el paquete | El paquete continúa su camino |
| `DROP` | Descartar silenciosamente | Nada (timeout para el emisor) |
| `REJECT` | Descartar y notificar | ICMP "port unreachable" (configurable) |
| `QUEUE` | Enviar a userspace | Para procesamiento por aplicación |

```
DROP vs REJECT:

DROP:
  Emisor envía paquete → (silencio) → timeout
  ├── El emisor no sabe si el host existe
  ├── Más "sigiloso"
  └── El emisor espera hasta timeout (lento para el usuario)

REJECT:
  Emisor envía paquete → ICMP "port unreachable" → emisor sabe inmediatamente
  ├── El emisor sabe que el host existe pero rechaza
  ├── Más "educado"
  └── Respuesta inmediata (mejor experiencia de usuario)

Recomendación:
  - Servidores públicos: DROP (no revelar información)
  - Redes internas: REJECT (debugging más fácil)
  - FORWARD/router: REJECT con --reject-with icmp-host-unreachable
```

### Targets no terminantes

No detienen el procesamiento; la evaluación continúa con la siguiente regla:

| Target | Acción |
|--------|--------|
| `LOG` | Registrar el paquete en syslog y continuar |
| `MARK` | Marcar el paquete (para routing/QoS) y continuar |
| `CONNMARK` | Marcar la conexión |
| `TCPMSS` | Ajustar MSS de TCP |

```bash
# LOG no detiene la evaluación:
iptables -A INPUT -p tcp --dport 22 -j LOG --log-prefix "SSH: "
iptables -A INPUT -p tcp --dport 22 -j ACCEPT
# Primero registra, LUEGO acepta (ambas reglas se evalúan)
```

### Targets de NAT

Solo válidos en la tabla `nat`:

| Target | Tabla/Chain | Acción |
|--------|-------------|--------|
| `DNAT` | nat/PREROUTING | Cambiar IP/puerto destino |
| `SNAT` | nat/POSTROUTING | Cambiar IP/puerto origen |
| `MASQUERADE` | nat/POSTROUTING | SNAT automático (usa IP de la interfaz de salida) |
| `REDIRECT` | nat/PREROUTING | Redirigir a un puerto local |

---

## 8. Connection tracking (conntrack)

El **connection tracking** (seguimiento de conexiones) es uno de los componentes más importantes de Netfilter. Permite que el firewall entienda el estado de cada conexión y tome decisiones basadas en él.

### Estados de conexión

| Estado | Significado |
|--------|-------------|
| `NEW` | Primer paquete de una conexión nueva |
| `ESTABLISHED` | Paquete de una conexión ya establecida (se vio tráfico en ambas direcciones) |
| `RELATED` | Paquete relacionado con una conexión existente (ej: ICMP error, FTP data) |
| `INVALID` | Paquete que no pertenece a ninguna conexión conocida |
| `UNTRACKED` | Paquete marcado con NOTRACK en la tabla raw |

```
Ejemplo: conexión TCP

1. SYN →           Estado: NEW
2. ← SYN-ACK       Estado: ESTABLISHED (ambas direcciones vistas)
3. ACK →           Estado: ESTABLISHED
4. Datos ↔         Estado: ESTABLISHED
5. FIN →           Estado: ESTABLISHED
6. ← FIN-ACK       Estado: ESTABLISHED (hasta que conntrack lo limpie)

Ejemplo: ICMP error relacionado

1. Host envía TCP SYN a puerto cerrado
2. Destino responde con ICMP "port unreachable"
3. El ICMP tiene estado: RELATED
   (relacionado con el intento TCP original)
```

### Uso en reglas de firewall

```bash
# La regla más importante de cualquier firewall:
# Permitir tráfico de conexiones ya establecidas y relacionadas
iptables -A INPUT -m conntrack --ctstate ESTABLISHED,RELATED -j ACCEPT

# Esto permite:
# - Respuestas a conexiones que nosotros iniciamos
# - ICMP errors relacionados con nuestro tráfico
# - Conexiones de datos FTP (RELATED)
# Sin esta regla, cada paquete de respuesta sería evaluado
# como si fuera nuevo → se descartaría con policy DROP
```

### Ver conexiones activas

```bash
# Tabla de conntrack
sudo conntrack -L

# Contar conexiones
sudo conntrack -C

# Filtrar por estado
sudo conntrack -L -p tcp --state ESTABLISHED

# Filtrar por destino
sudo conntrack -L -d 192.168.1.100

# Eliminar una entrada (forzar re-tracking)
sudo conntrack -D -s 10.0.0.5 -d 192.168.1.100 -p tcp --dport 80
```

```bash
# Parámetros del kernel para conntrack
# Máximo de conexiones rastreadas
cat /proc/sys/net/netfilter/nf_conntrack_max
# Default: 65536 (puede ser insuficiente en servidores ocupados)

# Tabla hash (afecta rendimiento)
cat /proc/sys/net/netfilter/nf_conntrack_buckets

# Timeouts
cat /proc/sys/net/netfilter/nf_conntrack_tcp_timeout_established
# Default: 432000 (5 días)

# Si el servidor tiene muchas conexiones (web, DNS):
sudo sysctl -w net.netfilter.nf_conntrack_max=262144
```

---

## 9. La relación entre iptables, nftables y Netfilter

```
Evolución temporal:

2000 ─────── 2014 ─────── 2020 ─────── 2024 ──►

iptables ═══════════════════════════════════════►
(legacy, aún funcional)

              nftables ═════════════════════════►
              (sucesor, integrado en kernel 3.13+)

                          iptables-nft ═════════►
                          (iptables CLI → nftables backend)
```

### Tres formas de configurar Netfilter

```
1. iptables (legacy):
   iptables → xtables API → Netfilter (kernel)
   Herramienta original, sintaxis conocida por todos

2. iptables-nft (compatibilidad):
   iptables → nf_tables API → Netfilter (kernel)
   Misma sintaxis iptables, pero usa nftables internamente
   Distribuciones modernas usan esto por defecto

3. nft (nativo):
   nft → nf_tables API → Netfilter (kernel)
   Sintaxis nueva, más expresiva, mejor rendimiento
```

```bash
# ¿Qué estoy usando?
# Verificar si iptables es legacy o nft-based:
iptables --version
# iptables v1.8.9 (nf_tables)     ← Usa nftables backend
# iptables v1.8.9 (legacy)         ← Usa xtables API
```

> **En la práctica**: la mayoría de distribuciones modernas (Fedora, RHEL 9, Debian 11+, Ubuntu 22.04+) usan `iptables-nft` por defecto. La sintaxis de `iptables` sigue funcionando, pero internamente usa nftables. Aprenderemos la sintaxis de `iptables` primero (siguiente tema) porque es la más conocida, y luego nftables (T04).

---

## 10. Errores comunes

### Error 1: No entender el orden de evaluación de las chains

```
PROBLEMA:
# "Puse una regla en FORWARD pero el paquete para
#  mi servicio local sigue bloqueado"

# Tráfico destinado a ESTE host NO pasa por FORWARD.
# Solo pasa por INPUT.

REGLA:
  Tráfico para mí      → INPUT
  Tráfico que pasa a través → FORWARD
  Tráfico que yo genero → OUTPUT
```

### Error 2: Confundir cuándo usar PREROUTING vs POSTROUTING para NAT

```
PROBLEMA:
# "Quiero hacer SNAT en PREROUTING"
iptables -t nat -A PREROUTING -j SNAT --to-source 1.2.3.4
# ERROR: SNAT target no válido en PREROUTING

CORRECTO:
# DNAT (cambiar destino) → PREROUTING (antes de routing)
iptables -t nat -A PREROUTING -j DNAT --to-destination ...

# SNAT (cambiar origen) → POSTROUTING (después de routing)
iptables -t nat -A POSTROUTING -j SNAT --to-source ...

# Lógica: cambias el destino ANTES de decidir la ruta (PREROUTING)
#         cambias el origen DESPUÉS de decidir la ruta (POSTROUTING)
```

### Error 3: Olvidar la regla ESTABLISHED,RELATED con policy DROP

```
PROBLEMA:
iptables -P INPUT DROP
iptables -A INPUT -p tcp --dport 22 -j ACCEPT
# SSH funciona... para conectar.
# Pero las respuestas de DNS, HTTP, ping que YO inicié se bloquean.

# Con policy DROP, TODA respuesta entrante se descarta a menos
# que haya una regla explícita.

CORRECTO:
iptables -P INPUT DROP
# PRIMERO: permitir tráfico de conexiones establecidas
iptables -A INPUT -m conntrack --ctstate ESTABLISHED,RELATED -j ACCEPT
# SEGUNDO: reglas para servicios específicos
iptables -A INPUT -p tcp --dport 22 -j ACCEPT
# Loopback
iptables -A INPUT -i lo -j ACCEPT
# ICMP (ping)
iptables -A INPUT -p icmp -j ACCEPT
```

### Error 4: No saber que NAT solo se aplica al primer paquete

```
PROBLEMA:
# Cambié una regla DNAT pero las conexiones existentes
# siguen usando la regla anterior

RAZÓN:
# Las reglas de NAT solo se evalúan para el PRIMER paquete
# de cada conexión (estado NEW). Los paquetes siguientes
# (ESTABLISHED) se traducen automáticamente por conntrack.

SOLUCIÓN:
# Para que las nuevas reglas afecten a todas las conexiones:
# 1. Cambiar la regla
# 2. Limpiar la tabla conntrack (interrumpe conexiones activas)
sudo conntrack -F
# O eliminar solo las entradas relevantes:
sudo conntrack -D -p tcp --dport 80
```

### Error 5: Confundir tablas (filter vs nat)

```
PROBLEMA:
# "Quiero bloquear tráfico al puerto 80"
iptables -t nat -A INPUT -p tcp --dport 80 -j DROP
# ERROR: no hay INPUT en la tabla nat

# La tabla nat solo tiene: PREROUTING, OUTPUT, POSTROUTING
# Para filtrar tráfico, usa la tabla filter (default)

CORRECTO:
iptables -A INPUT -p tcp --dport 80 -j DROP
# Sin -t → usa tabla filter por defecto
```

---

## 11. Cheatsheet

```
HOOKS (puntos de intercepción)
──────────────────────────────────────────────
PREROUTING   Paquete acaba de llegar (antes de routing)
INPUT        Paquete destinado a este host
FORWARD      Paquete que pasa a través (routing)
OUTPUT       Paquete generado localmente
POSTROUTING  Paquete a punto de salir

TABLAS
──────────────────────────────────────────────
filter    Filtrado (ACCEPT/DROP/REJECT)     ← DEFAULT
nat       Traducción de direcciones
mangle    Modificación de paquetes
raw       Skip connection tracking
security  SELinux

QUÉ TABLA TIENE QUÉ CHAIN
──────────────────────────────────────────────
         PRE   INPUT  FWD   OUTPUT  POST
raw       ✓                  ✓
mangle    ✓      ✓     ✓     ✓       ✓
nat       ✓             ✓     ✓       ✓
filter           ✓     ✓     ✓
security         ✓     ✓     ✓

FLUJO POR TIPO DE TRÁFICO
──────────────────────────────────────────────
Para mí:    PREROUTING → INPUT → proceso
De mí:      proceso → OUTPUT → POSTROUTING
A través:   PREROUTING → FORWARD → POSTROUTING

TARGETS PRINCIPALES
──────────────────────────────────────────────
Terminantes:
  ACCEPT    Aceptar
  DROP      Descartar (silencioso)
  REJECT    Descartar (con ICMP error)

No terminantes:
  LOG       Registrar y continuar
  MARK      Marcar paquete y continuar

NAT:
  DNAT          Cambiar destino (PREROUTING)
  SNAT          Cambiar origen (POSTROUTING)
  MASQUERADE    SNAT automático (POSTROUTING)
  REDIRECT      Redirigir a puerto local

CONNECTION TRACKING
──────────────────────────────────────────────
NEW          Primer paquete de conexión nueva
ESTABLISHED  Conexión vista en ambas direcciones
RELATED      Relacionado con conexión existente
INVALID      No pertenece a ninguna conexión

conntrack -L              Ver conexiones
conntrack -C              Contar conexiones
conntrack -F              Limpiar tabla
conntrack -D -p tcp ...   Eliminar entrada específica

POLÍTICAS
──────────────────────────────────────────────
ACCEPT (blacklist)  → Permitir todo, bloquear lo específico
DROP (whitelist)    → Bloquear todo, permitir lo específico
```

---

## 12. Ejercicios

### Ejercicio 1: Trazar el recorrido de un paquete

Para cada escenario, indica qué hooks de Netfilter atraviesa el paquete y en qué tablas se evalúa (en orden):

1. Un cliente externo hace `curl http://tu-servidor:80`. El paquete llega a tu servidor donde nginx escucha en el puerto 80.
2. Desde tu servidor, ejecutas `ping 8.8.8.8`. El paquete ICMP sale de tu máquina.
3. Tu servidor actúa como router NAT. Un PC en la red interna (192.168.1.100) accede a google.com. El paquete llega a tu servidor y debe ser reenviado.
4. Tienes DNAT configurado: el tráfico al puerto 80 de tu IP pública se redirige a un servidor interno 192.168.1.10:8080. Un paquete llega al puerto 80. ¿Qué hooks recorre?

### Ejercicio 2: Entender políticas y orden

Dadas estas reglas:

```bash
iptables -P INPUT DROP
iptables -A INPUT -i lo -j ACCEPT
iptables -A INPUT -m conntrack --ctstate ESTABLISHED,RELATED -j ACCEPT
iptables -A INPUT -p tcp --dport 22 -j ACCEPT
iptables -A INPUT -p tcp --dport 80 -j ACCEPT
iptables -A INPUT -p icmp -j ACCEPT
```

Responde:
1. ¿Qué pasa con un paquete TCP al puerto 443 (HTTPS) desde una IP externa?
2. ¿Qué pasa con un paquete de respuesta DNS (UDP 53) que regresa de 8.8.8.8 después de que este servidor hizo una consulta DNS?
3. ¿Qué pasa si reordenas las reglas y pones `-p icmp -j ACCEPT` al principio? ¿Cambia el comportamiento?
4. Si cambias la política a `iptables -P INPUT ACCEPT`, ¿qué reglas se vuelven innecesarias?
5. ¿Por qué la regla de loopback (`-i lo -j ACCEPT`) es importante?

### Ejercicio 3: Connection tracking

1. Ejecuta `sudo conntrack -L` (o `cat /proc/net/nf_conntrack`) y observa las conexiones activas. Identifica al menos una conexión TCP ESTABLISHED y describe los campos que ves.
2. Si tu servidor tiene `conntrack_max=65536` y recibes 70,000 conexiones simultáneas, ¿qué pasa con las conexiones 65,537 en adelante?
3. ¿Por qué el timeout por defecto para TCP ESTABLISHED es 5 días? ¿Cuándo sería un problema? ¿Cómo lo ajustarías para un servidor web con muchas conexiones cortas?
4. Explica con un ejemplo por qué el estado `RELATED` es necesario (piensa en FTP activo o en un mensaje ICMP "destination unreachable").

**Pregunta de reflexión**: Un administrador configura `iptables -P INPUT DROP` en un servidor remoto vía SSH, pero olvida agregar la regla para SSH **antes** de cambiar la política. ¿Qué pasa? ¿Cómo se previene esto? ¿Existe algún mecanismo de "timeout" que revierta la configuración si pierdes acceso?

---

> **Siguiente tema**: T02 — iptables (reglas, targets, matching)
