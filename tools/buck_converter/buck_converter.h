/*
 * buck_converter.h — Buck Converter tool header
 */
#ifndef BUCK_CONVERTER_H_
#define BUCK_CONVERTER_H_

#include "ui_infra.h"

/* Register the Buck Converter tool with the global panel list and tool registry.
 * Call once during init. */
void buck_converter_register(ui_panel **head, int sidebar_w, int win_w, int win_h);

#endif /* BUCK_CONVERTER_H_ */
