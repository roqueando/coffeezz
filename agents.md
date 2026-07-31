# agents.md — Project reference & feature-playbook

## 1. Architecture overview

```
nuklear_app   (C11 + GLFW 3.3 + OpenGL 3.3 Core)
├── nuklear.h               ← single‑header IMGUI (v4.13.3, public domain)
├── nuklear_glfw_gl3.h      ← backend: GLFW window + OpenGL 3.3 renderer
│
├── ui_infra.h / ui_infra.c ← OUR REUSABLE LAYER (see §3)
│   ├── Panel system        (window descriptor + linked‑list registry)
│   ├── Form builder        (declarative typed‑field form)
│   └── Plot widget         (ring‑buffer chart over nk_chart)
│
├── main.c                  ← application entry point & demo
└── Makefile                 ← one‑shot build
```

**Key property:** Nuklear is *immediate‑mode*. Every frame the entire UI tree is rebuilt from data — there is no retained widget tree. Our `ui_infra` layer provides *retained descriptors* (panels, form definitions, plot buffers) so you describe what should exist *once*, and the render loop re‑draws it every frame.

---

## 2. File map

| File | Role |
|------|------|
| `nuklear.h` | Nuklear core. Do **not** edit — refresh from `Immediate-Mode-UI/Nuklear` master. |
| `nuklear_glfw_gl3.h` | GLFW+OpenGL3 backend. Do **not** edit — refresh from same repo. |
| `ui_infra.h` | Public types & prototypes for panels, forms, plots. **Include after nuklear.h.** |
| `ui_infra.c` | Implementations. Compiled as a separate `.o` — no `NK_IMPLEMENTATION` here. |
| `main.c` | App entry point. Holds `NK_IMPLEMENTATION` + `NK_GLFW_GL3_IMPLEMENTATION`. Includes `nuklear.h` *then* `ui_infra.h`. |
| `Makefile` | Builds `nuklear_app` from `main.c` + `ui_infra.c`. |

---

## 3. Build

```sh
make          # compile & link
make clean    # remove artifacts
./nuklear_app # launch
```

**Dependencies** (macOS via Homebrew):
- `glfw` — `brew install glfw`
- System frameworks: OpenGL, Cocoa, IOKit (pre‑installed)

**Flags in use:**
- `-std=c11 -Wall -Wextra -O2`
- `-I/opt/homebrew/opt/glfw/include`
- `-L/opt/homebrew/opt/glfw/lib -lglfw -framework OpenGL -framework Cocoa -framework IOKit`

---

## 4. The three building blocks

### 4.1 Panels (`ui_panel`)

A panel = a Nuklear window with a draw callback.

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
// 1. Declare (stack or heap)
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

**Available window flags** (combine with `|`):

| Flag | Effect |
|------|--------|
| `NK_WINDOW_BORDER` | Draw a border |
| `NK_WINDOW_TITLE` | Show title bar |
| `NK_WINDOW_MOVABLE` | Draggable |
| `NK_WINDOW_SCALABLE` | Resizable |
| `NK_WINDOW_MINIMIZABLE` | Has minimise button |
| `NK_WINDOW_CLOSABLE` | Has close button (does NOT auto-close — check `nk_window_is_closed`) |

**Draw callback signature:**
```c
void my_draw_fn(struct nk_context *ctx, ui_panel *panel);
```
Inside it you must set a layout row before placing widgets (see §5.1).

---

### 4.2 Forms (`ui_form`)

A form is a **dynamic array of typed field descriptors**. You build it once with builder calls, render it every frame with `ui_form_render`. Fields are bound to **live variables by pointer** — no manual read‑back needed.

**Field types:**

| Enum value | Nuklear widget | Value type |
|------------|---------------|------------|
| `UI_FIELD_LABEL` | `nk_label` | `const char *` (passed as label) |
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
void ui_form_init(ui_form *form);                           // must call first
void ui_form_free(ui_form *form);                           // call at shutdown

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

