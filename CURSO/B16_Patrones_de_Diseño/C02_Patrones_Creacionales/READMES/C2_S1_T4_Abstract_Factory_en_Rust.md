# T04 — Abstract Factory en Rust

---

## 1. De C a Rust: que cambia

T03 implemento el Abstract Factory en C con vtables manuales:
una por la factory, una por cada tipo de producto. El boilerplate
era considerable (~300 lineas para dos familias con tres productos).

En Rust, los traits reemplazan las vtables. Pero ademas, Rust
ofrece una herramienta que C no tiene: **associated types**, que
permiten una forma de Abstract Factory con dispatch estatico.

```
  C (T03):                          Rust (T04):
  ──────                            ──────────
  GuiFactoryVt (struct de fn ptrs)  trait GuiFactory
  ButtonVt, TextBoxVt, CheckboxVt   trait Button, TextBox, Checkbox
  cast manual (void*)               type system automatico
  destroy manual                    Drop automatico
  NULL si falla                     Option/Result
  Solo dispatch dinamico            Dinamico O estatico (associated types)
```

Rust ofrece **dos formas** de Abstract Factory:

```
  Forma 1: dyn Trait (dinamica)
  ─────────────────────────────
  trait GuiFactory {
      fn create_button(&self, label: &str) -> Box<dyn Button>;
  }
  → Equivalente directo de la version C
  → Decide la familia en runtime
  → Paga allocation por producto

  Forma 2: Associated types (estatica)
  ─────────────────────────────────────
  trait GuiFactory {
      type Btn: Button;
      fn create_button(&self, label: &str) -> Self::Btn;
  }
  → Sin allocation, sin dispatch dinamico en los productos
  → La familia se elige en compile time
  → Monomorphization: el compilador genera codigo especializado
```

---

## 2. Los traits de producto

```rust
/* Equivalente a widget.h de T03 */

trait Button {
    fn render(&self);
    fn label(&self) -> &str;
}

trait TextBox {
    fn render(&self);
    fn set_text(&mut self, text: &str);
    fn get_text(&self) -> &str;
}

trait Checkbox {
    fn render(&self);
    fn set_checked(&mut self, val: bool);
    fn is_checked(&self) -> bool;
}
```

Comparado con C: sin vtable struct, sin convenience functions,
sin destroy. Todo lo que en C eran ~60 lineas de boilerplate es
~15 lineas aqui.

---

## 3. Forma 1: dyn Trait (dispatch dinamico)

### 3.1 El trait factory

```rust
trait GuiFactory {
    fn name(&self) -> &str;
    fn create_button(&self, label: &str) -> Box<dyn Button>;
    fn create_textbox(&self) -> Box<dyn TextBox>;
    fn create_checkbox(&self, initial: bool) -> Box<dyn Checkbox>;
}
```

### 3.2 Productos concretos: familia GTK

```rust
struct GtkButton {
    label: String,
}

impl Button for GtkButton {
    fn render(&self) {
        println!("[GTK Button: {}]", self.label);
    }

    fn label(&self) -> &str {
        &self.label
    }
}

struct GtkTextBox {
    text: String,
}

impl TextBox for GtkTextBox {
    fn render(&self) {
        println!("[GTK TextBox: \"{}\"]", self.text);
    }

    fn set_text(&mut self, text: &str) {
        self.text = text.to_string();
    }

    fn get_text(&self) -> &str {
        &self.text
    }
}

struct GtkCheckbox {
    checked: bool,
}

impl Checkbox for GtkCheckbox {
    fn render(&self) {
        println!("[GTK Checkbox: {}]",
                 if self.checked { "[X]" } else { "[ ]" });
    }

    fn set_checked(&mut self, val: bool) {
        self.checked = val;
    }

    fn is_checked(&self) -> bool {
        self.checked
    }
}
```

### 3.3 Concrete factory: GTK

```rust
struct GtkFactory;

impl GuiFactory for GtkFactory {
    fn name(&self) -> &str { "GTK" }

    fn create_button(&self, label: &str) -> Box<dyn Button> {
        Box::new(GtkButton { label: label.to_string() })
    }

    fn create_textbox(&self) -> Box<dyn TextBox> {
        Box::new(GtkTextBox { text: String::new() })
    }

    fn create_checkbox(&self, initial: bool) -> Box<dyn Checkbox> {
        Box::new(GtkCheckbox { checked: initial })
    }
}
```

### 3.4 Concrete factory: Win32

```rust
struct Win32Button {
    label: String,
    hwnd: u32,
}

impl Button for Win32Button {
    fn render(&self) {
        println!("<Win32 Button hwnd={}: {}>", self.hwnd, self.label);
    }

    fn label(&self) -> &str {
        &self.label
    }
}

struct Win32TextBox {
    text: String,
    hwnd: u32,
}

impl TextBox for Win32TextBox {
    fn render(&self) {
        println!("<Win32 TextBox hwnd={}: \"{}\">", self.hwnd, self.text);
    }

    fn set_text(&mut self, text: &str) {
        self.text = text.to_string();
    }

    fn get_text(&self) -> &str {
        &self.text
    }
}

struct Win32Checkbox {
    checked: bool,
    hwnd: u32,
}

impl Checkbox for Win32Checkbox {
    fn render(&self) {
        println!("<Win32 Checkbox hwnd={}: {}>",
                 self.hwnd,
                 if self.checked { "[X]" } else { "[ ]" });
    }

    fn set_checked(&mut self, val: bool) {
        self.checked = val;
    }

    fn is_checked(&self) -> bool {
        self.checked
    }
}

use std::sync::atomic::{AtomicU32, Ordering};
static NEXT_HWND: AtomicU32 = AtomicU32::new(1000);

struct Win32Factory;

impl GuiFactory for Win32Factory {
    fn name(&self) -> &str { "Win32" }

    fn create_button(&self, label: &str) -> Box<dyn Button> {
        Box::new(Win32Button {
            label: label.to_string(),
            hwnd: NEXT_HWND.fetch_add(1, Ordering::Relaxed),
        })
    }

    fn create_textbox(&self) -> Box<dyn TextBox> {
        Box::new(Win32TextBox {
            text: String::new(),
            hwnd: NEXT_HWND.fetch_add(1, Ordering::Relaxed),
        })
    }

    fn create_checkbox(&self, initial: bool) -> Box<dyn Checkbox> {
        Box::new(Win32Checkbox {
            checked: initial,
            hwnd: NEXT_HWND.fetch_add(1, Ordering::Relaxed),
        })
    }
}
```

### 3.5 Selector y cliente

