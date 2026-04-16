# Facade en C

## 1. El problema: complejidad interna expuesta

Un subsistema real tiene muchos módulos internos, cada uno con su propia API.
Obligar al usuario a coordinar todos estos módulos es frágil y confuso:

```
Sin Facade:                         Con Facade:

// El usuario coordina 5 módulos    // El usuario llama UNA función
config_load("db.conf");             Database *db = db_open("db.conf");
log_init(LOG_FILE);                 db_query(db, "SELECT ...");
pool_create(10);                    db_close(db);
conn = pool_acquire();
auth_check(conn, credentials);
result = query_exec(conn, sql);
pool_release(conn);
log_write("query done");
```

El Facade **no elimina** la complejidad: la **oculta** detrás de una interfaz
simple. Los módulos internos siguen existiendo, pero el usuario no necesita
conocerlos.

```
┌─────────────────────────────────────┐
│            Usuario                   │
└──────────────┬──────────────────────┘
               │  API simple
       ┌───────▼───────┐
       │    FACADE      │  ← header público (database.h)
       │   database.c   │
       └───┬───┬───┬───┘
           │   │   │      módulos internos
       ┌───▼┐ ┌▼──┐ ┌▼────┐
       │pool│ │log│ │query │  ← static linkage
       └────┘ └───┘ └─────┘
```

---

## 2. Mecanismo en C: header público + static linkage

C no tiene `private` o `public` como C++ o Rust. En su lugar:

| Mecanismo | Visibilidad | Rol en Facade |
|-----------|-------------|---------------|
| Header público (`.h`) | Cualquier archivo que lo incluya | La interfaz del Facade |
| `static` en funciones | Solo el `.c` donde se define | Módulos internos ocultos |
| No exportar en header | No accesible sin declarar | Oculto por omisión |
| Tipos opacos (`struct X;`) | Solo puntero, no campos | Ocultar representación |

**Regla fundamental**: lo que no está en el header público **no existe** para
el usuario. Las funciones `static` no pueden llamarse desde otros `.c`.

---

## 3. Ejemplo completo: subsistema de audio

### 3.1 Header público: la fachada

```c
// audio.h — Lo ÚNICO que el usuario ve
#ifndef AUDIO_H
#define AUDIO_H

#include <stdbool.h>

// Tipo opaco: el usuario no sabe qué hay dentro
typedef struct AudioSystem AudioSystem;

// API minimal: 5 funciones
AudioSystem *audio_init(const char *config_path);
bool         audio_play(AudioSystem *sys, const char *filename);
bool         audio_set_volume(AudioSystem *sys, float volume);  // 0.0 - 1.0
void         audio_stop(AudioSystem *sys);
void         audio_shutdown(AudioSystem *sys);

#endif
```

Esto es todo lo que el usuario necesita. No sabe que internamente hay:
- Un decoder (WAV, MP3, OGG)
- Un mixer de canales
- Un buffer ring
- Una interfaz con el hardware (ALSA/PulseAudio)
- Un sistema de logging

### 3.2 Módulos internos con static

