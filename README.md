# Coffeezz — Nuklear-based engineering tool GUI

A desktop application built with **C11**, **GLFW 3.3**, **OpenGL 3.3 Core**, and the
[Nuklear](https://github.com/Immediate-Mode-UI/Nuklear) immediate-mode GUI library.
Tools (e.g. boost converter calculator) are added as pluggable modules registered
through a sidebar panel — no changes to `main.c` needed.

---

## Quick start

```sh
# macOS only — install GLFW first
brew install glfw

# Build and run (debug)
make clean && make
./dist/debug/coffeez

# Or: make release → runs ./dist/release/coffeez

# Or grab the latest prebuilt binary (macOS arm64)
# https://github.com/.../releases/tag/latest
```

---

## Tests

```sh
make test
```

Tests cover panels, forms, plots, and the tool registry — no GL context needed.

---

## Architecture

```
coffeez
├── nuklear.h              ← single-header IMGUI (v4.13.3, public domain)
├── nuklear_glfw_gl3.h     ← GLFW + OpenGL 3.3 backend
│
├── ui_infra.h / ui_infra.c← reusable UI layer
│   ├── Panel system        window descriptor + linked-list registry
│   ├── Form builder        declarative typed-field form
│   └── Plot widget         ring-buffer chart with optional labelled axes
│
├── main.c                 ← entry point + GLFW/Nuklear bootstrap
├── Makefile               ← one-shot build (auto-discovers tool sources)
│
└── tools/
    ├── tool_registry.h/.c  sidebar button → panel visibility manager
    ├── tools.h/.c          master registry — all tools register here
    │
    ├── buck_converter/     example tool (placeholder)
    │   ├── buck_converter.h
    │   └── buck_converter.c
    │
    └── boost_converter/    Boost Converter calculator & simulator
        ├── boost_converter.h
        └── boost_converter.c
```

Nuklear is **immediate-mode** — the entire UI tree is rebuilt from data every
frame.  Our `ui_infra` layer provides *retained descriptors* (panels, form
definitions, plot buffers) so you describe what should exist **once**, and the
render loop re‑draws everything each frame.

---

## The three building blocks

### Panels (`ui_panel`)

A panel is a Nuklear window with a draw callback.

```c
typedef struct ui_panel {
    const char        *title;
    struct nk_rect      bounds;      // x, y, w, h
    nk_flags            flags;       // NK_WINDOW_BORDER | NK_WINDOW_TITLE | …
    nk_bool             visible;
    ui_panel_draw_fn    draw;        // called every frame inside nk_begin/nk_end
    void               *user_data;
    struct ui_panel    *next;
} ui_panel;
```

**Lifecycle:**

```c
// 1. Declare
static ui_panel my_panel;

// 2. Initialise
ui_panel_init(&my_panel, "Title", nk_rect(10, 10, 300, 200),
              NK_WINDOW_BORDER | NK_WINDOW_TITLE | NK_WINDOW_MOVABLE,
              my_draw_fn, NULL);

// 3. Register
ui_panel_add(&g_panels, &my_panel);

// 4. Hide / show
my_panel.visible = nk_false;

// 5. Remove (optional)
ui_panel_remove(&g_panels, &my_panel);
```

**Window flags** (combine with `|`): `NK_WINDOW_BORDER`, `NK_WINDOW_TITLE`,
`NK_WINDOW_MOVABLE`, `NK_WINDOW_SCALABLE`, `NK_WINDOW_MINIMIZABLE`,
`NK_WINDOW_CLOSABLE` (does NOT auto-close — check `nk_window_is_closed`).

**Draw callback** — always set a layout row before placing widgets:

```c
static void my_draw_fn(struct nk_context *ctx, ui_panel *panel) {
    (void)panel;
    nk_layout_row_dynamic(ctx, 30, 1);   // ← REQUIRED
    nk_label(ctx, "Hello!", NK_TEXT_LEFT);
}
```

**API reference:**

| Function | Purpose |
|---|---|
| `ui_panel_init(p, title, bounds, flags, draw, user_data)` | Initialise a panel |
| `ui_panel_add(head, p)` | Push onto linked list (prepend) |
| `ui_panel_remove(head, p)` | Unlink from list |
| `ui_panels_render(ctx, head)` | Render all visible panels in the list |

---

### Forms (`ui_form`)

A form is a **dynamic array of typed field descriptors**.  You build it once,
render it every frame.  Fields are bound to **live variables by pointer** — no
manual read‑back needed.

**Field types:**

| Constant | Widget | Value type |
|---|---|---|
| `UI_FIELD_LABEL` | `nk_label` | `const char *` |
| `UI_FIELD_SEPARATOR` | `nk_spacing` | none |
| `UI_FIELD_TEXT` | `nk_edit_string` | `char[]` buffer |
| `UI_FIELD_INT` | `nk_property_int` | `int *` |
| `UI_FIELD_FLOAT` | `nk_property_float` | `float *` |
| `UI_FIELD_CHECKBOX` | `nk_checkbox_label` | `nk_bool *` |
| `UI_FIELD_SLIDER_INT` | `nk_slider_int` | `int *` |
| `UI_FIELD_SLIDER_FLOAT` | `nk_slider_float` | `float *` |
| `UI_FIELD_COMBO` | `nk_combo_begin/end` | `int *` (selected index) |
| `UI_FIELD_COLOR` | `nk_color_picker` (collapsed) | `struct nk_colorf *` |
| `UI_FIELD_BUTTON` | `nk_button_label` + callback | none |
| `UI_FIELD_CUSTOM` | arbitrary draw callback | none |

**Builder API:**

```c
void ui_form_init(ui_form *form);
void ui_form_free(ui_form *form);

void ui_form_add_label      (form, const char *text);
void ui_form_add_separator  (form);
void ui_form_add_text       (form, label, char *buf, int max_len);
void ui_form_add_int        (form, label, int *val);
void ui_form_add_float      (form, label, float *val);
void ui_form_add_checkbox   (form, label, nk_bool *val);
void ui_form_add_slider_int (form, label, int *val, int min, int max, int step);
void ui_form_add_slider_float(form, label, float *val, float min, float max, float step);
void ui_form_add_combo      (form, label, int *selected, const char **items, int count);
void ui_form_add_color      (form, label, struct nk_colorf *color);
void ui_form_add_button     (form, label, void (*cb)(void *data), void *data);
void ui_form_add_custom     (form, void (*draw)(ctx, void *data), void *data);

void ui_form_render(struct nk_context *ctx, ui_form *form);
```

**Typical pattern:**

```c
static ui_form   my_form;
static nk_bool   enable_foo = nk_false;
static float     speed      = 1.0f;

// Build once at init:
ui_form_init(&my_form);
ui_form_add_label(&my_form, "Settings");
ui_form_add_separator(&my_form);
ui_form_add_checkbox(&my_form, "Enable Foo", &enable_foo);
ui_form_add_slider_float(&my_form, "Speed", &speed, 0.0f, 10.0f, 0.1f);
ui_form_add_button(&my_form, "Apply", my_apply_cb, NULL);

// Render every frame inside a panel draw callback:
static void settings_draw(struct nk_context *ctx, ui_panel *panel) {
    (void)panel;
    ui_form_render(ctx, &my_form);
}
```

---

### Plots (`ui_plot`)

Ring-buffer chart with optional labelled axes.

```c
#define UI_PLOT_MAX_POINTS  8192

typedef struct ui_plot {
    float               buffer[UI_PLOT_MAX_POINTS];
    int                 head, count;
    enum nk_chart_type  type;           // NK_CHART_LINES or NK_CHART_COLUMN
    float               min_val, max_val;
    const char         *title;

    struct nk_color     line_color;     // used when use_custom_colors = true
    struct nk_color     bg_color;
    nk_bool             use_custom_colors;

    // Axes (set x_max > x_min to enable labelled axes with tick marks)
    float               x_min, x_max;
    const char         *x_label, *y_label;
} ui_plot;
```

**API:**

```c
void  ui_plot_init       (ui_plot *plot, const char *title, enum nk_chart_type type);
void  ui_plot_push       (ui_plot *plot, float value);
void  ui_plot_set_x_range(ui_plot *plot, float x_min, float x_max);
void  ui_plot_render     (struct nk_context *ctx, ui_plot *plot);
```

**Typical pattern:**

```c
static ui_plot g_plot;

// Init:
ui_plot_init(&g_plot, "Vout(t)", NK_CHART_LINES);
g_plot.use_custom_colors = nk_true;
g_plot.line_color = nk_rgb(50, 200, 100);

// Enable labelled axes:
ui_plot_set_x_range(&g_plot, 0.0f, 5.0f);
g_plot.x_label = "Time (s)";
g_plot.y_label = "Vout (V)";

// Every frame, push a sample:
ui_plot_push(&g_plot, my_value);

// In panel draw:
static void plot_draw(struct nk_context *ctx, ui_panel *panel) {
    nk_layout_row_dynamic(ctx, 200, 1);
    ui_plot_render(ctx, &g_plot);
}
```

- Y-axis auto‑scales as data is pushed (5 % margin).
- When `x_max > x_min`, a **custom renderer** draws axis lines, tick marks,
  numeric labels, and a full grid — no Nuklear chart fallback is used.
- When `x_max <= x_min` (default), the original `nk_chart_*` path is used
  (backward compatible — the sine‑wave demo in `main.c` still works).

---

### Layout quick reference

Call **one** of these before any widget inside a panel draw callback:

```c
// Fixed row height, N equal-width columns:
nk_layout_row_dynamic(ctx, row_height_px, num_columns);

// Fixed row height, each column a specific pixel width:
nk_layout_row_static(ctx, row_height_px, column_width_px, num_columns);

// Variable column widths (array of ratios):
float ratios[] = { 0.3f, 0.7f };
nk_layout_row(ctx, NK_DYNAMIC, 30, 2, ratios);
```

---

## Include order (mandatory)

```c
// 1. Platform defs
#define GLFW_INCLUDE_GLCOREARB
#define GL_SILENCE_DEPRECATION
#include <GLFW/glfw3.h>

// 2. Nuklear feature flags
#define NK_INCLUDE_FIXED_TYPES
#define NK_INCLUDE_STANDARD_IO
// … (see main.c for the full set)

// 3. Nuklear implementation (ONLY in the file that defines main()!)
#define NK_IMPLEMENTATION
#define NK_GLFW_GL3_IMPLEMENTATION
#include "nuklear.h"
#include "nuklear_glfw_gl3.h"

// 4. Our infra (always after nuklear.h)
#include "ui_infra.h"
```

Tool `.c` files must **not** define `NK_IMPLEMENTATION` or
`NK_GLFW_GL3_IMPLEMENTATION` (duplicate‑symbol linker errors).

---

## Tool registry — how plugins work

The registry lives in `tools/tool_registry.h/.c` and decouples `main.c` from
individual tools.  Tools never touch `main.c` directly.

### Architecture

```
main.c
  └─ tools_init()                ← called once during startup
       ├─ tool_registry_init()   ← reset slot array
       ├─ buck_converter_register(head, sidebar_w, win_w, win_h)
       │    └─ tool_register() → creates panel, stores in g_slots[]
       └─ boost_converter_register(head, sidebar_w, win_w, win_h)
            └─ tool_register() → creates panel, stores in g_slots[]

main loop
  ├─ tool_registry_update()      ← sync panel.visible ← slot.visible
  └─ ui_panels_render()          ← draws panels (including registry panels)

sidebar draw callback
  └─ tool_registry_draw_sidebar(ctx)  ← one toggle button per tool
```

### Public API

```c
/* Descriptor passed by each tool to tool_register() */
typedef struct tool_desc {
    const char         *button_label;  /* sidebar button text           */
    const char         *panel_title;   /* window title bar              */
    ui_panel_draw_fn    draw;          /* draw callback (panel content) */
    void               *user_data;     /* passed to draw callback       */
} tool_desc;

/* --- Registry API --- */

/* Call once before any tool_register() */
void  tool_registry_init(void);

/* Register a tool.  Returns the new ui_panel* (owned by registry). */
ui_panel * tool_register(ui_panel **head, int sidebar_w, int win_w, int win_h,
                         const tool_desc *desc);

/* Call from the sidebar draw callback to render tool toggle buttons */
void  tool_registry_draw_sidebar(struct nk_context *ctx);

/* Call each frame to sync panel visibility from toggle state */
void  tool_registry_update(void);

/* Call at the START of each tool's draw callback to handle the close (×) button */
void  tool_registry_check_close(struct nk_context *ctx, ui_panel *pnl);
```

---

## Creating a new tool (by agent)

The AGENTS.md file defines slash commands that an AI agent can interpret.
For example:

```
/create-tool "Buck Converter" "Buck Converter"
```

The agent will:

1. Create `tools/buck_converter/buck_converter.h` and `.c` with a
   `buck_converter_register()` function and a placeholder draw callback.
2. Add `#include "tools/buck_converter/buck_converter.h"` to `tools/tools.h`.
3. Call `buck_converter_register(head, sidebar_w, win_w, win_h)` inside
   `tools_init()` in `tools/tools.c`.
4. Run `make clean && make` to verify no warnings.

The sidebar button, panel visibility toggle, and close‑button handling are all
automatic — no `main.c` changes.

---

## Creating a new tool (by hand)

Follow the same steps the agent would:

### 1. Create the tool directory

```
mkdir -p tools/my_tool
```

### 2. Write the header — `tools/my_tool/my_tool.h`

```c
#ifndef MY_TOOL_H_
#define MY_TOOL_H_

#include "ui_infra.h"

void my_tool_register(ui_panel **head, int sidebar_w, int win_w, int win_h);

#endif /* MY_TOOL_H_ */
```

### 3. Write the implementation — `tools/my_tool/my_tool.c`

```c
#include "nuklear.h"
#include "ui_infra.h"
#include "tools/tool_registry.h"
#include "tools/my_tool/my_tool.h"

/* ---- Tool-specific state (file‑scope statics) ---- */
static float g_some_value = 42.0f;

/* ---- Draw callback ---- */
static void my_tool_draw(struct nk_context *ctx, ui_panel *pnl)
{
    /* REQUIRED: handle the close (×) button */
    tool_registry_check_close(ctx, pnl);

    /* Set a layout row, then place widgets */
    nk_layout_row_dynamic(ctx, 25, 2);
    nk_label(ctx, "Value:", NK_TEXT_LEFT);
    nk_property_float(ctx, "#val", 0.0f, &g_some_value, 100.0f, 0.1f, 1.0f);
}

/* ---- Registration ---- */
void my_tool_register(ui_panel **head, int sidebar_w, int win_w, int win_h)
{
    tool_desc desc = {
        .button_label = "My Tool",
        .panel_title  = "My Tool",
        .draw         = my_tool_draw,
        .user_data    = NULL
    };
    tool_register(head, sidebar_w, win_w, win_h, &desc);
}
```

### 4. Register in `tools/tools.h`

Add the include line:

```c
#include "tools/my_tool/my_tool.h"
```

### 5. Register in `tools/tools.c`

Add the call inside `tools_init()`:

```c
my_tool_register(head, sidebar_w, win_w, win_h);
```

### 6. Build

```sh
make clean && make
```

The Makefile auto‑discovers all `.c` files under `tools/` via wildcards — no
Makefile changes needed.

---

## Project conventions

| Convention | Detail |
|---|---|
| **Panels** | `static` file‑scope, live forever, toggled via `.visible` |
| **Forms** | `static`, built once in init, rendered every frame |
| **Form values** | `static` variables at file scope — single source of truth |
| **Draw path** | No allocations; all heavy work outside the main loop |
| **Panel flags** | Always include `NK_WINDOW_BORDER \| NK_WINDOW_TITLE \| NK_WINDOW_MOVABLE` |
| **Naming** | `g_<snake>` for globals, `<snake>_panel` / `_form` / `_plot` for UI objects, `draw_<snake>` for callbacks |
| **Indent** | 4 spaces, no tabs |

---

## Tools included

| Tool | Description |
|---|---|
| **Buck Converter** | Placeholder — add components here |
| **Boost Converter** | Ideal boost converter with parameter inputs, steady‑state formulas, time‑domain simulation, and live Vout(t) plot with labelled axes |

---

## Dependencies

- **GLFW 3.3** — `brew install glfw`
- **OpenGL 3.3 Core**, **Cocoa**, **IOKit** — pre‑installed on macOS