```rust
fn factory_for_platform(name: &str) -> Option<Box<dyn GuiFactory>> {
    match name {
        "gtk" | "linux"     => Some(Box::new(GtkFactory)),
        "win32" | "windows" => Some(Box::new(Win32Factory)),
        _ => None,
    }
}

fn build_login_form(factory: &dyn GuiFactory) {
    println!("Building form with {} toolkit:", factory.name());

    let mut username = factory.create_textbox();
    let mut password = factory.create_textbox();
    let mut remember = factory.create_checkbox(false);
    let login  = factory.create_button("Login");
    let cancel = factory.create_button("Cancel");

    username.set_text("admin");
    password.set_text("****");
    remember.set_checked(true);

    username.render();
    password.render();
    remember.render();
    login.render();
    cancel.render();

    // Drop automatico — sin destroy manual
}

fn main() {
    let platform = std::env::args().nth(1)
        .unwrap_or_else(|| "gtk".into());

    let factory = factory_for_platform(&platform)
        .unwrap_or_else(|| {
            eprintln!("Unknown platform: {platform}");
            std::process::exit(1);
        });

    build_login_form(&*factory);
}
```

```
  Comparacion de boilerplate (para 2 familias, 3 productos):

  C (T03):   ~400 lineas (vtables + convenience + destroy + casts)
  Rust dyn:  ~200 lineas (traits + impls + Boxes)

  La mitad del codigo. Sin riesgo de NULL, leak, o cast incorrecto.
```

---

## 4. Forma 2: Associated types (dispatch estatico)

### 4.1 El trait factory con tipos asociados

```rust
trait GuiFactory {
    type Btn: Button;
    type Txt: TextBox;
    type Chk: Checkbox;

    fn name(&self) -> &str;
    fn create_button(&self, label: &str) -> Self::Btn;
    fn create_textbox(&self) -> Self::Txt;
    fn create_checkbox(&self, initial: bool) -> Self::Chk;
}
```

Diferencia clave: `Self::Btn` es un tipo **concreto**, no
`Box<dyn Button>`. El compilador sabe exactamente que tipo
retorna cada factory.

### 4.2 Implementar para GtkFactory

```rust
impl GuiFactory for GtkFactory {
    type Btn = GtkButton;
    type Txt = GtkTextBox;
    type Chk = GtkCheckbox;

    fn name(&self) -> &str { "GTK" }

    fn create_button(&self, label: &str) -> GtkButton {
        GtkButton { label: label.to_string() }
    }

    fn create_textbox(&self) -> GtkTextBox {
        GtkTextBox { text: String::new() }
    }

    fn create_checkbox(&self, initial: bool) -> GtkCheckbox {
        GtkCheckbox { checked: initial }
    }
}
```

### 4.3 Implementar para Win32Factory

```rust
impl GuiFactory for Win32Factory {
    type Btn = Win32Button;
    type Txt = Win32TextBox;
    type Chk = Win32Checkbox;

    fn name(&self) -> &str { "Win32" }

    fn create_button(&self, label: &str) -> Win32Button {
        Win32Button {
            label: label.to_string(),
            hwnd: NEXT_HWND.fetch_add(1, Ordering::Relaxed),
        }
    }

    fn create_textbox(&self) -> Win32TextBox {
        Win32TextBox {
            text: String::new(),
            hwnd: NEXT_HWND.fetch_add(1, Ordering::Relaxed),
        }
    }

    fn create_checkbox(&self, initial: bool) -> Win32Checkbox {
        Win32Checkbox {
            checked: initial,
            hwnd: NEXT_HWND.fetch_add(1, Ordering::Relaxed),
        }
    }
}
```

### 4.4 Cliente generico

```rust
fn build_login_form<F: GuiFactory>(factory: &F) {
    println!("Building form with {} toolkit:", factory.name());

    let mut username = factory.create_textbox();
    let mut password = factory.create_textbox();
    let mut remember = factory.create_checkbox(false);
    let login  = factory.create_button("Login");
    let cancel = factory.create_button("Cancel");

    username.set_text("admin");
    password.set_text("****");
    remember.set_checked(true);

    username.render();
    password.render();
    remember.render();
    login.render();
    cancel.render();
}

fn main() {
    // La familia se elige en COMPILE time
    build_login_form(&GtkFactory);
    // O:
    build_login_form(&Win32Factory);
}
```

### 4.5 Que genera el compilador

```
  Con associated types, el compilador genera DOS funciones:

  build_login_form::<GtkFactory>
  ├─ username: GtkTextBox (en stack, sin Box)
  ├─ login: GtkButton (en stack, sin Box)
  ├─ render → GtkButton::render (llamada directa, inlineable)
  └─ Todo GtkTextBox, dispatch estatico

  build_login_form::<Win32Factory>
  ├─ username: Win32TextBox (en stack, sin Box)
  ├─ login: Win32Button (en stack, sin Box)
  ├─ render → Win32Button::render (llamada directa, inlineable)
  └─ Todo Win32, dispatch estatico
```

```
  Comparacion:

                     dyn Trait              Associated types
  ─────────          ─────────              ────────────────
  Productos          Box<dyn Button>        GtkButton (concreto)
  Allocation         Heap (Box::new)        Stack (sin alloc)
  Dispatch           Dinamico (vtable)      Estatico (inlined)
  Familia elegida    Runtime                Compile time
  Un Vec de          dyn GuiFactory         NO (cada F es distinto tipo)
  Codigo generado    1 funcion              N funciones (monomorphization)
```

---

## 5. Cuando usar dyn vs associated types

```
  ┌─────────────────────────────────────────────────────────┐
  │ ¿La familia se elige en RUNTIME?                        │
  │ (config file, env var, CLI arg, user input)             │
  │                                                         │
  │    SI → dyn Trait (Forma 1)                             │
  │    │                                                    │
  │    │   let factory: Box<dyn GuiFactory> =              │
  │    │       factory_for_platform(&platform);             │
  │    │   build_login_form(&*factory);                     │
  │    │                                                    │
  │    NO → ¿Es un hot path?                               │
  │         │                                               │
  │         ├─ SI → Associated types (Forma 2)             │
  │         │       build_login_form(&GtkFactory);          │
  │         │       Zero-cost, inlining, sin alloc          │
  │         │                                               │
  │         └─ NO → Cualquiera. dyn es mas simple.         │
  └─────────────────────────────────────────────────────────┘
```

En la practica, **dyn Trait** es la opcion por defecto para
Abstract Factory porque:
1. La familia casi siempre se elige en runtime (config, CLI)
2. El costo de Box::new por widget es irrelevante (UI no es hot path)
3. Permite almacenar `Box<dyn GuiFactory>` como campo de un struct

Associated types es para cuando el rendimiento importa y la
familia se conoce en compile time (ej. embedded, game engines
compilados para una plataforma especifica).

---

## 6. Forma hibrida: associated types + dyn en los productos

Una combinacion util: la factory usa associated types, pero
algunos productos son `Box<dyn Trait>` cuando necesitas
heterogeneidad:

```rust
trait GuiFactory {
    type Btn: Button;

    fn create_button(&self, label: &str) -> Self::Btn;

    // Algunos productos siguen siendo dyn (ej. lista de widgets)
    fn create_widget(&self, kind: &str) -> Box<dyn Widget>;
}
```

O al reves: la factory es dyn, pero devuelve enums en lugar de
Box para productos con variantes conocidas:

```rust
enum AnyButton {
    Gtk(GtkButton),
    Win32(Win32Button),
}

impl Button for AnyButton {
    fn render(&self) {
        match self {
            AnyButton::Gtk(b) => b.render(),
            AnyButton::Win32(b) => b.render(),
        }
    }

    fn label(&self) -> &str {
        match self {
            AnyButton::Gtk(b) => b.label(),
            AnyButton::Win32(b) => b.label(),
        }
    }
}

trait GuiFactory {
    fn create_button(&self, label: &str) -> AnyButton;
    // Sin Box: AnyButton esta en stack
}
```

```
  Tradeoff:

  Box<dyn Button>:     extensible, heap alloc, dispatch dinamico
  AnyButton (enum):    cerrado, stack alloc, branch dispatch
  Self::Btn (assoc):   cerrado por factory, stack, dispatch estatico
```

---

## 7. Factory con estado (theming)

```rust
struct ThemedGtkFactory {
    theme: String,
    font_size: u32,
}

#[derive(Debug)]
struct ThemedGtkButton {
    label: String,
    theme: String,
    font_size: u32,
}

impl Button for ThemedGtkButton {
    fn render(&self) {
        println!("[GTK Button: {} | theme={} {}px]",
                 self.label, self.theme, self.font_size);
    }

    fn label(&self) -> &str {
        &self.label
    }
}

impl GuiFactory for ThemedGtkFactory {
    fn name(&self) -> &str { "GTK Themed" }

    fn create_button(&self, label: &str) -> Box<dyn Button> {
        Box::new(ThemedGtkButton {
            label: label.to_string(),
            theme: self.theme.clone(),
            font_size: self.font_size,
        })
    }

    fn create_textbox(&self) -> Box<dyn TextBox> {
        // TextBox tambien hereda el tema...
        Box::new(GtkTextBox { text: String::new() })
    }

    fn create_checkbox(&self, initial: bool) -> Box<dyn Checkbox> {
        Box::new(GtkCheckbox { checked: initial })
    }
}

// Uso:
let factory = ThemedGtkFactory {
    theme: "dark".into(),
    font_size: 14,
};
let btn = factory.create_button("OK");
btn.render();
// [GTK Button: OK | theme=dark 14px]
```

```
  C (T03):                        Rust:
  ──────                          ─────
  ThemedFactory con cast manual   Struct con campos tipados
  Copiar strings con strncpy      .clone() (o &str con lifetime)
  El tema vive hasta destroy      El tema se clona al producto (owned)
```

---

## 8. Factory con generics avanzados

### 8.1 Bound en la factory para traits adicionales

```rust
use std::fmt::Debug;

trait GuiFactory {
    type Btn: Button + Debug + Send;
    type Txt: TextBox + Debug + Send;

    fn create_button(&self, label: &str) -> Self::Btn;
    fn create_textbox(&self) -> Self::Txt;
}
```

Los bounds adicionales (`Debug + Send`) se propagan a todos los
productos. Cualquier concrete factory debe crear productos que
satisfagan todos los bounds.

### 8.2 Factory como type family

```rust
// Los associated types forman una "familia de tipos"
trait Platform {
    type Factory: GuiFactory;
    type Renderer;
    type EventLoop;

    fn init() -> Self;
    fn factory(&self) -> &Self::Factory;
    fn renderer(&self) -> &Self::Renderer;
    fn event_loop(&mut self) -> &mut Self::EventLoop;
}

struct LinuxPlatform {
    factory: GtkFactory,
    // renderer: GlRenderer,
    // event_loop: GlibEventLoop,
}

impl Platform for LinuxPlatform {
    type Factory = GtkFactory;
    type Renderer = (); // simplificado
    type EventLoop = ();

    fn init() -> Self {
        LinuxPlatform { factory: GtkFactory }
    }

    fn factory(&self) -> &GtkFactory {
        &self.factory
    }

    fn renderer(&self) -> &() { &() }
    fn event_loop(&mut self) -> &mut () { &mut () }
}

// Codigo generico sobre toda la plataforma:
fn run_app<P: Platform>(platform: &P)
where
    P::Factory: GuiFactory,
{
    let factory = platform.factory();
    let btn = factory.create_button("Start");
    btn.render();
}
```

Esto es un **type family**: un trait que agrupa multiples tipos
relacionados. Es el Abstract Factory elevado al sistema de tipos.

---

## 9. Segundo ejemplo: database backends

```rust
trait DbConnection {
    fn execute(&self, sql: &str) -> Result<u64, String>;
    fn query(&self, sql: &str) -> Result<Vec<Vec<String>>, String>;
}

trait DbStatement {
    fn bind_int(&mut self, index: usize, value: i64);
    fn bind_str(&mut self, index: usize, value: &str);
    fn execute(&mut self) -> Result<u64, String>;
}

// Abstract Factory con associated types
trait DbFactory {
    type Conn: DbConnection;
    type Stmt: DbStatement;

    fn name(&self) -> &str;
    fn connect(&self, dsn: &str) -> Result<Self::Conn, String>;
}

// ── SQLite ──────────────────────────────────────────
struct SqliteConnection {
    path: String,
}

impl DbConnection for SqliteConnection {
    fn execute(&self, sql: &str) -> Result<u64, String> {
        println!("[sqlite:{}] exec: {}", self.path, sql);
        Ok(1)
    }

    fn query(&self, sql: &str) -> Result<Vec<Vec<String>>, String> {
        println!("[sqlite:{}] query: {}", self.path, sql);
        Ok(vec![vec!["row1".into()]])
    }
}

struct SqliteFactory;

impl DbFactory for SqliteFactory {
    type Conn = SqliteConnection;
    type Stmt = ();  // simplificado

    fn name(&self) -> &str { "SQLite" }

    fn connect(&self, dsn: &str) -> Result<SqliteConnection, String> {
        println!("[sqlite] connecting to {dsn}");
        Ok(SqliteConnection { path: dsn.to_string() })
    }
}

// ── Mock (para tests) ───────────────────────────────
struct MockConnection {
    calls: std::cell::RefCell<Vec<String>>,
}

impl DbConnection for MockConnection {
    fn execute(&self, sql: &str) -> Result<u64, String> {
        self.calls.borrow_mut().push(sql.to_string());
        Ok(0)
    }

    fn query(&self, sql: &str) -> Result<Vec<Vec<String>>, String> {
        self.calls.borrow_mut().push(sql.to_string());
        Ok(vec![])
    }
}

struct MockFactory;

impl DbFactory for MockFactory {
    type Conn = MockConnection;
    type Stmt = ();

    fn name(&self) -> &str { "Mock" }

    fn connect(&self, _dsn: &str) -> Result<MockConnection, String> {
        Ok(MockConnection {
            calls: std::cell::RefCell::new(Vec::new()),
        })
    }
}

// Codigo generico sobre cualquier backend:
fn migrate<F: DbFactory>(factory: &F, dsn: &str) -> Result<(), String> {
    let conn = factory.connect(dsn)?;
    conn.execute("CREATE TABLE users (id INT, name TEXT)")?;
    conn.execute("CREATE TABLE posts (id INT, user_id INT)")?;
    println!("Migration complete on {}", factory.name());
    Ok(())
}

fn main() -> Result<(), String> {
    // Produccion:
    migrate(&SqliteFactory, "app.db")?;

    // Tests:
    migrate(&MockFactory, "test")?;

    Ok(())
}
```