```c
// audio.c — Implementación del facade
#include "audio.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// ============================================================
// Módulo interno: Decoder
// ============================================================

typedef enum { FMT_WAV, FMT_MP3, FMT_OGG, FMT_UNKNOWN } AudioFormat;

typedef struct {
    AudioFormat format;
    int         sample_rate;
    int         channels;
    float      *samples;       // PCM decodificado
    size_t      sample_count;
} DecodedAudio;

// static = invisible fuera de este archivo
static AudioFormat detect_format(const char *filename) {
    const char *ext = strrchr(filename, '.');
    if (!ext) return FMT_UNKNOWN;
    if (strcmp(ext, ".wav") == 0) return FMT_WAV;
    if (strcmp(ext, ".mp3") == 0) return FMT_MP3;
    if (strcmp(ext, ".ogg") == 0) return FMT_OGG;
    return FMT_UNKNOWN;
}

static DecodedAudio *decode_file(const char *filename) {
    AudioFormat fmt = detect_format(filename);
    if (fmt == FMT_UNKNOWN) return NULL;

    DecodedAudio *audio = calloc(1, sizeof(DecodedAudio));
    audio->format      = fmt;
    audio->sample_rate = 44100;
    audio->channels    = 2;

    // Simulado: en realidad llamaría a libsndfile, libmpg123, etc.
    audio->sample_count = 44100 * 2;  // 1 segundo estéreo
    audio->samples = calloc(audio->sample_count, sizeof(float));

    return audio;
}

static void decoded_audio_free(DecodedAudio *audio) {
    if (audio) {
        free(audio->samples);
        free(audio);
    }
}

// ============================================================
// Módulo interno: Mixer
// ============================================================

typedef struct {
    float  volume;        // 0.0 - 1.0
    float *output_buf;
    size_t buf_size;
} Mixer;

static Mixer *mixer_create(size_t buf_size) {
    Mixer *m = calloc(1, sizeof(Mixer));
    m->volume     = 1.0f;
    m->buf_size   = buf_size;
    m->output_buf = calloc(buf_size, sizeof(float));
    return m;
}

static void mixer_set_volume(Mixer *m, float vol) {
    if (vol < 0.0f) vol = 0.0f;
    if (vol > 1.0f) vol = 1.0f;
    m->volume = vol;
}

static void mixer_process(Mixer *m, const float *input, size_t count) {
    size_t to_process = count < m->buf_size ? count : m->buf_size;
    for (size_t i = 0; i < to_process; i++) {
        m->output_buf[i] = input[i] * m->volume;
    }
}

static void mixer_free(Mixer *m) {
    if (m) {
        free(m->output_buf);
        free(m);
    }
}

// ============================================================
// Módulo interno: Hardware output (simulado)
// ============================================================

typedef struct {
    int  device_fd;    // file descriptor del dispositivo
    bool active;
} HwOutput;

static HwOutput *hw_open(void) {
    HwOutput *hw = calloc(1, sizeof(HwOutput));
    hw->device_fd = -1;  // simulado
    hw->active = true;
    printf("[hw] Audio device opened\n");
    return hw;
}

static bool hw_write(HwOutput *hw, const float *buf, size_t count) {
    if (!hw->active) return false;
    // Simulado: escribiría a ALSA/PulseAudio
    (void)buf;
    (void)count;
    return true;
}

static void hw_close(HwOutput *hw) {
    if (hw) {
        if (hw->active) {
            printf("[hw] Audio device closed\n");
            hw->active = false;
        }
        free(hw);
    }
}

// ============================================================
// Módulo interno: Logger
// ============================================================

typedef struct {
    FILE *logfile;
} Logger;

static Logger *logger_create(const char *path) {
    Logger *log = calloc(1, sizeof(Logger));
    log->logfile = fopen(path, "a");
    if (!log->logfile) log->logfile = stderr;
    return log;
}

static void logger_write(Logger *log, const char *msg) {
    fprintf(log->logfile, "[audio] %s\n", msg);
}

static void logger_free(Logger *log) {
    if (log) {
        if (log->logfile && log->logfile != stderr) {
            fclose(log->logfile);
        }
        free(log);
    }
}

// ============================================================
// Struct opaco del Facade (definición completa, solo aquí)
// ============================================================

struct AudioSystem {
    Mixer    *mixer;
    HwOutput *hw;
    Logger   *log;
    bool      playing;
};
```

**Punto clave**: `struct AudioSystem` se declara como tipo opaco en el header
(`typedef struct AudioSystem AudioSystem;`) pero se **define** aquí. El usuario
solo puede manipularlo via puntero — no puede acceder a `sys->mixer`.

### 3.3 Implementación del Facade

```c
AudioSystem *audio_init(const char *config_path) {
    AudioSystem *sys = calloc(1, sizeof(AudioSystem));

    // Coordinar inicialización de todos los subsistemas
    sys->log   = logger_create(config_path ? config_path : "/dev/null");
    sys->mixer = mixer_create(4096);
    sys->hw    = hw_open();

    logger_write(sys->log, "Audio system initialized");
    return sys;
}

bool audio_play(AudioSystem *sys, const char *filename) {
    logger_write(sys->log, "Playing file");

    // 1. Decodificar
    DecodedAudio *decoded = decode_file(filename);
    if (!decoded) {
        logger_write(sys->log, "Failed to decode file");
        return false;
    }

    // 2. Mezclar (aplicar volumen)
    mixer_process(sys->mixer, decoded->samples, decoded->sample_count);

    // 3. Enviar al hardware
    bool ok = hw_write(sys->hw, sys->mixer->output_buf, decoded->sample_count);

    // 4. Limpiar
    decoded_audio_free(decoded);
    sys->playing = ok;

    logger_write(sys->log, ok ? "Playback started" : "Playback failed");
    return ok;
}

bool audio_set_volume(AudioSystem *sys, float volume) {
    mixer_set_volume(sys->mixer, volume);
    logger_write(sys->log, "Volume changed");
    return true;
}

void audio_stop(AudioSystem *sys) {
    sys->playing = false;
    logger_write(sys->log, "Playback stopped");
}

void audio_shutdown(AudioSystem *sys) {
    if (!sys) return;

    logger_write(sys->log, "Shutting down");

    // Orden inverso de inicialización
    hw_close(sys->hw);
    mixer_free(sys->mixer);
    logger_free(sys->log);
    free(sys);
}
```

