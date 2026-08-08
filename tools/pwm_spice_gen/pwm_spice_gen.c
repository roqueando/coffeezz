/*
 * pwm_spice_gen.c — PWM Spice Generator tool
 *
 * All inputs accept SI‑unit suffixes (e.g.  "10n" = 10e-9,  "1k" = 1000).
 * Supported prefixes: f p n u m  k/K M G
 *
 * Two‑column layout:  left  = parameter inputs
 *                      right = live waveform plot + SPICE output
 */
#include "nuklear.h"
#include "ui_infra.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <ctype.h>
#include "tools/tool_registry.h"
#include "tools/pwm_spice_gen/pwm_spice_gen.h"

/* ---- Parameter state (parsed floats) ------------------------------ */
static float    g_vlow     = 0.0f;
static float    g_vhigh    = 5.0f;
static float    g_freq     = 1000.0f;
static float    g_duty     = 50.0f;
static float    g_tr       = 1e-9f;
static float    g_tf       = 1e-9f;
static float    g_td       = 0.0f;
static int      g_ncycles  = 10;

/* ---- Text buffers for SI‑aware text fields ------------------------ */
static char g_buf_vlow[32],  g_buf_vhigh[32];
static char g_buf_freq[32],  g_buf_duty[32];
static char g_buf_tr[32],    g_buf_tf[32],    g_buf_td[32];
static char g_buf_ncycles[32];

/* Computed SPICE string */
static char g_spice_str[256];

/* Waveform plot */
static ui_plot g_plot;

/* Snapshot for change detection → auto‑regenerate waveform */
static float g_snap_vlow, g_snap_vhigh, g_snap_freq, g_snap_duty;
static float g_snap_tr, g_snap_tf, g_snap_td;
static int   g_snap_ncycles;

/* ================================================================== */
/*  SI‑unit helpers                                                    */
/* ================================================================== */

static float si_to_float(const char *s, float fallback)
{
    if (!s || !*s) return fallback;

    char *end = NULL;
    float val = strtof(s, &end);
    if (end == s) return fallback;

    while (end && isspace((unsigned char)*end)) end++;

    if (end && *end) {
        switch (*end) {
        case 'f': case 'F': val *= 1e-15f;  break;
        case 'p': case 'P': val *= 1e-12f;  break;
        case 'n': case 'N': val *= 1e-9f;   break;
        case 'u': case 'U': val *= 1e-6f;   break;
        case 'm':           val *= 1e-3f;   break;
        case 'k': case 'K': val *= 1e3f;    break;
        case 'M':           val *= 1e6f;    break;
        case 'G':           val *= 1e9f;    break;
        default: break;
        }
    }
    return val;
}

static void float_to_si(float v, char *buf, size_t bufsz)
{
    if (v == 0.0f) { snprintf(buf, bufsz, "0"); return; }

    float      av     = fabsf(v);
    const char *pre   = "";
    float       scale = 1.0f;

    if      (av >= 1e9f)  { pre = "G"; scale = 1e-9f;  }
    else if (av >= 1e6f)  { pre = "M"; scale = 1e-6f;  }
    else if (av >= 1e3f)  { pre = "k"; scale = 1e-3f;  }
    else if (av >= 1.0f)  { /* none */                  }
    else if (av >= 1e-3f) { pre = "m"; scale = 1e3f;    }
    else if (av >= 1e-6f) { pre = "u"; scale = 1e6f;    }
    else if (av >= 1e-9f) { pre = "n"; scale = 1e9f;    }
    else if (av >= 1e-12f){ pre = "p"; scale = 1e12f;   }
    else                  { pre = "f"; scale = 1e15f;   }

    snprintf(buf, bufsz, "%.4g%s", v * scale, pre);
}

/* ================================================================== */
/*  SPICE output                                                       */
/* ================================================================== */

static const char *fmt_time_spice(float t, char *buf, size_t bufsz)
{
    if (t <= 0.0f)        snprintf(buf, bufsz, "0");
    else if (t < 1e-6f)   snprintf(buf, bufsz, "%.3gn", t * 1e9f);
    else if (t < 1e-3f)   snprintf(buf, bufsz, "%.3gu", t * 1e6f);
    else if (t < 1.0f)    snprintf(buf, bufsz, "%.3gm", t * 1e3f);
    else                  snprintf(buf, bufsz, "%.6g", t);
    return buf;
}

static void recompute_spice(void)
{
    float period  = 1.0f / g_freq;
    float ton_raw = (g_duty / 100.0f) * period;
    float pw      = ton_raw - g_tr;
    if (pw < 0.0f) pw = 0.0f;

    char tr_buf[32], tf_buf[32], td_buf[32], per_buf[32], pw_buf[32];

    snprintf(g_spice_str, sizeof(g_spice_str),
        "PULSE(%.4g %.4g %s %s %s %s %s %d)",
        g_vlow, g_vhigh,
        fmt_time_spice(g_td,    td_buf,  sizeof(td_buf)),
        fmt_time_spice(g_tr,    tr_buf,  sizeof(tr_buf)),
        fmt_time_spice(g_tf,    tf_buf,  sizeof(tf_buf)),
        fmt_time_spice(pw,      pw_buf,  sizeof(pw_buf)),
        fmt_time_spice(period,  per_buf, sizeof(per_buf)),
        g_ncycles);
}

