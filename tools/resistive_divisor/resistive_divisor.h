/*
 * resistive_divisor.h — Resistive Divisor tool header
 */
#ifndef RESISTIVE_DIVISOR_H_
#define RESISTIVE_DIVISOR_H_

#include "ui_infra.h"

/* Register the Resistive Divisor tool with the global panel list and tool registry.
 * Call once during init. */
void resistive_divisor_register(ui_panel **head, int sidebar_w, int win_w, int win_h);

#endif /* RESISTIVE_DIVISOR_H_ */
