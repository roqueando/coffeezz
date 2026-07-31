# UI Infrastructure Plan — Panels, Forms, and Plots

## Context
The current `main.c` is a flat demo with hard‑coded windows. We need a reusable software infrastructure so that:
- **Panels** are defined as structs with a draw callback — easy to add/remove windows.
- **Forms** are built declaratively by appending typed fields, then rendered with one call.
- **Plots** wrap Nuklear's chart API with a ring buffer for streaming data, and render as panels.

Everything should be modular so any future application (dashboards, settings dialogs, data viewers) can be assembled by instantiating these building blocks.

## Architecture

```
main.c
  └─ ui_infra.h / ui_infra.c
       ├─ ui_panel  (window descriptor + registry)
       ├─ ui_form   (declarative form builder)
       └─ ui_plot   (chart wrapper + ring buffer)
```

### File layout
| File | Purpose |
|------|---------|
| `ui_infra.h` | Public types and function declarations for panels, forms, plots |
| `ui_infra.c` | All implementations |
| `main.c` | App entry point — uses the infra to build a demo with multiple panels, a settings form, and a live plot |
| `Makefile` | Updated to compile `main.c` + `ui_infra.c` |

---

## 1. Panel System (`ui_panel`)

```c
typedef void (*ui_panel_draw_fn)(struct nk_context *ctx, struct ui_panel *panel);

typedef struct ui_panel {
    const char        *title;
    struct nk_rect      bounds;
    nk_flags            flags;
    nk_bool             visible;
    ui_panel_draw_fn    draw;
    void               *user_data;
    struct ui_panel    *next;          /* linked list */
} ui_panel;
```

A global (or app‑scoped) linked list holds all panels. Helper functions:

| Function | Purpose |
|----------|---------|
| `ui_panel_init(p, title, bounds, flags, draw, user_data)` | Initialise one panel struct |
| `ui_panel_add(head, p)` | Append to linked list (or `ui_panel_remove`) |
| `ui_panels_render(ctx, head)` | Iterate visible panels, call `nk_begin` / draw / `nk_end` |

Panels are **user‑allocated** (stack or heap) — the list just links them.

---

## 2. Form Builder (`ui_form`)

A dynamic array of field descriptors. Each field knows its type, label, and value pointer.

```c
typedef enum {
    UI_FIELD_LABEL,
    UI_FIELD_SEPARATOR,
    UI_FIELD_TEXT,
    UI_FIELD_INT,
    UI_FIELD_FLOAT,
    UI_FIELD_CHECKBOX,
    UI_FIELD_SLIDER_INT,
    UI_FIELD_SLIDER_FLOAT,
    UI_FIELD_COMBO,
    UI_FIELD_COLOR,
    UI_FIELD_BUTTON,
    UI_FIELD_CUSTOM,        /* arbitrary draw callback */
} ui_field_type;

typedef struct ui_field {
    ui_field_type  type;
    const char    *label;
    void          *value;          /* ptr to the real variable */

    /* Per‑type parameters */
    union {
        struct { int len; }                          text_max;
        struct { float min, max, step; }             slider_f;
        struct { int   min, max, step; }             slider_i;
        struct { const char **items; int count;
                 int *selected; }                    combo;
        struct { void (*cb)(void*); void *data; }    button;
        struct { void (*draw)(struct nk_context*,
                              void *data); void *data; } custom;
    } params;
} ui_field;

typedef struct ui_form {
    ui_field  *fields;
    int        count;
    int        cap;
} ui_form;
```

Builder API (each adds a field to the dynamic array):

