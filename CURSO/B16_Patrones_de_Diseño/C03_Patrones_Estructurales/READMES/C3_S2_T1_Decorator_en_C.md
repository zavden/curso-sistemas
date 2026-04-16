# T01 — Decorator en C

---

## 1. Que es un Decorator

Un decorator envuelve un objeto existente y agrega comportamiento
**sin modificar el objeto original**. El decorator implementa la
misma interfaz que el objeto que envuelve:

```
  Sin decorator:
  ──────────────
  Caller ──> Logger (escribe al archivo)

  Con decorator:
  ──────────────
  Caller ──> [TimestampDecorator] ──> Logger
              agrega timestamp,
              luego delega al Logger real

  Con cadena de decoradores:
  ──────────────────────────
  Caller ──> [RateLimiter] ──> [Timestamp] ──> [Encrypt] ──> Logger
              limita freq.      agrega hora     cifra msg     escribe
```

La clave: el caller no sabe si habla con el Logger real o con
un decorator. Todos implementan la **misma interfaz**.

```
  Decorator vs Adapter (S01):
  ───────────────────────────
  Adapter:    interfaz A → interfaz B (TRADUCE)
  Decorator:  interfaz A → interfaz A + extra (EXTIENDE)

  El decorator no cambia la interfaz, agrega comportamiento.
```

---

## 2. Decorator con vtable

En C, la misma interfaz se logra con el mismo struct de
function pointers:

### 2.1 Interfaz base: Stream de escritura

```c
/* stream.h */
#ifndef STREAM_H
#define STREAM_H

#include <stddef.h>

typedef struct Stream Stream;

typedef struct {
    int  (*write)(Stream *s, const void *data, size_t len);
    int  (*flush)(Stream *s);
    void (*close)(Stream *s);
} StreamOps;

struct Stream {
    const StreamOps *ops;
    void            *ctx;  /* Datos del stream concreto */
};

/* Funciones de conveniencia */
static inline int stream_write(Stream *s, const void *data, size_t len) {
    return s->ops->write(s, data, len);
}
static inline int stream_flush(Stream *s) {
    return s->ops->flush(s);
}
static inline void stream_close(Stream *s) {
    s->ops->close(s);
}

#endif
```

### 2.2 Implementacion base: FileStream

```c
/* file_stream.c */
#include "stream.h"
#include <stdio.h>
#include <stdlib.h>

typedef struct {
    FILE *file;
    char  path[256];
} FileCtx;

static int file_write(Stream *s, const void *data, size_t len) {
    FileCtx *ctx = s->ctx;
    return (int)fwrite(data, 1, len, ctx->file);
}

static int file_flush(Stream *s) {
    FileCtx *ctx = s->ctx;
    return fflush(ctx->file);
}

static void file_close(Stream *s) {
    FileCtx *ctx = s->ctx;
    if (ctx->file) {
        fclose(ctx->file);
        ctx->file = NULL;
    }
    free(ctx);
    free(s);
}

static const StreamOps file_ops = {
    .write = file_write,
    .flush = file_flush,
    .close = file_close,
};

Stream *file_stream_open(const char *path) {
    FILE *f = fopen(path, "wb");
    if (!f) return NULL;

    FileCtx *ctx = malloc(sizeof(*ctx));
    snprintf(ctx->path, sizeof(ctx->path), "%s", path);
    ctx->file = f;

    Stream *s = malloc(sizeof(*s));
    s->ops = &file_ops;
    s->ctx = ctx;
    return s;
}
```

### 2.3 Decorator: BufferedStream

```c
/* buffered_stream.c */
#include "stream.h"
#include <stdlib.h>
#include <string.h>

typedef struct {
    Stream *inner;     /* El stream que envolvemos */
    char   *buffer;
    size_t  buf_size;
    size_t  buf_used;
} BufferedCtx;

static int buffered_write(Stream *s, const void *data, size_t len) {
    BufferedCtx *ctx = s->ctx;

    /* Si no cabe en el buffer, flush primero */
    if (ctx->buf_used + len > ctx->buf_size) {
        stream_write(ctx->inner, ctx->buffer, ctx->buf_used);
        ctx->buf_used = 0;
    }

    /* Si sigue sin caber, escribir directo */
    if (len > ctx->buf_size) {
        return stream_write(ctx->inner, data, len);
    }

    /* Copiar al buffer */
    memcpy(ctx->buffer + ctx->buf_used, data, len);
    ctx->buf_used += len;
    return (int)len;
}

static int buffered_flush(Stream *s) {
    BufferedCtx *ctx = s->ctx;
    if (ctx->buf_used > 0) {
        stream_write(ctx->inner, ctx->buffer, ctx->buf_used);
        ctx->buf_used = 0;
    }
    return stream_flush(ctx->inner);
}

static void buffered_close(Stream *s) {
    BufferedCtx *ctx = s->ctx;
    buffered_flush(s);           /* Flush antes de cerrar */
    stream_close(ctx->inner);    /* Cerrar el inner */
    free(ctx->buffer);
    free(ctx);
    free(s);
}

static const StreamOps buffered_ops = {
    .write = buffered_write,
    .flush = buffered_flush,
    .close = buffered_close,
};

Stream *buffered_stream(Stream *inner, size_t buf_size) {
    BufferedCtx *ctx = malloc(sizeof(*ctx));
    ctx->inner    = inner;
    ctx->buffer   = malloc(buf_size);
    ctx->buf_size = buf_size;
    ctx->buf_used = 0;

    Stream *s = malloc(sizeof(*s));
    s->ops = &buffered_ops;
    s->ctx = ctx;
    return s;
}
```