void ui_form_render(struct nk_context *ctx, ui_form *form);  // call every frame
```

**Typical pattern:**

```c
// In global scope:
static ui_form   my_form;
static nk_bool   enable_foo = nk_false;
static float     speed      = 1.0f;

// At init:
ui_form_init(&my_form);
ui_form_add_label(&my_form, "Settings");
ui_form_add_separator(&my_form);
ui_form_add_checkbox(&my_form, "Enable Foo", &enable_foo);
ui_form_add_slider_float(&my_form, "Speed", &speed, 0.0f, 10.0f, 0.1f);
ui_form_add_button(&my_form, "Apply", my_apply_cb, NULL);

// In panel draw:
static void settings_draw(struct nk_context *ctx, ui_panel *panel) {
    ui_form_render(ctx, &my_form);
}
```

---

### 4.3 Plots (`ui_plot`)

Ring‑buffer wrapper around Nuklear's `nk_chart_begin` / `nk_chart_push` / `nk_chart_end`.

```c
typedef struct ui_plot {
    float               buffer[1024];
    int                 head, count;
    enum nk_chart_type  type;          // NK_CHART_LINES or NK_CHART_COLUMN
    float               min_val, max_val;
    const char         *title;
    struct nk_color     line_color;    // only used if use_custom_colors is true
    struct nk_color     bg_color;
    nk_bool             use_custom_colors;
} ui_plot;
```

**API:**

```c
void ui_plot_init  (ui_plot *plot, const char *title, enum nk_chart_type type);
void ui_plot_push  (ui_plot *plot, float value);   // call every frame that produces data
void ui_plot_render(struct nk_context *ctx, ui_plot *plot);  // call inside panel draw
```

**Typical pattern:**

```c
static ui_plot g_plot;

// Init:
ui_plot_init(&g_plot, "Sensor", NK_CHART_LINES);

// Every frame, push a sample:
ui_plot_push(&g_plot, read_sensor());

// In panel draw:
static void plot_draw(struct nk_context *ctx, ui_panel *panel) {
    nk_layout_row_dynamic(ctx, 200, 1);
    ui_plot_render(ctx, &g_plot);
}
```

Auto‑scaling: `min_val`/`max_val` expand as you push values. A 5 % margin is added.

---

## 5. Feature playbook (copy‑paste recipes)

### 5.1 Add a new panel

```c
// === STEP 1: declare the panel struct ===
static ui_panel my_panel;

// === STEP 2: write the draw callback ===
static void my_panel_draw(struct nk_context *ctx, ui_panel *panel)
{
    (void)panel;
    nk_layout_row_dynamic(ctx, 30, 1);       // ← ALWAYS set a layout row first!
    nk_label(ctx, "Hello, world!", NK_TEXT_LEFT);
}

// === STEP 3: register in main() ===
ui_panel_init(&my_panel, "My Panel", nk_rect(10, 10, 300, 200),
              NK_WINDOW_BORDER | NK_WINDOW_TITLE | NK_WINDOW_MOVABLE,
              my_panel_draw, NULL);
ui_panel_add(&g_panels, &my_panel);
```

### 5.2 Add a panel that wraps a form

```c
static ui_panel form_panel;
static ui_form  my_form;
static nk_bool  opt_a = nk_false;
static int      count = 5;

static void form_panel_draw(struct nk_context *ctx, ui_panel *panel) {
    (void)panel;
    ui_form_render(ctx, &my_form);
}

// In main():
ui_form_init(&my_form);
ui_form_add_checkbox(&my_form, "Option A", &opt_a);
ui_form_add_slider_int(&my_form, "Count", &count, 0, 100, 1);

ui_panel_init(&form_panel, "Config", nk_rect(10, 10, 250, 300),
              NK_WINDOW_BORDER | NK_WINDOW_TITLE | NK_WINDOW_MOVABLE,
              form_panel_draw, NULL);
ui_panel_add(&g_panels, &form_panel);
```

### 5.3 Add a plot panel

```c
static ui_plot  my_plot;
static ui_panel plot_panel;

static void plot_panel_draw(struct nk_context *ctx, ui_panel *panel) {
    (void)panel;
    nk_layout_row_dynamic(ctx, 200, 1);
    ui_plot_render(ctx, &my_plot);
}

