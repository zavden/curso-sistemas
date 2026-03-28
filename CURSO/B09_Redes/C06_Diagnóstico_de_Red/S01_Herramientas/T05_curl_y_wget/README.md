# curl y wget — Pruebas HTTP y diagnóstico

## Índice

1. [curl vs wget — cuándo usar cada uno](#1-curl-vs-wget--cuándo-usar-cada-uno)
2. [curl — fundamentos](#2-curl--fundamentos)
3. [curl — cabeceras y métodos HTTP](#3-curl--cabeceras-y-métodos-http)
4. [curl — autenticación y datos](#4-curl--autenticación-y-datos)
5. [curl — diagnóstico de conexiones](#5-curl--diagnóstico-de-conexiones)
6. [curl — TLS/SSL](#6-curl--tlsssl)
7. [curl — redirecciones y cookies](#7-curl--redirecciones-y-cookies)
8. [curl — descargas y subidas](#8-curl--descargas-y-subidas)
9. [wget — fundamentos](#9-wget--fundamentos)
10. [wget — descarga recursiva y mirroring](#10-wget--descarga-recursiva-y-mirroring)
11. [wget — opciones avanzadas](#11-wget--opciones-avanzadas)
12. [Escenarios de diagnóstico](#12-escenarios-de-diagnóstico)
13. [Errores comunes](#13-errores-comunes)
14. [Cheatsheet](#14-cheatsheet)
15. [Ejercicios](#15-ejercicios)

---

## 1. curl vs wget — cuándo usar cada uno

Ambas son herramientas de transferencia por línea de comandos, pero tienen
filosofías distintas:

| Característica | curl | wget |
|---------------|------|------|
| **Filosofía** | Transferir datos con URLs | Descargar archivos |
| **Protocolos** | HTTP, HTTPS, FTP, SFTP, SCP, SMTP, IMAP, POP3, LDAP, MQTT… (~25) | HTTP, HTTPS, FTP |
| **Salida default** | stdout (pantalla) | Archivo en disco |
| **Descarga recursiva** | No | Sí (mirror de sitios) |
| **API REST** | Excelente (métodos, headers, JSON) | Limitado |
| **Reintentos** | Manual (`--retry`) | Automático en descargas |
| **Progreso** | Barra de progreso | Barra de progreso |
| **Dependencias** | libcurl (muy portable) | Autónomo |
| **Pipes** | Ideal (`curl url \| jq`) | No pensado para pipes |

```
¿Qué necesitas hacer?
     │
     ├── Probar API REST, ver headers, diagnosticar HTTP → curl
     ├── Descargar un archivo grande con reintentos    → wget (o curl)
     ├── Mirror de un sitio web completo               → wget
     ├── Enviar datos POST/PUT con JSON                 → curl
     ├── Transferir por SCP/SFTP/SMTP                  → curl
     └── Script simple: descargar y guardar            → wget
```

---

## 2. curl — fundamentos

### Sintaxis básica

```bash
curl [opciones] URL
```

### Petición simple

```bash
# GET a una URL (salida a stdout)
curl http://example.com

# Guardar a archivo
curl -o pagina.html http://example.com
curl -O http://example.com/archivo.tar.gz    # -O usa el nombre del archivo de la URL

# Silenciar la barra de progreso
curl -s http://example.com

# Silenciar progreso pero mostrar errores
curl -sS http://example.com

# Modo verbose (ver todo el intercambio HTTP)
curl -v http://example.com
```

### Anatomía de curl -v

```bash
$ curl -v http://example.com

*   Trying 93.184.216.34:80...                    ← resolución DNS + conexión TCP
* Connected to example.com (93.184.216.34) port 80 (#0)
> GET / HTTP/1.1                                   ← request line
> Host: example.com                                ← headers enviados
> User-Agent: curl/7.81.0                          (> = lo que envía curl)
> Accept: */*
>
* Mark bundle as not supporting multiuse
< HTTP/1.1 200 OK                                  ← response status
< Content-Type: text/html; charset=UTF-8           ← headers recibidos
< Content-Length: 1256                              (< = lo que recibe curl)
< Cache-Control: max-age=604800
< Date: Fri, 21 Mar 2026 14:23:01 GMT
<
<!doctype html>                                    ← body de la respuesta
<html>
...
```

Las líneas con `>` son lo que curl envía. Las líneas con `<` son lo que
recibe. Las líneas con `*` son información de diagnóstico de curl.

---

## 3. curl — cabeceras y métodos HTTP

### Ver solo las cabeceras de respuesta

```bash
# -I: petición HEAD (solo cabeceras, sin body)
curl -I http://example.com
# HTTP/1.1 200 OK
# Content-Type: text/html; charset=UTF-8
# Content-Length: 1256
# ...

# -i: incluir cabeceras en la salida (con body)
curl -i http://example.com
# HTTP/1.1 200 OK
# Content-Type: text/html
# ...
# (línea vacía)
# <!doctype html>...

# Solo cabeceras de respuesta con -v (filtrar con grep)
curl -sI http://example.com
```

### Enviar cabeceras personalizadas

```bash
# -H: añadir cabecera
curl -H "Accept: application/json" http://api.example.com/data

# Múltiples cabeceras
curl -H "Accept: application/json" \
     -H "Authorization: Bearer eyJhbG..." \
     -H "X-Custom-Header: value" \
     http://api.example.com/data

# Sobrescribir User-Agent
curl -H "User-Agent: MyApp/1.0" http://example.com

# Eliminar una cabecera que curl envía por defecto
curl -H "Accept:" http://example.com    # elimina Accept
```

### Métodos HTTP

```bash
# GET (default)
curl http://api.example.com/users

# POST
curl -X POST http://api.example.com/users

# PUT
curl -X PUT http://api.example.com/users/1

# DELETE
curl -X DELETE http://api.example.com/users/1

# PATCH
curl -X PATCH http://api.example.com/users/1

# OPTIONS (ver métodos permitidos)
curl -X OPTIONS -i http://api.example.com/users
```

**Nota**: para POST y PUT normalmente necesitas enviar datos (ver siguiente
sección). `-X POST` sin datos envía un POST con body vacío.

---

## 4. curl — autenticación y datos

### Enviar datos POST

```bash
# Datos de formulario (application/x-www-form-urlencoded)
curl -d "username=admin&password=secret" http://example.com/login

# Equivalente con -X POST (curl deduce POST cuando usas -d)
curl -d "username=admin&password=secret" http://example.com/login

# Datos JSON
curl -H "Content-Type: application/json" \
     -d '{"username":"admin","password":"secret"}' \
     http://api.example.com/login

# Datos desde archivo
curl -d @data.json -H "Content-Type: application/json" \
     http://api.example.com/users

# Datos desde stdin
echo '{"name":"test"}' | curl -d @- -H "Content-Type: application/json" \
     http://api.example.com/users
```

### Multipart form (subida de archivos)

```bash
# -F: form multipart (como un formulario HTML con file input)
curl -F "file=@/path/to/document.pdf" http://example.com/upload

# Con campos adicionales
curl -F "file=@photo.jpg" \
     -F "description=Mi foto" \
     -F "category=personal" \
     http://example.com/upload
```

### Autenticación

```bash
# Basic auth
curl -u username:password http://example.com/api

# Basic auth (pedirá la contraseña interactivamente)
curl -u username http://example.com/api

# Bearer token (API moderna)
curl -H "Authorization: Bearer eyJhbG..." http://api.example.com/data

# Digest auth
curl --digest -u username:password http://example.com/api
```

### Interacción con APIs REST

```bash
# Ejemplo completo: CRUD con una API REST

# CREATE (POST)
curl -s -X POST \
     -H "Content-Type: application/json" \
     -H "Authorization: Bearer $TOKEN" \
     -d '{"name":"Server1","ip":"192.168.1.100"}' \
     http://api.example.com/servers | jq .

# READ (GET)
curl -s -H "Authorization: Bearer $TOKEN" \
     http://api.example.com/servers/1 | jq .

# UPDATE (PUT)
curl -s -X PUT \
     -H "Content-Type: application/json" \
     -H "Authorization: Bearer $TOKEN" \
     -d '{"name":"Server1-updated","ip":"192.168.1.101"}' \
     http://api.example.com/servers/1 | jq .

# DELETE
curl -s -X DELETE \
     -H "Authorization: Bearer $TOKEN" \
     http://api.example.com/servers/1

# Verificar código de respuesta
curl -s -o /dev/null -w "%{http_code}" http://api.example.com/health
# 200
```

---

## 5. curl — diagnóstico de conexiones

### -w: formato de salida personalizado

La opción `-w` permite extraer métricas detalladas de la conexión:

```bash
# Medir tiempos de conexión
curl -s -o /dev/null -w "\
    DNS:        %{time_namelookup}s\n\
    Connect:    %{time_connect}s\n\
    TLS:        %{time_appconnect}s\n\
    TTFB:       %{time_starttransfer}s\n\
    Total:      %{time_total}s\n\
    HTTP Code:  %{http_code}\n\
    Size:       %{size_download} bytes\n\
    Speed:      %{speed_download} bytes/s\n" \
    https://www.example.com
```

Salida:
```
    DNS:        0.012s
    Connect:    0.045s
    TLS:        0.123s
    TTFB:       0.234s
    Total:      0.256s
    HTTP Code:  200
    Size:       1256 bytes
    Speed:      4906 bytes/s
```

### Desglose de tiempos

```
|── DNS lookup ──|── TCP connect ──|── TLS handshake ──|── Request + Wait ──|── Transfer ──|
0            namelookup         connect            appconnect          starttransfer      total

time_namelookup:    resolución DNS
time_connect:       conexión TCP establecida
time_appconnect:    handshake TLS completado (solo HTTPS)
time_starttransfer: primer byte recibido (TTFB)
time_total:         transferencia completa
```

```bash
# Guardar el formato en un archivo para reutilizar
cat > /tmp/curl-format.txt << 'EOF'
     DNS Lookup:  %{time_namelookup}s\n
  TCP Connected:  %{time_connect}s\n
TLS Handshake:  %{time_appconnect}s\n
         TTFB:  %{time_starttransfer}s\n
     Download:  %{time_total}s\n
   HTTP Code:  %{http_code}\n
   Downloaded:  %{size_download} bytes\n
EOF

curl -s -o /dev/null -w "@/tmp/curl-format.txt" https://www.example.com
```

### Variables -w útiles

| Variable | Significado |
|----------|------------|
| `%{http_code}` | Código HTTP (200, 301, 404, 500…) |
| `%{time_namelookup}` | Tiempo de resolución DNS |
| `%{time_connect}` | Tiempo hasta conexión TCP |
| `%{time_appconnect}` | Tiempo hasta handshake TLS |
| `%{time_starttransfer}` | Tiempo hasta primer byte (TTFB) |
| `%{time_total}` | Tiempo total |
| `%{size_download}` | Bytes descargados |
| `%{speed_download}` | Velocidad de descarga |
| `%{num_redirects}` | Número de redirecciones |
| `%{redirect_url}` | URL de la redirección |
| `%{ssl_verify_result}` | Resultado verificación SSL (0=OK) |
| `%{remote_ip}` | IP del servidor |
| `%{remote_port}` | Puerto del servidor |
| `%{local_ip}` | IP local usada |
| `%{local_port}` | Puerto local usado |
| `%{content_type}` | Content-Type de la respuesta |

### Resolver DNS manualmente

```bash
# Forzar una IP específica para un hostname (bypass DNS)
curl --resolve example.com:443:93.184.216.34 https://example.com

# Útil para:
# - Probar un servidor antes de cambiar DNS
# - Verificar un backend específico detrás de un balanceador
# - Probar con IP interna cuando DNS apunta a IP pública

# Ejemplo: probar nuevo servidor antes de migración DNS
curl --resolve www.example.com:443:10.0.0.50 https://www.example.com
```

### Timeouts

```bash
# Timeout de conexión (solo la fase TCP)
curl --connect-timeout 5 http://example.com

# Timeout total (toda la operación)
curl --max-time 30 http://example.com

# Ambos combinados
curl --connect-timeout 5 --max-time 30 http://example.com

# Diagnóstico: ¿es DNS lento, conexión lenta o servidor lento?
curl -s -o /dev/null -w "DNS: %{time_namelookup}s, TCP: %{time_connect}s, Total: %{time_total}s\n" \
     --connect-timeout 5 --max-time 10 http://example.com
```

---

## 6. curl — TLS/SSL

### Verificar certificados

```bash
# curl verifica certificados por defecto (correcto)
curl https://example.com
# Si el certificado es inválido → error

# Ver información del certificado
curl -vI https://example.com 2>&1 | grep -E "subject:|issuer:|expire|SSL"

# Ignorar errores de certificado (solo para diagnóstico, NUNCA en producción)
curl -k https://self-signed.example.com

# Especificar CA bundle
curl --cacert /path/to/ca-bundle.crt https://example.com

# Especificar certificado cliente (mTLS)
curl --cert client.crt --key client.key https://mtls.example.com
```

### Forzar versión TLS

```bash
# Solo TLS 1.2
curl --tlsv1.2 https://example.com

# Solo TLS 1.3
curl --tlsv1.3 https://example.com

# TLS 1.2 como mínimo (permite 1.2 y 1.3)
curl --tlsv1.2 --tls-max 1.3 https://example.com

# Verificar qué versión negoció
curl -vI https://example.com 2>&1 | grep "SSL connection"
# SSL connection using TLSv1.3 / TLS_AES_256_GCM_SHA384
```

### Diagnóstico SSL

```bash
# Ver toda la negociación TLS
curl -vI https://example.com 2>&1 | grep -E "SSL|TLS|certificate|subject|issuer"

# ¿El certificado expira pronto?
curl -vI https://example.com 2>&1 | grep "expire"
#   expire date: Jun 15 12:00:00 2026 GMT

# Verificar que no use TLS 1.0/1.1 (obsoletos)
curl --tlsv1.0 --tls-max 1.0 https://example.com
# Si falla: bien, el servidor rechaza TLS 1.0 ✓
# Si funciona: mal, TLS 1.0 sigue activo ✗
```

---

## 7. curl — redirecciones y cookies

### Seguir redirecciones

```bash
# Por defecto, curl NO sigue redirecciones
curl http://example.com
# Si hay 301/302, curl muestra la respuesta de redirección

# -L: seguir redirecciones automáticamente
curl -L http://example.com

# Ver la cadena de redirecciones
curl -L -v http://example.com 2>&1 | grep -E "^[<>*].*HTTP|Location:"

# Limitar número de redirecciones
curl -L --max-redirs 5 http://example.com

# Ver código de respuesta final vs intermedio
curl -sL -o /dev/null -w "Final: %{http_code}, Redirects: %{num_redirects}\n" \
     http://example.com
```

### Cookies

```bash
# Guardar cookies a archivo
curl -c cookies.txt http://example.com/login -d "user=admin&pass=secret"

# Enviar cookies desde archivo
curl -b cookies.txt http://example.com/dashboard

# Guardar y enviar en la misma sesión (simular navegador)
curl -c cookies.txt -b cookies.txt -L http://example.com

# Enviar cookie manualmente
curl -b "session=abc123; theme=dark" http://example.com

# Ver cookies en la respuesta
curl -I http://example.com 2>&1 | grep -i "set-cookie"
```

---

## 8. curl — descargas y subidas

### Descargar archivos

```bash
# Guardar con nombre específico
curl -o myfile.tar.gz http://example.com/file.tar.gz

# Guardar con el nombre original de la URL
curl -O http://example.com/file.tar.gz

# Descargar múltiples archivos
curl -O http://example.com/file1.tar.gz -O http://example.com/file2.tar.gz

# Reanudar descarga interrumpida
curl -C - -O http://example.com/large-file.iso

# Limitar velocidad de descarga
curl --limit-rate 1M -O http://example.com/large-file.iso

# Barra de progreso (en lugar de la tabla de estadísticas)
curl -# -O http://example.com/file.tar.gz

# Reintentar en caso de error
curl --retry 3 --retry-delay 5 -O http://example.com/file.tar.gz
```

### Subir archivos

```bash
# PUT upload
curl -T file.txt http://example.com/upload/file.txt

# POST multipart (formulario con archivo)
curl -F "file=@document.pdf" http://example.com/upload

# POST con tipo MIME específico
curl -F "file=@photo.jpg;type=image/jpeg" http://example.com/upload
```

---

## 9. wget — fundamentos

### Sintaxis básica

```bash
wget [opciones] URL
```

### Descarga simple

```bash
# Descargar archivo (se guarda en disco automáticamente)
wget http://example.com/file.tar.gz

# Guardar con nombre diferente
wget -O myfile.tar.gz http://example.com/file.tar.gz

# Guardar en directorio específico
wget -P /tmp/ http://example.com/file.tar.gz

# Salida a stdout (como curl por defecto)
wget -qO- http://example.com

# Modo silencioso
wget -q http://example.com/file.tar.gz
```

### Descargas robustas

```bash
# Continuar descarga interrumpida
wget -c http://example.com/large-file.iso

# Reintentar en caso de error (default: 20 intentos)
wget --tries=5 http://example.com/file.tar.gz

# Sin reintentos
wget --tries=1 http://example.com/file.tar.gz

# Timeout
wget --timeout=30 http://example.com/file.tar.gz
# Equivale a: --dns-timeout + --connect-timeout + --read-timeout

# Limitar velocidad
wget --limit-rate=500k http://example.com/large-file.iso

# Descarga en background
wget -b http://example.com/large-file.iso
# El progreso se guarda en wget-log
tail -f wget-log
```

### Descargar múltiples URLs

```bash
# Desde archivo (una URL por línea)
wget -i urls.txt

# Con intervalo entre descargas (cortesía)
wget -i urls.txt --wait=2

# Con intervalo aleatorio (menos detectable)
wget -i urls.txt --wait=2 --random-wait
```

---

## 10. wget — descarga recursiva y mirroring

### Descarga recursiva

```bash
# Descargar un sitio web recursivamente
wget -r http://example.com/docs/

# Limitar profundidad de recursión
wget -r -l 2 http://example.com/docs/
# -l 2: sigue enlaces hasta 2 niveles de profundidad

# Convertir enlaces para navegación offline
wget -r -l 2 -k http://example.com/docs/
# -k: convierte URLs absolutas a relativas en los archivos descargados

# Incluir recursos necesarios (CSS, imágenes, JS)
wget -r -l 2 -k -p http://example.com/docs/
# -p (page-requisites): descarga imágenes, CSS, scripts referenciados
```

### Mirror de un sitio

```bash
# Mirror completo (equivale a -r -N -l inf --no-remove-listing)
wget -m http://example.com/docs/

# Mirror con conversión para offline
wget -m -k -p http://example.com/docs/

# Mirror solo dentro del dominio
wget -m -k -p -np http://example.com/docs/
# -np (no-parent): no subir al directorio padre
```

### Filtros en descarga recursiva

```bash
# Solo archivos PDF
wget -r -A "*.pdf" http://example.com/docs/

# Excluir imágenes
wget -r -R "*.jpg,*.png,*.gif" http://example.com/docs/

# Solo un dominio (no seguir enlaces externos)
wget -r -D example.com http://example.com/

# Excluir directorios
wget -r -X /private,/admin http://example.com/

# Solo seguir enlaces dentro de un path
wget -r -I /docs,/api http://example.com/docs/
```

---

## 11. wget — opciones avanzadas

### Headers y autenticación

```bash
# Header personalizado
wget --header="Authorization: Bearer eyJhbG..." http://api.example.com/data

# User-Agent
wget --user-agent="MyBot/1.0" http://example.com

# Basic auth
wget --user=admin --password=secret http://example.com/private/

# Guardar cookies y usarlas
wget --save-cookies cookies.txt --keep-session-cookies \
     --post-data="user=admin&pass=secret" http://example.com/login
wget --load-cookies cookies.txt http://example.com/dashboard
```

### Certificados

```bash
# Ignorar errores de certificado (solo diagnóstico)
wget --no-check-certificate https://self-signed.example.com

# Especificar CA
wget --ca-certificate=/path/to/ca.crt https://example.com

# Certificado cliente
wget --certificate=client.crt --private-key=client.key https://mtls.example.com
```

### wget como herramienta de diagnóstico

```bash
# Ver cabeceras de respuesta
wget -S http://example.com -O /dev/null 2>&1 | head -20
# -S: mostrar cabeceras del servidor

# Ver solo el código de respuesta
wget --spider http://example.com 2>&1 | grep "HTTP/"
# --spider: no descarga, solo verifica que existe

# Verificar si una URL existe (sin descargar)
wget --spider -q http://example.com/file.tar.gz
echo $?    # 0 = existe, 8 = error 404

# Debug completo
wget -d http://example.com -O /dev/null 2>&1 | head -40
```

---

## 12. Escenarios de diagnóstico

### Escenario 1: "El sitio web está caído"

```bash
# Paso 1: ¿Responde el servidor? ¿Qué código devuelve?
curl -sI http://www.example.com
# Si no hay respuesta → problema de red o servidor apagado
# Si HTTP 502/503 → servidor arriba pero la app falla

# Paso 2: ¿Es problema de DNS?
curl -s -o /dev/null -w "DNS: %{time_namelookup}s\n" http://www.example.com
# DNS > 5s → DNS lento o roto
# Probar con IP directa:
curl -s --resolve www.example.com:80:93.184.216.34 http://www.example.com

# Paso 3: ¿Es problema de TLS?
curl -vI https://www.example.com 2>&1 | grep -E "SSL|error|expire"
# Certificado expirado → error TLS

# Paso 4: ¿Responde desde dentro del servidor?
ssh user@server 'curl -s localhost:80'
# Si funciona localmente pero no desde fuera → firewall o proxy
```

### Escenario 2: "La API responde lento"

```bash
# Medir cada fase de la conexión
curl -s -o /dev/null -w "\
DNS:     %{time_namelookup}s\n\
TCP:     %{time_connect}s\n\
TLS:     %{time_appconnect}s\n\
TTFB:    %{time_starttransfer}s\n\
Total:   %{time_total}s\n" \
https://api.example.com/endpoint

# Interpretar:
# DNS alto (>1s)        → problema de resolver DNS
# TCP-DNS alto (>200ms) → servidor lejos o red congestionada
# TLS-TCP alto (>500ms) → negociación TLS lenta (ciphers, OCSP)
# TTFB-TLS alto (>1s)   → el servidor tarda en procesar
# Total-TTFB alto       → respuesta grande o red lenta

# Comparar múltiples ejecuciones
for i in $(seq 1 5); do
    curl -s -o /dev/null -w "Run $i: TTFB=%{time_starttransfer}s Total=%{time_total}s\n" \
         https://api.example.com/endpoint
done
```

### Escenario 3: "Las redirecciones no funcionan"

```bash
# Ver la cadena de redirecciones
curl -sIL http://example.com 2>&1 | grep -E "HTTP/|Location:"
# HTTP/1.1 301 Moved Permanently
# Location: https://example.com/
# HTTP/2 301
# Location: https://www.example.com/
# HTTP/2 200

# ¿Loop de redirecciones?
curl -sL --max-redirs 10 -o /dev/null -w "Redirects: %{num_redirects}, Code: %{http_code}\n" \
     http://example.com
# Si num_redirects == max-redirs → hay un loop

# Verificar redirección HTTP → HTTPS
curl -sI http://example.com | grep Location
# Location: https://example.com/  ← correcto
```

### Escenario 4: probar un servidor antes de migrar DNS

```bash
# Servidor nuevo tiene IP 10.0.0.50, DNS apunta al viejo (198.51.100.1)
# Probar el nuevo sin cambiar DNS:
curl --resolve www.example.com:443:10.0.0.50 \
     -v https://www.example.com 2>&1 | head -20

# Verificar que el certificado funciona en el nuevo servidor
curl --resolve www.example.com:443:10.0.0.50 \
     -sI https://www.example.com

# Comparar respuestas viejo vs nuevo
diff <(curl -s https://www.example.com) \
     <(curl -s --resolve www.example.com:443:10.0.0.50 https://www.example.com)
```

### Escenario 5: verificar health checks

```bash
# Verificar endpoint de salud
curl -sf http://localhost:8080/health && echo "OK" || echo "FAIL"

# Script de monitorización básico
check_health() {
    local url=$1
    local code=$(curl -sf -o /dev/null -w "%{http_code}" --max-time 5 "$url")
    if [ "$code" = "200" ]; then
        echo "$(date): $url OK"
    else
        echo "$(date): $url FAIL (HTTP $code)" >&2
    fi
}

check_health http://localhost:8080/health
check_health http://localhost:3000/api/status
```

### Escenario 6: descargar archivo grande de forma fiable

```bash
# wget con reanudación automática (lo más robusto)
wget -c --tries=10 --timeout=60 http://example.com/large-file.iso

# curl con reanudación manual
curl -C - --retry 5 --retry-delay 10 -O http://example.com/large-file.iso

# Verificar integridad después de descargar
wget http://example.com/large-file.iso.sha256
sha256sum -c large-file.iso.sha256
```

---

## 13. Errores comunes

### Error 1: curl sin -L y no seguir redirecciones

```bash
# ✗ Obtener una página vacía o HTML de redirección
curl http://example.com
# <html><body>Moved Permanently</body></html>

# ✓ Seguir redirecciones
curl -L http://example.com
```

### Error 2: curl -v y no saber que la info va a stderr

```bash
# ✗ Intentar filtrar la salida verbose con pipe
curl -v http://example.com | grep "HTTP"
# No encuentra nada porque -v va a stderr

# ✓ Redirigir stderr
curl -v http://example.com 2>&1 | grep "HTTP"

# ✓ O separar body y headers
curl -sI http://example.com    # solo headers (a stdout)
```

### Error 3: wget descarga HTML en lugar del archivo

```bash
# ✗ URL con redirección o página de descarga
wget http://example.com/download?file=app.tar.gz
# Descarga el HTML de la página, no el archivo

# ✓ Usar --content-disposition
wget --content-disposition http://example.com/download?file=app.tar.gz

# ✓ O especificar nombre de salida
wget -O app.tar.gz http://example.com/download?file=app.tar.gz
```

### Error 4: curl -k en producción

```bash
# ✗ Ignorar errores de certificado en scripts de producción
curl -k https://api.internal.com/data
# Vulnerable a man-in-the-middle

# ✓ Configurar el CA correcto
curl --cacert /etc/pki/tls/certs/internal-ca.pem https://api.internal.com/data

# ✓ O instalar el CA en el sistema
cp internal-ca.pem /usr/local/share/ca-certificates/
update-ca-certificates
curl https://api.internal.com/data    # funciona sin -k
```

### Error 5: no escapar caracteres especiales en URLs

```bash
# ✗ Espacios y caracteres especiales en URL
curl http://example.com/path with spaces/file name.txt

# ✓ Codificar la URL
curl "http://example.com/path%20with%20spaces/file%20name.txt"

# ✓ O usar --data-urlencode para parámetros
curl -G --data-urlencode "q=hello world" http://api.example.com/search
```

---

## 14. Cheatsheet

```bash
# ╔══════════════════════════════════════════════════════════════════╗
# ║              CURL Y WGET — CHEATSHEET                          ║
# ╠══════════════════════════════════════════════════════════════════╣
# ║                                                                ║
# ║  CURL — DIAGNÓSTICO:                                          ║
# ║  curl -v URL                       # verbose (todo)           ║
# ║  curl -sI URL                      # solo headers             ║
# ║  curl -sL URL                      # seguir redirecciones     ║
# ║  curl -s -o /dev/null -w \                                    ║
# ║    "Code:%{http_code} TTFB:%{time_starttransfer}s\n" URL      ║
# ║  curl --resolve host:port:ip URL   # bypass DNS               ║
# ║  curl --connect-timeout 5 URL      # timeout TCP              ║
# ║  curl --max-time 30 URL            # timeout total            ║
# ║                                                                ║
# ║  CURL — API REST:                                             ║
# ║  curl -X POST -d '{"k":"v"}' \                                ║
# ║    -H "Content-Type: application/json" URL                    ║
# ║  curl -H "Authorization: Bearer TOKEN" URL                    ║
# ║  curl -u user:pass URL             # basic auth               ║
# ║  curl -F "file=@path" URL          # upload                   ║
# ║                                                                ║
# ║  CURL — DESCARGAS:                                            ║
# ║  curl -O URL                       # guardar nombre original  ║
# ║  curl -o file URL                  # guardar con nombre       ║
# ║  curl -C - -O URL                  # reanudar descarga        ║
# ║  curl --retry 3 URL                # reintentar               ║
# ║                                                                ║
# ║  CURL — TLS:                                                  ║
# ║  curl -k URL                       # ignorar cert (debug!)    ║
# ║  curl --cacert ca.pem URL          # CA personalizado         ║
# ║  curl --tlsv1.2 URL                # forzar TLS 1.2+          ║
# ║                                                                ║
# ║  WGET — DESCARGAS:                                            ║
# ║  wget URL                          # descargar archivo        ║
# ║  wget -c URL                       # reanudar                 ║
# ║  wget -O file URL                  # nombre custom            ║
# ║  wget -qO- URL                     # a stdout (como curl)     ║
# ║  wget -b URL                       # en background            ║
# ║  wget -i urls.txt                  # desde lista              ║
# ║                                                                ║
# ║  WGET — MIRROR:                                               ║
# ║  wget -r -l 2 URL                  # recursivo 2 niveles      ║
# ║  wget -m -k -p -np URL            # mirror para offline      ║
# ║  wget -r -A "*.pdf" URL           # solo PDFs                ║
# ║                                                                ║
# ║  WGET — DIAGNÓSTICO:                                          ║
# ║  wget -S URL -O /dev/null          # ver headers              ║
# ║  wget --spider URL                 # verificar sin descargar  ║
# ║                                                                ║
# ╚══════════════════════════════════════════════════════════════════╝
```

---

## 15. Ejercicios

### Ejercicio 1: medir rendimiento web con curl

**Contexto**: un sitio web responde lento y necesitas identificar dónde
está el cuello de botella.

**Tareas**:

1. Crea un archivo de formato `-w` que muestre: DNS, TCP connect, TLS
   handshake, TTFB, total, código HTTP, tamaño y velocidad
2. Mide los tiempos contra 3 sitios web diferentes (uno local, uno
   nacional, uno internacional)
3. Ejecuta 10 mediciones contra cada sitio y calcula promedios
4. Identifica en cuál de las fases (DNS, TCP, TLS, server processing)
   está la mayor parte del tiempo para cada sitio
5. Compara HTTP vs HTTPS del mismo sitio — ¿cuánto tiempo añade TLS?

**Pistas**:
- `time_appconnect - time_connect` = tiempo del handshake TLS
- `time_starttransfer - time_appconnect` = tiempo del servidor procesando
- Para promedios, puedes usar `awk` sobre la salida de un loop
- localhost debería tener DNS ~0 y TCP ~0

> **Pregunta de reflexión**: el TTFB (Time To First Byte) incluye DNS +
> TCP + TLS + server processing. Si el TTFB es 500ms, ¿siempre es culpa
> del servidor? ¿Cómo distinguirías un servidor lento de una conexión
> de red lenta usando solo los tiempos de curl?

---

### Ejercicio 2: verificar configuración de un servidor web

**Contexto**: acabas de configurar Nginx en un servidor y necesitas
verificar que todo funciona correctamente.

**Tareas**:

1. Verifica que HTTP redirige a HTTPS (`curl -sI http://...`)
2. Verifica que el certificado TLS es válido y no está expirado
3. Verifica que no se acepta TLS 1.0 ni TLS 1.1
4. Verifica las cabeceras de seguridad:
   - `Strict-Transport-Security` (HSTS)
   - `X-Content-Type-Options: nosniff`
   - `X-Frame-Options`
5. Verifica que las páginas de error personalizadas funcionan (404, 500)
6. Verifica el health check endpoint
7. Usa `--resolve` para probar un vhost antes de cambiar DNS

**Pistas**:
- `curl --tlsv1.0 --tls-max 1.0` debe fallar si TLS 1.0 está deshabilitado
- `curl -sI` muestra las cabeceras de seguridad
- `curl -sf -o /dev/null -w "%{http_code}"` para verificar códigos

> **Pregunta de reflexión**: ¿por qué es importante verificar estas
> cabeceras de seguridad (HSTS, X-Frame-Options) y no solo que "la página
> carga"? ¿Qué ataques previene cada cabecera?

---

### Ejercicio 3: descargar documentación para uso offline

**Contexto**: necesitas tener disponible la documentación de una herramienta
offline (en un servidor sin Internet o para un viaje).

**Tareas**:

1. Usa wget para hacer un mirror local de la documentación de un proyecto
   (ej. una sección específica de docs de un proyecto open source)
2. Configura las opciones para:
   - Limitar la recursión a 3 niveles
   - Descargar solo dentro del path `/docs/`
   - Incluir CSS, imágenes y JS necesarios
   - Convertir enlaces para navegación offline
   - No seguir enlaces externos al dominio
   - Esperar 1 segundo entre peticiones (cortesía)
3. Verifica que puedes navegar la documentación abriendo el archivo
   index.html local
4. ¿Cuánto espacio ocupa? ¿Cuántos archivos se descargaron?

**Pistas**:
- `wget -m -k -p -np -l 3 --wait=1 -I /docs/ URL`
- `-k` convierte los enlaces para que funcionen offline
- `-p` descarga los recursos necesarios (CSS, imágenes)
- Verifica con `du -sh` y `find . -type f | wc -l`

> **Pregunta de reflexión**: wget respeta el archivo `robots.txt` por
> defecto. ¿Esto afecta a la descarga de documentación? ¿Cuándo es
> apropiado usar `--no-robots` y cuándo es mejor respetar robots.txt?
> ¿Hay diferencia ética entre hacer mirror de documentación pública y
> scraping agresivo?

---

> **Siguiente capítulo**: C07 — Redes en Docker: T01 — Bridge networking avanzado