/* ================================================================== */
/*  Waveform generation                                                */
/* ================================================================== */

/* Evaluate the ideal PWM waveform at time t (seconds). */
static float pwm_sample(float t, float period, float tr, float tf,
                        float pw_flat, float vlow, float vhigh)
{
    if (t < g_td) return vlow;

    float tp = fmodf(t - g_td, period);
    if (tp < tr)
        return vlow + (vhigh - vlow) * (tp / tr);             /* rising edge  */
    if (tp < tr + pw_flat)
        return vhigh;                                          /* flat top     */
    if (tp < tr + pw_flat + tf)
        return vhigh - (vhigh - vlow) * ((tp - tr - pw_flat) / tf);   /* falling edge */
    return vlow;                                               /* low hold     */
}

static void regenerate_waveform(void)
{
    float period  = 1.0f / g_freq;
    float ton_raw = (g_duty / 100.0f) * period;
    float pw_flat = ton_raw - g_tr;
    if (pw_flat < 0.0f) pw_flat = 0.0f;

    float total_t = g_td + (float)g_ncycles * period;
    if (total_t <= 0.0f) total_t = 1e-9f;

    /* reset the plot ring‑buffer */
    g_plot.head  = 0;
    g_plot.count = 0;
    g_plot.min_val = 0.0f;
    g_plot.max_val = 0.0f;

    int N = g_ncycles * 500;
    if (N < 2)  N = 2;
    if (N > UI_PLOT_MAX_POINTS) N = UI_PLOT_MAX_POINTS;

    for (int i = 0; i < N; i++) {
        float t = total_t * (float)i / (float)(N - 1);
        ui_plot_push(&g_plot,
            pwm_sample(t, period, g_tr, g_tf, pw_flat, g_vlow, g_vhigh));
    }

    ui_plot_set_x_range(&g_plot, 0.0f, total_t);
    g_plot.x_label = "Time (s)";
    g_plot.y_label = "Voltage (V)";

    /* Update snapshot so we don't regenerate every frame */
    g_snap_vlow    = g_vlow;
    g_snap_vhigh   = g_vhigh;
    g_snap_freq    = g_freq;
    g_snap_duty    = g_duty;
    g_snap_tr      = g_tr;
    g_snap_tf      = g_tf;
    g_snap_td      = g_td;
    g_snap_ncycles = g_ncycles;
}

static int params_changed(void)
{
    return g_vlow    != g_snap_vlow    || g_vhigh != g_snap_vhigh ||
           g_freq    != g_snap_freq    || g_duty  != g_snap_duty  ||
           g_tr      != g_snap_tr      || g_tf    != g_snap_tf    ||
           g_td      != g_snap_td      || g_ncycles != g_snap_ncycles;
}

/* ================================================================== */
/*  Draw‑callback macros                                               */
/* ================================================================== */

#define SI_FIELD(lbl, buf, field, len) do {                     \
    nk_layout_row_dynamic(ctx, 25, 2);                          \
    nk_label(ctx, lbl, NK_TEXT_LEFT);                           \
    nk_edit_string_zero_terminated(ctx, NK_EDIT_SIMPLE,         \
                                   buf, len, nk_filter_default);\
    (field) = si_to_float(buf, field);                          \
} while(0)

#define INT_FIELD(lbl, buf, field, len) do {                    \
    nk_layout_row_dynamic(ctx, 25, 2);                          \
    nk_label(ctx, lbl, NK_TEXT_LEFT);                           \
    nk_edit_string_zero_terminated(ctx, NK_EDIT_SIMPLE,         \
                                   buf, len, nk_filter_default);\
    (field) = (int)si_to_float(buf, (float)(field));            \
} while(0)

/* ================================================================== */
/*  Draw callback                                                      */
/* ================================================================== */