// In main():
ui_plot_init(&my_plot, "Wave", NK_CHART_LINES);

ui_panel_init(&plot_panel, "Graph", nk_rect(300, 10, 500, 280),
              NK_WINDOW_BORDER | NK_WINDOW_TITLE | NK_WINDOW_MOVABLE,
              plot_panel_draw, NULL);
ui_panel_add(&g_panels, &plot_panel);

// In main loop (every frame):
ui_plot_push(&my_plot, sinf(my_time) * 0.5f);
```

### 5.4 Make one panel control another panel's data

All form values are **pointers to real variables**. Just use those same variables in the plot/other panel's draw callback:

```c
static float my_amplitude = 1.0f;

// Form field binds to my_amplitude:
ui_form_add_slider_float(&my_form, "Amplitude", &my_amplitude, 0, 5, 0.1f);

// In main loop:
float sample = sinf(t) * my_amplitude;
ui_plot_push(&my_plot, sample);
```

No event wiring needed — the form writes directly to the variable, the plot reads it.

### 5.5 Toggle panel visibility

```c
// Show:
my_panel.visible = nk_true;

// Hide:
my_panel.visible = nk_false;
```

From a button callback (form button or raw button):

```c
static void on_show_clicked(void *data) {
    ((ui_panel *)data)->visible = nk_true;
}

// Register:
ui_form_add_button(&form, "Show Panel", on_show_clicked, &my_panel);
```

### 5.6 Switch chart type at runtime

```c
// Toggle between NK_CHART_LINES and NK_CHART_COLUMN:
my_plot.type = NK_CHART_COLUMN;

// Or from a combo (0 = lines, 1 = columns):
my_plot.type = (combo_index == 0) ? NK_CHART_LINES : NK_CHART_COLUMN;
```

### 5.7 Change plot colours per point series

```c
my_plot.use_custom_colors = nk_true;
my_plot.line_color = nk_rgb(200, 50, 50);  // red
```

### 5.8 Add a custom widget (raw Nuklear inside a form)

Use `UI_FIELD_CUSTOM` to embed anything Nuklear can draw:

```c
static void draw_my_thing(struct nk_context *ctx, void *data) {
    (void)data;
    nk_layout_row_static(ctx, 64, 64, 1);
    nk_image(ctx, my_image);
}

ui_form_add_custom(&my_form, draw_my_thing, NULL);
```

### 5.9 Add a field that doesn't exist yet

1. Add the new constant to `ui_field_type` in `ui_infra.h`.
2. Add any needed parameters to the `params` union.
3. Add a builder function in `ui_infra.c` following the existing pattern.
4. Add a `case` in `ui_form_render()` that calls the right Nuklear widget.

### 5.10 Create a panel with multiple charts (slots)

Nuklear supports multiple overlaid data series through slots. The current `ui_plot` wrapper renders a single series. For multi‑slot charts, call Nuklear directly inside a custom field:

```c
static void draw_multi_chart(struct nk_context *ctx, void *data) {
    if (nk_chart_begin(ctx, NK_CHART_LINES, N, 0, 1)) {
        nk_chart_add_slot(ctx, NK_CHART_LINES, N, 0, 1);    // series 2
        nk_chart_add_slot(ctx, NK_CHART_COLUMN, N, 0, 1);   // series 3
        for (int i = 0; i < N; i++) {
            nk_chart_push_slot(ctx, series1[i], 0);
            nk_chart_push_slot(ctx, series2[i], 1);
            nk_chart_push_slot(ctx, series3[i], 2);
        }
        nk_chart_end(ctx);
    }
}
```

---

## 6. Layout quick reference

Inside a panel draw callback you **must** call a layout function before any widget. The two most useful:

```c
// Fixed row height, split into `cols` equal columns:
nk_layout_row_dynamic(ctx, row_height_px, num_columns);

// Fixed row height, each column has a specific pixel width:
nk_layout_row_static(ctx, row_height_px, column_width_px, num_columns);