### 2.4 Decorator: CompressingStream

```c
/* compress_stream.c */
#include "stream.h"
#include <stdlib.h>
#include <string.h>

typedef struct {
    Stream *inner;
} CompressCtx;

/* Compresion simplificada: RLE (Run-Length Encoding) */
static int compress_write(Stream *s, const void *data, size_t len) {
    CompressCtx *ctx = s->ctx;
    const unsigned char *bytes = data;
    size_t i = 0;
    int total = 0;

    while (i < len) {
        unsigned char ch = bytes[i];
        size_t count = 1;
        while (i + count < len && bytes[i + count] == ch && count < 255) {
            count++;
        }
        unsigned char rle[2] = { (unsigned char)count, ch };
        total += stream_write(ctx->inner, rle, 2);
        i += count;
    }
    return total;
}

static int compress_flush(Stream *s) {
    CompressCtx *ctx = s->ctx;
    return stream_flush(ctx->inner);
}

static void compress_close(Stream *s) {
    CompressCtx *ctx = s->ctx;
    stream_close(ctx->inner);
    free(ctx);
    free(s);
}

static const StreamOps compress_ops = {
    .write = compress_write,
    .flush = compress_flush,
    .close = compress_close,
};

Stream *compress_stream(Stream *inner) {
    CompressCtx *ctx = malloc(sizeof(*ctx));
    ctx->inner = inner;

    Stream *s = malloc(sizeof(*s));
    s->ops = &compress_ops;
    s->ctx = ctx;
    return s;
}
```

### 2.5 Encadenar decoradores

```c
/* main.c */
#include "stream.h"
#include <string.h>

/* Declaraciones de factory */
Stream *file_stream_open(const char *path);
Stream *buffered_stream(Stream *inner, size_t buf_size);
Stream *compress_stream(Stream *inner);

int main(void) {
    /* Construir la cadena: file → buffer → compress */
    Stream *file   = file_stream_open("/tmp/output.dat");
    Stream *buf    = buffered_stream(file, 4096);
    Stream *output = compress_stream(buf);

    /* Escribir — pasa por compress → buffer → file */
    const char *msg = "AAAAAABBBCCDDDDDDD";
    stream_write(output, msg, strlen(msg));

    /* Cerrar cierra toda la cadena */
    stream_close(output);
    return 0;
}
```

```
  Flujo de datos:
  ───────────────

  stream_write(output, "AAAAAABBBCCDDDDDDD")
  │
  ├─ compress_write()
  │   Convierte a RLE: [6,'A'] [3,'B'] [2,'C'] [7,'D']
  │   Llama stream_write(inner=buf, rle_data)
  │   │
  │   ├─ buffered_write()
  │   │   Acumula en buffer de 4096 bytes
  │   │   Cuando buffer lleno: stream_write(inner=file, buffer)
  │   │   │
  │   │   ├─ file_write()
  │   │   │   fwrite() al disco
  │   │   │
  │   │   └─ retorna bytes escritos
  │   └─ retorna bytes escritos
  └─ retorna bytes escritos

  stream_close(output)
  │
  ├─ compress_close() → stream_close(buf)
  │                      ├─ buffered_close() → flush + stream_close(file)
  │                      │                      └─ file_close() → fclose()
  │                      └─ free ctx, free s
  └─ free ctx, free s
```

---

## 3. Anatomia del patron

```
  Todos los componentes implementan StreamOps:

  ┌──────────────┐    ┌──────────────┐    ┌──────────────┐
  │ CompressStream│    │BufferedStream│    │  FileStream   │
  ├──────────────┤    ├──────────────┤    ├──────────────┤
  │ ops: compress│    │ ops: buffered│    │ ops: file_ops│
  │ ctx:          │    │ ctx:          │    │ ctx:          │
  │  └─inner: ───┼───>│  └─inner: ───┼───>│  └─file: FILE│
  │              │    │  └─buffer[]  │    │              │
  └──────────────┘    └──────────────┘    └──────────────┘

  El caller solo ve: Stream *output
  No sabe que hay 3 layers.
  Cualquier combinacion funciona porque todos implementan StreamOps.
```

Propiedades del patron:
1. **Misma interfaz**: todos son `Stream *` con `StreamOps`
2. **Composicion**: cada decorator tiene un `inner` que es otro `Stream *`
3. **Delegacion**: cada operacion procesa y luego llama `inner->ops->write`
4. **Transparencia**: el caller no sabe cuantos layers hay
5. **Open/Closed**: agregar un decorator no modifica los existentes

---

## 4. Decorator de logging (ejemplo completo)

Un caso clasico: agregar logging a cualquier stream sin
modificar el stream original:

