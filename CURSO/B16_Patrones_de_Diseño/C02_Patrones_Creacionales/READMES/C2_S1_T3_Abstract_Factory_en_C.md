# T03 — Abstract Factory en C

---

## 1. De Simple Factory a Abstract Factory

T01 implemento una Simple Factory: **una funcion** que crea
objetos de una familia. Pero tiene una limitacion: la funcion
decide internamente que tipos usar.

```c
/* Simple Factory: una sola familia de shapes */
Shape *shape_create(ShapeType type, ...);

/* ¿Y si necesitas crear FAMILIAS de objetos relacionados?
   Por ejemplo: widgets de GUI que varian por plataforma. */
```

El **Abstract Factory** resuelve un problema diferente: crear
**familias de objetos relacionados** sin especificar las clases
concretas. La factory misma es un objeto con metodos de creacion.

```
  Simple Factory:
  ┌────────────────────┐
  │ shape_create(type) │──→ Circle | Rect | Triangle
  └────────────────────┘
  UNA funcion, elige tipo por parametro.

  Abstract Factory:
  ┌─────────────────────────────┐
  │ GuiFactory                  │
  │ ├─ create_button()          │──→ Button
  │ ├─ create_textbox()         │──→ TextBox
  │ └─ create_checkbox()        │──→ Checkbox
  └─────────────────────────────┘
  UN OBJETO con multiples metodos de creacion.
  Cada metodo crea un tipo diferente de la MISMA familia.
```

---

## 2. El problema que resuelve

Imagina un toolkit de GUI que debe funcionar en Linux (GTK),
macOS (Cocoa), y Windows (Win32). Cada plataforma tiene sus
propias implementaciones de Button, TextBox, Checkbox, etc.

```
  Sin Abstract Factory:

  if (platform == LINUX) {
      Button *b = gtk_button_new("OK");
      TextBox *t = gtk_textbox_new();
  } else if (platform == MACOS) {
      Button *b = cocoa_button_new("OK");
      TextBox *t = cocoa_textbox_new();
  } else if (platform == WINDOWS) {
      Button *b = win32_button_new("OK");
      TextBox *t = win32_textbox_new();
  }

  Problemas:
  1. if/else por CADA widget creado (se repite por todo el codigo)
  2. Si agregas una plataforma, modificas TODOS los puntos de creacion
  3. Riesgo de mezclar: gtk_button con cocoa_textbox
```

```
  Con Abstract Factory:

  GuiFactory *factory = get_factory_for_platform();

  // El codigo de la aplicacion NO sabe la plataforma
  Button *b = factory->vt->create_button(factory, "OK");
  TextBox *t = factory->vt->create_textbox(factory);

  // Todos los widgets son de la misma familia (imposible mezclar)
```

### 2.1 Diferencia fundamental

```
  Simple Factory:
  - Crea objetos de UN tipo (Shape) con diferentes variantes
  - El parametro decide la variante
  - UNA interfaz de producto

  Abstract Factory:
  - Crea objetos de MULTIPLES tipos (Button, TextBox, Checkbox)
  - La factory decide la familia completa
  - MULTIPLES interfaces de producto
  - Garantiza consistencia entre productos

  Analogia:
  Simple Factory  = una maquina expendedora (un tipo de producto)
  Abstract Factory = una fabrica (produce toda una linea de productos)
```

---

## 3. Anatomia del patron

### 3.1 Componentes

```
  1. Abstract products:   Interfaces de cada producto (Button, TextBox)
  2. Concrete products:   Implementaciones por familia (GtkButton, Win32Button)
  3. Abstract factory:    Vtable con metodos de creacion
  4. Concrete factories:  Implementaciones de la factory (GtkFactory, Win32Factory)
  5. Cliente:             Usa la factory sin conocer la familia concreta
```

### 3.2 Diagrama de relaciones

```
  ┌──────────────────────────────────────────────────────────┐
  │ Cliente                                                  │
  │                                                          │
  │ void build_ui(const GuiFactory *f) {                     │
  │     Widget *btn = f->vt->create_button(f, "OK");        │
  │     Widget *txt = f->vt->create_textbox(f);             │
  │     // ... usar widgets polimorficamente                 │
  │ }                                                        │
  └──────────────────────────┬───────────────────────────────┘
                             │ solo conoce
                             ▼
  ┌──────────────────────────────────────────────────────────┐
  │ Abstract Factory (GuiFactoryVt)                          │
  │ ├─ create_button(factory, label) → Widget*              │
  │ ├─ create_textbox(factory)       → Widget*              │
  │ └─ create_checkbox(factory, val) → Widget*              │
  └──────────────┬────────────────────────────┬──────────────┘
                 │                            │
    ┌────────────▼────────────┐  ┌────────────▼────────────┐
    │ GtkFactory              │  │ Win32Factory             │
    │ create_button → GtkBtn  │  │ create_button → Win32Btn │
    │ create_textbox → GtkTxt │  │ create_textbox → Win32Txt│
    │ create_checkbox→ GtkChk │  │ create_checkbox→ Win32Chk│
    └─────────────────────────┘  └─────────────────────────┘
```

---

## 4. Implementacion paso a paso

### 4.1 Abstract products (interfaces)

```c
/* widget.h — interfaces abstractas de productos */
#ifndef WIDGET_H
#define WIDGET_H

#include <stdbool.h>

/* ── Button ──────────────────────────────────────── */
typedef struct Button Button;
typedef struct ButtonVt ButtonVt;

struct ButtonVt {
    void (*render)(const Button *self);
    void (*on_click)(Button *self, void (*callback)(void *ctx), void *ctx);
    void (*destroy)(Button *self);
};

struct Button {
    const ButtonVt *vt;
};

static inline void button_render(const Button *b)  { b->vt->render(b); }
static inline void button_destroy(Button *b)       { if (b) b->vt->destroy(b); }

/* ── TextBox ─────────────────────────────────────── */
typedef struct TextBox TextBox;
typedef struct TextBoxVt TextBoxVt;

struct TextBoxVt {
    void        (*render)(const TextBox *self);
    void        (*set_text)(TextBox *self, const char *text);
    const char *(*get_text)(const TextBox *self);
    void        (*destroy)(TextBox *self);
};

struct TextBox {
    const TextBoxVt *vt;
};

static inline void textbox_render(const TextBox *t)          { t->vt->render(t); }
static inline void textbox_set_text(TextBox *t, const char *s) { t->vt->set_text(t, s); }
static inline const char *textbox_get_text(const TextBox *t) { return t->vt->get_text(t); }
static inline void textbox_destroy(TextBox *t)               { if (t) t->vt->destroy(t); }

/* ── Checkbox ────────────────────────────────────── */
typedef struct Checkbox Checkbox;
typedef struct CheckboxVt CheckboxVt;

struct CheckboxVt {
    void (*render)(const Checkbox *self);
    void (*set_checked)(Checkbox *self, bool val);
    bool (*is_checked)(const Checkbox *self);
    void (*destroy)(Checkbox *self);
};

struct Checkbox {
    const CheckboxVt *vt;
};

static inline void checkbox_render(const Checkbox *c)        { c->vt->render(c); }
static inline void checkbox_set_checked(Checkbox *c, bool v) { c->vt->set_checked(c, v); }
static inline bool checkbox_is_checked(const Checkbox *c)    { return c->vt->is_checked(c); }
static inline void checkbox_destroy(Checkbox *c)             { if (c) c->vt->destroy(c); }

#endif /* WIDGET_H */
```

