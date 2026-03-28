# Bloque 12: GUI con Rust (egui) — Fases 1, 2 y 3

**Objetivo**: Desarrollar aplicaciones gráficas en Rust usando egui, desde formularios
simples hasta aplicaciones de dibujo e imágenes.

**Nota**: Este bloque se ejecuta en el host, no en contenedores Docker (requiere
acceso al display gráfico).

**Orden progresivo de capítulos**: Los capítulos teóricos preceden estrictamente a los
proyectos que los requieren. La secuencia obligatoria es:
1. Introducción a egui (C01) → base teórica
2. Calculadoras (C02) + Reloj (C03) → Fase 1, solo necesitan widgets básicos
3. Estado y persistencia (C04) → teoría de estado y archivos
4. Editor, Todo, Tablas (C05-C07) → Fase 2, necesitan estado + archivos
5. Painter y gráficos (C08) → teoría de dibujo 2D
6. Dibujo libre y Apps de imágenes (C09-C10) → Fase 3, necesitan Painter

## Capítulo 1: Introducción a egui y eframe [A]

### Sección 1: Fundamentos
- **T01 - Immediate mode GUI**: qué es, diferencia con retained mode, tradeoffs
- **T02 - eframe como framework**: App trait, setup, update loop, egui::Context
- **T03 - Proyecto mínimo**: ventana con texto, Cargo.toml con dependencias, compilar y ejecutar
- **T04 - El render loop**: RequestRepaint, cuándo se redibuja, frame rate
- **T05 - egui vs alternativas**: comparación breve con iced (Elm-like), gtk-rs (bindings GTK), Slint (declarativo), Tauri (web-based) — por qué egui para este curso (simplicidad, immediate mode, puro Rust, buena documentación)

### Sección 2: Widgets Básicos
- **T01 - Labels y botones**: ui.label(), ui.button(), respuesta a clicks
- **T02 - Text input**: TextEdit, singleline vs multiline, binding a String
- **T03 - Sliders y checkboxes**: ui.add(Slider), ui.checkbox, binding a estado
- **T04 - ComboBox y selección**: dropdown, enum como opciones

### Sección 3: Layout
- **T01 - Horizontal y vertical**: ui.horizontal(), ui.vertical(), anidamiento
- **T02 - Panels**: SidePanel, TopBottomPanel, CentralPanel
- **T03 - ScrollArea**: contenido largo, scroll vertical/horizontal
- **T04 - Grid y columns**: distribución tabular, alineación

### Sección 4: Temas y Accesibilidad
- **T01 - Dark/light mode**: Visuals, ctx.set_visuals(), detección de preferencia del OS
- **T02 - Escalado y DPI**: ctx.set_pixels_per_point(), adaptación a pantallas HiDPI
- **T03 - Limitaciones de accesibilidad**: por qué immediate mode GUI es difícil para screen readers, AccessKit (soporte experimental en egui)

## Capítulo 2: Proyecto — Calculadoras [P] (Fase 1)

### Sección 1: Calculadora Simple
- **T01 - Base**: dos campos de texto, un botón, resultado
- **T02 - Mejora 1**: resultado en tiempo real (sin botón)
- **T03 - Mejora 2**: menú desplegable para elegir operación
- **T04 - Mejora 3**: historial de operaciones en lista

### Sección 2: Calculadora Regular
- **T01 - Base**: teclado numérico en pantalla, display
- **T02 - Mejora 1**: respuesta al teclado físico (egui input)
- **T03 - Mejora 2**: historial de últimas 10 operaciones

### Sección 3: Calculadora Científica
- **T01 - Base**: sin, cos, tan, √, log, ^ — con crate meval para evaluación
- **T02 - Evaluador propio**: implementar precedencia de operadores (shunting-yard)
- **T03 - Mejora 1**: historial con expresiones completas
- **T04 - Mejora 2**: conversor a notación científica

## Capítulo 3: Proyecto — Reloj y Temporizadores [P] (Fase 1)

### Sección 1: Reloj Digital
- **T01 - Base**: hora actual, actualización cada segundo (request_repaint_after)
- **T02 - Mejora 1**: fecha debajo
- **T03 - Mejora 2**: reloj analógico con Painter (trigonometría, líneas, arcos)

### Sección 2: Cronómetro
- **T01 - Base**: Start/Stop/Reset, display 00:00.000, Instant para medición precisa
- **T02 - Mejora 1**: botón Lap, lista de tiempos parciales
- **T03 - Mejora 2**: cuenta regresiva, input de tiempo, alerta visual al llegar a 0

## Capítulo 4: Estado, Persistencia y Archivos [A]

### Sección 1: Gestión de Estado
- **T01 - Estado en struct App**: campos mutables, separación modelo-vista
- **T02 - IDs en egui**: por qué importan, conflictos, push_id
- **T03 - Diálogos de archivo**: rfd crate (native file dialog), abrir/guardar

### Sección 2: Serialización
- **T01 - serde**: Serialize, Deserialize, #[derive], JSON con serde_json
- **T02 - Persistencia**: guardar estado al cerrar, cargar al abrir, eframe::Storage
- **T03 - Formatos**: JSON, TOML, RON — cuándo usar cada uno