```c
/* logging_stream.c */
#include "stream.h"
#include <stdio.h>
#include <stdlib.h>

typedef struct {
    Stream     *inner;
    FILE       *log_file;
    const char *label;
    size_t      total_bytes;
    size_t      total_writes;
} LogCtx;

static int logging_write(Stream *s, const void *data, size_t len) {
    LogCtx *ctx = s->ctx;
    ctx->total_bytes += len;
    ctx->total_writes++;

    fprintf(ctx->log_file, "[%s] write #%zu: %zu bytes (total: %zu)\n",
            ctx->label, ctx->total_writes, len, ctx->total_bytes);

    /* Delegar al inner */
    return stream_write(ctx->inner, data, len);
}

static int logging_flush(Stream *s) {
    LogCtx *ctx = s->ctx;
    fprintf(ctx->log_file, "[%s] flush (total: %zu bytes in %zu writes)\n",
            ctx->label, ctx->total_bytes, ctx->total_writes);
    return stream_flush(ctx->inner);
}

static void logging_close(Stream *s) {
    LogCtx *ctx = s->ctx;
    fprintf(ctx->log_file, "[%s] close (final: %zu bytes in %zu writes)\n",
            ctx->label, ctx->total_bytes, ctx->total_writes);
    stream_close(ctx->inner);
    free(ctx);
    free(s);
}

static const StreamOps logging_ops = {
    .write = logging_write,
    .flush = logging_flush,
    .close = logging_close,
};

Stream *logging_stream(Stream *inner, FILE *log_file, const char *label) {
    LogCtx *ctx = malloc(sizeof(*ctx));
    ctx->inner        = inner;
    ctx->log_file     = log_file;
    ctx->label        = label;
    ctx->total_bytes  = 0;
    ctx->total_writes = 0;

    Stream *s = malloc(sizeof(*s));
    s->ops = &logging_ops;
    s->ctx = ctx;
    return s;
}
```

```c
/* Uso: logging en multiples puntos de la cadena */
Stream *file   = file_stream_open("/tmp/data.bin");
Stream *log1   = logging_stream(file, stderr, "after-file");
Stream *buf    = buffered_stream(log1, 4096);
Stream *log2   = logging_stream(buf, stderr, "after-buffer");
Stream *output = compress_stream(log2);

stream_write(output, "hello", 5);
stream_close(output);

/* Salida en stderr:
   [after-buffer] write #1: 10 bytes (total: 10)
   [after-file] write #1: 10 bytes (total: 10)
   [after-buffer] flush (total: 10 bytes in 1 writes)
   [after-file] flush (total: 10 bytes in 1 writes)
   ...
*/
```

---

## 5. Decorator con estado mutable

### 5.1 Rate limiter

```c
/* ratelimit_stream.c */
#include "stream.h"
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

typedef struct {
    Stream *inner;
    size_t  max_bytes_per_sec;
    size_t  bytes_this_second;
    time_t  current_second;
} RateLimitCtx;

static int ratelimit_write(Stream *s, const void *data, size_t len) {
    RateLimitCtx *ctx = s->ctx;
    time_t now = time(NULL);

    /* Reset del contador cada segundo */
    if (now != ctx->current_second) {
        ctx->current_second = now;
        ctx->bytes_this_second = 0;
    }

    /* Esperar si excedemos el limite */
    while (ctx->bytes_this_second + len > ctx->max_bytes_per_sec) {
        usleep(100000);  /* 100ms */
        now = time(NULL);
        if (now != ctx->current_second) {
            ctx->current_second = now;
            ctx->bytes_this_second = 0;
        }
    }

    ctx->bytes_this_second += len;
    return stream_write(ctx->inner, data, len);
}

static int ratelimit_flush(Stream *s) {
    RateLimitCtx *ctx = s->ctx;
    return stream_flush(ctx->inner);
}

static void ratelimit_close(Stream *s) {
    RateLimitCtx *ctx = s->ctx;
    stream_close(ctx->inner);
    free(ctx);
    free(s);
}

static const StreamOps ratelimit_ops = {
    .write = ratelimit_write,
    .flush = ratelimit_flush,
    .close = ratelimit_close,
};

Stream *ratelimit_stream(Stream *inner, size_t max_bytes_per_sec) {
    RateLimitCtx *ctx = malloc(sizeof(*ctx));
    ctx->inner             = inner;
    ctx->max_bytes_per_sec = max_bytes_per_sec;
    ctx->bytes_this_second = 0;
    ctx->current_second    = time(NULL);

    Stream *s = malloc(sizeof(*s));
    s->ops = &ratelimit_ops;
    s->ctx = ctx;
    return s;
}
```

### 5.2 Encriptar

```c
/* encrypt_stream.c */
#include "stream.h"
#include <stdlib.h>

typedef struct {
    Stream       *inner;
    unsigned char key;  /* XOR key (simplificado) */
} EncryptCtx;

static int encrypt_write(Stream *s, const void *data, size_t len) {
    EncryptCtx *ctx = s->ctx;
    const unsigned char *src = data;

    /* XOR cada byte (cifrado simetrico trivial) */
    unsigned char *encrypted = malloc(len);
    for (size_t i = 0; i < len; i++) {
        encrypted[i] = src[i] ^ ctx->key;
    }

    int result = stream_write(ctx->inner, encrypted, len);
    free(encrypted);
    return result;
}

static int encrypt_flush(Stream *s) {
    EncryptCtx *ctx = s->ctx;
    return stream_flush(ctx->inner);
}

static void encrypt_close(Stream *s) {
    EncryptCtx *ctx = s->ctx;
    stream_close(ctx->inner);
    free(ctx);
    free(s);
}

static const StreamOps encrypt_ops = {
    .write = encrypt_write,
    .flush = encrypt_flush,
    .close = encrypt_close,
};

Stream *encrypt_stream(Stream *inner, unsigned char key) {
    EncryptCtx *ctx = malloc(sizeof(*ctx));
    ctx->inner = inner;
    ctx->key   = key;

    Stream *s = malloc(sizeof(*s));
    s->ops = &encrypt_ops;
    s->ctx = ctx;
    return s;
}
```

