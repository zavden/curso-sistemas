# Configuración de clientes proxy

## Índice

1. [Variables de entorno: http_proxy y https_proxy](#1-variables-de-entorno-http_proxy-y-https_proxy)
2. [Configuración persistente por sistema](#2-configuración-persistente-por-sistema)
3. [Configuración por herramienta](#3-configuración-por-herramienta)
4. [no_proxy: excepciones](#4-no_proxy-excepciones)
5. [Proxy en navegadores](#5-proxy-en-navegadores)
6. [PAC y WPAD: autoconfiguración](#6-pac-y-wpad-autoconfiguración)
7. [Proxy transparente](#7-proxy-transparente)
8. [Diagnóstico del cliente](#8-diagnóstico-del-cliente)
9. [Errores comunes](#9-errores-comunes)
10. [Cheatsheet](#10-cheatsheet)
11. [Ejercicios](#11-ejercicios)

---

## 1. Variables de entorno: http_proxy y https_proxy

La forma estándar de configurar un proxy en Linux es mediante variables de entorno. La mayoría de herramientas de línea de comandos las respetan automáticamente.

### Las variables

```bash
# Proxy para tráfico HTTP
export http_proxy=http://proxy.empresa.local:3128

# Proxy para tráfico HTTPS
export https_proxy=http://proxy.empresa.local:3128
#                  ^^^^
# NOTA: el valor es http:// (no https://)
# Indica CÓMO conectar al proxy, no al destino
# El cliente habla HTTP con el proxy y pide un CONNECT al destino HTTPS

# Proxy para tráfico FTP
export ftp_proxy=http://proxy.empresa.local:3128

# Excepciones (no usar proxy para estos destinos)
export no_proxy=localhost,127.0.0.1,.empresa.local,10.0.0.0/8
```

### Mayúsculas vs minúsculas

```bash
# Ambas variantes existen y generan confusión:
http_proxy    # minúsculas — la más universal
HTTP_PROXY    # mayúsculas — algunas herramientas solo leen esta

https_proxy   # minúsculas
HTTPS_PROXY   # mayúsculas

no_proxy      # minúsculas
NO_PROXY      # mayúsculas
```

| Herramienta | Minúsculas | Mayúsculas | Notas |
|---|---|---|---|
| `curl` | Sí | Sí | Prefiere minúsculas |
| `wget` | Sí | Sí | También lee `~/.wgetrc` |
| `apt` | No | No | Usa su propia config (`/etc/apt/apt.conf.d/`) |
| `dnf/yum` | Sí | Sí | También lee `/etc/dnf/dnf.conf` |
| `pip` | Sí | Sí | También acepta `--proxy` |
| `git` | Sí | Sí | También lee `git config http.proxy` |
| `docker` | Sí | Sí | Requiere config específica en systemd |
| `npm` | No | No | Usa `npm config set proxy` |

> **Buena práctica**: Definir ambas variantes (mayúsculas y minúsculas) para máxima compatibilidad.

### Con autenticación

```bash
# Proxy que requiere usuario y contraseña
export http_proxy=http://usuario:contraseña@proxy.empresa.local:3128
export https_proxy=http://usuario:contraseña@proxy.empresa.local:3128

# Si la contraseña tiene caracteres especiales, usar URL encoding:
# @  → %40
# :  → %3A
# !  → %21
# #  → %23
# Ejemplo: contraseña "P@ss:w0rd!" →
export http_proxy=http://alice:P%40ss%3Aw0rd%21@proxy.empresa.local:3128
```

> **Seguridad**: Las variables de entorno son visibles con `env`, `printenv`, y en `/proc/PID/environ`. Cualquier usuario del sistema puede ver la contraseña del proxy. Para entornos multi-usuario, preferir autenticación Kerberos (SSO) o archivos de configuración con permisos restrictivos.

### Probar que funciona

```bash
# Establecer variable y probar
export http_proxy=http://proxy.empresa.local:3128
export https_proxy=http://proxy.empresa.local:3128

# Probar HTTP
curl -v http://example.com 2>&1 | grep -i proxy
# * Uses proxy env variable http_proxy == 'http://proxy.empresa.local:3128'

# Probar HTTPS
curl -v https://example.com 2>&1 | grep -i "CONNECT\|proxy"
# * Uses proxy env variable https_proxy == 'http://proxy.empresa.local:3128'
# > CONNECT example.com:443 HTTP/1.1

# Sin seguir la variable (bypass para debug)
curl --noproxy '*' https://example.com
```

---

## 2. Configuración persistente por sistema

Las variables en el shell solo duran la sesión. Para hacer la configuración permanente, hay varios puntos de inyección.

### Para todos los usuarios del sistema

```bash
# /etc/environment — cargado por PAM para todos los logins
# Formato: VARIABLE=valor (sin export)
http_proxy=http://proxy.empresa.local:3128
https_proxy=http://proxy.empresa.local:3128
no_proxy=localhost,127.0.0.1,.empresa.local,10.0.0.0/8
```

```bash
# /etc/profile.d/proxy.sh — ejecutado por shells de login
# Formato: shell script normal
export http_proxy=http://proxy.empresa.local:3128
export https_proxy=http://proxy.empresa.local:3128
export HTTP_PROXY=$http_proxy
export HTTPS_PROXY=$https_proxy
export no_proxy=localhost,127.0.0.1,.empresa.local,10.0.0.0/8
export NO_PROXY=$no_proxy
```

### Diferencias entre los métodos

```
/etc/environment
  ├── Cargado por: pam_env (todos los logins)
  ├── Formato: VARIABLE=valor (sin export, sin expansión)
  ├── Afecta a: sesiones de login, cron (si PAM configurado), servicios
  └── Limitación: no soporta $VAR ni comandos

/etc/profile.d/proxy.sh
  ├── Cargado por: bash/zsh login shells (interactive login)
  ├── Formato: shell script (export, expansión, condicionales)
  ├── Afecta a: terminales interactivas de login
  └── Limitación: no afecta a servicios systemd ni cron por defecto
```

### Para un usuario específico

```bash
# ~/.bashrc o ~/.bash_profile
export http_proxy=http://proxy.empresa.local:3128
export https_proxy=http://proxy.empresa.local:3128
export no_proxy=localhost,127.0.0.1,.empresa.local
```

### Para servicios systemd

Los servicios systemd no heredan las variables de entorno del usuario ni de `/etc/profile.d/`. Requieren configuración específica:

```bash
# Método 1: drop-in para un servicio específico
systemctl edit docker
# Crea /etc/systemd/system/docker.service.d/override.conf

[Service]
Environment="HTTP_PROXY=http://proxy.empresa.local:3128"
Environment="HTTPS_PROXY=http://proxy.empresa.local:3128"
Environment="NO_PROXY=localhost,127.0.0.1,.empresa.local"
```

```bash
# Método 2: configuración global para TODOS los servicios
# /etc/systemd/system.conf.d/proxy.conf
[Manager]
DefaultEnvironment="HTTP_PROXY=http://proxy.empresa.local:3128" "HTTPS_PROXY=http://proxy.empresa.local:3128" "NO_PROXY=localhost,127.0.0.1"
```

```bash
# Recargar después de cambios
systemctl daemon-reload
systemctl restart docker
```

---

## 3. Configuración por herramienta

Algunas herramientas tienen su propia configuración de proxy, independiente de las variables de entorno.

### apt (Debian/Ubuntu)

```bash
# /etc/apt/apt.conf.d/80proxy
Acquire::http::Proxy "http://proxy.empresa.local:3128";
Acquire::https::Proxy "http://proxy.empresa.local:3128";

# Con autenticación
Acquire::http::Proxy "http://usuario:pass@proxy.empresa.local:3128";

# Excepciones (acceso directo para repos internos)
Acquire::http::Proxy::mirror.empresa.local "DIRECT";
```

> **apt ignora las variables de entorno** `http_proxy`/`https_proxy` en muchas versiones. Siempre configurar vía `apt.conf.d/`.

### dnf/yum (RHEL/Fedora)

```bash
# /etc/dnf/dnf.conf
[main]
proxy=http://proxy.empresa.local:3128
proxy_username=usuario
proxy_password=contraseña

# Solo para un repositorio específico:
# /etc/yum.repos.d/internal.repo
[internal]
name=Internal Repo
baseurl=http://mirror.empresa.local/repo
proxy=_none_
# ^^^ _none_ = acceso directo (no usar proxy para este repo)
```

### git

```bash
# Global (para todos los repos)
git config --global http.proxy http://proxy.empresa.local:3128
git config --global https.proxy http://proxy.empresa.local:3128

# Solo para un dominio específico
git config --global http.https://github.com/.proxy http://proxy.empresa.local:3128

# Solo para el repo actual
git config http.proxy http://proxy.empresa.local:3128

# Desactivar proxy para repos internos
git config --global http.https://gitlab.empresa.local/.proxy ""

# Ver configuración actual
git config --global --get http.proxy

# Eliminar configuración de proxy
git config --global --unset http.proxy
```

### pip (Python)

```bash
# Vía variable de entorno (respeta http_proxy/https_proxy)
pip install requests

# Vía línea de comandos
pip install --proxy http://proxy.empresa.local:3128 requests

# Configuración persistente: ~/.config/pip/pip.conf
[global]
proxy = http://proxy.empresa.local:3128

# O con variable de entorno específica de pip
export PIP_PROXY=http://proxy.empresa.local:3128
```

### Docker

```bash
# Para el daemon Docker (descargar imágenes)
# /etc/systemd/system/docker.service.d/http-proxy.conf
[Service]
Environment="HTTP_PROXY=http://proxy.empresa.local:3128"
Environment="HTTPS_PROXY=http://proxy.empresa.local:3128"
Environment="NO_PROXY=localhost,127.0.0.1,.empresa.local,docker-registry.empresa.local"

systemctl daemon-reload
systemctl restart docker

# Para contenedores (builds y runtime)
# ~/.docker/config.json
{
    "proxies": {
        "default": {
            "httpProxy": "http://proxy.empresa.local:3128",
            "httpsProxy": "http://proxy.empresa.local:3128",
            "noProxy": "localhost,127.0.0.1,.empresa.local"
        }
    }
}
# Docker inyecta estas variables automáticamente en los contenedores
```

### wget

```bash
# Respeta http_proxy/https_proxy por defecto

# Configuración persistente: ~/.wgetrc o /etc/wgetrc
http_proxy = http://proxy.empresa.local:3128
https_proxy = http://proxy.empresa.local:3128
use_proxy = on

# Bypass proxy
wget --no-proxy http://mirror.empresa.local/file.iso
```

### npm (Node.js)

```bash
# npm tiene su propia configuración (ignora variables de entorno)
npm config set proxy http://proxy.empresa.local:3128
npm config set https-proxy http://proxy.empresa.local:3128

# Verificar
npm config get proxy

# Eliminar
npm config delete proxy
npm config delete https-proxy

# En ~/.npmrc:
# proxy=http://proxy.empresa.local:3128
# https-proxy=http://proxy.empresa.local:3128
```

---

## 4. no_proxy: excepciones

`no_proxy` define destinos que deben conectarse **directamente**, sin pasar por el proxy.

### Sintaxis

```bash
export no_proxy=localhost,127.0.0.1,.empresa.local,10.0.0.0/8,192.168.0.0/16
#               ─────┬── ────┬───── ──────┬────── ─────┬───── ──────┬──────
#                    │       │            │            │            │
#              hostname  IP literal   subdominio    subred       subred
#              exacto    exacta       (con punto)   CIDR         CIDR
```

### Reglas de matching

| Valor | Coincide con | No coincide con |
|---|---|---|
| `localhost` | `localhost` | `localhost.localdomain` |
| `127.0.0.1` | `127.0.0.1` | `127.0.0.2` |
| `.empresa.local` | `*.empresa.local`, `empresa.local` | `noempresa.local` |
| `empresa.local` | `empresa.local` | `sub.empresa.local` (depende de la herramienta) |
| `10.0.0.0/8` | `10.x.x.x` | `172.16.x.x` |
| `*` | Todo (desactiva el proxy) | — |

> **Cuidado**: El soporte de CIDR (`10.0.0.0/8`) en `no_proxy` no es universal. `curl` lo soporta desde la versión 7.86. Herramientas más antiguas solo aceptan IPs individuales y dominios. Para máxima compatibilidad, listar los rangos como dominios o IPs individuales.

### Qué incluir siempre en no_proxy

```bash
export no_proxy=localhost,127.0.0.1,::1,.empresa.local,10.0.0.0/8,169.254.0.0/16
#               ─────────────────────── ──────────────── ──────────── ───────────────
#               loopback (IPv4+IPv6)    servicios        red interna  link-local
#                                       internos                      (metadata cloud)
```

Para entornos cloud:

```bash
# AWS: endpoint de metadata
export no_proxy=localhost,127.0.0.1,169.254.169.254,.amazonaws.com

# Kubernetes: API server y servicios
export no_proxy=localhost,127.0.0.1,10.0.0.0/8,.cluster.local,.svc
```

---

## 5. Proxy en navegadores

### Firefox

Firefox tiene su propia configuración de proxy, independiente del sistema:

```
Menú → Settings → Network Settings → Settings...

Opciones:
  ○ No proxy                          acceso directo
  ○ Auto-detect proxy settings (WPAD) busca wpad.dat vía DHCP/DNS
  ○ Use system proxy settings         lee variables del sistema
  ○ Manual proxy configuration:
      HTTP Proxy: proxy.empresa.local  Port: 3128
      ☑ Also use this proxy for HTTPS
      No proxy for: localhost, 127.0.0.1, .empresa.local
  ○ Automatic proxy configuration URL:
      http://proxy.empresa.local/proxy.pac
```

Configuración vía `about:config`:

```
network.proxy.type           = 1 (manual), 2 (PAC), 4 (WPAD), 5 (system)
network.proxy.http           = proxy.empresa.local
network.proxy.http_port      = 3128
network.proxy.ssl            = proxy.empresa.local
network.proxy.ssl_port       = 3128
network.proxy.no_proxies_on  = localhost, 127.0.0.1, .empresa.local
```

Despliegue empresarial (políticas):

```json
// /etc/firefox/policies/policies.json (Linux)
{
    "policies": {
        "Proxy": {
            "Mode": "manual",
            "HTTPProxy": "proxy.empresa.local:3128",
            "UseHTTPProxyForAllProtocols": true,
            "Passthrough": "localhost, 127.0.0.1, .empresa.local"
        }
    }
}
```

### Chromium/Chrome

Chrome usa la configuración del sistema por defecto en Linux. Para forzar un proxy:

```bash
# Vía línea de comandos
chromium-browser --proxy-server="http://proxy.empresa.local:3128"
chromium-browser --proxy-pac-url="http://proxy.empresa.local/proxy.pac"

# Bypass
chromium-browser --proxy-bypass-list="localhost;127.0.0.1;*.empresa.local"
```

Despliegue empresarial:

```json
// /etc/chromium/policies/managed/proxy.json
{
    "ProxyMode": "fixed_servers",
    "ProxyServer": "proxy.empresa.local:3128",
    "ProxyBypassList": "localhost;127.0.0.1;*.empresa.local"
}
```

### GNOME (configuración del sistema)

```bash
# Via gsettings (afecta a aplicaciones GNOME)
gsettings set org.gnome.system.proxy mode 'manual'
gsettings set org.gnome.system.proxy.http host 'proxy.empresa.local'
gsettings set org.gnome.system.proxy.http port 3128
gsettings set org.gnome.system.proxy.https host 'proxy.empresa.local'
gsettings set org.gnome.system.proxy.https port 3128
gsettings set org.gnome.system.proxy ignore-hosts "['localhost', '127.0.0.1', '*.empresa.local']"

# Desactivar
gsettings set org.gnome.system.proxy mode 'none'
```

---

## 6. PAC y WPAD: autoconfiguración

### PAC — Proxy Auto-Configuration

Un archivo PAC es un script JavaScript que el navegador ejecuta para decidir qué proxy usar para cada URL. Permite lógica condicional que las variables de entorno no ofrecen.

```javascript
// proxy.pac — ejemplo
function FindProxyForURL(url, host) {

    // Conexiones locales: directo
    if (isPlainHostName(host) ||
        shExpMatch(host, "*.empresa.local") ||
        isInNet(host, "10.0.0.0", "255.0.0.0") ||
        host === "localhost" ||
        host === "127.0.0.1") {
        return "DIRECT";
    }

    // Tráfico interno a la DMZ: proxy específico
    if (shExpMatch(host, "*.dmz.empresa.local")) {
        return "PROXY proxy-dmz.empresa.local:3128";
    }

    // Todo lo demás: proxy principal con failover
    return "PROXY proxy1.empresa.local:3128; PROXY proxy2.empresa.local:3128; DIRECT";
    //     ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
    //     Intenta proxy1, si falla intenta proxy2, si ambos fallan va directo
}
```

Funciones disponibles en PAC:

| Función | Descripción | Ejemplo |
|---|---|---|
| `isPlainHostName(host)` | ¿No tiene puntos? | `"intranet"` → true |
| `dnsDomainIs(host, domain)` | ¿Pertenece al dominio? | `dnsDomainIs(h, ".empresa.local")` |
| `shExpMatch(str, pattern)` | Shell wildcard match | `shExpMatch(url, "*.jpg")` |
| `isInNet(host, ip, mask)` | ¿IP en subred? | `isInNet(h, "10.0.0.0", "255.0.0.0")` |
| `myIpAddress()` | IP del cliente | Para decisiones por ubicación |
| `weekdayRange(d1, d2)` | ¿Día de la semana? | `weekdayRange("MON", "FRI")` |
| `dateRange(...)` | ¿Rango de fechas? | `dateRange("JAN", "MAR")` |
| `timeRange(h1, h2)` | ¿Rango horario? | `timeRange(8, 18)` |

Valores de retorno:

```javascript
"DIRECT"                              // conexión directa
"PROXY proxy:3128"                    // usar este proxy HTTP
"SOCKS5 socks:1080"                   // usar proxy SOCKS5
"PROXY a:3128; PROXY b:3128; DIRECT"  // failover (intenta en orden)
```

El archivo PAC se sirve por HTTP con MIME type `application/x-ns-proxy-autoconfig`:

```nginx
# Nginx sirviendo el PAC
server {
    listen 80;
    server_name proxy.empresa.local;

    location /proxy.pac {
        types { }
        default_type application/x-ns-proxy-autoconfig;
        root /var/www/proxy;
    }
}
```

### WPAD — Web Proxy Auto-Discovery

WPAD permite que los clientes **descubran automáticamente** dónde está el archivo PAC, sin configurar nada.

```
Navegador con "Auto-detect proxy settings":

  1. Intenta DHCP option 252:
     → El servidor DHCP responde con URL del PAC
     → http://wpad.empresa.local/wpad.dat

  2. Si DHCP no da respuesta, intenta DNS:
     → Busca el hostname "wpad" en el dominio actual
     → Si el dominio es sub.empresa.local:
        wpad.sub.empresa.local → falla
        wpad.empresa.local → responde
     → Pide http://wpad.empresa.local/wpad.dat

  3. Descarga wpad.dat (que es un archivo PAC)

  4. Ejecuta FindProxyForURL() para cada petición
```

Configurar WPAD requiere:

```bash
# 1. DNS: crear registro A para wpad
wpad    IN  A   10.0.0.5

# 2. DHCP: option 252 (si se usa ISC DHCP)
# dhcpd.conf:
option wpad-url code 252 = text;
option wpad-url "http://wpad.empresa.local/wpad.dat\n";

# 3. Servidor web: servir el archivo
# El archivo se llama wpad.dat (es un archivo PAC renombrado)
cp proxy.pac /var/www/proxy/wpad.dat
# MIME type: application/x-ns-proxy-autoconfig
```

> **Seguridad de WPAD**: WPAD busca `wpad.DOMINIO` ascendiendo por los niveles del dominio. Si `empresa.local` no es un dominio propio, un atacante podría registrar `wpad.local` y servir un PAC malicioso que redirija todo el tráfico a su proxy. Algunos navegadores han limitado WPAD para mitigar esto.

---

## 7. Proxy transparente

Un proxy transparente intercepta el tráfico sin que el cliente configure nada. La redirección se hace en el router/firewall.

### Arquitectura

```
                     Red del cliente                    Internet
                                    ┌─────────────┐
┌────────┐    ┌────────────────┐    │    Squid    │
│ Cliente │───►│ Router/Firewall│───►│  (puerto    │───► Destino
│         │◄───│  (iptables)    │◄───│   3129)    │◄───
└────────┘    └────────────────┘    └─────────────┘
                │
                └── iptables redirige
                    puerto 80 → 3129 (Squid)
```

### Configuración en Squid

```bash
# squid.conf — modo transparente
# Puerto para proxy transparente (interceptación)
http_port 3129 intercept
#              ^^^^^^^^^
# "intercept" indica a Squid que recibirá tráfico redirigido
# NO es lo mismo que el puerto 3128 del proxy explícito
# Puedes tener ambos simultáneamente:
http_port 3128           # proxy explícito (clientes configurados)
http_port 3129 intercept # proxy transparente (tráfico redirigido)
```

### Redirección con iptables

```bash
# Caso 1: Squid en el mismo router/gateway
# Redirigir tráfico HTTP de la red interna al proxy local
iptables -t nat -A PREROUTING \
    -i eth0 \
    -s 10.0.0.0/8 \
    -p tcp --dport 80 \
    -j REDIRECT --to-port 3129
#   ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
# Redirige puerto 80 → 3129 (Squid intercept)
# Solo afecta a tráfico de la red 10.0.0.0/8
# Solo HTTP (puerto 80), NO HTTPS

# Excluir el propio proxy (evitar bucle)
iptables -t nat -A PREROUTING \
    -i eth0 \
    -s 10.0.0.1 \
    -p tcp --dport 80 \
    -j ACCEPT
# ^^^ Esta regla debe ir ANTES de la redirección
```

```bash
# Caso 2: Squid en una máquina separada del gateway
# En el router/gateway:
iptables -t nat -A PREROUTING \
    -i eth0 \
    -s 10.0.0.0/8 \
    -p tcp --dport 80 \
    -j DNAT --to-destination 10.0.0.5:3129
#   ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
# Redirige a la IP del servidor Squid

# También necesitas MASQUERADE o SNAT para el retorno:
iptables -t nat -A POSTROUTING \
    -o eth1 \
    -s 10.0.0.0/8 \
    -d 10.0.0.5 \
    -p tcp --dport 3129 \
    -j MASQUERADE
```

### Hacer las reglas persistentes

```bash
# RHEL
dnf install iptables-services
iptables-save > /etc/sysconfig/iptables
systemctl enable iptables

# Debian
apt install iptables-persistent
netfilter-persistent save
```

### HTTPS transparente: las limitaciones

```
HTTP (puerto 80) transparente:
  iptables redirige → Squid ve la URL completa → puede cachear y filtrar
  ✓ Funciona sin problemas

HTTPS (puerto 443) transparente:
  El tráfico está cifrado con TLS
  ├── Squid NO puede ver la URL (solo el SNI del ClientHello)
  ├── Squid NO puede cachear
  └── Squid PUEDE bloquear por dominio (usando SNI)
```

Para filtrar HTTPS por dominio sin inspección TLS:

```bash
# squid.conf — peek and splice (sin descifrar)
https_port 3130 intercept ssl-bump \
    cert=/etc/squid/ssl/squid-ca.pem \
    key=/etc/squid/ssl/squid-ca.key

acl step1 at_step SslBump1
ssl_bump peek step1
# peek: lee el SNI (dominio) sin descifrar el contenido
ssl_bump splice all
# splice: deja pasar el tráfico cifrado sin modificar

# Ahora se puede filtrar por dominio:
acl blocked_https ssl::server_name .facebook.com .tiktok.com
ssl_bump terminate blocked_https
# terminate: corta la conexión HTTPS a sitios bloqueados

# iptables para redirigir HTTPS:
# iptables -t nat -A PREROUTING -i eth0 -s 10.0.0.0/8 \
#     -p tcp --dport 443 -j REDIRECT --to-port 3130
```

### Comparación: proxy explícito vs transparente

| Aspecto | Explícito | Transparente |
|---|---|---|
| Config en cliente | Sí (`http_proxy`) | No |
| HTTP: filtrar URL | Sí | Sí |
| HTTP: caché | Sí | Sí |
| HTTPS: filtrar URL | Sí (CONNECT) | No (solo dominio/SNI) |
| HTTPS: caché | No (túnel cifrado) | No |
| Autenticación | Sí (407 + Basic/Negotiate) | No (no hay mecanismo estándar) |
| Usuarios que evaden | Pueden quitar la config | No pueden evadirlo (red lo fuerza) |
| Complejidad | Config en cada cliente | Config en router/firewall |

---

## 8. Diagnóstico del cliente

### Verificar qué proxy está configurado

```bash
# Variables de entorno
env | grep -i proxy
# http_proxy=http://proxy.empresa.local:3128
# https_proxy=http://proxy.empresa.local:3128
# no_proxy=localhost,127.0.0.1,.empresa.local

# Para un proceso específico (si se sospecha que tiene config diferente)
cat /proc/$(pgrep -f firefox)/environ | tr '\0' '\n' | grep -i proxy
```

### Probar conectividad al proxy

```bash
# ¿El proxy responde?
nc -zv proxy.empresa.local 3128
# Connection to proxy.empresa.local 3128 port [tcp/*] succeeded!

# ¿Responde HTTP?
curl -I http://proxy.empresa.local:3128/
# Squid devuelve una página de error (400) pero confirma que responde
```

### Probar acceso a través del proxy

```bash
# HTTP vía proxy explícito
curl -v -x http://proxy.empresa.local:3128 http://example.com
# Buscar en la salida:
# * Uses proxy env variable... → usa la variable
# * Connected to proxy.empresa.local (10.0.0.1) port 3128 → conectó al proxy
# < HTTP/1.1 200 OK → respuesta exitosa

# HTTPS vía proxy (CONNECT)
curl -v -x http://proxy.empresa.local:3128 https://example.com
# * CONNECT example.com:443 HTTP/1.1 → método CONNECT
# < HTTP/1.1 200 Connection established → túnel establecido

# Con autenticación
curl -v -x http://proxy.empresa.local:3128 \
    --proxy-user alice:password \
    http://example.com

# Sin usar proxy (bypass para comparar)
curl -v --noproxy '*' http://example.com
```

### Diagnosticar por qué no funciona

```bash
# 1. ¿La variable está definida?
echo $http_proxy
# Si vacío → no está configurada

# 2. ¿Hay typo en la URL del proxy?
curl -x http://prxy.empresa.local:3128 http://example.com
# curl: (5) Could not resolve proxy: prxy.empresa.local

# 3. ¿El proxy está caído?
curl -x http://proxy.empresa.local:3128 http://example.com
# curl: (7) Failed to connect to proxy.empresa.local port 3128

# 4. ¿El proxy deniega la petición?
curl -v -x http://proxy.empresa.local:3128 http://example.com
# < HTTP/1.1 403 Forbidden
# → ACL de Squid deniega (IP no permitida, sitio bloqueado, etc.)

# 5. ¿Falta autenticación?
curl -v -x http://proxy.empresa.local:3128 http://example.com
# < HTTP/1.1 407 Proxy Authentication Required
# → Necesita usuario/contraseña

# 6. ¿no_proxy está excluyendo un destino que sí debe pasar por proxy?
echo $no_proxy
# Si contiene el dominio destino → va directo, sin proxy
```

### Verificar desde el lado del proxy (Squid)

```bash
# En el servidor Squid: ver peticiones en tiempo real
tail -f /var/log/squid/access.log

# Filtrar por un cliente específico
tail -f /var/log/squid/access.log | grep "10.0.0.50"

# Ver denegaciones
tail -f /var/log/squid/access.log | grep "TCP_DENIED"
```

---

## 9. Errores comunes

### Error 1: https_proxy con valor https://

```bash
# MAL — valor con https://
export https_proxy=https://proxy.empresa.local:3128
# El cliente intenta conectar al proxy con TLS
# El proxy no espera TLS en su puerto → error de conexión

# BIEN — valor con http:// (el proxy habla HTTP, el destino es HTTPS)
export https_proxy=http://proxy.empresa.local:3128
# El cliente habla HTTP con el proxy:
#   CONNECT example.com:443 HTTP/1.1
# El proxy establece el túnel TLS al destino
```

La excepción: si el proxy específicamente soporta conexiones TLS del cliente (HTTPS proxy listener), entonces sí se usa `https://`. Pero esto es raro con Squid estándar.

### Error 2: Olvidar no_proxy para servicios internos

```bash
# MAL — no_proxy no definido
export http_proxy=http://proxy.empresa.local:3128

# Resultado: TODAS las conexiones HTTP pasan por el proxy
# Incluyendo:
curl http://10.0.0.5:9200   # Elasticsearch interno → pasa por proxy → falla
curl http://api.empresa.local/v1  # API interna → pasa por proxy → lento/falla

# BIEN — definir no_proxy
export no_proxy=localhost,127.0.0.1,.empresa.local,10.0.0.0/8,172.16.0.0/12,192.168.0.0/16
```

### Error 3: Configurar proxy en variables pero la herramienta las ignora

```bash
# Síntoma: apt no usa el proxy aunque las variables están definidas
echo $http_proxy
# http://proxy.empresa.local:3128

apt update
# Err:1 http://archive.ubuntu.com ... Could not connect

# Causa: apt no lee http_proxy en muchas versiones

# Solución: configurar apt específicamente
echo 'Acquire::http::Proxy "http://proxy.empresa.local:3128";' \
    > /etc/apt/apt.conf.d/80proxy
```

### Error 4: Proxy transparente en bucle

```bash
# MAL — redirigir TODO el tráfico al proxy, incluyendo el del propio proxy
iptables -t nat -A PREROUTING -p tcp --dport 80 -j REDIRECT --to-port 3129
# Si Squid está en la misma máquina, su propio tráfico de salida
# también se redirige → bucle infinito

# BIEN — excluir el tráfico que genera Squid
# Opción 1: excluir por usuario
iptables -t nat -A OUTPUT -m owner --uid-owner squid -j ACCEPT
iptables -t nat -A OUTPUT -p tcp --dport 80 -j REDIRECT --to-port 3129

# Opción 2: excluir la IP del proxy en PREROUTING
iptables -t nat -A PREROUTING -s 10.0.0.1 -j ACCEPT    # IP del proxy
iptables -t nat -A PREROUTING -s 10.0.0.0/8 -p tcp --dport 80 -j REDIRECT --to-port 3129
```

### Error 5: Proxy PAC no se carga por MIME type incorrecto

```bash
# Síntoma: el navegador configurado con URL PAC no detecta el proxy
# El archivo proxy.pac existe y es correcto, pero el navegador lo ignora

# Causa: el servidor web no envía el MIME type correcto
curl -I http://proxy.empresa.local/proxy.pac
# Content-Type: text/plain    ← MAL

# Solución: configurar el MIME type
# Nginx:
location /proxy.pac {
    types { }
    default_type application/x-ns-proxy-autoconfig;
}

# Apache:
AddType application/x-ns-proxy-autoconfig .pac .dat
```

---

## 10. Cheatsheet

```
=== VARIABLES DE ENTORNO ===
http_proxy=http://proxy:3128              proxy HTTP
https_proxy=http://proxy:3128             proxy HTTPS (valor es http://)
ftp_proxy=http://proxy:3128               proxy FTP
no_proxy=localhost,.local,10.0.0.0/8      excepciones
HTTP_PROXY, HTTPS_PROXY, NO_PROXY         variantes mayúsculas (compatibilidad)

=== PERSISTENCIA ===
/etc/environment                          todos los usuarios (PAM, sin export)
/etc/profile.d/proxy.sh                   shells de login (con export)
~/.bashrc                                 usuario específico
systemctl edit SERVICE                    servicios systemd (Environment=)

=== CONFIGURACIÓN POR HERRAMIENTA ===
apt        /etc/apt/apt.conf.d/80proxy         Acquire::http::Proxy "...";
dnf/yum    /etc/dnf/dnf.conf                   proxy=http://...
git        git config --global http.proxy ...   o https.proxy
pip        ~/.config/pip/pip.conf               proxy = http://...
docker     /etc/systemd/system/docker.service.d/  Environment="HTTP_PROXY=..."
npm        npm config set proxy http://...
wget       ~/.wgetrc                            http_proxy = http://...

=== NAVEGADORES ===
Firefox    Settings → Network → Manual/PAC/WPAD/System
Chrome     --proxy-server= o usa config del sistema
GNOME      gsettings org.gnome.system.proxy
Policies   /etc/firefox/policies/policies.json
           /etc/chromium/policies/managed/proxy.json

=== PAC (Proxy Auto-Configuration) ===
function FindProxyForURL(url, host) { ... }
Retornos: "DIRECT", "PROXY host:port", "SOCKS5 host:port"
Funciones: isPlainHostName, dnsDomainIs, shExpMatch, isInNet
MIME type: application/x-ns-proxy-autoconfig
Archivo: .pac o wpad.dat

=== WPAD (Web Proxy Auto-Discovery) ===
1. DHCP option 252 → URL del PAC
2. DNS: wpad.DOMINIO → descarga wpad.dat
Automático si navegador tiene "Auto-detect" habilitado

=== PROXY TRANSPARENTE ===
Squid: http_port 3129 intercept
iptables: -j REDIRECT --to-port 3129     (mismo host)
iptables: -j DNAT --to-destination IP:3129  (otro host)
HTTP: inspección completa (URL, caché)
HTTPS: solo dominio/SNI (peek and splice)

=== DIAGNÓSTICO ===
env | grep -i proxy                     ver variables configuradas
curl -v -x http://proxy:3128 URL        probar vía proxy explícito
curl --noproxy '*' URL                   probar sin proxy (bypass)
nc -zv proxy.empresa.local 3128         probar conectividad al proxy
tail -f /var/log/squid/access.log       ver peticiones en el proxy
```

---

## 11. Ejercicios

### Ejercicio 1: Configurar un servidor completo

Tienes un servidor RHEL que necesita acceso a Internet para:
- `dnf update` (repositorios oficiales)
- `pip install` (paquetes Python)
- `git clone` de repositorios en GitHub
- `docker pull` de imágenes desde Docker Hub

El proxy de la empresa es `http://proxy.corp.local:3128` con autenticación (usuario: `svc-server01`, contraseña: `Pr0xy!S3rv1c3`).

Hay servicios internos que NO deben pasar por el proxy:
- Registry Docker interno: `registry.corp.local`
- GitLab interno: `gitlab.corp.local`
- Red interna: `10.0.0.0/8`

Escribe la configuración completa para:
1. Variables de entorno persistentes para todos los usuarios
2. Configuración específica de dnf
3. Configuración de Docker (daemon + contenedores)
4. Configuración de git
5. Configuración de pip

Para cada punto, indica el archivo exacto y su contenido.

> **Pregunta de predicción**: Si un contenedor Docker ejecuta `curl http://gitlab.corp.local/api/v4/projects`, ¿usará el proxy o irá directo? ¿Depende de dónde configuraste `no_proxy`? ¿La variable `no_proxy` del host se hereda automáticamente dentro del contenedor?

### Ejercicio 2: Escribir un archivo PAC

Escribe un archivo PAC (`proxy.pac`) para esta política:

1. Todo el tráfico a `*.corp.local` va directo (sin proxy)
2. Tráfico a IPs internas (`10.0.0.0/8`, `172.16.0.0/12`) va directo
3. De lunes a viernes de 8:00 a 18:00, usar `proxy1.corp.local:3128`
4. Fuera de horario laboral, usar `proxy2.corp.local:8080`
5. Si el proxy principal falla, intentar el secundario, y si ambos fallan ir directo
6. `localhost` y nombres sin punto siempre van directo

Después de escribirlo, traza mentalmente qué devolvería `FindProxyForURL()` para:
- `http://intranet.corp.local/wiki` un martes a las 10:00
- `https://github.com/torvalds/linux` un sábado a las 22:00
- `http://10.0.0.5:9200/_cat/indices` un lunes a las 14:00

> **Pregunta de predicción**: Si `proxy1.corp.local` está caído un miércoles a las 11:00, ¿cuánto tardará el navegador en hacer failover a `proxy2.corp.local`? ¿Es instantáneo? ¿El usuario notará un retraso?

### Ejercicio 3: Diagnosticar problemas de proxy

Para cada síntoma, identifica la causa más probable y la solución:

1. `curl http://example.com` funciona, pero `apt update` falla con "Could not connect"
2. `curl -x http://proxy:3128 http://example.com` devuelve `407 Proxy Authentication Required`
3. `curl https://internal-api.corp.local/health` falla con timeout, pero `curl --noproxy '*' https://internal-api.corp.local/health` funciona
4. El navegador Firefox navega correctamente con proxy, pero `git clone https://github.com/user/repo` falla con "Could not resolve host"
5. `dnf update` funciona, pero `pip install requests` falla con "ProxyError: Cannot connect to proxy"

> **Pregunta de predicción**: Si defines `http_proxy=http://proxy:3128` pero NO defines `https_proxy`, ¿qué ocurre cuando ejecutas `curl https://example.com`? ¿Usa el proxy de `http_proxy` o va directo? ¿Depende de la versión de curl?

---

> **Fin de la Sección 5 — Proxy Forward.** Fin del Capítulo 5 — Otros Servicios. Fin del Bloque 10 — Servicios de Red.