static void pwm_spice_gen_draw(struct nk_context *ctx, ui_panel *pnl)
{
    tool_registry_check_close(ctx, pnl);

    /* ── Parse all text fields into floats ──────────────────────── */
    g_vlow    = si_to_float(g_buf_vlow,    g_vlow);
    g_vhigh   = si_to_float(g_buf_vhigh,   g_vhigh);
    g_freq    = si_to_float(g_buf_freq,    g_freq);
    g_duty    = si_to_float(g_buf_duty,    g_duty);
    g_tr      = si_to_float(g_buf_tr,      g_tr);
    g_tf      = si_to_float(g_buf_tf,      g_tf);
    g_td      = si_to_float(g_buf_td,      g_td);
    g_ncycles = (int)si_to_float(g_buf_ncycles, (float)g_ncycles);

    /* Clamp */
    if (g_freq  < 0.001f)  g_freq  = 0.001f;
    if (g_duty  < 0.0f)    g_duty  = 0.0f;
    if (g_duty  > 100.0f)  g_duty  = 100.0f;
    if (g_tr    < 0.0f)    g_tr    = 0.0f;
    if (g_tf    < 0.0f)    g_tf    = 0.0f;
    if (g_td    < 0.0f)    g_td    = 0.0f;
    if (g_ncycles < 1)     g_ncycles = 1;

    /* Regenerate waveform on change */
    if (params_changed())
        regenerate_waveform();

    recompute_spice();

    /* ── Parameters (top) ───────────────────────────────────────── */
    nk_layout_row_dynamic(ctx, 22, 1);
    nk_label(ctx, "PWM PARAMETERS  (SI prefixes: f p n u m k M G)", NK_TEXT_CENTERED);

    SI_FIELD( "Vlow:",      g_buf_vlow,    g_vlow,     sizeof(g_buf_vlow)    );
    SI_FIELD( "Vhigh:",     g_buf_vhigh,   g_vhigh,    sizeof(g_buf_vhigh)   );
    SI_FIELD( "Frequency:", g_buf_freq,    g_freq,     sizeof(g_buf_freq)    );
    SI_FIELD( "Duty (%):",  g_buf_duty,    g_duty,     sizeof(g_buf_duty)    );
    SI_FIELD( "Trise:",     g_buf_tr,      g_tr,       sizeof(g_buf_tr)      );
    SI_FIELD( "Tfall:",     g_buf_tf,      g_tf,       sizeof(g_buf_tf)      );
    SI_FIELD( "Tdelay:",    g_buf_td,      g_td,       sizeof(g_buf_td)      );
    INT_FIELD("Ncycles:",   g_buf_ncycles, g_ncycles,  sizeof(g_buf_ncycles) );

    /* ── Spacer ─────────────────────────────────────────────────── */
    nk_layout_row_dynamic(ctx, 10, 1);
    nk_spacing(ctx, 1);

    /* ── Waveform plot (bottom) ─────────────────────────────────── */
    nk_layout_row_dynamic(ctx, 22, 1);
    nk_label(ctx, "PWM WAVEFORM", NK_TEXT_CENTERED);

    nk_layout_row_dynamic(ctx, 200, 1);
    ui_plot_render(ctx, &g_plot);

    nk_layout_row_dynamic(ctx, 20, 1);
    nk_label(ctx, "Time  ──▶", NK_TEXT_CENTERED);

    /* ── Spacer ─────────────────────────────────────────────────── */
    nk_layout_row_dynamic(ctx, 6, 1);
    nk_spacing(ctx, 1);

    /* ── SPICE output ───────────────────────────────────────────── */
    nk_layout_row_dynamic(ctx, 22, 1);
    nk_label(ctx, "SPICE PULSE SOURCE", NK_TEXT_CENTERED);

    nk_layout_row_dynamic(ctx, 32, 1);
    nk_edit_string_zero_terminated(ctx,
        NK_EDIT_SELECTABLE | NK_EDIT_READ_ONLY,
        g_spice_str, (int)sizeof(g_spice_str), nk_filter_default);

    nk_layout_row_dynamic(ctx, 16, 1);
    nk_label(ctx,
        "PULSE(V1 V2 TD TR TF PW PER NP) · V1=Vlow V2=Vhigh",
        NK_TEXT_LEFT);
}

/* ================================================================== */
/*  Registration                                                       */
/* ================================================================== */

void pwm_spice_gen_register(ui_panel **head, int sidebar_w,
                            int win_w, int win_h)
{
    /* Seed text buffers from defaults */
    float_to_si(g_vlow,    g_buf_vlow,    sizeof(g_buf_vlow));
    float_to_si(g_vhigh,   g_buf_vhigh,   sizeof(g_buf_vhigh));
    float_to_si(g_freq,    g_buf_freq,    sizeof(g_buf_freq));
    float_to_si(g_duty,    g_buf_duty,    sizeof(g_buf_duty));
    float_to_si(g_tr,      g_buf_tr,      sizeof(g_buf_tr));
    float_to_si(g_tf,      g_buf_tf,      sizeof(g_buf_tf));
    float_to_si(g_td,      g_buf_td,      sizeof(g_buf_td));
    snprintf(g_buf_ncycles, sizeof(g_buf_ncycles), "%d", g_ncycles);

    /* Initialise the plot */
    ui_plot_init(&g_plot, "PWM Waveform", NK_CHART_LINES);
    g_plot.use_custom_colors = nk_true;
    g_plot.line_color        = nk_rgb(255, 180, 30);  /* orange */

    /* Invalidate snapshot so first draw regenerates */
    g_snap_vlow    = -1e30f;
    g_snap_vhigh   = -1e30f;
    g_snap_freq    = -1e30f;
    g_snap_duty    = -1e30f;
    g_snap_tr      = -1e30f;
    g_snap_tf      = -1e30f;
    g_snap_td      = -1e30f;
    g_snap_ncycles = -1;

    tool_desc desc = {
        .button_label = "PWM Spice GEN",
        .panel_title  = "PWM Spice Generator",
        .draw         = pwm_spice_gen_draw,
        .user_data    = NULL
    };
    tool_register(head, sidebar_w, win_w, win_h, &desc);
}