```
  Associated types aqui:
  - SqliteFactory::Conn = SqliteConnection (tipo concreto)
  - MockFactory::Conn = MockConnection (tipo concreto)
  - migrate<F: DbFactory> trabaja con F::Conn sin saber cual es
  - Sin Box, sin allocation extra, dispatch estatico
```

---

## 10. Comparacion completa: C vs Rust

```
  Aspecto               C (T03)                Rust dyn            Rust assoc types
  ───────               ───────                ────────            ────────────────
  Factory interface     struct de fn ptrs      trait               trait con type
  Product interface     struct de fn ptrs      trait               trait
  Concrete factory      implementar vtable     impl trait          impl trait
  Productos             malloc + init          Box::new            stack (sin alloc)
  Dispatch productos    vtable runtime         vtable runtime      estatico (inline)
  Dispatch factory      vtable runtime         vtable runtime      estatico (mono)
  Familia en runtime    SI                     SI                  NO (compile time)
  Vec<Factory>          SI (Factory*)          SI (Box<dyn>)       NO (tipos distintos)
  Agregar familia       facil (archivos)       facil               facil
  Agregar producto      dificil (todas vt)     dificil (trait)     dificil (assoc type)
  Destruccion           manual                 Drop automatico     Drop automatico
  Consistencia          convencion             tipo sistema        tipo sistema
  Completitud           runtime (NULL check)   compile time        compile time
  Boilerplate           alto (~400 lineas)     medio (~200)        bajo (~150)
  Hot path viable       no (indirecciones)     no (Box + vtable)   SI (zero-cost)
```

### 10.1 El trade-off es el mismo

Independientemente del lenguaje:

```
  Agregar FAMILIA (nueva plataforma) = FACIL
  ├─ Crear nuevos tipos + nueva factory
  └─ No tocar codigo existente

  Agregar PRODUCTO (nuevo widget) = DIFICIL
  ├─ Modificar la interfaz de factory
  └─ Actualizar TODAS las concrete factories
```

Rust mitiga la parte "dificil": si agregas un metodo al trait
`GuiFactory`, el compilador te fuerza a implementarlo en todas
las concrete factories. En C, si olvidas un puntero en una vtable,
es un crash en runtime.

### 10.2 Cuando elegir cada forma

```
  ¿Familia elegida en runtime?
  ├─ SI → dyn Trait (ambos lenguajes)
  │       C: vtable manual
  │       Rust: trait + Box<dyn>
  │
  └─ NO → ¿Hot path?
          ├─ SI → Associated types (solo Rust)
          │       C: no tiene equivalente real (macros simulan)
          │
          └─ NO → dyn Trait (mas simple, suficiente)
```

---

## Errores comunes

### Error 1 — Associated types con familia en runtime

```rust
// ESTO NO FUNCIONA:
fn get_factory(name: &str) -> impl GuiFactory {
    match name {
        "gtk"   => GtkFactory,    // tipo: GtkFactory
        "win32" => Win32Factory,  // tipo: Win32Factory ← DISTINTO
        _ => panic!(),
    }
    // ERROR: match arms have incompatible types
}
```

`impl Trait` y associated types requieren UN tipo concreto.
Si la familia se elige en runtime, necesitas `Box<dyn GuiFactory>`
(Forma 1).

### Error 2 — dyn Trait factory no es object-safe con associated types

```rust
// ESTE trait NO es object-safe:
trait GuiFactory {
    type Btn: Button;
    fn create_button(&self, label: &str) -> Self::Btn;
}

// No puedes hacer Box<dyn GuiFactory> porque Self::Btn
// tiene tamano desconocido en compile time.

// Solucion: un trait separado para dyn
trait DynGuiFactory {
    fn create_button(&self, label: &str) -> Box<dyn Button>;
}

// O: blanket impl
impl<T: GuiFactory> DynGuiFactory for T {
    fn create_button(&self, label: &str) -> Box<dyn Button> {
        Box::new(GuiFactory::create_button(self, label))
    }
}
```

### Error 3 — Mezclar familias con dyn

```rust
// El tipo sistema NO previene esto con dyn:
let gtk = GtkFactory;
let win = Win32Factory;

let btn: Box<dyn Button> = gtk.create_button("OK");     // GTK
let txt: Box<dyn TextBox> = win.create_textbox();        // Win32!
// Compilar OK — pero semanticamente incorrecto

// Con associated types, esto es IMPOSIBLE:
fn build<F: GuiFactory>(f: &F) {
    let btn = f.create_button("OK");  // F::Btn
    let txt = f.create_textbox();     // F::Txt
    // Ambos vienen de la misma F — consistencia garantizada
}
```

Con dyn, la consistencia depende de disciplina (pasar UNA factory).
Con associated types, el compilador la garantiza.

### Error 4 — Over-engineering: Abstract Factory para una sola familia

```rust
// Si solo tienes GTK y nunca tendras otra familia:
trait GuiFactory { ... }
struct GtkFactory;
impl GuiFactory for GtkFactory { ... }

// Overkill. Solo usa los tipos directamente:
let btn = GtkButton::new("OK");
let txt = GtkTextBox::new();

// Agrega la abstraccion CUANDO necesites la segunda familia.
```

---

## Ejercicios

### Ejercicio 1 — Factory basica con dyn

Implementa la Abstract Factory con dyn Trait para dos familias
(GTK y Terminal) con dos productos (Button y TextBox). El
cliente (`build_form`) no debe conocer la familia concreta.

**Prediccion**: Cuantos traits necesitas definir?

<details><summary>Respuesta</summary>

Tres traits: `Button`, `TextBox`, `GuiFactory`.

