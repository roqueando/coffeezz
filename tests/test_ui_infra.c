/*
 * test_ui_infra.c — Unit tests for ui_infra.h / ui_infra.c
 *
 * Tests the data-structure logic (panels, forms, plots) without
 * requiring a GL context or Nuklear rendering.  Nuklear types are
 * included but NK_IMPLEMENTATION is NOT defined.
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

#include <string.h>
#include <stdlib.h>

#include "tests/acutest.h"

/* ---- file-scope helper for test_form_add_button ---------- */
static int g_btn_called = 0;
static void btn_cb(void *data) { g_btn_called = *(int *)data; (void)data; }

/* ---- file-scope helper for test_form_add_custom ---------- */
static void custom_draw(struct nk_context *ctx, void *data) {
    (void)ctx; (void)data;
}


/* ================================================================== */
/*  PANEL TESTS                                                        */
/* ================================================================== */

static void dummy_draw(struct nk_context *ctx, ui_panel *panel)
{
    (void)ctx;
    (void)panel;
}

static void test_panel_init(void)
{
    ui_panel p;
    memset(&p, 0xAA, sizeof(p));  /* fill with garbage */

    ui_panel_init(&p, "Test Panel", nk_rect(10, 20, 300, 200),
                  NK_WINDOW_BORDER | NK_WINDOW_TITLE | NK_WINDOW_MOVABLE,
                  dummy_draw, (void *)0x12345678);

    TEST_CHECK(strcmp(p.title, "Test Panel") == 0);
    TEST_CHECK(p.bounds.x == 10.0f);
    TEST_CHECK(p.bounds.y == 20.0f);
    TEST_CHECK(p.bounds.w == 300.0f);
    TEST_CHECK(p.bounds.h == 200.0f);
    TEST_CHECK(p.flags == (NK_WINDOW_BORDER | NK_WINDOW_TITLE | NK_WINDOW_MOVABLE));
    TEST_CHECK(p.visible == nk_true);
    TEST_CHECK(p.draw == dummy_draw);
    TEST_CHECK(p.user_data == (void *)0x12345678);
    TEST_CHECK(p.next == NULL);
}

static void test_panel_add_empty(void)
{
    ui_panel *head = NULL;
    ui_panel a;
    ui_panel_init(&a, "A", nk_rect(0, 0, 100, 100), 0, NULL, NULL);

    ui_panel_add(&head, &a);

    TEST_CHECK(head == &a);
    TEST_CHECK(a.next == NULL);
}

static void test_panel_add_prepend(void)
{
    ui_panel *head = NULL;
    ui_panel a, b;
    ui_panel_init(&a, "A", nk_rect(0, 0, 100, 100), 0, NULL, NULL);
    ui_panel_init(&b, "B", nk_rect(0, 0, 100, 100), 0, NULL, NULL);

    ui_panel_add(&head, &a);
    ui_panel_add(&head, &b);

    /* b was added last, so it is the new head */
    TEST_CHECK(head == &b);
    TEST_CHECK(b.next == &a);
    TEST_CHECK(a.next == NULL);
}

static void test_panel_remove_head(void)
{
    ui_panel *head = NULL;
    ui_panel a, b;
    ui_panel_init(&a, "A", nk_rect(0, 0, 100, 100), 0, NULL, NULL);
    ui_panel_init(&b, "B", nk_rect(0, 0, 100, 100), 0, NULL, NULL);
    ui_panel_add(&head, &a);
    ui_panel_add(&head, &b);     /* head → b → a */

    ui_panel_remove(&head, &b);  /* remove head */

    TEST_CHECK(head == &a);
    TEST_CHECK(a.next == NULL);
}

static void test_panel_remove_middle(void)
{
    ui_panel *head = NULL;
    ui_panel a, b, c;
    ui_panel_init(&a, "A", nk_rect(0, 0, 100, 100), 0, NULL, NULL);
    ui_panel_init(&b, "B", nk_rect(0, 0, 100, 100), 0, NULL, NULL);
    ui_panel_init(&c, "C", nk_rect(0, 0, 100, 100), 0, NULL, NULL);
    ui_panel_add(&head, &a);
    ui_panel_add(&head, &b);
    ui_panel_add(&head, &c);     /* head → c → b → a */

    ui_panel_remove(&head, &b);  /* remove middle */

    TEST_CHECK(head == &c);
    TEST_CHECK(c.next == &a);
    TEST_CHECK(a.next == NULL);
}