### 3.4 Uso por el cliente

```c
int main(void) {
    AudioSystem *audio = audio_init("audio.log");

    audio_set_volume(audio, 0.75f);
    audio_play(audio, "music.wav");
    audio_play(audio, "effect.mp3");

    audio_stop(audio);
    audio_shutdown(audio);
    return 0;
}
```

El usuario nunca ve `Mixer`, `Decoder`, `HwOutput`, ni `Logger`. Solo ve
`AudioSystem *` y 5 funciones.

---

## 4. static linkage en detalle

### 4.1 Qué oculta static

```c
// En audio.c:
static void mixer_process(...) { ... }  // INVISIBLE fuera de audio.c
void audio_play(...) { ... }            // VISIBLE (si está en el header)
```

```
Tabla de símbolos del .o:

$ nm audio.o
0000000000001234 T audio_play        ← T = símbolo global (visible)
0000000000001100 t mixer_process     ← t = símbolo local (invisible)
0000000000001050 t decode_file       ← t = símbolo local
0000000000001300 T audio_shutdown    ← T = símbolo global
```

- `T` (mayúscula) = símbolo global, enlazable desde otros `.o`
- `t` (minúscula) = símbolo local, solo este `.o`

### 4.2 Múltiples archivos con coordinación

Para proyectos más grandes, los módulos internos pueden estar en archivos
separados con **headers internos**:

```
proyecto/
├── include/
│   └── audio.h            ← público (facade)
├── src/
│   ├── audio.c            ← facade (coordina)
│   ├── internal/
│   │   ├── decoder.h      ← header INTERNO (no se instala)
│   │   ├── decoder.c
│   │   ├── mixer.h
│   │   ├── mixer.c
│   │   ├── hw_output.h
│   │   ├── hw_output.c
│   │   ├── logger.h
│   │   └── logger.c
```

```c
// src/internal/mixer.h — Solo para uso interno
#ifndef INTERNAL_MIXER_H
#define INTERNAL_MIXER_H

// NO se instala con el proyecto
// NO es parte de la API pública
typedef struct Mixer Mixer;

Mixer *mixer_create(size_t buf_size);
void   mixer_set_volume(Mixer *m, float vol);
void   mixer_process(Mixer *m, const float *input, size_t count);
void   mixer_free(Mixer *m);

#endif
```

```c
// src/audio.c — Facade incluye headers internos
#include "audio.h"           // público
#include "internal/decoder.h" // interno
#include "internal/mixer.h"   // interno
#include "internal/hw_output.h"
#include "internal/logger.h"

// El facade orquesta los módulos
AudioSystem *audio_init(const char *config_path) {
    // ...
}
```

En este esquema, `static` se usa dentro de cada `.c` interno para funciones
auxiliares, y los headers internos exponen solo lo necesario al facade.

**Regla de distribución**: al instalar la biblioteca, solo se copia
`include/audio.h`. Los headers de `internal/` no se distribuyen.

---

## 5. Tipo opaco: ocultar la representación

El tipo opaco es la herramienta complementaria de static linkage:

```c
// audio.h (público)
typedef struct AudioSystem AudioSystem;  // forward declaration
// El usuario SOLO puede tener AudioSystem *
// No puede hacer: AudioSystem sys;          ← error: incomplete type
// No puede hacer: sys.mixer                  ← error: incomplete type
// No puede hacer: sizeof(AudioSystem)        ← error: incomplete type

// audio.c (privado)
struct AudioSystem {         // definición completa
    Mixer    *mixer;
    HwOutput *hw;
    Logger   *log;
    bool      playing;
};
// Solo audio.c puede acceder a los campos
```

```
Lo que el usuario puede hacer:        Lo que NO puede hacer:

AudioSystem *sys = audio_init(...);   AudioSystem sys;  // tamaño desconocido
audio_play(sys, "file.wav");          sys->mixer = ...;  // campos desconocidos
audio_shutdown(sys);                  sizeof(*sys);      // tamaño desconocido
```

**Ventaja para evolución**: puedes añadir campos a `AudioSystem` sin
recompilar el código del usuario. Solo recompilas `audio.c`.

---

## 6. Facade de inicialización: setup complejo en una llamada

Un uso muy común del Facade es simplificar la inicialización de un sistema
con muchas dependencias:

```c
// ssl_facade.h
typedef struct SslContext SslContext;

// UNA función en lugar de 15 llamadas a OpenSSL
SslContext *ssl_easy_init(const char *cert_path,
                          const char *key_path,
                          const char *ca_path);

int ssl_easy_connect(SslContext *ctx, const char *host, int port);
int ssl_easy_send(SslContext *ctx, const void *data, size_t len);
int ssl_easy_recv(SslContext *ctx, void *buf, size_t buf_size);
void ssl_easy_close(SslContext *ctx);
```