### 4.2 Abstract factory (interfaz)

```c
/* gui_factory.h — la abstract factory */
#ifndef GUI_FACTORY_H
#define GUI_FACTORY_H

#include "widget.h"

typedef struct GuiFactory GuiFactory;
typedef struct GuiFactoryVt GuiFactoryVt;

struct GuiFactoryVt {
    const char *(*name)(const GuiFactory *self);
    Button     *(*create_button)(const GuiFactory *self, const char *label);
    TextBox    *(*create_textbox)(const GuiFactory *self);
    Checkbox   *(*create_checkbox)(const GuiFactory *self, bool initial);
    void        (*destroy)(GuiFactory *self);
};

struct GuiFactory {
    const GuiFactoryVt *vt;
};

/* Convenience functions */
static inline const char *factory_name(const GuiFactory *f) {
    return f->vt->name(f);
}
static inline Button *factory_create_button(const GuiFactory *f,
                                             const char *label) {
    return f->vt->create_button(f, label);
}
static inline TextBox *factory_create_textbox(const GuiFactory *f) {
    return f->vt->create_textbox(f);
}
static inline Checkbox *factory_create_checkbox(const GuiFactory *f,
                                                 bool initial) {
    return f->vt->create_checkbox(f, initial);
}
static inline void factory_destroy(GuiFactory *f) {
    if (f) f->vt->destroy(f);
}

/* Seleccionar factory por nombre */
GuiFactory *gui_factory_for_platform(const char *platform);

#endif /* GUI_FACTORY_H */
```

### 4.3 Concrete products: familia GTK

```c
/* gtk_widgets.c — productos concretos para GTK */
#include "widget.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ── GTK Button ──────────────────────────────────── */
typedef struct {
    Button base;
    char label[64];
} GtkButton;

static void gtk_button_render(const Button *self) {
    const GtkButton *b = (const GtkButton *)self;
    printf("[GTK Button: %s]\n", b->label);
}

static void gtk_button_on_click(Button *self,
                                 void (*cb)(void *), void *ctx) {
    printf("GTK: connecting click signal\n");
    if (cb) cb(ctx);
}

static void gtk_button_destroy(Button *self) {
    printf("GTK: destroying button\n");
    free(self);
}

static const ButtonVt gtk_button_vt = {
    .render   = gtk_button_render,
    .on_click = gtk_button_on_click,
    .destroy  = gtk_button_destroy,
};

Button *gtk_button_new(const char *label) {
    GtkButton *b = malloc(sizeof *b);
    if (!b) return NULL;
    b->base.vt = &gtk_button_vt;
    strncpy(b->label, label, 63);
    b->label[63] = '\0';
    return &b->base;
}

/* ── GTK TextBox ─────────────────────────────────── */
typedef struct {
    TextBox base;
    char text[256];
} GtkTextBox;

static void gtk_textbox_render(const TextBox *self) {
    const GtkTextBox *t = (const GtkTextBox *)self;
    printf("[GTK TextBox: \"%s\"]\n", t->text);
}

static void gtk_textbox_set_text(TextBox *self, const char *text) {
    GtkTextBox *t = (GtkTextBox *)self;
    strncpy(t->text, text, 255);
    t->text[255] = '\0';
}

static const char *gtk_textbox_get_text(const TextBox *self) {
    return ((const GtkTextBox *)self)->text;
}

static void gtk_textbox_destroy(TextBox *self) {
    printf("GTK: destroying textbox\n");
    free(self);
}

static const TextBoxVt gtk_textbox_vt = {
    .render   = gtk_textbox_render,
    .set_text = gtk_textbox_set_text,
    .get_text = gtk_textbox_get_text,
    .destroy  = gtk_textbox_destroy,
};

TextBox *gtk_textbox_new(void) {
    GtkTextBox *t = malloc(sizeof *t);
    if (!t) return NULL;
    t->base.vt = &gtk_textbox_vt;
    t->text[0] = '\0';
    return &t->base;
}

/* ── GTK Checkbox ────────────────────────────────── */
typedef struct {
    Checkbox base;
    bool checked;
} GtkCheckbox;

static void gtk_checkbox_render(const Checkbox *self) {
    const GtkCheckbox *c = (const GtkCheckbox *)self;
    printf("[GTK Checkbox: %s]\n", c->checked ? "[X]" : "[ ]");
}

static void gtk_checkbox_set_checked(Checkbox *self, bool val) {
    ((GtkCheckbox *)self)->checked = val;
}

static bool gtk_checkbox_is_checked(const Checkbox *self) {
    return ((const GtkCheckbox *)self)->checked;
}

static void gtk_checkbox_destroy(Checkbox *self) {
    printf("GTK: destroying checkbox\n");
    free(self);
}

static const CheckboxVt gtk_checkbox_vt = {
    .render      = gtk_checkbox_render,
    .set_checked = gtk_checkbox_set_checked,
    .is_checked  = gtk_checkbox_is_checked,
    .destroy     = gtk_checkbox_destroy,
};

Checkbox *gtk_checkbox_new(bool initial) {
    GtkCheckbox *c = malloc(sizeof *c);
    if (!c) return NULL;
    c->base.vt = &gtk_checkbox_vt;
    c->checked = initial;
    return &c->base;
}
```

### 4.4 Concrete products: familia Win32

```c
/* win32_widgets.c — productos concretos para Win32 */
#include "widget.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ── Win32 Button ────────────────────────────────── */
typedef struct {
    Button base;
    char label[64];
    int hwnd;  /* simulado */
} Win32Button;

static void win32_button_render(const Button *self) {
    const Win32Button *b = (const Win32Button *)self;
    printf("<Win32 Button hwnd=%d: %s>\n", b->hwnd, b->label);
}

static void win32_button_on_click(Button *self,
                                   void (*cb)(void *), void *ctx) {
    printf("Win32: SetWindowLongPtr for WM_COMMAND\n");
    if (cb) cb(ctx);
}

static void win32_button_destroy(Button *self) {
    printf("Win32: DestroyWindow button\n");
    free(self);
}

static const ButtonVt win32_button_vt = {
    .render   = win32_button_render,
    .on_click = win32_button_on_click,
    .destroy  = win32_button_destroy,
};

static int next_hwnd = 1000;

Button *win32_button_new(const char *label) {
    Win32Button *b = malloc(sizeof *b);
    if (!b) return NULL;
    b->base.vt = &win32_button_vt;
    strncpy(b->label, label, 63);
    b->label[63] = '\0';
    b->hwnd = next_hwnd++;
    return &b->base;
}

/* ── Win32 TextBox ───────────────────────────────── */
typedef struct {
    TextBox base;
    char text[256];
    int hwnd;
} Win32TextBox;

static void win32_textbox_render(const TextBox *self) {
    const Win32TextBox *t = (const Win32TextBox *)self;
    printf("<Win32 TextBox hwnd=%d: \"%s\">\n", t->hwnd, t->text);
}

static void win32_textbox_set_text(TextBox *self, const char *text) {
    Win32TextBox *t = (Win32TextBox *)self;
    strncpy(t->text, text, 255);
    t->text[255] = '\0';
}

static const char *win32_textbox_get_text(const TextBox *self) {
    return ((const Win32TextBox *)self)->text;
}

static void win32_textbox_destroy(TextBox *self) {
    printf("Win32: DestroyWindow textbox\n");
    free(self);
}

static const TextBoxVt win32_textbox_vt = {
    .render   = win32_textbox_render,
    .set_text = win32_textbox_set_text,
    .get_text = win32_textbox_get_text,
    .destroy  = win32_textbox_destroy,
};

TextBox *win32_textbox_new(void) {
    Win32TextBox *t = malloc(sizeof *t);
    if (!t) return NULL;
    t->base.vt = &win32_textbox_vt;
    t->text[0] = '\0';
    t->hwnd = next_hwnd++;
    return &t->base;
}

/* ── Win32 Checkbox ──────────────────────────────── */
typedef struct {
    Checkbox base;
    bool checked;
    int hwnd;
} Win32Checkbox;

static void win32_checkbox_render(const Checkbox *self) {
    const Win32Checkbox *c = (const Win32Checkbox *)self;
    printf("<Win32 Checkbox hwnd=%d: %s>\n",
           c->hwnd, c->checked ? "[X]" : "[ ]");
}

static void win32_checkbox_set_checked(Checkbox *self, bool val) {
    ((Win32Checkbox *)self)->checked = val;
}

static bool win32_checkbox_is_checked(const Checkbox *self) {
    return ((const Win32Checkbox *)self)->checked;
}

static void win32_checkbox_destroy(Checkbox *self) {
    printf("Win32: DestroyWindow checkbox\n");
    free(self);
}

static const CheckboxVt win32_checkbox_vt = {
    .render      = win32_checkbox_render,
    .set_checked = win32_checkbox_set_checked,
    .is_checked  = win32_checkbox_is_checked,
    .destroy     = win32_checkbox_destroy,
};

Checkbox *win32_checkbox_new(bool initial) {
    Win32Checkbox *c = malloc(sizeof *c);
    if (!c) return NULL;
    c->base.vt = &win32_checkbox_vt;
    c->checked = initial;
    c->hwnd = next_hwnd++;
    return &c->base;
}
```