```rust
trait Button {
    fn render(&self);
    fn label(&self) -> &str;
}

trait TextBox {
    fn render(&self);
    fn set_text(&mut self, text: &str);
    fn get_text(&self) -> &str;
}

trait GuiFactory {
    fn create_button(&self, label: &str) -> Box<dyn Button>;
    fn create_textbox(&self) -> Box<dyn TextBox>;
}

// ── GTK ──
struct GtkButton { label: String }
impl Button for GtkButton {
    fn render(&self) { println!("[GTK: {}]", self.label); }
    fn label(&self) -> &str { &self.label }
}
struct GtkTextBox { text: String }
impl TextBox for GtkTextBox {
    fn render(&self) { println!("[GTK: \"{}\"]", self.text); }
    fn set_text(&mut self, t: &str) { self.text = t.into(); }
    fn get_text(&self) -> &str { &self.text }
}
struct GtkFactory;
impl GuiFactory for GtkFactory {
    fn create_button(&self, l: &str) -> Box<dyn Button> {
        Box::new(GtkButton { label: l.into() })
    }
    fn create_textbox(&self) -> Box<dyn TextBox> {
        Box::new(GtkTextBox { text: String::new() })
    }
}

// ── Terminal ──
struct TermButton { label: String }
impl Button for TermButton {
    fn render(&self) { println!("[btn:{}]", self.label); }
    fn label(&self) -> &str { &self.label }
}
struct TermTextBox { text: String }
impl TextBox for TermTextBox {
    fn render(&self) { println!("[txt:\"{}\"]", self.text); }
    fn set_text(&mut self, t: &str) { self.text = t.into(); }
    fn get_text(&self) -> &str { &self.text }
}
struct TermFactory;
impl GuiFactory for TermFactory {
    fn create_button(&self, l: &str) -> Box<dyn Button> {
        Box::new(TermButton { label: l.into() })
    }
    fn create_textbox(&self) -> Box<dyn TextBox> {
        Box::new(TermTextBox { text: String::new() })
    }
}

// ── Cliente ──
fn build_form(factory: &dyn GuiFactory) {
    let mut name = factory.create_textbox();
    name.set_text("Alice");
    let ok = factory.create_button("OK");
    name.render();
    ok.render();
}

fn main() {
    build_form(&GtkFactory);
    build_form(&TermFactory);
}
```

</details>

---

### Ejercicio 2 — Factory con associated types

Reescribe el ejercicio 1 usando associated types. El cliente
`build_form` debe ser generico (`fn build_form<F: GuiFactory>`).

**Prediccion**: Puedes almacenar `GtkFactory` y `TermFactory`
en un mismo `Vec`?

<details><summary>Respuesta</summary>

```rust
trait GuiFactory {
    type Btn: Button;
    type Txt: TextBox;

    fn create_button(&self, label: &str) -> Self::Btn;
    fn create_textbox(&self) -> Self::Txt;
}

impl GuiFactory for GtkFactory {
    type Btn = GtkButton;
    type Txt = GtkTextBox;

    fn create_button(&self, l: &str) -> GtkButton {
        GtkButton { label: l.into() }
    }
    fn create_textbox(&self) -> GtkTextBox {
        GtkTextBox { text: String::new() }
    }
}

impl GuiFactory for TermFactory {
    type Btn = TermButton;
    type Txt = TermTextBox;

    fn create_button(&self, l: &str) -> TermButton {
        TermButton { label: l.into() }
    }
    fn create_textbox(&self) -> TermTextBox {
        TermTextBox { text: String::new() }
    }
}

fn build_form<F: GuiFactory>(factory: &F) {
    let mut name = factory.create_textbox();
    name.set_text("Alice");
    let ok = factory.create_button("OK");
    name.render();
    ok.render();
}

fn main() {
    build_form(&GtkFactory);
    build_form(&TermFactory);
}
```

**No**, no puedes poner `GtkFactory` y `TermFactory` en un mismo
`Vec` porque son tipos distintos. `Vec<T>` exige que todos los
elementos sean del mismo `T`.

Para un Vec heterogeneo de factories necesitas:
- `Vec<Box<dyn DynGuiFactory>>` (volver a dyn)
- Un enum `AnyFactory { Gtk(GtkFactory), Term(TermFactory) }`

Esta es la limitacion fundamental: associated types = compile
time = sin heterogeneidad.

</details>

---

### Ejercicio 3 — Blanket impl: de associated a dyn

Escribe un trait `DynGuiFactory` con `Box<dyn Button>` como
retorno, y un blanket impl que lo implemente automaticamente
para cualquier `T: GuiFactory`:

```rust
impl<T: GuiFactory> DynGuiFactory for T { ... }
```

**Prediccion**: Despues del blanket impl, puedes usar
`Box<dyn DynGuiFactory>` con cualquier concrete factory?

<details><summary>Respuesta</summary>

```rust
// Trait para uso dinamico
trait DynGuiFactory {
    fn name(&self) -> &str;
    fn create_button_dyn(&self, label: &str) -> Box<dyn Button>;
    fn create_textbox_dyn(&self) -> Box<dyn TextBox>;
}

// Blanket impl: cualquier GuiFactory (con assoc types) tambien
// implementa DynGuiFactory (con Box<dyn>)
impl<T> DynGuiFactory for T
where
    T: GuiFactory,
    T::Btn: Button + 'static,
    T::Txt: TextBox + 'static,
{
    fn name(&self) -> &str { "factory" }

    fn create_button_dyn(&self, label: &str) -> Box<dyn Button> {
        Box::new(self.create_button(label))
    }

    fn create_textbox_dyn(&self) -> Box<dyn TextBox> {
        Box::new(self.create_textbox())
    }
}

// Ahora puedes usar ambas formas:
fn static_path<F: GuiFactory>(f: &F) {
    let btn = f.create_button("OK");  // tipo concreto, stack
    btn.render();
}

fn dynamic_path(f: &dyn DynGuiFactory) {
    let btn = f.create_button_dyn("OK");  // Box<dyn Button>, heap
    btn.render();
}

fn main() {
    let gtk = GtkFactory;

    // Estatico:
    static_path(&gtk);

    // Dinamico (via blanket impl):
    dynamic_path(&gtk);

    // Vec heterogeneo:
    let factories: Vec<Box<dyn DynGuiFactory>> = vec![
        Box::new(GtkFactory),
        Box::new(TermFactory),
    ];
    for f in &factories {
        dynamic_path(&**f);
    }
}
```

Si, despues del blanket impl, cualquier tipo que implemente
`GuiFactory` (con associated types) automaticamente implementa
`DynGuiFactory` (con Box<dyn>). Esto te da lo mejor de ambos
mundos: dispatch estatico cuando quieres rendimiento, y dispatch
dinamico cuando necesitas runtime polymorphism. Todo desde una
sola definicion de factory.

</details>

---

