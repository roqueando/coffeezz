/*
 * main.c — Nuklear UI demo using the ui_infra building blocks.
 *
 * Four panels:
 *   1. Main    — instructions
 *   2. Settings — form that controls the plot look
 *   3. Plot     — live sine‑wave chart
 *   4. Hello   — classic popup
 */
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>

#define GLFW_INCLUDE_GLCOREARB
#define GL_SILENCE_DEPRECATION
#include <GLFW/glfw3.h>

#define NK_INCLUDE_FIXED_TYPES
#define NK_INCLUDE_STANDARD_IO
#define NK_INCLUDE_STANDARD_VARARGS
#define NK_INCLUDE_DEFAULT_ALLOCATOR
#define NK_INCLUDE_VERTEX_BUFFER_OUTPUT
#define NK_INCLUDE_FONT_BAKING
#define NK_INCLUDE_DEFAULT_FONT
#define NK_IMPLEMENTATION
#define NK_GLFW_GL3_IMPLEMENTATION

#include "nuklear.h"
#include "nuklear_glfw_gl3.h"
#include "ui_infra.h"
#include "tools/tools.h"

#define WINDOW_WIDTH  1100
#define WINDOW_HEIGHT  700
#define SIDEBAR_W      200

#define MAX_VERTEX_BUFFER  (512 * 1024)
#define MAX_ELEMENT_BUFFER (128 * 1024)

/* ------------------------------------------------------------------ */
/*  Application state                                                 */
/* ------------------------------------------------------------------ */

/* Shared settings modified by the Settings form */
static nk_bool            g_custom_color = nk_false;  /* use colour picker */
static struct nk_colorf   g_line_col     = {0.0f, 0.47f, 0.86f, 1.0f};
static int                g_plot_speed   = 4;          /* sine frequency multiplier */

static const char *g_combo_items[] = { "Lines", "Columns" };
static int          g_combo_sel    = 0;   /* 0 = NK_CHART_LINES, 1 = NK_CHART_COLUMN */

static ui_plot       g_plot;
static ui_panel     *g_panels = NULL;          /* master panel list        */
static ui_form       g_settings_form;

static double g_time = 0.0;

/* ------------------------------------------------------------------ */
/*  Panel callbacks                                                    */
/* ------------------------------------------------------------------ */

/* ---------- Sidebar panel (fixed left dock, no move/resize) ---------- */
static void draw_sidebar(struct nk_context *ctx, ui_panel *panel)
{
    (void)panel;
    nk_layout_row_dynamic(ctx, 30, 1);
    nk_label(ctx, "TOOLS", NK_TEXT_CENTERED);
    nk_layout_row_dynamic(ctx, 6, 1);
    nk_spacing(ctx, 1);

    /* Tool buttons managed by the registry */
    tool_registry_draw_sidebar(ctx);
}


/* ---------- Hello popup ---------- */
static nk_bool g_hello_visible = nk_false;

/* ------------------------------------------------------------------ */
/*  Form button callbacks                                              */
/* ------------------------------------------------------------------ */

static void toggle_hello_cb(void *data)
{
    (void)data;
    g_hello_visible = nk_true;
}

/* ------------------------------------------------------------------ */
/*  GLFW error callback                                                */
/* ------------------------------------------------------------------ */

static void error_callback(int e, const char *d)
{
    fprintf(stderr, "GLFW Error %d: %s\n", e, d);
}

/* ------------------------------------------------------------------ */
/*  main                                                               */
/* ------------------------------------------------------------------ */