// Variable column widths (array of ratios):
float ratios[] = { 0.3f, 0.7f };
nk_layout_row(ctx, NK_DYNAMIC, 30, 2, ratios);
```

---

## 7. Include order (mandatory)

Every `.c` file that uses Nuklear + the infra must follow this order:

```c
// 1. Platform defs
#define GLFW_INCLUDE_GLCOREARB
#define GL_SILENCE_DEPRECATION
#include <GLFW/glfw3.h>

// 2. Nuklear feature flags
#define NK_INCLUDE_FIXED_TYPES
#define NK_INCLUDE_STANDARD_IO
#define NK_INCLUDE_STANDARD_VARARGS
#define NK_INCLUDE_DEFAULT_ALLOCATOR
#define NK_INCLUDE_VERTEX_BUFFER_OUTPUT
#define NK_INCLUDE_FONT_BAKING
#define NK_INCLUDE_DEFAULT_FONT

// 3. Nuklear implementation (ONLY in the file that defines main()!)
#define NK_IMPLEMENTATION
#define NK_GLFW_GL3_IMPLEMENTATION
#include "nuklear.h"
#include "nuklear_glfw_gl3.h"

// 4. Our infra (always after nuklear.h)
#include "ui_infra.h"
```

In `.c` files that are *not* the entry point (e.g. `ui_infra.c`), `NK_IMPLEMENTATION` and `NK_GLFW_GL3_IMPLEMENTATION` must **not** be defined — otherwise you get duplicate‑symbol linker errors.

---

## 8. Project conventions

- **All panels are `static`** (allocated once, live forever). Use `.visible` to toggle.
- **Forms are `static`** and built once in `main()` before the loop.
- **Form values are `static` variables** at file scope — they act as the single source of truth.
- **No allocations in the draw path** — all heavy work happens outside the main loop.
- **`ui_panels_render` is called once per frame** with the head of the panel linked list.
- **Window flags** should include at least `NK_WINDOW_BORDER | NK_WINDOW_TITLE | NK_WINDOW_MOVABLE` for a usable window.

---

## 9. Quick command reference

| Task | Command / snippet |
|------|-------------------|
| Build | `make` |
| Clean build | `make clean && make` |
| Run | `./nuklear_app` |
| New panel | `ui_panel_init` → `ui_panel_add` |
| New form | `ui_form_init` → `ui_form_add_*` → embed in panel draw |
| New plot | `ui_plot_init` → `ui_plot_push` each frame → `ui_plot_render` in panel draw |
| Hide panel | `panel.visible = nk_false;` |
| Show popup | `panel.visible = nk_true;` |
| Change plot colour | `plot.use_custom_colors = nk_true; plot.line_color = nk_rgb(r,g,b);` |
| Toggle chart type | `plot.type = NK_CHART_COLUMN;` |
| Add raw Nuklear widget | Use `UI_FIELD_CUSTOM` or call Nuklear directly in panel draw |
| Add new field type | Extend enum, union, builder function, render case (see §5.9) |

---

## 10. Agent commands (slash‑commands for pi agent)

Each command below tells an AI agent exactly what to add to `main.c` and where.
The agent must follow these specifications precisely — they are executable recipes,
not suggestions.

---

### `/create-tool`

```
/create-tool <button-label> <panel-title>
```

**What it builds:** A button (placed inside the existing **Main** panel)
that, when clicked, shows a new side‑panel docked to the right side of the
window. The new panel is empty — you define its contents afterward with
`/add-form` or `/add-plot`.

**Agent steps:**

1. **Add a `static nk_bool`** toggle variable in the "Application state" region
   (near `g_hello_visible`):
   ```c
   static nk_bool g_<panel_name>_visible = nk_false;
   ```
   where `<panel_name>` is a snake_case version of `<panel-title>`.

2. **Write a draw callback** in the "Panel callbacks" region:
   ```c
   static void draw_<panel_name>(struct nk_context *ctx, ui_panel *panel) {
       (void)panel;
       nk_layout_row_dynamic(ctx, 30, 1);
       nk_label(ctx, "Contents go here", NK_TEXT_LEFT);
   }
   ```

3. **Register the panel** in `main()` right after the other `ui_panel_init` calls:
   ```c
   static ui_panel <panel_name>_panel;
   float pw = 300;   /* panel width */
   float ph = (float)WINDOW_HEIGHT - 20;
   ui_panel_init(&<panel_name>_panel, "<panel-title>",
                 nk_rect(WINDOW_WIDTH - pw - 10, 10, pw, ph),
                 NK_WINDOW_BORDER | NK_WINDOW_TITLE |
                 NK_WINDOW_MOVABLE | NK_WINDOW_SCALABLE |
                 NK_WINDOW_CLOSABLE,
                 draw_<panel_name>, NULL);
   <panel_name>_panel.visible = nk_false;
   ui_panel_add(&g_panels, &<panel_name>_panel);
   ```

4. **Add the button** inside `draw_main` (the Main panel's draw callback),
   before the closing brace:
   ```c
   nk_layout_row_static(ctx, 30, 140, 1);
   if (nk_button_label(ctx, "<button-label>"))
       g_<panel_name>_visible = nk_true;
   ```

5. **Update panel visibility** in the main loop — find where
   `hello_panel.visible = g_hello_visible;` is set and add the same pattern:
   ```c
   <panel_name>_panel.visible = g_<panel_name>_visible;
   ```

6. **Handle close‑button**: the `NK_WINDOW_CLOSABLE` flag puts an × on the
   title bar. Add a check inside the new panel's draw callback:
   ```c
   if (nk_window_is_closed(ctx, "<panel-title>"))
       g_<panel_name>_visible = nk_false;
   ```

7. **Rebuild** with `make` and verify no warnings.

---

### `/add-form`

```
/add-form <panel-name> <form-name>
```

**What it builds:** Inserts a new `ui_form` into an existing panel's draw
callback. The form is built once in `main()` and rendered every frame.

**Agent steps:**

1. Declare form and bound variables in "Application state":
   ```c
   static ui_form <form_name>_form;
   /* form value variables go here */
   ```

2. Build the form in `main()` after the other `ui_form_init` blocks:
   ```c
   ui_form_init(&<form_name>_form);
   /* ui_form_add_* calls go here */
   ```

3. Replace the placeholder `nk_label` in the target panel's draw callback with:
   ```c
   ui_form_render(ctx, &<form_name>_form);
   ```

4. If the form needs cleanup, add `ui_form_free(&<form_name>_form);` before
   `nk_glfw3_shutdown` in the shutdown section.

5. Rebuild.

---

### `/add-field`

```
/add-field <form-name> <field-type> <label> [extra…]
```

**What it builds:** Appends a single field to an existing form.

**Agent steps:**

1. If the field binds to a variable that doesn't exist yet, declare it in
   "Application state" next to the form:
   ```c
   static <c-type> g_<varname> = <default>;
   ```

2. Add one line after the last `ui_form_add_*` call for that form:

   | `<field-type>` | C code to inject |
   |---|---|
   | `label` | `ui_form_add_label(&<form>, "<label>");` |
   | `sep` | `ui_form_add_separator(&<form>);` |
   | `text` | `ui_form_add_text(&<form>, "<label>", <buf>, <max>);` |
   | `int` | `ui_form_add_int(&<form>, "<label>", &g_<var>);` |
   | `float` | `ui_form_add_float(&<form>, "<label>", &g_<var>);` |
   | `checkbox` | `ui_form_add_checkbox(&<form>, "<label>", &g_<var>);` |
   | `slider-int` | `ui_form_add_slider_int(&<form>, "<label>", &g_<var>, <min>, <max>, <step>);` |
   | `slider-float` | `ui_form_add_slider_float(&<form>, "<label>", &g_<var>, <min>, <max>, <step>);` |
   | `combo` | `ui_form_add_combo(&<form>, "<label>", &g_<sel>, <items>, <count>);` |
   | `color` | `ui_form_add_color(&<form>, "<label>", &g_<colorf>);` |
   | `button` | `ui_form_add_button(&<form>, "<label>", <cb>, <data>);` (also write the callback) |
   | `custom` | `ui_form_add_custom(&<form>, <draw_fn>, <data>);` (also write the draw fn) |

3. Rebuild.

---

### `/add-plot`

```
/add-plot <panel-name> <chart-title>
```

**What it builds:** Adds a live chart to an existing panel.

**Agent steps:**

1. Declare the plot in "Application state":
   ```c
   static ui_plot <panel_name>_plot;
   ```

2. Init the plot in `main()` before the loop:
   ```c
   ui_plot_init(&<panel_name>_plot, "<chart-title>", NK_CHART_LINES);
   ```

3. Replace the panel's draw callback body with:
   ```c
   nk_layout_row_dynamic(ctx, 30, 1);
   nk_label(ctx, <panel_name>_plot.title, NK_TEXT_CENTERED);
   nk_layout_row_dynamic(ctx, 200, 1);
   ui_plot_render(ctx, &<panel_name>_plot);
   ```

4. Feed data in the main loop (find where `ui_plot_push(&g_plot, …)` is called):
   ```c
   ui_plot_push(&<panel_name>_plot, <value-expression>);
   ```

5. Rebuild.

---

### `/create-panel`

```
/create-panel <title> <x> <y> <w> <h>
```

**What it builds:** A bare standalone panel with a placeholder body.
No button triggers it — it starts visible.

**Agent steps:** Same as `/create-tool` steps 1–3 and 7, but skip steps 4–5
(the button and visibility toggle). The panel is visible from start:
`<panel>_panel.visible = nk_true;`.

---

### `/remove-panel`

```
/remove-panel <panel-name>
```

**What it does:** Completely removes a panel and all its supporting code.

**Agent steps:**

1. Remove the `static ui_panel <name>_panel;` declaration.
2. Remove the `ui_panel_init(&<name>_panel, …)` block and the
   `ui_panel_add(&g_panels, &<name>_panel);` line.
3. Remove the draw callback `draw_<name>()`.
4. If the panel had an associated toggle variable, remove its declaration
   and the `…_panel.visible = …` line in the main loop.
5. If the panel had an associated button in another panel, remove the
   `nk_button_label` → `g_<name>_visible = …` block.
6. Rebuild and verify no warnings about unused variables.

---

### `/remove-form`

```
/remove-form <form-name>
```

**Agent steps:**

1. Remove the `static ui_form <name>_form;` declaration.
2. Remove all `ui_form_add_*` lines for that form in `main()`.
3. Remove the `ui_form_init` and `ui_form_free` calls.
4. Replace `ui_form_render(ctx, &<name>_form);` in the panel draw callback
   with a placeholder label.
5. Remove any static variables that were *only* used by this form.
6. Rebuild.

---

### `/remove-plot`

```
/remove-plot <plot-name>
```

**Agent steps:**

1. Remove `static ui_plot <name>_plot;`.
2. Remove `ui_plot_init(&<name>_plot, …);` from `main()`.
3. Remove the `ui_plot_push(&<name>_plot, …);` line from the main loop.
4. Remove the `ui_plot_render(ctx, &<name>_plot);` call from the panel
   draw callback (replace with a placeholder if the panel becomes empty).
5. Rebuild.

---

### Command execution rules for the agent

- **Always read `main.c` first** before injecting code — positions may have
  shifted since the last command.
- **Use the exact anchor points** described in each command to place new code.
- **Match indentation and style** of surrounding code (4-space indent, no tabs).
- **Variable naming convention:** `g_<lower_snake>` for file‑scope globals,
  `<snake>_panel` for panels, `<snake>_form` for forms, `<snake>_plot` for plots,
  `draw_<snake>` for draw callbacks.
- **After every command, run `make clean && make`** and report any errors.
- **If a command fails,** fix the issue before proceeding. Common failure modes:
  - duplicate variable name → rename with suffix
  - missing `nk_window_is_closed` → add `NK_WINDOW_CLOSABLE` to panel flags and insert the check
  - form variable not declared → declare it in "Application state"