### Ejercicio 4 — Database factory con tests

Implementa la abstract factory de base de datos (seccion 9) con
`SqliteFactory` y `MockFactory`. Escribe un test que use
`MockFactory` para verificar que `migrate()` ejecuta exactamente
2 sentencias SQL.

**Prediccion**: Por que el test no necesita SQLite instalado?

<details><summary>Respuesta</summary>

```rust
trait DbConnection {
    fn execute(&self, sql: &str) -> Result<u64, String>;
}

trait DbFactory {
    type Conn: DbConnection;
    fn connect(&self, dsn: &str) -> Result<Self::Conn, String>;
}

// SQLite (produccion)
struct SqliteConn { path: String }
impl DbConnection for SqliteConn {
    fn execute(&self, sql: &str) -> Result<u64, String> {
        println!("[sqlite:{}] {}", self.path, sql);
        Ok(1)
    }
}
struct SqliteFactory;
impl DbFactory for SqliteFactory {
    type Conn = SqliteConn;
    fn connect(&self, dsn: &str) -> Result<SqliteConn, String> {
        Ok(SqliteConn { path: dsn.into() })
    }
}

// Mock (tests)
use std::cell::RefCell;

struct MockConn {
    log: RefCell<Vec<String>>,
}
impl DbConnection for MockConn {
    fn execute(&self, sql: &str) -> Result<u64, String> {
        self.log.borrow_mut().push(sql.to_string());
        Ok(0)
    }
}
impl MockConn {
    fn call_count(&self) -> usize {
        self.log.borrow().len()
    }
    fn calls(&self) -> Vec<String> {
        self.log.borrow().clone()
    }
}

struct MockFactory;
impl DbFactory for MockFactory {
    type Conn = MockConn;
    fn connect(&self, _dsn: &str) -> Result<MockConn, String> {
        Ok(MockConn { log: RefCell::new(Vec::new()) })
    }
}

// Logica de negocio (generica sobre cualquier backend)
fn migrate<F: DbFactory>(factory: &F, dsn: &str) -> Result<F::Conn, String> {
    let conn = factory.connect(dsn)?;
    conn.execute("CREATE TABLE users (id INT, name TEXT)")?;
    conn.execute("CREATE TABLE posts (id INT, user_id INT)")?;
    Ok(conn)
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn migrate_executes_two_statements() {
        let conn = migrate(&MockFactory, "test").unwrap();
        assert_eq!(conn.call_count(), 2);
        assert!(conn.calls()[0].contains("users"));
        assert!(conn.calls()[1].contains("posts"));
    }
}
```

El test no necesita SQLite porque `MockFactory` crea `MockConn`,
que simula la base de datos en memoria (un Vec de strings).
`migrate()` es generica — no sabe si habla con SQLite o Mock.
El tipo sistema garantiza que funciona con ambos.

Ademas, con associated types el mock connection retorna
`MockConn` (tipo concreto), lo que permite acceder a
`call_count()` y `calls()` sin downcasting. Con `dyn DbConnection`
necesitarias `Any` o un trait adicional.

</details>

---

### Ejercicio 5 — Factory con theming

Crea `ThemedGtkFactory` que tome un theme name y font size, y
los pase a todos los widgets creados. Todos los widgets deben
mostrar el tema al renderizar.

**Prediccion**: Es mejor clonar el theme a cada widget o pasar
una referencia?

<details><summary>Respuesta</summary>

```rust
struct ThemedGtkFactory {
    theme: String,
    font_size: u32,
}

struct ThemedButton {
    label: String,
    theme: String,
    font_size: u32,
}

impl Button for ThemedButton {
    fn render(&self) {
        println!("[GTK {} {}px: {}]",
                 self.theme, self.font_size, self.label);
    }
    fn label(&self) -> &str { &self.label }
}

struct ThemedTextBox {
    text: String,
    theme: String,
    font_size: u32,
}

impl TextBox for ThemedTextBox {
    fn render(&self) {
        println!("[GTK {} {}px: \"{}\"]",
                 self.theme, self.font_size, self.text);
    }
    fn set_text(&mut self, t: &str) { self.text = t.into(); }
    fn get_text(&self) -> &str { &self.text }
}

impl GuiFactory for ThemedGtkFactory {
    fn name(&self) -> &str { "GTK Themed" }

    fn create_button(&self, label: &str) -> Box<dyn Button> {
        Box::new(ThemedButton {
            label: label.into(),
            theme: self.theme.clone(),
            font_size: self.font_size,
        })
    }

    fn create_textbox(&self) -> Box<dyn TextBox> {
        Box::new(ThemedTextBox {
            text: String::new(),
            theme: self.theme.clone(),
            font_size: self.font_size,
        })
    }

    fn create_checkbox(&self, initial: bool) -> Box<dyn Checkbox> {
        Box::new(GtkCheckbox { checked: initial })
    }
}

fn main() {
    let dark = ThemedGtkFactory {
        theme: "dark".into(),
        font_size: 14,
    };
    let light = ThemedGtkFactory {
        theme: "light".into(),
        font_size: 12,
    };

    let b1 = dark.create_button("OK");
    let b2 = light.create_button("OK");
    b1.render();  // [GTK dark 14px: OK]
    b2.render();  // [GTK light 12px: OK]
}
```

Es mejor **clonar** el theme (owned String) porque:

1. El widget debe vivir independiente de la factory. Si la
   factory se destruye, los widgets deben seguir validos.
2. Con una referencia (`&str`), el widget tendria un lifetime
   atado a la factory: `ThemedButton<'a>`. Esto complica todo
   (no puedes retornar `Box<dyn Button + 'a>` facilmente).
3. Clonar un String corto ("dark") es ~30 ns — irrelevante
   comparado con renderizar un widget.

Si el theme fuera grande e inmutable, podrias usar `Arc<String>`
para compartir sin copiar.

</details>

---

### Ejercicio 6 — Enum como abstract factory

Implementa la abstract factory usando un enum de familias en
lugar de traits:

```rust
enum Toolkit { Gtk, Win32, Terminal }

impl Toolkit {
    fn create_button(&self, label: &str) -> AnyButton { ... }
    fn create_textbox(&self) -> AnyTextBox { ... }
}
```

**Prediccion**: Que se pierde comparado con trait-based factory?
Que se gana?

<details><summary>Respuesta</summary>

