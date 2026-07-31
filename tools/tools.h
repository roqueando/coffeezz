/*
 * tools.h — Master include for all tool modules.
 *
 * Include this header (and only this) in main.c to get everything.
 * To add a new tool:
 *   1. Create its subdirectory under tools/
 *   2. #include its header here
 *   3. Add its register call in tools.c
 */
#ifndef TOOLS_H_
#define TOOLS_H_

#include "tools/tool_registry.h"
#include "tools/buck_converter/buck_converter.h"

/* Initialise all tools. Must be called once during startup. */
void tools_init(ui_panel **head, int sidebar_w, int win_w, int win_h);

#endif /* TOOLS_H_ */