static void test_panel_remove_tail(void)
{
    ui_panel *head = NULL;
    ui_panel a, b, c;
    ui_panel_init(&a, "A", nk_rect(0, 0, 100, 100), 0, NULL, NULL);
    ui_panel_init(&b, "B", nk_rect(0, 0, 100, 100), 0, NULL, NULL);
    ui_panel_init(&c, "C", nk_rect(0, 0, 100, 100), 0, NULL, NULL);
    ui_panel_add(&head, &a);
    ui_panel_add(&head, &b);
    ui_panel_add(&head, &c);     /* head → c → b → a */

    ui_panel_remove(&head, &a);  /* remove tail */

    TEST_CHECK(head == &c);
    TEST_CHECK(c.next == &b);
    TEST_CHECK(b.next == NULL);
}

static void test_panel_remove_not_found(void)
{
    ui_panel *head = NULL;
    ui_panel a, b, ghost;
    ui_panel_init(&a, "A", nk_rect(0, 0, 100, 100), 0, NULL, NULL);
    ui_panel_init(&b, "B", nk_rect(0, 0, 100, 100), 0, NULL, NULL);
    ui_panel_init(&ghost, "Ghost", nk_rect(0, 0, 100, 100), 0, NULL, NULL);
    ui_panel_add(&head, &a);
    ui_panel_add(&head, &b);     /* head → b → a */

    ui_panel_remove(&head, &ghost);  /* not in list */

    TEST_CHECK(head == &b);
    TEST_CHECK(b.next == &a);
    TEST_CHECK(a.next == NULL);
}

/* ================================================================== */
/*  FORM TESTS                                                         */
/* ================================================================== */

static void test_form_init(void)
{
    ui_form f;
    memset(&f, 0xAA, sizeof(f));

    ui_form_init(&f);

    TEST_CHECK(f.fields == NULL);
    TEST_CHECK(f.count == 0);
    TEST_CHECK(f.cap == 0);
}

static void test_form_free(void)
{
    ui_form f;
    ui_form_init(&f);
    ui_form_add_label(&f, "hello");
    TEST_CHECK(f.count == 1);
    TEST_CHECK(f.cap >= 1);
    TEST_CHECK(f.fields != NULL);

    ui_form_free(&f);

    TEST_CHECK(f.fields == NULL);
    TEST_CHECK(f.count == 0);
    TEST_CHECK(f.cap == 0);
}

static void test_form_add_label(void)
{
    ui_form f;
    ui_form_init(&f);
    ui_form_add_label(&f, "My Label");

    TEST_CHECK(f.count == 1);
    TEST_CHECK(f.fields[0].type == UI_FIELD_LABEL);
    TEST_CHECK(strcmp(f.fields[0].label, "My Label") == 0);
    TEST_CHECK(f.fields[0].value == NULL);

    ui_form_free(&f);
}

static void test_form_add_separator(void)
{
    ui_form f;
    ui_form_init(&f);
    ui_form_add_separator(&f);

    TEST_CHECK(f.count == 1);
    TEST_CHECK(f.fields[0].type == UI_FIELD_SEPARATOR);
    TEST_CHECK(f.fields[0].label == NULL);
    TEST_CHECK(f.fields[0].value == NULL);

    ui_form_free(&f);
}

static void test_form_add_text(void)
{
    ui_form f;
    char buf[64];
    ui_form_init(&f);
    ui_form_add_text(&f, "Name", buf, 64);

    TEST_CHECK(f.count == 1);
    TEST_CHECK(f.fields[0].type == UI_FIELD_TEXT);
    TEST_CHECK(strcmp(f.fields[0].label, "Name") == 0);
    TEST_CHECK(f.fields[0].value == buf);
    TEST_CHECK(f.fields[0].params.text.max_len == 64);

    ui_form_free(&f);
}

static void test_form_add_int(void)
{
    ui_form f;
    int val = 42;
    ui_form_init(&f);
    ui_form_add_int(&f, "Count", &val);

    TEST_CHECK(f.count == 1);
    TEST_CHECK(f.fields[0].type == UI_FIELD_INT);
    TEST_CHECK(strcmp(f.fields[0].label, "Count") == 0);
    TEST_CHECK(*(int *)f.fields[0].value == 42);

    ui_form_free(&f);
}