| Function | Signature |
|----------|-----------|
| `ui_form_init(form)` | Allocate initial capacity |
| `ui_form_free(form)` | Free field array |
| `ui_form_add_label(form, text)` | Static text |
| `ui_form_add_separator(form)` | Horizontal line |
| `ui_form_add_text(form, label, buf, len)` | `nk_edit_string` |
| `ui_form_add_int(form, label, *val)` | `nk_property_int` |
| `ui_form_add_float(form, label, *val)` | `nk_property_float` |
| `ui_form_add_checkbox(form, label, *val)` | `nk_checkbox_label` |
| `ui_form_add_slider_int(form, label, *val, lo, hi, step)` | `nk_slider_int` |
| `ui_form_add_slider_float(form, label, *val, lo, hi, step)` | `nk_slider_float` |
| `ui_form_add_combo(form, label, *selected, items, count)` | `nk_combo` |
| `ui_form_add_color(form, label, *nk_colorf)` | `nk_color_picker` (collapsed combo) |
| `ui_form_add_button(form, label, callback, data)` | `nk_button_label` → callback on click |
| `ui_form_add_custom(form, draw_fn, data)` | Arbitrary draw callback |

Rendering:

```c
void ui_form_render(struct nk_context *ctx, ui_form *form);
/* Iterates fields, calls appropriate Nuklear widget, with sensible default layout */
```

A form can be embedded in any panel. A panel that wraps a form is just:

```c
void my_form_panel_draw(struct nk_context *ctx, ui_panel *panel) {
    ui_form_render(ctx, (ui_form *)panel->user_data);
}
```

---

## 3. Plot Widget (`ui_plot`)

Ring‑buffer wrapper for streaming data, plus a Nuklear chart render.

```c
#define UI_PLOT_MAX_POINTS  1024

typedef struct ui_plot {
    float               buffer[UI_PLOT_MAX_POINTS];
    int                 head;           /* next write position */
    int                 count;          /* how many written so far */
    enum nk_chart_type  type;           /* NK_CHART_LINES / NK_CHART_COLUMN */
    float               min_val, max_val;
    const char         *title;
    struct nk_color     color;
    struct nk_color     bg_color;
} ui_plot;
```

| Function | Purpose |
|----------|---------|
| `ui_plot_init(plot, type, title)` | Zero‑fill buffer, set default range |
| `ui_plot_push(plot, value)` | Append one point (circular) |
| `ui_plot_render(ctx, plot)` | Draw `nk_chart_begin` … `nk_chart_push` … `nk_chart_end` inside the current layout row |

The render function auto‑scales `min_val`/`max_val` as points are pushed.

A plot panel is just:

```c
void plot_panel_draw(struct nk_context *ctx, ui_panel *panel) {
    nk_layout_row_dynamic(ctx, 200, 1);
    ui_plot_render(ctx, (ui_plot *)panel->user_data);
}
```

---

## 4. Updated `main.c`

The demo will showcase all three pieces:

1. **Main panel** — brief instructions (using `ui_form` with a label and button).
2. **Settings panel** — a `ui_form` with checkbox, sliders, combo, color picker; values affect the plot appearance.
3. **Plot panel** — a `ui_plot` that streams a sine wave (updated each frame) and uses settings from the form.
4. **Hello panel** — kept from original for regression.

---

## Steps

- [ ] 1. Create `ui_infra.h` — all public types, enums, and function prototypes.
- [ ] 2. Create `ui_infra.c` — implement:
  - Panel helpers (`ui_panel_init`, `ui_panel_add`, `ui_panels_render`)
  - Form builder (`ui_form_init`, `ui_form_free`, all `ui_form_add_*`, `ui_form_render`)
  - Plot wrapper (`ui_plot_init`, `ui_plot_push`, `ui_plot_render`)
- [ ] 3. Update `Makefile` to compile `main.c` + `ui_infra.c` into `nuklear_app`.
- [ ] 4. Rewrite `main.c`:
  - Use `ui_app` struct wrapping GLFW + Nuklear init, main loop calling `ui_panels_render`.
  - Create panels: main, settings (form), plot (sine wave), hello.
  - Wiring: settings form values control plot colour/type in real time.
- [ ] 5. Build with `make` (zero warnings).
- [ ] 6. Run `./nuklear_app` — verify all four panels draw, form edits affect the live plot.

## Verification
- `make` compiles cleanly (`-Wall -Wextra`).
- App shows four panels: Main, Settings, Plot, Hello.
- Changing slider/checkbox/combo/color in Settings updates the Plot panel instantly.
- Plot draws a scrolling sine wave in real time.
- Clicking the Hello panel button still spawns/closes the "Hello, world!" child window.
