# Plan: Makefile tool support + separate tools into `tools/`

## Context

Currently the Makefile hardcodes `SRCS = main.c ui_infra.c`. There is a `tools/` folder with a stub `buck_converter/`, but the buck converter UI code lives directly in `main.c`. The goal is to:

1. Update the Makefile to auto-discover and compile all `.c` files under `tools/`.
2. Extract tool code out of `main.c` into their own directories under `tools/`.
3. Establish a registration pattern so `main.c` stays generic and new tools are just dropped in.

## Approach

### Makefile
- Use `wildcard` to auto-discover tool source files: `tools/*.c` (infrastructure at tools root) + `tools/*/*.c` (individual tool subdirectories).
- Compile tool `.c` files into `.o` files alongside the main objects.
- Add `-I.` (root) to CFLAGS so tools can include `nuklear.h`, `ui_infra.h`, etc. with `#include "tools/tool_registry.h"` style paths.
- Tools must **not** define `NK_IMPLEMENTATION` or `NK_GLFW_GL3_IMPLEMENTATION` — only `main.c` does.

### Tool API
Create a minimal **tool registry** (`tools/tool_registry.h` + `.c`) that:
- Each tool registers itself with a descriptor (button label, panel title, draw callback).
- The registry manages visibility toggles for each tool.
- Provides `tool_registry_draw_sidebar(ctx)` — renders all tool buttons in the sidebar.
- Provides `tool_registry_update()` — called each frame to sync panel visibility.
- Provides `tool_registry_render_panels(ctx)` — renders all tool panels (or they integrate with the existing panel list).

Actually — the existing `ui_panel` system already handles rendering via `ui_panels_render(ctx, g_panels)`. So the registry just needs to:
1. Store tool info (label, title, draw fn).
2. Create a `ui_panel` per tool and add it to `g_panels`.
3. Expose a sidebar draw function that renders buttons.

### Registration pattern
Each tool directory provides a `<tool>_register(ui_panel **panel_list)` function that creates its panel, adds it to the list, and registers with the tool registry for the sidebar button. These are called from `tools/tools_init()`.

A single `tools/tools.h` header includes all tool headers and `tools/tools.c` calls each tool's register function. Adding a tool means:
1. Create `tools/<name>/` with `<name>.h` + `<name>.c`
2. Add `#include "<name>/<name>.h"` to `tools/tools.h`
3. Add `call` to `tools/tools.c`

**Alternative (auto-discovery):** The Makefile could auto-generate the init list, but that adds build complexity. The explicit `tools/tools.h` approach is simpler and transparent.

### Buck converter migration
Move the buck converter UI code from `main.c` into `tools/buck_converter/buck_converter.h` + `.c`. The tool registers its panel with the right-side full-space formula (`pw = WINDOW_WIDTH - SIDEBAR_W - 20`, etc.).

### `main.c` changes
- Replace hardcoded buck converter code with calls to tool registry.
- Sidebar's `draw_sidebar` calls `tool_registry_draw_sidebar(ctx)` instead of hardcoded buttons.
- Main loop calls `tool_registry_update()` to sync visibility.

## Files to modify / create

| File | Action |
|------|--------|
| `Makefile` | Add wildcard tool discovery; add `-I.` to CFLAGS |
| `tools/tool_registry.h` | **New** — tool descriptor type + registry API |
| `tools/tool_registry.c` | **New** — registry implementation |
| `tools/tools.h` | **New** — master include that collects all tool headers |
| `tools/tools.c` | **New** — `tools_init()` calling each tool's register fn |
| `tools/buck_converter/buck_converter.h` | **Rewrite** — declare `buck_converter_register()` |
| `tools/buck_converter/buck_converter.c` | **Rewrite** — implement panel + draw callback |
| `main.c` | Replace hardcoded buck converter with registry calls |
| `agents.md` | Update `/create-tool` to reflect new tool registration pattern |

## Steps

- [ ] 1. Update `Makefile`: auto-discover `tools/*/*.c`, compile to `.o`, add `-I.` flag
- [ ] 2. Create `tools/tool_registry.h` — type `tool_desc`, registry API
- [ ] 3. Create `tools/tool_registry.c` — panel/visibility management, sidebar rendering
- [ ] 4. Create `tools/tools.h` — master include
- [ ] 5. Create `tools/tools.c` — `tools_init()` calling each tool register
- [ ] 6. Rewrite `tools/buck_converter/buck_converter.h` — declare register function
- [ ] 7. Rewrite `tools/buck_converter/buck_converter.c` — panel + draw callback
- [ ] 8. Update `main.c` — use registry instead of hardcoded buck converter
- [ ] 9. Build with `make clean && make`, verify no warnings
- [ ] 10. Run `./nuklear_app`, verify Buck Converter button works, panel opens/closes
- [ ] 11. Update `agents.md` `/create-tool` section for new pattern

## Verification

```sh
make clean && make && ./nuklear_app
```

- Sidebar shows "Buck Converter" button (dynamically from registry).
- Clicking it opens a right-side panel.
- Close button (×) hides the panel.
- No compiler warnings.
