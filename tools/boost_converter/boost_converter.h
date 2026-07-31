/*
 * boost_converter.h — Boost Converter tool header
 */
#ifndef BOOST_CONVERTER_H_
#define BOOST_CONVERTER_H_

#include "ui_infra.h"

/* Register the Boost Converter tool with the global panel list and tool registry.
 * Call once during init. */
void boost_converter_register(ui_panel **head, int sidebar_w, int win_w, int win_h);

#endif /* BOOST_CONVERTER_H_ */
