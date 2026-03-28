# Concepto de proxy forward vs reverse

## Índice

1. [¿Qué es un proxy?](#1-qué-es-un-proxy)
2. [Proxy forward](#2-proxy-forward)
3. [Proxy reverse](#3-proxy-reverse)
4. [Forward vs reverse: comparación directa](#4-forward-vs-reverse-comparación-directa)
5. [Proxy transparente](#5-proxy-transparente)
6. [Casos de uso corporativos](#6-casos-de-uso-corporativos)
7. [Cadena de proxies y encabezados](#7-cadena-de-proxies-y-encabezados)
8. [HTTPS y el problema del cifrado](#8-https-y-el-problema-del-cifrado)
9. [Errores comunes](#9-errores-comunes)
10. [Cheatsheet](#10-cheatsheet)
11. [Ejercicios](#11-ejercicios)

---

## 1. ¿Qué es un proxy?

Un proxy es un **intermediario** que se interpone entre un cliente y un servidor. En vez de comunicarse directamente, el tráfico pasa por el proxy, que puede inspeccionarlo, modificarlo, cachearlo o bloquearlo.

```
Sin proxy:
┌─────────┐                          ┌──────────┐
│ Cliente │ ────── HTTP/HTTPS ──────►│ Servidor │
│         │ ◄───────────────────────│          │
└─────────┘                          └──────────┘

Con proxy:
┌─────────┐         ┌───────┐         ┌──────────┐
│ Cliente │ ──────► │ Proxy │ ──────► │ Servidor │
│         │ ◄────── │       │ ◄────── │          │
└─────────┘         └───────┘         └──────────┘
                    El proxy puede:
                    - Cachear respuestas
                    - Filtrar contenido
                    - Registrar accesos
                    - Autenticar usuarios
                    - Modificar cabeceras
```

La distinción fundamental entre los dos tipos de proxy depende de **a quién sirve**:

```
Forward proxy  →  sirve a los CLIENTES (protege la red interna)
Reverse proxy  →  sirve a los SERVIDORES (protege el backend)
```

---

## 2. Proxy forward

Un proxy forward actúa **en nombre de los clientes**. Los clientes internos de una red envían sus peticiones al proxy, y éste las reenvía a Internet. El servidor destino ve la IP del proxy, no la del cliente.

```
Red interna                              Internet
┌──────────┐                                        ┌───────────────┐
│ PC-Alice │──┐                                     │ www.google.com│
│10.0.0.10 │  │     ┌───────────────┐               └───────────────┘
├──────────┤  ├────►│ Proxy forward │──────────────►
│ PC-Bob   │──┤     │  10.0.0.1     │               ┌───────────────┐
│10.0.0.11 │  │     │  (Squid)      │◄──────────────│ www.github.com│
├──────────┤  │     └───────────────┘               └───────────────┘
│ PC-Carol │──┘           │
│10.0.0.12 │         El servidor ve:
└──────────┘         IP origen = 10.0.0.1
                     (no sabe quién es Alice/Bob/Carol)
```

### Características del proxy forward

| Aspecto | Descripción |
|---|---|
| **Quién lo configura** | El cliente (o el admin de la red interna) |
| **Quién sabe que existe** | El cliente lo sabe (explícito) o no (transparente) |
| **Qué protege** | La red interna (clientes) |
| **IP visible** | El servidor destino ve la IP del proxy |
| **Dirección del tráfico** | Clientes internos → proxy → Internet |

### Funciones del proxy forward

**1. Control de acceso (filtrado de contenido)**

```
Alice quiere visitar youtube.com
    │
    ▼
Proxy revisa la política:
    ├── youtube.com está en la lista negra
    └── DENIEGA la petición → Alice ve página de bloqueo
```

**2. Caché**

```
09:00  Alice pide example.com/logo.png
         Proxy: no lo tengo → pide a Internet → guarda copia → responde a Alice

09:05  Bob pide example.com/logo.png
         Proxy: ya lo tengo en caché → responde directamente (sin salir a Internet)
         Ahorro: ancho de banda y tiempo
```

**3. Registro (logging)**

```
Quién         Cuándo              Qué                    Cuánto
alice         2026-03-21 09:00    www.google.com/search  15 KB
bob           2026-03-21 09:01    github.com/repo        120 KB
carol         2026-03-21 09:02    stackoverflow.com      45 KB
```

**4. Anonimato**

El servidor destino solo ve la IP del proxy. No puede identificar al cliente individual (a menos que el proxy inserte cabeceras como `X-Forwarded-For`).

### Software de proxy forward

| Software | Descripción |
|---|---|
| **Squid** | El estándar en Linux, maduro, potente caché |
| **Tinyproxy** | Ligero, simple, ideal para entornos pequeños |
| **Privoxy** | Enfocado en privacidad, filtra publicidad |
| **SOCKS proxy** (ssh -D) | Túnel genérico a nivel de socket |

---

## 3. Proxy reverse

Un proxy reverse actúa **en nombre de los servidores**. Los clientes de Internet no saben que hay un proxy — piensan que están hablando directamente con el servidor. El proxy recibe las peticiones y las reenvía al backend apropiado.

```
Internet                                 Red interna (DMZ)
                                         ┌──────────────┐
┌──────────┐                             │ App server 1 │
│ Usuario  │                             │ 10.0.1.10    │
│ (Chrome) │     ┌───────────────┐   ┌──►│ :8080        │
│          │────►│ Reverse proxy │───┤   ├──────────────┤
│          │◄────│ web.example.  │◄──┤   │ App server 2 │
└──────────┘     │ com           │   └──►│ 10.0.1.11    │
                 │ (Nginx)       │       │ :8080        │
                 │ IP pública    │       └──────────────┘
                 └───────────────┘
                 El usuario ve:
                 IP destino = web.example.com
                 (no sabe que hay dos backends)
```

### Características del proxy reverse

| Aspecto | Descripción |
|---|---|
| **Quién lo configura** | El administrador de los servidores backend |
| **Quién sabe que existe** | El cliente NO lo sabe (es transparente para él) |
| **Qué protege** | Los servidores backend |
| **IP visible** | El cliente ve la IP del reverse proxy |
| **Dirección del tráfico** | Internet → reverse proxy → servidores internos |

### Funciones del proxy reverse

**1. Balanceo de carga**

```
Petición 1 ──► Reverse proxy ──► Backend A
Petición 2 ──► Reverse proxy ──► Backend B
Petición 3 ──► Reverse proxy ──► Backend A
                    │
          Algoritmos: round-robin, least connections,
                      IP hash, weighted, health checks
```

**2. Terminación TLS**

```
Internet                    Red interna
Cliente ──HTTPS──► Nginx ──HTTP──► App (puerto 8080)
                   │
                   Nginx gestiona el certificado TLS
                   La app no necesita saber de TLS
                   (simplifica la aplicación)
```

**3. Virtual hosting**

```
                               ┌─────────────────┐
blog.example.com ──┐           │ WordPress :8080  │
                   │           └─────────────────┘
                   ▼
               Reverse ───────►  según Host header
               proxy
                   ▲
                   │           ┌─────────────────┐
shop.example.com ──┘           │ Magento :8081    │
                               └─────────────────┘
```

**4. WAF y protección**

El reverse proxy puede filtrar ataques antes de que lleguen al backend (SQL injection, XSS, DDoS).

**5. Caché de contenido estático**

```
/api/users ──────────► Reverse proxy ──► Backend (dinámico)
/images/logo.png ────► Reverse proxy ──► Caché local (no molesta al backend)
/css/style.css ──────► Reverse proxy ──► Caché local
```

### Software de reverse proxy

| Software | Descripción |
|---|---|
| **Nginx** | El más popular, alto rendimiento, config simple |
| **HAProxy** | Especializado en balanceo de carga, capa 4/7 |
| **Apache httpd** | mod_proxy, flexible pero más pesado |
| **Traefik** | Auto-configuración con Docker/Kubernetes |
| **Envoy** | Service mesh, observabilidad avanzada |
| **Caddy** | TLS automático con Let's Encrypt |

---

## 4. Forward vs reverse: comparación directa

```
FORWARD PROXY                           REVERSE PROXY

Los clientes ───► PROXY ───► Internet   Internet ───► PROXY ───► Backends

Protege a los CLIENTES                  Protege a los SERVIDORES
El servidor no ve al cliente            El cliente no ve al backend
El cliente SABE del proxy (*)           El cliente NO SABE del proxy
Caché SALIENTE                          Caché ENTRANTE
Filtra lo que sale                      Filtra lo que entra

(*) excepto en proxy transparente
```

| Aspecto | Forward proxy | Reverse proxy |
|---|---|---|
| **Posición** | Junto a los clientes | Junto a los servidores |
| **Protege** | Red interna / clientes | Backend / servidores |
| **El cliente sabe** | Sí (configura proxy) | No (accede por dominio normal) |
| **El servidor sabe** | No (ve IP del proxy) | Sí (sabe que el proxy reenvía) |
| **Caso de uso principal** | Filtrado, caché de salida, logging | Balanceo, TLS, virtual hosting |
| **Ejemplo de software** | Squid, Tinyproxy | Nginx, HAProxy, Traefik |
| **Puerto típico** | 3128 (Squid), 8080 | 80, 443 |
| **Config en el cliente** | `http_proxy=...` | Ninguna (DNS apunta al proxy) |

### Pueden coexistir

En una red corporativa típica, ambos proxies operan simultáneamente:

```
Red interna           DMZ                    Internet

┌────────┐     ┌─────────────┐
│ PC-Dev │────►│ Squid       │──────────────────────────► google.com
│        │     │ (forward)   │                            github.com
└────────┘     └─────────────┘                            stackoverflow.com
                                                          ▲
┌────────────┐ ┌─────────────┐                            │
│ App server │◄│ Nginx       │◄───────────────────────────┘
│ (backend)  │ │ (reverse)   │          Clientes externos
└────────────┘ └─────────────┘          acceden a app.empresa.com

Forward: controla qué sitios pueden visitar los empleados
Reverse: protege y balancea los servidores de la empresa
```

---

## 5. Proxy transparente

Un proxy transparente intercepta el tráfico **sin que el cliente lo configure**. El cliente cree que está hablando directamente con el servidor destino.

### Cómo funciona

```
Sin proxy transparente:
  Cliente ──────────────────────────────► Servidor
  (el cliente conoce la IP del servidor)

Con proxy transparente:
  Cliente ──────► [Firewall/Router] ──── REDIRIGE ──── ► Proxy ──► Servidor
                  iptables REDIRECT                       │
                  puerto 80 → 3128                        └── El cliente no sabe
                                                              que el proxy existe
```

La redirección se hace en el firewall/router:

```bash
# iptables: redirigir todo el tráfico HTTP al proxy Squid local
iptables -t nat -A PREROUTING -i eth0 -p tcp --dport 80 \
    -j REDIRECT --to-port 3128

# Si el proxy está en otra máquina:
iptables -t nat -A PREROUTING -i eth0 -p tcp --dport 80 \
    -j DNAT --to-destination 10.0.0.1:3128
```

### Limitaciones con HTTPS

```
HTTP (puerto 80):
  El proxy transparente puede inspeccionar la URL completa
  porque el tráfico no está cifrado
  → Funciona bien para caché y filtrado

HTTPS (puerto 443):
  El proxy solo ve el destino (IP/SNI) pero NO el contenido
  porque está cifrado con TLS
  → Solo puede filtrar por dominio, no por URL
  → Para inspeccionar: necesita TLS interception (ver sección 8)
```

### Comparación explícito vs transparente

| Aspecto | Proxy explícito | Proxy transparente |
|---|---|---|
| Config en el cliente | Sí (`http_proxy=...`) | No |
| El cliente sabe | Sí | No |
| Intercepta HTTPS (contenido) | Sí (CONNECT method) | No (solo SNI/dominio) |
| Requiere cambios en firewall | No | Sí (iptables/DNAT) |
| Protocolo | HTTP CONNECT, proxy auth | Redirección de paquetes |
| Despliegue | Más trabajo por cliente | Un solo punto (router/firewall) |

---

## 6. Casos de uso corporativos

### Caso 1: Control de acceso a Internet (forward proxy)

```
Política empresarial:
  - Departamento "Finanzas": solo acceso a bancos y herramientas de trabajo
  - Departamento "Desarrollo": acceso libre excepto redes sociales
  - Horario laboral: sin streaming (YouTube, Netflix, Spotify)
  - Fuera de horario: acceso libre

Implementación:
  Squid con ACLs por IP de origen + listas de dominios + horarios
  Autenticación contra LDAP/AD para identificar al usuario
```

### Caso 2: Ahorro de ancho de banda (forward proxy con caché)

```
Escenario: Oficina remota con enlace de 10 Mbps, 200 usuarios

Sin proxy:
  200 usuarios × descargas repetidas de los mismos JS/CSS/imágenes
  Saturación del enlace

Con Squid cacheando:
  Primera petición: descarga de Internet y cachea
  Siguientes 199: sirve desde caché local

  Objetos cacheables: imágenes, CSS, JS, updates del SO
  Hit ratio típico: 30-50% (significativo en enlaces lentos)
```

### Caso 3: Cumplimiento normativo (forward proxy con logging)

```
Requisitos legales/compliance:
  - Registrar TODA la navegación web de los empleados
  - Retener logs por 12 meses
  - Poder auditar qué usuario accedió a qué sitio y cuándo

Implementación:
  Squid con access_log detallado
  Autenticación LDAP para vincular IP → usuario
  Rotación de logs con logrotate
  Integración con SIEM (Splunk, ELK) para análisis
```

### Caso 4: Publicación segura de aplicaciones (reverse proxy)

```
Escenario: 3 aplicaciones internas que deben ser accesibles desde Internet

Sin reverse proxy:
  app1.empresa.com → 203.0.113.1:8080 (expuesta directamente)
  app2.empresa.com → 203.0.113.2:8080
  app3.empresa.com → 203.0.113.3:8080
  (3 IPs públicas, 3 servidores expuestos, 3 certificados TLS)

Con reverse proxy (Nginx):
  app1.empresa.com ─┐
  app2.empresa.com ──┼──► Nginx (203.0.113.1) ──► backends internos
  app3.empresa.com ─┘     1 IP pública
                          1 punto de TLS
                          1 WAF
                          Backends no expuestos a Internet
```

### Caso 5: Entornos sin acceso directo a Internet

```
Servidores en red aislada (PCI-DSS, clasificada, etc.):

  ┌────────────────────────────────────┐
  │ Red aislada (sin ruta a Internet)  │
  │                                    │
  │ ┌──────────┐    ┌──────────┐       │
  │ │ Server A │    │ Server B │       │    ┌──────────────┐
  │ │          │───►│          │───────┼───►│ Proxy forward│──► Internet
  │ └──────────┘    └──────────┘       │    │ (en DMZ)     │
  │                                    │    └──────────────┘
  └────────────────────────────────────┘

  Los servidores configuran http_proxy/https_proxy
  para salir por el proxy (necesario para yum/apt, pip, etc.)
  El proxy es el ÚNICO punto de salida → auditable y controlable
```

---

## 7. Cadena de proxies y encabezados

### X-Forwarded-For

Cuando hay proxies intermedios, se pierde la IP original del cliente. La cabecera `X-Forwarded-For` (XFF) preserva esta información:

```
Sin XFF:
  Cliente (1.2.3.4) ──► Proxy ──► Servidor
                                  Servidor ve: IP origen = proxy
                                  Pierde la IP del cliente

Con XFF:
  Cliente (1.2.3.4) ──► Proxy ──► Servidor
                         │
                         Añade cabecera:
                         X-Forwarded-For: 1.2.3.4

                         Servidor lee XFF → sabe que el
                         cliente real es 1.2.3.4
```

### Cadena de múltiples proxies

```
Cliente         Proxy 1          Proxy 2          Servidor
1.2.3.4    →    10.0.0.1    →    10.0.0.2    →    Backend

Cabecera XFF en cada salto:
  Proxy 1 añade: X-Forwarded-For: 1.2.3.4
  Proxy 2 añade: X-Forwarded-For: 1.2.3.4, 10.0.0.1

El backend lee: X-Forwarded-For: 1.2.3.4, 10.0.0.1
                                 ^^^^^^^^  ^^^^^^^^^
                                 cliente   primer proxy
```

### Cabeceras relacionadas

| Cabecera | Propósito | Ejemplo |
|---|---|---|
| `X-Forwarded-For` | IP original del cliente | `1.2.3.4, 10.0.0.1` |
| `X-Forwarded-Proto` | Protocolo original | `https` |
| `X-Forwarded-Host` | Host original solicitado | `app.example.com` |
| `X-Real-IP` | IP del cliente (solo la primera, Nginx) | `1.2.3.4` |
| `Forwarded` | Estándar RFC 7239 (reemplazo oficial) | `for=1.2.3.4;proto=https` |
| `Via` | Proxies intermedios (RFC 7230) | `1.1 proxy.example.com` |

> **Seguridad**: `X-Forwarded-For` puede ser falsificado por el cliente. Un atacante puede enviar `X-Forwarded-For: 127.0.0.1` para saltarse restricciones por IP. El servidor debe confiar solo en el XFF insertado por sus propios proxies, no en los valores que ya venían en la petición.

---

## 8. HTTPS y el problema del cifrado

HTTPS cifra el contenido entre el cliente y el servidor. Esto crea desafíos diferentes para cada tipo de proxy.

### Forward proxy con HTTPS: método CONNECT

Cuando un cliente configurado con proxy explícito quiere acceder a un sitio HTTPS, usa el método HTTP `CONNECT`:

```
Cliente                  Forward Proxy              Servidor
   │                         │                         │
   │── CONNECT host:443 ────►│                         │
   │                         │── TCP connect ─────────►│
   │◄── 200 OK ─────────────│                         │
   │                         │                         │
   │◄════════ Túnel TLS cifrado de extremo a extremo ════════►│
   │                         │                         │
   │  El proxy NO ve el      │  Solo ve bytes          │
   │  contenido (cifrado)    │  cifrados pasar         │
   │                         │                         │
```

El proxy puede:
- Ver a qué dominio/IP se conecta el cliente
- Bloquear conexiones por dominio (antes del CONNECT)
- Registrar que alice se conectó a github.com

El proxy NO puede:
- Ver la URL completa (`/repo/file.py`)
- Ver el contenido de la respuesta
- Cachear el contenido

### TLS interception (MITM proxy)

Algunas organizaciones realizan **inspección TLS**: el proxy genera un certificado falso para cada sitio, actuando como man-in-the-middle:

```
Cliente               Proxy (MITM)              Servidor
   │                      │                        │
   │── TLS a proxy ──────►│── TLS real a server ──►│
   │   cert: proxy's CA   │   cert: server's real  │
   │                      │                        │
   │  El cliente confía    │  El proxy descifra,    │
   │  en la CA del proxy   │  inspecciona, y        │
   │  (instalada por IT)   │  re-cifra              │
```

Esto requiere:
1. Instalar la CA del proxy en todos los clientes (GPO, MDM)
2. El proxy genera certificados firmados por esa CA para cada sitio
3. El cliente ve un certificado "válido" (firmado por la CA de la empresa)

```
Implicaciones:
  ✓ Permite inspeccionar TODO el tráfico HTTPS
  ✓ Filtrar malware en descargas HTTPS
  ✓ DLP (Data Loss Prevention) en tráfico cifrado

  ✗ Rompe certificate pinning (algunas apps fallan)
  ✗ Implicaciones legales y de privacidad
  ✗ Punto único de fallo y de compromiso
  ✗ El proxy tiene acceso a TODAS las credenciales
```

### Reverse proxy: terminación TLS normal

Para reverse proxies, la terminación TLS es un patrón estándar y aceptado — el certificado legítimo del dominio está instalado en el proxy:

```
Internet                    Red interna

Cliente ──HTTPS──► Nginx ──HTTP──► Backend
                   │
                   Tiene el certificado real de
                   app.example.com (Let's Encrypt, etc.)

                   Esto NO es MITM: el proxy ES el servidor
                   desde el punto de vista del cliente
```

---

## 9. Errores comunes

### Error 1: Confundir forward con reverse al describir la infraestructura

```
"Necesitamos un proxy para que los empleados naveguen seguros"
  → Forward proxy (Squid)

"Necesitamos un proxy para proteger nuestra API"
  → Reverse proxy (Nginx)

"Necesitamos un proxy para cachear"
  → Depende: ¿cachear las respuestas de Internet para los empleados?
    → Forward proxy
    ¿cachear las respuestas de nuestra API para los clientes?
    → Reverse proxy

Regla: ¿QUIÉN se beneficia del proxy?
  Nuestros usuarios → forward
  Nuestros servidores → reverse
```

### Error 2: Creer que un forward proxy puede inspeccionar HTTPS sin más

```
Escenario: Squid configurado como proxy explícito
Expectativa: "Podemos filtrar URLs HTTPS como /admin/secret"
Realidad: Solo se puede ver el dominio (via CONNECT), no la URL completa

Para inspeccionar HTTPS:
  → Necesitas TLS interception (ssl_bump en Squid)
  → Necesitas instalar la CA del proxy en todos los clientes
  → Necesitas evaluar las implicaciones legales
```

### Error 3: No configurar https_proxy además de http_proxy

```bash
# MAL — solo configura HTTP
export http_proxy=http://proxy.empresa.com:3128
# curl http://example.com  → usa proxy ✓
# curl https://example.com → conexión directa (ignora proxy) ✗

# BIEN — configurar ambos
export http_proxy=http://proxy.empresa.com:3128
export https_proxy=http://proxy.empresa.com:3128
# Nota: el valor de https_proxy es http:// (no https://)
# porque indica CÓMO llegar al proxy, no al destino

# TAMBIÉN — no olvidar no_proxy
export no_proxy=localhost,127.0.0.1,.empresa.com,10.0.0.0/8
# Tráfico local NO debe pasar por el proxy
```

### Error 4: Confiar en X-Forwarded-For sin validación

```
# MAL — confiar ciegamente en XFF
# El cliente malicioso envía:
# X-Forwarded-For: 10.0.0.1  (IP interna falsificada)

# Si la app usa XFF para autorización:
if request.headers['X-Forwarded-For'] in trusted_ips:
    grant_admin()  # ¡Un atacante externo obtiene acceso admin!

# BIEN — el reverse proxy debe:
# 1. Limpiar XFF que viene del cliente
# 2. Insertar el XFF real
# Nginx:
#   proxy_set_header X-Forwarded-For $remote_addr;
#   (sobrescribe, no añade al existente)
```

### Error 5: Usar proxy transparente y esperar filtrar HTTPS por URL

```
Proxy transparente con iptables REDIRECT:
  - Intercepta puerto 80 (HTTP) → puede filtrar URLs ✓
  - Intercepta puerto 443 (HTTPS) → solo ve el SNI (dominio) ✗

  El proxy transparente NO puede hacer CONNECT (el cliente no sabe
  que hay un proxy), por lo que con HTTPS solo puede:
  - Bloquear dominios enteros (basándose en SNI)
  - NO puede filtrar por URL/path
  - NO puede cachear contenido HTTPS
```

---

## 10. Cheatsheet

```
=== TIPOS DE PROXY ===
Forward proxy     cliente → PROXY → Internet     protege clientes
Reverse proxy     Internet → PROXY → backend     protege servidores
Transparente      cliente → [firewall] → PROXY    sin config en cliente

=== FORWARD PROXY ===
Funciones:        filtrado, caché saliente, logging, anonimato
Software:         Squid (3128), Tinyproxy, Privoxy
Config cliente:   http_proxy, https_proxy, no_proxy
Puerto típico:    3128, 8080
HTTPS:            método CONNECT (túnel cifrado, no inspecciona)
TLS inspection:   ssl_bump (MITM, requiere CA en clientes)

=== REVERSE PROXY ===
Funciones:        balanceo, terminación TLS, virtual hosting, WAF, caché
Software:         Nginx, HAProxy, Apache mod_proxy, Traefik, Caddy
Config cliente:   ninguna (DNS apunta al proxy)
Puerto típico:    80, 443
TLS:              terminación normal (certificado legítimo)

=== PROXY TRANSPARENTE ===
Requisito:        iptables REDIRECT o DNAT en router/firewall
HTTP:             inspección completa (URL, contenido, caché)
HTTPS:            solo dominio (SNI), NO url ni contenido
Ventaja:          sin configuración en clientes
Desventaja:       limitaciones con HTTPS

=== CABECERAS ===
X-Forwarded-For        IP(s) del cliente original
X-Forwarded-Proto      protocolo original (http/https)
X-Forwarded-Host       host original
X-Real-IP              IP del cliente (Nginx)
Via                    proxies intermedios

=== VARIABLES DE ENTORNO (cliente) ===
http_proxy=http://proxy:3128       proxy para HTTP
https_proxy=http://proxy:3128      proxy para HTTPS (valor es http://)
no_proxy=localhost,.local,10.0.0.0/8   excepciones
HTTP_PROXY, HTTPS_PROXY            variantes en mayúsculas

=== COMPARACIÓN RÁPIDA ===
¿Quién se beneficia?
  Usuarios internos → forward
  Servidores propios → reverse

¿Quién configura el proxy?
  El usuario/admin de red → forward
  El admin del servidor → reverse

¿Quién no sabe del proxy?
  El servidor destino → forward
  El cliente externo → reverse
```

---

## 11. Ejercicios

### Ejercicio 1: Clasificar escenarios

Para cada escenario, indica si necesitas un forward proxy, un reverse proxy, o ambos. Justifica tu respuesta:

1. Una empresa quiere bloquear el acceso a redes sociales durante horario laboral
2. Una startup tiene 3 microservicios y quiere exponerlos todos bajo `api.startup.com` con TLS
3. Un colegio quiere registrar qué sitios visitan los alumnos
4. Un e-commerce quiere distribuir carga entre 5 servidores web
5. Una oficina remota con enlace lento quiere cachear las actualizaciones de Windows/Linux
6. Una empresa quiere que sus servidores de base de datos puedan descargar parches pero sin acceso directo a Internet

> **Pregunta de predicción**: En el escenario 5, si el 80% del tráfico de actualizaciones ya es HTTPS, ¿cuánto beneficio real dará la caché del proxy forward? ¿Por qué? ¿Qué alternativa específica existe para cachear updates de paquetes sin hacer TLS interception?

### Ejercicio 2: Trazar el flujo de una petición

Un empleado en su PC (10.0.0.50) teclea `https://api.github.com/repos/torvalds/linux` en su navegador. La red tiene esta topología:

```
PC (10.0.0.50) ──► Squid (10.0.0.1:3128) ──► Firewall ──► Internet
                   proxy explícito
```

El navegador está configurado con `http_proxy=http://10.0.0.1:3128` y `https_proxy=http://10.0.0.1:3128`.

Describe paso a paso:

1. ¿Qué envía el navegador al proxy? ¿Usa HTTP o HTTPS para hablar con Squid?
2. ¿Qué método HTTP usa para iniciar la conexión HTTPS?
3. ¿Puede Squid ver la URL completa `/repos/torvalds/linux`?
4. ¿Puede Squid cachear la respuesta JSON de la API de GitHub?
5. ¿Qué ve GitHub como IP de origen?
6. Si Squid tiene una ACL que bloquea `github.com`, ¿en qué momento se bloquea la petición?

> **Pregunta de predicción**: Si el empleado cambia su navegador para ignorar el proxy (quitando la configuración), ¿podrá acceder directamente a GitHub? ¿Depende de la configuración del firewall?

### Ejercicio 3: Diseñar la arquitectura proxy

Tu empresa tiene estos requisitos:

- 500 empleados en oficina con acceso a Internet
- 3 aplicaciones web internas accesibles desde Internet
- Los empleados no pueden acceder a sitios de streaming
- Las aplicaciones deben tener alta disponibilidad
- Todo el tráfico web debe quedar registrado (compliance)
- Los servidores del datacenter necesitan acceso a Internet para actualizaciones

Dibuja (en ASCII) la topología de red incluyendo:
1. Qué proxies necesitas (forward, reverse, o ambos)
2. Dónde se ubican (red interna, DMZ, etc.)
3. Qué software usarías para cada uno y por qué
4. Cómo configurarías los clientes (explícito, transparente, GPO)
5. Qué registros (logs) necesitas y dónde los almacenas

> **Pregunta de predicción**: Si un empleado configura una VPN personal en su PC para saltarse el proxy forward, ¿el proxy registrará ese tráfico? ¿Cómo podrías detectar o prevenir esta evasión?

---

> **Siguiente tema**: T02 — Squid básico (instalación, squid.conf, ACLs, caché, autenticación).
