# Configuración TLS en Nginx

## Índice

1. [Activación de TLS en Nginx](#1-activación-de-tls-en-nginx)
2. [Configuración básica de un server block HTTPS](#2-configuración-básica-de-un-server-block-https)
3. [Protocolos y cipher suites](#3-protocolos-y-cipher-suites)
4. [Sesiones TLS y rendimiento](#4-sesiones-tls-y-rendimiento)
5. [HSTS y cabeceras de seguridad](#5-hsts-y-cabeceras-de-seguridad)
6. [OCSP Stapling](#6-ocsp-stapling)
7. [HTTP/2 sobre TLS](#7-http2-sobre-tls)
8. [Múltiples certificados y SNI](#8-múltiples-certificados-y-sni)
9. [Configuración recomendada para producción](#9-configuración-recomendada-para-producción)
10. [Errores comunes](#10-errores-comunes)
11. [Cheatsheet](#11-cheatsheet)
12. [Ejercicios](#12-ejercicios)

---

## 1. Activación de TLS en Nginx

### 1.1 Requisitos

Nginx necesita estar compilado con el módulo `http_ssl_module`. Los paquetes
de las distribuciones lo incluyen por defecto:

```bash
# Verificar que el módulo está disponible
nginx -V 2>&1 | grep ssl
# --with-http_ssl_module

# Verificar la versión de OpenSSL vinculada
nginx -V 2>&1 | grep -i openssl
# built with OpenSSL 3.0.x
```

No hay un paso de "activar módulo" como en Apache — si Nginx está compilado
con soporte SSL, basta con usar la directiva `ssl` en `listen`.

### 1.2 Diferencia con Apache

| Aspecto                  | Apache                        | Nginx                         |
|--------------------------|-------------------------------|-------------------------------|
| Activación               | `a2enmod ssl` + restart       | Siempre disponible (compilado)|
| Config global SSL        | `ssl.conf` separado           | Dentro de `http {}` o snippets|
| Directiva clave          | `SSLEngine On`                | `listen 443 ssl`              |
| Certificado + cadena     | `SSLCertificateFile`          | `ssl_certificate`             |
| Clave privada            | `SSLCertificateKeyFile`       | `ssl_certificate_key`         |

---

## 2. Configuración básica de un server block HTTPS

### 2.1 Estructura mínima

```nginx
server {
    listen 443 ssl;
    listen [::]:443 ssl;          # IPv6
    server_name ejemplo.com;

    ssl_certificate     /etc/letsencrypt/live/ejemplo.com/fullchain.pem;
    ssl_certificate_key /etc/letsencrypt/live/ejemplo.com/privkey.pem;

    root /var/www/ejemplo/public;
    index index.html;

    location / {
        try_files $uri $uri/ =404;
    }
}
```

La palabra `ssl` en `listen` activa TLS para ese puerto. No existe una
directiva `SSLEngine` separada como en Apache.

### 2.2 Redirección HTTP → HTTPS

```nginx
# Server block HTTP — solo redirige
server {
    listen 80;
    listen [::]:80;
    server_name ejemplo.com www.ejemplo.com;

    return 301 https://ejemplo.com$request_uri;
}

# Server block HTTPS — sirve contenido
server {
    listen 443 ssl;
    listen [::]:443 ssl;
    server_name ejemplo.com;

    ssl_certificate     /etc/letsencrypt/live/ejemplo.com/fullchain.pem;
    ssl_certificate_key /etc/letsencrypt/live/ejemplo.com/privkey.pem;

    root /var/www/ejemplo/public;
    # ...
}
```

> **`return 301` vs `rewrite`**: Para redirección simple HTTP→HTTPS, `return`
> es más eficiente que `rewrite` — no requiere evaluar expresiones regulares
> y su intención es más clara.

### 2.3 ssl_certificate — fullchain obligatorio

Nginx requiere que `ssl_certificate` contenga el certificado del servidor **y**
los certificados intermedios concatenados (fullchain). No tiene una directiva
separada para la cadena:

```nginx
# CORRECTO — fullchain (cert del servidor + intermedias)
ssl_certificate /etc/ssl/certs/fullchain.pem;

# INCORRECTO — solo el certificado del servidor
# Funcionará en algunos navegadores pero fallará en curl, apps móviles, etc.
ssl_certificate /etc/ssl/certs/cert.pem;
```

---

## 3. Protocolos y cipher suites

### 3.1 Protocolos TLS

```nginx
# Solo TLS 1.2 y 1.3 (recomendación actual)
ssl_protocols TLSv1.2 TLSv1.3;
```

A diferencia de Apache (que usa `-all +TLSv1.2`), Nginx solo lista los
protocolos habilitados, sin necesidad de negar primero.

### 3.2 Cipher suites

```nginx
# Cipher suites para TLS 1.2 (TLS 1.3 tiene sus propios, no configurables aquí)
ssl_ciphers ECDHE-ECDSA-AES128-GCM-SHA256:ECDHE-RSA-AES128-GCM-SHA256:ECDHE-ECDSA-AES256-GCM-SHA384:ECDHE-RSA-AES256-GCM-SHA384:ECDHE-ECDSA-CHACHA20-POLY1305:ECDHE-RSA-CHACHA20-POLY1305:DHE-RSA-AES128-GCM-SHA256:DHE-RSA-AES256-GCM-SHA384:DHE-RSA-CHACHA20-POLY1305;

# Dejar que el servidor elija el cipher
ssl_prefer_server_ciphers off;
```

### 3.3 ssl_prefer_server_ciphers — cuándo activar

| Valor  | Comportamiento                    | Cuándo usar                      |
|--------|-----------------------------------|----------------------------------|
| `on`   | El servidor impone su preferencia | Si la lista incluye ciphers débiles junto con fuertes |
| `off`  | El cliente elige su preferido     | Si todos los ciphers listados son seguros (recomendado por Mozilla Intermediate) |

Con el perfil Intermediate, todos los ciphers listados son seguros. Dejar
que el cliente elija permite que dispositivos móviles prefieran CHACHA20
(mejor sin aceleración AES por hardware) mientras que servidores con AES-NI
prefieran AES-GCM.

### 3.4 Curvas ECDH

```nginx
ssl_ecdh_curves X25519:secp256r1:secp384r1;
```

**X25519** es la curva más rápida y la preferida por defecto en TLS 1.3.
`secp256r1` (P-256) es la más compatible. `secp384r1` (P-384) ofrece seguridad
extra con más coste computacional.

### 3.5 Parámetros DH personalizados

Para cipher suites DHE en TLS 1.2:

```bash
openssl dhparam -out /etc/nginx/dhparam.pem 2048
```

```nginx
ssl_dhparam /etc/nginx/dhparam.pem;
```

Sin esta directiva, Nginx usa los parámetros DH de OpenSSL por defecto, que
son seguros en versiones recientes. Con TLS 1.3, DHE usa parámetros predefinidos
y `ssl_dhparam` es irrelevante.

### 3.6 Verificar configuración TLS

```bash
# Protocolos y cipher negociados
echo | openssl s_client -connect ejemplo.com:443 \
    -servername ejemplo.com 2>/dev/null | \
    grep -E "Protocol|Cipher"
# Protocol  : TLSv1.3
# Cipher    : TLS_AES_256_GCM_SHA384

# Intentar TLS 1.1 (debe fallar)
openssl s_client -connect ejemplo.com:443 \
    -servername ejemplo.com -tls1_1 2>&1 | head -5

# Listar todos los ciphers aceptados
nmap --script ssl-enum-ciphers -p 443 ejemplo.com
```

---

## 4. Sesiones TLS y rendimiento

### 4.1 El coste del handshake

Un handshake TLS completo requiere 2 round-trips (TLS 1.2) o 1 round-trip
(TLS 1.3), con operaciones criptográficas costosas. Reutilizar sesiones
evita repetir este proceso:

```
Handshake completo (TLS 1.2):        Sesión reutilizada:
  Cliente ──► ServerHello              Cliente ──► SessionTicket
  Cliente ◄── Certificado              Cliente ◄── Aceptado
  Cliente ──► KeyExchange              (listo para datos)
  Cliente ◄── Finished
  ~100-300ms                           ~50ms
```

### 4.2 Session cache (shared)

Almacena sesiones en memoria compartida entre workers:

```nginx
# Caché compartida de 10 MB (~40,000 sesiones)
ssl_session_cache shared:SSL:10m;

# Tiempo de vida de la sesión en caché
ssl_session_timeout 1d;     # 1 día (defecto: 5 minutos)
```

**Cálculo del tamaño**: 1 MB ≈ 4,000 sesiones. Para un servidor con 10,000
visitantes concurrentes, 10 MB es más que suficiente.

Tipos de caché disponibles:

| Tipo         | Sintaxis                   | Compartido entre workers | Uso              |
|--------------|----------------------------|--------------------------|------------------|
| `off`        | `ssl_session_cache off`    | —                        | Deshabilitar     |
| `none`       | `ssl_session_cache none`   | No                       | Por defecto      |
| `builtin`    | `ssl_session_cache builtin:1000` | No (por worker)   | No recomendado   |
| `shared`     | `ssl_session_cache shared:NAME:SIZE` | Sí              | **Recomendado**  |

### 4.3 Session tickets

Los session tickets permiten reutilización sin estado en el servidor. El
servidor cifra la sesión y se la envía al cliente como "ticket". En la
reconexión, el cliente presenta el ticket:

```nginx
# Desactivar session tickets (recomendación Mozilla)
ssl_session_tickets off;
```

**¿Por qué desactivar?** Los session tickets usan una clave simétrica que
Nginx genera al arrancar y no rota automáticamente. Si esa clave se
compromete, un atacante que capturó tráfico puede descifrar todas las sesiones
que usaron tickets — perdiendo Forward Secrecy.

Si se necesitan tickets (por ejemplo, con muchos servidores sin session cache
compartida), rotar la clave manualmente:

```nginx
# Rotar ticket keys (archivo de 80 bytes)
ssl_session_ticket_key /etc/nginx/tickets/current.key;
ssl_session_ticket_key /etc/nginx/tickets/previous.key;
```

```bash
# Generar nueva clave
openssl rand 80 > /etc/nginx/tickets/current.key
chmod 600 /etc/nginx/tickets/current.key

# Rotar periódicamente (ej. cada 12 horas via cron)
```

### 4.4 0-RTT (TLS 1.3 Early Data)

TLS 1.3 permite enviar datos en el primer paquete de la reconexión (0 round
trips), pero con riesgo de replay attacks:

```nginx
# Activar 0-RTT (con precaución)
ssl_early_data on;

# Informar al backend para que pueda detectar peticiones replay
proxy_set_header Early-Data $ssl_early_data;
```

> **Seguridad**: 0-RTT es vulnerable a ataques de replay. Solo es seguro
> para operaciones idempotentes (GET). Los backends deben verificar la
> cabecera `Early-Data` y rechazar operaciones no idempotentes (POST, PUT,
> DELETE) que lleguen como early data.

---

## 5. HSTS y cabeceras de seguridad

### 5.1 HSTS en Nginx

```nginx
server {
    listen 443 ssl;
    server_name ejemplo.com;

    # HSTS: 2 años, incluir subdominios
    add_header Strict-Transport-Security "max-age=63072000; includeSubDomains" always;

    # ...
}
```

El parámetro `always` es importante: sin él, `add_header` solo se aplica a
respuestas con código 2xx/3xx. Con `always`, se aplica también a 4xx/5xx,
garantizando que la política HSTS se transmita incluso en páginas de error.

### 5.2 Cabeceras de seguridad completas

```nginx
server {
    listen 443 ssl;
    server_name ejemplo.com;

    # TLS
    ssl_certificate     /path/fullchain.pem;
    ssl_certificate_key /path/privkey.pem;

    # Cabeceras de seguridad
    add_header Strict-Transport-Security "max-age=63072000; includeSubDomains" always;
    add_header X-Content-Type-Options "nosniff" always;
    add_header X-Frame-Options "SAMEORIGIN" always;
    add_header Referrer-Policy "strict-origin-when-cross-origin" always;
    add_header Permissions-Policy "camera=(), microphone=(), geolocation=()" always;

    # Ocultar información del servidor
    server_tokens off;

    # ...
}
```

### 5.3 Recordatorio: herencia de add_header

Todas estas cabeceras se perderán si una `location` define su propio
`add_header`. Usar snippets:

```nginx
# /etc/nginx/snippets/security-headers.conf
add_header Strict-Transport-Security "max-age=63072000; includeSubDomains" always;
add_header X-Content-Type-Options "nosniff" always;
add_header X-Frame-Options "SAMEORIGIN" always;
add_header Referrer-Policy "strict-origin-when-cross-origin" always;
```

```nginx
server {
    include snippets/security-headers.conf;

    location /api/ {
        include snippets/security-headers.conf;      # repetir
        add_header X-API-Version "2" always;
        proxy_pass http://backend;
    }
}
```

---

## 6. OCSP Stapling

### 6.1 Configuración en Nginx

```nginx
server {
    listen 443 ssl;
    server_name ejemplo.com;

    ssl_certificate     /etc/letsencrypt/live/ejemplo.com/fullchain.pem;
    ssl_certificate_key /etc/letsencrypt/live/ejemplo.com/privkey.pem;

    # OCSP Stapling
    ssl_stapling on;
    ssl_stapling_verify on;

    # CA para verificar la respuesta OCSP
    # Con Let's Encrypt fullchain, Nginx extrae la CA automáticamente
    # Si no, especificar:
    ssl_trusted_certificate /etc/letsencrypt/live/ejemplo.com/chain.pem;

    # Resolver DNS para consultas OCSP
    resolver 1.1.1.1 8.8.8.8 valid=300s;
    resolver_timeout 5s;

    # ...
}
```

### 6.2 Directivas de OCSP

| Directiva                 | Propósito                                           |
|---------------------------|-----------------------------------------------------|
| `ssl_stapling on`         | Activar OCSP Stapling                               |
| `ssl_stapling_verify on`  | Verificar la respuesta OCSP (recomendado)           |
| `ssl_trusted_certificate` | CA para verificar la respuesta OCSP                 |
| `resolver`                | Servidor DNS para resolver el responder OCSP        |
| `resolver_timeout`        | Timeout para la resolución DNS                      |

### 6.3 El requisito del resolver

A diferencia de Apache, Nginx **requiere** un `resolver` explícito para OCSP
Stapling porque necesita resolver el hostname del servidor OCSP de la CA.
Sin `resolver`, el stapling falla silenciosamente:

```nginx
# Usar DNS públicos o el resolver local
resolver 1.1.1.1 8.8.8.8 valid=300s;
# valid=300s: cachear respuestas DNS por 5 minutos
```

### 6.4 Verificar

```bash
# Verificar que OCSP Stapling funciona
echo | openssl s_client -connect ejemplo.com:443 \
    -servername ejemplo.com -status 2>/dev/null | \
    grep -A 5 "OCSP Response"

# Respuesta esperada:
# OCSP Response Status: successful (0x0)
# ...
# Cert Status: good
```

> **Nota**: Tras un restart de Nginx, la primera respuesta OCSP no estará
> disponible inmediatamente. Nginx la obtiene con la primera petición de un
> cliente. Se puede forzar con:
> ```bash
> openssl s_client -connect localhost:443 -servername ejemplo.com \
>     -status < /dev/null > /dev/null 2>&1
> ```

---

## 7. HTTP/2 sobre TLS

### 7.1 Activar HTTP/2

HTTP/2 ofrece multiplexación (múltiples peticiones sobre una sola conexión TCP),
compresión de cabeceras (HPACK) y server push. En la práctica, todos los
navegadores requieren TLS para HTTP/2:

```nginx
server {
    listen 443 ssl http2;
    listen [::]:443 ssl http2;
    server_name ejemplo.com;

    ssl_certificate     /path/fullchain.pem;
    ssl_certificate_key /path/privkey.pem;

    # ...
}
```

> **Nginx 1.25.1+**: La directiva `http2` se mueve al contexto `server`
> (separada de `listen`):
> ```nginx
> server {
>     listen 443 ssl;
>     http2 on;
>     # ...
> }
> ```

### 7.2 Requisitos de HTTP/2

```nginx
# HTTP/2 requiere:
# 1. TLS (en la práctica, todos los navegadores)
# 2. ALPN (Application-Layer Protocol Negotiation)
# 3. Nginx compilado con --with-http_v2_module

# Verificar que el módulo está disponible
nginx -V 2>&1 | grep http_v2
```

### 7.3 Verificar HTTP/2

```bash
# Con curl
curl -vI --http2 https://ejemplo.com 2>&1 | grep -i "http/2"
# < HTTP/2 200

# Con nghttp2
nghttp -v https://ejemplo.com

# Ver protocolo negociado (ALPN)
echo | openssl s_client -connect ejemplo.com:443 \
    -servername ejemplo.com -alpn h2 2>/dev/null | \
    grep "ALPN protocol"
# ALPN protocol: h2
```

### 7.4 Implicaciones para la configuración

HTTP/2 multiplexa peticiones, lo que cambia algunas consideraciones:

```nginx
# Con HTTP/2, una conexión TCP sirve múltiples peticiones
# → el keep-alive es menos relevante (ya multiplexado)
# → la concatenación de archivos (CSS/JS bundles) es menos necesaria
# → el domain sharding es contraproducente

# Configuración relevante para HTTP/2
http2_max_concurrent_streams 128;    # máximo de streams simultáneos
# (Nginx 1.25.1+: sin prefijo http2_ para algunas directivas)
```

---

## 8. Múltiples certificados y SNI

### 8.1 Múltiples server blocks con TLS

Nginx soporta SNI nativamente. Cada server block puede tener su propio
certificado:

```nginx
server {
    listen 443 ssl;
    server_name sitio-a.com;
    ssl_certificate     /etc/ssl/certs/sitio-a-fullchain.pem;
    ssl_certificate_key /etc/ssl/private/sitio-a.key;
    root /var/www/sitio-a;
}

server {
    listen 443 ssl;
    server_name sitio-b.com;
    ssl_certificate     /etc/ssl/certs/sitio-b-fullchain.pem;
    ssl_certificate_key /etc/ssl/private/sitio-b.key;
    root /var/www/sitio-b;
}
```

### 8.2 Default server para HTTPS

```nginx
# Catch-all HTTPS: rechazar conexiones sin SNI válido
server {
    listen 443 ssl default_server;
    server_name _;

    # Necesita un certificado aunque vaya a rechazar
    ssl_certificate     /etc/ssl/certs/dummy.pem;
    ssl_certificate_key /etc/ssl/private/dummy.key;

    return 444;    # cerrar conexión sin respuesta
}
```

> **El certificado dummy**: Nginx requiere un certificado válido
> sintácticamente incluso para el default server que rechaza todo. Se puede
> usar un autofirmado generado para este propósito.

### 8.3 Certificado dual RSA + ECDSA

```nginx
server {
    listen 443 ssl;
    server_name ejemplo.com;

    # ECDSA (preferido — handshake más rápido)
    ssl_certificate     /etc/ssl/certs/ejemplo-ecdsa-fullchain.pem;
    ssl_certificate_key /etc/ssl/private/ejemplo-ecdsa.key;

    # RSA (fallback)
    ssl_certificate     /etc/ssl/certs/ejemplo-rsa-fullchain.pem;
    ssl_certificate_key /etc/ssl/private/ejemplo-rsa.key;

    # ...
}
```

Nginx 1.11.0+ soporta múltiples pares certificado/clave. Selecciona
automáticamente según el cipher negociado con el cliente.

---

## 9. Configuración recomendada para producción

### 9.1 Snippet TLS reutilizable

Crear un snippet con la configuración TLS que se incluye en cada server block:

```nginx
# /etc/nginx/snippets/ssl-params.conf

# Protocolos
ssl_protocols TLSv1.2 TLSv1.3;

# Cipher suites (Mozilla Intermediate)
ssl_ciphers ECDHE-ECDSA-AES128-GCM-SHA256:ECDHE-RSA-AES128-GCM-SHA256:ECDHE-ECDSA-AES256-GCM-SHA384:ECDHE-RSA-AES256-GCM-SHA384:ECDHE-ECDSA-CHACHA20-POLY1305:ECDHE-RSA-CHACHA20-POLY1305:DHE-RSA-AES128-GCM-SHA256:DHE-RSA-AES256-GCM-SHA384:DHE-RSA-CHACHA20-POLY1305;
ssl_prefer_server_ciphers off;

# Curvas ECDH
ssl_ecdh_curves X25519:secp256r1:secp384r1;

# DH params (para DHE en TLS 1.2)
ssl_dhparam /etc/nginx/dhparam.pem;

# Sesiones
ssl_session_cache shared:SSL:10m;
ssl_session_timeout 1d;
ssl_session_tickets off;

# OCSP Stapling
ssl_stapling on;
ssl_stapling_verify on;
resolver 1.1.1.1 8.8.8.8 valid=300s;
resolver_timeout 5s;
```

```nginx
# /etc/nginx/snippets/security-headers.conf

add_header Strict-Transport-Security "max-age=63072000; includeSubDomains" always;
add_header X-Content-Type-Options "nosniff" always;
add_header X-Frame-Options "SAMEORIGIN" always;
add_header Referrer-Policy "strict-origin-when-cross-origin" always;
```

### 9.2 Server block completo

```nginx
# Redirección HTTP → HTTPS
server {
    listen 80;
    listen [::]:80;
    server_name ejemplo.com www.ejemplo.com;
    return 301 https://ejemplo.com$request_uri;
}

# Redirección www → no-www (HTTPS)
server {
    listen 443 ssl http2;
    listen [::]:443 ssl http2;
    server_name www.ejemplo.com;

    ssl_certificate     /etc/letsencrypt/live/ejemplo.com/fullchain.pem;
    ssl_certificate_key /etc/letsencrypt/live/ejemplo.com/privkey.pem;
    include snippets/ssl-params.conf;

    return 301 https://ejemplo.com$request_uri;
}

# Server block principal
server {
    listen 443 ssl http2;
    listen [::]:443 ssl http2;
    server_name ejemplo.com;

    ssl_certificate     /etc/letsencrypt/live/ejemplo.com/fullchain.pem;
    ssl_certificate_key /etc/letsencrypt/live/ejemplo.com/privkey.pem;
    include snippets/ssl-params.conf;
    include snippets/security-headers.conf;

    root /var/www/ejemplo/public;
    index index.html;

    # Logging
    access_log /var/log/nginx/ejemplo-access.log;
    error_log /var/log/nginx/ejemplo-error.log;

    # Estáticos
    location ~* \.(js|css|png|jpg|jpeg|gif|ico|woff2?)$ {
        expires 30d;
        add_header Cache-Control "public";
        include snippets/security-headers.conf;
        access_log off;
    }

    # Contenido
    location / {
        try_files $uri $uri/ =404;
    }
}
```

### 9.3 Generar dhparam

```bash
openssl dhparam -out /etc/nginx/dhparam.pem 2048
chmod 644 /etc/nginx/dhparam.pem
```

### 9.4 Comparación directiva por directiva Apache ↔ Nginx

| Función                    | Apache                          | Nginx                           |
|----------------------------|---------------------------------|---------------------------------|
| Activar TLS                | `SSLEngine On`                  | `listen 443 ssl`                |
| Certificado + cadena       | `SSLCertificateFile`            | `ssl_certificate`               |
| Clave privada              | `SSLCertificateKeyFile`         | `ssl_certificate_key`           |
| Protocolos                 | `SSLProtocol`                   | `ssl_protocols`                 |
| Cipher suites              | `SSLCipherSuite`                | `ssl_ciphers`                   |
| Prioridad del servidor     | `SSLHonorCipherOrder`           | `ssl_prefer_server_ciphers`     |
| Session cache              | `SSLSessionCache shmcb:`        | `ssl_session_cache shared:`     |
| Session timeout            | `SSLSessionCacheTimeout`        | `ssl_session_timeout`           |
| Session tickets            | `SSLSessionTickets`             | `ssl_session_tickets`           |
| OCSP Stapling              | `SSLUseStapling On`             | `ssl_stapling on`               |
| OCSP cache                 | `SSLStaplingCache`              | (automático)                    |
| OCSP CA                    | (automático con fullchain)      | `ssl_trusted_certificate`       |
| DNS para OCSP              | (usa resolver del sistema)      | `resolver` (explícito requerido)|
| DH params                  | `SSLOpenSSLConfCmd DHParameters` | `ssl_dhparam`                   |
| Curvas ECDH                | `SSLOpenSSLConfCmd Curves`      | `ssl_ecdh_curves`               |
| Compresión                 | `SSLCompression Off`            | (off por defecto desde 1.1.6)   |
| HTTP/2                     | `Protocols h2 http/1.1`         | `listen 443 ssl http2`          |
| HSTS                       | `Header always set`             | `add_header ... always`         |

---

## 10. Errores comunes

### Error 1: "no ssl_certificate is defined" para default_server

```
nginx: [warn] no "ssl_certificate" is defined for the "listen ... ssl"
directive in server block
```

**Causa**: Un server block con `listen 443 ssl default_server` no tiene
certificado configurado. Nginx lo requiere incluso para el server default.

**Solución**: Generar un certificado autofirmado dummy:

```bash
openssl req -x509 -nodes -days 3650 -newkey rsa:2048 \
    -keyout /etc/nginx/ssl/dummy.key \
    -out /etc/nginx/ssl/dummy.crt \
    -subj "/CN=_"
```

### Error 2: OCSP Stapling falla silenciosamente

**Causa habitual**: Falta la directiva `resolver` o el DNS no es alcanzable.

**Diagnóstico**:

```bash
# Ver errores de stapling en el log
grep -i "ocsp\|stapling" /var/log/nginx/error.log

# Error típico:
# OCSP responder timed out
# no resolver defined to resolve
```

**Solución**: Añadir `resolver` con DNS públicos accesibles:

```nginx
resolver 1.1.1.1 8.8.8.8 valid=300s;
resolver_timeout 5s;
```

### Error 3: add_header HSTS desaparece en location con proxy

```nginx
server {
    add_header Strict-Transport-Security "max-age=63072000" always;

    location /api/ {
        proxy_pass http://backend;
        # ← HSTS perdido porque no hay add_header aquí
    }
}
```

**Solución**: Usar `include snippets/security-headers.conf` en cada location
que defina sus propios `add_header`, o no definir ningún `add_header` en las
locations para heredar los del server.

### Error 4: Mixed content tras migrar a HTTPS

**Causa**: El HTML carga recursos (imágenes, CSS, JS) por HTTP. Los navegadores
bloquean estos recursos en una página HTTPS.

**Diagnóstico**: Consola del navegador muestra "Mixed Content" warnings.

**Solución**: Usar URLs relativas (`/images/logo.png`) o de protocolo relativo
(`//cdn.ejemplo.com/style.css`) en el código. Configurar el backend para
generar URLs HTTPS. Como parche temporal:

```nginx
# Reemplazar http:// por https:// en las respuestas proxied
# (solo como medida temporal)
sub_filter 'http://ejemplo.com' 'https://ejemplo.com';
sub_filter_once off;
```

### Error 5: HTTP/2 no negocia — cae a HTTP/1.1

**Causa**: Nginx no compilado con `http_v2_module`, o el cliente no soporta
ALPN (versiones antiguas de OpenSSL en el cliente).

**Diagnóstico**:

```bash
# Verificar módulo
nginx -V 2>&1 | grep http_v2

# Verificar ALPN
echo | openssl s_client -connect ejemplo.com:443 \
    -servername ejemplo.com -alpn h2 2>/dev/null | \
    grep "ALPN"
# ALPN protocol: h2     ← funciona
# No ALPN negotiated    ← problema
```

---

## 11. Cheatsheet

```
┌─────────────────────────────────────────────────────────────────┐
│            NGINX TLS — REFERENCIA RÁPIDA                        │
├─────────────────────────────────────────────────────────────────┤
│                                                                 │
│  SERVER BLOCK MÍNIMO HTTPS:                                     │
│  server {                                                       │
│      listen 443 ssl http2;                                      │
│      server_name ejemplo.com;                                   │
│      ssl_certificate     /path/fullchain.pem;                   │
│      ssl_certificate_key /path/privkey.pem;                     │
│      root /var/www/ejemplo;                                     │
│  }                                                              │
│                                                                 │
│  REDIRECCIÓN HTTP→HTTPS:                                        │
│  server {                                                       │
│      listen 80;                                                 │
│      return 301 https://$host$request_uri;                      │
│  }                                                              │
│                                                                 │
│  PROTOCOLOS Y CIPHERS:                                          │
│  ssl_protocols TLSv1.2 TLSv1.3;                                │
│  ssl_ciphers ECDHE-...:DHE-...;                                 │
│  ssl_prefer_server_ciphers off;    (si todos son seguros)       │
│  ssl_ecdh_curves X25519:secp256r1:secp384r1;                   │
│  ssl_dhparam /etc/nginx/dhparam.pem;                            │
│                                                                 │
│  SESIONES:                                                      │
│  ssl_session_cache shared:SSL:10m;                              │
│  ssl_session_timeout 1d;                                        │
│  ssl_session_tickets off;           (Forward Secrecy)           │
│                                                                 │
│  OCSP STAPLING:                                                 │
│  ssl_stapling on;                                               │
│  ssl_stapling_verify on;                                        │
│  ssl_trusted_certificate /path/chain.pem;                       │
│  resolver 1.1.1.1 8.8.8.8 valid=300s;  (¡obligatorio!)         │
│                                                                 │
│  HTTP/2:                                                        │
│  listen 443 ssl http2;              (Nginx < 1.25.1)            │
│  http2 on;                          (Nginx 1.25.1+)             │
│                                                                 │
│  HSTS:                                                          │
│  add_header Strict-Transport-Security \                         │
│      "max-age=63072000; includeSubDomains" always;              │
│  (always = también en errores 4xx/5xx)                          │
│                                                                 │
│  SNIPPETS (evitar repetición):                                  │
│  include snippets/ssl-params.conf;                              │
│  include snippets/security-headers.conf;                        │
│                                                                 │
│  DIFERENCIA CLAVE VS APACHE:                                    │
│  • No hay SSLEngine On → es listen 443 ssl                      │
│  • ssl_certificate DEBE ser fullchain                           │
│  • OCSP requiere resolver explícito                             │
│  • Compresión TLS off por defecto (no hace falta desactivar)    │
│  • add_header no se hereda → usar include snippets              │
│                                                                 │
│  VERIFICAR:                                                     │
│  nginx -t                                   sintaxis            │
│  curl -vI --http2 https://host              protocolo           │
│  openssl s_client -connect host:443 -status OCSP                │
│  nmap --script ssl-enum-ciphers -p 443 host ciphers             │
│                                                                 │
└─────────────────────────────────────────────────────────────────┘
```

---

## 12. Ejercicios

### Ejercicio 1: Server block HTTPS con snippet reutilizable

1. Genera un certificado autofirmado con SAN para `tls-nginx.local`.
2. Genera `dhparam.pem` de 2048 bits.
3. Crea el snippet `/etc/nginx/snippets/ssl-params.conf` con:
   - `ssl_protocols TLSv1.2 TLSv1.3`
   - Cipher suites del perfil Intermediate
   - `ssl_session_cache shared:SSL:10m`
   - `ssl_session_tickets off`
   - `ssl_dhparam`
4. Crea el snippet `security-headers.conf` con HSTS y cabeceras de seguridad.
5. Crea un server block HTTP que redirija a HTTPS con `return 301`.
6. Crea el server block HTTPS incluyendo ambos snippets.
7. Verifica con `nginx -t` y recarga.
8. Prueba con `curl -vk https://tls-nginx.local/` y verifica:
   - Protocolo TLS negociado
   - Cipher suite
   - Cabecera HSTS en la respuesta

**Pregunta de predicción**: Si pones `ssl_session_cache shared:SSL:10m` dentro
del server block en vez de en el snippet (que se incluye en cada server block),
¿compartirán la caché de sesiones dos server blocks diferentes? ¿Y si la
directiva está en el contexto `http {}`?

### Ejercicio 2: HTTP/2 y verificación de protocolos

1. Activa HTTP/2 en tu server block HTTPS.
2. Verifica con curl:
   ```bash
   curl -vkI --http2 https://tls-nginx.local/ 2>&1 | grep "HTTP/"
   ```
3. Verifica ALPN:
   ```bash
   echo | openssl s_client -connect tls-nginx.local:443 \
       -servername tls-nginx.local -alpn h2 2>/dev/null | grep ALPN
   ```
4. Intenta conectar forzando TLS 1.1:
   ```bash
   openssl s_client -connect tls-nginx.local:443 -tls1_1
   ```
   ¿Se conecta o falla?
5. Intenta forzar un cipher CBC (sin GCM):
   ```bash
   openssl s_client -connect tls-nginx.local:443 \
       -cipher AES128-SHA256 2>&1 | head -5
   ```
6. Verifica la reutilización de sesiones:
   ```bash
   openssl s_client -connect tls-nginx.local:443 \
       -servername tls-nginx.local -reconnect 2>&1 | grep -i reuse
   ```

**Pregunta de predicción**: Con HTTP/2, ¿cuántas conexiones TCP abrirá el
navegador para cargar una página con 20 recursos (HTML + CSS + JS + imágenes)?
¿Cuántas abriría con HTTP/1.1? ¿Cómo afecta esto al handshake TLS?

### Ejercicio 3: Comparación Apache vs Nginx TLS

1. Configura el mismo sitio con HTTPS en Apache (:8443) y Nginx (:443):
   - Mismo certificado y clave
   - Mismo perfil Intermediate de Mozilla
   - HSTS habilitado en ambos
2. Verifica con `openssl s_client` que ambos negocian los mismos protocolos
   y ciphers.
3. Verifica HSTS en ambos con `curl -sI`.
4. Compara la configuración necesaria (líneas de código) para lograr el
   mismo resultado.
5. Identifica las diferencias funcionales:
   - ¿Cuál requiere `resolver` explícito para OCSP?
   - ¿Cuál necesita activar el módulo SSL separadamente?
   - ¿Cuál necesita `always` en la cabecera HSTS y por qué?
6. Ejecuta un benchmark TLS:
   ```bash
   openssl s_time -connect localhost:443 -new -time 10
   openssl s_time -connect localhost:8443 -new -time 10
   ```
   Compara handshakes/segundo.

**Pregunta de reflexión**: Nginx requiere un `resolver` explícito para OCSP
mientras que Apache usa el resolver del sistema operativo. ¿Qué ventaja tiene
el enfoque de Nginx (especificar DNS explícitamente)? Piensa en escenarios
donde `/etc/resolv.conf` apunta a un DNS interno que no puede resolver hosts
de internet como `ocsp.letsencrypt.org`.

---

> **Siguiente tema**: T04 — Protocolos web (HTTP/1.1, HTTP/2, WebSocket —
> conceptos clave para WebAssembly).
