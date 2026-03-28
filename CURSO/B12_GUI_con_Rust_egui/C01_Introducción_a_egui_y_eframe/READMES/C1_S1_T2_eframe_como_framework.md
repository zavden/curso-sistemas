# eframe como framework

## Índice

1. [egui vs eframe: la distinción](#egui-vs-eframe-la-distinción)
2. [Dependencias y Cargo.toml](#dependencias-y-cargotoml)
3. [El trait eframe::App](#el-trait-eframeapp)
4. [La función main y run_native](#la-función-main-y-run_native)
5. [CreationContext: configuración inicial](#creationcontext-configuración-inicial)
6. [egui::Context: el centro de control](#eguicontext-el-centro-de-control)
7. [El ciclo de vida completo](#el-ciclo-de-vida-completo)
8. [NativeOptions: configuración de la ventana](#nativeoptions-configuración-de-la-ventana)
9. [El update loop en detalle](#el-update-loop-en-detalle)
10. [Múltiples ventanas y paneles](#múltiples-ventanas-y-paneles)
11. [Errores comunes](#errores-comunes)
12. [Cheatsheet](#cheatsheet)
13. [Ejercicios](#ejercicios)

---

## egui vs eframe: la distinción

En el tema anterior vimos que egui es la librería de UI y eframe es el framework que la
envuelve. Es fundamental entender la separación de responsabilidades:

```
  ┌─────────────────────────────────────────────────────────┐
  │                    Tu aplicación                         │
  │                                                          │
  │  struct MyApp { ... }                                    │
  │  impl eframe::App for MyApp { fn update(...) { ... } }  │
  └──────────────────────┬──────────────────────────────────┘
                         │ usa
                         ▼
  ┌─────────────────────────────────────────────────────────┐
  │  eframe                                                  │
  │                                                          │
  │  • Crea la ventana del OS (via winit)                    │
  │  • Captura eventos (mouse, teclado, resize)              │
  │  • Ejecuta el render loop                                │
  │  • Llama a tu update() en cada frame                     │
  │  • Envía los shapes de egui al backend GPU               │
  │  • Gestiona el ciclo de vida (init, run, shutdown)       │
  └──────────────────────┬──────────────────────────────────┘
                         │ usa
                         ▼
  ┌─────────────────────────────────────────────────────────┐
  │  egui (core)                                             │
  │                                                          │
  │  • Define widgets (Button, Label, Slider, TextEdit...)   │
  │  • Calcula layout (posición y tamaño de cada widget)     │
  │  • Genera shapes (rectángulos, texto, líneas)            │
  │  • Gestiona input (qué widget tiene foco, hover)         │
  │  • NO sabe nada sobre ventanas, GPU, ni el OS            │
  └─────────────────────────────────────────────────────────┘
```

Esta separación permite que egui funcione en contextos muy diferentes: nativo con eframe,
en un juego embebido, o compilado a WASM para el navegador. egui genera shapes; algo más
los dibuja.

---

## Dependencias y Cargo.toml

Para crear una aplicación egui nativa, solo necesitás una dependencia:

```toml
[package]
name = "my_app"
version = "0.1.0"
edition = "2021"

[dependencies]
eframe = "0.31"
```

eframe re-exporta egui, así que no necesitás agregarlo por separado:

```rust
use eframe::egui;  // egui viene incluido en eframe
```

### Features opcionales

eframe tiene features que podés habilitar según tus necesidades:

```toml
[dependencies]
eframe = { version = "0.31", features = ["persistence"] }
```

| Feature         | Qué agrega                                          |
|----------------|-----------------------------------------------------|
| `persistence`  | Guardar/restaurar estado al cerrar/abrir (via serde) |
| `wgpu`         | Backend wgpu en lugar de glow (más moderno)          |
| `puffin`       | Profiling integrado                                  |

Por defecto, eframe usa **glow** (OpenGL) como backend de renderizado. Para la mayoría
de las aplicaciones es suficiente.

### Dependencias del sistema

En Linux, egui necesita algunas librerías del sistema para compilar:

```bash
# Fedora / RHEL
sudo dnf install gcc-c++ libxkbcommon-devel openssl-devel \
    wayland-devel libXi-devel libXcursor-devel libXrandr-devel

# Ubuntu / Debian
sudo apt install build-essential libxkbcommon-dev libssl-dev \
    libwayland-dev libxi-dev libxcursor-dev libxrandr-dev

# Arch
sudo pacman -S base-devel libxkbcommon wayland libxi libxcursor libxrandr
```

---

## El trait eframe::App

El contrato entre tu aplicación y eframe se define con un solo trait:

```rust
pub trait App {
    /// Called each time the UI needs repainting.
    fn update(&mut self, ctx: &egui::Context, frame: &mut eframe::Frame);

    // Métodos opcionales con implementaciones por defecto:

    /// Called on shutdown — para guardar estado
    fn save(&mut self, _storage: &mut dyn eframe::Storage) {}

    /// Called once on startup after everything is set up
    fn on_exit(&mut self) {}

    /// How often to repaint (default: only when there is input)
    fn auto_save_interval(&self) -> std::time::Duration {
        std::time::Duration::from_secs(30)
    }
}
```

El método que importa es `update()`. Los demás son opcionales. Veamos la firma:

```rust
fn update(&mut self, ctx: &egui::Context, frame: &mut eframe::Frame);
```

```
  &mut self  →  Tu struct con el estado de la aplicación (mutable)
  ctx        →  El contexto de egui (para crear panels, pedir repaint, etc.)
  frame      →  Info del frame actual (tamaño de ventana, etc.)
```

### Implementación mínima

```rust
use eframe::egui;

struct MinimalApp;

impl eframe::App for MinimalApp {
    fn update(&mut self, ctx: &egui::Context, _frame: &mut eframe::Frame) {
        egui::CentralPanel::default().show(ctx, |ui| {
            ui.label("Hello, egui!");
        });
    }
}
```

Esto es una aplicación completa (excepto `main()`). Muestra una ventana con el texto
"Hello, egui!" y nada más.

---

## La función main y run_native

`main()` es donde arrancás eframe:

```rust
fn main() -> eframe::Result {
    let options = eframe::NativeOptions::default();

    eframe::run_native(
        "My App Title",                                    // título de la ventana
        options,                                           // configuración de ventana
        Box::new(|_cc| Ok(Box::new(MinimalApp))),          // factory function
    )
}
```

### Desglose de run_native

```rust
pub fn run_native(
    app_name: &str,                                        // 1
    native_options: NativeOptions,                         // 2
    app_creator: AppCreator,                               // 3
) -> eframe::Result;
```

```
  1. app_name        → Título que aparece en la barra de la ventana
  2. native_options  → Configuración: tamaño, posición, decoraciones, etc.
  3. app_creator     → Closure que crea tu App. Recibe CreationContext.
```

### El tipo AppCreator

`AppCreator` es un alias para un closure que recibe `&CreationContext` y retorna tu App:

```
type AppCreator = Box<dyn FnOnce(&CreationContext<'_>) -> Result<Box<dyn App>>>;
```

Descompuesto:

```
Box<dyn FnOnce(               ← Closure que se ejecuta UNA VEZ
    &CreationContext<'_>       ← Con acceso al contexto de creación
) -> Result<                   ← Puede fallar
    Box<dyn App>               ← Retorna tu App como trait object
>>
```

En la práctica, casi siempre se ve así:

```rust
Box::new(|cc| {
    // cc es el CreationContext — lo podés usar para configurar
    Ok(Box::new(MyApp::new(cc)))
})
```

O si no necesitás el `CreationContext`:

```rust
Box::new(|_cc| Ok(Box::new(MyApp::default())))
```

### Retorno de main

`eframe::run_native` retorna `eframe::Result` (que es `Result<(), eframe::Error>`).
Cuando la ventana se cierra, `run_native` retorna `Ok(())`. Si hay un error de
inicialización (no se pudo crear la ventana, no hay GPU disponible), retorna `Err(...)`.

```rust
fn main() -> eframe::Result {
    // run_native bloquea hasta que la ventana se cierra
    eframe::run_native("App", options, creator)?;
    // Este código se ejecuta después de cerrar la ventana
    println!("Window closed, goodbye!");
    Ok(())
}
```

---

## CreationContext: configuración inicial

El `CreationContext` que recibe tu factory function te da acceso a recursos que solo
están disponibles al momento de la creación:

```rust
pub struct CreationContext<'s> {
    pub egui_ctx: egui::Context,       // El contexto de egui (para configurar estilos)
    pub storage: Option<&'s dyn Storage>, // Estado persistido de la sesión anterior
    pub gl: Option<&'s glow::Context>,    // Acceso al contexto OpenGL (si usás glow)
    pub wgpu_render_state: Option<...>,   // Acceso a wgpu (si usás wgpu)
}
```

### Uso típico: configurar fuentes y estilos

```rust
struct MyApp {
    name: String,
}

impl MyApp {
    fn new(cc: &eframe::CreationContext<'_>) -> Self {
        // Configurar estilo visual
        cc.egui_ctx.set_visuals(egui::Visuals::dark());

        // Configurar fuentes
        let mut fonts = egui::FontDefinitions::default();
        fonts.font_data.insert(
            "my_font".to_owned(),
            std::sync::Arc::new(egui::FontData::from_static(
                include_bytes!("/path/to/font.ttf")
            )),
        );
        cc.egui_ctx.set_fonts(fonts);

        Self {
            name: String::new(),
        }
    }
}
```

### Uso típico: restaurar estado persistido

```rust
impl MyApp {
    fn new(cc: &eframe::CreationContext<'_>) -> Self {
        // Intentar restaurar estado de la sesión anterior
        if let Some(storage) = cc.storage {
            if let Some(state) = eframe::get_value(storage, eframe::APP_KEY) {
                return state;  // Restaurar el App deserializado
            }
        }
        // Si no hay estado guardado, usar valores por defecto
        Self::default()
    }
}
```

Para que esto funcione, tu App necesita derivar `serde::Serialize` y
`serde::Deserialize`, y eframe necesita la feature `persistence`.

---

## egui::Context: el centro de control

`egui::Context` es el objeto más importante de egui. Lo recibís en cada llamada a
`update()` y es tu interfaz con todo el sistema de UI:

```
  egui::Context
  ├── Crear panels    → CentralPanel::show(ctx, ...)
  ├── Crear windows   → egui::Window::new("...").show(ctx, ...)
  ├── Pedir repaint   → ctx.request_repaint()
  ├── Leer input      → ctx.input(|i| i.key_pressed(Key::Escape))
  ├── Modificar output→ ctx.output_mut(|o| o.cursor_icon = ...)
  ├── Estilos          → ctx.set_visuals(Visuals::dark())
  ├── Fuentes          → ctx.set_fonts(FontDefinitions::...)
  ├── Memoria interna → ctx.memory_mut(|mem| ...)
  ├── Datos temporales→ ctx.data_mut(|data| ...)
  └── Info del frame  → ctx.used_size(), ctx.screen_rect()
```

### Operaciones más usadas con Context

**Solicitar repaint:**

```rust
// Redibujar lo antes posible (en el próximo frame)
ctx.request_repaint();

// Redibujar después de un tiempo (útil para animaciones/relojes)
ctx.request_repaint_after(std::time::Duration::from_secs(1));
```

Por defecto, egui solo redibuja cuando hay input del usuario (mouse, teclado). Si tu
app muestra datos que cambian solos (un reloj, datos de red), necesitás pedir repaint
explícitamente.

**Leer input:**

```rust
fn update(&mut self, ctx: &egui::Context, _frame: &mut eframe::Frame) {
    // Detectar tecla Escape
    if ctx.input(|i| i.key_pressed(egui::Key::Escape)) {
        // cerrar un diálogo, por ejemplo
        self.dialog_open = false;
    }

    // Detectar Ctrl+S
    if ctx.input(|i| {
        i.modifiers.ctrl && i.key_pressed(egui::Key::S)
    }) {
        self.save_file();
    }

    // Posición del mouse
    let mouse_pos = ctx.input(|i| i.pointer.hover_pos());
}
```

**Configurar estilos en runtime:**

```rust
// Cambiar entre dark y light mode
if ui.button("Toggle theme").clicked() {
    let visuals = if ctx.style().visuals.dark_mode {
        egui::Visuals::light()
    } else {
        egui::Visuals::dark()
    };
    ctx.set_visuals(visuals);
}
```

### Context es Clone y Send

`egui::Context` implementa `Clone` y es barato de clonar (internamente es un `Arc`).
Esto significa que podés pasarlo a otros threads:

```rust
let ctx = ctx.clone();
std::thread::spawn(move || {
    // Hacer trabajo pesado en otro thread...
    let result = heavy_computation();
    // Cuando termina, pedir repaint desde el thread secundario
    ctx.request_repaint();
});
```

Esto es crucial para mantener la UI responsiva: hacés trabajo pesado en un thread
aparte y solo pedís repaint cuando los datos están listos.

---

## El ciclo de vida completo

Desde que se ejecuta `main()` hasta que se cierra la ventana:

```
  main()
    │
    ▼
  eframe::run_native(name, options, creator)
    │
    ├── 1. Crear ventana del OS (winit)
    │
    ├── 2. Inicializar backend GPU (glow/wgpu)
    │
    ├── 3. Crear egui::Context
    │
    ├── 4. Cargar Storage (si persistence está habilitado)
    │
    ├── 5. Llamar a creator(CreationContext) → obtener tu App
    │      (aquí configurás fuentes, estilos, restaurás estado)
    │
    ├── 6. LOOP ──────────────────────────────────────────┐
    │      │                                               │
    │      ├── Recoger eventos del OS                      │
    │      ├── Alimentar eventos a egui                    │
    │      ├── Llamar app.update(ctx, frame) ← TU CÓDIGO  │
    │      ├── Tessellate shapes → triángulos              │
    │      ├── Enviar triángulos al GPU                    │
    │      ├── Presentar frame                             │
    │      ├── ¿Auto-save? → app.save(storage)             │
    │      └── ¿Cerrar? ──── No → repetir ────────────────┘
    │                    │
    │                   Sí
    │                    │
    ├── 7. Llamar app.save(storage) (guardado final)
    │
    ├── 8. Llamar app.on_exit()
    │
    ├── 9. Limpiar recursos GPU
    │
    └── 10. Retornar Ok(()) a main()
```

### save() y persistencia

Si habilitaste la feature `persistence`, eframe guarda automáticamente el estado de
tu App cada `auto_save_interval()` (30 segundos por defecto) y al cerrar la ventana.
Tu App debe implementar `serde::Serialize`/`Deserialize`:

```rust
#[derive(serde::Serialize, serde::Deserialize)]
#[serde(default)]
struct MyApp {
    counter: i32,
    name: String,
    #[serde(skip)]  // No persistir este campo
    temp_data: Vec<u8>,
}

impl eframe::App for MyApp {
    fn save(&mut self, storage: &mut dyn eframe::Storage) {
        eframe::set_value(storage, eframe::APP_KEY, self);
    }

    fn update(&mut self, ctx: &egui::Context, _frame: &mut eframe::Frame) {
        // ...
    }
}
```

El almacenamiento usa un archivo en el directorio de datos de la aplicación del OS
(`~/.local/share/app_name` en Linux).

---

## NativeOptions: configuración de la ventana

`NativeOptions` controla cómo se crea la ventana:

```rust
let options = eframe::NativeOptions {
    viewport: egui::ViewportBuilder::default()
        .with_inner_size([800.0, 600.0])       // Tamaño inicial
        .with_min_inner_size([400.0, 300.0])    // Tamaño mínimo
        .with_title("My Application")           // Título (también se pasa en run_native)
        .with_icon(load_icon()),                // Ícono de la ventana

    centered: true,                             // Centrar en la pantalla

    ..Default::default()
};
```

### Opciones del viewport más usadas

```rust
ViewportBuilder::default()
    // Tamaño
    .with_inner_size([width, height])
    .with_min_inner_size([min_w, min_h])
    .with_max_inner_size([max_w, max_h])

    // Posición
    .with_position([x, y])                // Posición en pantalla

    // Decoraciones
    .with_decorations(true)               // Barra de título del OS (default: true)
    .with_transparent(true)               // Fondo transparente
    .with_resizable(true)                 // Permitir redimensionar (default: true)

    // Comportamiento
    .with_always_on_top()                 // Siempre encima
    .with_maximized(true)                 // Iniciar maximizada
    .with_fullscreen(true)                // Pantalla completa

    // Ícono
    .with_icon(icon_data)                 // egui::IconData
```

### Cargar un ícono

```rust
fn load_icon() -> egui::IconData {
    let icon_bytes = include_bytes!("../assets/icon.png");
    let image = image::load_from_memory(icon_bytes)
        .expect("Failed to load icon")
        .into_rgba8();
    let (width, height) = image.dimensions();

    egui::IconData {
        rgba: image.into_raw(),
        width,
        height,
    }
}
```

Necesitás agregar `image = "0.25"` a tu `Cargo.toml` para decodificar el PNG.

---

## El update loop en detalle

Veamos exactamente qué ocurre dentro de tu `update()`:

```rust
fn update(&mut self, ctx: &egui::Context, _frame: &mut eframe::Frame) {
    // 1. Panels — definen las regiones principales de la ventana
    egui::TopBottomPanel::top("menu_bar").show(ctx, |ui| {
        // Región fija arriba
        ui.horizontal(|ui| {
            ui.label("Menu");
        });
    });

    egui::SidePanel::left("side_panel")
        .default_width(200.0)
        .show(ctx, |ui| {
            // Región fija a la izquierda
            ui.heading("Navigation");
        });

    // CentralPanel toma el espacio restante — siempre al final
    egui::CentralPanel::default().show(ctx, |ui| {
        // Región central, todo lo que sobra
        ui.heading("Main Content");

        // 2. Widgets dentro del panel
        ui.label("Some text");

        if ui.button("Click me").clicked() {
            self.do_something();
        }

        // 3. Layout helpers para organizar widgets
        ui.horizontal(|ui| {
            ui.label("Name:");
            ui.text_edit_singleline(&mut self.name);
        });

        // 4. Condicionales — UI que aparece/desaparece
        if self.show_details {
            ui.separator();
            ui.label("Details...");
        }
    });

    // 5. Windows flotantes (pueden ir fuera de los panels)
    if self.show_settings {
        egui::Window::new("Settings")
            .open(&mut self.show_settings)  // botón X para cerrar
            .show(ctx, |ui| {
                ui.label("Settings go here");
            });
    }
}
```

### Orden de evaluación

El orden en que declarás los panels importa para el layout:

```
  Paso 1: TopBottomPanel::top → reserva espacio arriba
  Paso 2: SidePanel::left → reserva espacio a la izquierda
  Paso 3: CentralPanel → toma TODO el espacio restante

  ┌──────────────────────────────────┐
  │         TopBottomPanel           │
  ├────────┬─────────────────────────┤
  │        │                         │
  │ Side   │     CentralPanel        │
  │ Panel  │     (espacio restante)  │
  │        │                         │
  │        │                         │
  └────────┴─────────────────────────┘
```

Si invertís el orden (primero CentralPanel, después SidePanel), CentralPanel se queda
con todo el espacio y SidePanel no tiene dónde ir. **CentralPanel siempre va último.**

### Response: el return de cada widget

Cada widget retorna un `Response` con información sobre la interacción:

```rust
let response = ui.button("Click");

response.clicked()       // ¿Se hizo click este frame?
response.double_clicked() // ¿Doble click?
response.hovered()       // ¿El mouse está encima?
response.changed()       // ¿El valor cambió? (para sliders, text_edit)
response.lost_focus()    // ¿Perdió el foco del teclado?
response.gained_focus()  // ¿Ganó el foco?
response.dragged()       // ¿Se está arrastrando?
response.drag_delta()    // Cuánto se arrastró (Vec2)
```

Esto es lo que permite que immediate mode maneje eventos sin callbacks: el resultado
de la interacción es el return value de la función que dibuja el widget.

```rust
// Ejemplo: tooltip condicional
let r = ui.button("Save");
if r.hovered() {
    r.on_hover_text("Save file to disk (Ctrl+S)");
}
if r.clicked() {
    self.save();
}
```

---

## Múltiples ventanas y paneles

### egui::Window (ventanas flotantes)

Las "Windows" de egui son paneles flotantes dentro de la ventana principal del OS.
No son ventanas del sistema operativo — son paneles que se pueden mover, redimensionar
y cerrar dentro del viewport de egui:

```rust
egui::Window::new("My Window")
    .open(&mut self.window_open)          // Botón X, controla visibilidad
    .default_size([300.0, 200.0])         // Tamaño inicial
    .resizable(true)                      // Permitir resize
    .collapsible(true)                    // Permitir colapsar con ▾
    .anchor(egui::Align2::CENTER_CENTER, [0.0, 0.0]) // Posición fija
    .show(ctx, |ui| {
        ui.label("Window content");
    });
```

### Combinando panels

```rust
fn update(&mut self, ctx: &egui::Context, _frame: &mut eframe::Frame) {
    // Barra superior con menú
    egui::TopBottomPanel::top("top").show(ctx, |ui| {
        egui::menu::bar(ui, |ui| {
            ui.menu_button("File", |ui| {
                if ui.button("Quit").clicked() {
                    ctx.send_viewport_cmd(egui::ViewportCommand::Close);
                }
            });
        });
    });

    // Barra inferior con estado
    egui::TopBottomPanel::bottom("bottom").show(ctx, |ui| {
        ui.horizontal(|ui| {
            ui.label("Status: Ready");
            ui.with_layout(egui::Layout::right_to_left(egui::Align::Center), |ui| {
                ui.label("v1.0.0");
            });
        });
    });

    // Panel lateral
    egui::SidePanel::left("left").show(ctx, |ui| {
        ui.heading("Tools");
        ui.selectable_value(&mut self.tab, Tab::Home, "Home");
        ui.selectable_value(&mut self.tab, Tab::Settings, "Settings");
    });

    // Centro — siempre último
    egui::CentralPanel::default().show(ctx, |ui| {
        match self.tab {
            Tab::Home => self.show_home(ui),
            Tab::Settings => self.show_settings(ui),
        }
    });
}
```

Esto produce un layout típico de aplicación:

```
  ┌─────────────────────────────────────────┐
  │  File  Edit  View                       │  ← TopBottomPanel::top
  ├─────────┬───────────────────────────────┤
  │         │                               │
  │  Home   │     Contenido central         │
  │  Settings│    según tab seleccionado    │
  │         │                               │  ← SidePanel::left + CentralPanel
  │         │                               │
  │         │                               │
  ├─────────┴───────────────────────────────┤
  │  Status: Ready                   v1.0.0 │  ← TopBottomPanel::bottom
  └─────────────────────────────────────────┘
```

---

## Errores comunes

### 1. Poner CentralPanel antes que otros panels

```rust
// MAL: CentralPanel se queda con todo el espacio
egui::CentralPanel::default().show(ctx, |ui| { /* ... */ });
egui::SidePanel::left("nav").show(ctx, |ui| { /* ... */ }); // No tiene espacio

// BIEN: declarar los panels periféricos primero
egui::SidePanel::left("nav").show(ctx, |ui| { /* ... */ });
egui::CentralPanel::default().show(ctx, |ui| { /* ... */ }); // Toma el resto
```

### 2. Olvidar pedir repaint para datos que cambian solos

```rust
fn update(&mut self, ctx: &egui::Context, _frame: &mut eframe::Frame) {
    egui::CentralPanel::default().show(ctx, |ui| {
        // Mostrar la hora actual
        let now = chrono::Local::now();
        ui.label(format!("Time: {}", now.format("%H:%M:%S")));

        // MAL: sin request_repaint, la hora se congela cuando el mouse no se mueve

        // BIEN: pedir repaint cada segundo
        ctx.request_repaint_after(std::time::Duration::from_secs(1));
    });
}
```

### 3. Bloquear el update con trabajo pesado

```rust
// MAL: la UI se congela mientras calcula
if ui.button("Calculate").clicked() {
    self.result = heavy_computation();  // Bloquea el thread de UI
}

// BIEN: delegar a un thread
if ui.button("Calculate").clicked() {
    self.computing = true;
    let ctx = ctx.clone();
    let (tx, rx) = std::sync::mpsc::channel();
    self.receiver = Some(rx);
    std::thread::spawn(move || {
        let result = heavy_computation();
        let _ = tx.send(result);
        ctx.request_repaint();  // Avisar a la UI que hay resultado
    });
}
// Chequear resultado
if let Some(rx) = &self.receiver {
    if let Ok(result) = rx.try_recv() {
        self.result = result;
        self.computing = false;
        self.receiver = None;
    }
}
```

### 4. No entender que update() se llama muchas veces

```rust
// MAL: abrir un archivo en cada frame
fn update(&mut self, ctx: &egui::Context, _frame: &mut eframe::Frame) {
    let data = std::fs::read_to_string("data.txt").unwrap();  // ¡60 veces/segundo!
    // ...
}

// BIEN: cargar una vez y guardar en el estado
fn update(&mut self, ctx: &egui::Context, _frame: &mut eframe::Frame) {
    if self.data.is_none() {
        self.data = Some(std::fs::read_to_string("data.txt").unwrap());
    }
    // Usar self.data...
}
```

### 5. Confundir las "windows" de egui con ventanas del OS

`egui::Window` crea paneles flotantes **dentro** de la ventana del OS. No son ventanas
nativas. No aparecen en la taskbar. No pueden salir de los límites de la ventana
principal. Si necesitás múltiples ventanas del OS, eso requiere viewports (feature
avanzada de eframe).

---

## Cheatsheet

```
EFRAME — REFERENCIA RÁPIDA
============================

Cargo.toml:
  [dependencies]
  eframe = "0.31"

Imports:
  use eframe::egui;

Estructura mínima:
  struct App { ... }
  impl eframe::App for App {
      fn update(&mut self, ctx: &egui::Context, _frame: &mut eframe::Frame) {
          egui::CentralPanel::default().show(ctx, |ui| { ... });
      }
  }
  fn main() -> eframe::Result {
      eframe::run_native("Title", NativeOptions::default(),
          Box::new(|_cc| Ok(Box::new(App::default()))));
  }

CreationContext (cc):
  cc.egui_ctx          → para configurar estilos/fuentes
  cc.storage           → estado persistido de sesión anterior

Context (ctx):
  ctx.request_repaint()             → forzar redibujado
  ctx.request_repaint_after(dur)    → redibujado diferido
  ctx.input(|i| ...)                → leer teclado/mouse
  ctx.set_visuals(Visuals::dark())  → cambiar tema
  ctx.send_viewport_cmd(cmd)        → cerrar ventana, etc.

Panels (orden de declaración importa):
  TopBottomPanel::top("id")    → barra superior
  TopBottomPanel::bottom("id") → barra inferior
  SidePanel::left("id")        → panel izquierdo
  SidePanel::right("id")       → panel derecho
  CentralPanel::default()      → centro (SIEMPRE último)

Windows (flotantes dentro de la ventana):
  egui::Window::new("Title")
      .open(&mut bool)
      .show(ctx, |ui| { ... });

Response (retorno de widgets):
  .clicked()        .hovered()        .changed()
  .double_clicked() .lost_focus()     .gained_focus()
  .dragged()        .drag_delta()     .on_hover_text("...")

NativeOptions:
  viewport: ViewportBuilder::default()
      .with_inner_size([w, h])
      .with_min_inner_size([w, h])
      .with_decorations(bool)
      .with_resizable(bool)
      .with_maximized(bool)
      .with_fullscreen(bool)
  centered: true

Persistencia (feature "persistence"):
  #[derive(serde::Serialize, serde::Deserialize)]
  struct App { ... }
  fn save(&mut self, storage: &mut dyn Storage) {
      eframe::set_value(storage, APP_KEY, self);
  }
```

---

## Ejercicios

### Ejercicio 1: Ciclo de vida

Escribí el código completo de una aplicación eframe que:

- Tenga un struct `GreeterApp` con campos `name: String` y `greetings: Vec<String>`
- En `update()`: un `TextEdit` para el nombre, un botón "Greet" que agregue
  `format!("Hello, {}!", self.name)` al vector, y un loop que muestre todos los
  saludos como labels
- Use `NativeOptions` para crear una ventana de 600x400 píxeles, centrada

Probá compilarlo y ejecutarlo en tu máquina.

> **Pregunta de predicción**: Si escribís un nombre y hacés click en "Greet" tres
> veces seguidas, ¿cuántos labels de saludo aparecerán? ¿Y si cambiás el nombre entre
> clicks? ¿Qué pasa con los saludos anteriores — se actualizan o permanecen?

### Ejercicio 2: Panels y layout

Partiendo del ejercicio anterior, modificá la aplicación para tener:

- `TopBottomPanel::top` con el texto "Greeter App" como heading
- `SidePanel::left` con el campo de nombre y el botón "Greet"
- `CentralPanel` con la lista de saludos

Dibujá primero en papel cómo debería verse el layout antes de escribir código.

> **Pregunta de predicción**: ¿Qué pasa si ponés `CentralPanel` antes que
> `SidePanel::left` en tu código? Probalo y explicá el resultado. ¿Por qué egui
> necesita que CentralPanel vaya último?

### Ejercicio 3: Context y repaint

Escribí una aplicación que muestre un contador que se incrementa automáticamente cada
segundo (sin que el usuario tenga que mover el mouse). Mostrá el valor actual con
`ui.heading()`.

Pistas:
- Necesitás `std::time::Instant` para medir el tiempo desde el inicio
- Necesitás `ctx.request_repaint_after()` para que egui redibuje
- El contador es simplemente los segundos transcurridos desde que se abrió la app

> **Pregunta de predicción**: Si comentás la línea `ctx.request_repaint_after(...)`,
> ¿qué ocurre con el contador? ¿Se congela por completo, o se actualiza solo cuando
> movés el mouse? ¿Por qué?
