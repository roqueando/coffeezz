/*
 * ui_infra.c — Implementation of the UI infrastructure.
 */

#define NK_INCLUDE_FIXED_TYPES
#define NK_INCLUDE_STANDARD_IO
#define NK_INCLUDE_STANDARD_VARARGS
#define NK_INCLUDE_DEFAULT_ALLOCATOR
#define NK_INCLUDE_VERTEX_BUFFER_OUTPUT
#define NK_INCLUDE_FONT_BAKING
#define NK_INCLUDE_DEFAULT_FONT

#include "nuklear.h"
#include "ui_infra.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <limits.h>

/* ================================================================== */
/*  PANEL                                                              */
/* ================================================================== */

void ui_panel_init(ui_panel *p, const char *title, struct nk_rect bounds,
                   nk_flags flags, ui_panel_draw_fn draw, void *user_data)
{
    p->title     = title;
    p->bounds    = bounds;
    p->flags     = flags;
    p->visible   = nk_true;
    p->draw      = draw;
    p->user_data = user_data;
    p->next      = NULL;
}

void ui_panel_add(ui_panel **head, ui_panel *p)
{
    p->next = *head;
    *head   = p;
}

void ui_panel_remove(ui_panel **head, ui_panel *p)
{
    ui_panel *prev = NULL, *cur = *head;
    while (cur) {
        if (cur == p) {
            if (prev) prev->next = cur->next;
            else      *head      = cur->next;
            return;
        }
        prev = cur;
        cur  = cur->next;
    }
}

void ui_panels_render(struct nk_context *ctx, ui_panel *head)
{
    for (ui_panel *p = head; p; p = p->next) {
        if (!p->visible) continue;

        if (nk_begin(ctx, p->title, p->bounds, p->flags)) {
            if (p->draw) p->draw(ctx, p);
        }
        nk_end(ctx);
    }
}

/* ================================================================== */
/*  FORM BUILDER                                                       */
/* ================================================================== */

static void form_grow(ui_form *form)
{
    int new_cap = form->cap ? form->cap * 2 : 8;
    form->fields = (ui_field *)realloc(form->fields,
                                       (size_t)new_cap * sizeof(ui_field));
    form->cap = new_cap;
}

static ui_field *form_add(ui_form *form)
{
    if (form->count >= form->cap) form_grow(form);
    ui_field *f = &form->fields[form->count++];
    memset(f, 0, sizeof(*f));
    return f;
}

void ui_form_init(ui_form *form)
{
    form->fields = NULL;
    form->count  = 0;
    form->cap    = 0;
}

void ui_form_free(ui_form *form)
{
    free(form->fields);
    form->fields = NULL;
    form->count = form->cap = 0;
}

#define ADD(t, lbl, val) do { \
    ui_field *f_ = form_add(form); \
    f_->type  = (t);               \
    f_->label = (lbl);             \
    f_->value = (val);             \
} while(0)

void ui_form_add_label(ui_form *form, const char *text)
{
    ADD(UI_FIELD_LABEL, text, NULL);
}

void ui_form_add_separator(ui_form *form)
{
    ADD(UI_FIELD_SEPARATOR, NULL, NULL);
}

void ui_form_add_text(ui_form *form, const char *label, char *buf, int max_len)
{
    ui_field *f = form_add(form);
    f->type  = UI_FIELD_TEXT;
    f->label = label;
    f->value = buf;
    f->params.text.max_len = max_len;
}

void ui_form_add_int(ui_form *form, const char *label, int *val)
{
    ADD(UI_FIELD_INT, label, val);
}

void ui_form_add_float(ui_form *form, const char *label, float *val)
{
    ADD(UI_FIELD_FLOAT, label, val);
}

void ui_form_add_checkbox(ui_form *form, const char *label, nk_bool *val)
{
    ADD(UI_FIELD_CHECKBOX, label, val);
}

void ui_form_add_slider_int(ui_form *form, const char *label, int *val,
                             int min, int max, int step)
{
    ui_field *f = form_add(form);
    f->type  = UI_FIELD_SLIDER_INT;
    f->label = label;
    f->value = val;
    f->params.slider_i.min  = min;
    f->params.slider_i.max  = max;
    f->params.slider_i.step = step;
}

void ui_form_add_slider_float(ui_form *form, const char *label, float *val,
                               float min, float max, float step)
{
    ui_field *f = form_add(form);
    f->type  = UI_FIELD_SLIDER_FLOAT;
    f->label = label;
    f->value = val;
    f->params.slider_f.min  = min;
    f->params.slider_f.max  = max;
    f->params.slider_f.step = step;
}