### 5.3 Composicion flexible

```c
/* Diferentes configuraciones, mismo codigo de escritura */

/* Config 1: file + buffer */
Stream *s = buffered_stream(file_stream_open("out.dat"), 8192);

/* Config 2: file + encrypt + buffer */
Stream *s = buffered_stream(
                encrypt_stream(
                    file_stream_open("out.dat"), 0x42),
                8192);

/* Config 3: file + buffer + compress + logging + ratelimit */
Stream *file = file_stream_open("out.dat");
Stream *buf  = buffered_stream(file, 8192);
Stream *comp = compress_stream(buf);
Stream *log  = logging_stream(comp, stderr, "output");
Stream *s    = ratelimit_stream(log, 1024 * 1024);

/* El caller solo usa stream_write(s, ...) en todos los casos */
```

```
  Combinaciones posibles con 5 decoradores:
  ─────────────────────────────────────────
  Solo file:              1 forma
  File + 1 decorator:     5 formas
  File + 2 decorators:    5*4 = 20 formas
  File + 3 decorators:    5*4*3 = 60 formas
  ...

  Cada decorator es independiente.
  N decoradores dan N! combinaciones.
  Con herencia necesitarias una clase por combinacion.
```

---

## 6. El orden importa

```c
/* Orden A: encrypt → compress → file */
Stream *s = compress_stream(
                encrypt_stream(
                    file_stream_open("out.dat"), 0x42));
/* Datos: original → encrypt → compress → write
   Problema: datos encriptados no se comprimen bien
   (parecen aleatorios) */

/* Orden B: compress → encrypt → file */
Stream *s = encrypt_stream(
                compress_stream(
                    file_stream_open("out.dat")), 0x42);
/* Datos: original → compress → encrypt → write
   Mejor: comprime primero (patron visible), luego cifra */
```

```
  Reglas de orden tipicas:
  ────────────────────────
  Compress antes de encrypt (datos comprimibles antes de cifrar)
  Buffer cerca del I/O (reduce syscalls)
  Logging donde quieras observar (antes o despues de transformaciones)
  Rate limit en el extremo exterior (limita la tasa total)
```

---

## 7. Decorator en el mundo real

### 7.1 stdio.h: FILE con buffering

```
  fwrite(data, 1, len, file)
  │
  ├─ Buffer interno de FILE (como BufferedStream)
  │   Acumula datos hasta que el buffer esta lleno
  │
  ├─ write() syscall (cuando buffer lleno o fflush)
  │
  └─ El kernel tiene su propio buffer (page cache)
     → disco

  FILE es un decorator sobre file descriptors:
  - Agrega buffering (setvbuf)
  - Agrega formateo (fprintf)
  - Misma interfaz conceptual (read/write/close)
```

### 7.2 Linux: I/O stack como cadena de decoradores

```
  App: write(fd, data, len)
  │
  ├─ VFS layer (interfaz comun)
  ├─ Filesystem (ext4, xfs, btrfs)
  │   ├─ Journaling (decorator: agrega transaccionalidad)
  │   ├─ Encryption (fscrypt: decorator de cifrado)
  │   └─ Compression (btrfs: decorator de compresion)
  ├─ Block layer
  │   ├─ I/O scheduler (decorator: reordena requests)
  │   └─ dm-crypt (decorator: cifrado de disco)
  ├─ SCSI/NVMe driver
  └─ Hardware
```

---

## 8. Ownership en la cadena

### 8.1 Quien libera que

```
  Regla: cada decorator es dueno de su inner.
  close() libera en cascada.

  stream_close(output)
  │ output es dueno de buf
  ├─ compress_close(): free(ctx), free(s)
  │   └─ stream_close(buf)
  │      │ buf es dueno de file
  │      ├─ buffered_close(): flush, free(buffer), free(ctx), free(s)
  │      │   └─ stream_close(file)
  │      │      └─ file_close(): fclose, free(ctx), free(s)

  Solo llamas stream_close() en el MAS EXTERNO.
  El libera toda la cadena.
```

### 8.2 Problema: doble close

```c
Stream *file = file_stream_open("out.dat");
Stream *buf  = buffered_stream(file, 4096);

stream_close(buf);   /* Cierra buf → cierra file ✓ */
stream_close(file);  /* DOUBLE FREE — file ya fue cerrado! */
```

**Regla**: una vez que envuelves un stream en un decorator,
solo usas el decorator. No guardes referencia al inner.

```c
/* CORRECTO: no guardar referencia al inner */
Stream *output = buffered_stream(
                     file_stream_open("out.dat"), 4096);
/* Solo usas `output`. file_stream no se guarda. */
stream_close(output);
```

