# Facade en Rust

## 1. De static linkage a módulos con visibilidad

En C (T01) el Facade se construye con `static` en funciones y tipos opacos.
Rust tiene un sistema de visibilidad mucho más granular integrado en el lenguaje:

```
C:                              Rust:
static void internal(...)       fn internal(...)        // privado por defecto
void public_api(...)            pub fn public_api(...)  // explícitamente público

// Tipo opaco:                  // Campos privados por defecto:
typedef struct X X;             pub struct X {
// (definición oculta en .c)        field: i32,         // privado
                                    pub visible: i32,   // público
                                }
```

| C | Rust | Efecto |
|---|------|--------|
| `static` en función | `fn` (sin `pub`) | Solo visible en el módulo |
| Sin `static` | `pub fn` | Visible donde se importe |
| Forward declaration | Campos sin `pub` | Estructura opaca parcial |
| Header interno no instalado | `pub(crate)` | Visible en el crate, no fuera |
| Header público | `pub` + re-export | API pública del crate |

**Ventaja de Rust**: la visibilidad se verifica en **compilación**. En C, nada
impide que alguien declare `extern void mixer_process(...)` y la llame si no
es `static`. En Rust, `fn` sin `pub` es inaccesible — punto.

---

## 2. Estructura de módulos como facade

### 2.1 Layout del proyecto

```
audio_system/
├── Cargo.toml
└── src/
    ├── lib.rs              ← re-exports (la fachada)
    ├── decoder.rs          ← módulo interno
    ├── mixer.rs            ← módulo interno
    ├── hw_output.rs        ← módulo interno
    └── logger.rs           ← módulo interno
```

### 2.2 Módulos internos (sin pub)

```rust
// src/decoder.rs
#[derive(Debug)]
pub(crate) enum AudioFormat {
    Wav,
    Mp3,
    Ogg,
}

pub(crate) struct DecodedAudio {
    pub(crate) format: AudioFormat,
    pub(crate) sample_rate: u32,
    pub(crate) channels: u16,
    pub(crate) samples: Vec<f32>,
}

pub(crate) fn decode_file(path: &str) -> Option<DecodedAudio> {
    let format = match path.rsplit('.').next()? {
        "wav" => AudioFormat::Wav,
        "mp3" => AudioFormat::Mp3,
        "ogg" => AudioFormat::Ogg,
        _ => return None,
    };

    Some(DecodedAudio {
        format,
        sample_rate: 44100,
        channels: 2,
        samples: vec![0.0; 44100 * 2],  // 1 segundo estéreo simulado
    })
}
```

```rust
// src/mixer.rs
pub(crate) struct Mixer {
    volume: f32,
    output_buf: Vec<f32>,
}

impl Mixer {
    pub(crate) fn new(buf_size: usize) -> Self {
        Mixer {
            volume: 1.0,
            output_buf: vec![0.0; buf_size],
        }
    }

    pub(crate) fn set_volume(&mut self, vol: f32) {
        self.volume = vol.clamp(0.0, 1.0);
    }

    pub(crate) fn process(&mut self, input: &[f32]) {
        let len = input.len().min(self.output_buf.len());
        for i in 0..len {
            self.output_buf[i] = input[i] * self.volume;
        }
    }

    pub(crate) fn output(&self) -> &[f32] {
        &self.output_buf
    }
}
```

```rust
// src/hw_output.rs
pub(crate) struct HwOutput {
    active: bool,
}

impl HwOutput {
    pub(crate) fn open() -> Self {
        println!("[hw] Audio device opened");
        HwOutput { active: true }
    }

    pub(crate) fn write(&self, _buf: &[f32]) -> bool {
        self.active
    }
}

impl Drop for HwOutput {
    fn drop(&mut self) {
        if self.active {
            println!("[hw] Audio device closed");
            self.active = false;
        }
    }
}
```

```rust
// src/logger.rs
use std::fs::OpenOptions;
use std::io::Write;

pub(crate) struct Logger {
    file: Option<std::fs::File>,
}

impl Logger {
    pub(crate) fn new(path: Option<&str>) -> Self {
        let file = path.and_then(|p| {
            OpenOptions::new().create(true).append(true).open(p).ok()
        });
        Logger { file }
    }

    pub(crate) fn log(&mut self, msg: &str) {
        if let Some(f) = &mut self.file {
            let _ = writeln!(f, "[audio] {msg}");
        }
    }
}
```

