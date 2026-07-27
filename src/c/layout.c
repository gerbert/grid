/**
 * @file layout.c
 * @brief Main window, shared screen geometry, and decorative //GRID elements.
 */
#include "grid.h"

#ifdef PBL_PLATFORM_EMERY
#define WINDOW_BG_COLOR  GColorBlack
#define GRID_COLOR       GColorTiffanyBlue
#define ACCENT_COLOR     GColorCyan
#define REF_TIME_Y       20
#define REF_WEATHER_Y    86
#define REF_DATE_Y       106
#define WEATHER_HEIGHT   22
#define DATE_HEIGHT      30
#define HEALTH_HEIGHT    37
#define SHOW_BOTTOM_GRID 1
#else
#define WINDOW_BG_COLOR  GColorBlack
#define GRID_COLOR       GColorWhite
#define ACCENT_COLOR     GColorWhite
#define REF_TIME_Y       24
#define REF_WEATHER_Y    87
#define REF_DATE_Y       108
#define WEATHER_HEIGHT   16
#define DATE_HEIGHT      21
#define HEALTH_HEIGHT    35
#define SHOW_BOTTOM_GRID 0
#endif

#define SCANLINE_COUNT         5
#define SCANLINE_SPACING       4
#define SCREEN_MARGIN          4
#define BATTERY_HEIGHT         4
#define BATTERY_LABEL_WIDTH    32
#define BATTERY_RESERVED_WIDTH 35
#define SLEEP_HEIGHT           22
#define SLEEP_GAP              2

#define REF_HEIGHT          228
#define REF_TIME_HEIGHT     62
#define REF_HEALTH_Y        140
#define REF_BOTTOM_GRID_OFF 52

static int screen_scale(int ref, int height) { return (ref * height + REF_HEIGHT / 2) / REF_HEIGHT; }

static void screen_compute_geometry(GRect bounds, ScreenGeometry *geometry)
{
    int width        = bounds.size.w;
    int height       = bounds.size.h;
    int time_y       = screen_scale(REF_TIME_Y, height);
    int time_height  = screen_scale(REF_TIME_HEIGHT, height);
    int weather_y    = screen_scale(REF_WEATHER_Y, height);
    int date_y       = screen_scale(REF_DATE_Y, height);
    int health_y_raw = screen_scale(REF_HEALTH_Y, height);
    int battery_y    = height - SCREEN_MARGIN - BATTERY_HEIGHT;
    int health_y;
    int sleep_y;

#if SHOW_BOTTOM_GRID
    int bottom_offset = screen_scale(REF_BOTTOM_GRID_OFF, height);
    int health_y_max;

    geometry->bottom_grid_y = height - bottom_offset;
    health_y_max            = geometry->bottom_grid_y - HEALTH_HEIGHT + 1;
    health_y                = health_y_raw < health_y_max ? health_y_raw : health_y_max;
    sleep_y                 = geometry->bottom_grid_y + SLEEP_GAP;
#else
    int health_y_max;

    geometry->bottom_grid_y = -1;
    sleep_y                 = battery_y - SLEEP_HEIGHT - SLEEP_GAP;
    health_y_max            = sleep_y - HEALTH_HEIGHT;
    health_y                = health_y_raw < health_y_max ? health_y_raw : health_y_max;
#endif

    geometry->time    = GRect(0, time_y, width, time_height);
    geometry->weather = GRect(0, weather_y, width, WEATHER_HEIGHT);
    geometry->date    = GRect(0, date_y, width, DATE_HEIGHT);
    geometry->battery_bar =
        GRect(SCREEN_MARGIN, battery_y, width - 2 * SCREEN_MARGIN - BATTERY_RESERVED_WIDTH, BATTERY_HEIGHT);
    geometry->battery_label =
        GRect(width - SCREEN_MARGIN - BATTERY_LABEL_WIDTH - 2, battery_y - 9, BATTERY_LABEL_WIDTH, 14);
    geometry->health = GRect(0, health_y, width, HEALTH_HEIGHT);
    geometry->sleep  = GRect(0, sleep_y, width, SLEEP_HEIGHT);
}