### Sección 3: Atajos de Teclado
- **T01 - ctx.input()**: detectar teclas presionadas, Key enum, Modifiers
- **T02 - Combinaciones**: Ctrl+S, Ctrl+Z, Ctrl+Shift+S — pattern matching de modifiers + key
- **T03 - Conflictos con TextEdit**: cuándo un TextEdit consume el input vs cuándo pasa al handler global
- **T04 - Mapa de atajos configurable**: HashMap<KeyCombo, Action>, extensibilidad, patrón Command

## Capítulo 5: Proyecto — Editores de Texto [P] (Fase 2)

### Sección 1: Editor Básico
- **T01 - Base**: TextEdit multiline, sin guardar
- **T02 - Mejora 1**: selección y copiar al portapapeles (ctx.output_mut)
- **T03 - Mejora 2**: guardar y abrir archivos (rfd + std::fs)
- **T04 - Mejora 3**: cambiar fuente y tamaño (FontId, FontDefinitions)
- **T05 - Mejora 4**: buscar texto (Ctrl+F), resaltar ocurrencias

### Sección 2: Editor de Markdown
- **T01 - Base**: panel split, pulldown-cmark para parsear Markdown
- **T02 - Renderizado en egui**: convertir eventos cmark a widgets egui
- **T03 - Mejora 1**: botones de formato (insertar sintaxis Markdown)
- **T04 - Mejora 2**: exportar a HTML

## Capítulo 6: Proyecto — Lista de Tareas [P] (Fase 2)

### Sección 1: Todo List GUI
- **T01 - Base**: checkbox, campo de texto, botón delete por ítem
- **T02 - Mejora 1**: filtros (todas, pendientes, completadas)
- **T03 - Mejora 2**: drag and drop para reordenar
- **T04 - Mejora 3**: guardar en JSON al cerrar, cargar al abrir
- **T05 - Mejora 4**: múltiples listas con tabs

## Capítulo 7: Proyecto — Gestión Tabular [P] (Fase 2)

### Sección 1: Tabla Editable
- **T01 - Base**: filas/columnas, click para editar celda, ancho fijo
- **T02 - Mejora 1**: añadir y eliminar filas
- **T03 - Mejora 2**: ordenar por columna al click en header
- **T04 - Mejora 3**: exportar a CSV
- **T05 - Mejora 4**: importar desde CSV

### Sección 2: Editor Tipo Excel
- **T01 - Base**: parser de fórmulas (=SUMA(A1:A3)), referencias de celda
- **T02 - Evaluador recursivo**: resolución de dependencias, detección de ciclos
- **T03 - Mejora 1**: redimensionar columnas/filas arrastrando
- **T04 - Mejora 2**: formato por celda (negrita, color, alineación)

## Capítulo 8: Painter y Gráficos 2D [A]

### Sección 1: API de Painter
- **T01 - Painter**: egui::Painter, dibujar líneas, rectángulos, círculos, texto
- **T02 - Colores**: Color32, Rgba, conversiones RGB↔HSL
- **T03 - Shapes y Stroke**: grosor, relleno, anti-aliasing
- **T04 - Transformaciones**: translate, rotate (manejo manual con trigonometría)

### Sección 2: Interacción con Canvas
- **T01 - Sense y Response**: detectar hover, click, drag en regiones
- **T02 - Coordenadas**: screen space vs canvas space, conversiones
- **T03 - Texturas**: cargar imágenes como texturas, TextureHandle, performance

## Capítulo 9: Proyecto — Dibujo Libre [P] (Fase 3)

### Sección 1: Lienzo de Dibujo
- **T01 - Base**: dibujar con el mouse, almacenar puntos/strokes
- **T02 - Mejora 1**: selector de color (sliders RGB)
- **T03 - Mejora 2**: selector de tamaño del pincel
- **T04 - Mejora 3**: herramienta de borrar
- **T05 - Mejora 4**: guardar como PNG (image crate)

### Sección 2: Herramientas Geométricas
- **T01 - Línea**: click inicio, drag hasta fin, preview en tiempo real
- **T02 - Rectángulo**: click+drag para definir esquinas
- **T03 - Círculo**: click+drag define el radio
- **T04 - Mejora 1**: relleno sólido vs solo borde
- **T05 - Mejora 2**: Ctrl+Z con historial de operaciones (Command pattern)

## Capítulo 10: Proyecto — Apps de Imágenes [P] (Fase 3)

### Sección 1: Visor de Imágenes
- **T01 - Base**: cargar imagen (image crate), ajustar a ventana
- **T02 - Mejora 1**: zoom con scroll (mantener aspect ratio, centrar)
- **T03 - Mejora 2**: pan con drag
- **T04 - Mejora 3**: seleccionar región rectangular, exportar como PNG
- **T05 - Mejora 4**: texto posicionable sobre la imagen
- **T06 - Mejora 5**: dibujar rectángulos y flechas sobre la imagen

### Sección 2: Navegador de Imágenes
- **T01 - Base**: abrir carpeta, navegar con ←/→, nombre y dimensiones en barra inferior
- **T02 - Mejora 1**: miniaturas en panel lateral
- **T03 - Mejora 2**: búsqueda por nombre

### Sección 3: Color Picker
- **T01 - Base**: click en imagen, mostrar RGB, HSL, HEX
- **T02 - Mejora 1**: copiar HEX al portapapeles
- **T03 - Mejora 2**: paleta de últimos 16 colores
- **T04 - Mejora 3**: cuentagotas fuera de la app (screenshots crate)
