/*
 * buck_converter.c — Buck Converter calculator tool.
 *
 * Provides a UI panel for buck converter calculations.
 * Current implementation is a placeholder.
 */
#include "nuklear.h"
#include "ui_infra.h"
#include <stdlib.h>
#include "tools/tool_registry.h"
#include "tools/buck_converter/buck_converter.h"

/* ---- Tool-specific state (static, persistent) ---- */

/* ---- Draw callback ---- */
static void buck_converter_draw(struct nk_context *ctx, ui_panel *pnl)
{
    /* Handle close button (×) */
    tool_registry_check_close(ctx, pnl);

    nk_layout_row_dynamic(ctx, 30, 1);
    nk_label(ctx, "Buck Converter Calculator", NK_TEXT_CENTERED);

    nk_layout_row_dynamic(ctx, 10, 1);
    nk_spacing(ctx, 1);

    nk_layout_row_dynamic(ctx, 25, 1);
    nk_label(ctx, "Placeholder - add components here", NK_TEXT_LEFT);
}

/* ---- Registration ---- */
void buck_converter_register(ui_panel **head, int sidebar_w, int win_w, int win_h)
{
    tool_desc desc = {
        .button_label = "Buck Converter",
        .panel_title  = "Buck Converter",
        .draw         = buck_converter_draw,
        .user_data    = NULL
    };
    tool_register(head, sidebar_w, win_w, win_h, &desc);
}
