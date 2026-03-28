# Bloque 13: Multimedia con GStreamer — Fases 4 y 5

**Objetivo**: Integrar GStreamer con egui para reproducción de audio y video.

**Nota**: Se ejecuta en el host. GStreamer requiere acceso a dispositivos de audio/video.

## Capítulo 1: Introducción a GStreamer [A]

### Sección 1: Instalación y Verificación
- **T01 - Paquetes en Debian/Ubuntu**: gstreamer1.0-tools, libgstreamer1.0-dev, gstreamer1.0-plugins-{base,good,bad,ugly}, diferencias de licencia entre plugins
- **T02 - Paquetes en Fedora/RHEL**: gstreamer1-devel, gstreamer1-plugins-{base,good,bad-free,ugly-free}, RPM Fusion para codecs propietarios
- **T03 - Verificación del entorno**: gst-inspect-1.0 (listar plugins), gst-launch-1.0 (probar pipelines desde CLI), diagnóstico de plugins faltantes
- **T04 - Bindings Rust (gstreamer-rs)**: dependencia en Cargo.toml, cómo el link con las librerías nativas funciona (pkg-config), errores comunes de compilación y cómo resolverlos

### Sección 2: Fundamentos
- **T01 - Arquitectura**: pipelines, elements, pads, caps, bus
- **T02 - Bindings de Rust**: gstreamer-rs, inicialización, pipeline textual
- **T03 - Estados**: Null → Ready → Paused → Playing, transiciones
- **T04 - El bus de mensajes**: escuchar EOS, Error, StateChanged, tags

### Sección 3: Pipelines
- **T01 - Elements comunes**: filesrc, decodebin, autoaudiosink, autovideosink
- **T02 - playbin**: URI playback todo-en-uno, propiedades (volume, uri, current-uri)
- **T03 - Pipeline custom**: enlazar elements manualmente, sometimes pads, pad linking

## Capítulo 2: Pipeline de Audio [A]

### Sección 1: Audio en GStreamer
- **T01 - Decodificación**: formatos (MP3, WAV, FLAC, OGG), decodebin vs elementos específicos
- **T02 - AppSink para audio**: acceso a muestras crudas, formato de audio, canales
- **T03 - Metadata**: GStreamer Discoverer, tags, id3 crate como alternativa
- **T04 - Posición y duración**: query de posición/duración, seek preciso

## Capítulo 3: Proyecto — Reproductor de Audio [P] (Fase 4)

### Sección 1: Reproductor Básico
- **T01 - Base**: abrir MP3/WAV/FLAC, play/pause, solo un botón
- **T02 - Mejora 1**: barra de progreso visual (no interactiva)
- **T03 - Mejora 2**: barra interactiva (click para saltar, seek)
- **T04 - Mejora 3**: control de volumen con slider
- **T05 - Mejora 4**: metadatos (título, artista, álbum)

### Sección 2: Reproductor con Playlist
- **T01 - Base**: lista de archivos, click para reproducir, anterior/siguiente/repetir/mezclar
- **T02 - Mejora 1**: drag de archivos/carpetas a la lista
- **T03 - Mejora 2**: persistir playlist entre sesiones (serde + JSON)
- **T04 - Mejora 3**: visualizador de forma de onda (AppSink, muestras de audio, Painter)

## Capítulo 4: Pipeline de Video [A]

### Sección 1: Video en GStreamer
- **T01 - Decodificación de video**: formatos, codecs, contenedores (MP4, MKV, WebM)
- **T02 - AppSink para video**: frames RGBA, SampleRef, MapInfo, buffer → textura
- **T03 - new_preroll vs new_sample**: primer frame vs frames sucesivos
- **T04 - Sincronización A/V**: timestamps, pipeline clock, por qué importa

### Sección 2: Renderizado
- **T01 - Frames a textura egui**: TextureHandle, update frame por frame
- **T02 - Aspect ratio y escalado**: fit-to-window, letter/pillarboxing
- **T03 - Performance**: tasa de frames, buffering, threading entre GStreamer y egui

## Capítulo 5: Proyecto — Reproductores de Video [P] (Fase 5)

### Sección 1: Reproductor Básico
- **T01 - Base**: play y pause, sin barra de tiempo
- **T02 - Mejora 1**: drag & drop de archivos
- **T03 - Mejora 2**: volumen con slider

### Sección 2: Reproductor con Timeline
- **T01 - Base**: barra de tiempo interactiva, posición/duración
- **T02 - Mejora 1**: frame stepping (Shift+←/→, seek preciso por frame)
- **T03 - Mejora 2**: velocidad de reproducción (0.5×, 1×, 2×) con set_rate
- **T04 - Mejora 3**: miniaturas en timeline al hover (pipeline auxiliar de thumbnails)

### Sección 3: Reproductor con Marcadores
- **T01 - Base**: crear marcadores con nombre en posición actual, lista lateral, click para saltar
- **T02 - Mejora 1**: exportar marcadores como .txt con timestamps
- **T03 - Mejora 2**: leer marcadores MKV (toc/chapters en GStreamer)
- **T04 - Mejora 3 (subtítulos)**: cargar archivos SRT/ASS, renderizar subtítulos sincronizados sobre el video, playbin subtitle-uri — extensión opcional