```c
// ssl_facade.c
#include <openssl/ssl.h>
#include <openssl/err.h>
// ... muchos headers de OpenSSL

struct SslContext {
    SSL_CTX *ssl_ctx;
    SSL     *ssl;
    BIO     *bio;
    int      socket_fd;
};

SslContext *ssl_easy_init(const char *cert_path,
                          const char *key_path,
                          const char *ca_path) {
    SslContext *ctx = calloc(1, sizeof(SslContext));

    // 15 pasos de inicialización que el usuario NO necesita conocer:
    SSL_library_init();
    SSL_load_error_strings();
    OpenSSL_add_all_algorithms();

    ctx->ssl_ctx = SSL_CTX_new(TLS_client_method());
    SSL_CTX_set_verify(ctx->ssl_ctx, SSL_VERIFY_PEER, NULL);

    if (ca_path) {
        SSL_CTX_load_verify_locations(ctx->ssl_ctx, ca_path, NULL);
    }
    if (cert_path) {
        SSL_CTX_use_certificate_file(ctx->ssl_ctx, cert_path, SSL_FILETYPE_PEM);
    }
    if (key_path) {
        SSL_CTX_use_PrivateKey_file(ctx->ssl_ctx, key_path, SSL_FILETYPE_PEM);
    }

    SSL_CTX_set_min_proto_version(ctx->ssl_ctx, TLS1_2_VERSION);
    // ... más configuración

    return ctx;
}
```

El usuario de `ssl_easy_init` no necesita saber nada de OpenSSL. Si luego
migras a LibreSSL o BoringSSL, solo cambias `ssl_facade.c`.

---

## 7. Facade con múltiples niveles

Los facades pueden apilarse. Un facade de nivel alto usa facades de nivel bajo:

```
┌─────────────────────────────────┐
│       game_engine.h              │  ← Facade nivel 2
│  game_init() / game_update()     │
└──────┬──────────┬───────────────┘
       │          │
┌──────▼───┐  ┌──▼──────────┐
│ audio.h   │  │ graphics.h   │     ← Facades nivel 1
│(nuestro)  │  │              │
└──────┬───┘  └──┬──────────┘
       │          │
   [decoder]  [renderer]            ← módulos internos
   [mixer]    [shader_mgr]
   [hw_out]   [texture_mgr]
```

```c
// game_engine.h — Facade de nivel 2
typedef struct GameEngine GameEngine;

GameEngine *game_init(const char *title, int width, int height);
void        game_update(GameEngine *engine);
void        game_shutdown(GameEngine *engine);
```

```c
// game_engine.c — Usa facades de nivel 1
#include "game_engine.h"
#include "audio.h"      // facade de audio
#include "graphics.h"   // facade de gráficos

struct GameEngine {
    AudioSystem    *audio;
    GraphicsSystem *gfx;
    bool            running;
};

GameEngine *game_init(const char *title, int width, int height) {
    GameEngine *engine = calloc(1, sizeof(GameEngine));
    engine->audio   = audio_init("game.log");
    engine->gfx     = graphics_init(title, width, height);
    engine->running = true;
    return engine;
}
```

Cada nivel oculta los detalles del nivel inferior. El usuario del game engine
no sabe que existe un mixer ni un shader manager.

---

## 8. Facade vs otros patrones

| | Facade | Adapter (T01) | Decorator (T01) |
|---|--------|--------------|----------------|
| **Propósito** | Simplificar API compleja | Convertir interfaz A → B | Añadir comportamiento |
| **Dirección** | Muchos módulos → 1 interfaz | 1 interfaz → otra interfaz | 1 interfaz → misma interfaz + extra |
| **Módulos internos** | Varios, coordinados | Uno, envuelto | Uno, decorado |
| **Conocimiento del usuario** | Mínimo | Conoce la interfaz destino | Conoce la interfaz base |
| **Ejemplo** | `audio_init()` oculta 4 módulos | SDL → nuestra API de audio | Logger que añade timestamp |

```
Facade:         Adapter:         Decorator:
  ┌───┐           ┌───┐           ┌────┐
  │ F │           │ A │           │ D  │
  └─┬─┘           └─┬─┘           └─┬──┘
  ┌─┴──┐           ┌┴┐             ┌┴┐
 ┌┴┐┌┴┐┌┴┐        │X│            │base│
 │a││b││c│         └─┘            └───┘
 └─┘└─┘└─┘
 (simplifica)   (convierte)     (extiende)
```

---

## 9. Anti-patrones

### 9.1 Facade que expone internos

