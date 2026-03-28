# Mejora 1: historial de mensajes con scroll

## Índice

1. [El problema de la terminal raw](#el-problema-de-la-terminal-raw)
2. [Introducción a crossterm](#introducción-a-crossterm)
3. [Modo raw y alternate screen](#modo-raw-y-alternate-screen)
4. [Layout de la TUI](#layout-de-la-tui)
5. [El buffer de historial](#el-buffer-de-historial)
6. [Renderizado del área de mensajes](#renderizado-del-área-de-mensajes)
7. [Input line editable](#input-line-editable)
8. [Scroll con teclado](#scroll-con-teclado)
9. [Integración con el cliente de chat](#integración-con-el-cliente-de-chat)
10. [El cliente TUI completo](#el-cliente-tui-completo)
11. [Errores comunes](#errores-comunes)
12. [Cheatsheet](#cheatsheet)
13. [Ejercicios](#ejercicios)

---

## El problema de la terminal raw

En T01 construimos un cliente funcional, pero tiene un problema visible: cuando
llega un mensaje del servidor mientras el usuario está escribiendo, el texto se
mezcla:

```
[Alice] Hey, how is it going?
I'm writing a messa[Bob] Just joined!
ge and it got split
```

Esto ocurre porque `print!` y `stdin` comparten la misma terminal "cooked":
el output del servidor se imprime en la posición actual del cursor, que puede
estar en medio de lo que el usuario escribe.

```
┌──────────────────────────────────────────────────────────────┐
│  Terminal cooked (T01):                                      │
│                                                              │
│  - stdout y stdin comparten la misma pantalla               │
│  - El cursor se mueve con cada print! y cada keystroke      │
│  - No hay separación visual entre mensajes y input          │
│  - No hay scroll: la terminal scrollea sola cuando se llena │
│  - No hay historial: lo que salió de la pantalla, se perdió │
│                                                              │
│  Terminal raw (T02):                                         │
│                                                              │
│  - Control total del cursor y la pantalla                    │
│  - Separamos el área de mensajes del área de input          │
│  - Mantenemos un buffer en memoria para scroll              │
│  - Cada keystroke llega inmediato (sin esperar Enter)       │
│  - Nosotros manejamos echo, backspace, historial            │
└──────────────────────────────────────────────────────────────┘
```

La solución es tomar **control total de la terminal**: entrar en modo raw,
manejar la pantalla nosotros y separar visualmente las dos zonas.

---

## Introducción a crossterm

[crossterm](https://crates.io/crates/crossterm) es un crate multiplataforma
para control de terminal. A diferencia de `termion` (solo Unix), crossterm
funciona en Linux, macOS y Windows.

```toml
# Cargo.toml
[dependencies]
crossterm = "0.28"
tokio = { version = "1", features = ["full"] }
```

### Conceptos fundamentales

crossterm trabaja con **comandos** que se ejecutan sobre un writer (usualmente
stdout):

```rust
use std::io::{stdout, Write};
use crossterm::{
    cursor,
    execute,
    style::{self, Stylize},
    terminal,
};

fn main() -> std::io::Result<()> {
    let mut out = stdout();

    // execute! envía comandos directamente al terminal
    execute!(
        out,
        cursor::MoveTo(0, 5),              // Mover cursor a columna 0, fila 5
        style::Print("Hello!".green()),     // Imprimir texto verde
        cursor::MoveTo(0, 6),
        style::Print("World!".bold()),
    )?;

    Ok(())
}
```

### queue! vs execute!

```rust
use crossterm::queue;

// queue! almacena los comandos en un buffer interno (no los envía aún)
queue!(out, cursor::MoveTo(0, 0), style::Print("buffered"))?;
queue!(out, cursor::MoveTo(0, 1), style::Print("also buffered"))?;

// flush! envía todo de una vez (más eficiente que múltiples execute!)
out.flush()?;

// execute! = queue! + flush! automático
// Usar queue! cuando se envían muchos comandos seguidos
// Usar execute! cuando se envía un comando aislado
```

### Leer eventos del teclado

```rust
use crossterm::event::{self, Event, KeyCode, KeyModifiers, KeyEvent};

// Bloquea hasta que hay un evento
match event::read()? {
    Event::Key(KeyEvent { code, modifiers, .. }) => {
        match code {
            KeyCode::Char(c) => println!("Typed: {}", c),
            KeyCode::Enter => println!("Enter pressed"),
            KeyCode::Backspace => println!("Backspace"),
            KeyCode::Up => println!("Arrow up"),
            KeyCode::Down => println!("Arrow down"),
            _ => {}
        }
        // Detectar Ctrl+C
        if code == KeyCode::Char('c') && modifiers.contains(KeyModifiers::CONTROL) {
            println!("Ctrl+C!");
        }
    }
    Event::Resize(cols, rows) => println!("Terminal resized: {}x{}", cols, rows),
    _ => {}
}
```

---

## Modo raw y alternate screen

### Modo raw

En modo normal ("cooked"), la terminal interpreta ciertas teclas: Enter envía
la línea, Ctrl+C mata el proceso, backspace borra un carácter. En modo **raw**,
cada keystroke llega como evento sin interpretación.

```rust
use crossterm::terminal::{enable_raw_mode, disable_raw_mode};

fn main() -> std::io::Result<()> {
    // Entrar en modo raw
    enable_raw_mode()?;

    // Aquí cada tecla es un evento individual
    // Ctrl+C ya NO mata el proceso — debemos manejarlo nosotros
    // Echo desactivado — debemos imprimir nosotros lo que se teclea

    // CRÍTICO: siempre restaurar al salir
    disable_raw_mode()?;
    Ok(())
}
```

### Alternate screen

La "alternate screen" es una pantalla secundaria que la terminal ofrece. Al
entrar, se preserva lo que el usuario tenía en la terminal. Al salir, todo
vuelve como estaba (como hace `vim`, `less`, `htop`).

```rust
use crossterm::terminal::{EnterAlternateScreen, LeaveAlternateScreen};
use crossterm::execute;
use std::io::stdout;

fn main() -> std::io::Result<()> {
    let mut out = stdout();

    execute!(out, EnterAlternateScreen)?;
    // Aquí dibujamos nuestra TUI en una pantalla limpia
    // ...
    execute!(out, LeaveAlternateScreen)?;
    // El terminal vuelve a su estado anterior

    Ok(())
}
```

### Guard pattern para limpieza segura

Si el programa hace panic o sale por error, debemos restaurar la terminal.
Sin esto, la terminal del usuario queda en modo raw (inutilizable):

```rust
struct TerminalGuard;

impl TerminalGuard {
    fn new() -> std::io::Result<Self> {
        enable_raw_mode()?;
        execute!(stdout(), EnterAlternateScreen, cursor::Hide)?;
        Ok(Self)
    }
}

impl Drop for TerminalGuard {
    fn drop(&mut self) {
        // Restaurar terminal pase lo que pase
        let _ = execute!(stdout(), cursor::Show, LeaveAlternateScreen);
        let _ = disable_raw_mode();
    }
}

fn main() -> std::io::Result<()> {
    let _guard = TerminalGuard::new()?;

    // Si hay un panic aquí, Drop se ejecuta y restaura la terminal
    do_tui_stuff()?;

    Ok(())
    // _guard se dropea aquí → terminal restaurada
}
```

Para capturar panics explícitamente:

```rust
fn setup_panic_hook() {
    let original_hook = std::panic::take_hook();
    std::panic::set_hook(Box::new(move |info| {
        // Restaurar terminal antes de mostrar el panic
        let _ = execute!(stdout(), cursor::Show, LeaveAlternateScreen);
        let _ = disable_raw_mode();
        original_hook(info);
    }));
}
```

---

## Layout de la TUI

Nuestro chat TUI tendrá una estructura simple:

```
┌──────────────────────────────────────────────────────────────┐
│  === Chat Room ===                          [↑↓ to scroll]  │  <- Fila 0: barra de título
├──────────────────────────────────────────────────────────────┤
│  [10:01] Alice: Hey everyone!                               │  <- Fila 1
│  [10:01] *** Bob has joined ***                             │  │
│  [10:02] Bob: Hi Alice!                                     │  │ Área de
│  [10:03] Alice: How are you?                                │  │ mensajes
│  [10:03] Bob: Great, thanks! Working on the project.        │  │ (scrollable)
│  [10:04] *** Carol has joined ***                           │  │
│  [10:04] Carol: Hello hello!                                │  │
│                                                              │  <- filas vacías
│                                                              │  │
│                                                              │  <- Fila rows-3
├──────────────────────────────────────────────────────────────┤
│  ─────────────────────────────────────────────────────────── │  <- Fila rows-2: separador
│  > I'm typing a message here█                               │  <- Fila rows-1: input
└──────────────────────────────────────────────────────────────┘

  rows = terminal height (ej: 24)
  Área de mensajes: filas 1 a rows-3 → (rows-3) líneas visibles
  Separador:        fila rows-2
  Input:            fila rows-1
```

### Calcular dimensiones

```rust
fn layout() -> (u16, u16, u16) {
    let (cols, rows) = terminal::size().unwrap_or((80, 24));

    // Área de mensajes: desde fila 1 hasta fila rows-3
    let message_area_height = rows.saturating_sub(3); // rows - 3 líneas visibles

    (cols, rows, message_area_height)
}
```

---

## El buffer de historial

El corazón de esta mejora es un buffer en memoria que almacena todos los
mensajes recibidos. Cuando el buffer tiene más mensajes que las líneas visibles,
aparece el scroll.

```rust
use std::collections::VecDeque;

struct MessageHistory {
    messages: VecDeque<String>,
    max_messages: usize,
    scroll_offset: usize, // 0 = fondo (mensajes más recientes)
}

impl MessageHistory {
    fn new(max_messages: usize) -> Self {
        Self {
            messages: VecDeque::with_capacity(max_messages),
            max_messages,
            scroll_offset: 0,
        }
    }

    fn push(&mut self, message: String) {
        if self.messages.len() >= self.max_messages {
            self.messages.pop_front(); // Eliminar el más antiguo
        }
        self.messages.push_back(message);

        // Si estábamos al fondo, mantenernos al fondo
        // Si estábamos scrolleados arriba, no mover
        // (el usuario está leyendo historial)
    }

    fn total(&self) -> usize {
        self.messages.len()
    }

    /// Retorna los mensajes visibles dado un viewport_height
    fn visible_messages(&self, viewport_height: usize) -> &[String] {
        // VecDeque::make_contiguous() no es necesario aquí
        // porque usamos slicing lógico con índices

        let total = self.messages.len();
        if total == 0 {
            return &[];
        }

        // El scroll_offset cuenta desde el fondo:
        //   0 = mostrando los mensajes más recientes
        //   n = mostrando n mensajes más arriba

        let end = total.saturating_sub(self.scroll_offset);
        let start = end.saturating_sub(viewport_height);

        // make_contiguous para poder retornar un slice
        // (VecDeque internamente puede tener dos segmentos)
        let slice = self.messages.make_contiguous();
        &slice[start..end]
    }

    fn scroll_up(&mut self, lines: usize, viewport_height: usize) {
        let max_offset = self.messages.len().saturating_sub(viewport_height);
        self.scroll_offset = (self.scroll_offset + lines).min(max_offset);
    }

    fn scroll_down(&mut self, lines: usize) {
        self.scroll_offset = self.scroll_offset.saturating_sub(lines);
    }

    fn scroll_to_bottom(&mut self) {
        self.scroll_offset = 0;
    }

    fn is_at_bottom(&self) -> bool {
        self.scroll_offset == 0
    }
}
```

### Por qué VecDeque

```
┌──────────────────────────────────────────────────────────────┐
│  Vec<String>:                                                │
│  - push al final: O(1)                                      │
│  - remove del inicio (para limitar tamaño): O(n)            │
│  - Para 10,000 mensajes, mover todo en cada push es caro    │
│                                                              │
│  VecDeque<String>:                                           │
│  - push_back: O(1)                                          │
│  - pop_front: O(1)  ← ring buffer, no mueve nada            │
│  - Ideal para buffer circular de tamaño fijo                │
│                                                              │
│  Internamente VecDeque es un ring buffer:                    │
│                                                              │
│  [  5  |  6  |  7  |  _  |  _  |  3  |  4  ]               │
│                          tail          head                  │
│  Lectura lógica: 3, 4, 5, 6, 7                              │
│  push_back → escribe en tail, avanza tail                    │
│  pop_front → avanza head                                     │
│  Ninguna operación mueve datos existentes                   │
└──────────────────────────────────────────────────────────────┘
```

### Scroll offset desde el fondo

El `scroll_offset` cuenta desde el fondo hacia arriba. Esto es natural para un
chat donde lo más reciente está abajo:

```
Mensajes en el buffer: [m0, m1, m2, m3, m4, m5, m6, m7, m8, m9]
Viewport: 4 líneas visibles

scroll_offset = 0 (al fondo):
  m6
  m7
  m8
  m9  ← más reciente

scroll_offset = 3:
  m3
  m4
  m5
  m6

scroll_offset = 6 (máximo, tope del historial):
  m0
  m1
  m2
  m3
```

---

## Renderizado del área de mensajes

Con el buffer listo, necesitamos dibujar los mensajes en la pantalla:

```rust
use crossterm::{cursor, execute, queue, style, terminal};
use std::io::{stdout, Write};

fn render_messages(
    out: &mut impl Write,
    history: &mut MessageHistory,
    viewport_height: u16,
    cols: u16,
) -> std::io::Result<()> {
    let visible = history.visible_messages(viewport_height as usize);

    for (i, msg) in visible.iter().enumerate() {
        let row = 1 + i as u16; // Fila 1 en adelante (fila 0 es el título)

        queue!(
            out,
            cursor::MoveTo(0, row),
            terminal::Clear(terminal::ClearType::CurrentLine),
        )?;

        // Truncar si el mensaje es más largo que la terminal
        let display: String = msg.chars().take(cols as usize).collect();
        queue!(out, style::Print(&display))?;
    }

    // Limpiar filas sobrantes (si hay menos mensajes que viewport)
    let visible_count = visible.len() as u16;
    for row in (1 + visible_count)..=(viewport_height) {
        queue!(
            out,
            cursor::MoveTo(0, row),
            terminal::Clear(terminal::ClearType::CurrentLine),
        )?;
    }

    Ok(())
}
```

### Barra de título y separador

```rust
fn render_title_bar(out: &mut impl Write, cols: u16) -> std::io::Result<()> {
    let title = " === Chat Room === ";
    let scroll_hint = "[↑↓ scroll] ";

    queue!(
        out,
        cursor::MoveTo(0, 0),
        terminal::Clear(terminal::ClearType::CurrentLine),
        style::Print(title),
    )?;

    // Hint de scroll alineado a la derecha
    let hint_col = cols.saturating_sub(scroll_hint.len() as u16);
    queue!(
        out,
        cursor::MoveTo(hint_col, 0),
        style::Print(scroll_hint),
    )?;

    Ok(())
}

fn render_separator(out: &mut impl Write, cols: u16, rows: u16) -> std::io::Result<()> {
    let separator_row = rows - 2;
    let line: String = "─".repeat(cols as usize);

    queue!(
        out,
        cursor::MoveTo(0, separator_row),
        terminal::Clear(terminal::ClearType::CurrentLine),
        style::Print(line),
    )?;

    Ok(())
}
```

### Indicador de scroll

Cuando el usuario no está al fondo del historial, es útil indicarlo:

```rust
fn render_scroll_indicator(
    out: &mut impl Write,
    history: &MessageHistory,
    cols: u16,
    rows: u16,
) -> std::io::Result<()> {
    if !history.is_at_bottom() {
        let indicator = format!("── ↓ {} new ──", history.scroll_offset);
        let col = cols.saturating_sub(indicator.len() as u16) / 2;
        let separator_row = rows - 2;

        queue!(
            out,
            cursor::MoveTo(col, separator_row),
            style::Print(&indicator),
        )?;
    }

    Ok(())
}
```

---

## Input line editable

En modo raw, cada keystroke es un evento. Debemos manejar la edición
nosotros mismos:

```rust
struct InputLine {
    buffer: String,
    cursor_pos: usize, // Posición del cursor dentro del buffer
}

impl InputLine {
    fn new() -> Self {
        Self {
            buffer: String::new(),
            cursor_pos: 0,
        }
    }

    fn insert(&mut self, c: char) {
        self.buffer.insert(self.cursor_pos, c);
        self.cursor_pos += c.len_utf8();
    }

    fn backspace(&mut self) {
        if self.cursor_pos > 0 {
            // Encontrar el inicio del carácter anterior
            let prev = self.buffer[..self.cursor_pos]
                .char_indices()
                .last()
                .map(|(i, _)| i)
                .unwrap_or(0);
            self.buffer.remove(prev);
            self.cursor_pos = prev;
        }
    }

    fn delete(&mut self) {
        if self.cursor_pos < self.buffer.len() {
            self.buffer.remove(self.cursor_pos);
        }
    }

    fn move_left(&mut self) {
        if self.cursor_pos > 0 {
            self.cursor_pos = self.buffer[..self.cursor_pos]
                .char_indices()
                .last()
                .map(|(i, _)| i)
                .unwrap_or(0);
        }
    }

    fn move_right(&mut self) {
        if self.cursor_pos < self.buffer.len() {
            self.cursor_pos += self.buffer[self.cursor_pos..]
                .chars()
                .next()
                .map(|c| c.len_utf8())
                .unwrap_or(0);
        }
    }

    fn home(&mut self) {
        self.cursor_pos = 0;
    }

    fn end(&mut self) {
        self.cursor_pos = self.buffer.len();
    }

    /// Toma el contenido y resetea
    fn take(&mut self) -> String {
        let content = std::mem::take(&mut self.buffer);
        self.cursor_pos = 0;
        content
    }

    fn is_empty(&self) -> bool {
        self.buffer.is_empty()
    }
}
```

### Renderizar la línea de input

```rust
fn render_input(
    out: &mut impl Write,
    input: &InputLine,
    cols: u16,
    rows: u16,
) -> std::io::Result<()> {
    let input_row = rows - 1;
    let prompt = "> ";

    queue!(
        out,
        cursor::MoveTo(0, input_row),
        terminal::Clear(terminal::ClearType::CurrentLine),
        style::Print(prompt),
    )?;

    // Mostrar el buffer de input (truncado si excede el ancho)
    let available = (cols as usize).saturating_sub(prompt.len());
    let display: String = input.buffer.chars().take(available).collect();
    queue!(out, style::Print(&display))?;

    // Posicionar cursor donde corresponde
    let cursor_col = prompt.len() + input.buffer[..input.cursor_pos].chars().count();
    queue!(out, cursor::MoveTo(cursor_col as u16, input_row))?;

    Ok(())
}
```

---

## Scroll con teclado

Ahora conectamos los eventos de teclado con el historial:

```rust
use crossterm::event::{self, Event, KeyCode, KeyModifiers, KeyEvent};

fn handle_key_event(
    key: KeyEvent,
    input: &mut InputLine,
    history: &mut MessageHistory,
    viewport_height: u16,
) -> KeyAction {
    match key.code {
        // Enviar mensaje
        KeyCode::Enter => {
            if !input.is_empty() {
                let line = input.take();
                history.scroll_to_bottom(); // Volver al fondo al enviar
                return KeyAction::Send(line);
            }
            KeyAction::None
        }

        // Edición
        KeyCode::Char(c) => {
            // Ctrl+C para salir
            if c == 'c' && key.modifiers.contains(KeyModifiers::CONTROL) {
                return KeyAction::Quit;
            }
            input.insert(c);
            KeyAction::Redraw
        }
        KeyCode::Backspace => {
            input.backspace();
            KeyAction::Redraw
        }
        KeyCode::Delete => {
            input.delete();
            KeyAction::Redraw
        }
        KeyCode::Left => {
            input.move_left();
            KeyAction::Redraw
        }
        KeyCode::Right => {
            input.move_right();
            KeyAction::Redraw
        }
        KeyCode::Home => {
            input.home();
            KeyAction::Redraw
        }
        KeyCode::End => {
            input.end();
            KeyAction::Redraw
        }

        // Scroll
        KeyCode::PageUp => {
            history.scroll_up(viewport_height as usize / 2, viewport_height as usize);
            KeyAction::Redraw
        }
        KeyCode::PageDown => {
            history.scroll_down(viewport_height as usize / 2);
            KeyAction::Redraw
        }
        KeyCode::Up => {
            if key.modifiers.contains(KeyModifiers::SHIFT) {
                history.scroll_up(1, viewport_height as usize);
            } else {
                history.scroll_up(3, viewport_height as usize);
            }
            KeyAction::Redraw
        }
        KeyCode::Down => {
            if key.modifiers.contains(KeyModifiers::SHIFT) {
                history.scroll_down(1);
            } else {
                history.scroll_down(3);
            }
            KeyAction::Redraw
        }

        _ => KeyAction::None,
    }
}

enum KeyAction {
    None,
    Redraw,
    Send(String),
    Quit,
}
```

---

## Integración con el cliente de chat

Ahora debemos combinar la TUI con la conexión TCP del tema anterior. El reto
principal: `event::read()` de crossterm es bloqueante, igual que el stdin de
T01. Necesitamos leerlo sin bloquear al runtime async.

### event::poll para lectura no-bloqueante

crossterm ofrece `event::poll(timeout)` que retorna `true` si hay un evento
disponible, sin bloquear indefinidamente:

```rust
use crossterm::event;
use std::time::Duration;

// Poll con timeout de 0 = no-blocking check
if event::poll(Duration::from_millis(0))? {
    // Hay un evento disponible, leerlo no bloqueará
    let event = event::read()?;
    // procesar...
}
```

### Spawn de reader en un thread

La arquitectura es similar a T01 (spawn_stdin_reader) pero ahora leemos
eventos de crossterm en vez de líneas de stdin:

```rust
use tokio::sync::mpsc;
use crossterm::event::{self, Event, KeyEvent};

fn spawn_input_reader() -> mpsc::UnboundedReceiver<Event> {
    let (tx, rx) = mpsc::unbounded_channel();

    std::thread::spawn(move || {
        loop {
            // Esperar hasta 100ms por un evento
            match event::poll(Duration::from_millis(100)) {
                Ok(true) => {
                    if let Ok(evt) = event::read() {
                        if tx.send(evt).is_err() {
                            break; // Receiver dropped
                        }
                    }
                }
                Ok(false) => continue, // Timeout, revisar de nuevo
                Err(_) => break,
            }
        }
    });

    rx
}
```

### Diagrama del flujo completo

```
┌──────────────────────────────────────────────────────────────┐
│                 Arquitectura del Cliente TUI                  │
│                                                              │
│  ┌────────────────┐  Event    ┌──────────────────────────┐  │
│  │  Thread de     │──────────▶│                          │  │
│  │  crossterm     │  (mpsc)   │     Loop principal       │  │
│  │  event::read() │           │                          │  │
│  └────────────────┘           │  tokio::select! {        │  │
│                               │    event = input_rx ──┐  │  │
│  ┌────────────────┐  String   │    line = server_rx ┐ │  │  │
│  │  Servidor TCP  │──────────▶│  }                  │ │  │  │
│  │  BufReader     │  (async)  │                     │ │  │  │
│  └────────────────┘           │         ┌───────────┘ │  │  │
│                               │         ▼             ▼  │  │
│        ┌──────────────────────┤  ┌─────────────┐ ┌────┐  │  │
│        │                      │  │  history     │ │input│ │  │
│        │    stdout             │  │  .push()    │ │edit │ │  │
│        │    (render)           │  └──────┬──────┘ └──┬─┘  │  │
│        │                      │         │           │    │  │
│        │◀───────────────────  │  render_messages() + │    │  │
│        │                      │  render_input()      │    │  │
│        │                      │                      │    │  │
│        │                      │  Send(line) ─────────────▶│  │
│        │                      │     writer.write_all()    │  │
│        └──────────────────────┴──────────────────────────┘  │
└──────────────────────────────────────────────────────────────┘
```

---

## El cliente TUI completo

Aquí unimos todas las piezas:

```rust
use std::io::{stdout, Write};
use std::time::Duration;
use std::collections::VecDeque;
use crossterm::{
    cursor, event, execute, queue, style, terminal,
    event::{Event, KeyCode, KeyModifiers, KeyEvent},
    terminal::{
        enable_raw_mode, disable_raw_mode,
        EnterAlternateScreen, LeaveAlternateScreen,
    },
};
use tokio::io::{AsyncBufReadExt, AsyncWriteExt, BufReader};
use tokio::net::TcpStream;
use tokio::sync::mpsc;

// ---------- MessageHistory ----------

struct MessageHistory {
    messages: VecDeque<String>,
    max_messages: usize,
    scroll_offset: usize,
}

impl MessageHistory {
    fn new(max_messages: usize) -> Self {
        Self {
            messages: VecDeque::with_capacity(max_messages.min(1024)),
            max_messages,
            scroll_offset: 0,
        }
    }

    fn push(&mut self, message: String) {
        let was_at_bottom = self.is_at_bottom();
        if self.messages.len() >= self.max_messages {
            self.messages.pop_front();
            // Ajustar offset si se eliminó un mensaje por arriba
            if self.scroll_offset > 0 {
                self.scroll_offset = self.scroll_offset.saturating_sub(1);
            }
        }
        self.messages.push_back(message);
        // Auto-scroll si estábamos al fondo
        if was_at_bottom {
            self.scroll_offset = 0;
        }
    }

    fn visible_messages(&mut self, viewport_height: usize) -> &[String] {
        let total = self.messages.len();
        if total == 0 {
            return &[];
        }
        let end = total.saturating_sub(self.scroll_offset);
        let start = end.saturating_sub(viewport_height);
        let slice = self.messages.make_contiguous();
        &slice[start..end]
    }

    fn scroll_up(&mut self, lines: usize, viewport_height: usize) {
        let max_offset = self.messages.len().saturating_sub(viewport_height);
        self.scroll_offset = (self.scroll_offset + lines).min(max_offset);
    }

    fn scroll_down(&mut self, lines: usize) {
        self.scroll_offset = self.scroll_offset.saturating_sub(lines);
    }

    fn scroll_to_bottom(&mut self) {
        self.scroll_offset = 0;
    }

    fn is_at_bottom(&self) -> bool {
        self.scroll_offset == 0
    }
}

// ---------- InputLine ----------

struct InputLine {
    buffer: String,
    cursor_pos: usize,
}

impl InputLine {
    fn new() -> Self {
        Self { buffer: String::new(), cursor_pos: 0 }
    }

    fn insert(&mut self, c: char) {
        self.buffer.insert(self.cursor_pos, c);
        self.cursor_pos += c.len_utf8();
    }

    fn backspace(&mut self) {
        if self.cursor_pos > 0 {
            let prev = self.buffer[..self.cursor_pos]
                .char_indices().last().map(|(i, _)| i).unwrap_or(0);
            self.buffer.remove(prev);
            self.cursor_pos = prev;
        }
    }

    fn move_left(&mut self) {
        if self.cursor_pos > 0 {
            self.cursor_pos = self.buffer[..self.cursor_pos]
                .char_indices().last().map(|(i, _)| i).unwrap_or(0);
        }
    }

    fn move_right(&mut self) {
        if self.cursor_pos < self.buffer.len() {
            self.cursor_pos += self.buffer[self.cursor_pos..]
                .chars().next().map(|c| c.len_utf8()).unwrap_or(0);
        }
    }

    fn home(&mut self) { self.cursor_pos = 0; }
    fn end(&mut self) { self.cursor_pos = self.buffer.len(); }

    fn take(&mut self) -> String {
        let content = std::mem::take(&mut self.buffer);
        self.cursor_pos = 0;
        content
    }

    fn is_empty(&self) -> bool { self.buffer.is_empty() }
}

// ---------- Rendering ----------

fn render_full(
    out: &mut impl Write,
    history: &mut MessageHistory,
    input: &InputLine,
) -> std::io::Result<()> {
    let (cols, rows) = terminal::size().unwrap_or((80, 24));
    if rows < 4 {
        return Ok(()); // Terminal demasiado pequeña
    }
    let viewport_height = rows.saturating_sub(3);

    // Título
    queue!(
        out,
        cursor::MoveTo(0, 0),
        terminal::Clear(terminal::ClearType::CurrentLine),
        style::Print(" === Chat Room === "),
    )?;
    let hint = "[↑↓ scroll] ";
    let hint_col = cols.saturating_sub(hint.len() as u16);
    queue!(out, cursor::MoveTo(hint_col, 0), style::Print(hint))?;

    // Mensajes
    let visible = history.visible_messages(viewport_height as usize);
    for (i, msg) in visible.iter().enumerate() {
        let row = 1 + i as u16;
        queue!(
            out,
            cursor::MoveTo(0, row),
            terminal::Clear(terminal::ClearType::CurrentLine),
        )?;
        let display: String = msg.chars().take(cols as usize).collect();
        queue!(out, style::Print(&display))?;
    }
    for row in (1 + visible.len() as u16)..=viewport_height {
        queue!(
            out,
            cursor::MoveTo(0, row),
            terminal::Clear(terminal::ClearType::CurrentLine),
        )?;
    }

    // Separador
    let sep_row = rows - 2;
    let sep_line: String = "─".repeat(cols as usize);
    queue!(
        out,
        cursor::MoveTo(0, sep_row),
        terminal::Clear(terminal::ClearType::CurrentLine),
        style::Print(&sep_line),
    )?;

    // Indicador de scroll
    if !history.is_at_bottom() {
        let indicator = format!("── ↓ {} more ──", history.scroll_offset);
        let col = cols.saturating_sub(indicator.len() as u16) / 2;
        queue!(out, cursor::MoveTo(col, sep_row), style::Print(&indicator))?;
    }

    // Input
    let input_row = rows - 1;
    let prompt = "> ";
    queue!(
        out,
        cursor::MoveTo(0, input_row),
        terminal::Clear(terminal::ClearType::CurrentLine),
        style::Print(prompt),
    )?;
    let available = (cols as usize).saturating_sub(prompt.len());
    let display: String = input.buffer.chars().take(available).collect();
    queue!(out, style::Print(&display))?;

    let cursor_col = prompt.len() + input.buffer[..input.cursor_pos].chars().count();
    queue!(out, cursor::MoveTo(cursor_col as u16, input_row))?;

    out.flush()
}

// ---------- Input reader thread ----------

fn spawn_input_reader() -> mpsc::UnboundedReceiver<Event> {
    let (tx, rx) = mpsc::unbounded_channel();
    std::thread::spawn(move || {
        loop {
            match event::poll(Duration::from_millis(100)) {
                Ok(true) => {
                    if let Ok(evt) = event::read() {
                        if tx.send(evt).is_err() { break; }
                    }
                }
                Ok(false) => continue,
                Err(_) => break,
            }
        }
    });
    rx
}

// ---------- Terminal guard ----------

struct TerminalGuard;

impl TerminalGuard {
    fn new() -> std::io::Result<Self> {
        enable_raw_mode()?;
        execute!(stdout(), EnterAlternateScreen, cursor::Show)?;
        Ok(Self)
    }
}

impl Drop for TerminalGuard {
    fn drop(&mut self) {
        let _ = execute!(stdout(), cursor::Show, LeaveAlternateScreen);
        let _ = disable_raw_mode();
    }
}

// ---------- Main ----------

#[tokio::main]
async fn main() -> Result<(), Box<dyn std::error::Error>> {
    let args: Vec<String> = std::env::args().collect();
    let addr = args.get(1).cloned().unwrap_or_else(|| "127.0.0.1:8080".into());

    // Conectar ANTES de entrar en modo raw (para ver errores normalmente)
    eprintln!("Connecting to {}...", addr);
    let socket = TcpStream::connect(&addr).await?;

    // Entrar en modo TUI
    let _guard = TerminalGuard::new()?;
    let mut out = stdout();

    let (reader, mut writer) = socket.into_split();
    let mut server_reader = BufReader::new(reader);
    let mut input_rx = spawn_input_reader();

    let mut history = MessageHistory::new(10_000);
    let mut input = InputLine::new();
    let mut server_line = String::new();

    history.push("Connected! Type /help for commands, /quit to exit.".into());
    render_full(&mut out, &mut history, &input)?;

    loop {
        let (_, rows) = terminal::size().unwrap_or((80, 24));
        let viewport_height = rows.saturating_sub(3);

        tokio::select! {
            // Mensaje del servidor
            result = server_reader.read_line(&mut server_line) => {
                match result {
                    Ok(0) => {
                        history.push("*** Server closed the connection ***".into());
                        render_full(&mut out, &mut history, &input)?;
                        tokio::time::sleep(Duration::from_secs(2)).await;
                        break;
                    }
                    Ok(_) => {
                        let msg = server_line.trim_end().to_string();
                        history.push(msg);
                        server_line.clear();
                        render_full(&mut out, &mut history, &input)?;
                    }
                    Err(e) => {
                        history.push(format!("*** Connection error: {} ***", e));
                        render_full(&mut out, &mut history, &input)?;
                        tokio::time::sleep(Duration::from_secs(2)).await;
                        break;
                    }
                }
            }

            // Evento de teclado
            Some(evt) = input_rx.recv() => {
                if let Event::Key(key) = evt {
                    match key.code {
                        KeyCode::Enter => {
                            if !input.is_empty() {
                                let line = input.take();
                                history.scroll_to_bottom();

                                if line.trim() == "/quit" || line.trim() == "/exit" {
                                    let _ = writer.write_all(
                                        format!("{}\n", line).as_bytes()
                                    ).await;
                                    break;
                                }

                                if let Err(e) = writer.write_all(
                                    format!("{}\n", line).as_bytes()
                                ).await {
                                    history.push(format!("*** Send error: {} ***", e));
                                    break;
                                }
                            }
                        }

                        KeyCode::Char(c) => {
                            if c == 'c' && key.modifiers.contains(KeyModifiers::CONTROL) {
                                let _ = writer.write_all(b"/quit\n").await;
                                break;
                            }
                            input.insert(c);
                        }

                        KeyCode::Backspace => input.backspace(),
                        KeyCode::Left => input.move_left(),
                        KeyCode::Right => input.move_right(),
                        KeyCode::Home => input.home(),
                        KeyCode::End => input.end(),

                        KeyCode::Up => {
                            history.scroll_up(3, viewport_height as usize);
                        }
                        KeyCode::Down => {
                            history.scroll_down(3);
                        }
                        KeyCode::PageUp => {
                            history.scroll_up(
                                viewport_height as usize / 2,
                                viewport_height as usize,
                            );
                        }
                        KeyCode::PageDown => {
                            history.scroll_down(viewport_height as usize / 2);
                        }

                        _ => {}
                    }
                } else if let Event::Resize(_, _) = evt {
                    // Terminal redimensionada: simplemente re-render
                }
                render_full(&mut out, &mut history, &input)?;
            }
        }
    }

    Ok(())
}
```

### Word wrapping

El código anterior trunca líneas largas. Para word wrap real:

```rust
fn wrap_message(msg: &str, max_width: usize) -> Vec<String> {
    if msg.len() <= max_width {
        return vec![msg.to_string()];
    }

    let mut lines = Vec::new();
    let mut current = String::new();

    for word in msg.split_whitespace() {
        if current.is_empty() {
            current = word.to_string();
        } else if current.len() + 1 + word.len() <= max_width {
            current.push(' ');
            current.push_str(word);
        } else {
            lines.push(current);
            current = format!("  {}", word); // Indent continuation
        }
    }
    if !current.is_empty() {
        lines.push(current);
    }

    lines
}
```

Con word wrap, `MessageHistory::push` expande un mensaje largo en varias
líneas visuales antes de insertarlas:

```rust
fn push_wrapped(&mut self, message: String, max_width: usize) {
    for line in wrap_message(&message, max_width) {
        self.push(line);
    }
}
```

---

## Errores comunes

### 1. Olvidar restaurar la terminal al salir

```rust
// ❌ Si hay un error o panic, la terminal queda en modo raw
enable_raw_mode()?;
do_stuff()?; // Si esto falla...
disable_raw_mode()?; // ...esto nunca se ejecuta

// ✅ Usar el guard pattern (Drop se ejecuta siempre)
let _guard = TerminalGuard::new()?;
do_stuff()?;
// _guard.drop() restaura la terminal automáticamente

// Si tu terminal queda rota, ejecutar:
// reset          (comando de bash)
// stty sane      (alternativa)
```

### 2. Flicker al redibujar toda la pantalla

```rust
// ❌ Limpiar toda la pantalla antes de redibujar
execute!(out, terminal::Clear(terminal::ClearType::All))?;
// Esto causa parpadeo visible porque hay un frame completamente vacío

// ✅ Limpiar línea por línea con queue! y un solo flush
for row in 0..rows {
    queue!(out, cursor::MoveTo(0, row),
        terminal::Clear(terminal::ClearType::CurrentLine),
        style::Print(content_for_row(row)))?;
}
out.flush()?; // Un solo flush: todo aparece de golpe
```

### 3. No manejar el resize de la terminal

```rust
// ❌ Calcular dimensiones una vez al inicio
let (cols, rows) = terminal::size()?;
// Si el usuario redimensiona, el layout se rompe

// ✅ Recalcular en cada render y en Event::Resize
if let Event::Resize(_, _) = evt {
    // Re-render con nuevas dimensiones
    render_full(&mut out, &mut history, &input)?;
}
// render_full() siempre llama a terminal::size() internamente
```

### 4. scroll_offset inconsistente al recibir mensajes

```rust
// ❌ Siempre scroll_to_bottom al recibir un mensaje
fn push(&mut self, msg: String) {
    self.messages.push_back(msg);
    self.scroll_offset = 0; // Fuerza al fondo
    // El usuario estaba leyendo historial y lo perdió
}

// ✅ Solo auto-scroll si ya estaba al fondo
fn push(&mut self, msg: String) {
    let was_at_bottom = self.is_at_bottom();
    self.messages.push_back(msg);
    if was_at_bottom {
        self.scroll_offset = 0;
    }
    // Si estaba scrolleado arriba, no mover
}
```

### 5. Mezclar print!/println! con la TUI raw

```rust
// ❌ Usar println! en modo raw — no funciona como se espera
// En modo raw, \n mueve el cursor abajo pero NO al inicio de la línea
println!("Hello");
// Output: "Hello" seguido de cursor una línea abajo en la misma columna

// ✅ En modo raw, usar queue!/execute! con MoveTo para posicionar
queue!(
    out,
    cursor::MoveTo(0, row),
    style::Print("Hello"),
)?;
// O si realmente necesitas \r\n en raw:
// write!(out, "Hello\r\n")?;  // \r = carriage return, \n = line feed
```

---

## Cheatsheet

```
┌──────────────────────────────────────────────────────────────┐
│          HISTORIAL CON SCROLL CHEATSHEET                     │
├──────────────────────────────────────────────────────────────┤
│                                                              │
│  CROSSTERM SETUP                                             │
│  enable_raw_mode()           Cada keystroke es un evento    │
│  disable_raw_mode()          Restaurar modo normal          │
│  EnterAlternateScreen        Pantalla secundaria (vim-like) │
│  LeaveAlternateScreen        Restaurar pantalla original    │
│  terminal::size()            (cols, rows) actuales          │
│                                                              │
│  COMANDOS DE DIBUJO                                         │
│  cursor::MoveTo(col, row)    Posicionar cursor              │
│  cursor::Show / Hide         Mostrar/ocultar cursor         │
│  style::Print(text)          Imprimir en posición actual    │
│  Clear(CurrentLine)          Limpiar línea actual            │
│  queue!(out, cmd1, cmd2)     Buffer sin flush               │
│  execute!(out, cmd1, cmd2)   Buffer + flush                 │
│  out.flush()                 Enviar buffer al terminal       │
│                                                              │
│  EVENTOS                                                     │
│  event::poll(Duration)       ¿Hay evento? (no-blocking)    │
│  event::read()               Leer evento (blocking)         │
│  Event::Key(KeyEvent)        Tecla presionada               │
│  Event::Resize(cols, rows)   Terminal redimensionada        │
│  KeyCode::Char(c)            Carácter normal                │
│  KeyCode::Enter/Backspace    Teclas especiales               │
│  KeyCode::Up/Down/Left/Right Flechas                        │
│  KeyCode::PageUp/PageDown    Scroll rápido                  │
│  KeyModifiers::CONTROL       Detectar Ctrl+C, Ctrl+D       │
│                                                              │
│  MESSAGEHISTORY                                              │
│  VecDeque<String>            Ring buffer (O(1) ambos lados) │
│  scroll_offset = 0           Mostrar lo más reciente       │
│  scroll_up(n, viewport)      Subir n líneas                │
│  scroll_down(n)              Bajar n líneas                 │
│  scroll_to_bottom()          Volver al fondo                │
│  is_at_bottom()              ¿Estamos al fondo?            │
│  visible_messages(height)    Slice visible actual           │
│                                                              │
│  INPUTLINE                                                   │
│  insert(c) / backspace()     Editar texto                   │
│  move_left() / move_right()  Mover cursor                   │
│  home() / end()              Inicio/fin de línea            │
│  take()                      Extraer y resetear              │
│                                                              │
│  TECLAS DEL CHAT TUI                                         │
│  ↑/↓           Scroll 3 líneas                              │
│  Shift+↑/↓     Scroll 1 línea                               │
│  PageUp/Down   Scroll media pantalla                        │
│  Enter          Enviar mensaje                               │
│  Ctrl+C         Salir                                        │
│                                                              │
│  GUARD PATTERN                                               │
│  let _guard = TerminalGuard::new()?;                        │
│  // Drop restaura terminal incluso en panic                 │
│  // Recuperar terminal rota: reset o stty sane              │
└──────────────────────────────────────────────────────────────┘
```

---

## Ejercicios

### Ejercicio 1: TUI mínima sin red

Construye la interfaz TUI sin conexión de red (solo local):

```toml
[dependencies]
crossterm = "0.28"
```

**Tareas:**
1. Implementa `TerminalGuard` con `enable_raw_mode` + `EnterAlternateScreen`
2. Implementa `MessageHistory` con `VecDeque` y scroll
3. Implementa `InputLine` con insert, backspace, y movimiento de cursor
4. Dibuja el layout: título, área de mensajes, separador, input
5. Loop de eventos:
   - Escribir texto → aparece en la línea de input
   - Enter → el mensaje se agrega al historial (simulando un echo local)
   - ↑/↓ → scroll del historial
   - Ctrl+C → salir
6. Agrega 50 mensajes de prueba al inicio para verificar que el scroll funciona
7. Verifica que al redimensionar la terminal (Event::Resize) el layout se adapta

**Pregunta de reflexión:** ¿Por qué usamos `queue!` para múltiples comandos
y un solo `flush()` al final, en vez de `execute!` para cada comando? ¿Qué
efecto visual tendría usar `execute!` por cada línea?

---

### Ejercicio 2: Cliente TUI conectado

Integra la TUI con la conexión TCP del servidor de S01:

**Tareas:**
1. Conecta al servidor **antes** de entrar en modo raw (para ver errores)
2. Usa `spawn_input_reader` para leer eventos de crossterm en un thread
3. Integra con `tokio::select!`:
   - Rama 1: `server_reader.read_line()` → `history.push()` + render
   - Rama 2: `input_rx.recv()` → procesar keystroke + render
4. Prueba con dos clientes TUI contra el mismo servidor
5. Verifica que al recibir un mensaje mientras estás scrolleado arriba,
   el historial **no** salta al fondo automáticamente
6. Verifica que al enviar un mensaje (Enter), sí saltas al fondo
7. Desconecta el servidor y verifica que el mensaje de error aparece
   en el historial y el cliente sale limpiamente

**Pregunta de reflexión:** El `event::poll(Duration::from_millis(100))` en el
thread de input introduce un delay de hasta 100ms en la respuesta al teclado.
¿Por qué no usamos `Duration::ZERO` para respuesta instantánea? ¿Qué pasaría
con el uso de CPU?

---

### Ejercicio 3: Indicador de nuevos mensajes

Agrega un indicador visual cuando llegan mensajes mientras el usuario está
scrolleado arriba:

**Tareas:**
1. Agrega un campo `unread_count: usize` a `MessageHistory`
2. Cuando llega un mensaje y `!is_at_bottom()`, incrementa `unread_count`
3. Cuando el usuario hace `scroll_to_bottom()`, resetea `unread_count = 0`
4. Muestra en el separador: `── ↓ 5 new messages ──`
5. Colorea el indicador con `style::Stylize`:
   ```rust
   use crossterm::style::Stylize;
   style::Print(indicator.yellow().bold())
   ```
6. Agrega `Home` para ir al tope del historial y `End` para ir al fondo
7. Bonus: al presionar `End`, haz un scroll suave (baja de a 5 líneas con
   un pequeño delay entre frames) en vez de un salto instantáneo

**Pregunta de reflexión:** El word wrapping afecta el cálculo de scroll: un
mensaje de 200 caracteres en un terminal de 80 columnas ocupa 3 líneas visuales.
¿Cómo ajustarías `scroll_offset` y `visible_messages` para que cuenten líneas
**visuales** en vez de mensajes lógicos?