**Observación clave**: todo es `pub(crate)`. Visible dentro del crate para
que el facade los use, invisible fuera del crate.

### 2.3 El facade: lib.rs con re-exports selectivos

```rust
// src/lib.rs
mod decoder;
mod mixer;
mod hw_output;
mod logger;

// La struct pública — campos privados (opaco por defecto)
pub struct AudioSystem {
    mixer: mixer::Mixer,
    hw: hw_output::HwOutput,
    log: logger::Logger,
    playing: bool,
}

// Configuración pública
pub struct AudioConfig {
    pub sample_rate: u32,
    pub buffer_size: usize,
    pub initial_volume: f32,
    pub log_path: Option<String>,
}

impl Default for AudioConfig {
    fn default() -> Self {
        AudioConfig {
            sample_rate: 44100,
            buffer_size: 4096,
            initial_volume: 1.0,
            log_path: None,
        }
    }
}

// API pública: solo estas funciones
impl AudioSystem {
    pub fn new(config: AudioConfig) -> Self {
        let mut log = logger::Logger::new(config.log_path.as_deref());
        let mut m = mixer::Mixer::new(config.buffer_size);
        m.set_volume(config.initial_volume);
        let hw = hw_output::HwOutput::open();

        log.log("Audio system initialized");

        AudioSystem {
            mixer: m,
            hw,
            log,
            playing: false,
        }
    }

    pub fn with_defaults() -> Self {
        Self::new(AudioConfig::default())
    }

    pub fn play(&mut self, filename: &str) -> bool {
        self.log.log("Playing file");

        let decoded = match decoder::decode_file(filename) {
            Some(d) => d,
            None => {
                self.log.log("Failed to decode file");
                return false;
            }
        };

        self.mixer.process(&decoded.samples);
        let ok = self.hw.write(self.mixer.output());
        self.playing = ok;

        self.log.log(if ok { "Playback started" } else { "Playback failed" });
        ok
    }

    pub fn set_volume(&mut self, volume: f32) {
        self.mixer.set_volume(volume);
        self.log.log("Volume changed");
    }

    pub fn stop(&mut self) {
        self.playing = false;
        self.log.log("Playback stopped");
    }
}
// Drop automático: hw_output::Drop cierra el dispositivo
// No necesitamos audio_shutdown explícito
```

### 2.4 Uso por el cliente

```rust
use audio_system::{AudioSystem, AudioConfig};

fn main() {
    // Uso simple
    let mut audio = AudioSystem::with_defaults();
    audio.set_volume(0.75);
    audio.play("music.wav");
    audio.stop();

    // Uso configurado
    let config = AudioConfig {
        sample_rate: 48000,
        log_path: Some("/tmp/audio.log".into()),
        ..AudioConfig::default()
    };
    let mut audio2 = AudioSystem::new(config);
    audio2.play("effect.mp3");
}
// Drop automático al salir del scope
```

El usuario ve: `AudioSystem`, `AudioConfig`, y 4 métodos.
No ve: `Mixer`, `HwOutput`, `Logger`, `DecodedAudio`, `AudioFormat`.

---

## 3. pub, pub(crate), pub(super): el espectro de visibilidad

```rust
pub fn visible_everywhere() {}       // cualquiera que importe el crate
pub(crate) fn visible_in_crate() {}  // solo dentro de este crate
pub(super) fn visible_in_parent() {} // solo el módulo padre
fn visible_here() {}                 // solo este módulo
```

```
Visibilidad en un crate:

crate (lib.rs)
├── mod facade        ← pub fn API()
│   ├── mod decoder   ← pub(crate) fn decode()
│   │                    pub(super) fn helper()   ← solo facade lo ve
│   │                    fn internal()            ← solo decoder lo ve
│   ├── mod mixer     ← pub(crate) fn mix()
│   └── mod hw        ← pub(crate) fn write()
```

| Desde | `pub` | `pub(crate)` | `pub(super)` | `fn` (privado) |
|-------|-------|-------------|-------------|---------------|
| Mismo módulo | ✓ | ✓ | ✓ | ✓ |
| Módulo padre | ✓ | ✓ | ✓ | ✗ |
| Otro módulo del crate | ✓ | ✓ | ✗ | ✗ |
| Otro crate | ✓ | ✗ | ✗ | ✗ |

### Cuándo usar cada uno

- **`pub`**: API pública del facade (lo que el usuario ve)
- **`pub(crate)`**: módulos internos que el facade necesita orquestar
- **`pub(super)`**: helpers que solo el módulo padre directo necesita
- **privado** (sin pub): detalles internos de un solo módulo

