# Mejora 2: Directory listing

## Índice
1. [¿Qué es directory listing?](#1-qué-es-directory-listing)
2. [Leer el contenido de un directorio](#2-leer-el-contenido-de-un-directorio)
3. [Ordenar las entradas](#3-ordenar-las-entradas)
4. [Generar HTML del listado](#4-generar-html-del-listado)
5. [Metadatos de archivos](#5-metadatos-de-archivos)
6. [Integrar con el servidor](#6-integrar-con-el-servidor)
7. [Estilo CSS del listado](#7-estilo-css-del-listado)
8. [Casos especiales](#8-casos-especiales)
9. [Errores comunes](#9-errores-comunes)
10. [Cheatsheet](#10-cheatsheet)
11. [Ejercicios](#11-ejercicios)

---

## 1. ¿Qué es directory listing?

Cuando el usuario navega a un directorio que no tiene
`index.html`, el servidor puede generar automáticamente una
página HTML con la lista de archivos:

```
Navegador: GET /img/

Servidor detecta:
  ./www/img/ es un directorio
  ./www/img/index.html NO existe
  → generar listing automático

Respuesta:
┌─────────────────────────────────────────┐
│  Index of /img/                          │
│                                          │
│  Name              Size      Modified    │
│  ─────────────────────────────────────── │
│  ../               -         -           │
│  banner.jpg        245 KB    2026-03-15  │
│  logo.png          12 KB     2026-03-10  │
│  test.svg          342 B     2026-03-20  │
└─────────────────────────────────────────┘
```

### ¿Cuándo generar listing?

```
URI apunta a directorio?
  │
  ├── index.html existe → servir index.html (T04)
  │
  └── index.html NO existe
        │
        ├── listing habilitado → generar HTML con lista
        │
        └── listing deshabilitado → 403 Forbidden
```

En servidores de producción, el listing suele estar **deshabilitado**
por seguridad (no exponer la estructura de archivos). En desarrollo
y servidores internos, es muy útil.

---

## 2. Leer el contenido de un directorio

Usamos `opendir` / `readdir` / `closedir` para recorrer las
entradas:

```c
#include <dirent.h>
#include <sys/stat.h>
#include <string.h>
#include <stdlib.h>

typedef struct {
    char   name[256];
    int    is_dir;       // 1 si es directorio
    off_t  size;         // tamaño en bytes (0 para directorios)
    time_t mtime;        // última modificación
} dir_entry_t;

// Leer las entradas de un directorio
// Retorna número de entradas leídas, o -1 en error
int read_directory(const char *dirpath, dir_entry_t *entries,
                   int max_entries)
{
    DIR *dir = opendir(dirpath);
    if (dir == NULL) {
        return -1;
    }

    int count = 0;
    struct dirent *ent;

    while ((ent = readdir(dir)) != NULL && count < max_entries) {
        // Saltar "." (directorio actual)
        if (strcmp(ent->d_name, ".") == 0)
            continue;

        // Guardar nombre
        strncpy(entries[count].name, ent->d_name,
                sizeof(entries[count].name) - 1);
        entries[count].name[sizeof(entries[count].name) - 1] = '\0';

        // Obtener metadatos con stat
        char fullpath[PATH_MAX];
        snprintf(fullpath, sizeof(fullpath), "%s/%s", dirpath, ent->d_name);

        struct stat st;
        if (stat(fullpath, &st) == 0) {
            entries[count].is_dir = S_ISDIR(st.st_mode);
            entries[count].size   = S_ISDIR(st.st_mode) ? 0 : st.st_size;
            entries[count].mtime  = st.st_mtime;
        } else {
            entries[count].is_dir = 0;
            entries[count].size   = 0;
            entries[count].mtime  = 0;
        }

        count++;
    }

    closedir(dir);
    return count;
}
```

### ¿Por qué stat además de readdir?

`readdir` solo retorna el nombre (`d_name`) y opcionalmente
el tipo (`d_type`). Pero `d_type` no está garantizado en todos
los filesystems — puede ser `DT_UNKNOWN`. Además, necesitamos
tamaño y mtime, que solo `stat` proporciona:

```
readdir:  nombre + d_type (no confiable)
stat:     tipo + tamaño + permisos + timestamps (confiable)
```

### ¿Por qué saltar "." pero mantener ".."?

```
"."   → el directorio actual (no es útil en un listing)
".."  → el directorio padre (permite navegar hacia arriba)
```

---

## 3. Ordenar las entradas

Un listing desordenado es difícil de leer. Ordenamos:
directorios primero, luego por nombre:

```c
#include <stdlib.h>
#include <strings.h>

int compare_entries(const void *a, const void *b) {
    const dir_entry_t *ea = (const dir_entry_t *)a;
    const dir_entry_t *eb = (const dir_entry_t *)b;

    // ".." siempre primero
    if (strcmp(ea->name, "..") == 0) return -1;
    if (strcmp(eb->name, "..") == 0) return 1;

    // Directorios antes que archivos
    if (ea->is_dir && !eb->is_dir) return -1;
    if (!ea->is_dir && eb->is_dir) return 1;

    // Mismo tipo: ordenar por nombre (case-insensitive)
    return strcasecmp(ea->name, eb->name);
}

// Después de read_directory:
qsort(entries, count, sizeof(dir_entry_t), compare_entries);
```

### Resultado

```
Antes de ordenar:              Después de ordenar:
  test.svg                       ../
  logo.png                       css/          ← directorio
  ../                            js/           ← directorio
  css/                           banner.jpg    ← archivo
  banner.jpg                     logo.png      ← archivo
  js/                            test.svg      ← archivo
```

---

## 4. Generar HTML del listado

Construimos el HTML dinámicamente en un buffer. Cada entrada
es una fila de una tabla:

```c
#include <stdio.h>
#include <time.h>

// Formatear tamaño legible (1234567 → "1.2 MB")
void format_size(off_t size, char *buf, size_t bufsize) {
    if (size == 0) {
        snprintf(buf, bufsize, "-");
    } else if (size < 1024) {
        snprintf(buf, bufsize, "%ld B", (long)size);
    } else if (size < 1024 * 1024) {
        snprintf(buf, bufsize, "%.1f KB", size / 1024.0);
    } else if (size < 1024LL * 1024 * 1024) {
        snprintf(buf, bufsize, "%.1f MB", size / (1024.0 * 1024));
    } else {
        snprintf(buf, bufsize, "%.1f GB", size / (1024.0 * 1024 * 1024));
    }
}

// Formatear fecha (2026-03-20 14:30)
void format_mtime(time_t mtime, char *buf, size_t bufsize) {
    if (mtime == 0) {
        snprintf(buf, bufsize, "-");
        return;
    }
    struct tm tm;
    localtime_r(&mtime, &tm);
    strftime(buf, bufsize, "%Y-%m-%d %H:%M", &tm);
}

// Generar HTML del directory listing
// uri: la URI del directorio (ej: "/img/")
// dirpath: ruta en el filesystem (ej: "./www/img")
// html_buf: buffer donde escribir el HTML
// Retorna bytes escritos, o -1 en error
int generate_listing(const char *uri, const char *dirpath,
                     char *html_buf, size_t html_bufsize)
{
    dir_entry_t entries[512];
    int count = read_directory(dirpath, entries, 512);
    if (count == -1)
        return -1;

    qsort(entries, count, sizeof(dir_entry_t), compare_entries);

    // Cabecera HTML
    int n = snprintf(html_buf, html_bufsize,
        "<!DOCTYPE html>\n"
        "<html>\n"
        "<head>\n"
        "  <meta charset=\"utf-8\">\n"
        "  <title>Index of %s</title>\n"
        "  <style>\n"
        "    body { font-family: monospace; margin: 20px; }\n"
        "    h1 { font-size: 1.3em; }\n"
        "    table { border-collapse: collapse; }\n"
        "    th, td { text-align: left; padding: 4px 16px; }\n"
        "    th { border-bottom: 1px solid #999; }\n"
        "    a { text-decoration: none; color: #0366d6; }\n"
        "    a:hover { text-decoration: underline; }\n"
        "    .dir { font-weight: bold; }\n"
        "    .size, .mtime { color: #666; }\n"
        "  </style>\n"
        "</head>\n"
        "<body>\n"
        "<h1>Index of %s</h1>\n"
        "<table>\n"
        "  <tr><th>Name</th><th>Size</th><th>Modified</th></tr>\n",
        uri, uri);

    // Filas
    for (int i = 0; i < count; i++) {
        char size_str[32];
        char mtime_str[32];

        format_size(entries[i].size, size_str, sizeof(size_str));
        format_mtime(entries[i].mtime, mtime_str, sizeof(mtime_str));

        // Construir el href
        // ".." → href al directorio padre
        // directorio → nombre + "/"
        // archivo → nombre
        char href[512];
        if (strcmp(entries[i].name, "..") == 0) {
            // Calcular URI del padre
            // "/img/" → "/"
            // "/css/fonts/" → "/css/"
            char parent[MAX_PATH];
            strncpy(parent, uri, sizeof(parent) - 1);
            parent[sizeof(parent) - 1] = '\0';
            size_t plen = strlen(parent);
            if (plen > 1 && parent[plen - 1] == '/')
                parent[plen - 1] = '\0';
            char *slash = strrchr(parent, '/');
            if (slash != NULL)
                slash[1] = '\0';
            else
                strcpy(parent, "/");
            snprintf(href, sizeof(href), "%s", parent);
        } else if (entries[i].is_dir) {
            snprintf(href, sizeof(href), "%s/", entries[i].name);
        } else {
            snprintf(href, sizeof(href), "%s", entries[i].name);
        }

        const char *css_class = entries[i].is_dir ? " class=\"dir\"" : "";
        const char *slash     = entries[i].is_dir ? "/" : "";

        n += snprintf(html_buf + n, html_bufsize - n,
            "  <tr>\n"
            "    <td><a href=\"%s\"%s>%s%s</a></td>\n"
            "    <td class=\"size\">%s</td>\n"
            "    <td class=\"mtime\">%s</td>\n"
            "  </tr>\n",
            href, css_class, entries[i].name, slash,
            size_str, mtime_str);

        if ((size_t)n >= html_bufsize - 1)
            break;  // buffer lleno
    }

    // Pie
    n += snprintf(html_buf + n, html_bufsize - n,
        "</table>\n"
        "<hr>\n"
        "<p><em>minihttpd</em></p>\n"
        "</body>\n"
        "</html>\n");

    return n;
}
```

### HTML generado (ejemplo)

```html
<!DOCTYPE html>
<html>
<head>
  <meta charset="utf-8">
  <title>Index of /img/</title>
  <style>...</style>
</head>
<body>
<h1>Index of /img/</h1>
<table>
  <tr><th>Name</th><th>Size</th><th>Modified</th></tr>
  <tr>
    <td><a href="/" class="dir">../</a></td>
    <td class="size">-</td>
    <td class="mtime">-</td>
  </tr>
  <tr>
    <td><a href="banner.jpg">banner.jpg</a></td>
    <td class="size">245.3 KB</td>
    <td class="mtime">2026-03-15 10:30</td>
  </tr>
  <tr>
    <td><a href="logo.png">logo.png</a></td>
    <td class="size">12.1 KB</td>
    <td class="mtime">2026-03-10 08:15</td>
  </tr>
</table>
<hr>
<p><em>minihttpd</em></p>
</body>
</html>
```

---

## 5. Metadatos de archivos

### Formatear tamaño

El tamaño en bytes no es legible para archivos grandes. Usamos
unidades humanas:

```
Bytes         Formato
──────────    ────────
         0    -
       512    512 B
      3072    3.0 KB
   1048576    1.0 MB
1073741824    1.0 GB
```

La función `format_size` (sección 4) maneja esto con umbrales
de 1024.

### Formatear fecha de modificación

`st.st_mtime` es un `time_t` (segundos desde epoch). Lo
convertimos a formato legible con `localtime_r` + `strftime`:

```c
struct tm tm;
localtime_r(&mtime, &tm);        // thread-safe
strftime(buf, size, "%Y-%m-%d %H:%M", &tm);
// → "2026-03-20 14:30"
```

Usamos `localtime_r` (no `gmtime_r`) porque las fechas del
listing son para el usuario, que espera su hora local.

### Iconos por tipo (opcional)

Para mejorar la legibilidad visual, se puede añadir un indicador
del tipo de archivo basado en la extensión:

```
Directorio  →  nombre/
Imagen      →  nombre.png
Código      →  nombre.c
Archivo     →  nombre.txt
```

Nuestro listing ya muestra directorios con trailing `/` y
negrita (CSS class `dir`), lo cual es suficiente para
distinguir directorios de archivos.

---

## 6. Integrar con el servidor

### Modificar resolve_index

En T04, cuando `index.html` no existía, retornábamos -1 (404).
Ahora tenemos una tercera opción: generar listing:

```c
// Retorna:
//   0 = archivo encontrado (filepath actualizado)
//   1 = redirect necesario (directorio sin trailing /)
//   2 = directorio sin index.html → generar listing
//  -1 = error (no existe)
int resolve_index(char *filepath, size_t filepath_size,
                  const char *uri)
{
    struct stat st;
    if (stat(filepath, &st) == -1) {
        return -1;
    }

    if (S_ISDIR(st.st_mode)) {
        size_t uri_len = strlen(uri);
        if (uri_len == 0 || uri[uri_len - 1] != '/') {
            return 1;  // redirect → /path/
        }

        // Intentar index.html
        size_t len = strlen(filepath);
        if (filepath[len - 1] != '/') {
            filepath[len] = '/';
            filepath[len + 1] = '\0';
            len++;
        }

        char try_path[PATH_MAX];
        snprintf(try_path, sizeof(try_path), "%sindex.html", filepath);

        if (stat(try_path, &st) == 0 && S_ISREG(st.st_mode)) {
            strncpy(filepath, try_path, filepath_size - 1);
            return 0;  // index.html encontrado
        }

        // Sin index.html → quitar "index.html" del filepath
        filepath[len] = '\0';
        return 2;  // generar listing
    }

    return 0;
}
```

### Modificar handle_client

```c
void handle_client(int client_fd, const char *client_ip) {
    char buf[8192];

    ssize_t n = read_request(client_fd, buf, sizeof(buf));
    if (n <= 0) return;

    http_request_t req;
    if (parse_http_request(buf, &req) == -1) {
        send_error(client_fd, 400);
        return;
    }

    url_decode(req.path);

    if (strcmp(req.method, "GET") != 0 &&
        strcmp(req.method, "HEAD") != 0) {
        send_error_with_header(client_fd, 405, "Allow: GET, HEAD\r\n");
        return;
    }

    char filepath[PATH_MAX];
    snprintf(filepath, sizeof(filepath), "%s%s", DOCROOT, req.path);

    char resolved[PATH_MAX];
    if (validate_path(filepath, resolved, sizeof(resolved)) == -1) {
        send_error(client_fd, 403);
        return;
    }
    strncpy(filepath, resolved, sizeof(filepath) - 1);

    int idx_result = resolve_index(filepath, sizeof(filepath), req.path);

    switch (idx_result) {
        case 0:
            // Archivo encontrado → servir
            serve_file_cached(client_fd, filepath, &req);
            break;

        case 1: {
            // Redirect: directorio sin trailing slash
            char location[MAX_PATH + 2];
            snprintf(location, sizeof(location), "%s/", req.path);
            send_redirect(client_fd, 301, location);
            break;
        }

        case 2: {
            // Directory listing
            printf("[%s] %s %s → listing\n",
                   client_ip, req.method, req.path);

            char html[65536];
            int html_len = generate_listing(req.path, filepath,
                                            html, sizeof(html));
            if (html_len == -1) {
                send_error(client_fd, 500);
                break;
            }

            int is_head = (strcmp(req.method, "HEAD") == 0);
            http_response_t resp = {
                .status_code  = 200,
                .content_type = "text/html; charset=utf-8",
                .body         = html,
                .body_len     = html_len,
                .is_head      = is_head,
            };
            send_http_response(client_fd, &resp);
            break;
        }

        case -1:
        default:
            send_error(client_fd, 404);
            break;
    }
}
```

### Diagrama de decisión actualizado

```
parse request → url_decode → validate_path
                                  │
                            resolve_index()
                          ┌───────┼───────┬────────┐
                          │       │       │        │
                         0       1       2       -1
                      archivo  redirect listing   404
                          │       │       │
                          ▼       ▼       ▼
                    serve_file  301    generate_listing
                    (sendfile)  /path/  → HTML → 200
```

---

## 7. Estilo CSS del listado

El CSS inline de la sección 4 produce un listado funcional.
Para un resultado más pulido, se puede expandir:

```css
body {
    font-family: -apple-system, 'Segoe UI', Helvetica, Arial, sans-serif;
    max-width: 900px;
    margin: 40px auto;
    padding: 0 20px;
    color: #333;
    background: #fafafa;
}

h1 {
    font-size: 1.4em;
    font-weight: normal;
    color: #555;
    border-bottom: 1px solid #ddd;
    padding-bottom: 10px;
}

table {
    width: 100%;
    border-collapse: collapse;
}

th {
    text-align: left;
    padding: 8px 16px;
    border-bottom: 2px solid #ddd;
    color: #666;
    font-size: 0.85em;
    text-transform: uppercase;
}

td {
    padding: 6px 16px;
    border-bottom: 1px solid #eee;
}

tr:hover {
    background: #f0f0f0;
}

a {
    text-decoration: none;
    color: #0366d6;
}

a:hover {
    text-decoration: underline;
}

.dir a {
    font-weight: bold;
}

.size, .mtime {
    color: #888;
    font-size: 0.9em;
}

hr {
    border: none;
    border-top: 1px solid #ddd;
    margin: 20px 0;
}
```

### Resultado visual en el navegador

```
┌─────────────────────────────────────────────────┐
│                                                  │
│  Index of /img/                                  │
│  ────────────────────────────────────────────── │
│                                                  │
│  NAME              SIZE        MODIFIED          │
│  ═══════════════════════════════════════════════ │
│  ../               -           -                │ ← hover: fondo gris
│  css/              -           2026-03-15 10:30 │ ← negrita (directorio)
│  js/               -           2026-03-18 14:00 │
│  banner.jpg        245.3 KB    2026-03-15 10:30 │
│  logo.png          12.1 KB     2026-03-10 08:15 │
│  test.svg          342 B       2026-03-20 09:45 │
│  ────────────────────────────────────────────── │
│  minihttpd                                       │
│                                                  │
└─────────────────────────────────────────────────┘
```

---

## 8. Casos especiales

### Archivos ocultos (dotfiles)

En Linux, los archivos que empiezan con `.` se consideran ocultos.
¿Los mostramos en el listing?

```c
// Opción 1: ocultar dotfiles (como nginx por defecto)
if (ent->d_name[0] == '.' && strcmp(ent->d_name, "..") != 0)
    continue;

// Opción 2: mostrar todos (como `ls -a`)
// No filtrar
```

Para un servidor HTTP, ocultar dotfiles es lo habitual — evita
exponer `.htaccess`, `.git`, `.env`, etc.:

```c
while ((ent = readdir(dir)) != NULL && count < max_entries) {
    if (strcmp(ent->d_name, ".") == 0)
        continue;

    // Ocultar dotfiles (excepto "..")
    if (ent->d_name[0] == '.' && strcmp(ent->d_name, "..") != 0)
        continue;

    // ... procesar entrada ...
}
```

### Directorio raíz (/)

Cuando listamos `/`, el link `../` debería llevar a `/` (no
tiene padre). Lo manejamos en la construcción del href:

```c
if (strcmp(entries[i].name, "..") == 0) {
    // Si ya estamos en "/", .. → "/" (no "//../")
    if (strcmp(uri, "/") == 0) {
        continue;  // no mostrar ".." en la raíz
    }
    // ...
}
```

### Caracteres especiales en nombres de archivos

Un archivo llamado `<script>alert(1)</script>.html` inyectaría
JavaScript en el listing (XSS). Debemos escapar HTML:

```c
// Escapar caracteres especiales para HTML
int html_escape(const char *src, char *dst, size_t dst_size) {
    int n = 0;
    while (*src && (size_t)n < dst_size - 6) {
        switch (*src) {
            case '&':  n += snprintf(dst + n, dst_size - n, "&amp;");  break;
            case '<':  n += snprintf(dst + n, dst_size - n, "&lt;");   break;
            case '>':  n += snprintf(dst + n, dst_size - n, "&gt;");   break;
            case '"':  n += snprintf(dst + n, dst_size - n, "&quot;"); break;
            default:   dst[n++] = *src; break;
        }
        src++;
    }
    dst[n] = '\0';
    return n;
}
```

Usar en el listing:

```c
char safe_name[512];
html_escape(entries[i].name, safe_name, sizeof(safe_name));

n += snprintf(html_buf + n, html_bufsize - n,
    "<td><a href=\"%s\">%s%s</a></td>\n",
    href, safe_name, slash);
```

### Percent-encoding en hrefs

Los nombres de archivos con espacios u otros caracteres
especiales deben ser codificados en el `href`:

```c
// Codificar caracteres que no son seguros en URLs
int url_encode(const char *src, char *dst, size_t dst_size) {
    static const char *safe = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrs"
                              "tuvwxyz0123456789-._~/";
    int n = 0;
    while (*src && (size_t)n < dst_size - 4) {
        if (strchr(safe, *src)) {
            dst[n++] = *src;
        } else {
            n += snprintf(dst + n, dst_size - n,
                          "%%%02X", (unsigned char)*src);
        }
        src++;
    }
    dst[n] = '\0';
    return n;
}
```

```
Archivo:    "my file (1).txt"
Display:    my file (1).txt       (escapado con html_escape)
Href:       my%20file%20%281%29.txt  (codificado con url_encode)
```

---

## 9. Errores comunes

### Error 1: no escapar HTML en nombres de archivos

```c
// MAL: nombre sin escapar
snprintf(html, size, "<a href=\"%s\">%s</a>", name, name);
// Archivo: "<img src=x onerror=alert(1)>.txt"
// → XSS: el navegador ejecuta JavaScript
```

**Solución**: siempre escapar nombres antes de insertarlos en HTML:

```c
char safe_name[512];
html_escape(name, safe_name, sizeof(safe_name));
snprintf(html, size, "<a href=\"%s\">%s</a>", href, safe_name);
```

### Error 2: no cerrar DIR con closedir

```c
// MAL: opendir sin closedir
DIR *dir = opendir(path);
// ... leer entradas ...
// olvidar closedir(dir)
// → fd leak (DIR usa un fd internamente)
// → después de muchos listings: Too many open files
```

**Solución**: siempre `closedir()` al terminar:

```c
DIR *dir = opendir(path);
// ... leer entradas ...
closedir(dir);
```

### Error 3: buffer overflow en el HTML generado

```c
// MAL: no verificar espacio restante en el buffer
for (int i = 0; i < count; i++) {
    n += snprintf(html + n, bufsize - n, "<tr>...", ...);
    // Si bufsize - n es negativo (n > bufsize),
    // snprintf recibe un size_t enorme (underflow) → crash o overflow
}
```

**Solución**: verificar antes de cada snprintf que hay espacio:

```c
for (int i = 0; i < count; i++) {
    if ((size_t)n >= html_bufsize - 1)
        break;
    n += snprintf(html_buf + n, html_bufsize - n, "...", ...);
}
```

### Error 4: mostrar archivos ocultos sensibles

```c
// MAL: mostrar todos los archivos incluyendo dotfiles
while ((ent = readdir(dir)) != NULL) {
    // .env, .git/, .htaccess expuestos al público
    // → contraseñas, tokens, estructura del repo visibles
}
```

**Solución**: filtrar dotfiles (excepto `..`):

```c
if (ent->d_name[0] == '.' && strcmp(ent->d_name, "..") != 0)
    continue;
```

### Error 5: href de ".." incorrecto

```c
// MAL: href de ".." siempre apunta a ".."
snprintf(href, size, "..");
// Funciona en algunos navegadores pero no es consistente
// Algunos resuelven ".." relativo a la URI actual, otros no
```

**Solución**: calcular la URI padre explícitamente:

```c
// URI: "/img/photos/" → padre: "/img/"
// URI: "/img/" → padre: "/"
char parent[PATH_MAX];
strncpy(parent, uri, sizeof(parent) - 1);
// Quitar trailing slash, luego buscar el último /
// Truncar después de ese /
```

---

## 10. Cheatsheet

```
┌──────────────────────────────────────────────────────────────┐
│              Directory Listing                               │
├──────────────────────────────────────────────────────────────┤
│                                                              │
│  DECISIÓN:                                                   │
│    directorio + index.html existe → servir index.html       │
│    directorio + sin index.html   → generar listing          │
│    directorio sin trailing /     → 301 redirect             │
│                                                              │
│  LEER DIRECTORIO:                                            │
│    opendir(path) → readdir loop → stat cada entrada         │
│    → closedir (siempre)                                      │
│    Saltar ".", ocultar dotfiles (excepto "..")               │
│                                                              │
│  ORDENAR:                                                    │
│    qsort con comparador:                                    │
│      ".." primero → directorios → archivos → por nombre     │
│                                                              │
│  GENERAR HTML:                                               │
│    snprintf en buffer: head + table rows + footer            │
│    Verificar espacio restante en cada snprintf               │
│                                                              │
│  METADATOS:                                                  │
│    stat(fullpath, &st)                                       │
│    Tamaño: st.st_size → format_size (B/KB/MB/GB)            │
│    Fecha: st.st_mtime → localtime_r + strftime              │
│    Tipo: S_ISDIR(st.st_mode)                                │
│                                                              │
│  SEGURIDAD:                                                  │
│    html_escape(name)       evitar XSS en nombres            │
│    url_encode(name)        codificar href (%20, etc.)       │
│    ocultar dotfiles        no exponer .env, .git, etc.      │
│    validate_path sigue     activo (path traversal)           │
│                                                              │
│  resolve_index RETORNOS:                                     │
│    0 = archivo (servir)                                      │
│    1 = redirect (301 con /)                                  │
│    2 = listing (generar HTML)                                │
│   -1 = no existe (404)                                       │
│                                                              │
│  HREF DE ".." :                                              │
│    Calcular URI padre explícitamente                         │
│    / → no mostrar ..                                         │
│    /img/ → ../  =  /                                         │
│    /img/photos/ → ../  =  /img/                              │
│                                                              │
└──────────────────────────────────────────────────────────────┘
```

---

## 11. Ejercicios

### Ejercicio 1: listing básico

Implementa el listing de directorios con los metadatos esenciales.

**Pasos**:
1. Implementa `read_directory()` con opendir/readdir/stat
2. Implementa `compare_entries()` para qsort (.. primero,
   luego dirs, luego archivos por nombre)
3. Implementa `format_size()` y `format_mtime()`
4. Implementa `generate_listing()` que produzca HTML con tabla
5. Crea un directorio de prueba con subdirectorios y archivos
   de varios tamaños
6. Prueba en el navegador: `http://localhost:8080/`

**Predicción antes de ejecutar**: si el directorio tiene 3
archivos y 2 subdirectorios, ¿en qué orden aparecen en el
listing? ¿"../" aparece como primer o segundo elemento?
¿Qué muestra la columna "Size" para los directorios?

> **Pregunta de reflexión**: `readdir` no garantiza ningún
> orden — depende del filesystem. ¿Qué pasa si no ordenas
> las entradas? ¿El orden sería consistente entre refrescos
> de la página? ¿Y entre diferentes filesystems (ext4 vs tmpfs)?

### Ejercicio 2: seguridad XSS y encoding

Implementa las protecciones contra XSS y codificación URL.

**Pasos**:
1. Implementa `html_escape()` para &, <, >, "
2. Implementa `url_encode()` para caracteres no seguros en URLs
3. Crea un archivo con nombre especial:
   `touch 'www/<script>test</script>.txt'`
4. Crea un archivo con espacios:
   `touch 'www/my file.txt'`
5. Verifica que el listing no ejecuta JavaScript y que los
   links con espacios funcionan

**Predicción antes de ejecutar**: sin html_escape, ¿qué ve
el navegador al encontrar un archivo llamado `<b>bold</b>.txt`?
¿El nombre aparece en negrita o se ve el texto literal? ¿Y con
html_escape?

> **Pregunta de reflexión**: html_escape protege el **contenido
> visible** (`<td>name</td>`), pero url_encode protege el
> **atributo href** (`href="..."`). ¿Por qué necesitas ambos?
> ¿Qué ataque es posible si solo escapas el contenido visible
> pero no el href? Piensa en un archivo llamado
> `" onclick="alert(1)`.

### Ejercicio 3: listing con filtro de dotfiles y directorio raíz

Implementa los casos especiales del listing.

**Pasos**:
1. Oculta archivos que empiezan con `.` (excepto `..`)
2. Crea `.env`, `.git/`, `.hidden.txt` en www/ y verifica que
   no aparecen en el listing
3. Maneja el caso de la raíz (`/`): no mostrar `../`
4. Implementa el cálculo del href padre para `..`:
   `/a/b/c/` → `/a/b/`, `/a/` → `/`
5. Verifica que la navegación con `../` funciona correctamente
   en el navegador

**Predicción antes de ejecutar**: si creas `www/.secret` y
`www/.git/`, ¿aparecen en el listing? ¿Y en `ls www/`?
Si el listing de `/` no muestra `../`, ¿cuántas filas tiene
un directorio raíz vacío?

> **Pregunta de reflexión**: ocultar dotfiles en el listing
> evita que el **usuario vea** los archivos. Pero ¿puede un
> usuario acceder a `GET /.env` directamente? Si el listing
> está deshabilitado pero el archivo existe, ¿el servidor
> debería servirlo? ¿Cómo implementarías una "lista negra" de
> archivos/patrones que nunca se sirven (como `.git/`, `.env`)?