```c
// MAL: el header "facade" expone todo
// audio.h
typedef struct {
    Mixer    *mixer;     // ¡usuario puede tocar mixer directamente!
    HwOutput *hw;
    Logger   *log;
} AudioSystem;

Mixer *audio_get_mixer(AudioSystem *sys);  // ¡bypass del facade!
```

Si el usuario accede a `sys->mixer` directamente, el facade pierde su propósito.
Cualquier cambio interno rompe el código del usuario.

### 9.2 God Facade

```c
// MAL: facade con 50 funciones
void audio_play(...);
void audio_pause(...);
void audio_seek(...);
void audio_set_eq_band(...);
void audio_set_reverb_type(...);
void audio_set_compressor_threshold(...);
void audio_enable_3d_spatialization(...);
// ... 43 funciones más
```

Un facade con demasiadas funciones no simplifica: solo renombra. Divide en
facades por responsabilidad o expón módulos para usuarios avanzados:

```c
// audio.h — facade básico (80% de usuarios)
AudioSystem *audio_init(...);
void audio_play(...);
void audio_stop(...);
void audio_shutdown(...);

// audio_advanced.h — para usuarios que necesitan más control
AudioEqualizer *audio_get_equalizer(AudioSystem *sys);
AudioReverb    *audio_get_reverb(AudioSystem *sys);
```

### 9.3 Facade sin tipo opaco

```c
// MAL: struct definida en el header
typedef struct {
    int internal_state;
    void *private_data;
} AudioSystem;
// El usuario puede hacer: sys.internal_state = 42; (corrupción)
```

Sin tipo opaco, el "facade" es solo una sugerencia. El usuario puede
saltárselo accediendo a los campos directamente.

---

## 10. Ejemplos reales en C

### 10.1 SQLite

SQLite es uno de los mejores ejemplos de Facade en C:

```c
// API pública: ~10 funciones principales
sqlite3 *db;
sqlite3_open("data.db", &db);           // oculta: file I/O, page cache, WAL
sqlite3_exec(db, "CREATE TABLE ...");    // oculta: parser, planner, B-tree, pager
sqlite3_close(db);                       // oculta: flush, journal, cleanup

// Internamente: 200,000+ líneas de código
// Parser SQL, optimizador de consultas, B-tree, pager,
// WAL, journaling, VFS, memory allocator...
// El usuario no sabe nada de esto
```

### 10.2 libcurl

```c
// Facade "easy" (para el 90% de casos)
CURL *curl = curl_easy_init();
curl_easy_setopt(curl, CURLOPT_URL, "https://example.com");
curl_easy_perform(curl);
curl_easy_cleanup(curl);

// Internamente maneja: DNS, TCP, TLS, HTTP/1.1, HTTP/2,
// cookies, redirects, compression, proxy, auth...

// Para el 10% que necesita más: multi interface
CURLM *multi = curl_multi_init();  // facade de nivel más bajo
```

### 10.3 POSIX stdio

```c
// Facade sobre file descriptors del kernel
FILE *f = fopen("data.txt", "r");   // oculta: open(), buffer allocation
fgets(buf, sizeof(buf), f);          // oculta: read(), buffering, newline handling
fclose(f);                           // oculta: flush, close(), buffer free

// Internamente: buffering, encoding, line discipline, locking (thread-safe)
// El fd raw está disponible: fileno(f) — "escape hatch" para usuarios avanzados
```

En los tres casos: el facade expone una API minimal, usa un tipo opaco
(`sqlite3`, `CURL`, `FILE`), y oculta decenas de módulos internos.

---

## 11. Facade con configuración

Un facade bien diseñado permite algo de configuración sin exponer internos:

```c
// audio.h
typedef struct {
    int   sample_rate;    // default: 44100
    int   buffer_size;    // default: 4096
    float initial_volume; // default: 1.0
    const char *log_path; // default: NULL (no logging)
} AudioConfig;

// Proporcionar defaults razonables
static inline AudioConfig audio_default_config(void) {
    return (AudioConfig){
        .sample_rate    = 44100,
        .buffer_size    = 4096,
        .initial_volume = 1.0f,
        .log_path       = NULL,
    };
}

AudioSystem *audio_init_with_config(const AudioConfig *config);

// Shortcut: init con defaults
static inline AudioSystem *audio_init_simple(void) {
    AudioConfig cfg = audio_default_config();
    return audio_init_with_config(&cfg);
}
```

```c
// Uso básico:
AudioSystem *sys = audio_init_simple();

// Uso configurado:
AudioConfig cfg = audio_default_config();
cfg.sample_rate = 48000;
cfg.log_path    = "/var/log/audio.log";
AudioSystem *sys = audio_init_with_config(&cfg);
```