---

## 4. Re-exports selectivos con pub use

`pub use` permite "levantar" ítems de módulos internos a la raíz del crate
sin exponer la estructura de módulos:

```rust
// src/lib.rs

mod config;
mod error;
mod client;

// El usuario importa: use my_crate::Client;
// No necesita saber que está en mod client
pub use client::Client;
pub use config::Config;
pub use error::Error;

// Estos módulos NO se exportan:
// mod decoder;    ← no pub, no pub use → invisible
// mod protocol;   ← no pub, no pub use → invisible
```

```
Sin re-export:                      Con re-export:
use my_crate::client::Client;      use my_crate::Client;
use my_crate::config::Config;      use my_crate::Config;
use my_crate::error::Error;        use my_crate::Error;

// El usuario ve la estructura      // Interfaz plana, limpia
// interna del crate                // La estructura interna es irrelevante
```

### Re-export de tipos de terceros

```rust
// src/lib.rs
mod internal;

// Re-exportar un tipo de una dependencia para que el usuario
// no necesite añadirla a su Cargo.toml
pub use serde_json::Value as JsonValue;

// Ahora el usuario usa: my_crate::JsonValue
// Sin añadir serde_json a sus dependencias
```

---

## 5. Módulos como facade: pub mod vs mod

```rust
// src/lib.rs

pub mod public_api;      // el usuario puede hacer: use crate::public_api::*;
mod internal;            // invisible fuera del crate

// src/public_api.rs — todo aquí es accesible
pub fn easy_function() { ... }

// src/internal.rs — nada aquí es accesible desde fuera
pub(crate) fn helper() { ... }  // solo el crate lo ve
```

**Regla**: `pub mod X` hace que el módulo sea navegable por el usuario.
`mod X` lo oculta completamente. Un facade típico tiene:
- `lib.rs` con `pub use` de los pocos tipos públicos
- `mod` (sin pub) para todos los módulos internos

---

## 6. Prelude pattern: facade conveniente

Muchos crates Rust usan un módulo `prelude` como facade de "todo lo que
normalmente necesitas":

```rust
// src/lib.rs
pub mod prelude {
    pub use crate::AudioSystem;
    pub use crate::AudioConfig;
    pub use crate::AudioError;
}

// También exportar individualmente para uso selectivo
pub mod advanced {
    pub use crate::decoder::AudioFormat;
    pub use crate::mixer::MixerConfig;
}
```

```rust
// Uso simple (facade):
use audio_system::prelude::*;

// Uso avanzado (escape hatch):
use audio_system::advanced::AudioFormat;
```

Ejemplos reales del prelude pattern:
- `std::prelude` — traits y tipos fundamentales
- `tokio::prelude` — Future, Stream, Sink
- `diesel::prelude` — todo lo que necesitas para queries

---

## 7. Comparación completa con C

### 7.1 El mismo facade en ambos lenguajes

```
C:                                    Rust:
// audio.h (5 funciones)             // lib.rs (4 métodos + new)
AudioSystem *audio_init(config);     AudioSystem::new(config)
audio_play(sys, filename);           sys.play(filename)
audio_set_volume(sys, vol);          sys.set_volume(vol)
audio_stop(sys);                     sys.stop()
audio_shutdown(sys);                 // Drop automático

// audio.c                           // lib.rs
static void mixer_process(...)       mod mixer { pub(crate) fn process(...) }
static DecodedAudio *decode(...)     mod decoder { pub(crate) fn decode(...) }
```

### 7.2 Tabla de equivalencias

| Concepto | C | Rust |
|----------|---|------|
| Ocultar función | `static` | `fn` (sin pub) |
| Ocultar struct | Forward declaration | Campos sin `pub` |
| Visible en proyecto | No `static` + header interno | `pub(crate)` |
| Visible al usuario | No `static` + header público | `pub` + `pub use` |
| Destrucción coordinada | `shutdown()` manual en orden | `Drop` automático |
| Verificación | Enlazador (tarde, mensajes crípticos) | Compilador (temprano, mensajes claros) |
| Namespace | Prefijo manual (`audio_`) | Módulos (`audio::`) |
| Escape hatch | Header `_advanced.h` | `pub mod advanced` |
| Config con defaults | `default_config()` inline | `impl Default` + `..Default::default()` |

### 7.3 Lo que Rust automatiza