### 4.5 Las concrete factories

```c
/* gtk_factory.c */
#include "gui_factory.h"
#include <stdlib.h>

/* Declarar constructores (de gtk_widgets.c) */
Button   *gtk_button_new(const char *label);
TextBox  *gtk_textbox_new(void);
Checkbox *gtk_checkbox_new(bool initial);

static const char *gtk_factory_name(const GuiFactory *self) {
    (void)self;
    return "GTK";
}

static Button *gtk_factory_create_button(const GuiFactory *self,
                                          const char *label) {
    (void)self;
    return gtk_button_new(label);
}

static TextBox *gtk_factory_create_textbox(const GuiFactory *self) {
    (void)self;
    return gtk_textbox_new();
}

static Checkbox *gtk_factory_create_checkbox(const GuiFactory *self,
                                              bool initial) {
    (void)self;
    return gtk_checkbox_new(initial);
}

static void gtk_factory_destroy(GuiFactory *self) {
    free(self);
}

static const GuiFactoryVt gtk_factory_vt = {
    .name            = gtk_factory_name,
    .create_button   = gtk_factory_create_button,
    .create_textbox  = gtk_factory_create_textbox,
    .create_checkbox = gtk_factory_create_checkbox,
    .destroy         = gtk_factory_destroy,
};

GuiFactory *gtk_factory_new(void) {
    GuiFactory *f = malloc(sizeof *f);
    if (!f) return NULL;
    f->vt = &gtk_factory_vt;
    return f;
}
```

```c
/* win32_factory.c */
#include "gui_factory.h"
#include <stdlib.h>

Button   *win32_button_new(const char *label);
TextBox  *win32_textbox_new(void);
Checkbox *win32_checkbox_new(bool initial);

static const char *win32_factory_name(const GuiFactory *self) {
    (void)self;
    return "Win32";
}

static Button *win32_factory_create_button(const GuiFactory *self,
                                            const char *label) {
    (void)self;
    return win32_button_new(label);
}

static TextBox *win32_factory_create_textbox(const GuiFactory *self) {
    (void)self;
    return win32_textbox_new();
}

static Checkbox *win32_factory_create_checkbox(const GuiFactory *self,
                                                bool initial) {
    (void)self;
    return win32_checkbox_new(initial);
}

static void win32_factory_destroy(GuiFactory *self) {
    free(self);
}

static const GuiFactoryVt win32_factory_vt = {
    .name            = win32_factory_name,
    .create_button   = win32_factory_create_button,
    .create_textbox  = win32_factory_create_textbox,
    .create_checkbox = win32_factory_create_checkbox,
    .destroy         = win32_factory_destroy,
};

GuiFactory *win32_factory_new(void) {
    GuiFactory *f = malloc(sizeof *f);
    if (!f) return NULL;
    f->vt = &win32_factory_vt;
    return f;
}
```

### 4.6 Selector de factory

```c
/* gui_factory.c — meta-factory: elige la concrete factory */
#include "gui_factory.h"
#include <string.h>

/* Declarar constructores de concrete factories */
GuiFactory *gtk_factory_new(void);
GuiFactory *win32_factory_new(void);

GuiFactory *gui_factory_for_platform(const char *platform) {
    if (strcmp(platform, "linux") == 0 ||
        strcmp(platform, "gtk") == 0) {
        return gtk_factory_new();
    }
    if (strcmp(platform, "windows") == 0 ||
        strcmp(platform, "win32") == 0) {
        return win32_factory_new();
    }
    return NULL;
}
```

### 4.7 El cliente

```c
/* main.c — el cliente NO conoce GTK ni Win32 */
#include <stdio.h>
#include "gui_factory.h"

void build_login_form(const GuiFactory *factory) {
    printf("Building form with %s toolkit:\n", factory_name(factory));

    TextBox  *username = factory_create_textbox(factory);
    TextBox  *password = factory_create_textbox(factory);
    Checkbox *remember = factory_create_checkbox(factory, false);
    Button   *login    = factory_create_button(factory, "Login");
    Button   *cancel   = factory_create_button(factory, "Cancel");

    textbox_set_text(username, "admin");
    textbox_set_text(password, "****");
    checkbox_set_checked(remember, true);

    /* Render */
    textbox_render(username);
    textbox_render(password);
    checkbox_render(remember);
    button_render(login);
    button_render(cancel);

    /* Cleanup */
    textbox_destroy(username);
    textbox_destroy(password);
    checkbox_destroy(remember);
    button_destroy(login);
    button_destroy(cancel);
}

int main(int argc, char *argv[]) {
    const char *platform = (argc > 1) ? argv[1] : "gtk";

    GuiFactory *factory = gui_factory_for_platform(platform);
    if (!factory) {
        fprintf(stderr, "Unknown platform: %s\n", platform);
        return 1;
    }

    build_login_form(factory);

    factory_destroy(factory);
    return 0;
}
```

```
  $ ./app gtk
  Building form with GTK toolkit:
  [GTK TextBox: "admin"]
  [GTK TextBox: "****"]
  [GTK Checkbox: [X]]
  [GTK Button: Login]
  [GTK Button: Cancel]

  $ ./app windows
  Building form with Win32 toolkit:
  <Win32 TextBox hwnd=1000: "admin">
  <Win32 TextBox hwnd=1001: "****">
  <Win32 Checkbox hwnd=1002: [X]>
  <Win32 Button hwnd=1003: Login>
  <Win32 Button hwnd=1004: Cancel>
```

Observa: `build_login_form` **no sabe** que plataforma usa. No
incluye headers de GTK ni Win32. No tiene if/else por plataforma.
Toda la logica de plataforma esta encapsulada en la factory.