El usuario puede ajustar parámetros sin conocer los internos. Los defaults
cubren el caso común. Esto se conecta con el patrón Config Struct de C02.

---

## 12. Errores comunes

| Error | Problema | Solución |
|-------|----------|----------|
| Struct definida en header | Usuario accede a campos internos | Tipo opaco (forward declaration) |
| Funciones sin `static` | Enlazador las exporta, colisiones de nombres | `static` en funciones internas |
| Facade con 30+ funciones | No simplifica, solo renombra | Dividir en facade básico + avanzado |
| No liberar en orden correcto | Use-after-free entre módulos | Shutdown en orden inverso a init |
| Headers internos instalados | Usuario depende de implementación | Solo instalar header público |
| Facade sin escape hatch | Usuarios avanzados no pueden hacer lo que necesitan | Header `_advanced.h` opcional |
| Singleton como facade | Acopla todo al estado global | Pasar `AudioSystem *` explícitamente |
| Cambiar struct opaca sin recompilar | No aplica (es puntero, tamaño no cambia) | Es una ventaja, no un error |
| Facade que reexpone errores internos | Leaking abstractions | Mapear errores internos a códigos propios |
| No documentar qué hace el facade | Usuario no sabe qué se oculta | Documentar comportamiento, no implementación |

---

## 13. Ejercicios

### Ejercicio 1 — Visibilidad de static

**Predicción**: ¿compila este programa?

```c
// mixer.c
static void mixer_process(float *buf, size_t len) {
    for (size_t i = 0; i < len; i++) buf[i] *= 0.5f;
}

// main.c
void mixer_process(float *buf, size_t len);  // declaración manual

int main(void) {
    float buf[4] = {1.0, 2.0, 3.0, 4.0};
    mixer_process(buf, 4);
    return 0;
}
```

<details>
<summary>Respuesta</summary>

**No enlaza** (linker error). Cada archivo `.c` compila por separado:

- `mixer.o`: `mixer_process` es símbolo **local** (`static` → `t` en nm)
- `main.o`: referencia a `mixer_process` como símbolo **externo** (undefined)

El enlazador busca `mixer_process` global y no lo encuentra:

```
$ gcc mixer.c main.c -o prog
undefined reference to `mixer_process'
```

`static` hace que la función solo exista dentro de `mixer.c`. La declaración
en `main.c` promete que existe una versión global, pero no la hay. Es
exactamente el efecto que queremos para ocultar módulos internos.
</details>

---

### Ejercicio 2 — Tipo opaco

**Predicción**: ¿qué error da este código?

```c
// audio.h
typedef struct AudioSystem AudioSystem;
AudioSystem *audio_init(void);

// main.c
#include "audio.h"

int main(void) {
    AudioSystem sys;
    audio_init();
    return 0;
}
```

<details>
<summary>Respuesta</summary>

Error de compilación: `variable has incomplete type 'AudioSystem'`.

```
typedef struct AudioSystem AudioSystem;  // forward declaration
// El compilador sabe que AudioSystem EXISTE como tipo
// Pero NO sabe su tamaño ni sus campos

AudioSystem sys;    // ERROR: necesita saber sizeof(AudioSystem) para reservar stack
AudioSystem *ptr;   // OK: sizeof(ptr) = 8 (siempre, para cualquier puntero)
```

El tipo opaco **fuerza** al usuario a usar punteros. No puede:
- Crear instancias en stack (`AudioSystem sys;`)
- Acceder a campos (`sys.mixer`)
- Copiar por valor (`AudioSystem copy = *ptr;`)

Solo puede recibir un puntero de `audio_init` y pasarlo a las demás funciones.
Esto es precisamente la encapsulación que el facade necesita.
</details>

---

### Ejercicio 3 — Orden de shutdown

**Predicción**: ¿qué pasa si cambiamos el orden de shutdown?

```c
void audio_shutdown(AudioSystem *sys) {
    logger_free(sys->log);    // logger primero
    hw_close(sys->hw);         // hw segundo
    mixer_free(sys->mixer);
    free(sys);
}
```

Y si `hw_close` intenta escribir un log:

```c
static void hw_close(HwOutput *hw) {
    logger_write(current_log, "Closing device");  // ¿funciona?
    // ...
}
```

<details>
<summary>Respuesta</summary>

**Use-after-free**. Si `logger_free` se ejecuta primero, el logger ya fue
liberado cuando `hw_close` intenta escribir en él.

```
Orden correcto (inverso a init):     Orden incorrecto:
1. audio_init:                       1. logger_free ← libera log
   log = logger_create               2. hw_close
   mixer = mixer_create                   → logger_write(log, ...) ← ¡dangling!
   hw = hw_open                      3. mixer_free