1. **Destrucción ordenada**: Drop se ejecuta en orden inverso a la creación
   de campos (exactamente lo que haríamos manualmente en C)
2. **Encapsulación verificada**: `pub(crate)` es imposible de saltarse (en C,
   se puede declarar `extern` para cualquier símbolo no-static)
3. **Namespace**: `mod` evita colisiones sin prefijos manuales
4. **Re-exports**: `pub use` da una interfaz plana sin exponer módulos

---

## 8. Facade con traits: interfaz abstracta

A veces el facade expone un trait para permitir testing o implementaciones
alternativas:

```rust
// src/lib.rs
pub trait AudioPlayer {
    fn play(&mut self, filename: &str) -> bool;
    fn set_volume(&mut self, volume: f32);
    fn stop(&mut self);
}

// Implementación real (facade)
pub struct AudioSystem { /* ... */ }

impl AudioPlayer for AudioSystem {
    fn play(&mut self, filename: &str) -> bool {
        // coordina decoder + mixer + hw_output
        todo!()
    }
    fn set_volume(&mut self, volume: f32) { todo!() }
    fn stop(&mut self) { todo!() }
}
```

```rust
// En tests del usuario:
struct MockAudio {
    played: Vec<String>,
    volume: f32,
}

impl AudioPlayer for MockAudio {
    fn play(&mut self, filename: &str) -> bool {
        self.played.push(filename.to_string());
        true
    }
    fn set_volume(&mut self, volume: f32) {
        self.volume = volume;
    }
    fn stop(&mut self) {}
}

fn game_logic(audio: &mut dyn AudioPlayer) {
    audio.play("explosion.wav");
    audio.set_volume(0.5);
}

#[test]
fn test_game_logic() {
    let mut mock = MockAudio { played: vec![], volume: 1.0 };
    game_logic(&mut mock);
    assert_eq!(mock.played, vec!["explosion.wav"]);
    assert_eq!(mock.volume, 0.5);
}
```

En C esto requeriría vtables manuales (como en el patrón Adapter T01).
En Rust, `dyn AudioPlayer` da la misma funcionalidad automáticamente.

---

## 9. Facade con errores propios

Un buen facade mapea errores internos a su propia jerarquía, sin filtrar
tipos de error de las dependencias:

```rust
// src/error.rs
#[derive(Debug)]
pub enum AudioError {
    UnsupportedFormat(String),
    DeviceUnavailable,
    DecodeFailed(String),
    IoError(String),
}

impl std::fmt::Display for AudioError {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            AudioError::UnsupportedFormat(ext) => {
                write!(f, "unsupported audio format: {ext}")
            }
            AudioError::DeviceUnavailable => write!(f, "audio device unavailable"),
            AudioError::DecodeFailed(msg) => write!(f, "decode failed: {msg}"),
            AudioError::IoError(msg) => write!(f, "I/O error: {msg}"),
        }
    }
}

impl std::error::Error for AudioError {}

// Conversiones internas (pub(crate), no expuestas al usuario)
impl From<std::io::Error> for AudioError {
    fn from(e: std::io::Error) -> Self {
        AudioError::IoError(e.to_string())
    }
}
```

```rust
// src/lib.rs
pub use error::AudioError;

impl AudioSystem {
    pub fn play(&mut self, filename: &str) -> Result<(), AudioError> {
        self.log.log("Playing file");

        let decoded = decoder::decode_file(filename)
            .ok_or_else(|| AudioError::UnsupportedFormat(filename.into()))?;

        self.mixer.process(&decoded.samples);

        if !self.hw.write(self.mixer.output()) {
            return Err(AudioError::DeviceUnavailable);
        }

        self.playing = true;
        Ok(())
    }
}
```

El usuario ve `AudioError`, nunca `std::io::Error` ni errores internos del
decoder. Si cambias la librería de decodificación, los errores del usuario
no cambian.

---

## 10. Ejemplo real: crate reqwest como facade

`reqwest` es un facade sobre `hyper` + `tokio` + `rustls` + `h2` + ...:

```rust
// Lo que el usuario ve (facade):
let body = reqwest::get("https://example.com")
    .await?
    .text()
    .await?;

// Lo que el usuario NO ve (internamente):
// - DNS resolution (hickory-dns o sistema)
// - TCP connection (tokio::net::TcpStream)
// - TLS handshake (rustls o native-tls)
// - HTTP/1.1 o HTTP/2 negotiation (hyper + h2)
// - Decompression (flate2, brotli)
// - Cookie handling
// - Redirect following
// - Connection pooling
```

