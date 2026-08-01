# Boost Converter — Plot & Simulation Improvements

## Context

The user requested three improvements to the boost converter tool:

1. **Configurable time limit** — the plot should run for a configurable duration (default 5 s) then pause.
2. **Parameter changes restart the plot** — after the simulation finishes (or is paused), adjusting any input parameter should reset the plot and resume automatically.
3. **Numbered axes** — the plot chart needs visible numeric tick labels on the Y-axis (voltage) and X-axis (time).

## Approach

### 1. Configurable time limit + auto-pause

Add a `g_time_limit` static (default 5.0 s, editable via a `nk_property_float` in the panel). The simulation only advances when `g_sim_t < g_time_limit`. When the limit is hit, `g_running` is set to `nk_false` (paused). A **Restart** button resets everything (`reset_sim`) and sets `g_running = nk_true`.

### 2. Auto-restart on parameter change

Track a snapshot of all six parameters every frame (a simple float checksum or a dedicated snapshot struct). If the snapshot differs from the previous frame *and* the simulation is not currently running (`!g_running`), automatically call `reset_sim()` and set `g_running = nk_true`. During a running simulation, parameter changes take effect immediately (no restart needed).

### 3. Custom chart renderer with labelled axes

Nuklear’s built-in `nk_chart_*` has no axis labels or tick marks. We must:

- Use `nk_layout_space_begin` / `nk_layout_space_push` / `nk_layout_space_end` to reserve a rectangular area.
- Get the canvas via `nk_window_get_canvas(ctx)`.
- Draw:
  - **Background** fill (`nk_fill_rect`).
  - **Grid lines** (light strokes).
  - **Y-axis ticks & labels** — auto-scale using `min_val`/`max_val` from the data, compute 4–6 nice tick intervals, draw short tick marks and numeric labels left of the plot area.
  - **X-axis ticks & labels** — evenly spaced ticks from `x_min` to `x_max`, drawn below the plot area.
  - **Data series** — `nk_stroke_line` (or `nk_fill_rect` for columns) mapped from data-space to pixel coordinates.
- Labels are drawn with `nk_draw_text` using `ctx->style.font`.
- `nk_widget_bounds(ctx)` returns the allocated rectangle.

**Nuklear types / functions used:**
| Function | Purpose |
|---|---|
| `nk_layout_space_begin(ctx, NK_STATIC, height, 1)` | Allocate a fixed-height draw area |
| `nk_layout_space_push(ctx, nk_rect(0,0,w,h))` | Reserve the full box |
| `struct nk_rect nk_widget_bounds(ctx)` | Get the pixel rectangle of the reserved space |
| `struct nk_command_buffer* nk_window_get_canvas(ctx)` | Get the draw command buffer |
| `nk_stroke_line(canvas, x0,y0, x1,y1, thickness, color)` | Draw lines (grid,axes,data) |
| `nk_fill_rect(canvas, rect, rounding, color)` | Fill background |
| `nk_draw_text(canvas, rect, str, len, font, bg, fg)` | Draw tick labels |

The new renderer activates when `x_max > x_min && count > 0`; otherwise the existing `nk_chart_*` fallback is used (for backward compat with the sine-wave plot in main.c).

## Files to modify

| File | Changes |
|---|---|
| `ui_infra.h` | Add `x_min`, `x_max`, `x_label`, `y_label` fields to `ui_plot`. Declare `ui_plot_set_x_range()`. |
| `ui_infra.c` | `ui_plot_init` zeros new fields. Implement `ui_plot_set_x_range()`. Implement custom axis renderer in `ui_plot_render`. |
| `tools/boost_converter/boost_converter.c` | Add `g_time_limit`, param-snapshot tracking for auto-restart, Restart button, time-limit input field. Call `ui_plot_set_x_range` on init and when limit changes. |

## Reuse

- `ui_plot_init` / `ui_plot_push` — keep existing ring-buffer logic unchanged.
- `tool_registry_check_close` / `tool_registry_draw_sidebar` — unchanged.
- `nk_property_float` — already used for all inputs; add one more for time limit.

## Steps

- [ ] 1. Extend `ui_plot` struct and `ui_plot_init` in `ui_infra.h` / `ui_infra.c` with `x_min`, `x_max`, `x_label`, `y_label`, and `ui_plot_set_x_range()`.
- [ ] 2. Implement custom labelled-chart renderer in `ui_plot_render` (activated when `x_max > x_min`). Draw background, grid, Y-axis ticks/labels, X-axis ticks/labels, data lines.
- [ ] 3. In `boost_converter.c`, add `g_time_limit` (5.0 s default) with a `nk_property_float` input. Add **Restart** button. Add parameter-snapshot auto-restart logic.
- [ ] 4. Call `ui_plot_set_x_range(&g_plot, 0, g_time_limit)` at registration and whenever the user changes `g_time_limit`.
- [ ] 5. Clean build and verify no warnings.

## Verification

- Build: `make clean && make` — zero warnings.
- Launch: `./nuklear_app`, open **Boost Converter** from sidebar.
- Default run: Vout grows toward steady-state, plot shows labelled axes (0–5 s on X, auto-scaled volts on Y). After 5 s the simulation pauses.
- Restart: click **Restart** → plot clears and resumes.
- Parameter change while paused: tweak Duty or Vin → simulation auto-restarts with new values.
- Time limit change: set limit to 2 s → X-axis relabels to 0–2, simulation stops at 2 s.
