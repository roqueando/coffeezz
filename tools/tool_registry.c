/*
 * tool_registry.c — Implementation of the tool registry.
 *
 * Each tool gets a slot containing:
 *   - the panel pointer (added to the global panel list)
 *   - a visibility boolean managed by sidebar button toggles
 *   - a copy of the descriptor for sidebar rendering
 */
#include "nuklear.h"
#include "ui_infra.h"
#include "tools/tool_registry.h"

#include <string.h>
#include <stdlib.h>

/* ---- internal slot ------------------------------------------------- */
typedef struct {
    tool_desc   desc;
    ui_panel   *panel;
    nk_bool     visible;   /* toggle state */
} tool_slot;

static tool_slot  g_slots[TOOL_REGISTRY_MAX];
static int        g_slot_count = 0;
static int        g_sidebar_w  = 0;
static int        g_win_w      = 0;
static int        g_win_h      = 0;

/* ================================================================== */
/*  Public API                                                         */
/* ================================================================== */

void tool_registry_init(void)
{
    g_slot_count = 0;
    memset(g_slots, 0, sizeof(g_slots));
}

ui_panel * tool_register(ui_panel **head, int sidebar_w, int win_w, int win_h,
                         const tool_desc *desc)
{
    if (g_slot_count >= TOOL_REGISTRY_MAX)
        return NULL;

    /* stash geometry for panel bounds calculation */
    g_sidebar_w = sidebar_w;
    g_win_w     = win_w;
    g_win_h     = win_h;

    /* allocate a static panel (registry owns the memory) */
    tool_slot *slot = &g_slots[g_slot_count];

    slot->desc    = *desc;
    slot->visible = nk_false;

    /* create the panel */
    slot->panel = (ui_panel *)calloc(1, sizeof(ui_panel));
    if (!slot->panel)
        return NULL;

    /* panel fills all space to the right of the sidebar */
    float pw = (float)(win_w - sidebar_w - 20);
    float ph = (float)(win_h - 20);
    ui_panel_init(slot->panel, desc->panel_title,
                  nk_rect((float)(sidebar_w + 10), 10, pw, ph),
                  NK_WINDOW_BORDER | NK_WINDOW_TITLE |
                  NK_WINDOW_MOVABLE | NK_WINDOW_SCALABLE |
                  NK_WINDOW_CLOSABLE,
                  desc->draw, desc->user_data);
    slot->panel->visible = nk_false;   /* start hidden */

    /* add to the global panel list */
    ui_panel_add(head, slot->panel);

    g_slot_count++;
    return slot->panel;
}

void tool_registry_draw_sidebar(struct nk_context *ctx)
{
    for (int i = 0; i < g_slot_count; i++) {
        tool_slot *slot = &g_slots[i];

        nk_layout_row_dynamic(ctx, 35, 1);
        if (nk_button_label(ctx, slot->desc.button_label)) {
            slot->visible = nk_true;
        }
    }
}

void tool_registry_update(void)
{
    for (int i = 0; i < g_slot_count; i++) {
        tool_slot *slot = &g_slots[i];

        /* If the user clicked the close button (×) on the panel title bar,
         * we detect it via a helper.  Nuklear doesn't auto-hide, so we
         * rely on a flag that the draw callback sets.  Instead we use the
         * panel's visible flag as the source of truth and sync in both
         * directions. */
        slot->panel->visible = slot->visible;

        /* Reset toggle if panel was closed via NK_WINDOW_CLOSABLE */
        /* The draw callback must call nk_window_is_closed and set slot->visible = nk_false.
         * To keep the API simple, we store a pointer to our visible bool in user_data so
         * the panel draw callback can do: *(nk_bool*)pnl->user_data = nk_false;
         * Actually, let's store a pointer in a per-slot approach.  We'll do it via
         * panel->user_data so tools can be generic.  But panel->user_data is already the
         * tool's own user_data.  We'll instead embed a flag in the panel pointer index.

         * Simpler: we just use slot->visible = slot->panel->visible, and we set
         * slot->panel->visible = slot->visible at the top of this function.
         * For close-button detection, we'll have the panel draw callback check
         * nk_window_is_closed and set slot->visible = nk_false via a helper.
         * Since the draw callback receives the ui_panel*, we can store a pointer
         * to the slot in panel->user_data (but user_data may be used by tools).

         * Best approach: expose a `tool_registry_set_visible(pnl, val)` that tools call,
         * or simply store the slot index.  Let's store a nk_bool* in the panel's
         * user_data by convention.  The tool's real user data will be placed in
         * a separate field.  We'll add a `tool_data` field... actually that complicates.

         * For now we just expose `tool_registry_close_cb(struct nk_context *ctx, ui_panel *pnl)`
         * and the tool calls it in its draw fn.
         */
    }
}

/* ---- Helper for tools to call in their draw callback ---- */
void tool_registry_check_close(struct nk_context *ctx, ui_panel *pnl)
{
    (void)ctx;
    (void)pnl;
    /* Find the slot for this panel and toggle visibility off if closed. */
    for (int i = 0; i < g_slot_count; i++) {
        if (g_slots[i].panel == pnl && nk_window_is_closed(ctx, pnl->title)) {
            g_slots[i].visible = nk_false;
            return;
        }
    }
}