```
reqwest (facade público)
├── pub struct Client
├── pub struct Request / Response
├── pub enum Error
│
└── internamente:
    ├── hyper (HTTP engine)
    │   └── tokio (async runtime)
    ├── rustls (TLS)
    ├── h2 (HTTP/2)
    ├── url (URL parsing)
    ├── mime (content types)
    └── serde (JSON de/serialization)
```

El Cargo.toml de reqwest tiene ~20 dependencias. El usuario importa 1 crate.

---

## 11. #[doc(hidden)] y semver

Último mecanismo de ocultación: `#[doc(hidden)]` hace items públicos pero
invisibles en la documentación.

```rust
// Público por necesidad técnica (macros, interop) pero no parte de la API estable
#[doc(hidden)]
pub fn __internal_detail() { }

// Aparece en docs como API pública
pub fn stable_api() { }
```

**Cuándo usarlo**:
- Funciones que macros necesitan llamar (deben ser `pub` por cómo funcionan macros)
- Items requeridos por interop con otros crates del mismo ecosistema
- **No** como sustituto de `pub(crate)` — si puedes usar `pub(crate)`, úsalo

**Convención**: el prefijo `__` (doble underscore) más `#[doc(hidden)]` señala
"esto es interno, no lo uses, puede cambiar sin aviso".

---

## 12. Errores comunes

| Error | Problema | Solución |
|-------|----------|----------|
| `pub mod internal` | Expone módulos internos al usuario | `mod internal` (sin pub) |
| `pub` en campos del facade | Usuario accede a internos directamente | Campos sin pub |
| `pub(crate)` donde basta privado | Visibilidad más amplia de lo necesario | Usar la menor visibilidad posible |
| Re-exportar tipos de dependencias | Dependencia se vuelve parte de tu API pública | Newtype o wrapper propio |
| No implementar `Default` para config | Usuario debe especificar todo | `impl Default` con valores razonables |
| Facade sin `Result`/errores propios | Leaking errors de dependencias internas | `enum AudioError` propio |
| `#[doc(hidden)]` en vez de `pub(crate)` | Item accesible aunque oculto en docs | `pub(crate)` es más restrictivo |
| Demasiados `pub use` | Prelude gigante anula el propósito | Re-exportar solo lo esencial |
| Facade con 20 métodos | No simplifica, solo envuelve | Dividir en facade básico + módulo avanzado |
| No usar Drop para cleanup | Usuario debe llamar shutdown() | `impl Drop` para recursos del facade |

---

## 13. Ejercicios

### Ejercicio 1 — Visibilidad básica

**Predicción**: ¿qué líneas del main dan error de compilación?

```rust
// src/lib.rs
mod internal {
    pub(crate) fn helper() -> i32 { 42 }
    fn secret() -> i32 { 7 }
}

pub fn public_api() -> i32 {
    internal::helper()  // línea A
}

// main.rs (otro crate)
fn main() {
    println!("{}", my_crate::public_api());       // línea B
    println!("{}", my_crate::internal::helper());  // línea C
    println!("{}", my_crate::internal::secret());  // línea D
}
```

<details>
<summary>Respuesta</summary>

- **Línea A**: Compila. `helper` es `pub(crate)` y `lib.rs` está en el mismo crate.
- **Línea B**: Compila. `public_api` es `pub`.
- **Línea C**: Error. `mod internal` no tiene `pub`, así que el módulo completo es
  invisible desde fuera del crate. No importa que `helper` sea `pub(crate)`.
- **Línea D**: Error doble. `internal` no es público, y `secret` no tiene `pub`.

```
Visibilidad es jerárquica: para acceder a internal::helper(),
necesitas acceso a `internal` Y a `helper`.

mod internal → no visible fuera del crate → BLOQUEADO
  pub(crate) fn helper → visible en crate, pero irrelevante si
                          el módulo padre no es accesible
```

Incluso si cambiaras `mod internal` a `pub mod internal`, la línea D seguiría
fallando porque `secret` es privada.
</details>

---

### Ejercicio 2 — pub use re-export

**Predicción**: ¿qué puede importar el usuario de este crate?

```rust
// src/lib.rs
mod config;
mod engine;
mod error;

pub use config::Config;
pub use engine::Engine;
pub use error::Error;
```

```rust
// src/config.rs
pub struct Config { pub name: String }
pub(crate) struct InternalConfig { pub(crate) debug: bool }

// src/engine.rs
pub struct Engine { config: Config }
pub(crate) struct EngineState { pub(crate) running: bool }
```

