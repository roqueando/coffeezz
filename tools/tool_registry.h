/*
 * tool_registry.h — Generic tool registry for sidebar buttons and panel management
 *
 * The registry decouples main.c from individual tools.
 * Each tool calls tool_register() with a descriptor; the registry handles
 * sidebar rendering, visibility toggling, and panel lifecycle.
 */
#ifndef TOOL_REGISTRY_H_
#define TOOL_REGISTRY_H_

#include "ui_infra.h"

/* Maximum number of registered tools. Increase as needed. */
#define TOOL_REGISTRY_MAX  32

/* ---- Tool descriptor ---- */
typedef struct tool_desc {
    const char         *button_label;   /* label in the sidebar button  */
    const char         *panel_title;    /* title bar of the tool panel  */
    ui_panel_draw_fn    draw;           /* called every frame inside nk_begin/nk_end */
    void               *user_data;      /* passed to draw callback       */
    int                 panel_w;        /* 0 → fill; >0 → fixed width    */
    int                 panel_h;        /* 0 → fill; >0 → fixed height   */
} tool_desc;

/* ---- Registry API ---- */

/* Initialise the registry (must be called once before any tool_register). */
void  tool_registry_init(void);

/* Register a new tool. Returns the new ui_panel* (owned by registry).
 * The panel is added to the global panel list via the head pointer. */
ui_panel * tool_register(ui_panel **head, int sidebar_w, int win_w, int win_h,
                         const tool_desc *desc);

/* Draw all tool toggle buttons inside the sidebar panel.
 * Call this from the sidebar's draw callback. */
void  tool_registry_draw_sidebar(struct nk_context *ctx);

/* Call each frame (in the main loop) to sync visibility of tool panels
 * from the registry's toggle state. */
void  tool_registry_update(void);

/* Call this at the start of each tool panel's draw callback to auto-hide
 * when the user clicks the close (x) button. */
void  tool_registry_check_close(struct nk_context *ctx, ui_panel *pnl);

#endif /* TOOL_REGISTRY_H_ */
