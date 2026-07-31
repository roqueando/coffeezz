/*
 * boost_converter.c — Ideal Boost Converter calculator & simulator.
 *
 * Models an ideal boost converter with:
 *   - Parameter inputs (Vin, D, L, C, R_load, f_sw)
 *   - Ideal steady-state formulas displayed live
 *   - Time-domain simulation (two-phase: switch-on / switch-off)
 *   - Vout(t) plotted on a live chart (time on x-axis)
 */
#include "nuklear.h"
#include "ui_infra.h"
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include "tools/tool_registry.h"
#include "tools/boost_converter/boost_converter.h"

/* ---- Tool state --------------------------------------------------- */
static float    g_vin   = 12.0f;      /* input voltage (V)              */
static float    g_duty  = 0.5f;       /* duty cycle D                   */
static float    g_L     = 100e-6f;    /* inductance (H)                 */
static float    g_C     = 100e-6f;    /* capacitance (F)                */
static float    g_R     = 10.0f;      /* load resistance (Ω)            */
static float    g_freq  = 100e3f;     /* switching frequency (Hz)       */

/* simulation live state */
static float    g_vout  = 12.0f;      /* instantaneous Vout             */
static float    g_iL    = 0.0f;       /* instantaneous inductor current */
static float    g_sim_t = 0.0f;       /* elapsed simulation time (s)    */
static nk_bool  g_running = nk_true;  /* pause / resume                 */

/* plot for Vout(t) */
static ui_plot  g_plot;

/* ------------------------------------------------------------------ */
/*  Helpers                                                            */
/* ------------------------------------------------------------------ */

static void reset_sim(void)
{
    g_iL    = 0.0f;
    g_vout  = g_vin;                   /* start at Vin, not 0           */
    g_sim_t = 0.0f;
    /* reset the plot ring buffer */
    g_plot.head  = 0;
    g_plot.count = 0;
    g_plot.min_val = 0.0f;
    g_plot.max_val = 0.0f;
}

/* Advance simulation by one "chunk" (≈ 0.2 ms real time per frame).
 * Uses sub-cycle stepping with the two boost phases. */
static void step_simulation(void)
{
    if (!g_running) return;

    float chunk_dt  = 0.2e-3f;        /* 0.2 ms advance per frame      */
    float sub_dt    = 0.01e-6f;       /* 10 ns sub-step                */
    int   sub_steps = (int)(chunk_dt / sub_dt);
    if (sub_steps < 1)   sub_steps = 1;
    if (sub_steps > 20000) sub_steps = 20000;

    float dt   = chunk_dt / (float)sub_steps;
    float Tsw  = 1.0f / g_freq;       /* switching period               */
    float ton  = g_duty * Tsw;        /* on-time                        */

    float Vin  = g_vin;
    float L    = g_L;
    float C    = g_C;
    float R    = g_R;

    for (int i = 0; i < sub_steps; i++) {
        g_sim_t += dt;

        /* phase within switching cycle */
        float t_phase = fmodf(g_sim_t, Tsw);
        int   on      = (t_phase < ton) ? 1 : 0;

        float vout = g_vout;
        if (vout < Vin) vout = Vin;   /* diode prevents Vout < Vin     */

        float diL_dt, dvout_dt;

        if (on) {
            /* Switch ON: inductor charges from Vin, diode off.
             *   diL/dt  = Vin / L
             *   dvout/dt = -vout / (R*C)   (cap discharges into load) */
            diL_dt    = Vin / L;
            dvout_dt  = -vout / (R * C);
        } else {
            /* Switch OFF: inductor discharges into output (diode on).
             *   diL/dt  = (Vin - vout) / L
             *   dvout/dt = iL / C  -  vout / (R*C)                   */
            diL_dt    = (Vin - vout) / L;
            dvout_dt  = g_iL / C  -  vout / (R * C);
        }

        g_iL   += diL_dt   * dt;
        g_vout += dvout_dt * dt;

        /* clamp for sanity */
        if (g_iL   < 0.0f)    g_iL   = 0.0f;
        if (g_iL   > 100.0f)  g_iL   = 100.0f;
        if (g_vout < Vin)     g_vout = Vin;
        if (g_vout > 2000.0f) g_vout = 2000.0f;
    }

    /* push sample into plot every ~1 ms of sim time */
    static float plot_accum = 0.0f;
    plot_accum += chunk_dt;
    while (plot_accum >= 1e-3f) {
        ui_plot_push(&g_plot, g_vout);
        plot_accum -= 1e-3f;
    }
}