---

## 5. Estructura en memoria

```
  GuiFactory (GTK):
  ┌─────────────────┐
  │ vt ─────────────┼──→ GuiFactoryVt (gtk)
  └─────────────────┘    ├─ name → "GTK"
                         ├─ create_button → gtk_button_new
                         ├─ create_textbox → gtk_textbox_new
                         └─ create_checkbox → gtk_checkbox_new

  factory->vt->create_button(factory, "OK")
      │
      └──→ gtk_button_new("OK")
              │
              └──→ malloc(sizeof GtkButton)
                   ┌─────────────────────────┐
                   │ GtkButton               │
                   │ ├─ base.vt ─→ ButtonVt  │
                   │ └─ label: "OK"          │
                   └─────────────────────────┘

  La factory crea el producto con la vtable correcta.
  El producto retorna como Button* — el caller no sabe que es GtkButton.
```

---

## 6. Factory con estado

La factory puede tener estado que afecta a todos los productos:

```c
/* Factory con tema (colores, fuente) */
typedef struct {
    GuiFactory base;
    const char *theme;       /* "light" o "dark" */
    int default_font_size;
} ThemedFactory;

static Button *themed_create_button(const GuiFactory *self,
                                     const char *label) {
    const ThemedFactory *tf = (const ThemedFactory *)self;
    printf("Creating button with theme=%s, font=%d\n",
           tf->theme, tf->default_font_size);
    return gtk_button_new(label);  /* delegaria al real con config */
}

GuiFactory *themed_factory_new(const char *platform,
                                const char *theme,
                                int font_size) {
    ThemedFactory *tf = malloc(sizeof *tf);
    if (!tf) return NULL;
    /* Copiar vtable base y overridear create_button */
    static GuiFactoryVt themed_vt = { /* ... */ };
    tf->base.vt = &themed_vt;
    tf->theme = theme;
    tf->default_font_size = font_size;
    return &tf->base;
}
```

```
  Sin estado:                     Con estado:
  ┌─────────────┐                ┌─────────────────────┐
  │ GuiFactory  │                │ ThemedFactory        │
  │ ├─ vt       │                │ ├─ base.vt           │
  └─────────────┘                │ ├─ theme: "dark"     │
  Solo vtable,                   │ └─ font_size: 14     │
  sin configuracion              └─────────────────────┘
                                 Config afecta todos
                                 los productos creados
```

---

## 7. Factory singleton vs factory por instancia

### 7.1 Factory como singleton (vtable constante)

Si la factory no tiene estado, puede ser un singleton global:

```c
/* La factory GTK no tiene campos propios — solo vt */
static const GuiFactory gtk_factory_singleton = {
    .vt = &gtk_factory_vt,
};

const GuiFactory *get_gtk_factory(void) {
    return &gtk_factory_singleton;
}

/* Ventaja: sin malloc, sin destroy, sin leaks.
   El puntero es valido durante toda la vida del programa. */
```

### 7.2 Factory por instancia (con estado)

Si la factory tiene estado (tema, config, contexto), necesita
una instancia por configuracion:

```c
GuiFactory *f1 = themed_factory_new("gtk", "light", 12);
GuiFactory *f2 = themed_factory_new("gtk", "dark",  16);

/* f1 y f2 crean widgets diferentes (distinto tema y font) */

factory_destroy(f1);
factory_destroy(f2);
```

```
  Singleton:                    Instancia:
  ─────────                     ──────────
  Sin malloc                    malloc por factory
  Sin destroy                   destroy obligatorio
  Sin estado                    Estado configurable
  Un "estilo" por plataforma    Multiples estilos
  Thread-safe por defecto       Necesita sync si compartido
```

---

## 8. Agregar una nueva familia

Para agregar macOS (Cocoa), necesitas:

```
  Archivos a crear:
  1. cocoa_widgets.c    → CocoaButton, CocoaTextBox, CocoaCheckbox
  2. cocoa_factory.c    → cocoa_factory_new()

  Archivos a modificar:
  1. gui_factory.c      → agregar "macos" al selector

  Archivos que NO se modifican:
  ✓ widget.h            (interfaces no cambian)
  ✓ gui_factory.h       (interfaz de factory no cambia)
  ✓ main.c              (el cliente no cambia)
  ✓ gtk_widgets.c       (otra familia no se toca)
  ✓ win32_widgets.c     (otra familia no se toca)
```

```
  Agregar una FAMILIA (nueva plataforma):
  ────────────────────────────────────────
  Crear: N productos + 1 factory     → archivos NUEVOS
  Modificar: el selector             → 1 linea
  NO tocar: clientes ni otras familias

  Agregar un PRODUCTO (nuevo widget tipo):
  ─────────────────────────────────────────
  Crear: widget.h interfaz            → 1 header nuevo
  Modificar: TODAS las concrete factories + todos los widgets
  → Esto VIOLA Open/Closed: agregar un producto toca todas las familias

  Este es el trade-off del Abstract Factory:
  Agregar familias es FACIL.
  Agregar productos es DIFICIL.
```

---

## 9. Segundo ejemplo: base de datos

Un caso de uso mas realista que GUI: abstraer el acceso a
distintos backends de base de datos.

```c
/* db.h — abstract products */
typedef struct DbConnection DbConnection;
typedef struct DbConnectionVt {
    int  (*execute)(DbConnection *self, const char *sql);
    int  (*query)(DbConnection *self, const char *sql,
                  char ***rows, int *nrows);
    void (*close)(DbConnection *self);
} DbConnectionVt;

struct DbConnection {
    const DbConnectionVt *vt;
};

typedef struct DbStatement DbStatement;
typedef struct DbStatementVt {
    int  (*bind_int)(DbStatement *self, int index, int value);
    int  (*bind_str)(DbStatement *self, int index, const char *value);
    int  (*step)(DbStatement *self);
    void (*finalize)(DbStatement *self);
} DbStatementVt;

struct DbStatement {
    const DbStatementVt *vt;
};

/* db_factory.h — abstract factory */
typedef struct DbFactory DbFactory;
typedef struct DbFactoryVt {
    const char    *(*name)(const DbFactory *self);
    DbConnection  *(*connect)(const DbFactory *self, const char *dsn);
    DbStatement   *(*prepare)(const DbFactory *self, DbConnection *conn,
                              const char *sql);
    void           (*destroy)(DbFactory *self);
} DbFactoryVt;

struct DbFactory {
    const DbFactoryVt *vt;
};

DbFactory *db_factory_for_backend(const char *backend);
```

```c
/* Uso: */
DbFactory *db = db_factory_for_backend("sqlite");

DbConnection *conn = db->vt->connect(db, "app.db");
conn->vt->execute(conn, "CREATE TABLE users (id INT, name TEXT)");

DbStatement *stmt = db->vt->prepare(db, conn,
    "INSERT INTO users VALUES (?, ?)");
stmt->vt->bind_int(stmt, 0, 1);
stmt->vt->bind_str(stmt, 1, "Alice");
stmt->vt->step(stmt);
stmt->vt->finalize(stmt);

conn->vt->close(conn);
db->vt->destroy(db);
```

```
  Familias posibles:
  ├─ SqliteFactory  → SqliteConnection, SqliteStatement
  ├─ PgFactory      → PgConnection, PgStatement
  └─ MockFactory    → MockConnection, MockStatement (para tests)

  El codigo de la aplicacion solo conoce DbConnection y DbStatement.
  Cambiar de SQLite a Postgres: cambiar UNA linea (el backend string).
```

