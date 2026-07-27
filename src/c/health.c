/**
 * @file health.c
 * @brief Health and sleep data presentation for //GRID.
 */
#include "grid.h"

#ifdef PBL_PLATFORM_EMERY
#define METRIC_LABEL_COLOR     GColorCadetBlue
#define METRIC_VALUE_COLOR     GColorCyan
#define METRIC_SEPARATOR_COLOR GColorDarkGray
#define HEALTH_CELL_COUNT      4
#define HAS_HEART_RATE         1
#define SHOW_SLEEP_SEPARATOR   1
#else
#define METRIC_LABEL_COLOR     GColorWhite
#define METRIC_VALUE_COLOR     GColorWhite
#define METRIC_SEPARATOR_COLOR GColorWhite
#define HEALTH_CELL_COUNT      3
#define HAS_HEART_RATE         0
#define SHOW_SLEEP_SEPARATOR   0
#endif

#define METRIC_LABEL_FONT FONT_KEY_GOTHIC_14
#define METRIC_VALUE_FONT FONT_KEY_GOTHIC_14

static void draw_metric_cell(GContext *ctx, GRect cell, const char *label, const char *value)
{
    GFont label_font = fonts_get_system_font(METRIC_LABEL_FONT);
    GFont value_font = fonts_get_system_font(METRIC_VALUE_FONT);

    graphics_context_set_text_color(ctx, METRIC_LABEL_COLOR);
    graphics_draw_text(ctx, label, label_font, GRect(cell.origin.x, cell.origin.y, cell.size.w, 16),
                       GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);

    graphics_context_set_text_color(ctx, METRIC_VALUE_COLOR);
    graphics_draw_text(ctx, value, value_font, GRect(cell.origin.x, cell.origin.y + 15, cell.size.w, cell.size.h - 15),
                       GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
}

static void update_movement(Health *self)
{
    HealthValue steps = health_service_sum_today(HealthMetricStepCount);
    HealthValue kcal  = health_service_sum_today(HealthMetricActiveKCalories);
    HealthValue dist  = health_service_sum_today(HealthMetricWalkedDistanceMeters);

    if (steps >= 0)
        self->steps = (int32_t)steps;
    if (kcal >= 0)
        self->kcal = (int32_t)kcal;
    if (dist >= 0)
        self->dist_m = (int32_t)dist;
}

static void update_heart_rate(Health *self)
{
#if HAS_HEART_RATE
    HealthValue value = health_service_peek_current_value(HealthMetricHeartRateBPM);

    if (value > 0)
        self->heart_rate = (int32_t)value;
#else
    (void)self;
#endif
}

static void update_sleep(Health *self)
{
    HealthValue value = health_service_sum_today(HealthMetricSleepSeconds);

    if (value > 0)
        self->sleep_seconds = (int32_t)value;
}

void health_refresh(Health *self)
{
    if (!self)
        return;

    update_movement(self);
    update_heart_rate(self);
    update_sleep(self);
}

static void draw_health_row(const Health *self, GContext *ctx)
{
    char steps_buf[16];
#if HAS_HEART_RATE
    char hr_buf[16];
#endif
    char kcal_buf[16];
    char dist_buf[16];

    if (self->steps >= 0) {
        int steps = (int)(self->steps > 99999 ? 99999 : self->steps);

        if (steps >= 10000)
            snprintf(steps_buf, sizeof(steps_buf), "%dk", steps / 1000);
        else
            snprintf(steps_buf, sizeof(steps_buf), "%d", steps);
    } else {
        snprintf(steps_buf, sizeof(steps_buf), "--");
    }

#if HAS_HEART_RATE
    if (self->heart_rate > 0) {
        int heart_rate = (int)(self->heart_rate > 300 ? 300 : self->heart_rate);
        snprintf(hr_buf, sizeof(hr_buf), "%d", heart_rate);
    } else {
        snprintf(hr_buf, sizeof(hr_buf), "--");
    }
#endif

    if (self->kcal >= 0) {
        int kcal = (int)(self->kcal > 9999 ? 9999 : self->kcal);
        snprintf(kcal_buf, sizeof(kcal_buf), "%d", kcal);
    } else {
        snprintf(kcal_buf, sizeof(kcal_buf), "--");
    }

    if (self->dist_m >= 0) {
        int distance = (int)(self->dist_m > 999999 ? 999999 : self->dist_m);
        int km       = distance / 1000;
        int fraction = (distance % 1000) / 100;
        snprintf(dist_buf, sizeof(dist_buf), "%d.%d", km, fraction);
    } else {
        snprintf(dist_buf, sizeof(dist_buf), "--");
    }

    GRect row        = self->geometry->health;
    int   cell_width = row.size.w / HEALTH_CELL_COUNT;

#if HAS_HEART_RATE
    draw_metric_cell(ctx, GRect(0 * cell_width, row.origin.y, cell_width, row.size.h), "STP", steps_buf);
    draw_metric_cell(ctx, GRect(1 * cell_width, row.origin.y, cell_width, row.size.h), "BPM", hr_buf);
    draw_metric_cell(ctx, GRect(2 * cell_width, row.origin.y, cell_width, row.size.h), "CAL", kcal_buf);
    draw_metric_cell(ctx, GRect(3 * cell_width, row.origin.y, cell_width, row.size.h), "KM", dist_buf);
#else
    draw_metric_cell(ctx, GRect(0 * cell_width, row.origin.y, cell_width, row.size.h), "STP", steps_buf);
    draw_metric_cell(ctx, GRect(1 * cell_width, row.origin.y, cell_width, row.size.h), "CAL", kcal_buf);
    draw_metric_cell(ctx, GRect(2 * cell_width, row.origin.y, cell_width, row.size.h), "KM", dist_buf);
#endif

    graphics_context_set_stroke_color(ctx, METRIC_SEPARATOR_COLOR);
    graphics_context_set_stroke_width(ctx, 1);

    for (int i = 1; i < HEALTH_CELL_COUNT; i++)
        graphics_draw_line(ctx, GPoint(i * cell_width, row.origin.y + 2),
                           GPoint(i * cell_width, row.origin.y + row.size.h - 1));
}

static void draw_sleep_row(const Health *self, GContext *ctx)
{
    char value_buf[24];

    if (self->sleep_seconds > 0) {
        int total_minutes = (int)(self->sleep_seconds / 60);
        int hours         = total_minutes / 60;
        int minutes       = total_minutes % 60;

        snprintf(value_buf, sizeof(value_buf), "%dh %02dm", hours, minutes);
    } else {
        snprintf(value_buf, sizeof(value_buf), "--");
    }

    GRect row        = self->geometry->sleep;
    int   cell_width = row.size.w / 2;
    GFont font       = fonts_get_system_font(METRIC_VALUE_FONT);

    graphics_context_set_text_color(ctx, METRIC_VALUE_COLOR);
    graphics_draw_text(ctx, "Sleep", font, GRect(row.origin.x, row.origin.y, cell_width, row.size.h),
                       GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
    graphics_draw_text(ctx, value_buf, font,
                       GRect(row.origin.x + cell_width, row.origin.y, row.size.w - cell_width, row.size.h),
                       GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);

#if SHOW_SLEEP_SEPARATOR
    graphics_context_set_stroke_color(ctx, METRIC_SEPARATOR_COLOR);
    graphics_context_set_stroke_width(ctx, 1);
    graphics_draw_line(ctx, GPoint(row.origin.x + cell_width, row.origin.y + 2),
                       GPoint(row.origin.x + cell_width, row.origin.y + row.size.h - 2));
#endif
}

static void health_layer_update_proc(Layer *layer, GContext *ctx)
{
    Health *self = *(Health **)layer_get_data(layer);

    draw_health_row(self, ctx);
    draw_sleep_row(self, ctx);
}

static void health_handler(HealthEventType event, void *context)
{
    Health *self        = context;
    bool    significant = event == HealthEventSignificantUpdate;

    if (significant || event == HealthEventMovementUpdate)
        update_movement(self);

    if (significant || event == HealthEventHeartRateUpdate)
        update_heart_rate(self);

    if (significant || event == HealthEventSleepUpdate)
        update_sleep(self);
}

bool health_init(Health *self, Layer *details, const ScreenGeometry *geometry)
{
    if (!self || !details || !geometry)
        return false;

    self->geometry      = geometry;
    self->steps         = -1;
    self->heart_rate    = -1;
    self->kcal          = -1;
    self->dist_m        = -1;
    self->sleep_seconds = -1;
    self->layer         = layer_create_with_data(layer_get_bounds(details), sizeof(Health *));

    if (!self->layer)
        goto fail;

    *(Health **)layer_get_data(self->layer) = self;
    layer_set_update_proc(self->layer, health_layer_update_proc);
    layer_add_child(details, self->layer);

    health_refresh(self);
    health_service_events_subscribe(health_handler, self);
    self->service_subscribed = true;

    return true;

fail:
    health_deinit(self);
    return false;
}

void health_deinit(Health *self)
{
    if (!self)
        return;

    bool   service_subscribed = self->service_subscribed;
    Layer *layer              = self->layer;

    self->service_subscribed = false;
    self->layer              = NULL;
    self->geometry           = NULL;

    if (service_subscribed)
        health_service_events_unsubscribe();
    if (layer)
        layer_destroy(layer);
}