2. audio_shutdown:
   hw_close       ← puede logear
   mixer_free     ← puede logear
   logger_free    ← se libera último
```

**Regla**: destruir en orden **inverso** a la creación. El recurso que se
crea primero (logging) se destruye último, porque otros pueden depender de él.
Es la misma lógica que `atexit` (LIFO) y los destructores en C++ (orden
inverso a construcción).
</details>

---

### Ejercicio 4 — God Facade

**Predicción**: ¿cuál es el problema con este header?

```c
// image_processor.h — "Facade"
Image *img_load(const char *path);
void   img_save(const Image *img, const char *path);
void   img_resize(Image *img, int w, int h);
void   img_crop(Image *img, int x, int y, int w, int h);
void   img_rotate(Image *img, float degrees);
void   img_blur(Image *img, float radius);
void   img_sharpen(Image *img, float amount);
void   img_brightness(Image *img, float amount);
void   img_contrast(Image *img, float amount);
void   img_saturation(Image *img, float amount);
void   img_hue_shift(Image *img, float degrees);
void   img_sepia(Image *img);
void   img_grayscale(Image *img);
void   img_invert(Image *img);
void   img_histogram(const Image *img, int *out_r, int *out_g, int *out_b);
void   img_threshold(Image *img, uint8_t level);
void   img_edge_detect(Image *img, int method);
void   img_denoise(Image *img, float strength);
void   img_free(Image *img);
```

<details>
<summary>Respuesta</summary>

Tiene 19 funciones — no es un facade, es un **módulo completo expuesto**.
Un facade debería simplificar, no enumerar toda la funcionalidad.

Mejor diseño con dos niveles:

```c
// image.h — Facade simple (caso común)
Image *img_load(const char *path);
void   img_save(const Image *img, const char *path);
void   img_resize(Image *img, int w, int h);
void   img_free(Image *img);

// image_filters.h — Para quien necesite filtros específicos
void   img_blur(Image *img, float radius);
void   img_sharpen(Image *img, float amount);
// ...

// image_analysis.h — Para quien necesite análisis
void   img_histogram(const Image *img, int *out_r, int *out_g, int *out_b);
void   img_edge_detect(Image *img, int method);
// ...
```

El 80% de usuarios solo necesita load/save/resize. El facade les da eso.
El 20% que necesita filtros incluye el header adicional.
</details>

---

### Ejercicio 5 — Facade vs Adapter

**Predicción**: ¿cuál patrón es cada caso?

1. Función que convierte `errno` de POSIX a tus propios códigos de error
2. Módulo `database.h` que internamente coordina connection pool + query parser + cache
3. Wrapper que hace que la API de libpng se parezca a la de libjpeg

<details>
<summary>Respuesta</summary>

1. **Adapter**: convierte una interfaz (códigos errno) a otra (tus códigos).
   Un solo sistema envuelto, cambio de interfaz.

2. **Facade**: múltiples subsistemas (pool + parser + cache) coordinados detrás
   de una interfaz simple. Simplificación, no conversión.

3. **Adapter**: hace que libpng "se parezca" a libjpeg. Conversión de interfaz
   A → interfaz B.

Regla:
- **Adapter**: 1 sistema, interfaz A → interfaz B
- **Facade**: N sistemas, complejidad → simplicidad
</details>

---

### Ejercicio 6 — Escape hatch

**Predicción**: ¿por qué `fileno(FILE *f)` existe en POSIX? ¿Rompe el facade de stdio?

<details>
<summary>Respuesta</summary>

`fileno` es un **escape hatch**: permite acceder al file descriptor subyacente
cuando el facade de stdio no es suficiente.

Casos donde se necesita:
- `select()`/`poll()` requieren fd, no `FILE *`
- `mmap()` requiere fd
- `fcntl()` para flags avanzados (non-blocking, etc.)
- `dup2()` para redirección

¿Rompe el facade? **Parcialmente**. El usuario que usa `fileno` está conscientemente
saltándose la abstracción de stdio. Pero es mejor que forzar a todos a usar fd
crudos.

Un buen facade ofrece escape hatches **explícitos** para usuarios avanzados,
en lugar de pretender que nadie necesitará acceso al nivel inferior. libcurl
hace lo mismo: `curl_easy_getinfo(curl, CURLINFO_ACTIVESOCKET, &sockfd)`.

La clave es que el escape hatch es **opt-in**: quien no lo necesita nunca lo
ve. No contamina la API principal.
</details>

---

### Ejercicio 7 — Config struct + Facade

**Predicción**: ¿qué imprime este código?

```c
AudioConfig cfg = audio_default_config();
printf("rate=%d buf=%d vol=%.1f\n", cfg.sample_rate, cfg.buffer_size,
       cfg.initial_volume);

