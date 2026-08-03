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
#include <string.h>
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
static nk_bool  g_running = nk_false;  /* pause / resume                 */

/* configurable time limit */
static float    g_time_limit = 5.0f;

/* plot for Vout(t) */
static ui_plot  g_plot;

/* snapshot of last frame's parameters (for auto-restart detection) */
static float    g_snap_vin  = 0.0f;
static float    g_snap_duty = 0.0f;
static float    g_snap_L    = 0.0f;
static float    g_snap_C    = 0.0f;
static float    g_snap_R    = 0.0f;
static float    g_snap_freq = 0.0f;

/* plot sample accumulator (survives across frames; reset on restart) */
static float    g_plot_accum = 0.0f;

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

    g_plot_accum = 0.0f;

    /* capture current parameter snapshot */
    g_snap_vin  = g_vin;
    g_snap_duty = g_duty;
    g_snap_L    = g_L;
    g_snap_C    = g_C;
    g_snap_R    = g_R;
    g_snap_freq = g_freq;
}

/* Advance simulation by one "chunk" (≈ 0.2 ms real time per frame).
 * Uses sub-cycle stepping with the two boost phases. */
static void step_simulation(void)
{
    /* Honour pause flag and time limit */
    if (!g_running) return;
    if (g_sim_t >= g_time_limit) {
        g_running = nk_false;
        return;
    }

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

        /* stop early if we hit the time limit during sub-stepping */
        if (g_sim_t >= g_time_limit) {
            g_running = nk_false;
            break;
        }
    }

    /* push sample into plot every ~1 ms of sim time */
    g_plot_accum += chunk_dt;
    while (g_plot_accum >= 1e-3f) {
        ui_plot_push(&g_plot, g_vout);
        g_plot_accum -= 1e-3f;
    }
}

/* ------------------------------------------------------------------ */
/*  Draw callback                                                      */
/* ------------------------------------------------------------------ */

static void boost_converter_draw(struct nk_context *ctx, ui_panel *pnl)
{
    tool_registry_check_close(ctx, pnl);

    /* ---------------------------------------------------------------- */
    /* Auto-restart: if paused and any parameter changed, reset & run   */
    /* ---------------------------------------------------------------- */
    if (!g_running) {
        if (g_vin  != g_snap_vin  || g_duty != g_snap_duty ||
            g_L    != g_snap_L    || g_C    != g_snap_C    ||
            g_R    != g_snap_R    || g_freq != g_snap_freq) {
            reset_sim();
            g_running = nk_true;
        }
    }

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

        /* Time limit (s) */
        float prev_limit = g_time_limit;
        nk_layout_row_dynamic(ctx, 25, 2);
        nk_label(ctx, "Time limit (s):", NK_TEXT_LEFT);
        nk_property_float(ctx, "#tlim", 0.5f, &g_time_limit, 60.0f, 0.1f, 1.0f);

        /* Update plot X range if limit changed */
        if (g_time_limit != prev_limit) {
            ui_plot_set_x_range(&g_plot, 0, g_time_limit);
        }

        /* control buttons */
        nk_layout_row_dynamic(ctx, 8, 1);
        nk_spacing(ctx, 1);
        nk_layout_row_dynamic(ctx, 30, 4);
        if (nk_button_label(ctx, "Restart")) {
            reset_sim();
            g_running = nk_true;
        }
        if (nk_button_label(ctx, g_running ? "⏸ Pause" : "▶ Run")) {
            if (!g_running && g_sim_t >= g_time_limit) {
                reset_sim();
            }
            g_running = !g_running;
        }
        nk_spacing(ctx, 1);
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
    g_running = nk_false;  /* start stopped — user clicks Restart/Run */

    /* initialise the plot */
    ui_plot_init(&g_plot, "Vout(t)", NK_CHART_LINES);
    g_plot.use_custom_colors = nk_true;
    g_plot.line_color        = nk_rgb(50, 200, 100);   /* green       */

    /* configure X axis range and labels */
    ui_plot_set_x_range(&g_plot, 0.0f, g_time_limit);
    g_plot.x_label = "Time (s)";
    g_plot.y_label = "Vout (V)";

    /* capture initial parameter snapshot */
    g_snap_vin  = g_vin;
    g_snap_duty = g_duty;
    g_snap_L    = g_L;
    g_snap_C    = g_C;
    g_snap_R    = g_R;
    g_snap_freq = g_freq;

    tool_desc desc = {
        .button_label = "Boost Converter",
        .panel_title  = "Boost Converter",
        .draw         = boost_converter_draw,
        .user_data    = NULL
    };
    tool_register(head, sidebar_w, win_w, win_h, &desc);
}
