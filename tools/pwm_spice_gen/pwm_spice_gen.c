/*
 * pwm_spice_gen.c — PWM Spice Generator tool
 *
 * Given PWM parameters (Vlow, Vhigh, frequency, duty cycle, rise/fall times,
 * delay, number of cycles), computes and displays the equivalent
 * SPICE PULSE source string:
 *
 *   PULSE(V1 V2 TD TR TF PW PER NP)
 */
#include "nuklear.h"
#include "ui_infra.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "tools/tool_registry.h"
#include "tools/pwm_spice_gen/pwm_spice_gen.h"

/* ---- Tool state --------------------------------------------------- */
static float    g_vlow     = 0.0f;       /* low-level voltage (V)        */
static float    g_vhigh    = 5.0f;       /* high-level voltage (V)       */
static float    g_freq     = 1000.0f;    /* frequency (Hz)               */
static float    g_duty     = 50.0f;      /* duty cycle (%)               */
static float    g_tr       = 1e-9f;      /* rise time (s)                */
static float    g_tf       = 1e-9f;      /* fall time (s)                */
static float    g_td       = 0.0f;       /* delay (s)                    */
static int      g_ncycles  = 10;         /* number of cycles             */

/* Computed SPICE string (updated every frame) */
static char     g_spice_str[256];

/* ---- Engineering formatter for time values ------------------------ */
static const char *fmt_time(float t, char *buf, size_t bufsz)
{
    if (t <= 0.0f) {
        snprintf(buf, bufsz, "0");
    } else if (t < 1e-6f) {
        snprintf(buf, bufsz, "%.3gn", t * 1e9f);
    } else if (t < 1e-3f) {
        snprintf(buf, bufsz, "%.3gu", t * 1e6f);
    } else if (t < 1.0f) {
        snprintf(buf, bufsz, "%.3gm", t * 1e3f);
    } else {
        snprintf(buf, bufsz, "%.6g", t);
    }
    return buf;
}

/* ---- Recompute the SPICE string ----------------------------------- */
static void recompute_spice(void)
{
    float period  = 1.0f / g_freq;
    float ton_raw = (g_duty / 100.0f) * period;
    float pw      = ton_raw - g_tr;            /* flat-top width          */
    if (pw < 0.0f) pw = 0.0f;

    char tr_buf[32], tf_buf[32], td_buf[32], per_buf[32], pw_buf[32];

    snprintf(g_spice_str, sizeof(g_spice_str),
        "PULSE(%.4g %.4g %s %s %s %s %s %d)",
        g_vlow, g_vhigh,
        fmt_time(g_td,  td_buf,  sizeof(td_buf)),
        fmt_time(g_tr,  tr_buf,  sizeof(tr_buf)),
        fmt_time(g_tf,  tf_buf,  sizeof(tf_buf)),
        fmt_time(pw,    pw_buf,  sizeof(pw_buf)),
        fmt_time(period, per_buf, sizeof(per_buf)),
        g_ncycles);
}

/* ------------------------------------------------------------------ */
/*  Draw callback                                                      */
/* ------------------------------------------------------------------ */

static void pwm_spice_gen_draw(struct nk_context *ctx, ui_panel *pnl)
{
    tool_registry_check_close(ctx, pnl);

    float panel_w = nk_widget_width(ctx);
    (void)panel_w;

    /* ── Input section ──────────────────────────────────────────── */
    nk_layout_row_dynamic(ctx, 22, 1);
    nk_label(ctx, "PWM PARAMETERS", NK_TEXT_CENTERED);

    /* Vlow */
    nk_layout_row_dynamic(ctx, 25, 2);
    nk_label(ctx, "Vlow (V):", NK_TEXT_LEFT);
    nk_property_float(ctx, "#Vlow", -50.0f, &g_vlow, 50.0f, 0.1f, 0.1f);

    /* Vhigh */
    nk_layout_row_dynamic(ctx, 25, 2);
    nk_label(ctx, "Vhigh (V):", NK_TEXT_LEFT);
    nk_property_float(ctx, "#Vhigh", -50.0f, &g_vhigh, 50.0f, 0.1f, 0.1f);

    /* Frequency */
    nk_layout_row_dynamic(ctx, 25, 2);
    nk_label(ctx, "Frequency:", NK_TEXT_LEFT);
    float f_khz = g_freq * 1e-3f;
    nk_property_float(ctx, "#freq(kHz)", 0.001f, &f_khz, 10000.0f, 0.1f, 0.1f);
    g_freq = f_khz * 1e3f;
    if (g_freq < 0.001f) g_freq = 0.001f;

    /* Duty cycle */
    nk_layout_row_dynamic(ctx, 25, 2);
    nk_label(ctx, "Duty (%):", NK_TEXT_LEFT);
    nk_property_float(ctx, "#Duty", 0.01f, &g_duty, 99.99f, 0.1f, 1.0f);

    /* Rise time */
    nk_layout_row_dynamic(ctx, 25, 2);
    nk_label(ctx, "Trise (s):", NK_TEXT_LEFT);
    nk_property_float(ctx, "#Tr", 0.0f, &g_tr, 1.0f, 0.1e-9f, 1e-9f);

    /* Fall time */
    nk_layout_row_dynamic(ctx, 25, 2);
    nk_label(ctx, "Tfall (s):", NK_TEXT_LEFT);
    nk_property_float(ctx, "#Tf", 0.0f, &g_tf, 1.0f, 0.1e-9f, 1e-9f);

    /* Delay */
    nk_layout_row_dynamic(ctx, 25, 2);
    nk_label(ctx, "Tdelay (s):", NK_TEXT_LEFT);
    nk_property_float(ctx, "#Td", 0.0f, &g_td, 1.0f, 0.1e-9f, 1e-9f);

    /* Ncycles */
    nk_layout_row_dynamic(ctx, 25, 2);
    nk_label(ctx, "Ncycles:", NK_TEXT_LEFT);
    nk_property_int(ctx, "#Ncycles", 1, &g_ncycles, 100000, 1, 1);

    /* ── Spacer ─────────────────────────────────────────────────── */
    nk_layout_row_dynamic(ctx, 10, 1);
    nk_spacing(ctx, 1);

    /* ── SPICE output ───────────────────────────────────────────── */
    recompute_spice();

    nk_layout_row_dynamic(ctx, 22, 1);
    nk_label(ctx, "SPICE PULSE SOURCE", NK_TEXT_CENTERED);

    /* Display as a selectable text field */
    nk_layout_row_dynamic(ctx, 35, 1);
    nk_edit_string_zero_terminated(ctx, NK_EDIT_SELECTABLE | NK_EDIT_READ_ONLY,
        g_spice_str, (int)sizeof(g_spice_str), nk_filter_default);

    /* Quick-reference legend */
    nk_layout_row_dynamic(ctx, 18, 1);
    nk_label(ctx, "PULSE(V1 V2 TD TR TF PW PER NP)   V1=Vlow V2=Vhigh TD=delay TR=rise TF=fall PW=Ton-TR PER=1/f NP=cycles", NK_TEXT_LEFT);
}

/* ------------------------------------------------------------------ */
/*  Registration                                                       */
/* ------------------------------------------------------------------ */

void pwm_spice_gen_register(ui_panel **head, int sidebar_w,
                            int win_w, int win_h)
{
    /* Compute initial SPICE string */
    recompute_spice();

    tool_desc desc = {
        .button_label = "PWM Spice GEN",
        .panel_title  = "PWM Spice Generator",
        .draw         = pwm_spice_gen_draw,
        .user_data    = NULL
    };
    tool_register(head, sidebar_w, win_w, win_h, &desc);
}
