# Bloque 14: Interoperabilidad C/Rust y WebAssembly

**Objetivo**: Integrar C y Rust en proyectos compartidos y preparar el terreno para
WebAssembly. Bloque capstone que conecta múltiples bloques anteriores.

## Capítulo 1: FFI Avanzado [M]

### Sección 1: Patrones Complejos
- **T01 - Callbacks**: pasar funciones Rust como callbacks a C, closures con trampolines
- **T02 - Tipos opacos bidireccionales**: C→Rust y Rust→C, lifetime management
- **T03 - Error handling cruzado**: propagar errores entre ambos lenguajes, convenciones
- **T04 - Datos complejos**: structs con punteros anidados, ownership semántica, free functions

### Sección 2: Tooling
- **T01 - bindgen avanzado**: allowlist, blocklist, wrappers, build.rs integration
- **T02 - cbindgen avanzado**: renaming, includes, orden de declaraciones
- **T03 - Integración CMake+Cargo**: llamar cargo desde cmake, linking cruzado

## Capítulo 2: Bibliotecas Compartidas C/Rust [M]

### Sección 1: Biblioteca Dinámica
- **T01 - Rust cdylib**: crate-type = ["cdylib"], exportar funciones
- **T02 - Consumir desde C**: linking, headers generados, ldconfig
- **T03 - C .so consumida desde Rust**: linking, build.rs, pkg-config

### Sección 2: Plugin System
- **T01 - Plugins con dlopen**: sistema de plugins en C con carga dinámica
- **T02 - Plugin en Rust cargado por C**: ABI compatible, versionado
- **T03 - Plugin en C cargado por Rust**: libloading crate, safety wrappers

## Capítulo 3: Introducción a WebAssembly [A]

### Sección 1: Conceptos
- **T01 - Qué es WebAssembly**: modelo de ejecución, sandboxing, formato binario (.wasm) y texto (.wat)
- **T02 - WASI**: WebAssembly System Interface, acceso al filesystem, networking, I/O, preview1 vs preview2
- **T03 - Casos de uso**: navegador, serverless, edge computing, extensibilidad, plugins portables
- **T04 - WASM fuera del navegador**: Wasmtime, Wasmer, WasmEdge — ejecutar WASM en el servidor
- **T05 - WASM como formato de plugin**: aislamiento sin contenedores, comparación con Docker, shared-nothing architecture

### Sección 2: Rust → WASM
- **T01 - wasm-pack**: instalación, build, targets (web, bundler, nodejs)
- **T02 - wasm-bindgen**: #[wasm_bindgen], tipos soportados, JsValue
- **T03 - Ejemplo práctico**: función Rust compilada a WASM, ejecutada en navegador
- **T04 - Limitaciones**: sin threads (con SharedArrayBuffer sí), sin acceso directo a DOM
- **T05 - trunk como alternativa**: bundling simplificado para apps web Rust, index.html como entry point, hot reload, comparación con wasm-pack

### Sección 3: C → WASM
- **T01 - Emscripten**: instalación, emcc, compilar C a WASM
- **T02 - WASI SDK**: compilación sin Emscripten, wasmtime/wasmer para ejecución
- **T03 - Comparación Rust vs C para WASM**: ergonomía, tamaño de binario, ecosistema

## Capítulo 4: Proyecto Final — Aplicación C/Rust con Módulo WASM [P]

### Sección 1: Diseño e Implementación
- **T01 - Biblioteca compartida C/Rust**: lógica de negocio en Rust, interfaz C, tests
- **T02 - Compilación dual**: nativa (.so) y WASM (.wasm) desde el mismo código Rust
- **T03 - Programa C consumidor**: usa la biblioteca nativa vía FFI
- **T04 - Página web consumidora**: usa el módulo WASM, misma lógica, distinto runtime