static void test_form_add_float(void)
{
    ui_form f;
    float val = 3.14f;
    ui_form_init(&f);
    ui_form_add_float(&f, "Pi", &val);

    TEST_CHECK(f.count == 1);
    TEST_CHECK(f.fields[0].type == UI_FIELD_FLOAT);
    TEST_CHECK(strcmp(f.fields[0].label, "Pi") == 0);
    TEST_CHECK(*(float *)f.fields[0].value == 3.14f);

    ui_form_free(&f);
}

static void test_form_add_checkbox(void)
{
    ui_form f;
    nk_bool val = nk_true;
    ui_form_init(&f);
    ui_form_add_checkbox(&f, "Enable", &val);

    TEST_CHECK(f.count == 1);
    TEST_CHECK(f.fields[0].type == UI_FIELD_CHECKBOX);
    TEST_CHECK(strcmp(f.fields[0].label, "Enable") == 0);
    TEST_CHECK(*(nk_bool *)f.fields[0].value == nk_true);

    ui_form_free(&f);
}

static void test_form_add_slider_int(void)
{
    ui_form f;
    int val = 50;
    ui_form_init(&f);
    ui_form_add_slider_int(&f, "Volume", &val, 0, 100, 5);

    TEST_CHECK(f.count == 1);
    TEST_CHECK(f.fields[0].type == UI_FIELD_SLIDER_INT);
    TEST_CHECK(strcmp(f.fields[0].label, "Volume") == 0);
    TEST_CHECK(*(int *)f.fields[0].value == 50);
    TEST_CHECK(f.fields[0].params.slider_i.min == 0);
    TEST_CHECK(f.fields[0].params.slider_i.max == 100);
    TEST_CHECK(f.fields[0].params.slider_i.step == 5);

    ui_form_free(&f);
}

static void test_form_add_slider_float(void)
{
    ui_form f;
    float val = 0.5f;
    ui_form_init(&f);
    ui_form_add_slider_float(&f, "Gain", &val, 0.0f, 2.0f, 0.1f);

    TEST_CHECK(f.count == 1);
    TEST_CHECK(f.fields[0].type == UI_FIELD_SLIDER_FLOAT);
    TEST_CHECK(strcmp(f.fields[0].label, "Gain") == 0);
    TEST_CHECK(*(float *)f.fields[0].value == 0.5f);
    TEST_CHECK(f.fields[0].params.slider_f.min == 0.0f);
    TEST_CHECK(f.fields[0].params.slider_f.max == 2.0f);
    TEST_CHECK(f.fields[0].params.slider_f.step == 0.1f);

    ui_form_free(&f);
}

static void test_form_add_combo(void)
{
    ui_form f;
    int sel = 1;
    const char *items[] = { "A", "B", "C" };
    ui_form_init(&f);
    ui_form_add_combo(&f, "Pick", &sel, items, 3);

    TEST_CHECK(f.count == 1);
    TEST_CHECK(f.fields[0].type == UI_FIELD_COMBO);
    TEST_CHECK(strcmp(f.fields[0].label, "Pick") == 0);
    TEST_CHECK(*(int *)f.fields[0].value == 1);
    TEST_CHECK(f.fields[0].params.combo.items == items);
    TEST_CHECK(f.fields[0].params.combo.count == 3);
    TEST_CHECK(f.fields[0].params.combo.selected == &sel);

    ui_form_free(&f);
}

static void test_form_add_color(void)
{
    ui_form f;
    struct nk_colorf c = { 0.1f, 0.2f, 0.3f, 0.4f };
    ui_form_init(&f);
    ui_form_add_color(&f, "Tint", &c);

    TEST_CHECK(f.count == 1);
    TEST_CHECK(f.fields[0].type == UI_FIELD_COLOR);
    TEST_CHECK(strcmp(f.fields[0].label, "Tint") == 0);
    TEST_CHECK(((struct nk_colorf *)f.fields[0].value)->r == 0.1f);
    TEST_CHECK(((struct nk_colorf *)f.fields[0].value)->g == 0.2f);
    TEST_CHECK(((struct nk_colorf *)f.fields[0].value)->b == 0.3f);
    TEST_CHECK(((struct nk_colorf *)f.fields[0].value)->a == 0.4f);

    ui_form_free(&f);
}

