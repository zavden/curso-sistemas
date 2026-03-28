# Apache vs Nginx — diferencias de arquitectura

## Índice

1. [Dos filosofías de diseño](#1-dos-filosofías-de-diseño)
2. [Modelos de concurrencia](#2-modelos-de-concurrencia)
3. [Procesamiento de peticiones](#3-procesamiento-de-peticiones)
4. [Configuración y flexibilidad](#4-configuración-y-flexibilidad)
5. [Rendimiento comparado](#5-rendimiento-comparado)
6. [Módulos y extensibilidad](#6-módulos-y-extensibilidad)
7. [Contenido dinámico](#7-contenido-dinámico)
8. [Casos de uso y cuándo elegir cada uno](#8-casos-de-uso-y-cuándo-elegir-cada-uno)
9. [Apache y Nginx juntos](#9-apache-y-nginx-juntos)
10. [Errores comunes](#10-errores-comunes)
11. [Cheatsheet](#11-cheatsheet)
12. [Ejercicios](#12-ejercicios)

---

## 1. Dos filosofías de diseño

Apache y Nginx resuelven el mismo problema — servir contenido web — pero parten
de premisas diferentes nacidas de épocas distintas de internet.

**Apache (1995)**: Diseñado cuando cada conexión era breve (una petición, una
respuesta, cerrar). El modelo "un proceso por conexión" era natural y
suficiente. La extensibilidad mediante módulos internos era prioritaria.

**Nginx (2004)**: Diseñado para resolver el problema C10K — servir 10,000
conexiones concurrentes. Las conexiones HTTP/1.1 keep-alive y el aumento de
recursos embebidos (CSS, JS, imágenes) por página exigían manejar muchas
conexiones simultáneas con pocos recursos.

```
1995 (Apache)                          2004 (Nginx)
─────────────                          ────────────
1 página = 1 petición                  1 página = 50+ peticiones
Conexiones cortas                      Keep-alive, conexiones largas
Servidores con 32-256 MB RAM           C10K problem
CGI como modelo de extensión           Proxy a backends como modelo
El servidor hace todo                  Separar responsabilidades
```

---

## 2. Modelos de concurrencia

### 2.1 Apache — prefork

El modelo original. Cada conexión es manejada por un proceso dedicado:

```
                Master process
                     │
        ┌────────────┼────────────┐
        │            │            │
    ┌───▼───┐   ┌────▼───┐   ┌───▼───┐
    │Process│   │Process │   │Process│    ← pre-forked (esperando)
    │  idle │   │ client │   │ client│
    │       │   │   A    │   │   B   │
    └───────┘   └────────┘   └───────┘

    100 conexiones = 100 procesos
    Cada proceso: ~10-30 MB de RAM
    100 conexiones × 20 MB = 2 GB de RAM
```

**Ventajas**: Aislamiento total entre conexiones (un crash no afecta a otros),
compatible con módulos no thread-safe (mod_php clásico).

**Desventajas**: Consumo de memoria proporcional a conexiones, overhead de
context switching, no escala más allá de cientos de conexiones concurrentes.

### 2.2 Apache — worker

Modelo híbrido: múltiples procesos, cada uno con múltiples threads:

```
                Master process
                     │
           ┌─────────┼─────────┐
           │                   │
    ┌──────▼──────┐    ┌───────▼─────┐
    │  Process 1  │    │  Process 2  │
    │ ┌──┬──┬──┐  │    │ ┌──┬──┬──┐  │
    │ │T1│T2│T3│  │    │ │T1│T2│T3│  │
    │ └──┴──┴──┘  │    │ └──┴──┴──┘  │
    └─────────────┘    └─────────────┘

    Threads comparten memoria del proceso
    Menos overhead que prefork
    Requiere que los módulos sean thread-safe
```

### 2.3 Apache — event

Evolución del worker. Un thread dedicado (listener) gestiona las conexiones
keep-alive sin bloquear threads de trabajo:

```
                Master process
                     │
           ┌─────────┼─────────┐
           │                   │
    ┌──────▼──────┐    ┌───────▼─────┐
    │  Process 1  │    │  Process 2  │
    │ ┌─────────┐ │    │ ┌─────────┐ │
    │ │Listener │ │    │ │Listener │ │   ← gestiona keep-alive
    │ │ thread  │ │    │ │ thread  │ │
    │ └────┬────┘ │    │ └────┬────┘ │
    │ ┌──┬─┴┬──┐  │    │ ┌──┬─┴┬──┐  │
    │ │T1│T2│T3│  │    │ │T1│T2│T3│  │   ← procesan peticiones
    │ └──┴──┴──┘  │    │ └──┴──┴──┘  │
    └─────────────┘    └─────────────┘

    Conexiones keep-alive no bloquean workers
    Mejor que worker para alto keep-alive
    Sigue requiriendo módulos thread-safe
```

### 2.4 Nginx — event-driven (asíncrono)

Un modelo fundamentalmente diferente. Cada worker es un solo thread que maneja
miles de conexiones mediante un event loop:

```
    Master process (root)
         │
    ┌────┼────┐
    │    │    │
    ▼    ▼    ▼
  ┌────────────┐
  │  Worker 1  │
  │            │
  │  ┌──────────────────────────────────┐
  │  │         Event Loop (epoll)       │
  │  │                                  │
  │  │  ┌─────┐ ┌─────┐ ┌─────┐       │
  │  │  │conn1│ │conn2│ │conn3│ ...    │
  │  │  │ready│ │wait │ │ready│        │
  │  │  └──┬──┘ └─────┘ └──┬──┘       │
  │  │     │                │          │
  │  │  procesar         procesar      │
  │  │  (no-blocking)    (no-blocking) │
  │  │                                  │
  │  │  Si necesita I/O → registrar     │
  │  │  callback, pasar a siguiente     │
  │  └──────────────────────────────────┘
  └────────────┘

  4 workers × miles de conexiones cada uno
  Memoria total: ~50-100 MB para 10,000+ conexiones
  Sin context switching entre threads/procesos
```

**El event loop**:

```
  ┌─────────────────────────────────────┐
  │                                     │
  │    ┌─► epoll_wait() ◄──────────┐   │
  │    │   (espera eventos)        │   │
  │    │                           │   │
  │    │   evento: conn 42 ready   │   │
  │    │          │                │   │
  │    │   ┌──────▼──────┐         │   │
  │    │   │ read request │         │   │
  │    │   └──────┬──────┘         │   │
  │    │          │                │   │
  │    │   ┌──────▼──────┐         │   │
  │    │   │ process     │         │   │
  │    │   │ (non-block) │         │   │
  │    │   └──────┬──────┘         │   │
  │    │          │                │   │
  │    │   ┌──────▼──────┐         │   │
  │    │   │ write resp  │         │   │
  │    │   └──────┬──────┘         │   │
  │    │          │                │   │
  │    └──────────┘                │   │
  │         siguiente evento ──────┘   │
  └─────────────────────────────────────┘
```

La clave: **nunca bloquear**. Si una operación requiere espera (lectura de
disco, respuesta de backend), el worker registra un callback y pasa a la
siguiente conexión. Cuando la operación completa, el kernel notifica vía epoll
y el worker retoma el procesamiento.

---

## 3. Procesamiento de peticiones

### 3.1 Flujo en Apache

```
Petición HTTP entrante
       │
       ▼
  1. Asignar proceso/thread del pool
       │
       ▼
  2. Parsear HTTP
       │
       ▼
  3. Buscar .htaccess en cada directorio (stat() × N)
       │
       ▼
  4. Ejecutar hooks de módulos en orden de fase:
     URI Translation → Access → Auth → Content → Logging
       │
       ▼
  5. Generar respuesta (mod_php ejecuta PHP internamente,
     o mod_proxy reenvía a backend)
       │
       ▼
  6. Enviar respuesta al cliente
       │
       ▼
  7. Si keep-alive: proceso/thread ESPERA (bloqueado)
     Si no: proceso/thread vuelve al pool
```

### 3.2 Flujo en Nginx

```
Petición HTTP entrante
       │
       ▼
  1. Event loop detecta datos disponibles en socket
       │
       ▼
  2. Parsear HTTP (estado mantenido en estructura por conexión)
       │
       ▼
  3. Seleccionar server block → location
     (sin búsqueda de .htaccess, configuración ya en memoria)
       │
       ▼
  4. Procesar según location:
     ├── Archivo estático → sendfile() → respuesta
     └── Proxy → conectar al backend (async) → buffer → respuesta
       │
       ▼
  5. Enviar respuesta (async, non-blocking)
       │
       ▼
  6. Si keep-alive: volver al event loop (SIN bloquear nada)
     La conexión se registra para futuras notificaciones
```

### 3.3 Diferencia clave: el coste de keep-alive

```
Apache (prefork): 200 conexiones keep-alive = 200 procesos bloqueados
                  (cada uno consumiendo ~20 MB, sin hacer trabajo útil)

Apache (event):   200 conexiones keep-alive = gestionadas por listener threads
                  (mejor, pero sigue requiriendo threads dedicados)

Nginx:            200 conexiones keep-alive = 200 entradas en la tabla epoll
                  (consumo: ~2.5 KB cada una, zero CPU cuando están idle)
```

---

## 4. Configuración y flexibilidad

### 4.1 Modelo de configuración

```
Apache                                 Nginx
──────                                 ─────
Configuración distribuida              Configuración centralizada
(.htaccess en cada directorio)         (solo nginx.conf y includes)

Cambios sin reinicio                   Requiere reload
(efecto inmediato)                     (pero es graceful, zero downtime)

Usuarios sin root pueden               Solo el administrador
modificar su espacio web               modifica la configuración

Mayor overhead por petición            Configuración leída una vez,
(leer .htaccess cada vez)              en memoria
```

### 4.2 Expresividad de la configuración

**Apache** usa un modelo declarativo basado en contenedores XML-like con
directivas que se acumulan por fusión:

```apache
<Directory "/var/www/html">
    Options -Indexes
    AllowOverride FileInfo
    Require all granted
</Directory>

<Location "/admin">
    Require ip 10.0.0.0/8
</Location>
```

**Nginx** usa un modelo de bloques anidados con herencia top-down. Es más
simple conceptualmente, pero tiene particularidades (como la no-herencia de
directivas de array):

```nginx
server {
    root /var/www/html;
    autoindex off;

    location / {
        try_files $uri $uri/ =404;
    }

    location /admin/ {
        allow 10.0.0.0/8;
        deny all;
    }
}
```

### 4.3 Reescritura de URLs

| Aspecto                | Apache (mod_rewrite)              | Nginx (rewrite)                |
|------------------------|-----------------------------------|--------------------------------|
| Potencia               | Extremadamente flexible           | Más limitado pero suficiente   |
| Complejidad            | Complejo, difícil de depurar      | Más directo                    |
| Contexto               | VHost, Directory, .htaccess       | server, location               |
| Condicionales          | RewriteCond con variables         | if (limitado), map             |
| Loops                  | Posibles (difíciles de detectar)  | Protegido (máx. 10 ciclos)    |
| Debug                  | `RewriteLog`/`LogLevel rewrite:trace` | `rewrite_log on` + `error_log notice` |

Nginx desaconseja el uso de `if` dentro de `location` porque su comportamiento
es contraintuitivo (no es un condicional real, sino una directiva que crea un
contexto). La documentación oficial lo llama "if is evil":

```nginx
# EVITAR — comportamiento inesperado
location / {
    if ($request_uri ~ "\.php$") {
        # Esto NO es un condicional simple
        # Crea un contexto separado que puede interferir con otras directivas
    }
}

# PREFERIR — map + variable
map $request_uri $is_php {
    ~\.php$  1;
    default  0;
}
```

### 4.4 Equivalencias de directivas

| Función                    | Apache                          | Nginx                          |
|----------------------------|---------------------------------|--------------------------------|
| Raíz de documentos        | `DocumentRoot`                  | `root`                         |
| Archivo por defecto        | `DirectoryIndex`                | `index`                        |
| Redirección                | `Redirect`                      | `return 301`                   |
| Reescritura                | `RewriteRule`                   | `rewrite` / `try_files`        |
| Proxy inverso              | `ProxyPass` + `ProxyPassReverse`| `proxy_pass`                   |
| Alias de ruta              | `Alias`                         | `alias`                        |
| Denegar acceso             | `Require all denied`            | `deny all`                     |
| Control por IP             | `Require ip X`                  | `allow X; deny all;`           |
| Listado de directorio      | `Options Indexes`               | `autoindex on`                 |
| Ocultar versión            | `ServerTokens Prod`             | `server_tokens off`            |
| Páginas de error           | `ErrorDocument 404 /page`       | `error_page 404 /page`         |
| Compresión                 | `mod_deflate`                   | `gzip on`                      |
| Cabeceras                  | `Header set`                    | `add_header`                   |
| Autenticación HTTP         | `AuthType Basic`                | `auth_basic`                   |
| Log de acceso              | `CustomLog`                     | `access_log`                   |
| Log de errores             | `ErrorLog`                      | `error_log`                    |
| Tamaño máximo upload       | `LimitRequestBody`              | `client_max_body_size`         |
| Timeout                    | `TimeOut`                       | `proxy_read_timeout` y otros   |

---

## 5. Rendimiento comparado

### 5.1 Archivos estáticos

Nginx tiene ventaja clara gracias a sendfile, event loop y menor overhead
por conexión:

```
Benchmark típico: archivo de 10 KB, 1000 conexiones concurrentes

Apache (event MPM):
  Requests/sec:  ~8,000-12,000
  Memoria:       ~200-300 MB
  CPU:           Moderado

Nginx:
  Requests/sec:  ~15,000-25,000
  Memoria:       ~50-100 MB
  CPU:           Bajo

(Valores orientativos — dependen enormemente del hardware y configuración)
```

### 5.2 Contenido dinámico

La diferencia se reduce porque el cuello de botella es el backend, no el
servidor web:

```
Benchmark: aplicación PHP (Laravel) con base de datos

Apache + mod_php:
  Requests/sec:  ~500-800
  Nota: PHP embebido en Apache, sin overhead de comunicación

Apache + PHP-FPM:
  Requests/sec:  ~450-750
  Nota: Comunicación vía FastCGI, similar a Nginx

Nginx + PHP-FPM:
  Requests/sec:  ~500-800
  Nota: Misma cadena de procesamiento que Apache + PHP-FPM
```

### 5.3 Bajo carga alta (conexiones concurrentes)

Donde la diferencia arquitectural es definitiva:

```
Conexiones concurrentes:   100     1,000     10,000    50,000

Apache (prefork):
  Memoria:                200 MB   2 GB     imposible  imposible
  Requests/sec:           10,000   8,000    —          —

Apache (event):
  Memoria:                150 MB   500 MB   2 GB       difícil
  Requests/sec:           12,000   10,000   6,000      —

Nginx:
  Memoria:                50 MB    80 MB    150 MB     300 MB
  Requests/sec:           25,000   22,000   18,000     12,000
```

### 5.4 Lo que los benchmarks no muestran

Los benchmarks simples no capturan escenarios reales:

- **Aplicaciones CPU-bound**: El servidor web es irrelevante; el backend domina
- **Caché hit ratio alto**: Ambos servidores con caché configurado son similares
- **Pocos usuarios concurrentes**: La diferencia entre Apache y Nginx es
  imperceptible para un sitio con 50 visitas/hora
- **Ecosistema de módulos**: Apache puede hacer en un módulo lo que Nginx
  necesita un backend externo para hacer

---

## 6. Módulos y extensibilidad

### 6.1 Apache — módulos internos

Apache ejecuta módulos dentro de su propio proceso. El módulo tiene acceso
directo a la petición, la respuesta y los internos del servidor:

```
┌─────────────────────────────────┐
│          Apache httpd           │
│                                 │
│  Request → mod_rewrite          │
│          → mod_auth             │
│          → mod_php (ejecuta PHP)│
│          → mod_deflate          │
│          → Response             │
│                                 │
│  Todo en el mismo proceso       │
│  Acceso directo a la petición   │
└─────────────────────────────────┘
```

**Ventajas**: Sin overhead de comunicación, API rica para manipulación de
peticiones, más de 500 módulos disponibles.

**Desventajas**: Un módulo con bug puede crashear todo el worker, módulos
no thread-safe obligan a usar prefork, mayor superficie de ataque.

### 6.2 Nginx — procesamiento externo

Nginx delega el contenido dinámico a procesos externos vía protocolos estándar:

```
┌──────────────────┐     FastCGI      ┌──────────┐
│      Nginx       │─────────────────►│ PHP-FPM  │
│                  │                  └──────────┘
│  Request         │     HTTP proxy   ┌──────────┐
│  → rewrite       │─────────────────►│ Node.js  │
│  → access check  │                  └──────────┘
│  → proxy/fastcgi │     uWSGI        ┌──────────┐
│  → gzip          │─────────────────►│ Python   │
│  → Response      │                  └──────────┘
│                  │
│  Solo routing    │
│  y I/O           │
└──────────────────┘
```

**Ventajas**: Aislamiento (crash del backend no afecta a Nginx), cada
componente se escala independientemente, backend actualizable sin tocar Nginx.

**Desventajas**: Overhead de comunicación IPC, configuración de dos servicios
separados, debugging distribuido.

### 6.3 Módulos dinámicos de Nginx

Desde Nginx 1.9.11, se soportan módulos dinámicos cargables (similar a Apache):

```nginx
# Cargar módulo dinámico
load_module modules/ngx_http_geoip_module.so;

# Módulos comunes disponibles como paquetes
# Debian: apt install libnginx-mod-http-headers-more-filter
# RHEL:   dnf install nginx-mod-http-perl
```

Sin embargo, el ecosistema de módulos de Nginx es considerablemente menor que
el de Apache, y muchos requieren recompilación.

---

## 7. Contenido dinámico

### 7.1 Apache — mod_php (modelo clásico)

```
El modelo que dominó la web por 15+ años:

┌──────────────────────────────────┐
│         Apache + mod_php         │
│                                  │
│  Petición .php                   │
│       │                          │
│       ▼                          │
│  mod_php interpreta el script    │
│  dentro del proceso Apache       │
│       │                          │
│       ▼                          │
│  Respuesta generada              │
│                                  │
│  ✓ Simple: instalar y funciona   │
│  ✓ Sin config extra              │
│  ✗ PHP en cada proceso Apache    │
│  ✗ Obliga prefork (no thread-safe)│
│  ✗ Memoria: PHP cargado siempre  │
│    (incluso para servir .css)    │
└──────────────────────────────────┘
```

### 7.2 PHP-FPM (modelo moderno, ambos servidores)

```
┌──────────┐  FastCGI   ┌──────────────────┐
│  Apache   │──────────►│     PHP-FPM      │
│  o Nginx  │           │                  │
│           │           │  Pool de workers  │
│  Solo     │           │  ┌──┐ ┌──┐ ┌──┐  │
│  routing  │           │  │W1│ │W2│ │W3│  │
│  + I/O    │           │  └──┘ └──┘ └──┘  │
│           │◄──────────│                  │
│  Estáticos│ Respuesta │  Solo ejecuta PHP │
│  directo  │           │  cuando se pide   │
└──────────┘           └──────────────────┘

✓ Servidor web libre de PHP
✓ Escalar PHP y web server independientemente
✓ Apache puede usar event MPM
✓ Mismo modelo para Apache y Nginx
```

### 7.3 Proxy a aplicaciones (Node.js, Python, Go, etc.)

Ambos servidores actúan como proxy inverso. La diferencia es mínima:

```apache
# Apache
ProxyPass "/api" "http://127.0.0.1:3000"
ProxyPassReverse "/api" "http://127.0.0.1:3000"
```

```nginx
# Nginx
location /api/ {
    proxy_pass http://127.0.0.1:3000;
}
```

La elección del servidor web es menos relevante cuando el backend es un
servidor de aplicaciones independiente. La diferencia está en el overhead para
servir los estáticos asociados y en la gestión de conexiones concurrentes.

---

## 8. Casos de uso y cuándo elegir cada uno

### 8.1 Elegir Nginx cuando

| Escenario                                 | Razón                                |
|-------------------------------------------|--------------------------------------|
| Alto número de conexiones concurrentes    | Event-driven escala mejor            |
| Principalmente archivos estáticos         | sendfile, menor memoria              |
| Proxy inverso / balanceador               | Diseñado para esto nativamente       |
| Terminación TLS con alto tráfico          | Menor overhead por conexión          |
| Microservicios (muchos backends)          | Configuración de upstream simple     |
| Recursos de servidor limitados            | Menor uso de RAM y CPU              |

### 8.2 Elegir Apache cuando

| Escenario                                 | Razón                                |
|-------------------------------------------|--------------------------------------|
| Hosting compartido (múltiples usuarios)   | .htaccess permite delegación         |
| Aplicaciones PHP legacy con mod_php       | Integración directa, sin config extra|
| Necesidad de módulos específicos de Apache| Ecosistema más amplio               |
| Configuración altamente dinámica          | .htaccess sin restart                |
| Entornos donde ya está desplegado y funciona | No arreglar lo que no está roto   |
| Requisitos de compliance que lo especifican | Regulaciones o políticas internas  |

### 8.3 Resumen de decisión

```
¿Necesitas .htaccess?
├── Sí → Apache
└── No
    ├── ¿Alto tráfico / muchas conexiones?
    │   └── Nginx
    ├── ¿Solo proxy inverso?
    │   └── Nginx
    ├── ¿Infraestructura existente?
    │   └── Lo que ya esté funcionando
    └── ¿Empezando desde cero?
        └── Nginx (tendencia del mercado)
```

---

## 9. Apache y Nginx juntos

Un patrón frecuente en producción: Nginx como frontend y Apache como backend,
combinando las ventajas de ambos.

### 9.1 Arquitectura

```
Internet → Nginx (80/443) → Apache (8080)
           │                    │
           ├── TLS termination  ├── .htaccess (WordPress, etc.)
           ├── Estáticos        ├── mod_php / mod_perl
           ├── Compresión       ├── Módulos específicos
           ├── Rate limiting    └── Contenido dinámico
           └── Caché
```

### 9.2 Configuración

**Nginx (frontend):**

```nginx
server {
    listen 80;
    server_name ejemplo.com;

    # Estáticos directos desde Nginx
    location ~* \.(jpg|jpeg|png|gif|css|js|ico|woff2?)$ {
        root /var/www/ejemplo/public;
        expires 30d;
        access_log off;
    }

    # Todo lo demás al Apache backend
    location / {
        proxy_pass http://127.0.0.1:8080;
        proxy_set_header Host $host;
        proxy_set_header X-Real-IP $remote_addr;
        proxy_set_header X-Forwarded-For $proxy_add_x_forwarded_for;
        proxy_set_header X-Forwarded-Proto $scheme;
    }
}
```

**Apache (backend en puerto 8080):**

```apache
Listen 8080

<VirtualHost *:8080>
    ServerName ejemplo.com
    DocumentRoot "/var/www/ejemplo/public"

    # .htaccess funcional (WordPress, etc.)
    <Directory "/var/www/ejemplo/public">
        AllowOverride All
        Require all granted
    </Directory>

    # Configurar para leer IP real del proxy
    RemoteIPHeader X-Real-IP
    RemoteIPInternalProxy 127.0.0.1

    # Log con IP real, no la del proxy
    LogFormat "%{X-Real-IP}i %l %u %t \"%r\" %>s %b" proxy_combined
    CustomLog "/var/log/httpd/ejemplo-access.log" proxy_combined
</VirtualHost>
```

### 9.3 Ventajas de la combinación

| Componente | Responsabilidad            | Ventaja                           |
|------------|----------------------------|-----------------------------------|
| Nginx      | TLS, estáticos, compresión | Rendimiento, menos memoria        |
| Apache     | PHP, .htaccess, módulos    | Compatibilidad, flexibilidad      |
| Combinado  | Cada uno hace lo mejor     | WordPress + rendimiento           |

### 9.4 Cuándo NO combinar

- Si no necesitas .htaccess ni módulos específicos de Apache: usar solo Nginx
  con PHP-FPM es más simple y eficiente
- Si el tráfico es bajo: la complejidad operativa de dos servidores no se
  justifica
- Añade un punto de fallo adicional y complica el debugging

---

## 10. Errores comunes

### Error 1: Comparar sin considerar el caso de uso

**Problema**: Elegir servidor basándose en benchmarks genéricos sin considerar
que en la mayoría de aplicaciones reales, el cuello de botella es la base de
datos o el backend, no el servidor web.

**Realidad**: Para un sitio con 1,000 visitas diarias, la diferencia entre
Apache y Nginx es imperceptible. La elección importa a partir de miles de
conexiones concurrentes o cuando se sirven muchos estáticos.

### Error 2: Migrar de Apache a Nginx sin traducir .htaccess

**Problema**: Mover una aplicación PHP con `.htaccess` a Nginx y esperar que
funcione. Nginx no lee `.htaccess` — nunca lo hará.

**Solución**: Traducir cada regla del `.htaccess` a directivas Nginx:

```apache
# .htaccess de WordPress
RewriteEngine On
RewriteCond %{REQUEST_FILENAME} !-f
RewriteCond %{REQUEST_FILENAME} !-d
RewriteRule . /index.php [L]
```

```nginx
# Equivalente Nginx
location / {
    try_files $uri $uri/ /index.php$is_args$args;
}
```

### Error 3: Usar Apache en event MPM y esperar rendimiento de Nginx

**Causa**: Apache event MPM es significativamente mejor que prefork, pero sigue
sin alcanzar a Nginx en conexiones concurrentes porque mantiene una
arquitectura basada en procesos/threads, no en event loop puro.

**Realidad**: Apache event es una buena elección si necesitas las funcionalidades
de Apache. No es necesario migrar a Nginx solo por rendimiento — event MPM cubre
la mayoría de los escenarios.

### Error 4: Asumir que Nginx no puede hacer lo que Apache hace

**Ejemplos de funcionalidad equivalente:**

| "Solo Apache puede..."         | Nginx equivalente                      |
|-------------------------------|----------------------------------------|
| Autenticación HTTP            | `auth_basic` + `htpasswd`              |
| Autenticación LDAP            | `ngx_http_auth_request_module`         |
| Reescritura de URLs           | `rewrite` + `try_files` + `map`        |
| GeoIP blocking                | `ngx_http_geoip_module`                |
| Rate limiting                 | `limit_req` + `limit_conn`             |
| WAF                           | ModSecurity (disponible para ambos)    |

### Error 5: Correr ambos en puerto 80 y no entender el conflicto

```
AH00072: make_sock: could not bind to address [::]:80
```

**Causa**: Instalar Apache y Nginx sin cambiar puertos. Solo uno puede
escuchar en el mismo IP:puerto.

**Solución**: Si se usan juntos, Apache en puerto interno (8080) y Nginx en
80/443.

---

## 11. Cheatsheet

```
┌─────────────────────────────────────────────────────────────────┐
│            APACHE vs NGINX — REFERENCIA COMPARATIVA             │
├─────────────────────────────────────────────────────────────────┤
│                                                                 │
│  ARQUITECTURA:                                                  │
│  Apache prefork: 1 proceso/conexión (20 MB/conn)               │
│  Apache worker:  threads/proceso (menos memoria)                │
│  Apache event:   listener + workers (keep-alive eficiente)      │
│  Nginx:          event loop async (2.5 KB/conn)                 │
│                                                                 │
│  CONFIGURACIÓN:                                                 │
│  Apache: distribuida (.htaccess), cambios sin restart           │
│  Nginx:  centralizada (solo archivos conf), requiere reload    │
│                                                                 │
│  CONTENIDO DINÁMICO:                                            │
│  Apache: mod_php (embebido) o PHP-FPM (externo)                │
│  Nginx:  siempre externo (PHP-FPM, proxy a backend)            │
│                                                                 │
│  ESTÁTICOS:                                                     │
│  Apache: competente con event MPM                               │
│  Nginx:  superior (sendfile, menos overhead)                    │
│                                                                 │
│  ESCALABILIDAD:                                                 │
│  Apache: cientos-miles de conexiones                            │
│  Nginx:  decenas de miles de conexiones                         │
│                                                                 │
│  MÓDULOS:                                                       │
│  Apache: 500+, cargables dinámicamente, ecosistema maduro       │
│  Nginx:  menos módulos, muchos requieren recompilación          │
│                                                                 │
│  CUÁNDO ELEGIR:                                                 │
│  Apache: .htaccess, hosting compartido, mod_php legacy,         │
│          módulos específicos, infraestructura existente          │
│  Nginx:  alto tráfico, proxy inverso, estáticos, microservicios,│
│          recursos limitados, proyectos nuevos                   │
│                                                                 │
│  JUNTOS (Nginx frontend + Apache backend):                      │
│  Nginx :80/443 → proxy → Apache :8080                           │
│  Nginx: TLS, estáticos, compresión                              │
│  Apache: .htaccess, mod_php, módulos                            │
│  Usar: cuando la app requiere .htaccess + rendimiento           │
│  No usar: si puedes prescindir de .htaccess                     │
│                                                                 │
│  EQUIVALENCIAS RÁPIDAS:                                         │
│  DocumentRoot ............. root                                │
│  DirectoryIndex ........... index                               │
│  Redirect ................. return 301                          │
│  RewriteRule .............. rewrite / try_files                 │
│  ProxyPass ................ proxy_pass                          │
│  Alias .................... alias                               │
│  Require all denied ....... deny all                            │
│  Require ip X ............. allow X; deny all                   │
│  Options Indexes .......... autoindex on                        │
│  ServerTokens Prod ........ server_tokens off                   │
│  ErrorDocument 404 ........ error_page 404                     │
│  mod_deflate .............. gzip on                             │
│  Header set ............... add_header                          │
│  CustomLog ................ access_log                          │
│  LimitRequestBody ......... client_max_body_size               │
│                                                                 │
└─────────────────────────────────────────────────────────────────┘
```

---

## 12. Ejercicios

### Ejercicio 1: Benchmark comparativo de archivos estáticos

1. Instala Apache y Nginx en la misma máquina (Apache en 8080, Nginx en 80).
2. Crea un directorio con los mismos archivos estáticos para ambos:
   - Un HTML de 5 KB
   - Un CSS de 20 KB
   - Una imagen de 100 KB
3. Configura ambos servidores con:
   - Compresión activada
   - Keep-alive activado
   - Sin logging de acceso (para no sesgar con I/O de log)
4. Ejecuta el benchmark con `ab` o `wrk`:
   ```bash
   # 10,000 peticiones, 100 conexiones concurrentes
   ab -n 10000 -c 100 -k http://localhost:8080/test.html
   ab -n 10000 -c 100 -k http://localhost:80/test.html
   ```
5. Repite con 500 y 1000 conexiones concurrentes.
6. Compara: Requests/sec, Time per request, Transfer rate.
7. Monitoriza la memoria con `ps aux --sort=-%mem | grep -E 'apache|nginx|httpd'`
   durante el benchmark.

**Pregunta de predicción**: ¿En cuál de los tres niveles de concurrencia (100,
500, 1000) esperarías ver la mayor diferencia porcentual entre Apache y Nginx?
¿Por qué?

### Ejercicio 2: Traducción de .htaccess a Nginx

Traduce este `.htaccess` de una aplicación PHP a configuración Nginx equivalente:

```apache
RewriteEngine On

# Forzar HTTPS
RewriteCond %{HTTPS} off
RewriteRule ^(.*)$ https://%{HTTP_HOST}$1 [R=301,L]

# Redirigir www a no-www
RewriteCond %{HTTP_HOST} ^www\.(.+)$ [NC]
RewriteRule ^(.*)$ https://%1$1 [R=301,L]

# Front controller
RewriteCond %{REQUEST_FILENAME} !-f
RewriteCond %{REQUEST_FILENAME} !-d
RewriteRule ^(.*)$ index.php [QSA,L]

# Bloquear archivos sensibles
<FilesMatch "\.(env|git|sql|log)$">
    Require all denied
</FilesMatch>

# Cabeceras de seguridad
Header always set X-Content-Type-Options "nosniff"
Header always set X-Frame-Options "SAMEORIGIN"

# Caché de estáticos
<FilesMatch "\.(jpg|jpeg|png|gif|css|js)$">
    ExpiresActive On
    ExpiresByType image/jpeg "access plus 1 month"
    ExpiresByType text/css "access plus 1 week"
    ExpiresByType application/javascript "access plus 1 week"
</FilesMatch>

# Desactivar listado de directorio
Options -Indexes

# Error personalizado
ErrorDocument 404 /not-found.html
```

Escribe la configuración Nginx completa (server blocks para HTTP y HTTPS) y
verifica con `nginx -t`.

**Pregunta de predicción**: El `.htaccess` usa `RewriteCond %{HTTPS} off` para
detectar HTTP. ¿Cuál es el equivalente en Nginx? ¿Se necesita una condición o
se resuelve de otra manera?

### Ejercicio 3: Nginx como frontend de Apache

1. Configura Apache para escuchar en `127.0.0.1:8080` (solo localhost).
2. Crea un VirtualHost en Apache con `AllowOverride All` y un `.htaccess`
   con un front controller PHP simple.
3. Configura Nginx en puerto 80 como proxy inverso de Apache, sirviendo
   estáticos directamente.
4. Configura Apache para usar `RemoteIPHeader X-Real-IP` y verificar que los
   logs registran la IP del cliente real, no `127.0.0.1`.
5. Compara el rendimiento de servir una imagen:
   - Directamente desde Apache (:8080)
   - Desde Nginx (estático directo, sin proxy)
   - Desde Nginx → Apache (proxied)
6. ¿Cuál es más rápido para estáticos? ¿Y para PHP?

**Pregunta de reflexión**: En la arquitectura Nginx+Apache, cada petición
dinámica pasa por dos servidores web. ¿Este overhead adicional se compensa?
¿En qué escenarios el beneficio (TLS en Nginx, .htaccess en Apache) supera al
coste del doble procesamiento? ¿Cuándo sería mejor usar solo Nginx con
PHP-FPM directo?

---

> **Fin de la Sección 2 — Nginx.** Siguiente sección: S03 — HTTPS y TLS
> (T01 — Certificados: generación de CSR, self-signed, Let's Encrypt).