---

## 9. Errores comunes

| Error | Por que falla | Solucion |
|---|---|---|
| Decorator no delega al inner | Operacion se pierde en la cadena | Siempre llamar `stream_X(ctx->inner, ...)` |
| close() no cierra inner | Leak del stream base | Cascada: cada close() cierra su inner |
| Guardar referencia al inner | Use-after-free cuando decorator cierra | No guardar ref, solo usar el decorator externo |
| Orden incorrecto de decoradores | Compress despues de encrypt no comprime | Pensar en el flujo de datos |
| flush() no delega | Datos quedan en un buffer intermedio | Cada flush() llama `stream_flush(inner)` |
| Decorator sin estado compartido | Cada instancia tiene su propio ctx | Correcto — es feature, no bug |
| Demasiados layers | Overhead de indirecion | Maximo 3-4 layers en practica |
| Mezclar responsabilidades en un decorator | Un decorator que comprime Y cifra | Un decorator = una responsabilidad |

---

## 10. Ejercicios

### Ejercicio 1 — Decorator de conteo

Implementa `CountingStream` que cuenta bytes totales y numero
de writes, sin modificar los datos.

**Prediccion**: ¿el counting decorator modifica `data` antes de
delegarlo al inner?

<details>
<summary>Respuesta</summary>

```c
typedef struct {
    Stream *inner;
    size_t  total_bytes;
    size_t  write_count;
} CountCtx;

static int counting_write(Stream *s, const void *data, size_t len) {
    CountCtx *ctx = s->ctx;
    ctx->total_bytes += len;
    ctx->write_count++;
    /* NO modifica data — solo observa */
    return stream_write(ctx->inner, data, len);
}

static int counting_flush(Stream *s) {
    CountCtx *ctx = s->ctx;
    return stream_flush(ctx->inner);
}

static void counting_close(Stream *s) {
    CountCtx *ctx = s->ctx;
    stream_close(ctx->inner);
    free(ctx);
    free(s);
}

static const StreamOps counting_ops = {
    .write = counting_write,
    .flush = counting_flush,
    .close = counting_close,
};

Stream *counting_stream(Stream *inner, size_t *out_bytes,
                        size_t *out_count) {
    CountCtx *ctx = malloc(sizeof(*ctx));
    ctx->inner       = inner;
    ctx->total_bytes = 0;
    ctx->write_count = 0;

    Stream *s = malloc(sizeof(*s));
    s->ops = &counting_ops;
    s->ctx = ctx;
    return s;
}

/* Para leer las stats necesitas acceso al ctx.
   Alternativa: guardar punteros a las stats: */
typedef struct {
    Stream *inner;
    size_t *total_bytes_ptr;
    size_t *write_count_ptr;
} CountCtxShared;
```

No, el counting decorator NO modifica data. Es un decorator
"pasivo" — observa el trafico sin transformarlo. Esto es comun
para monitoring, metricas, y debugging. El data se pasa intacto
al inner.

</details>

---

### Ejercicio 2 — Decorator de prefijo

Implementa `PrefixStream` que antepone un string a cada write.
Ej: write("hello") → inner recibe "[APP] hello".

**Prediccion**: ¿puedes hacer dos writes al inner (uno para el
prefijo, uno para data) o debes concatenar?

<details>
<summary>Respuesta</summary>

Dos writes es mas eficiente (sin malloc para concatenar):

```c
typedef struct {
    Stream     *inner;
    const char *prefix;
    size_t      prefix_len;
} PrefixCtx;

static int prefix_write(Stream *s, const void *data, size_t len) {
    PrefixCtx *ctx = s->ctx;
    int total = 0;

    /* Escribir prefijo */
    total += stream_write(ctx->inner, ctx->prefix, ctx->prefix_len);

    /* Escribir datos originales */
    total += stream_write(ctx->inner, data, len);

    return total;
}

static int prefix_flush(Stream *s) {
    PrefixCtx *ctx = s->ctx;
    return stream_flush(ctx->inner);
}

static void prefix_close(Stream *s) {
    PrefixCtx *ctx = s->ctx;
    stream_close(ctx->inner);
    free(ctx);
    free(s);
}

static const StreamOps prefix_ops = {
    .write = prefix_write,
    .flush = prefix_flush,
    .close = prefix_close,
};

Stream *prefix_stream(Stream *inner, const char *prefix) {
    PrefixCtx *ctx = malloc(sizeof(*ctx));
    ctx->inner      = inner;
    ctx->prefix     = prefix;  /* Caller must keep alive */
    ctx->prefix_len = strlen(prefix);

    Stream *s = malloc(sizeof(*s));
    s->ops = &prefix_ops;
    s->ctx = ctx;
    return s;
}
```

Dos writes al inner es mejor porque:
- Sin malloc para concatenar (zero alloc)
- Si el inner es buffered, ambos writes van al mismo buffer
- Funciona con cualquier tamano de data

La alternativa (concatenar) requiere `malloc(prefix_len + len)`
en cada write — innecesario.

</details>

---

### Ejercicio 3 — Cadena de 3 decoradores

Combina FileStream + PrefixStream + BufferedStream. Escribe
"hello" tres veces y cierra.