```rust
#[derive(Debug)]
enum AnyButton {
    Gtk { label: String },
    Win32 { label: String, hwnd: u32 },
    Term { label: String },
}

impl AnyButton {
    fn render(&self) {
        match self {
            AnyButton::Gtk { label } =>
                println!("[GTK: {label}]"),
            AnyButton::Win32 { label, hwnd } =>
                println!("<Win32 hwnd={hwnd}: {label}>"),
            AnyButton::Term { label } =>
                println!("[btn:{label}]"),
        }
    }
}

#[derive(Debug)]
enum AnyTextBox {
    Gtk { text: String },
    Win32 { text: String, hwnd: u32 },
    Term { text: String },
}

impl AnyTextBox {
    fn render(&self) {
        match self {
            AnyTextBox::Gtk { text } =>
                println!("[GTK: \"{text}\"]"),
            AnyTextBox::Win32 { text, hwnd } =>
                println!("<Win32 hwnd={hwnd}: \"{text}\">"),
            AnyTextBox::Term { text } =>
                println!("[txt:\"{text}\"]"),
        }
    }

    fn set_text(&mut self, new_text: &str) {
        match self {
            AnyTextBox::Gtk { text }
            | AnyTextBox::Win32 { text, .. }
            | AnyTextBox::Term { text } => {
                *text = new_text.into();
            }
        }
    }
}

#[derive(Debug, Clone, Copy)]
enum Toolkit { Gtk, Win32, Terminal }

impl Toolkit {
    fn create_button(self, label: &str) -> AnyButton {
        match self {
            Toolkit::Gtk =>
                AnyButton::Gtk { label: label.into() },
            Toolkit::Win32 =>
                AnyButton::Win32 { label: label.into(), hwnd: 0 },
            Toolkit::Terminal =>
                AnyButton::Term { label: label.into() },
        }
    }

    fn create_textbox(self) -> AnyTextBox {
        match self {
            Toolkit::Gtk =>
                AnyTextBox::Gtk { text: String::new() },
            Toolkit::Win32 =>
                AnyTextBox::Win32 { text: String::new(), hwnd: 0 },
            Toolkit::Terminal =>
                AnyTextBox::Term { text: String::new() },
        }
    }
}

fn build_form(toolkit: Toolkit) {
    let mut name = toolkit.create_textbox();
    name.set_text("Alice");
    let ok = toolkit.create_button("OK");
    name.render();
    ok.render();
}
```

**Se pierde:**
- Extensibilidad: agregar toolkit requiere modificar TODOS los
  enums (AnyButton, AnyTextBox) y TODOS sus match arms.
- Separation of concerns: todo el codigo de todas las familias
  vive junto, no en archivos separados.

**Se gana:**
- Sin allocation: todo en stack (sin Box)
- Sin traits: codigo mas simple y directo
- Exhaustive matching: el compilador avisa si falta una familia
- Rendimiento: branch dispatch, no vtable indirection
- Facil de serializar (derive Serialize)

Usar cuando: las familias son pocas, fijas, y no crecen.
Tipico en: aplicaciones internas, prototipos, configuraciones.

</details>

---

### Ejercicio 7 — Type family pattern

Escribe un trait `Platform` que agrupe factory, renderer, y
event source como associated types. Implementa para Linux y
Windows (simplificado).

**Prediccion**: Cuantos tipos concretos necesitas por plataforma?

<details><summary>Respuesta</summary>

```rust
trait Renderer {
    fn clear(&self);
    fn draw_rect(&self, x: f64, y: f64, w: f64, h: f64);
}

trait EventSource {
    fn poll_event(&self) -> Option<String>;
}

trait Platform {
    type Factory: GuiFactory;
    type Render: Renderer;
    type Events: EventSource;

    fn factory(&self) -> &Self::Factory;
    fn renderer(&self) -> &Self::Render;
    fn events(&self) -> &Self::Events;
}

// ── Linux ──
struct X11Renderer;
impl Renderer for X11Renderer {
    fn clear(&self) { println!("[X11] clear"); }
    fn draw_rect(&self, x: f64, y: f64, w: f64, h: f64) {
        println!("[X11] rect at ({x},{y}) {w}x{h}");
    }
}

struct X11Events;
impl EventSource for X11Events {
    fn poll_event(&self) -> Option<String> {
        Some("X11:ButtonPress".into())
    }
}

struct LinuxPlatform {
    factory: GtkFactory,
    renderer: X11Renderer,
    events: X11Events,
}

impl Platform for LinuxPlatform {
    type Factory = GtkFactory;
    type Render = X11Renderer;
    type Events = X11Events;

    fn factory(&self) -> &GtkFactory { &self.factory }
    fn renderer(&self) -> &X11Renderer { &self.renderer }
    fn events(&self) -> &X11Events { &self.events }
}

// ── Windows ──
struct D3DRenderer;
impl Renderer for D3DRenderer {
    fn clear(&self) { println!("[D3D] clear"); }
    fn draw_rect(&self, x: f64, y: f64, w: f64, h: f64) {
        println!("[D3D] rect at ({x},{y}) {w}x{h}");
    }
}

struct WinEvents;
impl EventSource for WinEvents {
    fn poll_event(&self) -> Option<String> {
        Some("WM_LBUTTONDOWN".into())
    }
}

struct WindowsPlatform {
    factory: Win32Factory,
    renderer: D3DRenderer,
    events: WinEvents,
}

impl Platform for WindowsPlatform {
    type Factory = Win32Factory;
    type Render = D3DRenderer;
    type Events = WinEvents;

    fn factory(&self) -> &Win32Factory { &self.factory }
    fn renderer(&self) -> &D3DRenderer { &self.renderer }
    fn events(&self) -> &WinEvents { &self.events }
}

// Generico sobre la plataforma
fn run_app<P: Platform>(platform: &P)
where
    P::Factory: GuiFactory,
{
    platform.renderer().clear();
    let btn = platform.factory().create_button("Start");
    btn.render();
    if let Some(event) = platform.events().poll_event() {
        println!("Event: {event}");
    }
}
```

Por plataforma necesitas:
- 1 factory (GtkFactory / Win32Factory)
- 1 renderer (X11Renderer / D3DRenderer)
- 1 event source (X11Events / WinEvents)
- 1 platform struct que los agrupa
- N widgets (de ejercicios anteriores)

Total: ~4-5 tipos concretos por plataforma (sin contar widgets).

El trait `Platform` es un meta-factory: una factory de factories.
Agrupa toda una "familia de familias" en un solo type parameter.

</details>

---

### Ejercicio 8 — Comparar boilerplate

Cuenta las lineas de codigo (aproximadas) para implementar
Abstract Factory con 2 familias y 2 productos en:

1. C (vtables manuales, como T03)
2. Rust dyn Trait
3. Rust associated types

**Prediccion**: Cual tiene mas lineas? Cual tiene mas
seguridad en compile time?

<details><summary>Respuesta</summary>

Estimacion para 2 familias (GTK, Win32) × 2 productos (Button,
TextBox):