<details>
<summary>Respuesta</summary>

El usuario puede importar:
- `my_crate::Config` ✓ (re-exportado con `pub use`)
- `my_crate::Engine` ✓ (re-exportado con `pub use`)
- `my_crate::Error` ✓ (re-exportado con `pub use`)

El usuario **NO** puede importar:
- `my_crate::config::Config` ✗ (`mod config` sin pub → módulo inaccesible)
- `my_crate::config::InternalConfig` ✗ (módulo inaccesible + `pub(crate)`)
- `my_crate::engine::EngineState` ✗ (módulo inaccesible + `pub(crate)`)

`pub use` "levanta" ítems específicos al nivel raíz sin exponer el módulo.
Es como si `Config`, `Engine`, y `Error` estuvieran definidos directamente
en `lib.rs`.

Además: `Engine.config` es privado (sin `pub`), así que el usuario no puede
acceder al campo `config` de un `Engine`, aunque `Config` como tipo sí es visible.
</details>

---

### Ejercicio 3 — Drop automático vs shutdown manual

**Predicción**: ¿cuándo se imprime "[hw] Audio device closed" en cada caso?

```rust
// Caso A:
fn case_a() {
    let mut audio = AudioSystem::with_defaults();
    audio.play("test.wav");
}  // ← aquí?

// Caso B:
fn case_b() {
    let mut audio = AudioSystem::with_defaults();
    audio.play("test.wav");
    drop(audio);  // ← aquí?
    println!("After drop");
}
```

<details>
<summary>Respuesta</summary>

- **Caso A**: se imprime al final de `case_a`, cuando `audio` sale del scope.
  `Drop` de `AudioSystem` ejecuta `Drop` de cada campo, incluyendo `HwOutput`.

- **Caso B**: se imprime **antes** de "After drop". `drop(audio)` destruye
  el `AudioSystem` inmediatamente, no al final del scope.

```
Caso A:              Caso B:
play("test.wav")     play("test.wav")
} ← Drop aquí       drop(audio) ← Drop aquí
                     "After drop"  ← audio ya no existe
                     }
```

Comparación con C:
```c
// C: si olvidas audio_shutdown, memory leak + device leak
audio_play(sys, "test.wav");
// ... olvidas audio_shutdown(sys); → leak

// Rust: imposible olvidar, Drop es automático
```

Rust destruye los campos en orden inverso a su declaración en la struct.
Como `AudioSystem` tiene: `mixer, hw, log, playing`, se destruyen en orden:
`playing, log, hw, mixer` — equivalente al "shutdown en orden inverso" manual de C.
</details>

---

### Ejercicio 4 — pub(super) alcance

**Predicción**: ¿cuáles llamadas compilan?

```rust
mod parent {
    mod child {
        pub(super) fn for_parent() -> i32 { 1 }
        pub(crate) fn for_crate() -> i32 { 2 }
        fn private() -> i32 { 3 }
    }

    pub fn from_parent() -> i32 {
        child::for_parent()  // llamada A
        + child::for_crate() // llamada B
    }
}

fn from_root() {
    parent::from_parent();          // llamada C
    // parent::child::for_parent(); // llamada D
    // parent::child::for_crate();  // llamada E
}
```

<details>
<summary>Respuesta</summary>

- **A**: Compila. `for_parent` es `pub(super)`, y `parent` es el módulo padre de `child`.
- **B**: Compila. `for_crate` es `pub(crate)`, visible en todo el crate.
- **C**: Compila. `from_parent` es `pub` y `parent` es accesible (está en la raíz).
- **D**: No compila. `child` no es `pub`, así que `from_root` no puede navegar a `child`.
  Aunque `for_parent` es `pub(super)`, el **módulo** no es accesible.
- **E**: No compila. Mismo problema: `child` no es `pub`.

```
Visibilidad de funciones:
  for_parent: child → parent ✓, parent → root ✗ (pub(super) no llega)
  for_crate:  child → parent ✓, parent → root ✓ (pub(crate) llega a todo)
  private:    child → child ✓, child → parent ✗

Pero: para acceder a child::X desde fuera de parent,
el módulo child DEBE ser pub. No lo es → bloqueado.
```
</details>

---

### Ejercicio 5 — Prelude pattern

**Predicción**: ¿cuántos items importa cada línea?