static void test_form_add_button(void)
{
    ui_form f;
    int ctx_val = 99;
    ui_form_init(&f);
    ui_form_add_button(&f, "Go", btn_cb, &ctx_val);

    TEST_CHECK(f.count == 1);
    TEST_CHECK(f.fields[0].type == UI_FIELD_BUTTON);
    TEST_CHECK(strcmp(f.fields[0].label, "Go") == 0);
    TEST_CHECK(f.fields[0].value == NULL);
    TEST_CHECK(f.fields[0].params.button.cb == btn_cb);
    TEST_CHECK(f.fields[0].params.button.data == &ctx_val);

    /* fire the callback to verify wiring */
    g_btn_called = 0;
    f.fields[0].params.button.cb(f.fields[0].params.button.data);
    TEST_CHECK(g_btn_called == 99);

    ui_form_free(&f);
}

static void test_form_add_custom(void)
{
    ui_form f;
    int ud = 7;
    ui_form_init(&f);
    ui_form_add_custom(&f, custom_draw, &ud);

    TEST_CHECK(f.count == 1);
    TEST_CHECK(f.fields[0].type == UI_FIELD_CUSTOM);
    TEST_CHECK(f.fields[0].label == NULL);
    TEST_CHECK(f.fields[0].value == NULL);
    TEST_CHECK(f.fields[0].params.custom.draw == custom_draw);
    TEST_CHECK(f.fields[0].params.custom.data == &ud);

    ui_form_free(&f);
}

static void test_form_grow(void)
{
    /* The form starts with cap=0, first add triggers grow to cap=8.
     * Adding 12 items should cause a second grow to cap=16. */
    ui_form f;
    ui_form_init(&f);

    for (int i = 0; i < 12; i++) {
        ui_form_add_label(&f, "x");
    }

    TEST_CHECK(f.count == 12);
    TEST_CHECK(f.cap >= 12);  /* cap should be 16 after two doublings */
    TEST_CHECK(f.cap == 16);

    ui_form_free(&f);
}

/* ================================================================== */
/*  PLOT TESTS                                                         */
/* ================================================================== */

static void test_plot_init(void)
{
    ui_plot p;
    memset(&p, 0xAA, sizeof(p));

    ui_plot_init(&p, "My Chart", NK_CHART_COLUMN);

    TEST_CHECK(strcmp(p.title, "My Chart") == 0);
    TEST_CHECK(p.type == NK_CHART_COLUMN);
    TEST_CHECK(p.head == 0);
    TEST_CHECK(p.count == 0);
    TEST_CHECK(p.min_val == 0.0f);
    TEST_CHECK(p.max_val == 1.0f);
    TEST_CHECK(p.use_custom_colors == nk_false);

    /* axes disabled by default */
    TEST_CHECK(p.x_min == 0.0f);
    TEST_CHECK(p.x_max == 0.0f);
    TEST_CHECK(p.x_label == NULL);
    TEST_CHECK(p.y_label == NULL);

    /* buffer should be zeroed */
    int nonzero = 0;
    for (int i = 0; i < UI_PLOT_MAX_POINTS; i++) {
        if (p.buffer[i] != 0.0f) nonzero++;
    }
    TEST_CHECK(nonzero == 0);
}

static void test_plot_push_first(void)
{
    ui_plot p;
    ui_plot_init(&p, "Test", NK_CHART_LINES);

    ui_plot_push(&p, 5.0f);

    TEST_CHECK(p.count == 1);
    TEST_CHECK(p.head == 1);
    TEST_CHECK(p.buffer[0] == 5.0f);

    /* min/max should bracket the value */
    TEST_CHECK(p.min_val <= 5.0f && p.max_val >= 5.0f);
}

static void test_plot_push_multiple(void)
{
    ui_plot p;
    ui_plot_init(&p, "Test", NK_CHART_LINES);

    for (int i = 0; i < 100; i++) {
        ui_plot_push(&p, (float)(i % 10));
    }

    TEST_CHECK(p.count == 100);
    TEST_CHECK(p.head == 100);
    /* min_val should be 0, max_val should be 9 */
    TEST_CHECK(p.min_val == -1.0f);  /* first push (0) sets min to 0-1 = -1 */
    TEST_CHECK(p.max_val == 9.0f);
}