int main(void)
{
    /* ---------- GLFW + Nuklear init ---------- */
    struct nk_glfw nk_glfw = {0};
    GLFWwindow *win = NULL;
    int width = 0, height = 0;
    struct nk_context *ctx;

    glfwSetErrorCallback(error_callback);
    if (!glfwInit()) {
        fprintf(stderr, "Failed to init GLFW\n");
        return EXIT_FAILURE;
    }
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif
    win = glfwCreateWindow(WINDOW_WIDTH, WINDOW_HEIGHT,
                           "Coffeezz", NULL, NULL);
    if (!win) {
        fprintf(stderr, "Failed to create window\n");
        glfwTerminate();
        return EXIT_FAILURE;
    }
    glfwMakeContextCurrent(win);
    glfwGetFramebufferSize(win, &width, &height);
    glViewport(0, 0, width, height);

    ctx = nk_glfw3_init(&nk_glfw, win, NK_GLFW3_INSTALL_CALLBACKS);
    {
        struct nk_font_atlas *atlas;
        nk_glfw3_font_stash_begin(&nk_glfw, &atlas);
        nk_glfw3_font_stash_end(&nk_glfw);
    }

    /* ---------- Build panels ---------- */

    /* Sidebar — fixed left dock */
    static ui_panel sidebar_panel;
    ui_panel_init(&sidebar_panel, "Tools",
                  nk_rect(0, 0, SIDEBAR_W, WINDOW_HEIGHT),
                  NK_WINDOW_BORDER | NK_WINDOW_TITLE,
                  draw_sidebar, NULL);
    ui_panel_add(&g_panels, &sidebar_panel);

    /* ---------- Register all tools (auto-registers panels) ---------- */
    tools_init(&g_panels, SIDEBAR_W, WINDOW_WIDTH, WINDOW_HEIGHT);

    /* ---------- Build the Settings form ---------- */
    ui_form_init(&g_settings_form);
    ui_form_add_label(&g_settings_form, "Plot Settings");
    ui_form_add_separator(&g_settings_form);
    ui_form_add_checkbox(&g_settings_form, "Custom line colour", &g_custom_color);
    ui_form_add_color(&g_settings_form, "Line colour", &g_line_col);
    ui_form_add_combo(&g_settings_form, "Chart type",
                       &g_combo_sel, g_combo_items,
                       sizeof(g_combo_items)/sizeof(g_combo_items[0]));
    ui_form_add_slider_int(&g_settings_form, "Sine speed",
                           &g_plot_speed, 1, 10, 1);
    ui_form_add_separator(&g_settings_form);
    ui_form_add_button(&g_settings_form, "Show Hello popup",
                        toggle_hello_cb, NULL);

    /* ---------- Init the plot ---------- */
    ui_plot_init(&g_plot, "Live Sine Wave", NK_CHART_LINES);

    /* ---------- Main loop ---------- */
    struct nk_colorf bg;
    bg.r = 0.12f; bg.g = 0.16f; bg.b = 0.22f; bg.a = 1.0f;

    while (!glfwWindowShouldClose(win))
    {
        glfwPollEvents();

        /* Update plot with next sine sample */
        g_time += 0.016;  /* ~60 fps */
        float val = sinf((float)(g_time * g_plot_speed)) * 0.9f;
        ui_plot_push(&g_plot, val);

        /* Sync tool panel visibility from registry toggles */
        tool_registry_update();

        /* Nuklear frame */
        nk_glfw3_new_frame(&nk_glfw);

        /* Render all panels */
        ui_panels_render(ctx, g_panels);

        /* OpenGL draw */
        glfwGetFramebufferSize(win, &width, &height);
        glViewport(0, 0, width, height);
        glClear(GL_COLOR_BUFFER_BIT);
        glClearColor(bg.r, bg.g, bg.b, bg.a);
        nk_glfw3_render(&nk_glfw, NK_ANTI_ALIASING_ON,
                        MAX_VERTEX_BUFFER, MAX_ELEMENT_BUFFER);
        glfwSwapBuffers(win);
    }

    /* ---------- Cleanup ---------- */
    ui_form_free(&g_settings_form);
    nk_glfw3_shutdown(&nk_glfw);
    glfwTerminate();
    return 0;
}