---

## 10. Comparacion con Rust (preview)

```c
/* C: Abstract Factory = struct de function pointers */
struct GuiFactoryVt {
    Button   *(*create_button)(const GuiFactory *self, const char *label);
    TextBox  *(*create_textbox)(const GuiFactory *self);
    Checkbox *(*create_checkbox)(const GuiFactory *self, bool initial);
};
```

```rust
// Rust: Abstract Factory = trait con associated types o dyn
trait GuiFactory {
    fn create_button(&self, label: &str) -> Box<dyn Button>;
    fn create_textbox(&self) -> Box<dyn TextBox>;
    fn create_checkbox(&self, initial: bool) -> Box<dyn Checkbox>;
}

// O con associated types (generics, mas flexible):
trait GuiFactory {
    type Btn: Button;
    type Txt: TextBox;
    type Chk: Checkbox;

    fn create_button(&self, label: &str) -> Self::Btn;
    fn create_textbox(&self) -> Self::Txt;
    fn create_checkbox(&self, initial: bool) -> Self::Chk;
}
```

```
  C:                                Rust:
  ──                                ────
  Vtable manual                     trait (automatico)
  N vtables (1 por producto +       1 trait por producto +
   1 por factory)                    1 trait por factory
  Cast manual (unsafe)              Sin cast (type system)
  Boilerplate ~200 lineas/familia   ~50 lineas/familia
  Destroy manual                    Drop automatico
  NULL checks                       Option/Result
```

T04 implementa la version completa en Rust.

---

## Errores comunes

### Error 1 — Mezclar productos de distintas familias

```c
/* BUG: factory GTK crea un widget, factory Win32 crea otro */
GuiFactory *gtk = gui_factory_for_platform("gtk");
GuiFactory *win = gui_factory_for_platform("windows");

Button  *b = factory_create_button(gtk, "OK");   /* GTK */
TextBox *t = factory_create_textbox(win);         /* Win32! */

/* Ahora tienes un GtkButton junto a un Win32TextBox.
   En una GUI real, esto causaria crashes o rendering incorrecto. */
```

Solucion: pasar UNA factory a todo el subsistema. La factory
garantiza que todos los productos son de la misma familia.

### Error 2 — Agregar un producto sin actualizar TODAS las factories

```c
/* Agregaste Slider al vtable: */
struct GuiFactoryVt {
    /* ... existentes ... */
    Slider *(*create_slider)(const GuiFactory *self, int min, int max);
};

/* Actualizaste gtk_factory_vt con create_slider = gtk_slider_create */
/* Olvidaste win32_factory_vt → create_slider queda NULL */

/* Cuando el cliente llama: */
factory_create_slider(factory, 0, 100);
/* Si factory es Win32 → NULL dereference → crash */
```

No hay forma en C de forzar que todas las factories implementen
el nuevo metodo. Solucion: una funcion de verificacion que
comprueba que todos los punteros del vtable son non-NULL al crear
la factory.

### Error 3 — Factory sin destroy para la factory misma

```c
/* Olvidaste que la factory tambien necesita destroy */
GuiFactory *f = gui_factory_for_platform("gtk");
build_login_form(f);
/* Nunca llamas factory_destroy(f) → leak de la factory */
```

Cada malloc necesita su free. La factory ES un objeto alocado,
no solo un creador de objetos.

### Error 4 — Factory demasiado granular

```c
/* Anti-pattern: una factory por cada producto */
ButtonFactory *bf = button_factory_for_platform("gtk");
TextBoxFactory *tf = textbox_factory_for_platform("gtk");
CheckboxFactory *cf = checkbox_factory_for_platform("gtk");

/* Esto NO es Abstract Factory. Es tres Simple Factories.
   No garantiza consistencia: puedes mezclar btn_gtk con txt_win32.
   El Abstract Factory agrupa TODO en UNA factory para evitar esto. */
```

---

## Ejercicios

### Ejercicio 1 — Agregar una familia

Agrega una tercera familia "terminal" (TUI) que renderiza widgets
como texto ASCII:

```
  [btn: OK]
  [txt: "hello"]
  [chk: [X]]
```

Crea `term_widgets.c` y `term_factory.c`. El main debe poder
recibir `./app terminal`.

**Prediccion**: Cuantos archivos existentes necesitas modificar?

<details><summary>Respuesta</summary>

Archivos a **crear**: 2 (term_widgets.c, term_factory.c)
Archivos a **modificar**: 1 (gui_factory.c — agregar "terminal" al
selector)

```c
/* term_widgets.c (simplificado, solo Button) */
#include "widget.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    Button base;
    char label[64];
} TermButton;

static void term_button_render(const Button *self) {
    const TermButton *b = (const TermButton *)self;
    printf("[btn: %s]\n", b->label);
}

static void term_button_on_click(Button *self,
                                  void (*cb)(void *), void *ctx) {
    /* TUI: no hay eventos reales, llamar callback directo */
    if (cb) cb(ctx);
}

static void term_button_destroy(Button *self) { free(self); }

static const ButtonVt term_button_vt = {
    .render   = term_button_render,
    .on_click = term_button_on_click,
    .destroy  = term_button_destroy,
};

Button *term_button_new(const char *label) {
    TermButton *b = malloc(sizeof *b);
    if (!b) return NULL;
    b->base.vt = &term_button_vt;
    strncpy(b->label, label, 63);
    b->label[63] = '\0';
    return &b->base;
}

/* ... TextBox y Checkbox similares ... */
```

```c
/* term_factory.c */
#include "gui_factory.h"
#include <stdlib.h>

Button   *term_button_new(const char *label);
TextBox  *term_textbox_new(void);
Checkbox *term_checkbox_new(bool initial);

static const char *term_name(const GuiFactory *f) {
    (void)f; return "Terminal";
}
static Button *term_create_btn(const GuiFactory *f, const char *l) {
    (void)f; return term_button_new(l);
}
static TextBox *term_create_txt(const GuiFactory *f) {
    (void)f; return term_textbox_new();
}
static Checkbox *term_create_chk(const GuiFactory *f, bool v) {
    (void)f; return term_checkbox_new(v);
}
static void term_destroy(GuiFactory *f) { free(f); }

static const GuiFactoryVt term_factory_vt = {
    term_name, term_create_btn, term_create_txt,
    term_create_chk, term_destroy,
};

GuiFactory *term_factory_new(void) {
    GuiFactory *f = malloc(sizeof *f);
    if (!f) return NULL;
    f->vt = &term_factory_vt;
    return f;
}
```

En gui_factory.c, agregar:
```c
GuiFactory *term_factory_new(void);

/* En gui_factory_for_platform: */
if (strcmp(platform, "terminal") == 0) return term_factory_new();
```

El main.c **no cambia** — ese es el beneficio del Abstract Factory.

</details>

---

### Ejercicio 2 — Factory con estado compartido

Modifica la factory GTK para que todos los widgets creados
compartan un "theme" (nombre de fuente y tamaño). La factory
almacena el theme como estado, y los widgets lo leen al render.

**Prediccion**: Donde almacenas el theme — en la factory, en cada
widget, o ambos?

<details><summary>Respuesta</summary>

Almacenas en la factory y **copias** a cada widget al crearlo.
No conviene que el widget tenga un puntero a la factory (la
factory puede destruirse antes que el widget).