**Prediccion**: ¿en que orden construyes la cadena para que
el prefijo se agregue ANTES del buffering?

<details>
<summary>Respuesta</summary>

```c
int main(void) {
    /* Orden: file (base) → prefix (agrega) → buffer (acumula) */
    Stream *file   = file_stream_open("/tmp/decorated.log");
    Stream *prefix = prefix_stream(file, "[APP] ");
    Stream *output = buffered_stream(prefix, 4096);

    stream_write(output, "hello\n", 6);
    stream_write(output, "world\n", 6);
    stream_write(output, "done\n", 5);

    stream_close(output);  /* flush buffer → prefix → file → close all */
    return 0;
}

/* Archivo contiene:
   [APP] hello
   [APP] world
   [APP] done
*/
```

Orden de construccion: file → prefix → buffer (de adentro hacia
afuera). El flujo de datos es inverso: buffer → prefix → file.

```
  stream_write(output, "hello\n")
  │
  ├─ buffered_write() — acumula en buffer
  │   Cuando flush:
  │   ├─ prefix_write() — "[APP] " + datos
  │   │   ├─ file_write("[APP] ") — al disco
  │   │   └─ file_write("hello\n") — al disco
```

Si el orden fuera file → buffer → prefix, el prefijo se
agregaria al buffer pero el buffer podria cortar entre prefijo
y datos al hacer flush — resultado corrupto.

</details>

---

### Ejercicio 4 — Decorator de checksum

Implementa un decorator que calcula un checksum (suma de bytes)
de todo lo escrito, y lo escribe al inner al cerrar.

**Prediccion**: ¿donde escribes el checksum: en write, flush, o close?

<details>
<summary>Respuesta</summary>

En close — cuando toda la data fue escrita:

```c
typedef struct {
    Stream  *inner;
    uint32_t checksum;
} ChecksumCtx;

static int checksum_write(Stream *s, const void *data, size_t len) {
    ChecksumCtx *ctx = s->ctx;
    const unsigned char *bytes = data;

    /* Acumular checksum */
    for (size_t i = 0; i < len; i++) {
        ctx->checksum += bytes[i];
    }

    /* Pasar data sin modificar */
    return stream_write(ctx->inner, data, len);
}

static int checksum_flush(Stream *s) {
    ChecksumCtx *ctx = s->ctx;
    return stream_flush(ctx->inner);
}

static void checksum_close(Stream *s) {
    ChecksumCtx *ctx = s->ctx;

    /* Escribir el checksum final como 4 bytes al inner */
    unsigned char sum[4];
    sum[0] = (ctx->checksum >> 24) & 0xFF;
    sum[1] = (ctx->checksum >> 16) & 0xFF;
    sum[2] = (ctx->checksum >> 8)  & 0xFF;
    sum[3] = ctx->checksum & 0xFF;
    stream_write(ctx->inner, sum, 4);

    stream_close(ctx->inner);
    free(ctx);
    free(s);
}

static const StreamOps checksum_ops = {
    .write = checksum_write,
    .flush = checksum_flush,
    .close = checksum_close,
};

Stream *checksum_stream(Stream *inner) {
    ChecksumCtx *ctx = malloc(sizeof(*ctx));
    ctx->inner    = inner;
    ctx->checksum = 0;

    Stream *s = malloc(sizeof(*s));
    s->ops = &checksum_ops;
    s->ctx = ctx;
    return s;
}
```

En close porque el checksum debe cubrir TODO lo escrito. Si lo
escribieras en cada write, tendrias checksums parciales. En
flush, podria ejecutarse multiples veces. Close se ejecuta
exactamente una vez, cuando toda la data fue procesada.

</details>

---

### Ejercicio 5 — Decorator vs modificar el original

Tienes un FileStream que funciona. Necesitas agregar buffering.

Opcion A: modificar file_stream.c para agregar buffer interno.
Opcion B: crear buffered_stream como decorator.

**Prediccion**: ¿cual es mejor? ¿Cuando elegirias A?

<details>
<summary>Respuesta</summary>

**Opcion B (decorator) es casi siempre mejor** porque:

1. **Open/Closed**: FileStream no se modifica
2. **Composicion**: el buffer se puede agregar o quitar sin
   cambiar nada
3. **Reutilizacion**: BufferedStream funciona con cualquier stream,
   no solo FileStream
4. **Testing**: puedes testear FileStream y BufferedStream por separado

**Elegirias A cuando**:
- El buffering es inseparable del FileStream (como `FILE *` de stdio)
- El overhead de la indirecion extra es inaceptable (rare)
- Solo hay un tipo de stream y nunca habra otro (rare)
- El buffer necesita acceso a internals de FileStream que no estan
  en la interfaz publica

```
  Opcion A (modificar):           Opcion B (decorator):
  ─────────────────────           ─────────────────────
  1 struct (FileStream con buf)   2 structs independientes
  Sin overhead de indirecion       1 nivel extra de vtable call
  Solo FileStream tiene buffer    Cualquier stream puede tener buffer
  FileStream se vuelve complejo   Cada componente es simple
  No puedes tener FileStream      Puedes elegir: con o sin buffer
    sin buffer
```

La respuesta tipica en C de produccion es B. Ejemplo: POSIX
tiene `setvbuf()` como propiedad de FILE (opcion A), pero el
I/O stack del kernel usa decoradores (opcion B).

