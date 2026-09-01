/**
 * @file doppler.c
 * @brief Gesture handling, Doppler animation, and temporary details visibility.
 */
#include "grid.h"

#ifdef PBL_PLATFORM_EMERY
#define WINDOW_BG_COLOR       GColorBlack
#define DOPPLER_TRAIL_3_COLOR GColorDarkGray
#define DOPPLER_TRAIL_2_COLOR GColorCadetBlue
#define DOPPLER_TRAIL_1_COLOR GColorTiffanyBlue
#else
#define WINDOW_BG_COLOR       GColorBlack
#define DOPPLER_TRAIL_3_COLOR GColorDarkGray
#define DOPPLER_TRAIL_2_COLOR GColorCadetBlue
#define DOPPLER_TRAIL_1_COLOR GColorTiffanyBlue
#endif

#define DOPPLER_FRAME_MS 50
#define DOPPLER_FRAMES   18
#define DOPPLER_SPEED    6

static void cancel_timer(AppTimer **timer)
{
    if (!timer || !*timer)
        return;

    app_timer_cancel(*timer);
    *timer = NULL;
}

static void draw_doppler_wipe(GContext *ctx, GRect bounds, int frame)
{
    const struct {
        int    lag;
        GColor color;
    } rings[] = {
        {0, DOPPLER_TRAIL_1_COLOR},
        {3, DOPPLER_TRAIL_2_COLOR},
        {6, DOPPLER_TRAIL_3_COLOR},
    };

    int width      = bounds.size.w;
    int height     = bounds.size.h;
    int center_x   = width / 2;
    int center_y   = height / 2;
    int ring_count = (int)(sizeof(rings) / sizeof(rings[0]));

    if (frame < 0)
        frame = 0;
    if (frame > DOPPLER_FRAMES)
        frame = DOPPLER_FRAMES;

    if (frame > 0) {
        int wipe_radius = frame * DOPPLER_SPEED;

        graphics_context_set_fill_color(ctx, WINDOW_BG_COLOR);
        graphics_fill_circle(ctx, GPoint(center_x, center_y), wipe_radius);
    }

    graphics_context_set_stroke_width(ctx, 1);

    for (int i = 0; i < ring_count; i++) {
        int effective_frame = frame - rings[i].lag;

        if (effective_frame <= 0)
            continue;

        graphics_context_set_stroke_color(ctx, rings[i].color);
        graphics_draw_circle(ctx, GPoint(center_x, center_y), effective_frame * DOPPLER_SPEED);
    }
}

static void doppler_layer_update_proc(Layer *layer, GContext *ctx)
{
    Doppler *self = *(Doppler **)layer_get_data(layer);

    draw_doppler_wipe(ctx, layer_get_bounds(layer), self->frame);
}

static uint8_t validate_visible_duration(uint8_t duration)
{
    if (duration < GRID_GLANCE_MIN_DURATION_SEC || duration > GRID_GLANCE_MAX_DURATION_SEC)
        return GRID_GLANCE_DEFAULT_DURATION_SEC;

    return duration;
}

static void target_timeout_cb(void *context)
{
    Doppler *self = context;

    self->visible_timer = NULL;

    if (self->target_layer)
        layer_set_hidden(self->target_layer, true);
}

static void show_target(Doppler *self)
{
    cancel_timer(&self->visible_timer);

    layer_set_hidden(self->target_layer, false);

    uint32_t duration_ms = (uint32_t)self->visible_duration_sec * 1000;
    self->visible_timer  = app_timer_register(duration_ms, target_timeout_cb, self);
}

static void doppler_frame_cb(void *context)
{
    Doppler *self = context;

    self->frame_timer = NULL;

    if (self->frame < DOPPLER_FRAMES) {
        self->frame++;
        layer_mark_dirty(self->layer);
        self->frame_timer = app_timer_register(DOPPLER_FRAME_MS, doppler_frame_cb, self);
        return;
    }

    self->frame = 0;
    layer_set_hidden(self->layer, true);
    show_target(self);
}

void doppler_start(Doppler *self, uint8_t visible_duration_sec)
{
    if (!self || !self->layer || !self->target_layer)
        return;

    if (self->frame_timer || self->visible_timer)
        return;

    self->visible_duration_sec = validate_visible_duration(visible_duration_sec);

    layer_set_hidden(self->target_layer, true);

    self->frame = 1;
    layer_set_hidden(self->layer, false);

    if (!light_is_on())
        light_enable_interaction();

    self->frame_timer = app_timer_register(DOPPLER_FRAME_MS, doppler_frame_cb, self);
}

static void flick_handler(__attribute__((__unused__)) AccelAxisType axis, __attribute__((__unused__)) int32_t direction)
{
    App *app = app_from_active_window();

    if (!app)
        return;

    if (alarm_is_ringing(&app->alarm)) {
        alarm_handle_flick(&app->alarm, axis);
        return;
    }

    if (app->doppler.frame_timer || app->doppler.visible_timer)
        return;

    app_activate_glance(app);
}

bool doppler_init(Doppler *self, Layer *root, Layer *target, GRect bounds)
{
    if (!self || !root || !target)
        return false;

    self->target_layer         = target;
    self->visible_duration_sec = GRID_GLANCE_DEFAULT_DURATION_SEC;
    self->layer                = layer_create_with_data(bounds, sizeof(Doppler *));

    if (!self->layer)
        goto fail;

    *(Doppler **)layer_get_data(self->layer) = self;
    layer_set_update_proc(self->layer, doppler_layer_update_proc);
    layer_set_hidden(self->layer, true);
    layer_add_child(root, self->layer);

    accel_tap_service_subscribe(flick_handler);

    return true;

fail:
    doppler_deinit(self);
    return false;
}

void doppler_deinit(Doppler *self)
{
    if (!self)
        return;

    Layer *layer = self->layer;

    self->layer                = NULL;
    self->target_layer         = NULL;
    self->frame                = 0;
    self->visible_duration_sec = GRID_GLANCE_DEFAULT_DURATION_SEC;

    cancel_timer(&self->frame_timer);
    cancel_timer(&self->visible_timer);

    if (!layer)
        return;

    accel_tap_service_unsubscribe();
    layer_destroy(layer);
}
