# Fix right panel size + update agents.md

1. In main.c, change `panel_panel` bounds so the right panel fills all remaining space:
   - pw = WINDOW_WIDTH - SIDEBAR_W - 20
   - ph = WINDOW_HEIGHT - 20
   - x = SIDEBAR_W + 10, y = 10

2. In agents.md §10, update `/create-tool` step 3 so future tools use the same full‑remaining‑space formula instead of the old right‑corner dock.

3. Build with `make`, verify no warnings.
