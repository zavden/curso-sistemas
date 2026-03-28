# Configuración TLS en Apache

## Índice

1. [mod_ssl — activación y estructura](#1-mod_ssl--activación-y-estructura)
2. [Configuración básica de un VirtualHost HTTPS](#2-configuración-básica-de-un-virtualhost-https)
3. [Cipher suites y protocolos](#3-cipher-suites-y-protocolos)
4. [Directivas avanzadas de mod_ssl](#4-directivas-avanzadas-de-mod_ssl)
5. [HSTS y redirección HTTP → HTTPS](#5-hsts-y-redirección-http--https)
6. [OCSP Stapling](#6-ocsp-stapling)
7. [Múltiples certificados y SNI](#7-múltiples-certificados-y-sni)
8. [Configuración recomendada para producción](#8-configuración-recomendada-para-producción)
9. [Errores comunes](#9-errores-comunes)
10. [Cheatsheet](#10-cheatsheet)
11. [Ejercicios](#11-ejercicios)

---

## 1. mod_ssl — activación y estructura

### 1.1 Instalación y activación

```bash
# Debian/Ubuntu — mod_ssl viene con apache2 pero hay que activarlo
a2enmod ssl
a2enmod headers          # necesario para HSTS
systemctl restart apache2

# RHEL/Fedora — paquete separado
dnf install mod_ssl
# Se activa automáticamente (conf.modules.d/00-ssl.conf)
systemctl restart httpd
```

### 1.2 Archivos instalados por mod_ssl

**Debian/Ubuntu:**

```
/etc/apache2/mods-available/ssl.load       ← LoadModule
/etc/apache2/mods-available/ssl.conf       ← configuración global SSL
/etc/apache2/sites-available/default-ssl.conf  ← VirtualHost HTTPS ejemplo
```

**RHEL/Fedora:**

```
/etc/httpd/conf.modules.d/00-ssl.conf      ← LoadModule
/etc/httpd/conf.d/ssl.conf                 ← configuración global + VHost default
```

### 1.3 Flujo de una conexión TLS en Apache

```
Cliente                         Apache (mod_ssl)
  │                                 │
  │── ClientHello ─────────────────►│
  │   (versión TLS, ciphers,       │
  │    SNI: ejemplo.com)           │
  │                                 │
  │◄── ServerHello ────────────────│
  │    (cipher elegido,            │ ← selecciona VHost por SNI
  │     certificado,               │ ← envía cert + cadena
  │     parámetros DH/ECDH)       │
  │                                 │
  │── Key Exchange ────────────────►│
  │   (premaster secret cifrado    │
  │    con clave pública del cert) │
  │                                 │
  │◄══ Conexión cifrada ══════════►│
  │    (HTTP sobre TLS)            │
```

---

## 2. Configuración básica de un VirtualHost HTTPS

### 2.1 Estructura mínima

```apache
Listen 443

<VirtualHost *:443>
    ServerName ejemplo.com
    DocumentRoot "/var/www/ejemplo/public"

    SSLEngine On
    SSLCertificateFile    /etc/letsencrypt/live/ejemplo.com/fullchain.pem
    SSLCertificateKeyFile /etc/letsencrypt/live/ejemplo.com/privkey.pem

    <Directory "/var/www/ejemplo/public">
        Require all granted
    </Directory>

    ErrorLog ${APACHE_LOG_DIR}/ejemplo-ssl-error.log
    CustomLog ${APACHE_LOG_DIR}/ejemplo-ssl-access.log combined
</VirtualHost>
```

### 2.2 Directivas esenciales

| Directiva                  | Descripción                                       |
|----------------------------|---------------------------------------------------|
| `SSLEngine On`             | Activa TLS para este VirtualHost                  |
| `SSLCertificateFile`       | Ruta al certificado (o fullchain)                 |
| `SSLCertificateKeyFile`    | Ruta a la clave privada                           |
| `SSLCertificateChainFile`  | Ruta a intermedias (innecesario si se usa fullchain)|

### 2.3 Separar certificado y cadena vs fullchain

```apache
# Opción 1: fullchain (recomendado, Apache 2.4.8+)
SSLCertificateFile    /etc/ssl/certs/fullchain.pem
SSLCertificateKeyFile /etc/ssl/private/ejemplo.key

# Opción 2: separados (necesario en Apache < 2.4.8)
SSLCertificateFile      /etc/ssl/certs/ejemplo.crt
SSLCertificateChainFile /etc/ssl/certs/chain.crt
SSLCertificateKeyFile   /etc/ssl/private/ejemplo.key
```

### 2.4 Puerto de escucha

`Listen 443` debe estar declarado. En Debian suele estar en `ports.conf`:

```apache
# /etc/apache2/ports.conf
Listen 80

<IfModule ssl_module>
    Listen 443
</IfModule>
```

En RHEL, `ssl.conf` incluye su propia directiva `Listen 443 https`.

---

## 3. Cipher suites y protocolos

### 3.1 Protocolos TLS

```
TLS 1.0 (1999) ──► OBSOLETO, vulnerabilidades conocidas (BEAST, POODLE)
TLS 1.1 (2006) ──► OBSOLETO, sin soporte en navegadores modernos
TLS 1.2 (2008) ──► SEGURO, ampliamente soportado, mínimo recomendado
TLS 1.3 (2018) ──► ÓPTIMO, handshake más rápido, ciphers más seguros
```

```apache
# Solo permitir TLS 1.2 y 1.3
SSLProtocol -all +TLSv1.2 +TLSv1.3
```

La sintaxis usa `-` para deshabilitar y `+` para habilitar. `-all` deshabilita
todo como base.

### 3.2 Anatomía de una cipher suite (TLS 1.2)

Una cipher suite define los algoritmos para cada fase de la conexión:

```
TLS_ECDHE_RSA_WITH_AES_256_GCM_SHA384
│    │     │        │   │   │    │
│    │     │        │   │   │    └── Hash para PRF
│    │     │        │   │   └── Modo (GCM = AEAD)
│    │     │        │   └── Tamaño de clave simétrica
│    │     │        └── Cifrado simétrico
│    │     └── Autenticación del servidor
│    └── Intercambio de claves (con Forward Secrecy)
└── Protocolo
```

**Componentes:**

| Fase                    | Algoritmo       | Propósito                        |
|-------------------------|-----------------|----------------------------------|
| Key Exchange            | ECDHE, DHE      | Intercambiar clave simétrica     |
| Authentication          | RSA, ECDSA      | Verificar identidad del servidor |
| Bulk Encryption         | AES-128/256-GCM | Cifrar datos                     |
| MAC / AEAD              | GCM, SHA384     | Integridad de datos              |

### 3.3 TLS 1.3 — cipher suites simplificadas

TLS 1.3 separa la negociación de claves de los cipher suites. Solo quedan
ciphers AEAD:

```
TLS 1.3 cipher suites (todos son seguros):
  TLS_AES_256_GCM_SHA384
  TLS_AES_128_GCM_SHA256
  TLS_CHACHA20_POLY1305_SHA256

No incluyen key exchange ni auth en el nombre porque
TLS 1.3 siempre usa (EC)DHE y la autenticación está
determinada por el tipo de certificado.
```

### 3.4 Forward Secrecy (PFS)

Forward Secrecy garantiza que si la clave privada del servidor se compromete en
el futuro, el tráfico capturado previamente no puede ser descifrado:

```
Sin Forward Secrecy (RSA key exchange):
  Clave privada comprometida → todo el tráfico pasado descifrable

Con Forward Secrecy (ECDHE/DHE):
  Cada conexión genera claves efímeras únicas
  Clave privada comprometida → solo autenticación afectada
  Tráfico pasado → indescifrable (claves efímeras ya descartadas)
```

> **Regla**: Siempre usar cipher suites con ECDHE o DHE, nunca RSA para key
> exchange.

### 3.5 Configuración de cipher suites

```apache
# Configuración moderna (solo ciphers con Forward Secrecy + AEAD)
SSLCipherSuite TLSv1.3 TLS_AES_256_GCM_SHA384:TLS_CHACHA20_POLY1305_SHA256:TLS_AES_128_GCM_SHA256
SSLCipherSuite ECDHE+AESGCM:ECDHE+CHACHA20:DHE+AESGCM:DHE+CHACHA20

# El servidor elige el cipher (no el cliente)
SSLHonorCipherOrder On

# Curvas ECDH preferidas
SSLOpenSSLConfCmd Curves X25519:secp256r1:secp384r1
```

**`SSLHonorCipherOrder On`**: El servidor impone su orden de preferencia de
ciphers en vez de aceptar la preferencia del cliente. Esto asegura que se use
el cipher más seguro que ambos soporten.

### 3.6 Perfiles de Mozilla

Mozilla publica tres perfiles de configuración TLS mantenidos y actualizados.
Son la referencia de la industria:

| Perfil          | Compatibilidad     | Protocolos      | Caso de uso               |
|-----------------|--------------------|-----------------|-----------------------------|
| **Modern**      | Navegadores recientes | TLS 1.3 only | Nuevos despliegues          |
| **Intermediate**| Amplia             | TLS 1.2 + 1.3   | Producción general          |
| **Old**         | Máxima (legacy)    | TLS 1.0+         | Solo si hay clientes legacy |

La herramienta de Mozilla (SSL Configuration Generator) genera configuraciones
completas para Apache, Nginx y otros servidores.

**Perfil Intermediate (el más común para producción):**

```apache
SSLProtocol             -all +TLSv1.2 +TLSv1.3
SSLCipherSuite          ECDHE-ECDSA-AES128-GCM-SHA256:ECDHE-RSA-AES128-GCM-SHA256:ECDHE-ECDSA-AES256-GCM-SHA384:ECDHE-RSA-AES256-GCM-SHA384:ECDHE-ECDSA-CHACHA20-POLY1305:ECDHE-RSA-CHACHA20-POLY1305:DHE-RSA-AES128-GCM-SHA256:DHE-RSA-AES256-GCM-SHA384:DHE-RSA-CHACHA20-POLY1305
SSLHonorCipherOrder     Off
SSLSessionTickets       Off
```

> **Nota**: En el perfil Intermediate, `SSLHonorCipherOrder Off` permite que
> el cliente elija, ya que todos los ciphers listados son seguros y el cliente
> puede preferir los más eficientes para su hardware (ej: CHACHA20 en móviles
> sin aceleración AES).

---

## 4. Directivas avanzadas de mod_ssl

### 4.1 Sesiones TLS

Reutilizar sesiones TLS evita repetir el handshake completo en conexiones
subsiguientes:

```apache
# Caché de sesiones compartida entre procesos
SSLSessionCache        shmcb:/var/run/apache2/ssl_scache(512000)
SSLSessionCacheTimeout 300    # 5 minutos

# Session tickets (alternativa sin estado en el servidor)
# Desactivar si se prioriza Forward Secrecy sobre rendimiento
SSLSessionTickets Off
```

**Session tickets vs session cache:**

| Mecanismo        | Forward Secrecy | Estado en servidor | Rendimiento |
|------------------|-----------------|--------------------|-------------|
| Session cache    | Preservado      | Requiere memoria   | Bueno       |
| Session tickets  | Comprometido*   | Sin estado         | Mejor       |

*Los session tickets cifran la sesión con una clave simétrica del servidor.
Si esa clave se compromete, las sesiones reutilizadas son descifrables. En
Apache no se puede rotar automáticamente la clave del ticket, por eso Mozilla
recomienda desactivarlos.

### 4.2 Parámetros DH personalizados

Para cipher suites DHE, se pueden generar parámetros DH propios:

```bash
openssl dhparam -out /etc/ssl/private/dhparams.pem 2048
```

```apache
SSLOpenSSLConfCmd DHParameters "/etc/ssl/private/dhparams.pem"
```

Con TLS 1.3, DHE es reemplazado por ECDHE y los parámetros DH clásicos son
irrelevantes.

### 4.3 Compresión TLS

```apache
# SIEMPRE desactivar — vulnerable a ataque CRIME
SSLCompression Off
```

### 4.4 Variables de entorno SSL

mod_ssl expone información del certificado como variables de entorno,
accesibles desde CGI y aplicaciones:

```apache
<VirtualHost *:443>
    SSLEngine On
    # ...

    # Exportar variables SSL al entorno de la aplicación
    SSLOptions +StdEnvVars

    <Directory "/var/www/app/cgi-bin">
        SSLOptions +StdEnvVars
    </Directory>
</VirtualHost>
```

Variables disponibles:

| Variable              | Contenido                              |
|-----------------------|----------------------------------------|
| `SSL_PROTOCOL`        | Versión TLS (TLSv1.2, TLSv1.3)        |
| `SSL_CIPHER`          | Cipher suite negociada                 |
| `SSL_CLIENT_S_DN`     | Subject del certificado del cliente    |
| `SSL_SERVER_S_DN`     | Subject del certificado del servidor   |
| `SSL_SESSION_ID`      | ID de la sesión TLS                    |
| `HTTPS`               | `on` si la conexión es TLS             |

### 4.5 Autenticación de cliente con certificado

Raramente usado en sitios web públicos, pero común en APIs internas y
entornos corporativos:

```apache
<VirtualHost *:443>
    SSLEngine On
    SSLCertificateFile    /etc/ssl/certs/server.crt
    SSLCertificateKeyFile /etc/ssl/private/server.key

    # Requerir certificado de cliente
    SSLVerifyClient require
    SSLVerifyDepth 2
    SSLCACertificateFile /etc/ssl/certs/client-ca.crt

    # Solo para rutas específicas
    <Location "/api/internal">
        SSLVerifyClient require
    </Location>

    <Location "/public">
        SSLVerifyClient none
    </Location>
</VirtualHost>
```

---

## 5. HSTS y redirección HTTP → HTTPS

### 5.1 Redirección HTTP → HTTPS

El primer paso es redirigir todo el tráfico HTTP al equivalente HTTPS:

```apache
<VirtualHost *:80>
    ServerName ejemplo.com
    ServerAlias www.ejemplo.com

    # Redirigir todo a HTTPS
    Redirect permanent "/" "https://ejemplo.com/"
</VirtualHost>
```

Alternativa con mod_rewrite (más control):

```apache
<VirtualHost *:80>
    ServerName ejemplo.com
    ServerAlias www.ejemplo.com

    RewriteEngine On
    RewriteCond %{HTTPS} off
    RewriteRule ^(.*)$ https://%{HTTP_HOST}$1 [R=301,L]
</VirtualHost>
```

### 5.2 HSTS (HTTP Strict Transport Security)

HSTS instruye al navegador para que **nunca** acceda al sitio por HTTP. Tras
recibir la cabecera, el navegador convierte automáticamente cualquier URL HTTP
a HTTPS antes de enviar la petición:

```apache
<VirtualHost *:443>
    ServerName ejemplo.com
    # ...

    # Activar HSTS — 1 año, incluir subdominios
    Header always set Strict-Transport-Security \
        "max-age=31536000; includeSubDomains"
</VirtualHost>
```

```
Sin HSTS:
  Usuario escribe http://ejemplo.com
  → Petición HTTP al servidor (texto plano, interceptable)
  → Servidor responde 301 → https://ejemplo.com
  → Petición HTTPS

Con HSTS (tras primera visita):
  Usuario escribe http://ejemplo.com
  → Navegador internamente redirige a https://ejemplo.com
  → Petición HTTPS directa (nunca sale HTTP)
```

**Parámetros de HSTS:**

| Parámetro          | Efecto                                             |
|--------------------|----------------------------------------------------|
| `max-age=N`        | Segundos que el navegador recuerda la política      |
| `includeSubDomains`| Aplica a todos los subdominios                     |
| `preload`          | Permite inclusión en lista preload de navegadores   |

> **Precaución**: Activar HSTS es fácil de hacer pero difícil de revertir.
> Si se establece `max-age=31536000`, los navegadores que visitaron el sitio
> se negarán a acceder por HTTP durante un año. Empezar con valores bajos
> (`max-age=300`) para probar, luego aumentar gradualmente.

### 5.3 HSTS Preload

Para eliminar incluso la primera petición HTTP (que es vulnerable), los
navegadores mantienen una lista hardcoded de dominios que solo aceptan HTTPS:

```apache
Header always set Strict-Transport-Security \
    "max-age=31536000; includeSubDomains; preload"
```

Se envía el dominio a `hstspreload.org` para su inclusión. Los requisitos son:
certificado válido, redirección HTTP→HTTPS, HSTS con `max-age` ≥ 1 año,
`includeSubDomains` y `preload`.

---

## 6. OCSP Stapling

### 6.1 El problema de la verificación de revocación

Cuando un certificado se compromete, la CA lo revoca. El navegador necesita
verificar si el certificado está revocado:

```
Sin OCSP Stapling:
  Cliente ─── TLS ───► Servidor
  Cliente ─── OCSP ──► CA (ocsp.letsencrypt.org)
                        ← respuesta OCSP (válido/revocado)

Problemas:
  - Latencia adicional (consulta OCSP por cada conexión)
  - Privacidad (la CA sabe qué sitios visita el cliente)
  - Si OCSP no responde: ¿bloquear o permitir? (soft-fail)

Con OCSP Stapling:
  Servidor ─── OCSP ──► CA (periódicamente, cada pocas horas)
  Servidor cachea la respuesta OCSP firmada

  Cliente ─── TLS ───► Servidor
                        (incluye respuesta OCSP en el handshake)

Ventajas:
  - Sin latencia extra para el cliente
  - Privacidad preservada
  - Respuesta OCSP firmada por la CA = no falsificable
```

### 6.2 Configuración

```apache
# Activar OCSP Stapling
SSLUseStapling On

# Caché para respuestas OCSP (compartida entre procesos)
SSLStaplingCache shmcb:/var/run/apache2/ssl_stapling(32768)

# Timeout para consultas OCSP
SSLStaplingResponderTimeout 5

# Si OCSP falla, no devolver respuesta stapled (en vez de una vacía)
SSLStaplingReturnResponderErrors Off
```

Estas directivas van en el contexto global (fuera de VirtualHost) excepto
`SSLUseStapling` que puede ir dentro.

### 6.3 Verificar OCSP Stapling

```bash
openssl s_client -connect ejemplo.com:443 -servername ejemplo.com \
    -status < /dev/null 2>/dev/null | grep -A 10 "OCSP Response"

# Si funciona:
# OCSP Response Status: successful (0x0)
# ...
# Cert Status: good

# Si no está configurado:
# OCSP response: no response sent
```

---

## 7. Múltiples certificados y SNI

### 7.1 SNI (Server Name Indication)

SNI permite servir múltiples certificados desde la misma IP:puerto. El cliente
envía el nombre del servidor durante el handshake TLS, antes de que se
establezca la conexión cifrada:

```apache
# Dos sitios HTTPS en la misma IP
<VirtualHost *:443>
    ServerName sitio-a.com
    SSLEngine On
    SSLCertificateFile    /etc/ssl/certs/sitio-a-fullchain.pem
    SSLCertificateKeyFile /etc/ssl/private/sitio-a.key
    DocumentRoot "/var/www/sitio-a"
</VirtualHost>

<VirtualHost *:443>
    ServerName sitio-b.com
    SSLEngine On
    SSLCertificateFile    /etc/ssl/certs/sitio-b-fullchain.pem
    SSLCertificateKeyFile /etc/ssl/private/sitio-b.key
    DocumentRoot "/var/www/sitio-b"
</VirtualHost>
```

### 7.2 Certificado dual RSA + ECDSA

Un servidor puede ofrecer dos certificados y seleccionar según la capacidad
del cliente:

```apache
<VirtualHost *:443>
    ServerName ejemplo.com

    SSLEngine On

    # Certificado ECDSA (preferido — handshake más rápido)
    SSLCertificateFile    /etc/ssl/certs/ejemplo-ecdsa-fullchain.pem
    SSLCertificateKeyFile /etc/ssl/private/ejemplo-ecdsa.key

    # Certificado RSA (fallback para clientes legacy)
    SSLCertificateFile    /etc/ssl/certs/ejemplo-rsa-fullchain.pem
    SSLCertificateKeyFile /etc/ssl/private/ejemplo-rsa.key

    DocumentRoot "/var/www/ejemplo"
</VirtualHost>
```

Apache 2.4.8+ permite múltiples pares `SSLCertificateFile`/`SSLCertificateKeyFile`
en el mismo VirtualHost. Selecciona automáticamente según el cipher negociado.

---

## 8. Configuración recomendada para producción

### 8.1 Perfil Intermediate completo

Basado en las recomendaciones de Mozilla, adaptado para Apache 2.4 con
OpenSSL 1.1.1+:

```apache
# ──── Configuración global SSL (ssl.conf o conf global) ────

# Protocolos
SSLProtocol             -all +TLSv1.2 +TLSv1.3

# Cipher suites
SSLCipherSuite          ECDHE-ECDSA-AES128-GCM-SHA256:ECDHE-RSA-AES128-GCM-SHA256:ECDHE-ECDSA-AES256-GCM-SHA384:ECDHE-RSA-AES256-GCM-SHA384:ECDHE-ECDSA-CHACHA20-POLY1305:ECDHE-RSA-CHACHA20-POLY1305:DHE-RSA-AES128-GCM-SHA256:DHE-RSA-AES256-GCM-SHA384:DHE-RSA-CHACHA20-POLY1305
SSLHonorCipherOrder     Off

# Curvas ECDH
SSLOpenSSLConfCmd Curves X25519:secp256r1:secp384r1

# Sesiones
SSLSessionCache         shmcb:/var/run/apache2/ssl_scache(512000)
SSLSessionCacheTimeout  300
SSLSessionTickets       Off

# Compresión (desactivar por CRIME)
SSLCompression          Off

# OCSP Stapling
SSLUseStapling          On
SSLStaplingCache        shmcb:/var/run/apache2/ssl_stapling(32768)
SSLStaplingResponderTimeout 5
SSLStaplingReturnResponderErrors Off


# ──── VirtualHost HTTPS ────

<VirtualHost *:80>
    ServerName ejemplo.com
    Redirect permanent "/" "https://ejemplo.com/"
</VirtualHost>

<VirtualHost *:443>
    ServerName ejemplo.com
    DocumentRoot "/var/www/ejemplo/public"

    SSLEngine On
    SSLCertificateFile    /etc/letsencrypt/live/ejemplo.com/fullchain.pem
    SSLCertificateKeyFile /etc/letsencrypt/live/ejemplo.com/privkey.pem

    # HSTS
    Header always set Strict-Transport-Security \
        "max-age=63072000; includeSubDomains"

    # Cabeceras de seguridad adicionales
    Header always set X-Content-Type-Options "nosniff"
    Header always set X-Frame-Options "SAMEORIGIN"
    Header always set Referrer-Policy "strict-origin-when-cross-origin"

    <Directory "/var/www/ejemplo/public">
        Require all granted
        Options -Indexes
    </Directory>

    ErrorLog ${APACHE_LOG_DIR}/ejemplo-ssl-error.log
    CustomLog ${APACHE_LOG_DIR}/ejemplo-ssl-access.log combined
</VirtualHost>
```

### 8.2 Verificar la configuración

```bash
# Validar sintaxis
apachectl configtest

# Verificar protocolos y ciphers desde fuera
# Con nmap
nmap --script ssl-enum-ciphers -p 443 ejemplo.com

# Con openssl
openssl s_client -connect ejemplo.com:443 -servername ejemplo.com \
    -tls1_2 < /dev/null 2>/dev/null | grep "Protocol\|Cipher"

# Intentar TLS 1.1 (debe fallar)
openssl s_client -connect ejemplo.com:443 -servername ejemplo.com \
    -tls1_1 < /dev/null 2>&1 | grep -i "error\|alert"

# Verificar HSTS
curl -sI https://ejemplo.com | grep -i strict

# Test online completo
# Usar SSL Labs (ssllabs.com/ssltest) para verificación exhaustiva
```

---

## 9. Errores comunes

### Error 1: Apache no arranca tras activar SSL — "no listening sockets"

```
AH00526: Syntax error on line 36: SSLCertificateFile: file does not exist
```

o

```
no listening sockets available, shutting down
```

**Causa**: Falta `Listen 443` o los archivos de certificado no existen.

**Solución**:

```bash
# Verificar que Listen 443 está configurado
grep -rn "Listen 443" /etc/apache2/    # Debian
grep -rn "Listen 443" /etc/httpd/      # RHEL

# Verificar que los archivos de certificado existen
ls -la /etc/ssl/certs/ejemplo.crt
ls -la /etc/ssl/private/ejemplo.key
```

### Error 2: ERR_SSL_PROTOCOL_ERROR — mismatch de clave

```
SSL Library Error: error:0B080074: x509 certificate routines:
  X509_check_private_key: key values mismatch
```

**Causa**: La clave privada no corresponde al certificado.

**Diagnóstico**:

```bash
openssl x509 -in cert.pem -modulus -noout | openssl md5
openssl rsa -in key.pem -modulus -noout | openssl md5
# Deben coincidir
```

### Error 3: Cadena incompleta — funciona en Chrome pero no en curl

**Causa**: Chrome puede descargar certificados intermedios automáticamente, pero
`curl` y otras herramientas no. Si falta la cadena, estas herramientas fallan.

**Diagnóstico**:

```bash
# Muestra la cadena recibida
openssl s_client -connect ejemplo.com:443 -servername ejemplo.com < /dev/null
# Si depth=0 es el único certificado → falta la cadena

# Verificar contra el almacén del sistema
openssl s_client -connect ejemplo.com:443 -servername ejemplo.com \
    < /dev/null 2>&1 | grep "Verify return code"
# code 21 = unable to verify → cadena incompleta
```

**Solución**: Usar `fullchain.pem` en `SSLCertificateFile`.

### Error 4: HSTS bloquea acceso tras revertir a HTTP

**Causa**: Se activó HSTS con `max-age` largo, luego se quitó HTTPS. Los
navegadores que visitaron el sitio se niegan a conectar por HTTP.

**Solución**: Antes de quitar HTTPS, establecer `max-age=0` y esperar a que
los navegadores actualicen su caché HSTS. Si ya es tarde, los usuarios afectados
deben limpiar el estado HSTS de su navegador manualmente.

### Error 5: Rendimiento degradado tras activar HTTPS

**Causa habitual**: Session cache no configurada, cada conexión hace handshake
completo.

**Solución**:

```apache
SSLSessionCache shmcb:/var/run/apache2/ssl_scache(512000)
SSLSessionCacheTimeout 300
```

Sin session cache, cada nueva conexión (incluyendo cada recurso en keep-alive
expirado) requiere un handshake TLS completo, que es computacionalmente costoso.

---

## 10. Cheatsheet

```
┌─────────────────────────────────────────────────────────────────┐
│           APACHE TLS (mod_ssl) — REFERENCIA RÁPIDA              │
├─────────────────────────────────────────────────────────────────┤
│                                                                 │
│  ACTIVAR:                                                       │
│  Debian: a2enmod ssl headers && systemctl restart apache2       │
│  RHEL:   dnf install mod_ssl && systemctl restart httpd         │
│                                                                 │
│  VHOST MÍNIMO:                                                  │
│  <VirtualHost *:443>                                            │
│      ServerName ejemplo.com                                     │
│      SSLEngine On                                               │
│      SSLCertificateFile    /path/fullchain.pem                  │
│      SSLCertificateKeyFile /path/privkey.pem                    │
│      DocumentRoot "/var/www/ejemplo"                            │
│  </VirtualHost>                                                 │
│                                                                 │
│  PROTOCOLOS:                                                    │
│  SSLProtocol -all +TLSv1.2 +TLSv1.3                            │
│                                                                 │
│  CIPHER SUITES (intermediate):                                  │
│  SSLCipherSuite ECDHE-ECDSA-AES128-GCM-SHA256:ECDHE-RSA-...    │
│  SSLHonorCipherOrder Off   (todos los listados son seguros)     │
│                                                                 │
│  SESIONES:                                                      │
│  SSLSessionCache shmcb:/path(512000)                            │
│  SSLSessionCacheTimeout 300                                     │
│  SSLSessionTickets Off                                          │
│                                                                 │
│  OCSP STAPLING:                                                 │
│  SSLUseStapling On                                              │
│  SSLStaplingCache shmcb:/path(32768)                            │
│                                                                 │
│  SEGURIDAD:                                                     │
│  SSLCompression Off                     (prevenir CRIME)        │
│  Header always set Strict-Transport-Security \                  │
│      "max-age=63072000; includeSubDomains"   (HSTS)             │
│                                                                 │
│  REDIRECCIÓN HTTP→HTTPS:                                        │
│  <VirtualHost *:80>                                             │
│      Redirect permanent "/" "https://ejemplo.com/"              │
│  </VirtualHost>                                                 │
│                                                                 │
│  PERFILES MOZILLA:                                              │
│  Modern:       TLS 1.3 only (nuevos despliegues)               │
│  Intermediate: TLS 1.2+1.3 (producción general)                │
│  Old:          TLS 1.0+ (solo legacy)                           │
│                                                                 │
│  FORWARD SECRECY:                                               │
│  Siempre: ECDHE o DHE en los ciphers                            │
│  Nunca: RSA key exchange (sin FS)                               │
│                                                                 │
│  VERIFICAR:                                                     │
│  openssl s_client -connect host:443 -servername host            │
│  nmap --script ssl-enum-ciphers -p 443 host                     │
│  curl -sI https://host | grep -i strict   (HSTS)               │
│  apachectl configtest                      (sintaxis)           │
│  SSL Labs online                           (test completo)      │
│                                                                 │
│  PAR CLAVE-CERT:                                                │
│  openssl x509 -in crt -modulus -noout | openssl md5             │
│  openssl rsa -in key -modulus -noout | openssl md5              │
│  (deben coincidir)                                              │
│                                                                 │
└─────────────────────────────────────────────────────────────────┘
```

---

## 11. Ejercicios

### Ejercicio 1: VirtualHost HTTPS con certificado autofirmado

1. Activa mod_ssl y mod_headers en tu distribución.
2. Genera un certificado autofirmado con SAN para `tls-lab.local`.
3. Crea un VirtualHost HTTPS en puerto 443 con:
   - `SSLEngine On` y las rutas al certificado y clave.
   - Un `index.html` simple.
4. Crea un VirtualHost HTTP en puerto 80 que redirija a HTTPS con
   `Redirect permanent`.
5. Verifica con `apachectl configtest`.
6. Accede con `curl -vk https://tls-lab.local/` y examina:
   - El protocolo TLS negociado
   - La cipher suite usada
   - Las fechas del certificado
7. Intenta acceder con `curl` (sin `-k`). ¿Qué error obtienes?
8. Verifica la redirección HTTP→HTTPS con `curl -v http://tls-lab.local/`.

**Pregunta de predicción**: Si configuras `SSLProtocol -all +TLSv1.3` y pruebas
con un cliente que solo soporta TLS 1.2, ¿qué sucederá? ¿Apache devolverá un
error HTTP o la conexión fallará antes del HTTP?

### Ejercicio 2: Cipher suites y protocolos

1. Configura el perfil Intermediate de Mozilla en tu VirtualHost.
2. Verifica los ciphers disponibles:
   ```bash
   nmap --script ssl-enum-ciphers -p 443 tls-lab.local
   ```
3. Intenta conectar forzando TLS 1.1:
   ```bash
   openssl s_client -connect tls-lab.local:443 -tls1_1
   ```
   ¿Se conecta o falla?
4. Intenta forzar un cipher sin Forward Secrecy:
   ```bash
   openssl s_client -connect tls-lab.local:443 \
       -cipher AES128-SHA256
   ```
   ¿Se conecta o falla?
5. Configura `SSLSessionCache` y verifica que funciona reutilizando sesiones:
   ```bash
   openssl s_client -connect tls-lab.local:443 -reconnect
   ```
   Busca `Reused` en la salida de la reconexión.
6. Activa OCSP Stapling y verifica con `openssl s_client -status`.

**Pregunta de predicción**: Si activas `SSLHonorCipherOrder On` y pones
`CHACHA20-POLY1305` primero en la lista, ¿todos los clientes usarán CHACHA20?
¿O algún cliente podría negociar AES-GCM? ¿De qué depende?

### Ejercicio 3: HSTS y seguridad completa

1. Añade la cabecera HSTS con `max-age=300` (5 minutos, para probar).
2. Accede con un navegador a `https://tls-lab.local` (aceptando el certificado
   autofirmado).
3. Cambia la URL en la barra del navegador a `http://tls-lab.local`. ¿El
   navegador envía una petición HTTP o redirige internamente a HTTPS?
4. Inspecciona las herramientas de desarrollo del navegador (Network tab) para
   ver la redirección interna (código 307 Internal Redirect).
5. Añade las cabeceras de seguridad completas: `X-Content-Type-Options`,
   `X-Frame-Options`, `Referrer-Policy`.
6. Verifica todas las cabeceras con `curl -sI https://tls-lab.local/`.
7. Sube el `max-age` de HSTS a `31536000` (1 año). Reflexiona: ¿qué pasaría
   si después necesitas volver a HTTP?

**Pregunta de reflexión**: HSTS con `includeSubDomains` afecta a todos los
subdominios, incluyendo los que podrían no tener HTTPS configurado (ej:
`intranet.ejemplo.com` solo accesible por HTTP internamente). ¿Qué
consecuencias tendría activar `includeSubDomains` sin verificar que todos
los subdominios soporten HTTPS? ¿Cómo diagnosticarías qué subdominios se
verían afectados antes de activarlo?

---

> **Siguiente tema**: T03 — Configuración TLS en Nginx (ssl_certificate,
> ssl_protocols, HSTS).