```rust
// src/lib.rs
pub mod prelude {
    pub use crate::AudioSystem;
    pub use crate::AudioConfig;
    pub use crate::AudioError;
}

pub struct AudioSystem;
pub struct AudioConfig;
pub enum AudioError { Decode, Device }
pub struct MixerConfig;        // no en prelude
pub fn advanced_setup() {}     // no en prelude
```

```rust
// usuario:
use audio_crate::prelude::*;   // línea A: ¿cuántos?
use audio_crate::*;             // línea B: ¿cuántos?
```

<details>
<summary>Respuesta</summary>

- **Línea A**: importa **3 items**: `AudioSystem`, `AudioConfig`, `AudioError`.
  Solo lo que está en el módulo `prelude`.

- **Línea B**: importa **5 items**: `AudioSystem`, `AudioConfig`, `AudioError`,
  `MixerConfig`, `advanced_setup`. Todo lo que es `pub` en `lib.rs`, más el
  módulo `prelude` en sí.

El prelude es un **subconjunto curado** de la API pública. `use crate::prelude::*`
es la forma idiomática de importar "lo que normalmente necesitas" sin traer todo.

Convención: el prelude no incluye ítems que la mayoría de usuarios no necesita.
En este caso, `MixerConfig` y `advanced_setup` son para usuarios avanzados y
no forman parte del prelude.
</details>

---

### Ejercicio 6 — Error mapping

**Predicción**: ¿por qué es mala práctica esto?

```rust
pub fn play(&mut self, filename: &str) -> Result<(), std::io::Error> {
    let file = std::fs::File::open(filename)?;
    // ...
    Ok(())
}
```

<details>
<summary>Respuesta</summary>

Expone `std::io::Error` como parte de la API pública del facade. Problemas:

1. **Leaking abstraction**: el usuario ahora sabe que internamente se usa
   `std::fs::File`. Si cambias a leer desde red o desde memoria, el tipo de
   error cambia.

2. **Semver**: cambiar de `std::io::Error` a otro error es un **breaking change**
   porque los usuarios ya hacen `match` sobre los `ErrorKind`.

3. **Información inútil**: `io::Error` puede decir "file not found", pero el
   usuario del facade de audio quiere saber "formato no soportado" o "device
   unavailable", no detalles de I/O.

Solución: error propio del facade:
```rust
pub fn play(&mut self, filename: &str) -> Result<(), AudioError> {
    let file = std::fs::File::open(filename)
        .map_err(|e| AudioError::IoError(e.to_string()))?;
    // ...
    Ok(())
}
```

El facade traduce errores internos a su propia jerarquía, aislando al
usuario de los detalles de implementación.
</details>

---

### Ejercicio 7 — Campos privados como tipo opaco

**Predicción**: ¿qué puede y qué no puede hacer el usuario?

```rust
pub struct AudioSystem {
    mixer: Mixer,        // privado
    pub playing: bool,   // público
}
```

```rust
// ¿Compilan?
let sys = AudioSystem::with_defaults();
println!("{}", sys.playing);           // A
sys.mixer.set_volume(0.5);            // B
let copy = AudioSystem {              // C
    mixer: sys.mixer,
    playing: false,
};
```

<details>
<summary>Respuesta</summary>

- **A**: Compila. `playing` es `pub`, lectura permitida.
- **B**: No compila. `mixer` es privado, acceso denegado.
- **C**: No compila. Para construir una struct con sintaxis literal, necesitas
  acceso a **todos** los campos. `mixer` es privado → no puedes construir
  `AudioSystem` directamente.

```
Campos privados implican:
1. No leer: sys.mixer → error
2. No escribir: sys.mixer = ... → error
3. No construir literal: AudioSystem { ... } → error
4. No destructurar: let AudioSystem { mixer, .. } = sys; → error

El usuario DEBE usar el constructor pub (::new o ::with_defaults).
```

Esto es el equivalente Rust del tipo opaco de C, pero más granular: puedes
hacer algunos campos públicos (`playing`) y otros privados (`mixer`). En C
es todo-o-nada (toda la struct opaca o toda visible).
</details>

---

### Ejercicio 8 — Facade multinivel

**Predicción**: en esta estructura, ¿cuántos `pub use` necesita el facade
de nivel 2 para que el usuario solo vea `GameEngine`?

```rust
// game_engine crate (nivel 2)
// Depende de: audio_crate, graphics_crate

// src/lib.rs
use audio_crate::AudioSystem;
use graphics_crate::GraphicsSystem;

pub struct GameEngine {
    audio: AudioSystem,
    gfx: GraphicsSystem,
}
```

