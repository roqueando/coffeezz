# Plan: Tests, CI/CD, and Target Rename

## Context

The `nuklear_app` C11+GLFW+Nuklear desktop app currently has:
- No automated tests
- No CI/CD pipeline
- A target name (`nuklear_app`) that should be changed to `coffeez`
- A hardcoded macOS-only Makefile (`/opt/homebrew/opt/glfw`)

We need to add tests for the logic-layer code, set up GitHub Actions for build+test+release, and rename the binary.

## Approach

### 1. Target rename: `nuklear_app` → `coffeez`

Change the `TARGET` variable in the Makefile. Also update `README.md` where it references `./nuklear_app`.

### 2. Testing strategy

Many functions call Nuklear (`nk_begin`, `nk_button_label`, etc.) and require a GL context — these are **untestable** in a headless CI environment. The testable surface is the **data-structure logic**:

| Function | What it tests |
|---|---|
| `ui_panel_init` | Fields are set correctly |
| `ui_panel_add` | Linked-list prepend |
| `ui_panel_remove` | Unlink from list (head, middle, not‑found) |
| `ui_form_init` | Zero-initialised form |
| `ui_form_free` | Frees and zeroes |
| Form builder (`ui_form_add_*`) | Append grows array, sets type/label/value/params |
| `ui_plot_init` | Buffer zeroed, defaults correct |
| `ui_plot_push` | Ring-buffer wrapping, count capping, auto‑scale |
| `ui_plot_set_x_range` | Range stored, swapped when inverted |
| `tool_registry_init` | Slot count zeroed |
| `tool_register` | Panel created with correct bounds, added to list |
| `tool_registry_update` | Panel.visible synced from slot.visible |

Untestable (require Nuklear context or GL): `ui_panels_render`, `ui_form_render`, `ui_plot_render`, `draw_axes_chart`, `tool_registry_draw_sidebar`, `tool_registry_check_close`.

**Test runner**: Use a single-header C test framework. **`acutest.h`** (MIT-licensed, ~1 file) is ideal — no dependencies, works with `make`, outputs TAP/JUnit.

**Structure**: A single `tests/` directory at the repo root containing:
- `tests/acutest.h` — the test framework header
- `tests/test_ui_infra.c` — tests for panel, form, plot
- `tests/test_tool_registry.c` — tests for the tool registry

**Compile tests** by adding a `test` target to the Makefile. Since the tested code needs `#include "nuklear.h"` (for `nk_rect`, `nk_bool`, etc.), we define the Nuklear types without `NK_IMPLEMENTATION` — the test code uses only the header types.

### 3. CI/CD (GitHub Actions)

**Workflow file**: `.github/workflows/ci.yml`

**Jobs**:

1. **build** — `macos-latest` runner
   - Install GLFW: `brew install glfw`
   - `make clean && make`
   - Upload `coffeez` binary as artifact

2. **test** — `macos-latest` runner
   - Install GLFW
   - `make test`
   - (Tests are pure logic — no GL context needed, so they can run headless)

3. **release** — triggered on `v*` tag push
   - Build
   - Create GitHub Release with `coffeez` binary attached
   - Use `softprops/action-gh-release`

### 4. Makefile changes

- Rename `TARGET = coffeez`
- Add test source files and a `test` target:
  ```makefile
  TEST_SRCS = tests/test_ui_infra.c tests/test_tool_registry.c
  TEST_OBJS = $(TEST_SRCS:.c=.o) ui_infra.o tools/tool_registry.o
  test: $(TEST_OBJS)
      $(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)
      ./test
  ```
- Add `test` to `.PHONY`
- `clean` should also remove `test` binary and test `.o` files

### 5. README updates

- Update `./nuklear_app` to `./coffeez`
- Add `make test` to quick start

## Files to modify / create

| File | Action |
|---|---|
| `Makefile` | Rename TARGET, add `test` target and test compilation |
| `README.md` | Update binary name references |
| `tests/acutest.h` | **New** — single-header test framework |
| `tests/test_ui_infra.c` | **New** — tests for panels, forms, plots |
| `tests/test_tool_registry.c` | **New** — tests for tool registry |
| `.github/workflows/ci.yml` | **New** — build, test, release workflow |

## Reuse

- All testable functions are in `ui_infra.c` and `tools/tool_registry.c` — no new code needed, just call them from test files.
- Nuklear types (`nk_rect`, `nk_bool`, `nk_flags`, `nk_color`, `nk_colorf`, `nk_chart_type`) are available from `nuklear.h` without `NK_IMPLEMENTATION`.

## Steps

- [ ] 1. Rename `TARGET = coffeez` in Makefile
- [ ] 2. Update `README.md` references to `nuklear_app` → `coffeez`
- [ ] 3. Download `acutest.h` and place at `tests/acutest.h`
- [ ] 4. Create `tests/test_ui_infra.c` — test panel, form, and plot logic
- [ ] 5. Create `tests/test_tool_registry.c` — test registry logic
- [ ] 6. Add `test` target to Makefile (compile test sources + run)
- [ ] 7. Add `test` to `.PHONY` and `clean` rules
- [ ] 8. Create `.github/workflows/ci.yml` with build, test, and release jobs
- [ ] 9. Run `make clean && make && make test` locally to verify
- [ ] 10. Push and verify CI passes on GitHub

## Verification

```sh
# Local
make clean && make          # builds coffeez
./coffeez                   # launches app (manual check, needs display)
make test                   # runs test suite, should output "All tests passed"

# CI
# Push to GitHub → Actions tab shows green build + test
# Push a v* tag (e.g. v1.0.0) → Release created with coffeez binary attached
```