/* ------------------------------------------------------------------ */
/*  Draw callback                                                      */
/* ------------------------------------------------------------------ */

static void boost_converter_draw(struct nk_context *ctx, ui_panel *pnl)
{
    tool_registry_check_close(ctx, pnl);

    /* advance simulation */
    step_simulation();

    float panel_h = pnl->bounds.h;
    float ratios[] = { 0.32f, 0.68f };
    nk_layout_row(ctx, NK_DYNAMIC, panel_h - 40, 2, ratios);

    /* ============================================================== */
    /*  LEFT GROUP — Parameters + Steady-state + Live readings         */
    /* ============================================================== */
    if (nk_group_begin(ctx, "BC_Params",
                       NK_WINDOW_BORDER | NK_WINDOW_TITLE)) {

        nk_layout_row_dynamic(ctx, 22, 1);
        nk_label(ctx, "INPUT PARAMETERS", NK_TEXT_CENTERED);

        /* Vin */
        nk_layout_row_dynamic(ctx, 25, 2);
        nk_label(ctx, "Vin (V):", NK_TEXT_LEFT);
        nk_property_float(ctx, "#Vin", 0.1f, &g_vin, 500.0f, 0.1f, 0.1f);

        /* Duty cycle */
        nk_layout_row_dynamic(ctx, 25, 2);
        nk_label(ctx, "Duty D:", NK_TEXT_LEFT);
        nk_property_float(ctx, "#D", 0.05f, &g_duty, 0.95f, 0.01f, 0.01f);

        /* L (display as µH) */
        nk_layout_row_dynamic(ctx, 25, 2);
        nk_label(ctx, "L (µH):", NK_TEXT_LEFT);
        float L_uH = g_L * 1e6f;
        nk_property_float(ctx, "#L", 0.1f, &L_uH, 100000.0f, 1.0f, 1.0f);
        g_L = L_uH * 1e-6f;

        /* C (display as µF) */
        nk_layout_row_dynamic(ctx, 25, 2);
        nk_label(ctx, "C (µF):", NK_TEXT_LEFT);
        float C_uF = g_C * 1e6f;
        nk_property_float(ctx, "#C", 0.1f, &C_uF, 100000.0f, 1.0f, 1.0f);
        g_C = C_uF * 1e-6f;

        /* R load */
        nk_layout_row_dynamic(ctx, 25, 2);
        nk_label(ctx, "R load (Ω):", NK_TEXT_LEFT);
        nk_property_float(ctx, "#R", 0.1f, &g_R, 1000.0f, 0.1f, 1.0f);

        /* f (display as kHz) */
        nk_layout_row_dynamic(ctx, 25, 2);
        nk_label(ctx, "f_sw (kHz):", NK_TEXT_LEFT);
        float f_kHz = g_freq * 1e-3f;
        nk_property_float(ctx, "#f", 1.0f, &f_kHz, 1000.0f, 1.0f, 1.0f);
        g_freq = f_kHz * 1e3f;

        /* control buttons */
        nk_layout_row_dynamic(ctx, 8, 1);
        nk_spacing(ctx, 1);
        nk_layout_row_dynamic(ctx, 30, 3);
        if (nk_button_label(ctx, "Reset"))  reset_sim();
        if (nk_button_label(ctx, g_running ? "⏸ Pause" : "▶ Run"))
            g_running = !g_running;
        nk_spacing(ctx, 1);

        /* ========================================================== */
        /*  STEADY-STATE (ideal formulas)                              */
        /* ========================================================== */
        float vout_ideal = g_vin / (1.0f - g_duty);
        float iout_ideal = vout_ideal / g_R;
        float il_avg     = iout_ideal / (1.0f - g_duty);
        float delta_iL   = (g_vin * g_duty) / (g_freq * g_L);
        float delta_vout = (vout_ideal * g_duty) / (g_freq * g_R * g_C);

        char buf[64];

        nk_layout_row_dynamic(ctx, 8, 1);
        nk_spacing(ctx, 1);
        nk_layout_row_dynamic(ctx, 20, 1);
        nk_label(ctx, "STEADY-STATE (ideal)", NK_TEXT_CENTERED);
        nk_layout_row_dynamic(ctx, 22, 2);
        nk_label(ctx, "Vout:", NK_TEXT_LEFT);
        snprintf(buf, sizeof(buf), "%.2f V", vout_ideal);
        nk_label(ctx, buf, NK_TEXT_RIGHT);
        nk_layout_row_dynamic(ctx, 22, 2);
        nk_label(ctx, "Iout:", NK_TEXT_LEFT);
        snprintf(buf, sizeof(buf), "%.2f A", iout_ideal);
        nk_label(ctx, buf, NK_TEXT_RIGHT);
        nk_layout_row_dynamic(ctx, 22, 2);
        nk_label(ctx, "IL(avg):", NK_TEXT_LEFT);
        snprintf(buf, sizeof(buf), "%.2f A", il_avg);
        nk_label(ctx, buf, NK_TEXT_RIGHT);
        nk_layout_row_dynamic(ctx, 22, 2);
        nk_label(ctx, "ΔIL(p-p):", NK_TEXT_LEFT);
        snprintf(buf, sizeof(buf), "%.3f A", delta_iL);
        nk_label(ctx, buf, NK_TEXT_RIGHT);
        nk_layout_row_dynamic(ctx, 22, 2);
        nk_label(ctx, "ΔVout(p-p):", NK_TEXT_LEFT);
        snprintf(buf, sizeof(buf), "%.3f V", delta_vout);
        nk_label(ctx, buf, NK_TEXT_RIGHT);

        /* ========================================================== */
        /*  LIVE SIMULATION state                                      */
        /* ========================================================== */
        nk_layout_row_dynamic(ctx, 8, 1);
        nk_spacing(ctx, 1);
        nk_layout_row_dynamic(ctx, 20, 1);
        nk_label(ctx, "SIMULATION (live)", NK_TEXT_CENTERED);
        nk_layout_row_dynamic(ctx, 22, 2);
        nk_label(ctx, "Vout(t):", NK_TEXT_LEFT);
        snprintf(buf, sizeof(buf), "%.2f V", g_vout);
        nk_label(ctx, buf, NK_TEXT_RIGHT);
        nk_layout_row_dynamic(ctx, 22, 2);
        nk_label(ctx, "IL(t):", NK_TEXT_LEFT);
        snprintf(buf, sizeof(buf), "%.2f A", g_iL);
        nk_label(ctx, buf, NK_TEXT_RIGHT);
        nk_layout_row_dynamic(ctx, 22, 2);
        nk_label(ctx, "t:", NK_TEXT_LEFT);
        snprintf(buf, sizeof(buf), "%.2f ms", g_sim_t * 1000.0f);
        nk_label(ctx, buf, NK_TEXT_RIGHT);

        nk_group_end(ctx);
    }

    /* ============================================================== */
    /*  RIGHT GROUP — Live Vout(t) plot                                */
    /* ============================================================== */
    if (nk_group_begin(ctx, "BC_Chart",
                       NK_WINDOW_BORDER | NK_WINDOW_TITLE)) {
        nk_layout_row_dynamic(ctx, (int)(panel_h - 90), 1);
        ui_plot_render(ctx, &g_plot);
        nk_layout_row_dynamic(ctx, 22, 1);
        nk_label(ctx, "Time  ──▶   |   Vout(t) [V]  —  Ideal Boost Converter",
                 NK_TEXT_CENTERED);
        nk_group_end(ctx);
    }
}

/* ------------------------------------------------------------------ */
/*  Registration                                                       */
/* ------------------------------------------------------------------ */

void boost_converter_register(ui_panel **head, int sidebar_w,
                              int win_w, int win_h)
{
    /* set initial sim state */
    g_vout    = g_vin;
    g_iL      = 0.0f;
    g_sim_t   = 0.0f;
    g_running = nk_true;

    /* initialise the plot */
    ui_plot_init(&g_plot, "Vout(t)", NK_CHART_LINES);
    g_plot.use_custom_colors = nk_true;
    g_plot.line_color        = nk_rgb(50, 200, 100);   /* green       */

    tool_desc desc = {
        .button_label = "Boost Converter",
        .panel_title  = "Boost Converter",
        .draw         = boost_converter_draw,
        .user_data    = NULL
    };
    tool_register(head, sidebar_w, win_w, win_h, &desc);
}