<details>
<summary>Respuesta</summary>

**Cero** `pub use` adicionales.

`AudioSystem` y `GraphicsSystem` ya están ocultos porque son campos privados
de `GameEngine`. El usuario del game engine:

- Ve: `GameEngine` (el struct)
- No ve: `AudioSystem`, `GraphicsSystem` (campos privados)
- No necesita `audio_crate` ni `graphics_crate` en su `Cargo.toml`

```rust
// El usuario solo escribe:
use game_engine::GameEngine;

let engine = GameEngine::new("My Game", 800, 600);
engine.update();
// No sabe que existe AudioSystem ni GraphicsSystem
```

Los `pub use` solo son necesarios cuando quieres **exponer** tipos de otros
módulos o crates. En un facade, el propósito es exactamente lo contrario:
ocultarlos.
</details>

---

### Ejercicio 9 — doc(hidden) vs pub(crate)

**Predicción**: ¿cuál es la diferencia práctica entre estas dos funciones?

```rust
#[doc(hidden)]
pub fn __internal_a() -> i32 { 1 }

pub(crate) fn internal_b() -> i32 { 2 }
```

<details>
<summary>Respuesta</summary>

| | `#[doc(hidden)] pub` | `pub(crate)` |
|---|---------------------|-------------|
| Visible en docs | No | No (ni aparece fuera) |
| Llamable desde otro crate | **Sí** | **No** |
| Semver | Ambiguo (¿es API pública?) | Claramente no es API pública |
| Uso legítimo | Macros que necesitan llamarla | Todo lo demás |

`#[doc(hidden)] pub fn` es **accesible** desde fuera aunque no aparezca en
la documentación. Otro crate puede hacer:
```rust
my_crate::__internal_a();  // ¡funciona! (aunque no sale en docs)
```

`pub(crate) fn` es **inaccesible** desde fuera. Punto. El compilador lo impide.

Regla: **siempre preferir `pub(crate)`** sobre `#[doc(hidden)] pub`. Usa
`#[doc(hidden)]` solo cuando el item **debe** ser `pub` por razones técnicas
(típicamente macros procedurales que necesitan llamar funciones del crate).
</details>

---

### Ejercicio 10 — Diseñar un facade en Rust

**Predicción**: tienes estos módulos internos. Escribe la estructura de `lib.rs`
con re-exports y visibilidades correctas.

Módulos:
- `dns` — resolución de nombres
- `tls` — handshake y cifrado
- `smtp` — protocolo SMTP
- `mime` — construcción de mensajes MIME

Tipos públicos deseados: `EmailClient`, `EmailConfig`, `EmailError`

<details>
<summary>Respuesta</summary>

```rust
// src/lib.rs

// Módulos internos — sin pub, invisibles fuera del crate
mod dns;
mod tls;
mod smtp;
mod mime;
mod config;
mod error;

// Re-exports selectivos — la fachada
pub use config::EmailConfig;
pub use error::EmailError;

// El facade principal
pub struct EmailClient {
    smtp: smtp::SmtpConnection,  // privado
    config: EmailConfig,          // privado (aunque el tipo es público)
}

impl EmailClient {
    pub fn connect(config: EmailConfig) -> Result<Self, EmailError> {
        // Coordina: dns → tls → smtp
        let server = dns::resolve_mx(&config.domain)?;
        let tls_stream = tls::connect(&server)?;
        let smtp = smtp::SmtpConnection::new(tls_stream, &config)?;

        Ok(EmailClient { smtp, config })
    }

    pub fn send(&mut self, to: &str, subject: &str, body: &str)
        -> Result<(), EmailError>
    {
        let message = mime::build_message(to, subject, body);
        self.smtp.send(&message)?;
        Ok(())
    }
}

// Prelude para import fácil
pub mod prelude {
    pub use crate::EmailClient;
    pub use crate::EmailConfig;
    pub use crate::EmailError;
}
```

```rust
// Uso:
use email_crate::prelude::*;

let config = EmailConfig { /* ... */ ..Default::default() };
let mut client = EmailClient::connect(config)?;
client.send("user@example.com", "Subject", "Body")?;
// Drop automático cierra la conexión SMTP
```

4 módulos internos, 1 struct facade, 3 tipos públicos, 2 métodos.
El usuario no sabe que existe DNS, TLS, SMTP, ni MIME.
</details>