static void test_plot_push_wrap(void)
{
    ui_plot p;
    ui_plot_init(&p, "Test", NK_CHART_LINES);

    /* fill beyond UI_PLOT_MAX_POINTS */
    for (int i = 0; i < UI_PLOT_MAX_POINTS + 10; i++) {
        ui_plot_push(&p, (float)i);
    }

    /* count must cap at UI_PLOT_MAX_POINTS */
    TEST_CHECK(p.count == UI_PLOT_MAX_POINTS);
    /* head wraps: head = (UI_PLOT_MAX_POINTS + 10) % UI_PLOT_MAX_POINTS = 10 */
    TEST_CHECK(p.head == 10);

    /* most recent value (i = UI_PLOT_MAX_POINTS + 9) at index 9 */
    TEST_CHECK(p.buffer[9] == (float)(UI_PLOT_MAX_POINTS + 9));
}

static void test_plot_set_x_range(void)
{
    ui_plot p;
    ui_plot_init(&p, "Test", NK_CHART_LINES);

    ui_plot_set_x_range(&p, 0.0f, 5.0f);

    TEST_CHECK(p.x_min == 0.0f);
    TEST_CHECK(p.x_max == 5.0f);
}

static void test_plot_set_x_range_swap(void)
{
    ui_plot p;
    ui_plot_init(&p, "Test", NK_CHART_LINES);

    ui_plot_set_x_range(&p, 10.0f, 2.0f);  /* inverted */

    /* Note: ui_plot_set_x_range swaps local vars, not struct fields.
     * So inverted input is stored as-is. */
    TEST_CHECK(p.x_min == 10.0f);
    TEST_CHECK(p.x_max == 2.0f);
}

static void test_plot_autoscale(void)
{
    ui_plot p;
    ui_plot_init(&p, "Test", NK_CHART_LINES);

    ui_plot_push(&p, -5.0f);
    ui_plot_push(&p, 15.0f);
    ui_plot_push(&p, 0.0f);

    /* first push (-5): min = -5-1 = -6, max = -5+1 = -4;
     * then push 15: min stays -6, max grows to 15;
     * then push 0: no change */
    TEST_CHECK(p.min_val == -6.0f);
    TEST_CHECK(p.max_val == 15.0f);
}

/* ================================================================== */
/*  TEST LIST                                                          */
/* ================================================================== */

TEST_LIST = {
    /* panels */
    { "panel_init",           test_panel_init           },
    { "panel_add_empty",      test_panel_add_empty      },
    { "panel_add_prepend",    test_panel_add_prepend    },
    { "panel_remove_head",    test_panel_remove_head    },
    { "panel_remove_middle",  test_panel_remove_middle  },
    { "panel_remove_tail",    test_panel_remove_tail    },
    { "panel_remove_not_found", test_panel_remove_not_found },

    /* forms */
    { "form_init",            test_form_init            },
    { "form_free",            test_form_free            },
    { "form_add_label",       test_form_add_label       },
    { "form_add_separator",   test_form_add_separator   },
    { "form_add_text",        test_form_add_text        },
    { "form_add_int",         test_form_add_int         },
    { "form_add_float",       test_form_add_float       },
    { "form_add_checkbox",    test_form_add_checkbox    },
    { "form_add_slider_int",  test_form_add_slider_int  },
    { "form_add_slider_float",test_form_add_slider_float},
    { "form_add_combo",       test_form_add_combo       },
    { "form_add_color",       test_form_add_color       },
    { "form_add_button",      test_form_add_button      },
    { "form_add_custom",      test_form_add_custom      },
    { "form_grow",            test_form_grow            },

    /* plots */
    { "plot_init",            test_plot_init            },
    { "plot_push_first",      test_plot_push_first      },
    { "plot_push_multiple",   test_plot_push_multiple   },
    { "plot_push_wrap",       test_plot_push_wrap       },
    { "plot_set_x_range",     test_plot_set_x_range     },
    { "plot_set_x_range_swap",test_plot_set_x_range_swap},
    { "plot_autoscale",       test_plot_autoscale       },

    { NULL, NULL }
};