</details>

---

### Ejercicio 6 — Decorator de retry

Implementa un decorator que reintenta writes fallidos hasta N veces.

**Prediccion**: ¿que codigo de retorno de `stream_write(inner, ...)`
indica fallo?

<details>
<summary>Respuesta</summary>

```c
typedef struct {
    Stream *inner;
    int     max_retries;
    int     retry_delay_ms;
} RetryCtx;

static int retry_write(Stream *s, const void *data, size_t len) {
    RetryCtx *ctx = s->ctx;

    for (int attempt = 0; attempt <= ctx->max_retries; attempt++) {
        int result = stream_write(ctx->inner, data, len);
        if (result >= 0) {
            return result;  /* Exito */
        }

        /* Fallo — esperar y reintentar */
        if (attempt < ctx->max_retries) {
            usleep(ctx->retry_delay_ms * 1000);
        }
    }

    return -1;  /* Todos los reintentos fallaron */
}

static int retry_flush(Stream *s) {
    RetryCtx *ctx = s->ctx;
    return stream_flush(ctx->inner);
}

static void retry_close(Stream *s) {
    RetryCtx *ctx = s->ctx;
    stream_close(ctx->inner);
    free(ctx);
    free(s);
}

static const StreamOps retry_ops = {
    .write = retry_write,
    .flush = retry_flush,
    .close = retry_close,
};

Stream *retry_stream(Stream *inner, int max_retries, int delay_ms) {
    RetryCtx *ctx = malloc(sizeof(*ctx));
    ctx->inner          = inner;
    ctx->max_retries    = max_retries;
    ctx->retry_delay_ms = delay_ms;

    Stream *s = malloc(sizeof(*s));
    s->ops = &retry_ops;
    s->ctx = ctx;
    return s;
}
```

En nuestra interfaz, `stream_write` retorna el numero de bytes
escritos. Un valor `< 0` indica error (convencion POSIX).
Un valor `>= 0` pero `< len` podria indicar escritura parcial
(como `write(2)` del kernel). El decorator reintenta si el
resultado es negativo.

</details>

---

### Ejercicio 7 — Orden de decoradores

Dado: `encrypt_stream`, `compress_stream`, `buffered_stream`.

¿En que orden los compones sobre `file_stream`?

**Prediccion**: ¿por que compress antes de encrypt?

<details>
<summary>Respuesta</summary>

```c
Stream *file = file_stream_open("out.dat");
Stream *buf  = buffered_stream(file, 8192);     /* 1. Cerca de I/O */
Stream *enc  = encrypt_stream(buf, 0x42);        /* 2. Cifra bloques */
Stream *comp = compress_stream(enc);             /* 3. Comprime original */

/* INCORRECTO — esto es al reves */
```

Espera — el flujo de datos es de afuera hacia adentro:
```
write(comp, data)
  → compress_write() → stream_write(enc, compressed)
    → encrypt_write() → stream_write(buf, encrypted)
      → buffered_write() → acumula, luego stream_write(file)
```

Queremos: data → **compress** → **encrypt** → **buffer** → file.
Entonces la construccion es:

```c
Stream *file   = file_stream_open("out.dat");
Stream *buf    = buffered_stream(file, 8192);   /* Mas interno */
Stream *enc    = encrypt_stream(buf, 0x42);
Stream *output = compress_stream(enc);           /* Mas externo */
```

Compress es el mas externo: recibe data original (comprimible),
la comprime, y pasa al encrypt. Encrypt recibe data comprimida
y la cifra. Buffer acumula bloques cifrados y los escribe al file.

Si encrypt fuera externo: recibiria data original, la cifraria
(parece aleatoria), y compress intentaria comprimir datos
aleatorios — resultado mas grande, no mas pequeno.

</details>

---

### Ejercicio 8 — Decorator generico con callback

Implementa un decorator que ejecuta un callback definido por
el usuario antes de cada write.

**Prediccion**: ¿como pasas contexto al callback en C?

<details>
<summary>Respuesta</summary>

```c
typedef int (*WriteHook)(const void *data, size_t len, void *user_data);

typedef struct {
    Stream    *inner;
    WriteHook  hook;
    void      *user_data;
} HookCtx;

static int hook_write(Stream *s, const void *data, size_t len) {
    HookCtx *ctx = s->ctx;

    /* Ejecutar callback antes del write */
    int hook_result = ctx->hook(data, len, ctx->user_data);
    if (hook_result != 0) {
        return -1;  /* Hook cancelo el write */
    }

    return stream_write(ctx->inner, data, len);
}

static int hook_flush(Stream *s) {
    HookCtx *ctx = s->ctx;
    return stream_flush(ctx->inner);
}

static void hook_close(Stream *s) {
    HookCtx *ctx = s->ctx;
    stream_close(ctx->inner);
    free(ctx);
    free(s);
}

static const StreamOps hook_ops = {
    .write = hook_write,
    .flush = hook_flush,
    .close = hook_close,
};

Stream *hook_stream(Stream *inner, WriteHook hook, void *user_data) {
    HookCtx *ctx = malloc(sizeof(*ctx));
    ctx->inner     = inner;
    ctx->hook      = hook;
    ctx->user_data = user_data;

    Stream *s = malloc(sizeof(*s));
    s->ops = &hook_ops;
    s->ctx = ctx;
    return s;
}

/* Ejemplo: hook que rechaza writes mayores a N bytes */
int max_size_hook(const void *data, size_t len, void *user_data) {
    size_t max = *(size_t *)user_data;
    if (len > max) {
        fprintf(stderr, "Write rejected: %zu > %zu bytes\n", len, max);
        return -1;  /* Cancelar */
    }
    return 0;  /* Permitir */
}

/* Uso: */
size_t max_write = 1024;
Stream *output = hook_stream(
    file_stream_open("out.dat"),
    max_size_hook,
    &max_write
);
```