cfg.sample_rate = 48000;
AudioSystem *sys = audio_init_with_config(&cfg);
```

<details>
<summary>Respuesta</summary>

```
rate=44100 buf=4096 vol=1.0
```

`audio_default_config()` retorna la struct con los valores por defecto
definidos en la función inline. El usuario luego modifica solo lo que necesita
(`sample_rate = 48000`) y pasa la config al facade.

Este patrón combina **Facade** (API simple) con **Config Struct** (de B16 C02):
- El facade oculta los subsistemas
- La config struct da flexibilidad sin complejidad
- Los defaults cubren el 80% de usos

El usuario no necesita saber que `sample_rate` va al decoder, `buffer_size`
al mixer, y `log_path` al logger. El facade distribuye cada campo al módulo
correcto internamente.
</details>

---

### Ejercicio 8 — Colisión de nombres sin static

**Predicción**: ¿qué pasa al enlazar estos dos archivos?

```c
// audio.c
void process(float *buf, size_t len) { /* ... */ }

// video.c
void process(uint8_t *frame, size_t w, size_t h) { /* ... */ }
```

<details>
<summary>Respuesta</summary>

**Error de enlace**: `multiple definition of 'process'`.

Ambas funciones `process` son globales (sin `static`). El enlazador ve dos
símbolos con el mismo nombre y no sabe cuál usar.

```
$ nm audio.o
0000000000001000 T process   ← global

$ nm video.o
0000000000001000 T process   ← global (¡colisión!)
```

Soluciones:
1. **`static`**: `static void process(...)` en ambos → invisibles fuera
2. **Prefijo**: `audio_process(...)`, `video_process(...)` → namespace manual
3. **Ambas**: `static` para internos, prefijo para públicos

En C no hay namespaces. El prefijo (`audio_`, `video_`) es el namespace manual.
Las funciones internas deben ser `static` para evitar colisiones.
</details>

---

### Ejercicio 9 — Niveles de facade

**Predicción**: en la arquitectura de niveles (game_engine → audio + graphics →
módulos internos), ¿cuántos tipos opacos necesita el usuario del game engine?

```c
// game_engine.h
typedef struct GameEngine GameEngine;
GameEngine *game_init(const char *title, int w, int h);
void game_update(GameEngine *engine);
void game_shutdown(GameEngine *engine);
```

<details>
<summary>Respuesta</summary>

**Solo uno**: `GameEngine *`.

El usuario del game engine no ve `AudioSystem`, `GraphicsSystem`, `Mixer`,
`HwOutput`, ni ningún otro tipo interno. Todos están ocultos detrás del
facade de nivel 2.

```
Usuario ve:          1 tipo: GameEngine *
game_engine.c ve:    2 tipos: AudioSystem *, GraphicsSystem *
audio.c ve:          4 tipos: Mixer *, HwOutput *, Logger *, DecodedAudio *
```

Cada nivel solo conoce el nivel inmediatamente inferior. El usuario del
nivel N no sabe cuántas capas hay debajo. Si reemplazas el motor de audio
completo, `game_engine.h` no cambia.
</details>

---

### Ejercicio 10 — Diseñar un facade

**Predicción**: tienes estos módulos internos para un sistema de email. Diseña
el header público del facade (máximo 6 funciones).

Módulos internos:
- `smtp_connect`, `smtp_auth`, `smtp_send`, `smtp_disconnect`
- `mime_create`, `mime_add_text`, `mime_add_attachment`, `mime_encode`
- `dns_resolve_mx`
- `tls_handshake`, `tls_wrap_socket`
- `base64_encode`

<details>
<summary>Respuesta</summary>

```c
// email.h — Facade
typedef struct EmailClient EmailClient;

EmailClient *email_connect(const char *server, const char *user,
                           const char *password);

bool email_send(EmailClient *client,
                const char *to,
                const char *subject,
                const char *body);

bool email_send_with_attachment(EmailClient *client,
                                const char *to,
                                const char *subject,
                                const char *body,
                                const char *attachment_path);

int email_error_code(const EmailClient *client);

const char *email_error_message(const EmailClient *client);

void email_disconnect(EmailClient *client);
```

6 funciones. Internamente, `email_connect` hace:
1. `dns_resolve_mx` → encuentra el servidor MX
2. `smtp_connect` → conexión TCP
3. `tls_handshake` + `tls_wrap_socket` → TLS
4. `smtp_auth` + `base64_encode` → autenticación

Y `email_send_with_attachment` hace:
1. `mime_create` + `mime_add_text` + `mime_add_attachment` + `mime_encode`
2. `smtp_send`

El usuario no sabe nada de SMTP, MIME, DNS, TLS, ni base64.
</details>