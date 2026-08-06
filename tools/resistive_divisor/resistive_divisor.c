/*
 * resistive_divisor.c — Resistive Divisor calculator tool.
 *
 * Given Vin and two resistor values (R1, R2), computes and displays
 * the output voltage:  Vout = Vin * R2 / (R1 + R2)
 */
#include "nuklear.h"
#include "ui_infra.h"
#include <stdio.h>
#include "tools/tool_registry.h"
#include "tools/resistive_divisor/resistive_divisor.h"

/* ---- Tool-specific state (static, persistent) ---- */
static float    g_vin  = 5.0f;
static float    g_r1   = 10000.0f;   /* 10 kΩ */
static float    g_r2   = 10000.0f;   /* 10 kΩ */
static ui_form  g_form;

/* ---- Draw callback ---- */
static void resistive_divisor_draw(struct nk_context *ctx, ui_panel *pnl)
{
    float vout;

    tool_registry_check_close(ctx, pnl);

    /* Render input fields via form */
    ui_form_render(ctx, &g_form);

    /* Result line */
    {
        char buf[64];
        vout = g_vin * g_r2 / (g_r1 + g_r2);

        nk_layout_row_dynamic(ctx, 10, 1);
        nk_spacing(ctx, 1);

        nk_layout_row_template_begin(ctx, 30);
        nk_layout_row_template_push_static(ctx, 120);
        nk_layout_row_template_push_static(ctx, 160);
        nk_layout_row_template_end(ctx);

        nk_label(ctx, "Vout:", NK_TEXT_LEFT);
        snprintf(buf, sizeof(buf), "%.4f V", vout);
        nk_label(ctx, buf, NK_TEXT_LEFT);
    }


}

/* ---- Registration ---- */
void resistive_divisor_register(ui_panel **head, int sidebar_w, int win_w, int win_h)
{
    /* Build the form once */
    ui_form_init(&g_form);
    ui_form_add_float(&g_form, "Vin  (V)", &g_vin);
    ui_form_add_float(&g_form, "R1   (Ω)", &g_r1);
    ui_form_add_float(&g_form, "R2   (Ω)", &g_r2);

    tool_desc desc = {
        .button_label = "Resistive Divisor",
        .panel_title  = "Resistive Divisor",
        .draw         = resistive_divisor_draw,
        .user_data    = NULL,
        .panel_w      = 300,
        .panel_h      = 200
    };
    tool_register(head, sidebar_w, win_w, win_h, &desc);
}
