/*
 * ui_infra.h — Reusable UI infrastructure for Nuklear
 *
 * IMPORTANT: you must #include "nuklear.h" BEFORE this header.
 *
 * Provides:
 *   - Panel system (window descriptor + linked-list registry)
 *   - Form builder  (declarative typed-field form)
 *   - Plot wrapper   (ring-buffer chart with Nuklear)
 */
#ifndef UI_INFRA_H_
#define UI_INFRA_H_

#ifndef NK_NUKLEAR_H_
#  error "Include nuklear.h before ui_infra.h"
#endif

/* ------------------------------------------------------------------ */
/*  PANEL                                                              */
/* ------------------------------------------------------------------ */

typedef struct ui_panel ui_panel;

typedef void (*ui_panel_draw_fn)(struct nk_context *ctx, ui_panel *panel);

struct ui_panel {
    const char        *title;
    struct nk_rect      bounds;
    nk_flags            flags;
    nk_bool             visible;
    ui_panel_draw_fn    draw;
    void               *user_data;
    ui_panel           *next;
};

void  ui_panel_init(ui_panel *p, const char *title, struct nk_rect bounds,
                    nk_flags flags, ui_panel_draw_fn draw, void *user_data);
void  ui_panel_add(ui_panel **head, ui_panel *p);
void  ui_panel_remove(ui_panel **head, ui_panel *p);
void  ui_panels_render(struct nk_context *ctx, ui_panel *head);

/* ------------------------------------------------------------------ */
/*  FORM BUILDER                                                       */
/* ------------------------------------------------------------------ */

typedef enum {
    UI_FIELD_LABEL,
    UI_FIELD_SEPARATOR,
    UI_FIELD_TEXT,
    UI_FIELD_INT,
    UI_FIELD_FLOAT,
    UI_FIELD_CHECKBOX,
    UI_FIELD_SLIDER_INT,
    UI_FIELD_SLIDER_FLOAT,
    UI_FIELD_COMBO,
    UI_FIELD_COLOR,
    UI_FIELD_BUTTON,
    UI_FIELD_CUSTOM,
} ui_field_type;

typedef struct ui_field {
    ui_field_type  type;
    const char    *label;
    void          *value;

    union {
        struct { int max_len; }                     text;
        struct { float min, max, step; }             slider_f;
        struct { int   min, max, step; }             slider_i;
        struct {
            const char **items;
            int          count;
            int         *selected;
        } combo;
        struct {
            void (*cb)(void *data);
            void *data;
        } button;
        struct {
            void (*draw)(struct nk_context *ctx, void *data);
            void *data;
        } custom;
    } params;
} ui_field;

typedef struct ui_form {
    ui_field *fields;
    int       count;
    int       cap;
} ui_form;

void  ui_form_init(ui_form *form);
void  ui_form_free(ui_form *form);

/* Builder helpers — each appends a field */
void  ui_form_add_label    (ui_form *form, const char *text);
void  ui_form_add_separator(ui_form *form);
void  ui_form_add_text     (ui_form *form, const char *label, char *buf, int max_len);
void  ui_form_add_int      (ui_form *form, const char *label, int *val);
void  ui_form_add_float    (ui_form *form, const char *label, float *val);
void  ui_form_add_checkbox (ui_form *form, const char *label, nk_bool *val);
void  ui_form_add_slider_int (ui_form *form, const char *label, int *val,
                              int min, int max, int step);
void  ui_form_add_slider_float(ui_form *form, const char *label, float *val,
                               float min, float max, float step);
void  ui_form_add_combo    (ui_form *form, const char *label,
                            int *selected, const char **items, int count);
void  ui_form_add_color    (ui_form *form, const char *label,
                            struct nk_colorf *color);
void  ui_form_add_button   (ui_form *form, const char *label,
                            void (*cb)(void *data), void *data);
void  ui_form_add_custom   (ui_form *form,
                            void (*draw)(struct nk_context *ctx, void *data),
                            void *data);

/* Render all fields of a form (call inside a nk_begin/nk_end window) */
void  ui_form_render(struct nk_context *ctx, ui_form *form);

/* ------------------------------------------------------------------ */
/*  PLOT                                                               */
/* ------------------------------------------------------------------ */

#define UI_PLOT_MAX_POINTS  1024

typedef struct ui_plot {
    float               buffer[UI_PLOT_MAX_POINTS];
    int                 head;        /* next write index      */
    int                 count;       /* points written so far */
    enum nk_chart_type  type;        /* NK_CHART_LINES / NK_CHART_COLUMN */
    float               min_val;
    float               max_val;
    const char         *title;

    /* Visual overrides */
    struct nk_color     line_color;
    struct nk_color     bg_color;
    nk_bool             use_custom_colors;
} ui_plot;

void  ui_plot_init (ui_plot *plot, const char *title, enum nk_chart_type type);
void  ui_plot_push (ui_plot *plot, float value);
void  ui_plot_render(struct nk_context *ctx, ui_plot *plot);

#endif /* UI_INFRA_H_ */