void ui_form_add_combo(ui_form *form, const char *label,
                        int *selected, const char **items, int count)
{
    ui_field *f = form_add(form);
    f->type  = UI_FIELD_COMBO;
    f->label = label;
    f->value = selected;
    f->params.combo.items    = items;
    f->params.combo.count    = count;
    f->params.combo.selected = selected;
}

void ui_form_add_color(ui_form *form, const char *label,
                        struct nk_colorf *color)
{
    ADD(UI_FIELD_COLOR, label, color);
}

void ui_form_add_button(ui_form *form, const char *label,
                         void (*cb)(void *data), void *data)
{
    ui_field *f = form_add(form);
    f->type  = UI_FIELD_BUTTON;
    f->label = label;
    f->value = NULL;
    f->params.button.cb   = cb;
    f->params.button.data = data;
}

void ui_form_add_custom(ui_form *form,
                         void (*draw)(struct nk_context *ctx, void *data),
                         void *data)
{
    ui_field *f = form_add(form);
    f->type  = UI_FIELD_CUSTOM;
    f->label = NULL;
    f->value = NULL;
    f->params.custom.draw = draw;
    f->params.custom.data = data;
}

/* ---------- render ---------- */

void ui_form_render(struct nk_context *ctx, ui_form *form)
{
    for (int i = 0; i < form->count; i++) {
        ui_field *f = &form->fields[i];

        switch (f->type) {

        case UI_FIELD_LABEL:
            nk_layout_row_dynamic(ctx, 25, 1);
            nk_label(ctx, f->label, NK_TEXT_LEFT);
            break;

        case UI_FIELD_SEPARATOR:
            nk_layout_row_dynamic(ctx, 6, 1);
            nk_spacing(ctx, 1);
            break;

        case UI_FIELD_TEXT: {
            int len = f->params.text.max_len;
            nk_layout_row_static(ctx, 25, 120, 1);
            nk_edit_string_zero_terminated(ctx, NK_EDIT_SIMPLE,
                (char *)f->value, len, nk_filter_default);
            if (f->label && f->label[0]) {
                nk_layout_row_dynamic(ctx, 18, 1);
                nk_label(ctx, f->label, NK_TEXT_LEFT);
            }
            break;
        }

        case UI_FIELD_INT:
            nk_layout_row_dynamic(ctx, 25, 1);
            nk_property_int(ctx, f->label, INT_MIN, (int*)f->value, INT_MAX, 1, 1);
            break;

        case UI_FIELD_FLOAT:
            nk_layout_row_dynamic(ctx, 25, 1);
            nk_property_float(ctx, f->label, -1e6f, (float*)f->value, 1e6f, 0.01f, 0.1f);
            break;

        case UI_FIELD_CHECKBOX:
            nk_layout_row_dynamic(ctx, 25, 1);
            nk_checkbox_label(ctx, f->label, (nk_bool*)f->value);
            break;

        case UI_FIELD_SLIDER_INT:
            nk_layout_row_dynamic(ctx, 25, 1);
            nk_slider_int(ctx, f->params.slider_i.min,
                          (int*)f->value,
                          f->params.slider_i.max,
                          f->params.slider_i.step);
            nk_layout_row_dynamic(ctx, 15, 1);
            nk_labelf(ctx, NK_TEXT_LEFT, "%s: %d", f->label, *(int*)f->value);
            break;

        case UI_FIELD_SLIDER_FLOAT:
            nk_layout_row_dynamic(ctx, 25, 1);
            nk_slider_float(ctx, f->params.slider_f.min,
                            (float*)f->value,
                            f->params.slider_f.max,
                            f->params.slider_f.step);
            nk_layout_row_dynamic(ctx, 15, 1);
            nk_labelf(ctx, NK_TEXT_LEFT, "%s: %.3f", f->label, *(float*)f->value);
            break;

        case UI_FIELD_COMBO: {
            nk_layout_row_dynamic(ctx, 25, 1);
            if (nk_combo_begin_label(ctx, f->label,
                    nk_vec2(nk_widget_width(ctx), 30 * f->params.combo.count)))
            {
                nk_layout_row_dynamic(ctx, 25, 1);
                for (int j = 0; j < f->params.combo.count; j++) {
                    if (nk_combo_item_label(ctx, f->params.combo.items[j],
                            NK_TEXT_LEFT))
                        *(int*)f->value = j;
                }
                nk_combo_end(ctx);
            }
            break;
        }

        case UI_FIELD_COLOR: {
            struct nk_colorf *c = (struct nk_colorf *)f->value;
            float row_height = 200.0f;
            nk_layout_row_dynamic(ctx, 25, 1);
            if (nk_combo_begin_color(ctx, nk_rgb_cf(*c),
                    nk_vec2(nk_widget_width(ctx), row_height)))
            {
                nk_layout_row_dynamic(ctx, 140, 1);
                *c = nk_color_picker(ctx, *c, NK_RGBA);
                nk_layout_row_dynamic(ctx, 25, 1);
                c->r = nk_propertyf(ctx, "#R:", 0, c->r, 1.0f, 0.01f, 0.005f);
                c->g = nk_propertyf(ctx, "#G:", 0, c->g, 1.0f, 0.01f, 0.005f);
                c->b = nk_propertyf(ctx, "#B:", 0, c->b, 1.0f, 0.01f, 0.005f);
                c->a = nk_propertyf(ctx, "#A:", 0, c->a, 1.0f, 0.01f, 0.005f);
                nk_combo_end(ctx);
            }
            if (f->label && f->label[0]) {
                nk_layout_row_dynamic(ctx, 18, 1);
                nk_labelf(ctx, NK_TEXT_LEFT, "%s: #%02x%02x%02x",
                    f->label,
                    (int)(c->r*255), (int)(c->g*255), (int)(c->b*255));
            }
            break;
        }

        case UI_FIELD_BUTTON:
            nk_layout_row_static(ctx, 30, 120, 1);
            if (nk_button_label(ctx, f->label)) {
                if (f->params.button.cb)
                    f->params.button.cb(f->params.button.data);
            }
            break;

        case UI_FIELD_CUSTOM:
            if (f->params.custom.draw)
                f->params.custom.draw(ctx, f->params.custom.data);
            break;

        default:
            break;
        }
    }
}
#undef ADD

