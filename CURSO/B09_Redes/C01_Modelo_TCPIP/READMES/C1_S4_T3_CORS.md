# CORS (Cross-Origin Resource Sharing)

## Índice
1. [Same-Origin Policy](#same-origin-policy)
2. [Qué es CORS](#qué-es-cors)
3. [Peticiones simples](#peticiones-simples)
4. [Preflight requests](#preflight-requests)
5. [Cabeceras CORS](#cabeceras-cors)
6. [Credenciales en cross-origin](#credenciales-en-cross-origin)
7. [CORS y WebAssembly](#cors-y-webassembly)
8. [Configuración en servidores](#configuración-en-servidores)
9. [Depuración de errores CORS](#depuración-de-errores-cors)
10. [Errores comunes](#errores-comunes)
11. [Cheatsheet](#cheatsheet)
12. [Ejercicios](#ejercicios)

---

## Same-Origin Policy

Antes de entender CORS, hay que entender qué problema resuelve. La **Same-Origin Policy** (SOP) es una restricción de seguridad de los navegadores que impide que JavaScript en una página acceda a recursos de otro origen.

### ¿Qué es un "origin"?

Un origin se define por la combinación de **esquema + host + puerto**:

```
https://www.example.com:443/path/page.html
──────   ───────────────  ───
  │            │            │
esquema      host         puerto
└────────────────────────────┘
        = ORIGIN
```

```
┌──────────────────────────────────────────────────────────────┐
│                 ¿Mismo origin?                               │
│                                                              │
│  Página:  https://www.example.com/app                        │
│                                                              │
│  URL destino                           ¿Mismo origin?       │
│  ─────────────────────────────────     ──────────────        │
│  https://www.example.com/api/data      ✅ Sí (misma base)   │
│  https://www.example.com:443/other     ✅ Sí (443 = default)│
│  http://www.example.com/api            ❌ No (http ≠ https) │
│  https://api.example.com/data          ❌ No (subdominio ≠) │
│  https://www.example.com:8080/api      ❌ No (puerto ≠)     │
│  https://www.other.com/api             ❌ No (host ≠)       │
└──────────────────────────────────────────────────────────────┘
```

### ¿Por qué existe la SOP?

Sin esta restricción, un sitio malicioso podría:

```
┌──────────────────────────────────────────────────────────────┐
│              Ataque sin Same-Origin Policy                   │
│                                                              │
│  1. El usuario está logueado en bank.com (tiene cookie)      │
│                                                              │
│  2. Visita evil.com, que contiene:                           │
│     fetch("https://bank.com/api/transfer", {                 │
│       method: "POST",                                        │
│       body: '{"to": "evil", "amount": 10000}'               │
│     })                                                       │
│                                                              │
│  3. Sin SOP: el navegador envía la cookie de bank.com        │
│     automáticamente → la transferencia se ejecuta            │
│                                                              │
│  4. Con SOP: el navegador BLOQUEA la respuesta               │
│     (y en muchos casos, ni siquiera envía la petición)       │
└──────────────────────────────────────────────────────────────┘
```

### ¿Qué bloquea exactamente la SOP?

La SOP se aplica **solo en el navegador** y **solo a JavaScript**:

| Recurso | ¿Bloqueado por SOP? | Motivo |
|---------|:-------------------:|--------|
| `<img src="otro-origin">` | No | Imágenes cross-origin siempre permitidas |
| `<script src="otro-origin">` | No | Scripts cross-origin siempre permitidos |
| `<link href="otro-origin">` | No | CSS cross-origin siempre permitido |
| `<form action="otro-origin">` | No | Formularios envían datos sin restricción |
| `fetch("otro-origin")` | **Sí** | JavaScript no puede leer la respuesta |
| `XMLHttpRequest` a otro origin | **Sí** | Igual que fetch |

> **Concepto clave**: la SOP no impide que el navegador *envíe* la petición en todos los casos — impide que JavaScript *lea* la respuesta. Esta distinción es fundamental para entender CORS.

---

## Qué es CORS

CORS es un mecanismo que permite a los servidores **relajar** la Same-Origin Policy de forma controlada. El servidor indica, mediante cabeceras específicas, qué orígenes pueden acceder a sus recursos.

```
┌──────────────────────────────────────────────────────────────┐
│                    Flujo CORS básico                         │
│                                                              │
│  JS en https://app.example.com                               │
│                                                              │
│  fetch("https://api.example.com/data")                       │
│           │                                                  │
│           ▼                                                  │
│  ┌─────────────────┐                                         │
│  │   Navegador     │  "El origin de JS es app.example.com"   │
│  │                 │  "El destino es api.example.com"         │
│  │  ¿Mismo origin? │  "→ Son DIFERENTES → aplica CORS"       │
│  └────────┬────────┘                                         │
│           │                                                  │
│           ▼  Envía petición con:                             │
│              Origin: https://app.example.com                 │
│           │                                                  │
│           ▼                                                  │
│  ┌─────────────────────┐                                     │
│  │ api.example.com     │                                     │
│  │                     │  Responde con:                       │
│  │ Access-Control-     │  Access-Control-Allow-Origin:        │
│  │ Allow-Origin:       │    https://app.example.com          │
│  │ https://app...      │                                     │
│  └─────────────────────┘                                     │
│           │                                                  │
│           ▼                                                  │
│  ┌─────────────────┐                                         │
│  │   Navegador     │  "El servidor permite este origin"      │
│  │                 │  "→ le entrego la respuesta a JS"       │
│  └─────────────────┘                                         │
└──────────────────────────────────────────────────────────────┘
```

CORS no es un mecanismo de seguridad del servidor — es una **instrucción del servidor al navegador** sobre qué está permitido. `curl`, `wget`, y cualquier cliente no-navegador ignoran CORS completamente.

---

## Peticiones simples

No todas las peticiones cross-origin requieren un paso extra. Las **peticiones simples** se envían directamente y el navegador evalúa las cabeceras CORS en la respuesta.

Una petición es "simple" si cumple **todas** estas condiciones:

```
┌──────────────────────────────────────────────────────────────┐
│              Condiciones para petición simple                 │
│                                                              │
│  1. Método: GET, HEAD, o POST                                │
│                                                              │
│  2. Solo cabeceras "safe" (CORS-safelisted):                 │
│     Accept, Accept-Language, Content-Language,                │
│     Content-Type (con restricciones)                         │
│                                                              │
│  3. Content-Type solo puede ser:                             │
│     • application/x-www-form-urlencoded                      │
│     • multipart/form-data                                    │
│     • text/plain                                             │
│                                                              │
│  ⚠ application/json NO es un Content-Type "simple"          │
│    → Una petición POST con JSON SIEMPRE dispara preflight    │
└──────────────────────────────────────────────────────────────┘
```

### Flujo de una petición simple

```
┌─────────────────────────────────────────────────────────────┐
│  JS: fetch("https://api.example.com/public")                │
│                                                              │
│  1. Navegador envía directamente:                            │
│     GET /public HTTP/1.1                                     │
│     Host: api.example.com                                    │
│     Origin: https://app.example.com    ← añadida por browser│
│                                                              │
│  2. Servidor responde:                                       │
│     HTTP/1.1 200 OK                                          │
│     Access-Control-Allow-Origin: https://app.example.com     │
│     Content-Type: application/json                           │
│     {"data": "..."}                                          │
│                                                              │
│  3. Navegador comprueba:                                     │
│     ¿Origin coincide con Access-Control-Allow-Origin?        │
│     Sí → JS recibe la respuesta                              │
│     No → JS recibe un error de CORS (respuesta bloqueada)    │
│                                                              │
│  IMPORTANTE: el servidor PROCESÓ la petición y envió la      │
│  respuesta. CORS solo controla si JS puede LEERLA.           │
└─────────────────────────────────────────────────────────────┘
```

---

## Preflight requests

Cuando una petición **no es simple**, el navegador envía primero una petición `OPTIONS` automática (el "preflight") para preguntar al servidor si permite esa operación:

```
┌──────────────────────────────────────────────────────────────┐
│              Flujo con preflight                             │
│                                                              │
│  JS: fetch("https://api.example.com/users", {                │
│    method: "POST",                                           │
│    headers: {"Content-Type": "application/json"},  ← trigger│
│    body: JSON.stringify({name: "Ana"})                        │
│  })                                                          │
│                                                              │
│  ┌─── Paso 1: PREFLIGHT ─────────────────────────────────┐  │
│  │                                                        │  │
│  │  OPTIONS /users HTTP/1.1                               │  │
│  │  Host: api.example.com                                 │  │
│  │  Origin: https://app.example.com                       │  │
│  │  Access-Control-Request-Method: POST                   │  │
│  │  Access-Control-Request-Headers: Content-Type          │  │
│  │                                                        │  │
│  │  → "¿Puedo hacer POST con Content-Type: json?"        │  │
│  │                                                        │  │
│  │  HTTP/1.1 204 No Content                               │  │
│  │  Access-Control-Allow-Origin: https://app.example.com  │  │
│  │  Access-Control-Allow-Methods: GET, POST, PUT, DELETE  │  │
│  │  Access-Control-Allow-Headers: Content-Type, Auth...   │  │
│  │  Access-Control-Max-Age: 86400                         │  │
│  │                                                        │  │
│  │  → "Sí, puedes."                                       │  │
│  └────────────────────────────────────────────────────────┘  │
│                                                              │
│  ┌─── Paso 2: PETICIÓN REAL ─────────────────────────────┐  │
│  │                                                        │  │
│  │  POST /users HTTP/1.1                                  │  │
│  │  Host: api.example.com                                 │  │
│  │  Origin: https://app.example.com                       │  │
│  │  Content-Type: application/json                        │  │
│  │  {"name": "Ana"}                                       │  │
│  │                                                        │  │
│  │  HTTP/1.1 201 Created                                  │  │
│  │  Access-Control-Allow-Origin: https://app.example.com  │  │
│  │  {"id": 42, "name": "Ana"}                             │  │
│  └────────────────────────────────────────────────────────┘  │
└──────────────────────────────────────────────────────────────┘
```

### ¿Qué dispara un preflight?

```
┌──────────────────────────────────────────────────────────┐
│  Dispara preflight si cumple CUALQUIERA de estas:        │
│                                                          │
│  • Método distinto de GET, HEAD, POST                    │
│    (ej. PUT, DELETE, PATCH)                               │
│                                                          │
│  • Content-Type distinto de los tres "safe":             │
│    (ej. application/json)                                │
│                                                          │
│  • Cabeceras personalizadas                              │
│    (ej. Authorization, X-Request-ID)                     │
│                                                          │
│  • ReadableStream en el body                             │
└──────────────────────────────────────────────────────────┘
```

### Caché del preflight

La cabecera `Access-Control-Max-Age` indica por cuántos segundos el navegador puede cachear la respuesta del preflight:

```
Access-Control-Max-Age: 86400    ← 24 horas
```

Sin esta cabecera, el navegador envía un preflight **antes de cada petición** que lo requiera. Con un valor alto, solo envía un preflight y reutiliza el permiso:

```
┌──────────────────────────────────────────────────────────┐
│  Sin Max-Age:                                            │
│    OPTIONS → POST                                        │
│    OPTIONS → PUT                                         │
│    OPTIONS → DELETE     ← 3 preflights = 3 RTTs extra    │
│                                                          │
│  Con Max-Age: 86400:                                     │
│    OPTIONS → POST                                        │
│    PUT                  ← sin preflight (cacheado)       │
│    DELETE               ← sin preflight (cacheado)       │
└──────────────────────────────────────────────────────────┘
```

> **Límite de navegadores**: Chrome limita `Max-Age` a 7200 (2 horas) independientemente del valor del servidor. Firefox lo limita a 86400 (24 horas).

---

## Cabeceras CORS

### Cabeceras de respuesta (las envía el servidor)

| Cabecera | Descripción | Ejemplo |
|----------|------------|---------|
| `Access-Control-Allow-Origin` | Qué origins pueden acceder | `https://app.example.com` o `*` |
| `Access-Control-Allow-Methods` | Qué métodos permite (solo en preflight) | `GET, POST, PUT, DELETE` |
| `Access-Control-Allow-Headers` | Qué cabeceras permite (solo en preflight) | `Content-Type, Authorization` |
| `Access-Control-Max-Age` | Segundos que se cachea el preflight | `86400` |
| `Access-Control-Allow-Credentials` | Permitir cookies/auth cross-origin | `true` |
| `Access-Control-Expose-Headers` | Qué cabeceras puede leer JS | `X-Request-ID, X-Total-Count` |

### Cabeceras de petición (las envía el navegador automáticamente)

| Cabecera | Descripción |
|----------|------------|
| `Origin` | Origin del JavaScript que hace la petición |
| `Access-Control-Request-Method` | Método que se usará (solo en preflight) |
| `Access-Control-Request-Headers` | Cabeceras que se enviarán (solo en preflight) |

### El wildcard `*`

```
# Permite CUALQUIER origin
Access-Control-Allow-Origin: *
```

Restricciones del wildcard:

```
┌──────────────────────────────────────────────────────────────┐
│  Access-Control-Allow-Origin: *                              │
│                                                              │
│  ✅ Funciona para peticiones sin credenciales                │
│  ❌ NO funciona con credentials: "include"                   │
│     → Si necesitas cookies, DEBES especificar el origin      │
│        exacto, no *                                          │
│                                                              │
│  Access-Control-Allow-Headers: *                             │
│  ✅ Permite cualquier cabecera (excepto Authorization)       │
│  ⚠  Authorization SIEMPRE debe listarse explícitamente      │
│                                                              │
│  Access-Control-Allow-Methods: *                             │
│  ✅ Permite cualquier método                                 │
│                                                              │
│  Access-Control-Expose-Headers: *                            │
│  ✅ Expone cualquier cabecera a JS (excepto Set-Cookie)      │
└──────────────────────────────────────────────────────────────┘
```

### Expose-Headers: qué puede leer JS

Por defecto, JavaScript solo puede leer un conjunto limitado de cabeceras de la respuesta (los "CORS-safelisted response headers"):

```
# JS puede leer estas SIN Expose-Headers:
Cache-Control, Content-Language, Content-Length,
Content-Type, Expires, Pragma

# Para cualquier otra, el servidor debe exponerla explícitamente:
Access-Control-Expose-Headers: X-Request-ID, X-Total-Count

# Ahora JS puede hacer:
response.headers.get("X-Total-Count")    // funciona
```

---

## Credenciales en cross-origin

Por defecto, las peticiones cross-origin **no incluyen cookies** ni cabeceras de autenticación. Para habilitarlas, tanto el cliente como el servidor deben optar explícitamente:

```
┌──────────────────────────────────────────────────────────────┐
│               CORS con credenciales                          │
│                                                              │
│  CLIENTE (JavaScript):                                       │
│  fetch("https://api.example.com/me", {                       │
│    credentials: "include"     ← "envía mis cookies"          │
│  })                                                          │
│                                                              │
│  SERVIDOR (respuesta):                                       │
│  Access-Control-Allow-Origin: https://app.example.com        │
│  Access-Control-Allow-Credentials: true                      │
│                                                              │
│  ⚠ REGLAS cuando credentials = true:                        │
│                                                              │
│  1. Allow-Origin NO puede ser *                              │
│     → Debe ser el origin exacto                              │
│                                                              │
│  2. Allow-Headers NO puede ser *                             │
│     → Debe listar cada cabecera explícitamente               │
│                                                              │
│  3. Allow-Methods NO puede ser *                             │
│     → Debe listar cada método explícitamente                 │
│                                                              │
│  4. Expose-Headers NO puede ser *                            │
│     → Debe listar cada cabecera explícitamente               │
└──────────────────────────────────────────────────────────────┘
```

### Patrón dinámico para Allow-Origin

Si tu API tiene múltiples clientes legítimos, no puedes usar `*` con credenciales. La solución es reflejar el Origin si está en una lista permitida:

```
┌───────────────────────────────────────────────────────────┐
│  Lógica del servidor (pseudocódigo):                      │
│                                                           │
│  allowed = ["https://app.example.com",                    │
│             "https://admin.example.com"]                   │
│                                                           │
│  origin = request.headers["Origin"]                       │
│                                                           │
│  if origin in allowed:                                    │
│      response.headers["Access-Control-Allow-Origin"]      │
│        = origin                    ← reflejar el origin   │
│      response.headers["Vary"] = "Origin"   ← IMPORTANTE  │
│  else:                                                    │
│      // no incluir la cabecera → el navegador bloquea     │
└───────────────────────────────────────────────────────────┘
```

> **Vary: Origin** es obligatorio cuando `Allow-Origin` cambia según la petición. Sin él, un CDN podría cachear la respuesta para `app.example.com` y servirla a `admin.example.com`.

---

## CORS y WebAssembly

CORS tiene relevancia directa para WebAssembly (WASM) en el navegador, que es parte de este curso:

### Carga de módulos WASM

```javascript
// Cargar un .wasm desde otro origin
const response = await fetch("https://cdn.example.com/app.wasm");
const bytes = await response.arrayBuffer();
const module = await WebAssembly.instantiate(bytes);
```

Si el `.wasm` está en otro origin, se aplican las mismas reglas CORS. El CDN debe responder con:

```
Access-Control-Allow-Origin: https://app.example.com
```

### WASM y peticiones de red

Si tu módulo WASM (compilado desde Rust con `wasm-bindgen`) hace peticiones HTTP, estas se canalizan a través de `fetch()` del navegador — y están sujetas a CORS:

```
┌──────────────────────────────────────────────────────────────┐
│              WASM en el navegador                            │
│                                                              │
│  ┌───────────┐     fetch()       ┌──────────────┐           │
│  │ Rust WASM │ ──────────────►   │ API server   │           │
│  │ (browser) │   (via JS glue)   │              │           │
│  └───────────┘                   └──────────────┘           │
│       │                                │                    │
│       │  El código WASM NO hace        │                    │
│       │  TCP directo — usa fetch()     │                    │
│       │  del navegador                 │                    │
│       │                                │                    │
│       └── CORS aplica exactamente ─────┘                    │
│           igual que para JS                                 │
│                                                              │
│  Fuera del navegador (ej. WASI):                            │
│    WASM puede hacer TCP directo → CORS NO aplica            │
└──────────────────────────────────────────────────────────────┘
```

### SharedArrayBuffer y COOP/COEP

Funcionalidades avanzadas de WASM como `SharedArrayBuffer` (memoria compartida entre threads) requieren cabeceras de aislamiento adicionales:

```
# El servidor debe enviar:
Cross-Origin-Opener-Policy: same-origin        (COOP)
Cross-Origin-Embedder-Policy: require-corp      (COEP)

# Sin estas cabeceras, SharedArrayBuffer no está disponible
# (restricción post-Spectre/Meltdown)
```

Esto afecta directamente si tu aplicación Rust+WASM puede usar threads (via `wasm-bindgen-rayon` o similar).

---

## Configuración en servidores

### nginx

```nginx
# Configuración CORS en nginx
server {
    listen 80;
    server_name api.example.com;

    # Preflight
    if ($request_method = 'OPTIONS') {
        add_header 'Access-Control-Allow-Origin' 'https://app.example.com';
        add_header 'Access-Control-Allow-Methods' 'GET, POST, PUT, DELETE, OPTIONS';
        add_header 'Access-Control-Allow-Headers' 'Content-Type, Authorization';
        add_header 'Access-Control-Max-Age' '86400';
        add_header 'Content-Length' '0';
        return 204;
    }

    # Respuestas normales
    add_header 'Access-Control-Allow-Origin' 'https://app.example.com' always;

    location /api/ {
        proxy_pass http://backend:3000;
    }
}
```

> **`always`**: sin este parámetro, `add_header` en nginx solo se aplica a respuestas 2xx y 3xx. Con `always`, también se aplica a errores (4xx, 5xx), donde las cabeceras CORS también son necesarias para que JS pueda leer el error.

### Apache

```apache
<VirtualHost *:80>
    ServerName api.example.com

    Header always set Access-Control-Allow-Origin "https://app.example.com"
    Header always set Access-Control-Allow-Methods "GET, POST, PUT, DELETE, OPTIONS"
    Header always set Access-Control-Allow-Headers "Content-Type, Authorization"

    # Manejar preflight
    RewriteEngine On
    RewriteCond %{REQUEST_METHOD} OPTIONS
    RewriteRule ^(.*)$ $1 [R=204,L]
</VirtualHost>
```

### ¿Dónde configurar CORS?

```
┌───────────────────────────────────────────────────────────┐
│  Opción 1: En el reverse proxy (nginx/Apache)             │
│  + Centralizado, una sola configuración                   │
│  + No depende del framework de la aplicación              │
│  + Funciona igual para todos los backends                 │
│  - Menos flexible para lógica compleja                    │
│                                                           │
│  Opción 2: En la aplicación (Express, Flask, etc.)        │
│  + Lógica dinámica (whitelist por base de datos)          │
│  + Más control por endpoint                               │
│  - Cada aplicación debe configurarlo                      │
│                                                           │
│  Recomendación:                                           │
│  Reverse proxy para configuración global                  │
│  Aplicación para casos especiales por endpoint            │
│  NUNCA en ambos lugares (cabeceras duplicadas = errores)  │
└───────────────────────────────────────────────────────────┘
```

---

## Depuración de errores CORS

### Mensaje típico en la consola del navegador

```
Access to fetch at 'https://api.example.com/data' from origin
'https://app.example.com' has been blocked by CORS policy:
No 'Access-Control-Allow-Origin' header is present on the
requested resource.
```

### Proceso de diagnóstico

```
┌──────────────────────────────────────────────────────────────┐
│            Diagnóstico de errores CORS                       │
│                                                              │
│  1. ¿Funciona con curl?                                      │
│     curl -v https://api.example.com/data                     │
│     → Si falla: el problema NO es CORS, es el servidor       │
│     → Si funciona: el problema ES CORS (sigue al paso 2)    │
│                                                              │
│  2. ¿El servidor envía Access-Control-Allow-Origin?          │
│     curl -v -H "Origin: https://app.example.com" \           │
│       https://api.example.com/data                           │
│     → Busca: Access-Control-Allow-Origin en la respuesta     │
│     → Si falta: configurar en el servidor                    │
│                                                              │
│  3. ¿Es un problema de preflight?                            │
│     curl -v -X OPTIONS \                                     │
│       -H "Origin: https://app.example.com" \                 │
│       -H "Access-Control-Request-Method: POST" \             │
│       -H "Access-Control-Request-Headers: Content-Type" \    │
│       https://api.example.com/data                           │
│     → ¿Responde 204 con las cabeceras Allow-*?               │
│     → Si 405/404: el servidor no maneja OPTIONS              │
│                                                              │
│  4. ¿Es un problema de credenciales?                         │
│     → ¿credentials: "include" en fetch?                      │
│     → ¿Allow-Origin es * en vez del origin exacto?           │
│     → ¿Falta Allow-Credentials: true?                        │
└──────────────────────────────────────────────────────────────┘
```

### curl para simular peticiones del navegador

```bash
# Simular petición simple con Origin
curl -v -H "Origin: https://app.example.com" \
  https://api.example.com/data

# Simular preflight
curl -v -X OPTIONS \
  -H "Origin: https://app.example.com" \
  -H "Access-Control-Request-Method: DELETE" \
  -H "Access-Control-Request-Headers: Authorization" \
  https://api.example.com/users/42

# Verificar que la respuesta incluye las cabeceras necesarias
curl -sI -H "Origin: https://app.example.com" \
  https://api.example.com/data | grep -i "access-control"
```

---

## Errores comunes

### 1. Usar `Access-Control-Allow-Origin: *` con credenciales

```javascript
// Cliente
fetch(url, { credentials: "include" })
```

```
// Servidor — INCORRECTO
Access-Control-Allow-Origin: *
Access-Control-Allow-Credentials: true
// → El navegador RECHAZA esto: * no es compatible con credentials

// Servidor — CORRECTO
Access-Control-Allow-Origin: https://app.example.com
Access-Control-Allow-Credentials: true
```

### 2. No manejar OPTIONS en el servidor

```
# El navegador envía:
OPTIONS /api/users HTTP/1.1

# Servidor sin manejo de OPTIONS responde:
405 Method Not Allowed
# o peor:
404 Not Found

# → El navegador nunca envía la petición real
# → JS recibe: "CORS preflight response was not successful"

# Solución: configurar el servidor para responder 204 a OPTIONS
# con las cabeceras Access-Control-Allow-* apropiadas
```

### 3. Configurar CORS en dos lugares

```
# nginx añade:
Access-Control-Allow-Origin: https://app.example.com

# La aplicación TAMBIÉN añade:
Access-Control-Allow-Origin: https://app.example.com

# Resultado en la respuesta:
Access-Control-Allow-Origin: https://app.example.com
Access-Control-Allow-Origin: https://app.example.com   ← DUPLICADA

# → El navegador ve DOS cabeceras y RECHAZA la petición
# → "The 'Access-Control-Allow-Origin' header contains
#    multiple values, but only one is allowed"
```

### 4. Olvidar `Vary: Origin` con Allow-Origin dinámico

```
# Si Allow-Origin cambia según el Origin del request:
# Petición 1 (Origin: https://app.example.com)
Access-Control-Allow-Origin: https://app.example.com

# Un CDN cachea esta respuesta
# Petición 2 (Origin: https://admin.example.com)
# El CDN sirve la respuesta cacheada con el origin incorrecto
# → CORS falla para admin.example.com

# Solución: siempre incluir Vary: Origin
Vary: Origin
```

### 5. Pensar que CORS protege tu API

```
┌───────────────────────────────────────────────────────────┐
│  CORS NO es un mecanismo de seguridad del servidor.       │
│                                                           │
│  CORS solo lo aplica el NAVEGADOR.                        │
│                                                           │
│  Cualquiera puede hacer:                                  │
│  curl https://api.example.com/data                        │
│  → Funciona perfectamente, sin importar CORS              │
│                                                           │
│  Proteger tu API requiere:                                │
│  • Autenticación (tokens, API keys)                       │
│  • Autorización (permisos por usuario/rol)                │
│  • Rate limiting                                          │
│  • Validación de input                                    │
│                                                           │
│  CORS protege a los USUARIOS del navegador, no al API.    │
└───────────────────────────────────────────────────────────┘
```

---

## Cheatsheet

```
┌──────────────────────────────────────────────────────────────┐
│                     CORS Cheatsheet                          │
├──────────────────────────────────────────────────────────────┤
│                                                              │
│  ORIGIN = esquema + host + puerto                            │
│    https://app.example.com:443                               │
│                                                              │
│  SAME-ORIGIN POLICY (SOP):                                   │
│    JS en origin A no puede leer respuestas de origin B       │
│    Solo aplica en navegadores, no en curl/wget/etc.          │
│                                                              │
│  PETICIÓN SIMPLE (sin preflight):                            │
│    Método: GET, HEAD, POST                                   │
│    Content-Type: urlencoded, form-data, text/plain           │
│    Sin cabeceras custom                                      │
│                                                              │
│  PREFLIGHT (OPTIONS automático):                             │
│    Se dispara si: PUT/DELETE/PATCH, JSON, Authorization,     │
│    o cualquier cabecera/content-type no "safe"               │
│                                                              │
│  CABECERAS DEL SERVIDOR:                                     │
│    Allow-Origin: origin | *       quién puede acceder        │
│    Allow-Methods: GET, POST...    métodos permitidos         │
│    Allow-Headers: Content-Type    cabeceras permitidas       │
│    Allow-Credentials: true        permitir cookies           │
│    Expose-Headers: X-Custom       cabeceras legibles por JS  │
│    Max-Age: 86400                 caché del preflight (seg)  │
│                                                              │
│  CON CREDENCIALES (cookies/auth):                            │
│    Cliente: credentials: "include"                           │
│    Servidor: Allow-Origin = origin exacto (NO *)             │
│    Servidor: Allow-Credentials: true                         │
│    + Vary: Origin                                            │
│                                                              │
│  DIAGNÓSTICO:                                                │
│    1. ¿curl funciona? → no es CORS, es el server             │
│    2. ¿Tiene Allow-Origin? → configurar servidor             │
│    3. ¿OPTIONS devuelve 204? → manejar preflight             │
│    4. ¿* + credentials? → usar origin exacto                 │
│    5. ¿Cabecera duplicada? → solo un lugar (proxy O app)     │
│                                                              │
│  SIMULAR PREFLIGHT CON CURL:                                 │
│    curl -X OPTIONS \                                         │
│      -H "Origin: https://app.example.com" \                  │
│      -H "Access-Control-Request-Method: POST" \              │
│      -H "Access-Control-Request-Headers: Content-Type" \     │
│      https://api.example.com/endpoint                        │
│                                                              │
│  WASM EN NAVEGADOR:                                          │
│    fetch() desde WASM → mismas reglas CORS que JS            │
│    Cargar .wasm cross-origin → necesita Allow-Origin         │
│    SharedArrayBuffer → necesita COOP + COEP                  │
│                                                              │
│  RECUERDA: CORS protege al USUARIO, no al servidor.          │
│  curl ignora CORS. Protege tu API con authn/authz.           │
│                                                              │
└──────────────────────────────────────────────────────────────┘
```

---

## Ejercicios

### Ejercicio 1: Clasificar peticiones

Para cada una de estas peticiones cross-origin hechas desde JavaScript, determina si el navegador enviará un **preflight** o si será una **petición simple**. Justifica tu respuesta:

```
A) fetch("https://api.example.com/search?q=linux")

B) fetch("https://api.example.com/users", {
     method: "POST",
     headers: {"Content-Type": "application/x-www-form-urlencoded"},
     body: "name=Ana&role=admin"
   })

C) fetch("https://api.example.com/users", {
     method: "POST",
     headers: {"Content-Type": "application/json"},
     body: JSON.stringify({name: "Ana"})
   })

D) fetch("https://api.example.com/users/42", {
     method: "DELETE"
   })

E) fetch("https://api.example.com/data", {
     headers: {"Authorization": "Bearer token123"}
   })

F) fetch("https://api.example.com/public", {
     method: "GET",
     credentials: "include"
   })
```

**Pregunta de reflexión**: ¿Por qué la especificación CORS decidió que `application/json` requiere preflight pero `application/x-www-form-urlencoded` no? ¿Qué tiene que ver con los formularios HTML clásicos?

---

### Ejercicio 2: Diagnosticar errores CORS

Un desarrollador frontend tiene una aplicación en `https://dashboard.myapp.com` que llama a una API en `https://api.myapp.com`. Describe qué error de CORS obtendrá en cada escenario y cómo solucionarlo:

**Escenario A**: La API no envía ninguna cabecera CORS.

**Escenario B**: La API responde con:
```
Access-Control-Allow-Origin: *
```
Y el frontend usa:
```javascript
fetch(url, { credentials: "include" })
```

**Escenario C**: nginx y la aplicación Express **ambos** añaden:
```
Access-Control-Allow-Origin: https://dashboard.myapp.com
```

**Escenario D**: La API responde correctamente a GET, pero cuando el frontend envía un POST con JSON, recibe un error CORS. Al investigar, ves que el servidor devuelve `405 Method Not Allowed` para peticiones OPTIONS.

**Escenario E**: Todo funciona excepto que el frontend no puede leer la cabecera `X-Total-Count` de la respuesta — `response.headers.get("X-Total-Count")` devuelve `null`.

**Pregunta de reflexión**: Si el frontend y la API están en `dashboard.myapp.com` y `api.myapp.com` respectivamente, ¿son el mismo origin? Si pusieras ambos detrás de un solo dominio con un reverse proxy (`myapp.com/dashboard` y `myapp.com/api`), ¿necesitarías CORS?

---

### Ejercicio 3: Configurar CORS para una aplicación WASM

Estás desarrollando una aplicación Rust compilada a WASM que se sirve desde `https://app.rustproject.dev`. La aplicación necesita:

1. Cargar el archivo `.wasm` desde un CDN en `https://cdn.rustproject.dev`.
2. Hacer peticiones GET y POST (con JSON) a una API en `https://api.rustproject.dev`.
3. Enviar un token JWT en la cabecera `Authorization`.
4. Leer la cabecera `X-Request-ID` de las respuestas para logging.

Escribe la configuración nginx del CDN y de la API que satisfaga todos estos requisitos. Para cada cabecera que incluyas, explica por qué es necesaria.

Después, responde:
- ¿Las peticiones GET con `Authorization` disparán preflight? ¿Por qué?
- ¿Puedes usar `*` en `Access-Control-Allow-Origin` si envías `Authorization`?
- Si quisieras usar `SharedArrayBuffer` para threading en tu WASM, ¿qué cabeceras adicionales necesitarías en el servidor que sirve el HTML?

**Pregunta de reflexión**: ¿Por qué CORS no aplica cuando tu aplicación Rust+WASM corre fuera del navegador (ej. con Wasmtime o Wasmer)? ¿Qué componente es responsable de hacer cumplir CORS?

---

> **Siguiente tema**: T04 — HTTP/2 y HTTP/3 (multiplexing, server push, QUIC, binary framing, diferencias prácticas con HTTP/1.1)
