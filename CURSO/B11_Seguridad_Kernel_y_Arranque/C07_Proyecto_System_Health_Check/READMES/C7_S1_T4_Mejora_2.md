# Mejora 2 — Interfaz TUI con ratatui

## Índice

1. [¿Qué es una TUI y por qué ratatui?](#1-qué-es-una-tui-y-por-qué-ratatui)
2. [Arquitectura de una aplicación TUI](#2-arquitectura-de-una-aplicación-tui)
3. [Setup del proyecto Rust](#3-setup-del-proyecto-rust)
4. [ratatui: conceptos fundamentales](#4-ratatui-conceptos-fundamentales)
5. [Layout: dividir la terminal en paneles](#5-layout-dividir-la-terminal-en-paneles)
6. [Widgets: bloques, gauges, tablas, listas](#6-widgets-bloques-gauges-tablas-listas)
7. [Recolectar datos del sistema en Rust](#7-recolectar-datos-del-sistema-en-rust)
8. [Actualización periódica y eventos](#8-actualización-periódica-y-eventos)
9. [Implementación completa](#9-implementación-completa)
10. [Errores comunes](#10-errores-comunes)
11. [Cheatsheet](#11-cheatsheet)
12. [Ejercicios](#12-ejercicios)

---

## 1. ¿Qué es una TUI y por qué ratatui?

Una **TUI** (Text User Interface) es una interfaz gráfica que se ejecuta dentro de la terminal, usando caracteres para dibujar paneles, barras, tablas y otros elementos visuales. A diferencia de un script que imprime líneas de texto, una TUI controla toda la pantalla y se actualiza en tiempo real.

```
Script (T02):                        TUI (T04):
┌───────────────────────┐            ┌───────────────────────────────┐
│ $ ./health_report.sh  │            │ System Health ─ web-prod-01   │
│                       │            ├───────────────┬───────────────┤
│   DISK                │            │ DISK          │ MEMORY        │
│   / 84% [████░░] OK   │            │ / ████░░ 84%  │ RAM  ██░░ 37% │
│   /home 44% ...       │            │ /home ██░ 44% │ Swap ░░░░  2% │
│   MEMORY              │            ├───────────────┼───────────────┤
│   RAM 37% ...         │            │ LOAD          │ SERVICES      │
│   ...                 │            │ 0.45 0.62 0.38│ ✓ All running │
│ $                     │            │ CPUs: 8    OK │               │
│                       │            ├───────────────┴───────────────┤
│ (salida estática,     │            │ Recent logs...                │
│  se ejecuta y termina)│            │ (actualización cada 2s)       │
└───────────────────────┘            └───────────────────────────────┘
                                      q=quit  r=refresh  j/k=scroll
```

### ¿Por qué ratatui?

```
Alternativas TUI en Rust:
  ├── ratatui        ← Estándar de facto, muy activo, excelente docs
  │                    Fork mejorado de tui-rs (abandonado)
  ├── cursive         Widgets de alto nivel, estilo ncurses
  └── crossterm       Solo backend de terminal (lo usa ratatui)

ratatui destaca por:
  ├── Immediate mode rendering (como un game loop)
  ├── Widgets composables (Block, Gauge, Table, List, Chart...)
  ├── Layout flexible (constraints con porcentajes, min, max)
  ├── Ecosistema activo (color-eyre, tui-textarea, tui-logger...)
  └── Excelente documentación y ejemplos
```

### Immediate mode vs retained mode

```
Retained mode (GUI tradicional):
  Crear widget → modificar propiedades → el framework redibuja
  Ejemplo: GTK, Qt

Immediate mode (ratatui):
  Cada frame:
    1. Leer estado de la aplicación
    2. Construir TODOS los widgets desde el estado
    3. Renderizar la pantalla completa
    4. Esperar input o timeout
    5. Actualizar estado
    6. Volver a 1

  No hay "objetos widget" persistentes.
  El código de rendering ES la descripción de la UI.
```

---

## 2. Arquitectura de una aplicación TUI

### El patrón App + Event Loop

```
┌─────────────┐     ┌──────────────┐     ┌──────────────┐
│   App       │     │  Event Loop  │     │   Terminal    │
│  (estado)   │◄───▶│  (main loop) │◄───▶│  (crossterm)  │
│             │     │              │     │              │
│ - disk_data │     │ loop {       │     │ - raw mode   │
│ - mem_data  │     │   poll event │     │ - alternate  │
│ - load_data │     │   handle key │     │   screen     │
│ - services  │     │   update data│     │ - draw       │
│ - logs      │     │   render UI  │     │              │
│ - running   │     │ }            │     │              │
└─────────────┘     └──────────────┘     └──────────────┘
```

### Estructura del proyecto

```
health-tui/
├── Cargo.toml
└── src/
    ├── main.rs          Event loop, setup/teardown del terminal
    ├── app.rs           Estado de la aplicación
    ├── collector.rs     Recolección de datos del sistema
    └── ui.rs            Rendering (layout + widgets)
```

---

## 3. Setup del proyecto Rust

### Crear el proyecto

```bash
cargo new health-tui
cd health-tui
```

### Cargo.toml

```toml
[package]
name = "health-tui"
version = "0.1.0"
edition = "2021"

[dependencies]
ratatui = "0.29"
crossterm = "0.28"
```

> **Pregunta de predicción**: El proyecto depende de `ratatui` y `crossterm`. ¿Cuál es el rol de cada uno? ¿Podría ratatui funcionar sin crossterm?

ratatui es la librería de **widgets y layout** — sabe qué dibujar (barras, tablas, bordes) pero no sabe cómo hablar con el terminal. crossterm es el **backend de terminal** — maneja raw mode, alternate screen, colores, y eventos de teclado. ratatui necesita un backend (crossterm, termion, o termwiz) para funcionar. crossterm es el más portable (funciona en Linux, macOS, Windows).

---

## 4. ratatui: conceptos fundamentales

### Terminal y Frame

```rust
use ratatui::{Terminal, Frame};
use ratatui::backend::CrosstermBackend;

// Terminal: maneja el backend y el doble buffer
let backend = CrosstermBackend::new(std::io::stdout());
let mut terminal = Terminal::new(backend)?;

// Frame: una "foto" de un frame de renderizado
// Se obtiene dentro de terminal.draw()
terminal.draw(|frame: &mut Frame| {
    // frame.area() → Rect { x: 0, y: 0, width: 80, height: 24 }
    // frame.render_widget(widget, area);
})?;
```

### Rect: áreas rectangulares

```rust
use ratatui::layout::Rect;

// Rect define un área rectangular en la terminal
// x, y: esquina superior izquierda
// width, height: dimensiones
let area = Rect::new(0, 0, 80, 24);  // Pantalla completa (ejemplo)

// Los widgets se renderizan dentro de un Rect
// El layout system subdivide el Rect principal en sub-áreas
```

### Widget trait

```rust
// Todo lo que se puede renderizar implementa Widget
use ratatui::widgets::Widget;

// Los widgets más comunes:
// Block      — borde con título
// Paragraph  — texto (multi-línea, con estilos)
// Gauge      — barra de progreso
// Table      — tabla con filas y columnas
// List       — lista con ítems
// BarChart   — gráfico de barras
// Sparkline  — mini gráfico de tendencia
```

### Estilos

```rust
use ratatui::style::{Style, Color, Modifier};

// Definir estilos
let ok_style = Style::default().fg(Color::Green);
let warn_style = Style::default().fg(Color::Yellow).add_modifier(Modifier::BOLD);
let crit_style = Style::default().fg(Color::Red).add_modifier(Modifier::BOLD);
let dim_style = Style::default().fg(Color::DarkGray);
let title_style = Style::default().fg(Color::Cyan).add_modifier(Modifier::BOLD);
```

---

## 5. Layout: dividir la terminal en paneles

### Constraints

```rust
use ratatui::layout::{Layout, Direction, Constraint};

// Layout divide un Rect en sub-áreas usando constraints
let chunks = Layout::default()
    .direction(Direction::Vertical)
    .constraints([
        Constraint::Length(3),       // Exactamente 3 filas (header)
        Constraint::Percentage(40),  // 40% del espacio restante
        Constraint::Percentage(40),  // 40%
        Constraint::Min(5),          // Al menos 5 filas (footer)
    ])
    .split(frame.area());

// chunks[0] = header (3 filas)
// chunks[1] = sección superior (40%)
// chunks[2] = sección inferior (40%)
// chunks[3] = footer (resto, mínimo 5)
```

### Layout jerárquico para el dashboard

```
┌────────────────────────────────────────────────┐
│  Header: hostname, status, uptime     (Len 3)  │
├───────────────────────┬────────────────────────┤
│  Disk Usage    (50%)  │  Memory         (50%)  │  ← Vertical 40%
│  ████░░ /  84%        │  RAM  ██░░ 37%         │
│  ██░░░ /home 44%      │  Swap ░░░░  2%         │
├───────────────────────┼────────────────────────┤
│  Load Average  (50%)  │  Services       (50%)  │  ← Vertical 30%
│  0.45  0.62  0.38     │  ✓ All running         │
│  CPUs: 8              │                        │
├───────────────────────┴────────────────────────┤
│  Recent Logs / Journal                (Min 5)  │  ← Vertical rest
│  Mar 21 14:30 kernel: ...                      │
└────────────────────────────────────────────────┘
```

```rust
fn ui(frame: &mut Frame, app: &App) {
    // Nivel 1: dividir verticalmente
    let main_chunks = Layout::default()
        .direction(Direction::Vertical)
        .constraints([
            Constraint::Length(3),       // Header
            Constraint::Percentage(40),  // Disk + Memory
            Constraint::Percentage(30),  // Load + Services
            Constraint::Min(5),          // Logs
        ])
        .split(frame.area());

    // Nivel 2: dividir horizontalmente las filas centrales
    let top_chunks = Layout::default()
        .direction(Direction::Horizontal)
        .constraints([
            Constraint::Percentage(50),
            Constraint::Percentage(50),
        ])
        .split(main_chunks[1]);

    let mid_chunks = Layout::default()
        .direction(Direction::Horizontal)
        .constraints([
            Constraint::Percentage(50),
            Constraint::Percentage(50),
        ])
        .split(main_chunks[2]);

    // Renderizar widgets en cada área
    render_header(frame, main_chunks[0], app);
    render_disk(frame, top_chunks[0], app);
    render_memory(frame, top_chunks[1], app);
    render_load(frame, mid_chunks[0], app);
    render_services(frame, mid_chunks[1], app);
    render_logs(frame, main_chunks[3], app);
}
```

---

## 6. Widgets: bloques, gauges, tablas, listas

### Block: contenedor con borde y título

```rust
use ratatui::widgets::{Block, Borders};

let block = Block::default()
    .title(" Disk Usage ")
    .borders(Borders::ALL)
    .border_style(Style::default().fg(Color::DarkGray))
    .title_style(Style::default().fg(Color::Cyan).add_modifier(Modifier::BOLD));

// Renderizar el bloque (solo el borde)
frame.render_widget(block, area);

// Para renderizar contenido DENTRO del bloque:
let inner = block.inner(area);  // Rect sin los bordes
frame.render_widget(content_widget, inner);
```

### Gauge: barra de progreso

```rust
use ratatui::widgets::Gauge;

fn severity_color(pct: u16) -> Color {
    if pct >= 90 { Color::Red }
    else if pct >= 80 { Color::Yellow }
    else { Color::Green }
}

let pct = 84;
let gauge = Gauge::default()
    .block(Block::default().title(" / ").borders(Borders::ALL))
    .gauge_style(Style::default().fg(severity_color(pct)))
    .percent(pct)
    .label(format!("{}% — 42G / 50G", pct));

frame.render_widget(gauge, area);
```

### Table: tabla con filas

```rust
use ratatui::widgets::{Table, Row, Cell};

let header = Row::new(vec![
    Cell::from("Mount").style(Style::default().fg(Color::DarkGray)),
    Cell::from("Used").style(Style::default().fg(Color::DarkGray)),
    Cell::from("Size").style(Style::default().fg(Color::DarkGray)),
    Cell::from("Use%").style(Style::default().fg(Color::DarkGray)),
]);

let rows: Vec<Row> = app.disks.iter().map(|d| {
    let color = severity_color(d.percent);
    Row::new(vec![
        Cell::from(d.mount.as_str()),
        Cell::from(format_kb(d.used_kb)),
        Cell::from(format_kb(d.size_kb)),
        Cell::from(format!("{}%", d.percent)).style(Style::default().fg(color)),
    ])
}).collect();

let table = Table::new(rows, [
        Constraint::Percentage(30),
        Constraint::Percentage(25),
        Constraint::Percentage(25),
        Constraint::Percentage(20),
    ])
    .header(header)
    .block(Block::default().title(" Disk Usage ").borders(Borders::ALL));

frame.render_widget(table, area);
```

### List: lista con ítems

```rust
use ratatui::widgets::{List, ListItem};

let items: Vec<ListItem> = app.logs.iter().map(|log| {
    let style = if log.contains("error") || log.contains("FAIL") {
        Style::default().fg(Color::Red)
    } else if log.contains("warn") {
        Style::default().fg(Color::Yellow)
    } else {
        Style::default().fg(Color::DarkGray)
    };
    ListItem::new(log.as_str()).style(style)
}).collect();

let list = List::new(items)
    .block(Block::default().title(" Recent Logs ").borders(Borders::ALL));

frame.render_widget(list, area);
```

### Paragraph: texto con estilos inline

```rust
use ratatui::widgets::Paragraph;
use ratatui::text::{Line, Span};

let header = Paragraph::new(vec![
    Line::from(vec![
        Span::styled("System Health ", Style::default().fg(Color::Cyan).add_modifier(Modifier::BOLD)),
        Span::styled(format!("— {}", app.hostname), Style::default().fg(Color::DarkGray)),
    ]),
    Line::from(vec![
        Span::styled("Status: ", Style::default()),
        Span::styled(
            app.global_status.to_uppercase(),
            Style::default().fg(match app.global_status.as_str() {
                "ok" => Color::Green,
                "warn" => Color::Yellow,
                _ => Color::Red,
            }).add_modifier(Modifier::BOLD),
        ),
        Span::styled(format!("  Up: {}", app.uptime_str), Style::default().fg(Color::DarkGray)),
    ]),
])
.block(Block::default().borders(Borders::ALL));

frame.render_widget(header, area);
```

---

## 7. Recolectar datos del sistema en Rust

### Estructuras de datos

```rust
// src/app.rs

pub struct DiskInfo {
    pub mount: String,
    pub size_kb: u64,
    pub used_kb: u64,
    pub avail_kb: u64,
    pub percent: u16,
}

pub struct MemInfo {
    pub total_kb: u64,
    pub used_kb: u64,
    pub available_kb: u64,
    pub percent: u16,
}

pub struct LoadInfo {
    pub avg_1: f64,
    pub avg_5: f64,
    pub avg_15: f64,
    pub cpus: u32,
}

pub struct App {
    pub hostname: String,
    pub kernel: String,
    pub uptime_str: String,
    pub global_status: String,

    pub disks: Vec<DiskInfo>,
    pub mem: MemInfo,
    pub swap: MemInfo,
    pub load: LoadInfo,
    pub failed_services: Vec<String>,
    pub logs: Vec<String>,

    pub running: bool,
}
```

### Leer /proc/meminfo

```rust
// src/collector.rs
use std::fs;

pub fn collect_memory() -> (MemInfo, MemInfo) {
    let content = fs::read_to_string("/proc/meminfo")
        .unwrap_or_default();

    let mut mem_total: u64 = 0;
    let mut mem_available: u64 = 0;
    let mut swap_total: u64 = 0;
    let mut swap_free: u64 = 0;

    for line in content.lines() {
        let parts: Vec<&str> = line.split_whitespace().collect();
        if parts.len() < 2 { continue; }
        let val: u64 = parts[1].parse().unwrap_or(0);
        match parts[0] {
            "MemTotal:"     => mem_total = val,
            "MemAvailable:" => mem_available = val,
            "SwapTotal:"    => swap_total = val,
            "SwapFree:"     => swap_free = val,
            _ => {}
        }
    }

    let mem_used = mem_total.saturating_sub(mem_available);
    let mem_pct = if mem_total > 0 { (mem_used * 100 / mem_total) as u16 } else { 0 };

    let swap_used = swap_total.saturating_sub(swap_free);
    let swap_pct = if swap_total > 0 { (swap_used * 100 / swap_total) as u16 } else { 0 };

    let ram = MemInfo {
        total_kb: mem_total,
        used_kb: mem_used,
        available_kb: mem_available,
        percent: mem_pct,
    };

    let swap = MemInfo {
        total_kb: swap_total,
        used_kb: swap_used,
        available_kb: swap_free,
        percent: swap_pct,
    };

    (ram, swap)
}
```

### Leer /proc/loadavg

```rust
pub fn collect_load() -> LoadInfo {
    let content = fs::read_to_string("/proc/loadavg")
        .unwrap_or_default();
    let parts: Vec<&str> = content.split_whitespace().collect();

    let avg_1: f64 = parts.first().and_then(|s| s.parse().ok()).unwrap_or(0.0);
    let avg_5: f64 = parts.get(1).and_then(|s| s.parse().ok()).unwrap_or(0.0);
    let avg_15: f64 = parts.get(2).and_then(|s| s.parse().ok()).unwrap_or(0.0);

    // Contar CPUs leyendo /proc/cpuinfo
    let cpuinfo = fs::read_to_string("/proc/cpuinfo").unwrap_or_default();
    let cpus = cpuinfo.lines()
        .filter(|l| l.starts_with("processor"))
        .count() as u32;

    LoadInfo { avg_1, avg_5, avg_15, cpus: cpus.max(1) }
}
```

### Obtener discos con df (via Command)

```rust
use std::process::Command;

pub fn collect_disks() -> Vec<DiskInfo> {
    let output = Command::new("df")
        .args(["-P", "-k", "--output=target,size,used,avail,pcent",
               "-x", "tmpfs", "-x", "devtmpfs", "-x", "squashfs",
               "-x", "overlay", "-x", "efivarfs"])
        .output();

    let output = match output {
        Ok(o) => String::from_utf8_lossy(&o.stdout).to_string(),
        Err(_) => return Vec::new(),
    };

    output.lines()
        .skip(1) // header
        .filter_map(|line| {
            let parts: Vec<&str> = line.split_whitespace().collect();
            if parts.len() < 5 { return None; }
            Some(DiskInfo {
                mount: parts[0].to_string(),
                size_kb: parts[1].parse().unwrap_or(0),
                used_kb: parts[2].parse().unwrap_or(0),
                avail_kb: parts[3].parse().unwrap_or(0),
                percent: parts[4].trim_end_matches('%').parse().unwrap_or(0),
            })
        })
        .collect()
}
```

### Obtener servicios fallidos

```rust
pub fn collect_failed_services() -> Vec<String> {
    let output = Command::new("systemctl")
        .args(["--failed", "--no-legend", "--no-pager", "--plain"])
        .output();

    match output {
        Ok(o) => {
            let text = String::from_utf8_lossy(&o.stdout);
            text.lines()
                .filter(|l| !l.is_empty())
                .map(|l| l.split_whitespace().next().unwrap_or("").to_string())
                .collect()
        }
        Err(_) => Vec::new(),
    }
}
```

### Obtener logs recientes del journal

```rust
pub fn collect_recent_logs(count: usize) -> Vec<String> {
    let output = Command::new("journalctl")
        .args(["--no-pager", "-n", &count.to_string(),
               "--output=short-iso", "-p", "warning"])
        .output();

    match output {
        Ok(o) => {
            String::from_utf8_lossy(&o.stdout)
                .lines()
                .map(|l| l.to_string())
                .collect()
        }
        Err(_) => vec!["(journal not available)".to_string()],
    }
}
```

---

## 8. Actualización periódica y eventos

### Event loop con polling

```rust
// src/main.rs
use std::time::{Duration, Instant};
use crossterm::event::{self, Event, KeyCode, KeyEventKind};

fn main() -> Result<(), Box<dyn std::error::Error>> {
    // Setup terminal
    crossterm::terminal::enable_raw_mode()?;
    let mut stdout = std::io::stdout();
    crossterm::execute!(stdout,
        crossterm::terminal::EnterAlternateScreen,
        crossterm::event::EnableMouseCapture,
    )?;
    let backend = ratatui::backend::CrosstermBackend::new(stdout);
    let mut terminal = ratatui::Terminal::new(backend)?;

    // App state
    let mut app = App::new();
    app.refresh_data();

    let tick_rate = Duration::from_secs(2);
    let mut last_tick = Instant::now();

    // Event loop
    loop {
        // Render
        terminal.draw(|frame| ui(frame, &app))?;

        // Poll events with timeout
        let timeout = tick_rate.saturating_sub(last_tick.elapsed());
        if event::poll(timeout)? {
            if let Event::Key(key) = event::read()? {
                if key.kind == KeyEventKind::Press {
                    match key.code {
                        KeyCode::Char('q') => break,
                        KeyCode::Char('r') => app.refresh_data(),
                        _ => {}
                    }
                }
            }
        }

        // Auto-refresh on tick
        if last_tick.elapsed() >= tick_rate {
            app.refresh_data();
            last_tick = Instant::now();
        }
    }

    // Restore terminal
    crossterm::terminal::disable_raw_mode()?;
    crossterm::execute!(
        terminal.backend_mut(),
        crossterm::terminal::LeaveAlternateScreen,
        crossterm::event::DisableMouseCapture,
    )?;
    terminal.show_cursor()?;

    Ok(())
}
```

### Raw mode y alternate screen

```
Raw mode:
  Normal: el terminal procesa Enter, Ctrl+C, bufferea líneas
  Raw:    el programa recibe CADA tecla individualmente
          Ctrl+C no mata el proceso (lo manejamos nosotros)
          No hay echo automático

Alternate screen:
  Normal: la salida se mezcla con el historial del terminal
  Alternate: pantalla separada, al salir se restaura el contenido original
             (como hace vim, htop, less)

IMPORTANTE: siempre restaurar el terminal al salir (disable_raw_mode +
LeaveAlternateScreen), incluso en caso de panic. Sino la terminal
queda inutilizable.
```

### Manejo seguro de panic

```rust
use std::panic;

fn main() -> Result<(), Box<dyn std::error::Error>> {
    // Setup
    setup_terminal()?;

    // Instalar panic hook que restaura el terminal
    let original_hook = panic::take_hook();
    panic::set_hook(Box::new(move |panic_info| {
        let _ = restore_terminal();
        original_hook(panic_info);
    }));

    // Run app
    let result = run_app();

    // Restore
    restore_terminal()?;
    result
}

fn setup_terminal() -> Result<(), Box<dyn std::error::Error>> {
    crossterm::terminal::enable_raw_mode()?;
    crossterm::execute!(
        std::io::stdout(),
        crossterm::terminal::EnterAlternateScreen,
    )?;
    Ok(())
}

fn restore_terminal() -> Result<(), Box<dyn std::error::Error>> {
    crossterm::terminal::disable_raw_mode()?;
    crossterm::execute!(
        std::io::stdout(),
        crossterm::terminal::LeaveAlternateScreen,
    )?;
    Ok(())
}
```

---

## 9. Implementación completa

### Cargo.toml

```toml
[package]
name = "health-tui"
version = "0.1.0"
edition = "2021"

[dependencies]
ratatui = "0.29"
crossterm = "0.28"
```

### src/main.rs

```rust
mod app;
mod collector;
mod ui;

use std::io;
use std::panic;
use std::time::{Duration, Instant};

use crossterm::event::{self, Event, KeyCode, KeyEventKind};
use crossterm::terminal::{
    disable_raw_mode, enable_raw_mode, EnterAlternateScreen, LeaveAlternateScreen,
};
use crossterm::execute;
use ratatui::backend::CrosstermBackend;
use ratatui::Terminal;

use app::App;
use ui::draw_ui;

fn setup_terminal() -> io::Result<Terminal<CrosstermBackend<io::Stdout>>> {
    enable_raw_mode()?;
    let mut stdout = io::stdout();
    execute!(stdout, EnterAlternateScreen)?;
    let backend = CrosstermBackend::new(stdout);
    Terminal::new(backend)
}

fn restore_terminal(terminal: &mut Terminal<CrosstermBackend<io::Stdout>>) -> io::Result<()> {
    disable_raw_mode()?;
    execute!(terminal.backend_mut(), LeaveAlternateScreen)?;
    terminal.show_cursor()
}

fn run_app(terminal: &mut Terminal<CrosstermBackend<io::Stdout>>) -> io::Result<()> {
    let mut app = App::new();
    app.refresh();

    let tick_rate = Duration::from_secs(2);
    let mut last_tick = Instant::now();

    loop {
        terminal.draw(|frame| draw_ui(frame, &app))?;

        let timeout = tick_rate.saturating_sub(last_tick.elapsed());
        if event::poll(timeout)? {
            if let Event::Key(key) = event::read()? {
                if key.kind == KeyEventKind::Press {
                    match key.code {
                        KeyCode::Char('q') | KeyCode::Esc => return Ok(()),
                        KeyCode::Char('r') => app.refresh(),
                        _ => {}
                    }
                }
            }
        }

        if last_tick.elapsed() >= tick_rate {
            app.refresh();
            last_tick = Instant::now();
        }
    }
}

fn main() -> Result<(), Box<dyn std::error::Error>> {
    // Panic hook to restore terminal
    let original_hook = panic::take_hook();
    panic::set_hook(Box::new(move |info| {
        let _ = disable_raw_mode();
        let _ = execute!(io::stdout(), LeaveAlternateScreen);
        original_hook(info);
    }));

    let mut terminal = setup_terminal()?;
    let result = run_app(&mut terminal);
    restore_terminal(&mut terminal)?;
    result?;
    Ok(())
}
```

### src/app.rs

```rust
use crate::collector;

pub struct DiskInfo {
    pub mount: String,
    pub size_kb: u64,
    pub used_kb: u64,
    pub percent: u16,
}

pub struct MemInfo {
    pub total_kb: u64,
    pub used_kb: u64,
    pub percent: u16,
}

pub struct App {
    pub hostname: String,
    pub kernel: String,
    pub uptime_str: String,
    pub status: String,

    pub disks: Vec<DiskInfo>,
    pub ram: MemInfo,
    pub swap: MemInfo,
    pub load: [f64; 3],
    pub cpus: u32,
    pub failed_services: Vec<String>,
    pub logs: Vec<String>,
}

impl App {
    pub fn new() -> Self {
        Self {
            hostname: String::new(),
            kernel: String::new(),
            uptime_str: String::new(),
            status: "ok".to_string(),
            disks: Vec::new(),
            ram: MemInfo { total_kb: 0, used_kb: 0, percent: 0 },
            swap: MemInfo { total_kb: 0, used_kb: 0, percent: 0 },
            load: [0.0; 3],
            cpus: 1,
            failed_services: Vec::new(),
            logs: Vec::new(),
        }
    }

    pub fn refresh(&mut self) {
        self.hostname = collector::hostname();
        self.kernel = collector::kernel_version();
        self.uptime_str = collector::uptime_string();
        self.disks = collector::disks();
        let (ram, swap) = collector::memory();
        self.ram = ram;
        self.swap = swap;
        let (load, cpus) = collector::load();
        self.load = load;
        self.cpus = cpus;
        self.failed_services = collector::failed_services();
        self.logs = collector::recent_logs(20);
        self.evaluate_status();
    }

    fn evaluate_status(&mut self) {
        self.status = "ok".to_string();

        for d in &self.disks {
            if d.percent >= 90 { self.status = "crit".to_string(); return; }
            if d.percent >= 80 && self.status != "crit" { self.status = "warn".to_string(); }
        }
        if self.ram.percent >= 95 { self.status = "crit".to_string(); return; }
        if self.ram.percent >= 80 && self.status != "crit" { self.status = "warn".to_string(); }
        if !self.failed_services.is_empty() { self.status = "crit".to_string(); return; }
        if self.load[0] >= self.cpus as f64 * 2.0 { self.status = "crit".to_string(); return; }
        if self.load[0] >= self.cpus as f64 && self.status != "crit" {
            self.status = "warn".to_string();
        }
    }
}
```

### src/collector.rs

```rust
use crate::app::{DiskInfo, MemInfo};
use std::fs;
use std::process::Command;

pub fn hostname() -> String {
    fs::read_to_string("/proc/sys/kernel/hostname")
        .unwrap_or_else(|_| "unknown".into())
        .trim()
        .to_string()
}

pub fn kernel_version() -> String {
    fs::read_to_string("/proc/version")
        .unwrap_or_default()
        .split_whitespace()
        .nth(2)
        .unwrap_or("unknown")
        .to_string()
}

pub fn uptime_string() -> String {
    let secs: u64 = fs::read_to_string("/proc/uptime")
        .unwrap_or_default()
        .split_whitespace()
        .next()
        .and_then(|s| s.parse::<f64>().ok())
        .map(|f| f as u64)
        .unwrap_or(0);

    let d = secs / 86400;
    let h = (secs % 86400) / 3600;
    let m = (secs % 3600) / 60;
    format!("{}d {}h {}m", d, h, m)
}

pub fn memory() -> (MemInfo, MemInfo) {
    let content = fs::read_to_string("/proc/meminfo").unwrap_or_default();
    let mut mt: u64 = 0;
    let mut ma: u64 = 0;
    let mut st: u64 = 0;
    let mut sf: u64 = 0;

    for line in content.lines() {
        let parts: Vec<&str> = line.split_whitespace().collect();
        if parts.len() < 2 { continue; }
        let val: u64 = parts[1].parse().unwrap_or(0);
        match parts[0] {
            "MemTotal:"     => mt = val,
            "MemAvailable:" => ma = val,
            "SwapTotal:"    => st = val,
            "SwapFree:"     => sf = val,
            _ => {}
        }
    }

    let mu = mt.saturating_sub(ma);
    let su = st.saturating_sub(sf);

    (
        MemInfo { total_kb: mt, used_kb: mu, percent: if mt > 0 { (mu * 100 / mt) as u16 } else { 0 } },
        MemInfo { total_kb: st, used_kb: su, percent: if st > 0 { (su * 100 / st) as u16 } else { 0 } },
    )
}

pub fn load() -> ([f64; 3], u32) {
    let content = fs::read_to_string("/proc/loadavg").unwrap_or_default();
    let parts: Vec<&str> = content.split_whitespace().collect();
    let l1: f64 = parts.first().and_then(|s| s.parse().ok()).unwrap_or(0.0);
    let l5: f64 = parts.get(1).and_then(|s| s.parse().ok()).unwrap_or(0.0);
    let l15: f64 = parts.get(2).and_then(|s| s.parse().ok()).unwrap_or(0.0);

    let cpuinfo = fs::read_to_string("/proc/cpuinfo").unwrap_or_default();
    let cpus = cpuinfo.lines().filter(|l| l.starts_with("processor")).count() as u32;

    ([l1, l5, l15], cpus.max(1))
}

pub fn disks() -> Vec<DiskInfo> {
    let output = Command::new("df")
        .args(["-P", "-k", "--output=target,size,used,avail,pcent",
               "-x", "tmpfs", "-x", "devtmpfs", "-x", "squashfs",
               "-x", "overlay", "-x", "efivarfs"])
        .output()
        .ok();

    let text = match output {
        Some(o) => String::from_utf8_lossy(&o.stdout).to_string(),
        None => return Vec::new(),
    };

    text.lines()
        .skip(1)
        .filter_map(|line| {
            let p: Vec<&str> = line.split_whitespace().collect();
            if p.len() < 5 { return None; }
            Some(DiskInfo {
                mount: p[0].to_string(),
                size_kb: p[1].parse().unwrap_or(0),
                used_kb: p[2].parse().unwrap_or(0),
                percent: p[4].trim_end_matches('%').parse().unwrap_or(0),
            })
        })
        .collect()
}

pub fn failed_services() -> Vec<String> {
    let output = Command::new("systemctl")
        .args(["--failed", "--no-legend", "--no-pager", "--plain"])
        .output()
        .ok();

    match output {
        Some(o) => String::from_utf8_lossy(&o.stdout)
            .lines()
            .filter(|l| !l.is_empty())
            .map(|l| l.split_whitespace().next().unwrap_or("").to_string())
            .collect(),
        None => Vec::new(),
    }
}

pub fn recent_logs(count: usize) -> Vec<String> {
    let output = Command::new("journalctl")
        .args(["--no-pager", "-n", &count.to_string(), "--output=short", "-p", "warning"])
        .output()
        .ok();

    match output {
        Some(o) => String::from_utf8_lossy(&o.stdout)
            .lines()
            .map(|l| l.to_string())
            .collect(),
        None => vec!["(journal not available)".to_string()],
    }
}
```

### src/ui.rs

```rust
use ratatui::Frame;
use ratatui::layout::{Constraint, Direction, Layout};
use ratatui::style::{Color, Modifier, Style};
use ratatui::text::{Line, Span};
use ratatui::widgets::{Block, Borders, Gauge, List, ListItem, Paragraph, Row, Cell, Table};

use crate::app::App;

fn severity_color(pct: u16, warn: u16, crit: u16) -> Color {
    if pct >= crit { Color::Red }
    else if pct >= warn { Color::Yellow }
    else { Color::Green }
}

fn format_kb(kb: u64) -> String {
    if kb >= 1_048_576 { format!("{:.1}G", kb as f64 / 1_048_576.0) }
    else if kb >= 1024 { format!("{}M", kb / 1024) }
    else { format!("{}K", kb) }
}

fn status_color(status: &str) -> Color {
    match status {
        "ok" => Color::Green,
        "warn" => Color::Yellow,
        _ => Color::Red,
    }
}

pub fn draw_ui(frame: &mut Frame, app: &App) {
    let main = Layout::default()
        .direction(Direction::Vertical)
        .constraints([
            Constraint::Length(4),
            Constraint::Min(6),
            Constraint::Min(5),
            Constraint::Min(5),
        ])
        .split(frame.area());

    let top_panels = Layout::default()
        .direction(Direction::Horizontal)
        .constraints([Constraint::Percentage(55), Constraint::Percentage(45)])
        .split(main[1]);

    let mid_panels = Layout::default()
        .direction(Direction::Horizontal)
        .constraints([Constraint::Percentage(50), Constraint::Percentage(50)])
        .split(main[2]);

    draw_header(frame, main[0], app);
    draw_disk(frame, top_panels[0], app);
    draw_memory(frame, top_panels[1], app);
    draw_load(frame, mid_panels[0], app);
    draw_services(frame, mid_panels[1], app);
    draw_logs(frame, main[3], app);
}

fn draw_header(frame: &mut Frame, area: ratatui::layout::Rect, app: &App) {
    let sc = status_color(&app.status);
    let status_text = match app.status.as_str() {
        "ok" => "ALL OK",
        "warn" => "WARNINGS",
        _ => "CRITICAL",
    };

    let header = Paragraph::new(vec![
        Line::from(vec![
            Span::styled(" System Health ", Style::default().fg(Color::Cyan).add_modifier(Modifier::BOLD)),
            Span::styled(format!("— {} ", app.hostname), Style::default().fg(Color::DarkGray)),
        ]),
        Line::from(vec![
            Span::raw(" Status: "),
            Span::styled(format!(" {} ", status_text), Style::default().fg(Color::Black).bg(sc).add_modifier(Modifier::BOLD)),
            Span::styled(format!("  Kernel: {}  Up: {}", app.kernel, app.uptime_str), Style::default().fg(Color::DarkGray)),
        ]),
        Line::from(Span::styled(" q=quit  r=refresh  (auto-refresh 2s)", Style::default().fg(Color::DarkGray))),
    ])
    .block(Block::default().borders(Borders::BOTTOM).border_style(Style::default().fg(Color::DarkGray)));

    frame.render_widget(header, area);
}

fn draw_disk(frame: &mut Frame, area: ratatui::layout::Rect, app: &App) {
    let header = Row::new(vec!["Mount", "Used", "Size", "Use%"])
        .style(Style::default().fg(Color::DarkGray));

    let rows: Vec<Row> = app.disks.iter().map(|d| {
        let color = severity_color(d.percent, 80, 90);
        Row::new(vec![
            Cell::from(d.mount.as_str()),
            Cell::from(format_kb(d.used_kb)),
            Cell::from(format_kb(d.size_kb)),
            Cell::from(format!("{}%", d.percent)).style(Style::default().fg(color)),
        ])
    }).collect();

    let table = Table::new(rows, [
            Constraint::Percentage(35),
            Constraint::Percentage(22),
            Constraint::Percentage(22),
            Constraint::Percentage(21),
        ])
        .header(header)
        .block(Block::default().title(" Disk ").borders(Borders::ALL)
            .border_style(Style::default().fg(Color::DarkGray))
            .title_style(Style::default().fg(Color::Cyan)));

    frame.render_widget(table, area);
}

fn draw_memory(frame: &mut Frame, area: ratatui::layout::Rect, app: &App) {
    let inner_layout = Layout::default()
        .direction(Direction::Vertical)
        .margin(1)
        .constraints([Constraint::Length(2), Constraint::Length(2), Constraint::Min(0)])
        .split(area);

    let block = Block::default()
        .title(" Memory ")
        .borders(Borders::ALL)
        .border_style(Style::default().fg(Color::DarkGray))
        .title_style(Style::default().fg(Color::Cyan));
    frame.render_widget(block, area);

    // RAM gauge
    let ram_color = severity_color(app.ram.percent, 80, 95);
    let ram_gauge = Gauge::default()
        .block(Block::default().title("RAM"))
        .gauge_style(Style::default().fg(ram_color))
        .percent(app.ram.percent)
        .label(format!("{}% — {} / {}", app.ram.percent, format_kb(app.ram.used_kb), format_kb(app.ram.total_kb)));
    frame.render_widget(ram_gauge, inner_layout[0]);

    // Swap gauge
    let swap_color = severity_color(app.swap.percent, 50, 80);
    let swap_label = if app.swap.total_kb > 0 {
        format!("{}% — {} / {}", app.swap.percent, format_kb(app.swap.used_kb), format_kb(app.swap.total_kb))
    } else {
        "not configured".to_string()
    };
    let swap_gauge = Gauge::default()
        .block(Block::default().title("Swap"))
        .gauge_style(Style::default().fg(swap_color))
        .percent(if app.swap.total_kb > 0 { app.swap.percent } else { 0 })
        .label(swap_label);
    frame.render_widget(swap_gauge, inner_layout[1]);
}

fn draw_load(frame: &mut Frame, area: ratatui::layout::Rect, app: &App) {
    let load_color = if app.load[0] >= app.cpus as f64 * 2.0 { Color::Red }
        else if app.load[0] >= app.cpus as f64 { Color::Yellow }
        else { Color::Green };

    let content = Paragraph::new(vec![
        Line::from(vec![
            Span::raw(" 1m: "),
            Span::styled(format!("{:.2}", app.load[0]), Style::default().fg(load_color).add_modifier(Modifier::BOLD)),
            Span::raw("   5m: "),
            Span::styled(format!("{:.2}", app.load[1]), Style::default().fg(Color::White)),
            Span::raw("   15m: "),
            Span::styled(format!("{:.2}", app.load[2]), Style::default().fg(Color::White)),
        ]),
        Line::from(Span::styled(
            format!(" CPUs: {}  (threshold: {:.0}/{:.0})", app.cpus, app.cpus as f64, app.cpus as f64 * 2.0),
            Style::default().fg(Color::DarkGray),
        )),
    ])
    .block(Block::default().title(" Load ").borders(Borders::ALL)
        .border_style(Style::default().fg(Color::DarkGray))
        .title_style(Style::default().fg(Color::Cyan)));

    frame.render_widget(content, area);
}

fn draw_services(frame: &mut Frame, area: ratatui::layout::Rect, app: &App) {
    let content = if app.failed_services.is_empty() {
        Paragraph::new(Line::from(Span::styled(
            " ✓ All services running",
            Style::default().fg(Color::Green),
        )))
    } else {
        let mut lines = vec![Line::from(Span::styled(
            format!(" ✗ {} failed:", app.failed_services.len()),
            Style::default().fg(Color::Red).add_modifier(Modifier::BOLD),
        ))];
        for name in &app.failed_services {
            lines.push(Line::from(Span::styled(
                format!("   {}", name),
                Style::default().fg(Color::Red),
            )));
        }
        Paragraph::new(lines)
    };

    let widget = content
        .block(Block::default().title(" Services ").borders(Borders::ALL)
            .border_style(Style::default().fg(Color::DarkGray))
            .title_style(Style::default().fg(Color::Cyan)));

    frame.render_widget(widget, area);
}

fn draw_logs(frame: &mut Frame, area: ratatui::layout::Rect, app: &App) {
    let items: Vec<ListItem> = app.logs.iter().map(|log| {
        let style = if log.contains("error") || log.contains("crit") {
            Style::default().fg(Color::Red)
        } else if log.contains("warn") {
            Style::default().fg(Color::Yellow)
        } else {
            Style::default().fg(Color::DarkGray)
        };
        ListItem::new(Line::from(Span::styled(log.as_str(), style)))
    }).collect();

    let list = List::new(items)
        .block(Block::default().title(" Recent Logs (warning+) ").borders(Borders::ALL)
            .border_style(Style::default().fg(Color::DarkGray))
            .title_style(Style::default().fg(Color::Cyan)));

    frame.render_widget(list, area);
}
```

### Compilar y ejecutar

```bash
cd health-tui
cargo build --release
sudo ./target/release/health-tui

# sudo es necesario para leer journalctl con filtro de prioridad
# y para acceder a algunos datos de systemctl
```

---

## 10. Errores comunes

### Error 1: No restaurar el terminal al salir

```rust
// MAL — si el programa crasha, la terminal queda en raw mode
fn main() {
    enable_raw_mode().unwrap();
    // ... panic! ...
    // disable_raw_mode() nunca se ejecuta
    // La terminal queda inutilizable (reset para arreglar)
}

// BIEN — instalar panic hook
panic::set_hook(Box::new(move |info| {
    let _ = disable_raw_mode();
    let _ = execute!(io::stdout(), LeaveAlternateScreen);
    eprintln!("{}", info);
}));
```

### Error 2: Bloquear el event loop con recolección lenta

```rust
// MAL — collect tarda 2 segundos (journalctl lento)
// La UI se congela durante la recolección
loop {
    terminal.draw(|f| ui(f, &app))?;
    app.refresh();  // Bloquea 2 segundos
}

// BIEN — recolectar solo en el tick, no en cada frame
if last_tick.elapsed() >= tick_rate {
    app.refresh();
    last_tick = Instant::now();
}
// Entre ticks, solo se renderizan frames y se procesan teclas
```

### Error 3: Porcentaje mayor que 100 en el Gauge

```rust
// MAL — Gauge::percent() acepta u16, pero panics si > 100
let gauge = Gauge::default().percent(105);  // PANIC

// BIEN — clamp el valor
let pct = disk.percent.min(100);
let gauge = Gauge::default().percent(pct);
```

### Error 4: Layout que no suma 100%

```rust
// MAL — los porcentajes suman más de 100
Layout::default()
    .constraints([
        Constraint::Percentage(50),
        Constraint::Percentage(50),
        Constraint::Percentage(50),  // Total: 150%
    ])

// ratatui no da error pero los widgets se solapan o desaparecen

// BIEN — usar Min para el último elemento
Layout::default()
    .constraints([
        Constraint::Percentage(40),
        Constraint::Percentage(30),
        Constraint::Min(5),  // Toma el resto
    ])
```

### Error 5: No manejar terminales pequeñas

```rust
// MAL — asumir que la terminal tiene al menos 80×24
// En terminales pequeñas, los Rect pueden tener width o height 0
// lo que causa panics en algunos widgets

// BIEN — verificar tamaño mínimo
if frame.area().width < 40 || frame.area().height < 15 {
    let msg = Paragraph::new("Terminal too small. Resize to 40x15+")
        .style(Style::default().fg(Color::Red));
    frame.render_widget(msg, frame.area());
    return;
}
```

---

## 11. Cheatsheet

```
SETUP PROYECTO
  cargo new health-tui
  ratatui = "0.29"     Widget library
  crossterm = "0.28"   Terminal backend

TERMINAL
  enable_raw_mode()                  Modo raw (recibir cada tecla)
  EnterAlternateScreen               Pantalla alternativa
  disable_raw_mode()                 Restaurar
  LeaveAlternateScreen               Restaurar pantalla

LAYOUT
  Layout::default()
    .direction(Vertical | Horizontal)
    .constraints([Length(3), Percentage(50), Min(5)])
    .split(area)                     Dividir área en sub-áreas

WIDGETS
  Block::default()
    .title("Title").borders(ALL)     Contenedor con borde
  Gauge::default()
    .percent(84).label("84%")        Barra de progreso
  Table::new(rows, widths)
    .header(header_row)              Tabla
  List::new(items)                   Lista
  Paragraph::new(lines)              Texto con estilos

ESTILOS
  Style::default()
    .fg(Color::Green)
    .add_modifier(Modifier::BOLD)
  Span::styled("text", style)        Texto con estilo inline

EVENT LOOP
  event::poll(timeout)?              Esperar evento con timeout
  Event::Key(key) => match key.code  Manejar teclas
  KeyCode::Char('q') => break        Salir

DATOS DEL SISTEMA
  /proc/meminfo                      Memoria
  /proc/loadavg                      Load average
  /proc/uptime                       Uptime
  /proc/cpuinfo                      CPUs
  df -P -k                           Disco (via Command)
  systemctl --failed                 Servicios (via Command)
  journalctl -n 20 -p warning        Logs recientes (via Command)
```

---

## 12. Ejercicios

### Ejercicio 1: Hello TUI — ventana mínima con ratatui

**Objetivo**: Verificar que el entorno funciona y entender el ciclo básico.

```rust
// Crear un nuevo proyecto y reemplazar src/main.rs con:

use std::io;
use crossterm::event::{self, Event, KeyCode, KeyEventKind};
use crossterm::terminal::{disable_raw_mode, enable_raw_mode, EnterAlternateScreen, LeaveAlternateScreen};
use crossterm::execute;
use ratatui::backend::CrosstermBackend;
use ratatui::Terminal;
use ratatui::widgets::{Block, Borders, Paragraph};
use ratatui::style::{Color, Style, Modifier};
use ratatui::text::{Line, Span};
use ratatui::layout::{Layout, Direction, Constraint};

fn main() -> Result<(), Box<dyn std::error::Error>> {
    enable_raw_mode()?;
    execute!(io::stdout(), EnterAlternateScreen)?;
    let backend = CrosstermBackend::new(io::stdout());
    let mut terminal = Terminal::new(backend)?;

    loop {
        terminal.draw(|frame| {
            let chunks = Layout::default()
                .direction(Direction::Vertical)
                .constraints([Constraint::Length(3), Constraint::Min(0)])
                .split(frame.area());

            let title = Paragraph::new(Line::from(vec![
                Span::styled(" Hello TUI! ", Style::default()
                    .fg(Color::Cyan)
                    .add_modifier(Modifier::BOLD)),
                Span::styled(" Press 'q' to quit", Style::default().fg(Color::DarkGray)),
            ]))
            .block(Block::default().borders(Borders::ALL));
            frame.render_widget(title, chunks[0]);

            let body = Paragraph::new(format!(
                "Terminal size: {}×{}\nThis is ratatui!",
                frame.area().width, frame.area().height
            ))
            .block(Block::default().title(" Info ").borders(Borders::ALL));
            frame.render_widget(body, chunks[1]);
        })?;

        if event::poll(std::time::Duration::from_millis(250))? {
            if let Event::Key(key) = event::read()? {
                if key.kind == KeyEventKind::Press && key.code == KeyCode::Char('q') {
                    break;
                }
            }
        }
    }

    disable_raw_mode()?;
    execute!(terminal.backend_mut(), LeaveAlternateScreen)?;
    Ok(())
}
```

```bash
# Ejecutar:
cargo run

# Observar:
# - La pantalla se limpia y muestra los widgets
# - El tamaño del terminal se muestra en tiempo real
# - Presionar 'q' sale limpiamente
# - Redimensionar la ventana actualiza el layout
```

**Pregunta de reflexión**: ¿Por qué usamos `event::poll()` con un timeout en vez de `event::read()` directamente? ¿Qué pasaría si usáramos solo `read()`?

> `event::read()` es **bloqueante** — se queda esperando indefinidamente hasta que el usuario presiona una tecla. Esto impediría que la TUI se redibuje o actualice datos automáticamente. Con `poll(Duration)`, verificamos si hay un evento disponible dentro del timeout; si no lo hay, el bucle continúa y puede redibujar la pantalla, actualizar datos, o ejecutar la lógica del tick. Es la diferencia entre una UI que responde solo a input y una que se actualiza en tiempo real.

---

### Ejercicio 2: Panel de memoria con Gauge

**Objetivo**: Crear un panel con barras de progreso que muestren la memoria real del sistema.

```rust
// Modificar el ejercicio 1 para añadir gauges de memoria.
// Añadir esta función y llamarla desde el draw:

use ratatui::widgets::Gauge;

fn draw_memory(frame: &mut ratatui::Frame, area: ratatui::layout::Rect) {
    let meminfo = std::fs::read_to_string("/proc/meminfo").unwrap_or_default();
    let mut mt: u64 = 0;
    let mut ma: u64 = 0;
    let mut st: u64 = 0;
    let mut sf: u64 = 0;

    for line in meminfo.lines() {
        let parts: Vec<&str> = line.split_whitespace().collect();
        if parts.len() < 2 { continue; }
        let val: u64 = parts[1].parse().unwrap_or(0);
        match parts[0] {
            "MemTotal:" => mt = val,
            "MemAvailable:" => ma = val,
            "SwapTotal:" => st = val,
            "SwapFree:" => sf = val,
            _ => {}
        }
    }

    let mu = mt.saturating_sub(ma);
    let mp = if mt > 0 { (mu * 100 / mt) as u16 } else { 0 };
    let su = st.saturating_sub(sf);
    let sp = if st > 0 { (su * 100 / st) as u16 } else { 0 };

    let inner = Layout::default()
        .direction(Direction::Vertical)
        .margin(1)
        .constraints([Constraint::Length(2), Constraint::Length(2), Constraint::Min(0)])
        .split(area);

    let block = Block::default()
        .title(" Memory ")
        .borders(Borders::ALL)
        .border_style(Style::default().fg(Color::DarkGray));
    frame.render_widget(block, area);

    let ram_color = if mp >= 95 { Color::Red } else if mp >= 80 { Color::Yellow } else { Color::Green };
    let ram = Gauge::default()
        .block(Block::default().title("RAM"))
        .gauge_style(Style::default().fg(ram_color))
        .percent(mp.min(100))
        .label(format!("{}% — {:.1}G / {:.1}G", mp, mu as f64 / 1048576.0, mt as f64 / 1048576.0));
    frame.render_widget(ram, inner[0]);

    if st > 0 {
        let swap_color = if sp >= 80 { Color::Red } else if sp >= 50 { Color::Yellow } else { Color::Green };
        let swap = Gauge::default()
            .block(Block::default().title("Swap"))
            .gauge_style(Style::default().fg(swap_color))
            .percent(sp.min(100))
            .label(format!("{}% — {:.1}G / {:.1}G", sp, su as f64 / 1048576.0, st as f64 / 1048576.0));
        frame.render_widget(swap, inner[1]);
    }
}

// En el draw, reemplazar el body con:
// draw_memory(frame, chunks[1]);
```

**Pregunta de reflexión**: Estamos leyendo `/proc/meminfo` en cada frame (cada 250ms). ¿Es esto un problema de rendimiento? ¿Cómo lo mejorarías?

> Leer `/proc/meminfo` es muy rápido (es un pseudo-archivo que el kernel genera al vuelo, no I/O de disco real), pero hacerlo 4 veces por segundo es innecesario — la memoria no cambia tan rápido. La mejora es separar la recolección del renderizado: recolectar datos cada 2 segundos (en el tick) y almacenarlos en el struct App, y renderizar desde los datos almacenados en cada frame. Así el rendering es instantáneo (solo lee variables en memoria) y la recolección ocurre a una frecuencia razonable. Esto es exactamente lo que hace la implementación completa de la sección 9.

---

### Ejercicio 3: Dashboard completo

**Objetivo**: Compilar y ejecutar la implementación completa de la sección 9.

```bash
# 1. Crear el proyecto
cargo new health-tui
cd health-tui

# 2. Copiar los archivos de la sección 9:
#    - Cargo.toml (reemplazar)
#    - src/main.rs
#    - src/app.rs
#    - src/collector.rs
#    - src/ui.rs

# 3. Compilar
cargo build --release

# 4. Ejecutar (sudo para journalctl)
sudo ./target/release/health-tui

# 5. Interactuar:
#    - Observar la actualización cada 2 segundos
#    - Presionar 'r' para forzar refresh
#    - Redimensionar la terminal y observar el layout adaptarse
#    - Presionar 'q' para salir

# 6. Verificar que la terminal se restauró correctamente
echo "Terminal restored OK"

# EXTENSIONES SUGERIDAS (no obligatorias):
# a) Añadir un panel con las interfaces de red y sus IPs
# b) Añadir Sparkline con historial de load average
# c) Añadir scroll a la lista de logs (j/k o flechas)
# d) Añadir flag --interval para cambiar el tick rate
# e) Añadir exportación: presionar 's' para guardar snapshot JSON
```

**Pregunta de reflexión**: La TUI se actualiza cada 2 segundos. Si quisieras añadir un Sparkline (mini gráfico de tendencia) que muestre el historial del load average de los últimos 5 minutos, ¿qué estructura de datos usarías para almacenar los valores históricos? ¿Cuántos puntos de datos necesitas?

> Usaría un `VecDeque<f64>` con capacidad fija (ring buffer). Con un tick de 2 segundos y 5 minutos de historial: 300 / 2 = 150 puntos de datos. En cada tick, hacer `push_back(load_1)` y si la longitud excede 150, hacer `pop_front()`. El widget `Sparkline` de ratatui acepta `&[u64]`, así que convertirías los `f64` a `u64` multiplicando por 100 (para preservar 2 decimales). `VecDeque` es la estructura ideal porque las inserciones y eliminaciones en ambos extremos son O(1), perfecto para un buffer circular de series temporales.

---

> **Bloque 11 completado.** Este era el último tema de C07 (Proyecto) y de todo B11 — Seguridad, Kernel y Arranque.