static void draw_scanline(GContext *ctx, int y, int width)
{
    graphics_context_set_stroke_color(ctx, GRID_COLOR);
    graphics_context_set_stroke_width(ctx, 1);
    graphics_draw_line(ctx, GPoint(SCREEN_MARGIN, y), GPoint(width - SCREEN_MARGIN, y));
}

static void draw_top_grid(GContext *ctx, int width)
{
    for (int i = 0; i < SCANLINE_COUNT; i++)
        draw_scanline(ctx, 4 + i * SCANLINE_SPACING, width);
}

static void draw_bottom_grid(GContext *ctx, int width, const ScreenGeometry *geometry)
{
    if (geometry->bottom_grid_y < 0)
        return;

    draw_scanline(ctx, geometry->bottom_grid_y, width);
    draw_scanline(ctx, geometry->sleep.origin.y + geometry->sleep.size.h + SLEEP_GAP, width);
}

static void draw_accent_line(GContext *ctx, int width)
{
    graphics_context_set_stroke_color(ctx, ACCENT_COLOR);
    graphics_context_set_stroke_width(ctx, 2);
    graphics_draw_line(ctx, GPoint(0, 1), GPoint(width - 1, 1));
}

static void decor_layer_update_proc(Layer *layer, GContext *ctx)
{
    Screen *self   = *(Screen **)layer_get_data(layer);
    GRect   bounds = layer_get_bounds(layer);
    int     width  = bounds.size.w;

    draw_accent_line(ctx, width);
    draw_top_grid(ctx, width);
    draw_bottom_grid(ctx, width, &self->geometry);

#if !SHOW_BOTTOM_GRID
    draw_scanline(ctx, self->geometry.health.origin.y + self->geometry.health.size.h - 1, width);
#endif
}

static void main_window_load(Window *window)
{
    App    *app    = window_get_user_data(window);
    Screen *self   = app ? &app->screen : NULL;
    Layer  *root   = window_get_root_layer(window);
    GRect   bounds = root ? layer_get_bounds(root) : GRect(0, 0, 0, 0);

    if (!app || !self || !root)
        return;

    window_set_background_color(window, WINDOW_BG_COLOR);
    screen_compute_geometry(bounds, &self->geometry);

    self->details_layer = layer_create(bounds);

    if (!self->details_layer)
        return;

    layer_set_hidden(self->details_layer, true);
    layer_add_child(root, self->details_layer);

    self->decor_layer = layer_create_with_data(bounds, sizeof(Screen *));

    if (!self->decor_layer) {
        layer_destroy(self->details_layer);
        self->details_layer = NULL;
        return;
    }

    *(Screen **)layer_get_data(self->decor_layer) = self;
    layer_set_update_proc(self->decor_layer, decor_layer_update_proc);
    layer_add_child(self->details_layer, self->decor_layer);

    app_mount(app);
}

static void main_window_unload(Window *window)
{
    App    *app  = window_get_user_data(window);
    Screen *self = app ? &app->screen : NULL;

    if (!self)
        return;

    app_unmount(app);

    Layer *decor_layer   = self->decor_layer;
    Layer *details_layer = self->details_layer;

    self->decor_layer   = NULL;
    self->details_layer = NULL;

    if (decor_layer)
        layer_destroy(decor_layer);
    if (details_layer)
        layer_destroy(details_layer);
}

static const WindowHandlers WINDOW_HANDLERS = {
    .load   = main_window_load,
    .unload = main_window_unload,
};

bool screen_init(Screen *self, App *app)
{
    if (!self || !app)
        return false;

    self->window = window_create();

    if (!self->window)
        return false;

    window_set_user_data(self->window, app);
    window_set_window_handlers(self->window, WINDOW_HANDLERS);

    return true;
}

void screen_show(Screen *self)
{
    if (self && self->window)
        window_stack_push(self->window, true);
}

void screen_deinit(Screen *self)
{
    if (!self || !self->window)
        return;

    Window *window = self->window;

    self->window = NULL;
    window_destroy(window);
}