```c
typedef struct {
    GuiFactory base;
    char font[64];
    int font_size;
} GtkThemedFactory;

typedef struct {
    Button btn_base;
    char label[64];
    char font[64];    /* copiado de la factory */
    int font_size;
} GtkThemedButton;

static void gtk_themed_button_render(const Button *self) {
    const GtkThemedButton *b = (const GtkThemedButton *)self;
    printf("[GTK Button: %s | font=%s %dpx]\n",
           b->label, b->font, b->font_size);
}

static Button *gtk_themed_create_button(const GuiFactory *self,
                                         const char *label) {
    const GtkThemedFactory *tf = (const GtkThemedFactory *)self;
    GtkThemedButton *b = malloc(sizeof *b);
    if (!b) return NULL;
    static const ButtonVt vt = {
        .render = gtk_themed_button_render,
        .on_click = /* ... */,
        .destroy = /* free */,
    };
    b->btn_base.vt = &vt;
    strncpy(b->label, label, 63);
    strncpy(b->font, tf->font, 63);
    b->font_size = tf->font_size;
    return &b->btn_base;
}

GtkThemedFactory *gtk_themed_factory_new(const char *font, int size) {
    GtkThemedFactory *tf = malloc(sizeof *tf);
    if (!tf) return NULL;
    static const GuiFactoryVt vt = {
        .name = /* "GTK Themed" */,
        .create_button = gtk_themed_create_button,
        /* ... */
    };
    tf->base.vt = &vt;
    strncpy(tf->font, font, 63);
    tf->font_size = size;
    return tf;
}

/* Uso: */
GuiFactory *f = (GuiFactory *)
    gtk_themed_factory_new("DejaVu Sans", 14);
Button *b = factory_create_button(f, "OK");
button_render(b);
// [GTK Button: OK | font=DejaVu Sans 14px]
```

Almacenar en **ambos** es lo mas robusto: la factory es la fuente
de verdad, y copia al widget al crearlo. Asi el widget es
independiente de la factory despues de la creacion.

</details>

---

### Ejercicio 3 — Verificar completitud del vtable

Escribe una funcion que verifique que todos los punteros de un
`GuiFactoryVt` son non-NULL:

```c
int gui_factory_validate(const GuiFactory *f);
/* Retorna 0 si OK, -1 si hay metodos NULL */
```

**Prediccion**: Puede Rust tener este problema (metodos sin
implementar)?

<details><summary>Respuesta</summary>

```c
int gui_factory_validate(const GuiFactory *f) {
    if (!f || !f->vt) return -1;

    const GuiFactoryVt *vt = f->vt;
    int ok = 1;

    if (!vt->name) {
        fprintf(stderr, "factory: name is NULL\n");
        ok = 0;
    }
    if (!vt->create_button) {
        fprintf(stderr, "factory: create_button is NULL\n");
        ok = 0;
    }
    if (!vt->create_textbox) {
        fprintf(stderr, "factory: create_textbox is NULL\n");
        ok = 0;
    }
    if (!vt->create_checkbox) {
        fprintf(stderr, "factory: create_checkbox is NULL\n");
        ok = 0;
    }
    if (!vt->destroy) {
        fprintf(stderr, "factory: destroy is NULL\n");
        ok = 0;
    }

    return ok ? 0 : -1;
}

/* Uso en gui_factory_for_platform: */
GuiFactory *f = gtk_factory_new();
if (gui_factory_validate(f) != 0) {
    fprintf(stderr, "BUG: incomplete factory\n");
    abort();
}
return f;
```

**En Rust: imposible.** Si un tipo implementa un trait, el
compilador exige que TODOS los metodos esten implementados (a
menos que tengan default). Si falta uno, no compila. No necesitas
validacion en runtime.

```rust
// Rust:
impl GuiFactory for GtkFactory {
    fn create_button(&self, label: &str) -> Box<dyn Button> { ... }
    fn create_textbox(&self) -> Box<dyn TextBox> { ... }
    // Olvidar create_checkbox → error de compilacion:
    // "not all trait items implemented, missing: `create_checkbox`"
}
```

Esta es una de las ventajas fundamentales de los traits sobre
vtables manuales.

</details>

---

### Ejercicio 4 — Abstract Factory vs Simple Factory

Reescribe el ejemplo GUI usando Simple Factory (una funcion
`create_widget` con tipo) en lugar de Abstract Factory. Compara:

1. Cuantos if/else por plataforma tiene el codigo cliente?
2. Que pasa si agregas una nueva plataforma?
3. Que pasa si agregas un nuevo tipo de widget?

**Prediccion**: En cual caso es mas facil agregar plataformas?
En cual es mas facil agregar widgets?

<details><summary>Respuesta</summary>

Simple Factory:

```c
typedef enum { WIDGET_BUTTON, WIDGET_TEXTBOX, WIDGET_CHECKBOX } WidgetType;
typedef enum { PLATFORM_GTK, PLATFORM_WIN32 } Platform;

void *create_widget(Platform p, WidgetType w, ...) {
    if (p == PLATFORM_GTK) {
        switch (w) {
        case WIDGET_BUTTON:   return gtk_button_new(...);
        case WIDGET_TEXTBOX:  return gtk_textbox_new();
        case WIDGET_CHECKBOX: return gtk_checkbox_new(...);
        }
    } else if (p == PLATFORM_WIN32) {
        switch (w) {
        case WIDGET_BUTTON:   return win32_button_new(...);
        case WIDGET_TEXTBOX:  return win32_textbox_new();
        case WIDGET_CHECKBOX: return win32_checkbox_new(...);
        }
    }
    return NULL;
}
```

**1. if/else en el cliente:**

Con Simple Factory: el cliente llama `create_widget(platform, ...)`
en cada lugar donde crea un widget. El platform se propaga por
todo el codigo.

Con Abstract Factory: el cliente recibe `GuiFactory *` una vez.
Cero if/else por plataforma en el codigo del cliente.

**2. Agregar plataforma:**

Simple Factory: agregar un `else if` en `create_widget` con N
cases (uno por widget). Un solo archivo, pero la funcion crece.

Abstract Factory: crear archivos nuevos (widgets + factory).
Modificar el selector (1 linea). No tocar el cliente.

**3. Agregar widget:**

Simple Factory: agregar un case en cada bloque de plataforma.
Un solo archivo.

Abstract Factory: agregar al vtable de la factory, y crear la
implementacion en CADA concrete factory. Multiples archivos.

**Resumen:**

|                    | Agregar plataforma | Agregar widget |
|--------------------|--------------------|----------------|
| Simple Factory     | Medio (1 funcion)  | Facil (1 case) |
| Abstract Factory   | Facil (archivos)   | Dificil (todos)|

Abstract Factory es superior cuando las familias crecen (nuevas
plataformas). Simple Factory es superior cuando los productos
crecen (nuevos widgets).

</details>

---

### Ejercicio 5 — Factory de base de datos

Implementa la abstract factory de base de datos (seccion 9) con
dos familias: "sqlite" y "mock" (para tests). Solo necesitas
`DbConnection` con `execute` y `close`.

**Prediccion**: Por que una MockFactory es util en tests?

<details><summary>Respuesta</summary>

