/*
 * tools.c — Central initialisation of all tool modules.
 *
 * Adding a new tool:
 *   1. #include its header
 *   2. Call its register function in tools_init()
 */
#include "nuklear.h"
#include "tools/tools.h"

void tools_init(ui_panel **head, int sidebar_w, int win_w, int win_h)
{
    /* Initialise the registry first (idempotent). */
    tool_registry_init();

    /* ----- Register each tool below ----- */
    buck_converter_register(head, sidebar_w, win_w, win_h);
    boost_converter_register(head, sidebar_w, win_w, win_h);
}
