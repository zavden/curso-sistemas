# Immediate Mode GUI

## Índice

1. [Qué es una GUI](#qué-es-una-gui)
2. [Retained mode: el modelo tradicional](#retained-mode-el-modelo-tradicional)
3. [Immediate mode: el modelo reactivo](#immediate-mode-el-modelo-reactivo)
4. [Comparación profunda](#comparación-profunda)
5. [El loop de una immediate mode GUI](#el-loop-de-una-immediate-mode-gui)
6. [Anatomía de un frame](#anatomía-de-un-frame)
7. [Estado en immediate mode](#estado-en-immediate-mode)
8. [Tradeoffs y cuándo elegir cada modelo](#tradeoffs-y-cuándo-elegir-cada-modelo)
9. [Historia y adopción](#historia-y-adopción)
10. [Immediate mode en Rust: egui](#immediate-mode-en-rust-egui)
11. [Errores comunes](#errores-comunes)
12. [Cheatsheet](#cheatsheet)
13. [Ejercicios](#ejercicios)

---

## Qué es una GUI

Una **GUI** (Graphical User Interface) es una interfaz que permite al usuario interactuar
con un programa mediante elementos visuales: botones, campos de texto, listas, menús. A
diferencia de una CLI donde el usuario escribe comandos, en una GUI el usuario hace click,
arrastra, escribe en campos, y ve resultados visuales inmediatos.

Construir una GUI requiere resolver varios problemas:

```
+--------------------------------------------------+
|  Problemas fundamentales de una GUI               |
+--------------------------------------------------+
|                                                    |
|  1. Dibujar elementos en pantalla                 |
|  2. Detectar input del usuario (mouse, teclado)   |
|  3. Actualizar el estado de la aplicación          |
|  4. Redibujar los elementos que cambiaron          |
|  5. Gestionar el layout (posición y tamaño)        |
|  6. Mantener la interfaz responsiva                |
|                                                    |
+--------------------------------------------------+
```

Existen dos paradigmas fundamentalmente distintos para resolver estos problemas:
**retained mode** e **immediate mode**.

---

## Retained mode: el modelo tradicional

En retained mode, la GUI mantiene un **árbol de objetos** persistente en memoria. Cada
widget (botón, label, campo de texto) es un objeto que existe independientemente del
ciclo de renderizado. Tú creas los objetos, los configuras, los conectas a callbacks, y
el framework se encarga de dibujarlos y actualizarlos.

### Cómo funciona

```
 Inicialización (una vez)                    Evento
 ========================                    ======

 let button = Button::new("Click me");       Usuario hace click
 button.set_position(100, 50);                      │
 button.on_click(|| {                               ▼
     label.set_text("Clicked!");             Framework detecta evento
 });                                                │
 window.add(button);                                ▼
 window.add(label);                          Busca qué widget lo recibe
                                                    │
 // El framework ahora "posee"                      ▼
 // estos objetos y los mantiene             Ejecuta el callback asociado
 // vivos en un árbol interno                       │
                                                    ▼
                                             callback modifica otros widgets
                                                    │
                                                    ▼
                                             Framework redibuja lo necesario
```

### El árbol de widgets

```
              Window
             /      \
        MenuBar    VBox
        /  |  \      |
      File Edit View  +──────────────+
                      |              |
                   HBox           Button
                  /    \         "Submit"
              Label  TextInput
            "Name:"  [________]
```

Cada nodo en este árbol es un **objeto persistente** con su propio estado interno: si un
`TextInput` tiene foco, qué texto contiene, si está habilitado o deshabilitado. El
framework almacena todo esto y lo gestiona entre frames.

### Frameworks retained mode populares

| Framework    | Lenguaje     | Plataforma        |
|-------------|-------------|-------------------|
| Qt          | C++ (bindings) | Multiplataforma |
| GTK         | C (bindings)   | Multiplataforma |
| WPF/WinUI   | C#            | Windows          |
| SwiftUI     | Swift         | Apple            |
| Flutter     | Dart          | Multiplataforma  |
| React (DOM) | JavaScript    | Web              |

### Ejemplo conceptual (pseudocódigo)

```
// Retained mode: creamos objetos que persisten
fn setup() {
    let counter = 0;
    let label = Label::new("Count: 0");
    let button = Button::new("+1");

    button.on_click(move || {
        counter += 1;
        label.set_text(format!("Count: {}", counter));
    });

    window.add_vertical(label, button);
    window.show();
    // Y ya está. El framework se encarga de todo.
}
```

El programador **no dibuja nada manualmente**. Crea objetos, conecta eventos, y el
framework resuelve el renderizado.

---

## Immediate mode: el modelo reactivo

En immediate mode, **no hay árbol de objetos persistente**. En cada frame (típicamente
60 veces por segundo), tu código describe la interfaz completa desde cero. Los widgets
no existen como objetos entre frames — existen solo durante la llamada que los crea.

### Cómo funciona

```
 Frame N                          Frame N+1
 =======                          =========

 fn update(state) {               fn update(state) {
   label("Count: {state.n}");       label("Count: {state.n}");
   if button("+1").clicked() {      if button("+1").clicked() {
     state.n += 1;                    state.n += 1;
   }                                }
 }                                }

 // Se ejecuta CADA frame          // Se ejecuta CADA frame
 // ~60 veces por segundo          // El MISMO código
 // Describe la UI completa        // Pero state.n puede haber cambiado
```

La función `update` se llama en cada frame. No hay callbacks. No hay árbol almacenado.
El código **es** la descripción de la UI. Si el estado cambió, la siguiente llamada a
`update` producirá una UI diferente — automáticamente.

### La metáfora de la pizarra

La mejor forma de entender immediate mode es la metáfora de la pizarra:

```
  Retained mode = Stickers en una pizarra
  =========================================
  - Pegas un sticker "Botón" en la posición (100, 50)
  - El sticker se queda ahí permanentemente
  - Para moverlo, lo despegás y lo pegás en otro lugar
  - Para borrarlo, lo despegás
  - El sticker "existe" independientemente de que lo mires

  Immediate mode = Dibujar en una pizarra cada frame
  ===================================================
  - Frame 1: Dibujás "Botón" en (100, 50)
  - Frame 2: Borrás todo, dibujás "Botón" en (100, 50) otra vez
  - Frame 3: Borrás todo, dibujás "Botón" en (120, 50) ← se movió
  - Frame 4: Borrás todo, no dibujás el botón ← desapareció
  - Nada "existe" entre frames, solo el estado que vos mantenés
```

### Ejemplo conceptual (pseudocódigo)

```
// Immediate mode: describimos la UI cada frame
struct MyApp {
    counter: i32,
}

fn update(app: &mut MyApp, ui: &mut Ui) {
    ui.label(format!("Count: {}", app.counter));

    if ui.button("+1").clicked() {
        app.counter += 1;
    }
    // No hay callbacks, no hay objetos widget.
    // ui.button() dibuja el botón Y retorna si fue clickeado.
    // Todo en una sola llamada.
}
```

Observá la diferencia fundamental: `ui.button("+1")` **dibuja** el botón y al mismo
tiempo **retorna** si fue clickeado. No hay separación entre definir la UI y manejar
eventos. Es una sola operación atómica.

---

## Comparación profunda

### Modelo mental

```
  Retained Mode                        Immediate Mode
  ==============                       ==============

  "Construí una vez,                   "Describí cada frame,
   el framework mantiene"               yo mantengo el estado"

  Objetos ←→ Callbacks                 Funciones ←→ Estado
  (orientado a objetos)                (orientado a datos)

  UI = árbol de objetos                UI = resultado de ejecutar código
  Estado distribuido en widgets        Estado centralizado en tu struct
  Eventos vía callbacks/señales        Eventos vía return values
```

### Tabla comparativa

| Aspecto              | Retained Mode              | Immediate Mode              |
|---------------------|---------------------------|---------------------------|
| **Widgets**          | Objetos persistentes       | Efímeros (solo en el frame) |
| **Estado de UI**     | En los widgets             | En tu struct de estado      |
| **Eventos**          | Callbacks / señales        | Return values (clicked?)    |
| **Renderizado**      | Framework redibuja selectivamente | Se redescribe todo cada frame |
| **Layout**           | Cálculo incremental        | Cálculo completo cada frame |
| **Código**           | Imperativo + declarativo   | Puramente procedural        |
| **Sincronización**   | Widget ↔ modelo (binding)  | No hay — el estado ES la fuente |
| **Curva de aprendizaje** | Más conceptos (señales, slots, bindings) | Menos conceptos, más directo |
| **Complejidad de UI** | Escala mejor para UIs grandes | Más simple para UIs medianas |
| **Personalización**  | Requiere subclassing/CSS   | Cualquier cosa que puedas dibujar |
| **Accesibilidad**    | Soportada (árbol semántico) | Difícil (no hay árbol)      |
| **Texto complejo**   | Bien soportado             | Limitado                    |
| **Memoria**          | Más (objetos persistentes) | Menos (nada persiste)       |
| **CPU por frame**    | Menos (actualización parcial) | Más (recalcula todo)       |

### El problema del binding en retained mode

Uno de los dolores más comunes en retained mode es mantener sincronizados el modelo de
datos y los widgets de la UI:

```
// Retained mode: el "binding hell"
struct Model {
    name: String,
    age: u32,
}

fn setup(model: &Model) {
    let name_input = TextInput::new(model.name.clone());
    let age_slider = Slider::new(model.age);

    // Problema 1: Si model.name cambia externamente,
    // ¿cómo actualizo name_input?
    // Problema 2: Si el usuario edita name_input,
    // ¿cómo actualizo model.name?
    // Problema 3: Si cambio model.name desde código,
    // ¿cómo evito un loop infinito de actualizaciones?
}
```

En immediate mode este problema **no existe**:

```
// Immediate mode: sin binding
fn update(model: &mut Model, ui: &mut Ui) {
    ui.text_edit(&mut model.name);   // Lee Y escribe directamente
    ui.slider(&mut model.age);       // No hay dos copias del dato
    // El widget siempre muestra el estado actual.
    // El widget modifica el estado directamente.
    // No hay sincronización porque no hay duplicación.
}
```

---

## El loop de una immediate mode GUI

Una aplicación immediate mode vive dentro de un loop infinito que se ejecuta en cada
frame:

```
                 ┌──────────────────────────┐
                 │      Inicio del frame     │
                 └─────────┬────────────────┘
                           │
                           ▼
                 ┌──────────────────────────┐
                 │   Recoger input del OS   │──── mouse, teclado, resize,
                 │   (eventos pendientes)    │     touch, scroll, etc.
                 └─────────┬────────────────┘
                           │
                           ▼
                 ┌──────────────────────────┐
                 │   Ejecutar update()       │──── TU código:
                 │   (describir la UI)       │     ui.label(), ui.button()
                 └─────────┬────────────────┘     cada llamada agrega un
                           │                       "shape" a la lista de
                           ▼                       primitivas a dibujar
                 ┌──────────────────────────┐
                 │   Calcular layout         │──── posición y tamaño final
                 │   (resolver constraints)  │     de cada elemento
                 └─────────┬────────────────┘
                           │
                           ▼
                 ┌──────────────────────────┐
                 │   Rasterizar y pintar     │──── enviar triángulos/texturas
                 │   (GPU o software)        │     al backend de renderizado
                 └─────────┬────────────────┘
                           │
                           ▼
                 ┌──────────────────────────┐
                 │   Presentar en pantalla   │──── swap buffers / vsync
                 └─────────┬────────────────┘
                           │
                           ▼
                 ┌──────────────────────────┐
                 │   ¿Cerrar ventana?        │── No ──→ volver al inicio
                 └─────────┬────────────────┘
                        Sí │
                           ▼
                        [FIN]
```

La clave es que **tu función `update()`** se ejecuta en cada iteración del loop. Cada
llamada a `ui.button("text")` no crea un objeto persistente — genera una serie de
**shapes** (rectángulos, texto) que se dibujan una sola vez y luego se descartan.

### Frecuencia de frames

En una immediate mode GUI, el frame rate determina cuántas veces por segundo se ejecuta
`update()`:

```
60 FPS  → update() se ejecuta 60 veces/segundo  → 16.6 ms por frame
30 FPS  → update() se ejecuta 30 veces/segundo  → 33.3 ms por frame

Si update() tarda más que el presupuesto por frame → la UI se congela
```

En la práctica, frameworks como egui son inteligentes: no redibujan a 60 FPS
constantemente. Solo redibujan cuando hay input del usuario o cuando el programa
solicita explícitamente un repaint. Esto ahorra CPU/batería cuando la UI está idle.

---

## Anatomía de un frame

Veamos qué ocurre internamente cuando ejecutamos código immediate mode:

```rust
fn update(&mut self, ctx: &egui::Context, _frame: &mut eframe::Frame) {
    egui::CentralPanel::default().show(ctx, |ui| {
        ui.heading("My App");

        ui.horizontal(|ui| {
            ui.label("Name:");
            ui.text_edit_singleline(&mut self.name);
        });

        if ui.button("Greet").clicked() {
            self.greeting = format!("Hello, {}!", self.name);
        }

        ui.label(&self.greeting);
    });
}
```

Lo que ocurre internamente en un solo frame:

```
Paso 1: ctx comienza un nuevo frame, recoge input pendiente
         (mouse en posición (230, 180), click izquierdo soltado)

Paso 2: CentralPanel::show() reserva el espacio central disponible

Paso 3: ui.heading("My App")
         → Calcula el tamaño del texto "My App" con fuente heading
         → Reserva un rectángulo vertical de ese tamaño
         → Emite Shape::Text("My App", pos, font, color)

Paso 4: ui.horizontal(|ui| { ... })
         → Cambia la dirección de layout a horizontal dentro del closure

Paso 5: ui.label("Name:")
         → Emite Shape::Text("Name:", pos, font, color)

Paso 6: ui.text_edit_singleline(&mut self.name)
         → Dibuja un rectángulo (el campo)
         → Dibuja el texto actual de self.name dentro
         → Detecta si el mouse clickeó dentro → da foco
         → Si tiene foco y hay input de teclado → modifica self.name
         → Retorna Response (con info de interacción)

Paso 7: ui.button("Greet")
         → Dibuja un rectángulo con texto "Greet"
         → Detecta si el mouse está encima → hover highlight
         → Detecta si hubo click dentro → retorna Response{clicked: true}

Paso 8: if clicked → self.greeting se actualiza

Paso 9: ui.label(&self.greeting)
         → Emite Shape::Text con el contenido de self.greeting

Paso 10: Todos los shapes emitidos se envían al painter
          → Se rasteriza todo → Se presenta en pantalla

Al final del frame: todos los shapes se descartan.
                    Solo self (el estado) sobrevive al siguiente frame.
```

---

## Estado en immediate mode

En immediate mode, **tú eres responsable de todo el estado**. No hay objetos widget que
recuerden si tienen foco, cuánto scroll acumularon, o si están expandidos. Todo eso lo
manejás vos.

### Estado de la aplicación vs estado de la UI

```
  Estado de la aplicación (tuyo)     Estado de la UI (del framework)
  ================================   ===============================
  - Datos del modelo                 - Qué widget tiene foco
  - Configuración                    - Posición de scroll
  - Resultados de cálculos           - Estado de animaciones
  - Historial de acciones            - Hover / tooltip timers
                                     - ID de elementos expandidos

  Lo almacenás en tu struct          egui lo almacena internamente
  MyApp { ... }                      usando un sistema de IDs
```

egui mantiene un poco de estado interno (foco, scroll, animaciones) usando un sistema de
**IDs automáticos** basados en la posición del widget en el árbol de llamadas. Esto es
transparente para el programador en la mayoría de los casos.

### El patrón canónico

```rust
struct MyApp {
    // Estado de la aplicación — TÚ lo definís
    items: Vec<String>,
    new_item: String,
    selected: Option<usize>,
    show_completed: bool,
}

impl eframe::App for MyApp {
    fn update(&mut self, ctx: &egui::Context, _frame: &mut eframe::Frame) {
        // Cada frame: describir la UI basándote en self
        egui::CentralPanel::default().show(ctx, |ui| {
            // Input
            ui.horizontal(|ui| {
                ui.text_edit_singleline(&mut self.new_item);
                if ui.button("Add").clicked() && !self.new_item.is_empty() {
                    self.items.push(self.new_item.drain(..).collect());
                }
            });

            // Condicional — si show_completed es false,
            // ciertos items simplemente no se dibujan
            ui.checkbox(&mut self.show_completed, "Show completed");

            // Lista — puede cambiar de tamaño cada frame
            for (i, item) in self.items.iter().enumerate() {
                let is_selected = self.selected == Some(i);
                if ui.selectable_label(is_selected, item).clicked() {
                    self.selected = Some(i);
                }
            }
        });
    }
}
```

Observá cómo **la UI es una función del estado**. No hay sincronización, no hay binding,
no hay callbacks. Cambiás `self.items` y la lista se actualiza automáticamente en el
siguiente frame porque el `for` loop itera sobre el estado actual.

---

## Tradeoffs y cuándo elegir cada modelo

### Ventajas de immediate mode

**1. Simplicidad conceptual**

No hay señales, slots, bindings, observadores, callbacks, property change notifications.
El código de UI es código normal: `if`, `for`, `match`, llamadas a funciones.

**2. UI condicional trivial**

```rust
// Mostrar un panel solo si el usuario es admin
if user.is_admin {
    ui.label("Admin panel");
    // ... widgets de administración
}
// En retained mode necesitarías: panel.set_visible(user.is_admin)
// y acordarte de actualizarlo cada vez que user.is_admin cambie
```

**3. UI dinámica trivial**

```rust
// Crear N botones donde N viene de los datos
for item in &self.items {
    if ui.button(&item.name).clicked() {
        self.selected = Some(item.id);
    }
}
// En retained mode: crear/destruir objetos Button al cambiar items
```

**4. No hay estado zombi**

Si dejás de llamar a `ui.button("X")`, el botón desaparece. No hay objetos huérfanos
que consuman memoria. No hay referencias colgantes a widgets destruidos.

**5. Debugging directo**

Podés poner un breakpoint en `update()` y ver exactamente qué se está dibujando y
por qué. No hay indirección a través de callbacks registrados en otro lugar.

### Desventajas de immediate mode

**1. CPU usage**

Recalcular toda la UI cada frame consume más CPU que actualizar solo lo que cambió.
Para UIs simples/medianas esto es insignificante. Para UIs con miles de widgets o
cálculos de layout pesados, puede importar.

**2. Accesibilidad limitada**

Los screen readers necesitan un **árbol semántico** persistente de la UI para navegar.
Immediate mode no tiene ese árbol. egui tiene soporte experimental via AccessKit, pero
es más limitado que lo que ofrecen frameworks retained mode.

**3. Texto complejo**

Selección de texto, cursor, undo/redo, bidi text, input methods (IME) — todo esto es
más difícil cuando no hay un objeto TextInput persistente que acumule estado entre
frames.

**4. Animaciones**

Las animaciones requieren estado temporal que transiciona entre valores. En retained mode,
el framework maneja esto. En immediate mode, egui lo resuelve con estado interno
por ID, pero es menos natural.

**5. Layout de un solo paso**

egui calcula el layout en un solo paso (top-to-bottom, left-to-right). Esto significa que
un widget no puede saber el tamaño de un widget que viene después. Retained mode frameworks
hacen múltiples pasadas de layout y resuelven constraints bidireccionales.

### Diagrama de decisión

```
  ¿Qué tipo de aplicación estás construyendo?
  │
  ├─ Herramienta interna / debug UI / prototipo rápido
  │  └─→ Immediate mode ✓  (simplicidad > perfección visual)
  │
  ├─ Aplicación de escritorio con UI moderada
  │  └─→ Ambos funcionan. Immediate mode si preferís la simplicidad.
  │
  ├─ Editor de texto / IDE / procesador de texto
  │  └─→ Retained mode ✓  (texto complejo, accesibilidad)
  │
  ├─ App con miles de widgets simultáneos
  │  └─→ Retained mode ✓  (virtual scrolling, actualización parcial)
  │
  ├─ Overlay de un juego / panel de debug
  │  └─→ Immediate mode ✓  (ya tenés un render loop, integración natural)
  │
  └─ App comercial con accesibilidad obligatoria
     └─→ Retained mode ✓  (WCAG compliance, screen readers)
```

---

## Historia y adopción

El concepto de immediate mode GUI fue popularizado por **Casey Muratori** en 2005 con su
charla y artículo "Immediate-Mode Graphical User Interfaces" (IMGUI). La idea existía
antes, pero Casey le dio nombre y estructura.

### Línea temporal

```
2005  Casey Muratori publica "IMGUI" → formaliza el concepto
2007  Dear ImGui (Omar Cornut) → primer framework IMGUI popular
      Originalmente para herramientas de videojuegos (C++)
2014  Dear ImGui se vuelve estándar de facto para debug UIs en gamedev
2018  egui (Emil Ernerfeldt) → immediate mode GUI para Rust
2020  egui madura, soporte web (WASM) y nativo
2024  egui se vuelve el framework GUI puro-Rust más popular
```

### Frameworks immediate mode notables

| Framework  | Lenguaje | Uso principal                    |
|-----------|---------|----------------------------------|
| Dear ImGui | C++     | Debug UIs, herramientas de juegos |
| egui       | Rust    | Aplicaciones nativas y web        |
| Nuklear    | C       | Minimalista, embebido             |
| microui    | C       | Ultra-minimalista (~1000 LOC)     |
| Gio        | Go      | Aplicaciones móviles y desktop    |

Dear ImGui es, con diferencia, el más usado — prácticamente todo estudio de videojuegos
lo usa para herramientas internas. egui lleva el concepto al ecosistema Rust con una API
más idiomática.

---

## Immediate mode en Rust: egui

egui (pronunciado "e-gooey") es un framework immediate mode GUI escrito en puro Rust.
Es el framework que usaremos a lo largo de este bloque.

### Por qué egui para este curso

```
  ┌─────────────────────────────────────────────────────┐
  │  egui: características clave                         │
  ├─────────────────────────────────────────────────────┤
  │                                                      │
  │  • Puro Rust — sin bindings a C/C++                  │
  │  • API simple, idiomática, bien documentada          │
  │  • Immediate mode — código directo, sin callbacks    │
  │  • Corre nativo (OpenGL/wgpu) y en web (WASM)       │
  │  • No requiere sistema de build externo              │
  │  • Ecosistema activo (eframe, egui_extras, etc.)    │
  │  • Buena integración con el sistema de tipos Rust    │
  │                                                      │
  └─────────────────────────────────────────────────────┘
```

### Arquitectura de egui

```
  Tu aplicación
       │
       │ impl eframe::App
       ▼
  ┌──────────┐     ┌──────────┐     ┌──────────────┐
  │  eframe   │────▶│   egui   │────▶│  Tessellator  │
  │ (framework│     │ (library) │     │ (shapes →     │
  │  wrapper) │     │           │     │  triangles)   │
  └──────────┘     └──────────┘     └──────┬───────┘
       │                                     │
       │ maneja ventana,                     │ triángulos
       │ eventos OS,                         │ + texturas
       │ render loop                         ▼
       │                            ┌──────────────┐
       │                            │   Backend     │
       │                            │  (glow/wgpu   │
       └───────────────────────────▶│   o WASM)     │
                                    └──────────────┘
```

- **egui**: La librería core. Genera shapes (rectángulos, texto, líneas). No sabe nada
  sobre ventanas ni GPU.
- **eframe**: El framework que envuelve egui. Crea la ventana, maneja eventos del OS,
  ejecuta el render loop, y envía los shapes de egui al backend de renderizado.
- **Backend**: glow (OpenGL) o wgpu para nativo; WebGL/WebGPU para WASM.

### Un vistazo al código real

```rust
// Cargo.toml
// [dependencies]
// eframe = "0.31"

use eframe::egui;

struct CounterApp {
    count: i32,
}

impl Default for CounterApp {
    fn default() -> Self {
        Self { count: 0 }
    }
}

impl eframe::App for CounterApp {
    fn update(&mut self, ctx: &egui::Context, _frame: &mut eframe::Frame) {
        egui::CentralPanel::default().show(ctx, |ui| {
            ui.heading("Counter");
            ui.label(format!("Count: {}", self.count));

            ui.horizontal(|ui| {
                if ui.button("-1").clicked() {
                    self.count -= 1;
                }
                if ui.button("+1").clicked() {
                    self.count += 1;
                }
            });
        });
    }
}

fn main() -> eframe::Result {
    let options = eframe::NativeOptions::default();
    eframe::run_native(
        "Counter",
        options,
        Box::new(|_cc| Ok(Box::new(CounterApp::default()))),
    )
}
```

Este código produce una ventana con un heading, un contador, y dos botones. Todo en
~35 líneas. Sin callbacks, sin señales, sin bindings. Solo una función `update()` que
se ejecuta cada frame y describe la interfaz.

---

## Errores comunes

### 1. Pensar que immediate mode redibuja todo la pantalla a lo bruto

egui (y Dear ImGui) optimizan internamente. Solo solicitan repaint cuando hay input o
cambios. No consumen 100% CPU en idle. La API es immediate mode, pero el rendering es
inteligente.

### 2. Intentar almacenar referencias a widgets

```rust
// MAL: los widgets no son objetos persistentes
let my_button = ui.button("Click");  // my_button es un Response, no un widget
// No podés "guardar" el botón para usarlo después.
// En el siguiente frame, tenés que llamar ui.button() de nuevo.
```

El `Response` que retorna `ui.button()` solo es válido durante el frame actual. En el
siguiente frame hay que volver a llamar a `ui.button()`.

### 3. Confundir estado de la UI con estado del modelo

No pongas lógica de negocio dentro de los condicionales de UI:

```rust
// MAL: cálculo pesado dentro del if que chequea click
if ui.button("Calculate").clicked() {
    // Esto se ejecuta UNA VEZ al clickear, está bien.
    // Pero no pongas aquí algo que necesite ejecutarse cada frame.
    self.result = expensive_computation();
}

// MAL: cálculo pesado en cada frame sin necesidad
ui.label(format!("{}", expensive_computation())); // Se llama 60 veces/seg!

// BIEN: calcular una vez, mostrar siempre
ui.label(format!("{}", self.cached_result));
```

### 4. Asumir que el orden del código no importa

En immediate mode, el orden en que llamás a los widgets determina su posición en
pantalla. Si movés una línea de código, movés el widget.

```rust
// El label aparece ARRIBA del botón
ui.label("Status: OK");
ui.button("Action");

// El label aparece ABAJO del botón
ui.button("Action");
ui.label("Status: OK");
```

### 5. No entender que immediate mode no es "lento"

Para una aplicación con decenas o cientos de widgets, immediate mode es perfectamente
rápido. El mito de que "recalcular todo cada frame es lento" viene de confundir
immediate mode GUI con redibujar cada píxel. El framework tessella shapes eficientemente,
usa caching de texturas, y solo repinta cuando es necesario.

---

## Cheatsheet

```
IMMEDIATE MODE GUI — REFERENCIA RÁPIDA
=======================================

Concepto central:
  La UI se describe completamente en cada frame, no se construye una vez.
  Los widgets no son objetos — son llamadas a funciones.
  El estado lo mantenés vos en un struct.

Retained vs Immediate:
  Retained:  crear objetos → conectar callbacks → framework gestiona
  Immediate: cada frame → describir UI → framework dibuja y descarta

Flujo de un frame:
  input → update() → layout → render → present → repetir

Patrón básico egui:
  struct App { state... }
  impl eframe::App for App {
      fn update(&mut self, ctx, _frame) {
          CentralPanel::default().show(ctx, |ui| {
              // describir UI aquí
          });
      }
  }

Eventos = return values:
  if ui.button("X").clicked() { ... }    // click
  let r = ui.text_edit(&mut s);          // edición
  r.changed()                            // ¿cambió?
  r.hovered()                            // ¿mouse encima?
  r.lost_focus()                         // ¿perdió foco?

Estado:
  Tu struct MyApp    → estado de aplicación (datos, configuración)
  egui interno (IDs) → estado de UI (foco, scroll, animaciones)

Arquitectura egui:
  egui (core)  → genera shapes
  eframe       → maneja ventana + render loop
  backend      → glow/wgpu (nativo) o WebGL (WASM)

Cuándo usar immediate mode:
  ✓ Herramientas, prototipos, debug UIs, apps medianas
  ✗ Apps con accesibilidad obligatoria, editores de texto complejos
```

---

## Ejercicios

### Ejercicio 1: Análisis de paradigma

Dado el siguiente pseudocódigo retained mode:

```
fn setup() {
    let slider = Slider::new(0, 100);
    let label = Label::new("Value: 0");
    let progress = ProgressBar::new(0.0);

    slider.on_change(|value| {
        label.set_text(format!("Value: {}", value));
        progress.set_value(value as f64 / 100.0);
    });

    window.add(slider);
    window.add(label);
    window.add(progress);
}
```

Reescribilo en pseudocódigo immediate mode. Tu versión debe tener un struct con el
estado, y una función `update()` que use `ui.slider()`, `ui.label()` y
`ui.progress_bar()`.

> **Pregunta de predicción**: ¿Cuántas líneas necesita la versión immediate mode
> comparada con la retained mode? ¿Cuál de los dos problemas de sincronización
> (widget→modelo y modelo→widget) desaparece completamente en immediate mode, y por qué?

### Ejercicio 2: Diagrama de flujo

Dibujá (en papel o ASCII) el diagrama de flujo de lo que ocurre en **3 frames
consecutivos** cuando el usuario clickea un botón "+1" que incrementa un contador:

- Frame N: el mouse se mueve sobre el botón (hover)
- Frame N+1: el usuario hace click (press + release)
- Frame N+2: frame posterior al click

Para cada frame, indicá: (a) qué input recibe el framework, (b) qué retorna
`ui.button("+1")`, (c) qué valor tiene `self.count`, (d) qué se dibuja.

> **Pregunta de predicción**: En el Frame N+1, ¿el label muestra el valor viejo o el
> nuevo? Pista: pensá en qué momento del frame se ejecuta `self.count += 1` relativo
> a `ui.label(format!("Count: {}", self.count))`. ¿Depende del orden del código?

### Ejercicio 3: Tradeoff analysis

Un cliente te pide tres aplicaciones distintas. Para cada una, elegí retained mode o
immediate mode y justificá con al menos dos razones técnicas:

1. **Un panel de monitoreo** que muestra métricas de servidores en tiempo real (CPU,
   memoria, red) con gráficos que se actualizan cada segundo. Usuarios: equipo de
   DevOps interno.

2. **Un formulario de alta de pacientes** para un hospital. Debe cumplir normas de
   accesibilidad (WCAG 2.1 AA). Usuarios: personal administrativo, algunos con
   discapacidades visuales.

3. **Un editor de niveles** para un juego indie en desarrollo. Permite colocar tiles,
   enemigos, triggers. Solo lo usan los desarrolladores del juego.

> **Pregunta de predicción**: Si el panel de monitoreo necesitara funcionar también en
> el navegador (no solo nativo), ¿cambiaría tu elección? ¿Por qué egui podría ser una
> buena opción en ese caso específico?