| Componente | C | Rust dyn | Rust assoc |
|-----------|-----|---------|-----------|
| Product interfaces (widget.h) | ~40 | ~15 | ~15 |
| Factory interface | ~25 | ~6 | ~8 |
| GTK products (3 structs + vtables) | ~80 | ~30 | ~30 |
| Win32 products | ~80 | ~30 | ~30 |
| GTK factory | ~30 | ~15 | ~15 |
| Win32 factory | ~30 | ~15 | ~15 |
| Selector | ~15 | ~8 | ~5 |
| Cliente | ~20 | ~15 | ~15 |
| **Total** | **~320** | **~134** | **~133** |

| Seguridad | C | Rust dyn | Rust assoc |
|-----------|---|----------|-----------|
| Metodos completos | No (NULL en vtable) | Si (compile error) | Si |
| Tipo correcto de retorno | No (void* cast) | Si (trait) | Si |
| Destruccion | Manual (olvidable) | Automatica | Automatica |
| Consistencia familias | No (mezcla posible) | No (mezcla posible) | Si (type param) |
| NULL safety | No | Si (Option) | Si |

Rust associated types tiene la misma cantidad de lineas que dyn,
pero ofrece la mayor seguridad (garantiza consistencia de familia
a nivel de tipo sistema).

C tiene ~2.4x mas codigo y menos safety en cada dimension.

</details>

---

### Ejercicio 9 — Factory async

Adapta la database factory para que `connect` sea async (simula
una conexion de red):

```rust
#[async_trait::async_trait]
trait DbFactory {
    async fn connect(&self, dsn: &str) -> Result<Box<dyn DbConnection>, String>;
}
```

**Prediccion**: Puedes usar associated types con async?

<details><summary>Respuesta</summary>

```rust
// Con el crate async-trait (o Rust nightly con RPITIT):
use async_trait::async_trait;

#[async_trait]
trait DbConnection: Send + Sync {
    async fn execute(&self, sql: &str) -> Result<u64, String>;
}

#[async_trait]
trait DbFactory: Send + Sync {
    async fn connect(&self, dsn: &str)
        -> Result<Box<dyn DbConnection>, String>;
}

// PostgreSQL
struct PgConn { /* ... */ }

#[async_trait]
impl DbConnection for PgConn {
    async fn execute(&self, sql: &str) -> Result<u64, String> {
        // Simulacion de query async
        tokio::time::sleep(std::time::Duration::from_millis(1)).await;
        println!("[pg] {sql}");
        Ok(1)
    }
}

struct PgFactory { host: String }

#[async_trait]
impl DbFactory for PgFactory {
    async fn connect(&self, dsn: &str)
        -> Result<Box<dyn DbConnection>, String>
    {
        println!("[pg] connecting to {}...", self.host);
        tokio::time::sleep(std::time::Duration::from_millis(10)).await;
        Ok(Box::new(PgConn { /* ... */ }))
    }
}

// Codigo generico async
async fn migrate(factory: &dyn DbFactory, dsn: &str) -> Result<(), String> {
    let conn = factory.connect(dsn).await?;
    conn.execute("CREATE TABLE users (id INT)").await?;
    Ok(())
}
```

Sobre associated types con async: **si, pero con restricciones.**

En Rust estable con `async-trait` crate, los metodos async se
convierten en `-> Pin<Box<dyn Future>>` (allocation). Los
associated types funcionan pero los productos deben ser `Send`.

En nightly/futuro Rust con RPITIT (Return Position Impl Trait In
Traits), sera posible sin allocation:

```rust
trait DbFactory {
    type Conn: DbConnection;
    // RPITIT: sin Box, sin allocation
    fn connect(&self, dsn: &str) -> impl Future<Output = Result<Self::Conn, String>>;
}
```

Esto dara async abstract factories completamente zero-cost.

</details>

---

### Ejercicio 10 — Reflexion: Abstract Factory en Rust vs C

Responde sin ver la respuesta:

1. Cual es la ventaja mas importante de associated types sobre
   dyn Trait para abstract factory?

2. En un proyecto real, la mayoria de abstract factories serian
   dyn o associated types? Por que?

3. El blanket impl (`impl<T: GuiFactory> DynGuiFactory for T`)
   es un patron que elimina la necesidad de elegir entre dyn y
   associated types. Explica como.

4. El Abstract Factory tiene el mismo trade-off en ambos
   lenguajes (familias faciles, productos dificiles). Que
   herramienta de Rust mitiga la parte "dificil"?

<details><summary>Respuesta</summary>

**1. Ventaja de associated types:**

**Consistencia de familia garantizada por el compilador.** Con
`fn build<F: GuiFactory>(f: &F)`, todos los productos creados
vienen del mismo `F`. Es imposible mezclar GtkButton con
Win32TextBox — el type system lo prohibe. Con `dyn`, la
consistencia depende de disciplina del programador.

Ademas: zero-cost dispatch y sin allocation (stack). Pero la
consistencia es la ventaja *unica* — el rendimiento se puede
obtener con otras tecnicas.

**2. La mayoria serian dyn.**

Porque la familia casi siempre se elige en runtime (config file,
variable de entorno, user preference). Associated types requieren
que el tipo sea conocido en compile time, lo cual limita su uso
a binarios compilados para una plataforma especifica.

Excepciones donde associated types brillan:
- Embedded (una sola plataforma por compilacion)
- Game engines (renderer elegido en compile time con features)
- Tests (el tipo `MockFactory` se conoce en compile time)

**3. Blanket impl:**

El blanket impl convierte automaticamente cualquier factory con
associated types en una factory dyn. Defines tus tipos con
associated types (maximo rendimiento y type safety), y el blanket
impl te da la version dyn gratis:

```rust
// Defines una vez:
impl<T: GuiFactory> DynGuiFactory for T { ... }

// Usas lo que necesites:
static_path(&GtkFactory);           // associated types, zero-cost
dynamic_path(&GtkFactory as &dyn DynGuiFactory);  // dyn, runtime
```

No necesitas elegir: defines con associated types y usas dyn
cuando necesitas runtime polymorphism. Lo mejor de ambos mundos.

**4. Herramienta que mitiga la parte dificil:**

El compilador de Rust con **exhaustive implementation checking**.

Si agregas un metodo al trait `GuiFactory`:
```rust
trait GuiFactory {
    // ... existentes ...
    fn create_slider(&self, min: i32, max: i32) -> Box<dyn Slider>;  // NUEVO
}
```

El compilador produce un error en **cada** concrete factory que
no implemente `create_slider`. No puedes olvidarlo — no compila.

En C, si olvidas agregar el function pointer a una vtable, el
puntero queda NULL y tienes un crash en runtime. Rust convierte
este bug de runtime en un error de compilacion.

Ademas, con default methods puedes agregar productos sin romper
las factories existentes:
```rust
fn create_slider(&self, _min: i32, _max: i32) -> Box<dyn Slider> {
    unimplemented!("slider not supported on this platform")
}
```

</details>