```c
/* db.h — products */
typedef struct DbConnection DbConnection;
typedef struct DbConnectionVt {
    int   (*execute)(DbConnection *self, const char *sql);
    void  (*close)(DbConnection *self);
} DbConnectionVt;
struct DbConnection { const DbConnectionVt *vt; };

/* db_factory.h */
typedef struct DbFactory DbFactory;
typedef struct DbFactoryVt {
    const char   *(*name)(const DbFactory *self);
    DbConnection *(*connect)(const DbFactory *self, const char *dsn);
    void          (*destroy)(DbFactory *self);
} DbFactoryVt;
struct DbFactory { const DbFactoryVt *vt; };

/* sqlite_db.c */
typedef struct { DbConnection base; } SqliteConn;

static int sqlite_execute(DbConnection *self, const char *sql) {
    printf("[sqlite] executing: %s\n", sql);
    return 0;  /* en realidad llamaria a sqlite3_exec */
}
static void sqlite_close(DbConnection *self) {
    printf("[sqlite] closing connection\n");
    free(self);
}
static const DbConnectionVt sqlite_conn_vt = {
    sqlite_execute, sqlite_close
};

static DbConnection *sqlite_connect(const DbFactory *f, const char *dsn) {
    (void)f;
    printf("[sqlite] connecting to %s\n", dsn);
    SqliteConn *c = malloc(sizeof *c);
    c->base.vt = &sqlite_conn_vt;
    return &c->base;
}
/* ... factory vtable and constructor ... */

/* mock_db.c */
typedef struct {
    DbConnection base;
    int call_count;
} MockConn;

static int mock_execute(DbConnection *self, const char *sql) {
    MockConn *m = (MockConn *)self;
    m->call_count++;
    printf("[mock] execute #%d: %s\n", m->call_count, sql);
    return 0;
}
static void mock_close(DbConnection *self) {
    MockConn *m = (MockConn *)self;
    printf("[mock] closed after %d calls\n", m->call_count);
    free(self);
}
static const DbConnectionVt mock_conn_vt = {
    mock_execute, mock_close
};

static DbConnection *mock_connect(const DbFactory *f, const char *dsn) {
    (void)f; (void)dsn;
    printf("[mock] fake connection\n");
    MockConn *c = malloc(sizeof *c);
    c->base.vt = &mock_conn_vt;
    c->call_count = 0;
    return &c->base;
}
/* ... factory vtable and constructor ... */
```

MockFactory es util en tests porque:
1. **Sin dependencias externas**: no necesitas SQLite instalado
2. **Determinista**: siempre retorna lo mismo, sin estado de DB
3. **Rapido**: sin IO de disco, sin parsing SQL real
4. **Verificable**: puedes contar llamadas, registrar queries
5. **Aislado**: cada test empieza en estado limpio

El codigo de la aplicacion recibe `DbFactory*` — no sabe si es
real o mock. En produccion recibes SqliteFactory, en tests
recibes MockFactory. Misma interfaz, distinto comportamiento.

</details>

---

### Ejercicio 6 — Contar productos por factory

Modifica la abstract factory para que cada factory lleve un
conteo de cuantos productos ha creado (total y por tipo).

```c
void factory_print_stats(const GuiFactory *f);
/*  Buttons: 3
    TextBoxes: 2
    Checkboxes: 1
    Total: 6  */
```

**Prediccion**: Puedes hacerlo sin cambiar el vtable?

<details><summary>Respuesta</summary>

Si, usando una factory wrapper que intercepta las llamadas:

```c
typedef struct {
    GuiFactory base;
    const GuiFactory *inner;  /* la factory real */
    int buttons_created;
    int textboxes_created;
    int checkboxes_created;
} CountingFactory;

static Button *counting_create_button(const GuiFactory *self,
                                       const char *label) {
    CountingFactory *cf = (CountingFactory *)self;
    cf->buttons_created++;
    return cf->inner->vt->create_button(cf->inner, label);
}

static TextBox *counting_create_textbox(const GuiFactory *self) {
    CountingFactory *cf = (CountingFactory *)self;
    cf->textboxes_created++;
    return cf->inner->vt->create_textbox(cf->inner);
}

static Checkbox *counting_create_checkbox(const GuiFactory *self,
                                           bool initial) {
    CountingFactory *cf = (CountingFactory *)self;
    cf->checkboxes_created++;
    return cf->inner->vt->create_checkbox(cf->inner, initial);
}

static const char *counting_name(const GuiFactory *self) {
    const CountingFactory *cf = (const CountingFactory *)self;
    return cf->inner->vt->name(cf->inner);
}

static void counting_destroy(GuiFactory *self) {
    CountingFactory *cf = (CountingFactory *)self;
    /* no destroy inner — caller may own it */
    free(self);
}

static const GuiFactoryVt counting_vt = {
    .name            = counting_name,
    .create_button   = counting_create_button,
    .create_textbox  = counting_create_textbox,
    .create_checkbox = counting_create_checkbox,
    .destroy         = counting_destroy,
};

GuiFactory *counting_factory_wrap(const GuiFactory *inner) {
    CountingFactory *cf = malloc(sizeof *cf);
    if (!cf) return NULL;
    cf->base.vt = &counting_vt;
    cf->inner = inner;
    cf->buttons_created = 0;
    cf->textboxes_created = 0;
    cf->checkboxes_created = 0;
    return &cf->base;
}

void factory_print_stats(const GuiFactory *f) {
    const CountingFactory *cf = (const CountingFactory *)f;
    printf("Buttons:    %d\n", cf->buttons_created);
    printf("TextBoxes:  %d\n", cf->textboxes_created);
    printf("Checkboxes: %d\n", cf->checkboxes_created);
    printf("Total:      %d\n",
           cf->buttons_created + cf->textboxes_created +
           cf->checkboxes_created);
}
```

Este es el patron **Decorator** (C03) aplicado a una factory:
envuelves la factory real con una capa que agrega funcionalidad
(conteo) sin modificar la factory original.

</details>

---

### Ejercicio 7 — Registry de factories

Implementa un sistema donde las concrete factories se registran
en un registry global (como el shape registry de T01 seccion 7):

```c
int gui_factory_register(const char *name, GuiFactory *(*create)(void));
GuiFactory *gui_factory_get(const char *name);
void gui_factory_list(void);
```

**Prediccion**: Es mejor registrar factory singletons o factory
constructors?

<details><summary>Respuesta</summary>

```c
#define MAX_FACTORIES 16

typedef struct {
    char name[32];
    GuiFactory *(*create)(void);
} FactoryEntry;

static FactoryEntry factory_registry[MAX_FACTORIES];
static int factory_count = 0;

int gui_factory_register(const char *name, GuiFactory *(*create)(void)) {
    if (factory_count >= MAX_FACTORIES) return -1;
    strncpy(factory_registry[factory_count].name, name, 31);
    factory_registry[factory_count].name[31] = '\0';
    factory_registry[factory_count].create = create;
    factory_count++;
    return 0;
}

GuiFactory *gui_factory_get(const char *name) {
    for (int i = 0; i < factory_count; i++) {
        if (strcmp(factory_registry[i].name, name) == 0) {
            return factory_registry[i].create();
        }
    }
    return NULL;
}

void gui_factory_list(void) {
    printf("Registered factories:\n");
    for (int i = 0; i < factory_count; i++) {
        printf("  - %s\n", factory_registry[i].name);
    }
}

/* Registro (al inicio de main o con constructor attribute): */
void register_all_factories(void) {
    gui_factory_register("gtk",      gtk_factory_new);
    gui_factory_register("win32",    win32_factory_new);
    gui_factory_register("terminal", term_factory_new);
}
```