/* ================================================================== */
/*  PLOT                                                               */
/* ================================================================== */

void ui_plot_init(ui_plot *plot, const char *title, enum nk_chart_type type)
{
    memset(plot->buffer, 0, sizeof(plot->buffer));
    plot->head   = 0;
    plot->count  = 0;
    plot->type   = type;
    plot->min_val = 0.0f;
    plot->max_val = 1.0f;
    plot->title  = title;
    plot->line_color = nk_rgb(0, 120, 220);
    plot->bg_color   = nk_rgb(30, 30, 40);
    plot->use_custom_colors = nk_false;
}

void ui_plot_push(ui_plot *plot, float value)
{
    plot->buffer[plot->head] = value;
    plot->head = (plot->head + 1) % UI_PLOT_MAX_POINTS;
    if (plot->count < UI_PLOT_MAX_POINTS) plot->count++;

    /* auto-scale */
    if (plot->count == 1) {
        plot->min_val = value - 1.0f;
        plot->max_val = value + 1.0f;
    } else {
        if (value < plot->min_val) plot->min_val = value;
        if (value > plot->max_val) plot->max_val = value;
    }
}

void ui_plot_render(struct nk_context *ctx, ui_plot *plot)
{
    if (plot->count == 0) {
        nk_label(ctx, "(no data)", NK_TEXT_CENTERED);
        return;
    }

    float margin = (plot->max_val - plot->min_val) * 0.05f;
    float lo = plot->min_val - margin;
    float hi = plot->max_val + margin;

    enum nk_chart_type ctype = plot->type;

    if (plot->use_custom_colors) {
        if (nk_chart_begin_colored(ctx, ctype,
                plot->line_color, plot->line_color,
                plot->count, lo, hi))
        {
            /* Draw from oldest to newest (head is next write position) */
            int oldest = (plot->head >= plot->count)
                         ? 0 : plot->head;
            for (int i = 0; i < plot->count; i++) {
                int idx = (oldest + i) % UI_PLOT_MAX_POINTS;
                nk_chart_push(ctx, plot->buffer[idx]);
            }
            nk_chart_end(ctx);
        }
    } else {
        if (nk_chart_begin(ctx, ctype, plot->count, lo, hi)) {
            int oldest = (plot->head >= plot->count)
                         ? 0 : plot->head;
            for (int i = 0; i < plot->count; i++) {
                int idx = (oldest + i) % UI_PLOT_MAX_POINTS;
                nk_chart_push(ctx, plot->buffer[idx]);
            }
            nk_chart_end(ctx);
        }
    }
}
