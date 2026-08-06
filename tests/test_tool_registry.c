/*
 * test_tool_registry.c — Unit tests for tools/tool_registry.h/.c
 *
 * Tests the registry's data management (init, register, update)
 * without requiring a GL context or Nuklear rendering.
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
#include "tools/tool_registry.h"

#include <string.h>
#include <stdlib.h>

#include "tests/acutest.h"

/* ------------------------------------------------------------------ */
/*  Dummy draw callback for tool registration                          */
/* ------------------------------------------------------------------ */
static void dummy_tool_draw(struct nk_context *ctx, ui_panel *panel)
{
    (void)ctx;
    (void)panel;
}

/* ================================================================== */
/*  TESTS                                                              */
/* ================================================================== */

static void test_registry_init(void)
{
    /* Reset registry to known state */
    tool_registry_init();

    /* Try registering a tool — should succeed (slots empty) */
    ui_panel *head = NULL;
    tool_desc desc = {
        .button_label = "Test Tool",
        .panel_title  = "Test Tool Panel",
        .draw         = dummy_tool_draw,
        .user_data    = NULL,
        .panel_w      = 0,
        .panel_h      = 0
    };

    ui_panel *pnl = tool_register(&head, 200, 1100, 700, &desc);
    TEST_CHECK(pnl != NULL);
    TEST_CHECK(pnl->visible == nk_false);  /* starts hidden */
    TEST_CHECK(strcmp(pnl->title, "Test Tool Panel") == 0);

    /* Clean up */
    free(pnl);
}

static void test_registry_register_default_bounds(void)
{
    tool_registry_init();

    ui_panel *head = NULL;
    tool_desc desc = {
        .button_label = "A",
        .panel_title  = "A Panel",
        .draw         = dummy_tool_draw,
        .user_data    = NULL,
        .panel_w      = 0,   /* 0 → fill remaining space */
        .panel_h      = 0
    };

    ui_panel *pnl = tool_register(&head, 200, 1100, 700, &desc);
    TEST_CHECK(pnl != NULL);

    /* Default panel position: sidebar_w + 10, 10 */
    TEST_CHECK(pnl->bounds.x == 210.0f);  /* 200 + 10 */
    TEST_CHECK(pnl->bounds.y == 10.0f);

    /* Default panel size: win_w - sidebar_w - 20, win_h - 20 */
    TEST_CHECK(pnl->bounds.w == 880.0f);  /* 1100 - 200 - 20 */
    TEST_CHECK(pnl->bounds.h == 680.0f);  /* 700 - 20 */

    /* Verify it's in the global panel list */
    TEST_CHECK(head == pnl);

    free(pnl);
}

static void test_registry_register_explicit_bounds(void)
{
    tool_registry_init();

    ui_panel *head = NULL;
    tool_desc desc = {
        .button_label = "Fixed",
        .panel_title  = "Fixed Panel",
        .draw         = dummy_tool_draw,
        .user_data    = NULL,
        .panel_w      = 600,
        .panel_h      = 400
    };

    ui_panel *pnl = tool_register(&head, 200, 1100, 700, &desc);
    TEST_CHECK(pnl != NULL);

    TEST_CHECK(pnl->bounds.w == 600.0f);
    TEST_CHECK(pnl->bounds.h == 400.0f);

    free(pnl);
}

static void test_registry_register_multiple(void)
{
    tool_registry_init();

    ui_panel *head = NULL;

    tool_desc d1 = { "B1", "P1", dummy_tool_draw, NULL, 0, 0 };
    tool_desc d2 = { "B2", "P2", dummy_tool_draw, NULL, 0, 0 };
    tool_desc d3 = { "B3", "P3", dummy_tool_draw, NULL, 0, 0 };

    ui_panel *a = tool_register(&head, 200, 1000, 600, &d1);
    ui_panel *b = tool_register(&head, 200, 1000, 600, &d2);
    ui_panel *c = tool_register(&head, 200, 1000, 600, &d3);

    TEST_CHECK(a != NULL);
    TEST_CHECK(b != NULL);
    TEST_CHECK(c != NULL);
    TEST_CHECK(a != b);
    TEST_CHECK(b != c);

    /* All panels should be in the list (order: last added first) */
    TEST_CHECK(head == c);
    TEST_CHECK(c->next == b);
    TEST_CHECK(b->next == a);

    free(a); free(b); free(c);
}

static void test_registry_update_visibility(void)
{
    tool_registry_init();

    ui_panel *head = NULL;

    tool_desc d1 = { "B1", "P1", dummy_tool_draw, NULL, 0, 0 };
    tool_desc d2 = { "B2", "P2", dummy_tool_draw, NULL, 0, 0 };

    ui_panel *a = tool_register(&head, 200, 1000, 600, &d1);
    ui_panel *b = tool_register(&head, 200, 1000, 600, &d2);

    /* Both start hidden */
    TEST_CHECK(a->visible == nk_false);
    TEST_CHECK(b->visible == nk_false);

    /* After update, should still be hidden (no toggle) */
    tool_registry_update();
    TEST_CHECK(a->visible == nk_false);
    TEST_CHECK(b->visible == nk_false);

    free(a); free(b);
}

static void test_registry_max_slots(void)
{
    tool_registry_init();

    ui_panel *head = NULL;

    /* Register TOOL_REGISTRY_MAX tools — all should succeed */
    int registered = 0;
    for (int i = 0; i < TOOL_REGISTRY_MAX; i++) {
        tool_desc desc = { "Btn", "Panel", dummy_tool_draw, NULL, 0, 0 };
        ui_panel *pnl = tool_register(&head, 200, 1000, 600, &desc);
        if (pnl) registered++;
    }

    TEST_CHECK(registered == TOOL_REGISTRY_MAX);

    /* One more should fail (return NULL) */
    tool_desc desc = { "Overflow", "Panel", dummy_tool_draw, NULL, 0, 0 };
    ui_panel *pnl = tool_register(&head, 200, 1000, 600, &desc);
    TEST_CHECK(pnl == NULL);

    /* Clean up all panels we allocated */
    ui_panel *cur = head;
    while (cur) {
        ui_panel *next = cur->next;
        free(cur);
        cur = next;
    }
}

static void test_registry_register_user_data(void)
{
    tool_registry_init();

    ui_panel *head = NULL;
    int mydata = 42;

    tool_desc desc = {
        .button_label = "U",
        .panel_title  = "U Panel",
        .draw         = dummy_tool_draw,
        .user_data    = &mydata,
        .panel_w      = 0,
        .panel_h      = 0
    };

    ui_panel *pnl = tool_register(&head, 200, 1000, 600, &desc);
    TEST_CHECK(pnl != NULL);
    TEST_CHECK(pnl->user_data == &mydata);

    free(pnl);
}

/* ================================================================== */
/*  TEST LIST                                                          */
/* ================================================================== */

TEST_LIST = {
    { "registry_init",              test_registry_init              },
    { "registry_register_default_bounds", test_registry_register_default_bounds },
    { "registry_register_explicit_bounds", test_registry_register_explicit_bounds },
    { "registry_register_multiple", test_registry_register_multiple },
    { "registry_update_visibility", test_registry_update_visibility },
    { "registry_max_slots",         test_registry_max_slots         },
    { "registry_register_user_data",test_registry_register_user_data},
    { NULL, NULL }
};