Es mejor registrar **constructors** (`GuiFactory *(*)(void)`)
que singletons porque:

1. Cada `gui_factory_get()` retorna una **nueva instancia** —
   el caller puede destruirla sin afectar a otros.
2. Si la factory tiene estado (tema, config), cada instancia
   puede configurarse independientemente.
3. No hay problemas de lifetime: el registry solo almacena
   function pointers (no objetos que puedan invalidarse).

Con singletons, la destruccion es problematica: si un caller
llama `factory_destroy` al singleton, TODOS los usuarios del
singleton quedan con un dangling pointer.

</details>

---

### Ejercicio 8 — Factory sin allocation

Diseña una version de la abstract factory que no use malloc:
la factory es un `const` global, y los productos se crean en
un buffer proporcionado por el caller.

```c
Button *factory_create_button_in(const GuiFactory *f,
                                  void *buf, size_t buf_size,
                                  const char *label);
```

**Prediccion**: Este patron es util en embedded. Por que?

<details><summary>Respuesta</summary>

```c
/* Factory sin malloc: caller proporciona el buffer */
typedef struct GuiFactoryNoAllocVt {
    const char *(*name)(const GuiFactory *self);
    size_t      (*button_size)(void);
    size_t      (*textbox_size)(void);
    Button     *(*init_button)(void *buf, const char *label);
    TextBox    *(*init_textbox)(void *buf);
    /* no destroy — el caller maneja la memoria */
} GuiFactoryNoAllocVt;

/* GTK sin alloc: */
static size_t gtk_button_size(void) { return sizeof(GtkButton); }

static Button *gtk_init_button(void *buf, const char *label) {
    GtkButton *b = buf;
    b->base.vt = &gtk_button_vt;
    strncpy(b->label, label, 63);
    b->label[63] = '\0';
    return &b->base;
}

/* Uso: */
char buf[128];
size_t needed = factory->vt->button_size();
if (needed > sizeof buf) { /* error */ }
Button *b = factory->vt->init_button(buf, "OK");
button_render(b);
/* No free — buf es stack o static */

/* O con un pool: */
static char widget_pool[4096];
static size_t pool_used = 0;

void *pool_alloc(size_t size) {
    if (pool_used + size > sizeof widget_pool) return NULL;
    void *ptr = widget_pool + pool_used;
    pool_used += size;
    return ptr;
}
```

Util en embedded porque:
1. **Sin malloc**: muchos RTOS no tienen heap, o el heap es tiny
2. **Determinista**: sin fragmentacion, sin allocation failure
   inesperado en runtime
3. **Predecible**: sabes en compile time cuanta memoria necesitas
4. **Rapido**: pool_alloc es O(1), sin free list

El trade-off: el caller debe saber cuanto espacio necesita
(`button_size()`), y la lifetime del producto esta atada al buffer.

</details>

---

### Ejercicio 9 — Comparar con C++

En C++, el Abstract Factory usa herencia virtual. Compara con la
implementacion en C:

```cpp
class GuiFactory {
public:
    virtual ~GuiFactory() = default;
    virtual Button* createButton(const char* label) = 0;
    virtual TextBox* createTextBox() = 0;
};

class GtkFactory : public GuiFactory {
    Button* createButton(const char* label) override;
    TextBox* createTextBox() override;
};
```

Lista al menos 3 cosas que C++ hace automaticamente que en C
hiciste manualmente.

<details><summary>Respuesta</summary>

1. **Vtable**: C++ genera el vtable automaticamente. En C,
   escribiste `GuiFactoryVt`, `ButtonVt`, etc. a mano, con riesgo
   de punteros NULL o firma incorrecta.

2. **Destructor virtual**: C++ `virtual ~GuiFactory()` garantiza
   que al hacer `delete factory`, se llama el destructor correcto.
   En C, implementaste `destroy` en el vtable y el caller debe
   llamarlo explicitamente.

3. **Override checking**: C++ `override` verifica en compile time
   que la subclase realmente overridea un metodo virtual. En C, si
   la firma del function pointer no coincide con la implementacion,
   es UB silencioso.

4. **Herencia de datos**: C++ `GtkFactory : public GuiFactory`
   hereda automaticamente los campos. En C, usaste el patron
   "base como primer campo" y casts manuales.

5. **RTTI**: C++ puede hacer `dynamic_cast<GtkFactory*>(factory)`
   para verificar el tipo. En C, no hay forma segura de verificar
   que tipo de factory tienes.

6. **pure virtual = 0**: C++ fuerza que las subclases implementen
   todos los metodos puros. En C, puedes dejar un puntero NULL
   en el vtable sin que el compilador avise.

Lo que C mantiene sobre C++:
- ABI estable (C++ manglea nombres, vtable layout varia)
- Control total (ves exactamente cada byte del vtable)
- No hay RTTI overhead (C++ guarda type_info por clase)
- Compilacion mas rapida (sin templates ni headers transitivos)

</details>

---

### Ejercicio 10 — Reflexion: cuando usar Abstract Factory

Responde sin ver la respuesta:

1. Nombra la diferencia clave entre Simple Factory y Abstract
   Factory en una frase.

2. Cual es el trade-off principal del Abstract Factory? (Que es
   facil y que es dificil?)

3. En un proyecto real, cuantas veces has visto (o crees ver)
   familias de objetos que varian por plataforma, backend, o tema?
   Da un ejemplo concreto.

4. Si solo tienes UNA familia y nunca planeas agregar otra, es
   Abstract Factory un overkill? Que usarias?

<details><summary>Respuesta</summary>

**1. Diferencia clave:**

Simple Factory: una funcion que elige entre variantes de UN tipo.
Abstract Factory: un objeto que crea familias de MULTIPLES tipos
relacionados, garantizando consistencia entre ellos.

**2. Trade-off principal:**

- **Facil**: agregar una nueva familia (nueva plataforma/backend).
  Solo creas archivos nuevos, el cliente no cambia.
- **Dificil**: agregar un nuevo tipo de producto (nuevo widget).
  Debes actualizar TODAS las concrete factories y el vtable.

Regla: si las familias crecen mas que los productos → Abstract
Factory. Si los productos crecen mas que las familias → otro
patron (Visitor, o simplemente funciones).

**3. Ejemplos reales:**

- **Bases de datos**: SQLite / PostgreSQL / MySQL — misma interfaz
  (connect, query, prepare), distinta implementacion.
- **Rendering**: OpenGL / Vulkan / Metal — mismos conceptos
  (shader, texture, buffer), APIs distintas.
- **Tests**: produccion vs mock — misma interfaz, mock retorna
  datos predefinidos.
- **UI themes**: Material / Cupertino / Fluent — mismos widgets,
  distinto aspecto.
- **Cloud providers**: AWS / GCP / Azure — mismos servicios
  (storage, compute, queue), distintos SDKs.

**4. Con una sola familia:**

Si, es overkill. Abstract Factory agrega una capa de abstraccion
que solo vale la pena si tienes (o planeas tener) multiples
familias.

Con una sola familia, usa:
- **Simple Factory** si quieres centralizar la creacion
- **Constructores directos** (`circle_new()`) si no necesitas
  centralizar
- **Config struct** si quieres constructores flexibles

No agregues abstracciones para un futuro que puede no llegar
("YAGNI"). Si manana necesitas una segunda familia, es relativamente
facil refactorizar a Abstract Factory en ese momento.

</details>