El contexto se pasa via `void *user_data` — el pattern standard
de C para closures manuales. El hook recibe los datos, el tamano,
y su contexto propio. Puede retornar 0 (continuar) o no-0 (cancelar).

</details>

---

### Ejercicio 9 — Tee decorator (duplicar output)

Implementa un decorator que escribe a dos streams simultaneamente
(como el comando `tee`).

**Prediccion**: ¿que pasa si el write al segundo stream falla?

<details>
<summary>Respuesta</summary>

```c
typedef struct {
    Stream *primary;
    Stream *secondary;
} TeeCtx;

static int tee_write(Stream *s, const void *data, size_t len) {
    TeeCtx *ctx = s->ctx;

    int r1 = stream_write(ctx->primary, data, len);

    /* Escribir al secundario independientemente del resultado primario */
    int r2 = stream_write(ctx->secondary, data, len);

    /* Retornar resultado del primario (es el "principal") */
    (void)r2;
    return r1;
}

static int tee_flush(Stream *s) {
    TeeCtx *ctx = s->ctx;
    int r1 = stream_flush(ctx->primary);
    stream_flush(ctx->secondary);
    return r1;
}

static void tee_close(Stream *s) {
    TeeCtx *ctx = s->ctx;
    stream_close(ctx->primary);
    stream_close(ctx->secondary);
    free(ctx);
    free(s);
}

static const StreamOps tee_ops = {
    .write = tee_write,
    .flush = tee_flush,
    .close = tee_close,
};

Stream *tee_stream(Stream *primary, Stream *secondary) {
    TeeCtx *ctx = malloc(sizeof(*ctx));
    ctx->primary   = primary;
    ctx->secondary = secondary;

    Stream *s = malloc(sizeof(*s));
    s->ops = &tee_ops;
    s->ctx = ctx;
    return s;
}

/* Uso: escribir a archivo y a stdout */
Stream *file   = file_stream_open("log.txt");
Stream *stdout_s = /* stdout stream */;
Stream *output = tee_stream(file, stdout_s);

stream_write(output, "hello\n", 6);
/* Aparece en log.txt Y en stdout */
```

Si el write al secundario falla: se ignora. El primario es el
"importante" y su resultado se retorna al caller. El secundario
es best-effort (como el comando `tee` que no falla si el
archivo secundario tiene problemas).

Alternativa: retornar error si cualquiera falla — depende del
caso de uso.

</details>

---

### Ejercicio 10 — Construir cadena desde config

Escribe una funcion que construye una cadena de decoradores a
partir de una configuracion:

```c
typedef struct {
    const char *output_path;
    int         use_buffering;
    size_t      buffer_size;
    int         use_compression;
    int         use_encryption;
    unsigned char encrypt_key;
    int         use_logging;
} StreamConfig;
```

**Prediccion**: ¿en que orden aplicas los decoradores?

<details>
<summary>Respuesta</summary>

```c
Stream *build_stream(const StreamConfig *cfg) {
    Stream *s = file_stream_open(cfg->output_path);
    if (!s) return NULL;

    /* 1. Buffer cerca del I/O (reduce syscalls) */
    if (cfg->use_buffering) {
        s = buffered_stream(s, cfg->buffer_size);
    }

    /* 2. Encrypt despues del buffer (cifra bloques completos) */
    if (cfg->use_encryption) {
        s = encrypt_stream(s, cfg->encrypt_key);
    }

    /* 3. Compress en el exterior (comprime datos originales) */
    if (cfg->use_compression) {
        s = compress_stream(s);
    }

    /* 4. Logging en el extremo (observa datos como los ve el caller) */
    if (cfg->use_logging) {
        s = logging_stream(s, stderr, "output");
    }

    return s;
}

/* Uso: */
StreamConfig cfg = {
    .output_path     = "/tmp/data.bin",
    .use_buffering   = 1,
    .buffer_size     = 8192,
    .use_compression = 1,
    .use_encryption  = 1,
    .encrypt_key     = 0x42,
    .use_logging     = 1,
};

Stream *output = build_stream(&cfg);
stream_write(output, "hello world", 11);
stream_close(output);
```

Flujo resultante:
```
  write("hello world")
  → logging (observa "hello world")
  → compress (comprime "hello world")
  → encrypt (cifra datos comprimidos)
  → buffer (acumula datos cifrados)
  → file (escribe al disco)
```

Este patron (construir cadena desde config) es exactamente lo
que hacen frameworks como Log4j (appenders), nginx (filters),
y middleware stacks en web servers. La config determina que
decoradores se activan sin cambiar el codigo.

</details>
